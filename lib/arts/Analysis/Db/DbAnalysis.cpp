///==========================================================================///
/// File: DbAnalysis.cpp
/// Implementation of the DbAnalysis class for DB operation analysis.
///==========================================================================///

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_analysis);

using namespace mlir;
using namespace mlir::arts;

namespace {

static unsigned dbPatternRank(DbAccessPattern pattern) {
  switch (pattern) {
  case DbAccessPattern::stencil:
    return 3;
  case DbAccessPattern::indexed:
    return 2;
  case DbAccessPattern::uniform:
    return 1;
  case DbAccessPattern::unknown:
    return 0;
  }
  return 0;
}

static DbAccessPattern mergeDbPattern(DbAccessPattern lhs, DbAccessPattern rhs) {
  return dbPatternRank(rhs) > dbPatternRank(lhs) ? rhs : lhs;
}

static DbAccessPattern toDbAccessPattern(AccessPattern pattern) {
  switch (pattern) {
  case AccessPattern::Stencil:
    return DbAccessPattern::stencil;
  case AccessPattern::Indexed:
    return DbAccessPattern::indexed;
  case AccessPattern::Uniform:
    return DbAccessPattern::uniform;
  case AccessPattern::Unknown:
    return DbAccessPattern::unknown;
  }
  return DbAccessPattern::unknown;
}

static void collectLoadInfo(Value value, DenseSet<Value> &loadedMemrefs,
                            bool &hasMul, bool &hasAdd,
                            DenseSet<Operation *> &visited) {
  Operation *def = value.getDefiningOp();
  if (!def || !visited.insert(def).second)
    return;

  if (auto load = dyn_cast<memref::LoadOp>(def))
    loadedMemrefs.insert(load.getMemRef());
  if (isa<arith::MulIOp, arith::MulFOp>(def))
    hasMul = true;
  if (isa<arith::AddIOp, arith::AddFOp>(def))
    hasAdd = true;

  for (Value operand : def->getOperands())
    collectLoadInfo(operand, loadedMemrefs, hasMul, hasAdd, visited);
}

static bool detectMatmulUpdate(ForOp forOp) {
  bool hasMatmulUpdate = false;
  forOp.walk([&](memref::StoreOp store) {
    DenseSet<Value> loadedMemrefs;
    DenseSet<Operation *> visited;
    bool hasMul = false;
    bool hasAdd = false;
    collectLoadInfo(store.getValueToStore(), loadedMemrefs, hasMul, hasAdd,
                    visited);

    if (!hasMul || !hasAdd)
      return WalkResult::advance();
    if (!loadedMemrefs.contains(store.getMemRef()))
      return WalkResult::advance();
    if (loadedMemrefs.size() < 2)
      return WalkResult::advance();

    hasMatmulUpdate = true;
    return WalkResult::interrupt();
  });
  return hasMatmulUpdate;
}

static bool detectTriangularBound(ForOp forOp, Value loopIV) {
  bool hasTriangularBound = false;
  forOp.walk([&](scf::ForOp nestedFor) {
    if (ValueUtils::dependsOn(nestedFor.getLowerBound(), loopIV) ||
        ValueUtils::dependsOn(nestedFor.getUpperBound(), loopIV)) {
      hasTriangularBound = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasTriangularBound;
}

static EdtDistributionPattern
classifyDistributionPattern(const DbAnalysis::LoopDbAccessSummary &summary) {
  if (summary.hasTriangularBound)
    return EdtDistributionPattern::triangular;
  if (summary.hasStencilAccessHint || summary.hasStencilOffset)
    return EdtDistributionPattern::stencil;
  if (summary.hasMatmulUpdate)
    return EdtDistributionPattern::matmul;
  return EdtDistributionPattern::uniform;
}

} // namespace

DbAnalysis::DbAnalysis(ArtsAnalysisManager &AM) : ArtsAnalysis(AM) {
  ARTS_DEBUG("Initializing DbAnalysis");
  dbAliasAnalysis = std::make_unique<DbAliasAnalysis>(this);
}

DbAnalysis::~DbAnalysis() { ARTS_DEBUG("Destroying DbAnalysis"); }

DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  ARTS_INFO("Getting or creating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end())
    return *it->second.get();

  ARTS_DEBUG(" - Creating new DbGraph for function: " << func.getName());
  auto newGraph = std::make_unique<DbGraph>(func, this);

  /// Build nodes and dependencies
  newGraph->build();

  /// Store the graph
  DbGraph *graphPtr = newGraph.get();
  functionGraphMap[func] = std::move(newGraph);
  return *graphPtr;
}

bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  ARTS_INFO("Invalidating DbGraph for function: " << func.getName());
  auto it = functionGraphMap.find(func);
  if (it != functionGraphMap.end()) {
    functionGraphMap.erase(it);
    loopAccessSummaryByOp.clear();
    if (dbAliasAnalysis)
      dbAliasAnalysis->resetCache();
    return true;
  }
  return false;
}

void DbAnalysis::invalidate() {
  ARTS_INFO("Invalidating all DbGraphs");
  functionGraphMap.clear();
  loopAccessSummaryByOp.clear();
  if (dbAliasAnalysis)
    dbAliasAnalysis->resetCache();
}

void DbAnalysis::print(func::FuncOp func) {
  ARTS_INFO("Printing DbGraph for function: " << func.getName());
  DbGraph &graph = getOrCreateGraph(func);
  graph.print(ARTS_DBGS());
}

LoopAnalysis *DbAnalysis::getLoopAnalysis() {
  return &getAnalysisManager().getLoopAnalysis();
}

DbAnalysis::LoopDbAccessSummary
DbAnalysis::analyzeLoopDbAccessPatterns(ForOp forOp) {
  LoopDbAccessSummary summary;
  if (!forOp) {
    summary.distributionPattern = EdtDistributionPattern::unknown;
    return summary;
  }

  if (auto it = loopAccessSummaryByOp.find(forOp.getOperation());
      it != loopAccessSummaryByOp.end())
    return it->second;

  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() == 0) {
    summary.distributionPattern = EdtDistributionPattern::unknown;
    loopAccessSummaryByOp[forOp.getOperation()] = summary;
    return summary;
  }

  Value loopIV = forBody.getArgument(0);
  summary.hasTriangularBound = detectTriangularBound(forOp, loopIV);
  summary.hasMatmulUpdate = detectMatmulUpdate(forOp);

  Region &forRegion = forOp.getRegion();
  EdtOp edt = forOp->getParentOfType<EdtOp>();
  func::FuncOp func = forOp->getParentOfType<func::FuncOp>();
  if (!edt || !func) {
    summary.distributionPattern = classifyDistributionPattern(summary);
    loopAccessSummaryByOp[forOp.getOperation()] = summary;
    return summary;
  }

  DbGraph &graph = getOrCreateGraph(func);
  graph.forEachAcquireNode([&](DbAcquireNode *acqNode) {
    if (!acqNode || acqNode->getEdtUser() != edt)
      return;

    DbAllocNode *rootAlloc = acqNode->getRootAlloc();
    if (!rootAlloc || !rootAlloc->getDbAllocOp())
      return;
    Operation *allocOp = rootAlloc->getDbAllocOp().getOperation();

    DbAccessPattern combinedPattern = DbAccessPattern::unknown;
    if (auto basePattern = getAcquireAccessPattern(acqNode->getDbAcquireOp())) {
      DbAccessPattern depPattern = toDbAccessPattern(*basePattern);
      combinedPattern = mergeDbPattern(combinedPattern, depPattern);
      if (depPattern == DbAccessPattern::stencil)
        summary.hasStencilAccessHint = true;
    }

    DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
    acqNode->collectAccessOperations(dbRefToMemOps);

    SmallVector<AccessIndexInfo, 16> accesses;
    for (auto &[dbRef, memOps] : dbRefToMemOps) {
      for (Operation *memOp : memOps) {
        Region *memRegion = memOp ? memOp->getParentRegion() : nullptr;
        if (!memRegion)
          continue;
        if (memRegion != &forRegion && !forRegion.isAncestor(memRegion))
          continue;

        SmallVector<Value> indexChain =
            DatablockUtils::collectFullIndexChain(dbRef, memOp);
        if (indexChain.empty())
          continue;
        AccessIndexInfo info;
        info.dbRefPrefix = dbRef.getIndices().size();
        info.indexChain.append(indexChain.begin(), indexChain.end());
        accesses.push_back(std::move(info));
      }
    }

    if (!accesses.empty()) {
      AccessBoundsResult bounds =
          analyzeAccessBoundsFromIndices(accesses, loopIV, loopIV);
      if (bounds.valid) {
        if (bounds.isStencil || bounds.minOffset != 0 || bounds.maxOffset != 0) {
          combinedPattern =
              mergeDbPattern(combinedPattern, DbAccessPattern::stencil);
          summary.hasStencilOffset = true;
        } else {
          combinedPattern =
              mergeDbPattern(combinedPattern, DbAccessPattern::uniform);
        }
      } else if (bounds.hasVariableOffset) {
        combinedPattern =
            mergeDbPattern(combinedPattern, DbAccessPattern::indexed);
      }
    }

    auto it = summary.allocPatterns.find(allocOp);
    if (it == summary.allocPatterns.end()) {
      summary.allocPatterns[allocOp] = combinedPattern;
    } else {
      it->second = mergeDbPattern(it->second, combinedPattern);
    }
  });

  summary.distributionPattern = classifyDistributionPattern(summary);
  loopAccessSummaryByOp[forOp.getOperation()] = summary;
  return summary;
}

std::optional<DbAnalysis::LoopDbAccessSummary>
DbAnalysis::getLoopDbAccessSummary(ForOp forOp) {
  if (!forOp)
    return std::nullopt;
  if (auto it = loopAccessSummaryByOp.find(forOp.getOperation());
      it != loopAccessSummaryByOp.end())
    return it->second;
  return analyzeLoopDbAccessPatterns(forOp);
}

std::optional<EdtDistributionPattern>
DbAnalysis::getLoopDistributionPattern(ForOp forOp) {
  if (!forOp)
    return std::nullopt;
  return analyzeLoopDbAccessPatterns(forOp).distributionPattern;
}

std::optional<AccessPattern>
DbAnalysis::getAcquireAccessPattern(DbAcquireOp acquire) {
  if (!acquire)
    return std::nullopt;

  func::FuncOp func = acquire->getParentOfType<func::FuncOp>();
  if (!func)
    return std::nullopt;

  DbGraph &graph = getOrCreateGraph(func);
  DbAcquireNode *node = graph.getDbAcquireNode(acquire);
  if (!node)
    return std::nullopt;
  return node->getAccessPattern();
}

std::optional<DbAccessPattern> DbAnalysis::getAllocAccessPattern(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  if (auto attrPattern = getDbAccessPattern(alloc.getOperation()))
    return attrPattern;

  func::FuncOp func = alloc->getParentOfType<func::FuncOp>();
  if (!func)
    return std::nullopt;

  DbGraph &graph = getOrCreateGraph(func);
  DbAllocNode *node = graph.getDbAllocNode(alloc);
  if (!node)
    return std::nullopt;

  AcquirePatternSummary summary = node->summarizeAcquirePatterns();
  if (summary.hasStencil)
    return DbAccessPattern::stencil;
  if (summary.hasIndexed)
    return DbAccessPattern::indexed;
  if (summary.hasUniform)
    return DbAccessPattern::uniform;
  return DbAccessPattern::unknown;
}
