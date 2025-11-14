///==========================================================================///
/// File: EdtDataFlowAnalysis.cpp
/// Builds EDT-to-EDT dependencies by tracking datablock acquires.
///==========================================================================///

#include "arts/Analysis/Edt/EdtDataFlowAnalysis.h"

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_dataflow);

using namespace mlir;
using namespace mlir::arts;

namespace {
struct IndentScope {
  IndentScope() { ++depth; }
  ~IndentScope() { --depth; }
  static int depth;
};
int IndentScope::depth = 0;

std::string indent() { return std::string(IndentScope::depth * 2, ' '); }
} // namespace

EdtDataFlowAnalysis::EdtDataFlowAnalysis(DbGraph *dbGraph, DbAnalysis *analysis)
    : dbGraph(dbGraph), analysis(analysis) {
  assert(dbGraph && "DbGraph is required");
  assert(analysis && "DbAnalysis is required");
  aliasAA = analysis->getAliasAnalysis();
  assert(aliasAA && "Alias analysis is required");
}

SmallVector<EdtDependency, 16> EdtDataFlowAnalysis::run(func::FuncOp func) {
  Environment env;
  processRegion(func.getBody(), env);

  SmallVector<EdtDependency, 16> result;
  for (auto &entry : dependencyMap) {
    Operation *fromOp = entry.first.first;
    Operation *toOp = entry.first.second;
    auto from = dyn_cast_or_null<EdtOp>(fromOp);
    auto to = dyn_cast_or_null<EdtOp>(toOp);
    if (!from || !to || from == to)
      continue;
    result.push_back(EdtDependency{from, to, entry.second});
  }
  return result;
}

std::pair<EdtDataFlowAnalysis::Environment, bool>
EdtDataFlowAnalysis::processRegion(Region &region, Environment &env) {
  Environment newEnv = env;
  bool changed = false;

  for (Block &block : region) {
    for (Operation &op : block) {
      IndentScope scope;
      std::pair<Environment, bool> result = {env, false};

      if (auto edt = dyn_cast<EdtOp>(&op))
        result = processEdt(edt, newEnv);
      else if (auto epoch = dyn_cast<EpochOp>(&op))
        result = processEpoch(epoch, newEnv);
      else if (auto ifOp = dyn_cast<scf::IfOp>(&op))
        result = processIf(ifOp, newEnv);
      else if (auto forOp = dyn_cast<scf::ForOp>(&op))
        result = processFor(forOp, newEnv);
      else
        continue;

      newEnv = std::move(result.first);
      changed |= result.second;
    }
  }

  return {std::move(newEnv), changed};
}

std::pair<EdtDataFlowAnalysis::Environment, bool>
EdtDataFlowAnalysis::processEdt(EdtOp edtOp, Environment &env) {
  ARTS_DEBUG(indent() << "Processing EDT");

  auto deps = edtOp.getDependenciesAsVector();
  bool changed = false;
  Environment newEnv = env;

  SmallVector<DbAcquireNode *, 4> inputAcquires;
  SmallVector<DbAcquireNode *, 4> outputAcquires;

  for (auto dep : deps) {
    auto acquireOp = dyn_cast_or_null<DbAcquireOp>(dep.getDefiningOp());
    if (!acquireOp)
      continue;
    auto *node = dbGraph->getDbAcquireNode(acquireOp);
    if (!node)
      continue;
    auto mode = acquireOp.getMode();
    if (mode == ArtsMode::in || mode == ArtsMode::inout)
      inputAcquires.push_back(node);
    if (mode == ArtsMode::out || mode == ArtsMode::inout)
      outputAcquires.push_back(node);
  }

  ARTS_DEBUG(indent() << "  RAW candidates: " << inputAcquires.size());
  for (auto *consumer : inputAcquires) {
    auto prodDefs = findDefinitions(consumer, newEnv);
    if (prodDefs.empty())
      continue;

    for (auto *producer : prodDefs) {
      if (!mayDepend(producer, consumer))
        continue;
      recordDependency(producer, consumer, DbDepType::RAW);
    }
  }

  ARTS_DEBUG(indent() << "  WAW candidates: " << outputAcquires.size());
  for (auto *writer : outputAcquires) {
    auto prodDefs = findDefinitions(writer, newEnv);
    for (auto *producer : prodDefs) {
      if (!mayDepend(producer, writer))
        continue;
      recordDependency(producer, writer, DbDepType::WAW);
    }

    DbAllocNode *parentAlloc = writer->getParent();
    auto &defs = newEnv.writers[parentAlloc];
    bool needsUpdate = defs.size() != 1 || !defs.contains(writer);
    if (needsUpdate) {
      defs.clear();
      defs.insert(writer);
      changed = true;
    }
  }

  ARTS_DEBUG(indent() << "  WAR candidates: " << outputAcquires.size());
  for (auto *writer : outputAcquires) {
    DbAllocNode *parentAlloc = writer->getParent();
    auto &liveReaders = newEnv.readers[parentAlloc];
    if (liveReaders.empty())
      continue;
    for (auto *reader : liveReaders) {
      if (!mayDepend(reader, writer))
        continue;
      recordDependency(reader, writer, DbDepType::WAR);
    }
    liveReaders.clear();
  }

  for (auto *reader : inputAcquires) {
    DbAllocNode *parentAlloc = reader->getParent();
    newEnv.readers[parentAlloc].insert(reader);
  }

  return {newEnv, changed};
}

std::pair<EdtDataFlowAnalysis::Environment, bool>
EdtDataFlowAnalysis::processEpoch(EpochOp epochOp, Environment &env) {
  ARTS_DEBUG(indent() << "Processing Epoch");
  auto bodyResult = processRegion(epochOp.getBody(), env);
  Environment merged = bodyResult.first;
  merged.writers.clear();
  merged.readers.clear();
  return {std::move(merged), bodyResult.second};
}

std::pair<EdtDataFlowAnalysis::Environment, bool>
EdtDataFlowAnalysis::processIf(scf::IfOp ifOp, Environment &env) {
  ARTS_DEBUG(indent() << "Processing scf.if");
  auto thenResult = processRegion(ifOp.getThenRegion(), env);

  Environment elseEnv = env;
  bool elseChanged = false;
  if (!ifOp.getElseRegion().empty()) {
    auto elseResult = processRegion(ifOp.getElseRegion(), env);
    elseEnv = std::move(elseResult.first);
    elseChanged = elseResult.second;
  }

  Environment merged = thenResult.first;
  bool mergeChanged = unionInto(merged, elseEnv);
  bool changed = thenResult.second || elseChanged || mergeChanged;
  return {std::move(merged), changed};
}

std::pair<EdtDataFlowAnalysis::Environment, bool>
EdtDataFlowAnalysis::processFor(scf::ForOp forOp, Environment &env) {
  ARTS_DEBUG(indent() << "Processing scf.for");

  Environment loopEnv = env;
  bool overallChanged = false;
  constexpr unsigned maxIterations = 5;

  for (unsigned iter = 0; iter < maxIterations; ++iter) {
    auto bodyResult = processRegion(forOp.getRegion(), loopEnv);
    overallChanged |= bodyResult.second;
    bool envChanged = unionInto(loopEnv, bodyResult.first);
    overallChanged |= envChanged;
    if (!bodyResult.second && !envChanged)
      break;
  }

  return {loopEnv, overallChanged};
}

SmallVector<DbAcquireNode *>
EdtDataFlowAnalysis::findDefinitions(DbAcquireNode *acquire,
                                     Environment &env) {
  SmallVector<DbAcquireNode *> defs;
  DbAllocNode *parentAlloc = acquire->getParent();

  auto it = env.writers.find(parentAlloc);
  if (it == env.writers.end())
    return defs;

  for (auto *def : it->second) {
    auto aliasRes = aliasAA->classifyAlias(*def, *acquire);
    if (aliasRes == DbAliasAnalysis::AliasResult::NoAlias)
      continue;
    if (aliasRes == DbAliasAnalysis::AliasResult::MayAlias) {
      auto k = aliasAA->estimateOverlap(def, acquire);
      if (k == DbAliasAnalysis::OverlapKind::Disjoint)
        continue;
    }
    defs.push_back(def);
  }

  return defs;
}

bool EdtDataFlowAnalysis::unionInto(Environment &target,
                                    const Environment &source) {
  bool changed = false;

  for (const auto &pair : source.writers) {
    auto &defs = target.writers[pair.first];
    for (auto *acq : pair.second)
      if (!defs.contains(acq)) {
        defs.insert(acq);
        changed = true;
      }
  }

  for (const auto &pair : source.readers) {
    auto &defs = target.readers[pair.first];
    for (auto *acq : pair.second)
      if (!defs.contains(acq)) {
        defs.insert(acq);
        changed = true;
      }
  }

  return changed;
}

bool EdtDataFlowAnalysis::mayDepend(DbAcquireNode *producer,
                                    DbAcquireNode *consumer) {
  assert(producer->getParent() == consumer->getParent() &&
         "Producer and consumer must share same allocation");

  auto prodMode = producer->getDbAcquireOp().getMode();
  auto consMode = consumer->getDbAcquireOp().getMode();

  if ((prodMode == ArtsMode::out || prodMode == ArtsMode::inout) &&
      (consMode == ArtsMode::in || consMode == ArtsMode::inout))
    return true; // RAW

  if ((prodMode == ArtsMode::out || prodMode == ArtsMode::inout) &&
      (consMode == ArtsMode::out || consMode == ArtsMode::inout))
    return true; // WAW

  if ((prodMode == ArtsMode::in || prodMode == ArtsMode::inout) &&
      (consMode == ArtsMode::out || consMode == ArtsMode::inout))
    return true; // WAR

  return false;
}

DbAcquireUsage EdtDataFlowAnalysis::buildUsage(DbAcquireNode *node) {
  DbAcquireUsage usage;
  if (!node)
    return usage;
  usage.acquireNode = node;
  usage.allocNode = node->getRootAlloc();
  usage.acquire = node->getDbAcquireOp();
  if (usage.acquire)
    usage.mode = usage.acquire.getMode();
  usage.estimatedBytes = node->estimatedBytes;
  if (auto *alloc = node->getRootAlloc()) {
    usage.alloc = alloc->getDbAllocOp();
    usage.label = alloc->getHierId().str();
  }
  if (auto *loopAnalysis = analysis->getLoopAnalysis()) {
    loopAnalysis->collectEnclosingLoops(node->getOp(), usage.loops);
  }
  return usage;
}

void EdtDataFlowAnalysis::recordDependency(DbAcquireNode *producer,
                                           DbAcquireNode *consumer,
                                           DbDepType depType) {
  if (!producer || !consumer)
    return;
  EdtOp from = producer->getEdtUser();
  EdtOp to = consumer->getEdtUser();
  if (!from || !to || from == to)
    return;

  auto &slices =
      dependencyMap[{from.getOperation(), to.getOperation()}];
  DbEdgeSlice slice;
  slice.depType = depType;
  slice.producer = buildUsage(producer);
  slice.consumer = buildUsage(consumer);
  slice.description =
      slice.producer.label.empty() ? slice.consumer.label : slice.producer.label;
  if (slice.description.empty())
    slice.description = "db";
  slices.push_back(std::move(slice));
}

void EdtDataFlowAnalysis::recordDependency(DbAcquireNode *reader,
                                           DbAcquireNode *writer) {
  recordDependency(reader, writer, DbDepType::WAR);
}
