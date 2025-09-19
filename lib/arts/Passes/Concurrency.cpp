///==========================================================================
/// File: Concurrency.cpp
/// Defines concurrency-driven optimizations using ARTS analysis managers.
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Concurrency/DbPlacement.h"
#include "arts/Analysis/Concurrency/EdtPlacement.h"
#include "arts/Analysis/Graphs/Concurrency/ConcurrencyGraph.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsAbstractMachine.h"

#include "mlir/IR/IRMapping.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(concurrency)

using namespace mlir;
using namespace mlir::arts;

///==========================================================================
/// ParallelEdtLowerer - Dedicated struct for lowering parallel EDTs
///
/// This struct encapsulates all logic for converting parallel EDTs into
/// distributed task EDTs with proper route assignment and work distribution.
///==========================================================================
struct ParallelEdtLowerer {
  ArtsAbstractMachine *abstractMachine;
  ArtsCodegen *AC;
  Location loc;

  /// Information about chunked for loop bounds
  struct ChunkedForInfo {
    Value lowerBound;
    Value upperBound;
    Value step;
    Value span;
    bool isValid() const { return lowerBound && upperBound && step; }
  };

  ParallelEdtLowerer(ArtsAbstractMachine *abstractMachine, ArtsCodegen *AC,
                     Location loc)
      : abstractMachine(abstractMachine), AC(AC), loc(loc) {}

  /// Main entry point for lowering parallel EDTs
  void lowerParallelEdt(EdtOp op);

private:
  /// Create epoch wrapper for parallel execution
  EpochOp createEpochWrapper();

  /// Lower parallel EDT with contained ForOp (chunked execution)
  void lowerChunkedParallelEdt(EdtOp op, ForOp containedForOp);

  /// Lower simple parallel EDT (no contained ForOp)
  void lowerSimpleParallelEdt(EdtOp op);

  /// Setup EDT region with proper arguments and terminators
  void setupEdtRegion(EdtOp edt, EdtOp sourceEdt);

  /// Determine parallelism strategy and create loop bounds
  /// Returns (lowerBound, upperBound, isInterNode)
  std::tuple<Value, Value, bool> determineParallelismStrategy();

  /// Find contained ForOp in EDT
  ForOp findContainedForOp(EdtOp op);

  /// Create chunked dependencies for parallel loop iterations
  struct ChunkedDepsResult {
    SmallVector<Value> deps;
    ChunkedForInfo chunkInfo;
    DenseMap<Value, Value> depRemap;
  };
  ChunkedDepsResult createChunkedDeps(EdtOp op, ForOp containedForOp,
                                      Value workerId, Value totalWorkers,
                                      DenseMap<Value, Value> *blockArgMap);

  /// Compute chunk bounds for work distribution
  ChunkedForInfo computeChunkBounds(Value flb, Value fub, Value fst,
                                    Value workerId, Value totalWorkers);

  /// Process a single dependency for chunking
  Value processDependencyForChunking(Value dep, ChunkedForInfo chunkInfo,
                                     DenseMap<Value, Value> &valueCache,
                                     DenseMap<Value, Value> *blockArgMap);

  /// Create simple dependencies (no chunking)
  SmallVector<Value> createSimpleDeps(EdtOp op,
                                      DenseMap<Value, Value> *blockArgMap,
                                      DenseMap<Value, Value> &depRemap);

  /// Move operations from source EDT to new task EDT
  void moveOperationsToNewEdt(EdtOp sourceOp, EdtOp newEdt,
                              ForOp containedForOp,
                              const ChunkedForInfo *chunkInfo,
                              DenseMap<Value, Value> *depRemap);

  /// Create dependency mapping for EDT
  DenseMap<Value, Value> makeDependencyMap(EdtOp op);

  /// Materialize value in new context
  Value materializeValue(Value val, DenseMap<Value, Value> &cache,
                         DenseMap<Value, Value> *blockArgMap);
};

///==========================================================================
/// ConcurrencyPass
///==========================================================================
struct ConcurrencyPass : public ConcurrencyBase<ConcurrencyPass> {
  explicit ConcurrencyPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }
  void runOnOperation() override;

private:
  /// Process parallel EDTs of the module
  void processParallelEdts();

  /// Lower a parallel EDT. If it contains an arts.for, create chunked DbAcquire
  /// deps per worker and outline the loop body as a task EDT per worker.
  void lowerParallelEdt(EdtOp op);

  /// Remove unnecessary barriers between low-risk concurrent EDTs
  void pruneBarriers(func::FuncOp func);

  /// Apply EDT and DB placement decisions
  void applyPlacementDecisions(func::FuncOp func);

  /// Check if a barrier can be safely removed
  bool canRemoveBarrier(BarrierOp barrier, EdtGraph *edtGraph);

  /// Insert a barrier between two EDTs
  void insertBarrierBetween(EdtOp a, EdtOp b);

  /// Apply EDT placement decisions to operations
  void applyEdtPlacement(const SmallVector<PlacementDecision, 16> &decisions,
                         const SmallVector<std::string, 8> &nodes);

  /// Apply DB placement decisions to operations
  void applyDbPlacement(const SmallVector<DbPlacementDecision, 16> &decisions,
                        const SmallVector<std::string, 8> &nodes);

  /// Emit debug output for placement decisions
  void emitDebugOutput(func::FuncOp func,
                       const EdtPlacementHeuristics &edtPlacer,
                       const SmallVector<PlacementDecision, 16> &edtDecisions,
                       const DbPlacementHeuristics &dbPlacer,
                       const SmallVector<DbPlacementDecision, 16> &dbDecisions);

  ArtsAbstractMachine *abstractMachine = nullptr;
  ArtsAnalysisManager *AM = nullptr;
  ModuleOp module;
};

void ConcurrencyPass::runOnOperation() {
  module = getOperation();

  /// Get abstractMachine from ArtsAnalysisManager
  abstractMachine = &AM->getAbstractMachine();

  ARTS_DEBUG_HEADER(concurrency);
  ARTS_DEBUG_REGION(module.dump(););
  ARTS_INFO("Machine valid=" << (abstractMachine->isValid() ? 1 : 0)
                             << ", nodes=" << abstractMachine->getNodeCount()
                             << ", threads=" << abstractMachine->getThreads());
  processParallelEdts();
  ARTS_DEBUG_FOOTER(concurrency);
  ARTS_DEBUG_REGION(module.dump(););
}

void ConcurrencyPass::processParallelEdts() {
  module.walk([&](func::FuncOp func) {
    SmallVector<EdtOp, 8> parallelEdts;
    func.walk([&](EdtOp edt) {
      if (edt.getType() == EdtType::parallel)
        parallelEdts.push_back(edt);
    });
    for (EdtOp edt : parallelEdts)
      lowerParallelEdt(edt);
  });
}

void ConcurrencyPass::lowerParallelEdt(EdtOp op) {
  Location loc = op.getLoc();

  /// Create ArtsCodegen instance for this operation
  ArtsCodegen AC(module, false);
  AC.setInsertionPoint(op);

  /// Use ParallelEdtLowerer for dedicated lowering logic
  ParallelEdtLowerer lowerer(abstractMachine, &AC, loc);
  lowerer.lowerParallelEdt(op);
}

void ConcurrencyPass::pruneBarriers(func::FuncOp func) {
  SmallVector<BarrierOp, 8> toErase;
  auto &edtGraph = AM->getEdtGraph(func);
  func.walk([&](BarrierOp barrier) {
    if (canRemoveBarrier(barrier, &edtGraph))
      toErase.push_back(barrier);
  });

  for (auto barrier : toErase)
    barrier.erase();
}

void ConcurrencyPass::applyPlacementDecisions(func::FuncOp func) {
  auto nodes = EdtPlacementHeuristics::makeNodeNames(4);
  auto &dbGraph = AM->getDbGraph(func);
  auto &edtGraph = AM->getEdtGraph(func);
  auto &edtAnalysis = AM->getEdtAnalysis();

  /// EDT placement
  EdtPlacementHeuristics edtPlacer(&edtGraph, &edtAnalysis);
  auto edtDecisions = edtPlacer.compute(nodes);
  applyEdtPlacement(edtDecisions, nodes);

  /// DB placement
  DbPlacementHeuristics dbPlacer(&dbGraph);
  auto dbDecisions = dbPlacer.compute(func, nodes);
  applyDbPlacement(dbDecisions, nodes);

  /// Debug output
  emitDebugOutput(func, edtPlacer, edtDecisions, dbPlacer, dbDecisions);
}

bool ConcurrencyPass::canRemoveBarrier(BarrierOp barrier, EdtGraph *edtGraph) {
  Block *block = barrier->getBlock();
  SmallVector<EdtOp, 8> before, after;

  /// Collect EDTs before and after the barrier
  bool pastBarrier = false;
  for (Operation &op : *block) {
    if (&op == barrier.getOperation()) {
      pastBarrier = true;
      continue;
    }
    if (auto edt = dyn_cast<EdtOp>(&op)) {
      (pastBarrier ? after : before).push_back(edt);
    }
  }

  if (before.empty() || after.empty()) {
    return false;
  }

  /// Check if all pairs are concurrent
  for (EdtOp a : before) {
    for (EdtOp b : after) {
      if (edtGraph->isEdtReachable(a, b) || edtGraph->isEdtReachable(b, a)) {
        return false; /// Not all pairs are concurrent
      }
    }
  }

  /// Check if all pairs are low-risk
  auto &edtAnalysis = AM->getEdtAnalysis();
  for (EdtOp a : before) {
    for (EdtOp b : after) {
      auto affinity = edtAnalysis.affinity(a, b);
      if (affinity.mayConflict || affinity.concurrencyRisk > 0.0) {
        return false; /// Risky pair found
      }
    }
  }

  return true;
}

void ConcurrencyPass::insertBarrierBetween(EdtOp a, EdtOp b) {
  if (a->getBlock() != b->getBlock())
    return;

  Block *block = a->getBlock();

  /// Determine order of operations
  bool aBeforeB = true;
  for (Operation &op : *block) {
    if (&op == a) {
      aBeforeB = true;
      break;
    }
    if (&op == b) {
      aBeforeB = false;
      break;
    }
  }

  EdtOp first = aBeforeB ? a : b;
  EdtOp second = aBeforeB ? b : a;

  /// Check if barrier already exists between them
  for (Operation *it = first->getNextNode(); it && it != second;
       it = it->getNextNode()) {
    if (isa<BarrierOp>(it)) {
      return;
    }
  }

  /// Insert new barrier
  OpBuilder builder(second);
  builder.create<BarrierOp>(second->getLoc());
}

void ConcurrencyPass::applyEdtPlacement(
    const SmallVector<PlacementDecision, 16> &decisions,
    const SmallVector<std::string, 8> &nodes) {
  llvm::StringMap<unsigned> nodeIndex;
  for (unsigned i = 0; i < nodes.size(); ++i) {
    nodeIndex[nodes[i]] = i;
  }

  ModuleOp module = getOperation();
  OpBuilder builder(module.getContext());
  for (const auto &decision : decisions) {
    auto it = nodeIndex.find(decision.chosenNode);
    unsigned idx = (it != nodeIndex.end()) ? it->second : 0u;
    decision.edtOp->setAttr(
        "node", IntegerAttr::get(builder.getI32Type(), (int32_t)idx));
  }
}

void ConcurrencyPass::applyDbPlacement(
    const SmallVector<DbPlacementDecision, 16> &decisions,
    const SmallVector<std::string, 8> &nodes) {
  llvm::StringMap<unsigned> nodeIndex;
  for (unsigned i = 0; i < nodes.size(); ++i) {
    nodeIndex[nodes[i]] = i;
  }

  ModuleOp module = getOperation();
  OpBuilder builder(module.getContext());
  for (const auto &decision : decisions) {
    auto it = nodeIndex.find(decision.chosenNode);
    unsigned idx = (it != nodeIndex.end()) ? it->second : 0u;
    if (Operation *op = decision.dbAllocOp) {
      op->setAttr("node", IntegerAttr::get(builder.getI32Type(), (int32_t)idx));
    }
  }
}

void ConcurrencyPass::emitDebugOutput(
    func::FuncOp func, const EdtPlacementHeuristics &edtPlacer,
    const SmallVector<PlacementDecision, 16> &edtDecisions,
    const DbPlacementHeuristics &dbPlacer,
    const SmallVector<DbPlacementDecision, 16> &dbDecisions) {
  auto &concurrencyGraph = AM->getConcurrencyGraph(func);

  ARTS_DEBUG_SECTION("edt-concurrency-pass", {
    std::string s;
    llvm::raw_string_ostream os(s);
    concurrencyGraph.print(os);
    ARTS_DBGS() << os.str();
  });

  ARTS_DEBUG_SECTION("edt-placement-json", {
    std::string js;
    llvm::raw_string_ostream jos(js);
    edtPlacer.exportToJson(func, edtDecisions, jos);
    ARTS_DBGS() << js;
  });

  ARTS_DEBUG_SECTION("db-placement-json", {
    std::string js;
    llvm::raw_string_ostream jos(js);
    dbPlacer.exportToJson(func, dbDecisions, jos);
    ARTS_DBGS() << js;
  });
}

//==========================================================================
// ParallelEdtLowerer Implementation
//==========================================================================

void ParallelEdtLowerer::lowerParallelEdt(EdtOp op) {
  /// Validate input EDT
  if (!op.getRegion().hasOneBlock()) {
    ARTS_ERROR("Parallel EDT must have exactly one block");
    assert(false);
    return;
  }

  /// Create epoch to wrap the parallel execution
  auto epochOp = createEpochWrapper();
  Block *epochBlock = &epochOp.getRegion().front();
  AC->getBuilder().setInsertionPointToEnd(epochBlock);

  /// Determine if we have a contained ForOp for chunked execution
  ForOp containedForOp = findContainedForOp(op);

  if (containedForOp) {
    ARTS_INFO("Lowering parallel EDT with contained arts.for");
    lowerChunkedParallelEdt(op, containedForOp);
  } else {
    ARTS_INFO("Lowering simple parallel EDT");
    lowerSimpleParallelEdt(op);
  }

  /// Complete the epoch and clean up
  AC->getBuilder().setInsertionPointToEnd(epochBlock);
  AC->create<YieldOp>(loc);
  op.erase();
}

EpochOp ParallelEdtLowerer::createEpochWrapper() {
  auto epochOp = AC->create<EpochOp>(loc);
  auto &region = epochOp.getRegion();
  if (region.empty())
    region.push_back(new Block());
  return epochOp;
}

void ParallelEdtLowerer::lowerChunkedParallelEdt(EdtOp op,
                                                 ForOp containedForOp) {
  auto [lb, ub, isInterNode] = determineParallelismStrategy();
  ARTS_INFO("Lowering parallel EDT with arts.for; bounds: " << lb << ".."
                                                            << ub);

  Value one = AC->createIndexConstant(1, loc);
  auto scfForOp = AC->create<scf::ForOp>(loc, lb, ub, one);

  OpBuilder::InsertionGuard guard(AC->getBuilder());
  AC->setInsertionPointToStart(scfForOp.getBody());

  DenseMap<Value, Value> blockArgMap = makeDependencyMap(op);
  auto chunkResult = createChunkedDeps(
      op, containedForOp, scfForOp.getInductionVar(), ub, &blockArgMap);

  /// Determine route based on parallelism level
  Value route;
  if (isInterNode) {
    /// Inter-node parallelism: route = node ID (induction variable)
    route = AC->castToInt(AC->Int32, scfForOp.getInductionVar(), loc);
  } else {
    /// Intra-node parallelism: route = 0 (all tasks stay on same node)
    route = AC->createIntConstant(0, AC->Int32, loc);
  }

  auto newEdt = AC->create<EdtOp>(loc, EdtType::task, route, chunkResult.deps);
  ARTS_INFO("Created task EDT with route " << route);

  setupEdtRegion(newEdt, op);
  moveOperationsToNewEdt(op, newEdt, containedForOp, &chunkResult.chunkInfo,
                         &chunkResult.depRemap);
}

void ParallelEdtLowerer::lowerSimpleParallelEdt(EdtOp op) {
  auto [lb, ub, isInterNode] = determineParallelismStrategy();
  ARTS_INFO("Lowering simple parallel EDT; bounds: " << lb << ".." << ub);

  Value one = AC->createIndexConstant(1, loc);
  auto scfForOp = AC->create<scf::ForOp>(loc, lb, ub, one);

  OpBuilder::InsertionGuard guard(AC->getBuilder());
  AC->setInsertionPointToStart(scfForOp.getBody());

  DenseMap<Value, Value> blockArgMap = makeDependencyMap(op);
  DenseMap<Value, Value> depRemap;
  SmallVector<Value> deps = createSimpleDeps(op, &blockArgMap, depRemap);

  /// Determine route based on parallelism level
  Value route;
  if (isInterNode) {
    /// Inter-node parallelism: route = node ID (induction variable)
    route = AC->castToInt(AC->getBuilder().getI32Type(),
                          scfForOp.getInductionVar(), loc);
    ARTS_INFO("Using route=" << route
                             << " (node ID for inter-node parallelism)");
  } else {
    /// Intra-node parallelism: route = 0 (all tasks stay on same node)
    route = AC->createIntConstant(0, AC->getBuilder().getI32Type(), loc);
  }

  auto newEdt = AC->create<EdtOp>(loc, EdtType::task, route, deps);
  ARTS_INFO("Created task EDT with route " << route);

  setupEdtRegion(newEdt, op);
  moveOperationsToNewEdt(op, newEdt, nullptr, nullptr, &depRemap);
}

void ParallelEdtLowerer::setupEdtRegion(EdtOp edt, EdtOp sourceEdt) {
  Region &dst = edt.getRegion();
  if (dst.empty())
    dst.push_back(new Block());

  Block &dstBody = dst.front();
  dstBody.clear();

  /// Copy arguments from source EDT
  auto srcArgs = sourceEdt.getRegion().front().getArguments();
  for (BlockArgument srcArg : srcArgs)
    dstBody.addArgument(srcArg.getType(), loc);
  /// to avoid duplicate terminators
}

std::tuple<Value, Value, bool>
ParallelEdtLowerer::determineParallelismStrategy() {
  Value lb = AC->createIndexConstant(0, loc);
  Value ub;
  bool isInterNode = false;

  /// Validate abstractMachine configuration
  if (!abstractMachine || !abstractMachine->isValid()) {
    ARTS_WARN("Invalid abstractMachine configuration, using runtime fallback");
    Value nodes = AC->getTotalNodes(loc);
    ub = AC->castToIndex(nodes, loc);
    isInterNode = true;
    return {lb, ub, isInterNode};
  }

  /// Determine parallelism strategy based on abstractMachine configuration
  if (abstractMachine->hasValidNodeCount()) {
    int nodeCount = abstractMachine->getNodeCount();
    if (nodeCount > 1) {
      /// Inter-node parallelism: distribute across multiple nodes
      ub = AC->createIndexConstant(nodeCount, loc);
      isInterNode = true;
      ARTS_INFO("Inter-node parallelism: distributing across " << nodeCount
                                                               << " nodes");
    } else if (abstractMachine->hasValidThreads()) {
      int threads = abstractMachine->getThreads();
      /// Intra-node parallelism: distribute across threads within single node
      ub = AC->createIndexConstant(threads, loc);
      ARTS_INFO("Intra-node parallelism: distributing across " << threads
                                                               << " threads");
    } else {
      /// Fallback to runtime workers
      Value workers = AC->getTotalWorkers(loc);
      ub = AC->castToIndex(workers, loc);
      ARTS_INFO("Unknown thread count: using runtime workers");
    }
  } else {
    /// Fallback to runtime nodes
    Value nodes = AC->getTotalNodes(loc);
    ub = AC->castToIndex(nodes, loc);
    isInterNode = true;
    ARTS_INFO("Unknown node count: using runtime nodes");
  }

  return {lb, ub, isInterNode};
}

ForOp ParallelEdtLowerer::findContainedForOp(EdtOp op) {
  ForOp containedForOp = nullptr;
  op.walk([&](ForOp forOp) {
    if (!containedForOp)
      containedForOp = forOp;
    return WalkResult::advance();
  });
  return containedForOp;
}

ParallelEdtLowerer::ChunkedDepsResult
ParallelEdtLowerer::createChunkedDeps(EdtOp op, ForOp containedForOp,
                                      Value workerId, Value totalWorkers,
                                      DenseMap<Value, Value> *blockArgMap) {
  ChunkedDepsResult result;

  /// Extract loop bounds and step
  Value flb = containedForOp.getLowerBound()[0];
  Value fub = containedForOp.getUpperBound()[0];
  Value fst = containedForOp.getStep()[0];

  /// Materialize values in the new context
  DenseMap<Value, Value> valueCache;
  flb = materializeValue(flb, valueCache, blockArgMap);
  fub = materializeValue(fub, valueCache, blockArgMap);
  fst = materializeValue(fst, valueCache, blockArgMap);

  /// Create chunking computation
  ChunkedForInfo chunkInfo =
      computeChunkBounds(flb, fub, fst, workerId, totalWorkers);

  /// Process dependencies
  ValueRange deps = op.getDependencies();
  for (auto [index, dep] : llvm::enumerate(deps)) {
    Value newDep =
        processDependencyForChunking(dep, chunkInfo, valueCache, blockArgMap);
    result.deps.push_back(newDep);
  }

  result.chunkInfo = chunkInfo;
  return result;
}

ParallelEdtLowerer::ChunkedForInfo
ParallelEdtLowerer::computeChunkBounds(Value flb, Value fub, Value fst,
                                       Value workerId, Value totalWorkers) {
  /// Compute span and chunk size
  Value span = AC->create<arith::SubIOp>(loc, fub, flb);
  Value chunkSize = AC->create<arith::DivUIOp>(loc, span, totalWorkers);
  Value remainder = AC->create<arith::RemUIOp>(loc, span, totalWorkers);

  /// Compute worker-specific bounds
  ChunkedForInfo info;
  info.span = AC->create<arith::AddIOp>(
      loc, chunkSize,
      AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::ult, workerId,
                                remainder)
          .getResult());

  info.lowerBound = AC->create<arith::AddIOp>(
      loc, flb, AC->create<arith::MulIOp>(loc, workerId, chunkSize));
  info.upperBound = AC->create<arith::AddIOp>(loc, info.lowerBound, info.span);
  info.step = fst;

  return info;
}

Value ParallelEdtLowerer::processDependencyForChunking(
    Value dep, ChunkedForInfo chunkInfo, DenseMap<Value, Value> &valueCache,
    DenseMap<Value, Value> *blockArgMap) {

  /// For DbAcquire dependencies, create chunked acquire with worker-specific
  /// bounds
  if (auto acq = dep.getDefiningOp<DbAcquireOp>()) {
    Value guid = materializeValue(acq.getSourceGuid(), valueCache, blockArgMap);
    Value ptr = materializeValue(acq.getSourcePtr(), valueCache, blockArgMap);
    auto newAcq = AC->create<DbAcquireOp>(
        loc, ArtsMode::in, guid, ptr, SmallVector<Value>{chunkInfo.lowerBound},
        SmallVector<Value>{chunkInfo.upperBound},
        SmallVector<Value>{chunkInfo.step});
    return newAcq.getResult(0);
  }

  /// For non-DbAcquire dependencies, just materialize them
  return materializeValue(dep, valueCache, blockArgMap);
}

SmallVector<Value>
ParallelEdtLowerer::createSimpleDeps(EdtOp op,
                                     DenseMap<Value, Value> *blockArgMap,
                                     DenseMap<Value, Value> &depRemap) {
  SmallVector<Value> deps;
  ValueRange originalDeps = op.getDependencies();

  DenseMap<Value, Value> valueCache;
  for (auto [index, dep] : llvm::enumerate(originalDeps))
    deps.push_back(materializeValue(dep, valueCache, blockArgMap));

  return deps;
}

void ParallelEdtLowerer::moveOperationsToNewEdt(
    EdtOp sourceOp, EdtOp newEdt, ForOp containedForOp,
    const ChunkedForInfo *chunkInfo, DenseMap<Value, Value> *depRemap) {
  Block &dstBody = newEdt.getRegion().front();
  Block &srcBody = sourceOp.getRegion().front();

  IRMapping mapper;
  /// Map source EDT block arguments to destination EDT block arguments
  DenseMap<Value, Value> blockArgRemap;
  for (auto [srcArg, dstArg] :
       zip(srcBody.getArguments(), dstBody.getArguments())) {
    mapper.map(srcArg, dstArg);
    blockArgRemap[srcArg] = dstArg;
  }

  OpBuilder bodyBuilder(newEdt);
  bodyBuilder.setInsertionPointToEnd(&dstBody);

  if (depRemap)
    for (auto &entry : *depRemap)
      mapper.map(entry.first, entry.second);

  SmallVector<Operation *, 16> originalOps;
  for (Operation &op : srcBody.without_terminator())
    originalOps.push_back(&op);

  for (Operation *origOp : originalOps) {
    bodyBuilder.setInsertionPointToEnd(&dstBody);

    /// Handle contained ForOp specially
    if (containedForOp && origOp == containedForOp.getOperation()) {
      DenseMap<Value, Value> rematCache;
      Location forLoc = origOp->getLoc();

      Value originalLower = materializeValue(containedForOp.getLowerBound()[0],
                                             rematCache, &blockArgRemap);
      Value originalUpper = materializeValue(containedForOp.getUpperBound()[0],
                                             rematCache, &blockArgRemap);
      Value step = materializeValue(containedForOp.getStep()[0], rematCache,
                                    &blockArgRemap);

      Value workerIndex = AC->getCurrentWorker(forLoc);
      Value totalWorkers = AC->getTotalWorkers(forLoc);

      Value one = AC->createIndexConstant(1, forLoc);

      Value workersSafe = AC->create<arith::SelectOp>(
          forLoc,
          AC->create<arith::CmpIOp>(forLoc, arith::CmpIPredicate::slt,
                                    totalWorkers, one),
          one, totalWorkers);

      Value span =
          AC->create<arith::SubIOp>(forLoc, originalUpper, originalLower);
      Value chunkSize = AC->create<arith::DivUIOp>(forLoc, span, workersSafe);
      Value remainder = AC->create<arith::RemUIOp>(forLoc, span, workersSafe);

      Value workerChunkSize = AC->create<arith::AddIOp>(
          forLoc, chunkSize,
          AC->create<arith::CmpIOp>(forLoc, arith::CmpIPredicate::ult,
                                    workerIndex, remainder)
              .getResult());

      Value workerStart = AC->create<arith::AddIOp>(
          forLoc, originalLower,
          AC->create<arith::MulIOp>(forLoc, workerIndex, chunkSize));

      Value workerEnd =
          AC->create<arith::AddIOp>(forLoc, workerStart, workerChunkSize);

      auto newFor = AC->create<ForOp>(forLoc, SmallVector<Value>{workerStart},
                                      SmallVector<Value>{workerEnd},
                                      SmallVector<Value>{step}, nullptr);
      Region &newRegion = newFor.getRegion();
      if (newRegion.empty())
        newRegion.push_back(new Block());
      Block &newBlock = newRegion.front();
      Block &oldBlock = containedForOp.getRegion().front();

      while (newBlock.getNumArguments() < oldBlock.getNumArguments())
        newBlock.addArgument(AC->getBuilder().getIndexType(), forLoc);

      IRMapping loopMapper(mapper);
      for (auto [oldArg, newArg] :
           zip(oldBlock.getArguments(), newBlock.getArguments()))
        loopMapper.map(oldArg, newArg);

      OpBuilder forBodyBuilder(newFor);
      forBodyBuilder.setInsertionPointToEnd(&newBlock);
      for (Operation &inner : oldBlock.without_terminator())
        forBodyBuilder.clone(inner, loopMapper);
      if (newBlock.empty() || !isa<YieldOp>(newBlock.back()))
        forBodyBuilder.create<YieldOp>(forLoc);

      if (containedForOp->getNumResults() == newFor->getNumResults())
        for (auto [oldRes, newRes] :
             llvm::zip(containedForOp->getResults(), newFor->getResults()))
          mapper.map(oldRes, newRes);
      continue;
    }

    /// Clone other operations
    Operation *cloned = bodyBuilder.clone(*origOp, mapper);
    SmallVector<Value, 4> replacements;
    for (auto [oldRes, newRes] :
         llvm::zip(origOp->getResults(), cloned->getResults())) {
      mapper.map(oldRes, newRes);
      replacements.push_back(newRes);
    }
    if (!replacements.empty())
      origOp->replaceAllUsesWith(replacements);
  }

  /// Clean up original operations
  for (auto it = originalOps.rbegin(); it != originalOps.rend(); ++it) {
    Operation *origOp = *it;
    origOp->erase();
  }

  /// Ensure proper terminator
  if (dstBody.empty() || !isa<YieldOp>(dstBody.back())) {
    bodyBuilder.setInsertionPointToEnd(&dstBody);
    bodyBuilder.create<YieldOp>(newEdt.getLoc());
  }
}

DenseMap<Value, Value> ParallelEdtLowerer::makeDependencyMap(EdtOp op) {
  DenseMap<Value, Value> mapping;
  ValueRange deps = op.getDependencies();
  Block &srcBlock = op.getRegion().front();
  for (auto [index, arg] : enumerate(srcBlock.getArguments())) {
    if (index < deps.size())
      mapping[arg] = deps[index];
  }
  return mapping;
}

Value ParallelEdtLowerer::materializeValue(
    Value val, DenseMap<Value, Value> &cache,
    DenseMap<Value, Value> *blockArgMap) {
  if (cache.count(val))
    return cache[val];

  if (blockArgMap && blockArgMap->count(val))
    return (*blockArgMap)[val];

  if (auto constOp = val.getDefiningOp<arith::ConstantOp>()) {
    Value newConst = AC->getBuilder().clone(*constOp)->getResult(0);
    cache[val] = newConst;
    return newConst;
  }

  if (auto blockArg = val.dyn_cast<BlockArgument>()) {
    if (blockArgMap) {
      auto it = blockArgMap->find(val);
      if (it != blockArgMap->end()) {
        cache[val] = it->second;
        return it->second;
      }
    }
  }

  cache[val] = val;
  return val;
}

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createConcurrencyPass(ArtsAnalysisManager *AM) {
  return std::make_unique<ConcurrencyPass>(AM);
}

} // namespace arts
} // namespace mlir
