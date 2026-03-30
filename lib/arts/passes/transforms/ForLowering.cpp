///==========================================================================///
/// File: ForLowering.cpp
///
/// Lowers arts.for operations within EDT regions (parallel/task)
/// 1. Splits parallel regions at arts.for boundaries
/// 2. Creates EpochOp wrappers with task EDTs for loop iterations
/// 3. Creates continuation parallel EDTs for post-loop work
/// 4. Rewires DB acquires directly to DbAllocOp (not through block arguments)
///
/// Transformation:
///   BEFORE: edt.parallel { work_1; arts.for {...}; work_2 }
///   AFTER:  edt.parallel { work_1 }
///           arts.epoch { scf.for { edt.task {...} }; edt.task<result> }
///           edt.parallel { work_2 }  /// with reacquired DBs from DbAllocOp
///
/// Worker partitioning uses block distribution:
///   - blockSize = ceil(totalIterations / numWorkers)
///   - Each worker processes iterations [start, start + count)
///
/// Example:
///   one `arts.for` inside `arts.edt<parallel>` -> epoch + per-worker task EDTs
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/AccessPatternAnalysis.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/heuristics/DistributionHeuristics.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#define GEN_PASS_DEF_FORLOWERING
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/transforms/edt/EdtParallelSplitLowering.h"
#include "arts/transforms/edt/EdtReductionLowering.h"
#include "arts/transforms/edt/EdtRewriter.h"
#include "arts/transforms/edt/EdtTaskLoopLowering.h"
#include "arts/transforms/edt/WorkDistributionUtils.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/EdtUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "mlir/Pass/Pass.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(for_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;

namespace {

static std::optional<unsigned> inferLoopMappedDim(DbAcquireNode *acqNode,
                                                  ForOp forOp) {
  if (!acqNode || !forOp)
    return std::nullopt;

  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() == 0)
    return std::nullopt;

  Value loopIV = forBody.getArgument(0);
  if (auto mappedDim =
          acqNode->getPartitionOffsetDim(loopIV, /*requireLeading=*/false)) {
    return mappedDim;
  }

  Region &forRegion = forOp.getRegion();
  std::optional<unsigned> mappedDim;
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  acqNode->collectAccessOperations(dbRefToMemOps);

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      Region *memRegion = memOp ? memOp->getParentRegion() : nullptr;
      if (!memRegion)
        continue;
      if (memRegion != &forRegion && !forRegion.isAncestor(memRegion))
        continue;

      SmallVector<Value> fullChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (fullChain.empty())
        continue;

      unsigned memrefStart = dbRef.getIndices().size();
      if (memrefStart >= fullChain.size())
        continue;

      SmallVector<unsigned, 2> matchingDims;
      ArrayRef<Value> memrefChain(fullChain);
      memrefChain = memrefChain.drop_front(memrefStart);
      for (auto [dimIdx, indexVal] : llvm::enumerate(memrefChain)) {
        if (ValueAnalysis::dependsOn(
                ValueAnalysis::stripNumericCasts(indexVal), loopIV))
          matchingDims.push_back(dimIdx);
      }

      if (matchingDims.size() != 1)
        return std::nullopt;

      unsigned dim = matchingDims.front();
      if (!mappedDim)
        mappedDim = dim;
      else if (*mappedDim != dim)
        return std::nullopt;
    }
  }

  return mappedDim;
}

static std::optional<unsigned> inferLoopMappedDimFromValue(Value dep,
                                                           ForOp forOp) {
  if (!dep || !forOp)
    return std::nullopt;

  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() == 0)
    return std::nullopt;

  Value loopIV = forBody.getArgument(0);
  Region &forRegion = forOp.getRegion();
  std::optional<unsigned> mappedDim;
  llvm::SetVector<Value> sources;
  sources.insert(dep);
  for (Operation *user : dep.getUsers()) {
    auto edt = dyn_cast<EdtOp>(user);
    if (!edt)
      continue;

    ValueRange deps = edt.getDependencies();
    Block &edtBody = edt.getBody().front();
    unsigned argCount = edtBody.getNumArguments();
    for (auto [idx, operand] : llvm::enumerate(deps)) {
      if (operand != dep || idx >= argCount)
        continue;
      sources.insert(edtBody.getArgument(static_cast<unsigned>(idx)));
    }
  }

  llvm::SetVector<Operation *> memOps;
  for (Value source : sources)
    DbUtils::collectReachableMemoryOps(source, memOps, /*scope=*/nullptr);

  for (Operation *memOp : memOps) {
    Region *memRegion = memOp ? memOp->getParentRegion() : nullptr;
    if (!memRegion)
      continue;
    if (memRegion != &forRegion && !forRegion.isAncestor(memRegion))
      continue;

    std::optional<DbUtils::MemoryAccessInfo> access =
        DbUtils::getMemoryAccessInfo(memOp);
    if (!access || access->indices.empty())
      continue;

    SmallVector<unsigned, 2> matchingDims;
    for (auto [dimIdx, indexVal] : llvm::enumerate(access->indices)) {
      if (ValueAnalysis::dependsOn(
              ValueAnalysis::stripNumericCasts(indexVal), loopIV))
        matchingDims.push_back(dimIdx);
    }

    if (matchingDims.size() != 1)
      return std::nullopt;

    unsigned dim = matchingDims.front();
    if (!mappedDim)
      mappedDim = dim;
    else if (*mappedDim != dim)
      return std::nullopt;
  }

  return mappedDim;
}

static std::optional<unsigned>
inferAcquireMappedDim(DbAnalysis *dbAnalysis, DbAcquireOp acquire,
                      ForOp forOp) {
  if (!acquire || !forOp)
    return std::nullopt;

  if (dbAnalysis)
    if (func::FuncOp func = acquire->getParentOfType<func::FuncOp>()) {
      DbGraph &graph = dbAnalysis->getOrCreateGraph(func);
      if (DbAcquireNode *acqNode = graph.getDbAcquireNode(acquire))
        if (auto mappedDim = inferLoopMappedDim(acqNode, forOp))
          return mappedDim;
    }

  return inferLoopMappedDimFromValue(acquire.getPtr(), forOp);
}

static std::optional<AccessBoundsResult>
inferLoopHaloBounds(DbAcquireNode *acqNode, ForOp forOp, unsigned mappedDim) {
  if (!acqNode || !forOp)
    return std::nullopt;

  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() == 0)
    return std::nullopt;

  if (acqNode->hasIndirectAccess())
    return std::nullopt;

  Value loopIV = forBody.getArgument(0);
  Region &forRegion = forOp.getRegion();
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

      SmallVector<Value> fullChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      if (fullChain.empty())
        continue;

      AccessIndexInfo info;
      info.dbRefPrefix = dbRef.getIndices().size();
      info.indexChain.append(fullChain.begin(), fullChain.end());
      accesses.push_back(std::move(info));
    }
  }

  if (accesses.empty())
    return std::nullopt;

  AccessBoundsResult bounds =
      analyzeAccessBoundsFromIndices(accesses, loopIV, loopIV, mappedDim);
  if (!bounds.valid || (bounds.minOffset == 0 && bounds.maxOffset == 0))
    return std::nullopt;
  return bounds;
}

static std::optional<AccessBoundsResult>
inferLoopHaloBoundsFromValue(Value dep, ForOp forOp, unsigned mappedDim) {
  if (!dep || !forOp)
    return std::nullopt;

  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() == 0)
    return std::nullopt;

  Value loopIV = forBody.getArgument(0);
  Region &forRegion = forOp.getRegion();
  llvm::SetVector<Operation *> memOps;
  DbUtils::collectReachableMemoryOps(dep, memOps, /*scope=*/nullptr);

  SmallVector<AccessIndexInfo, 16> accesses;
  for (Operation *memOp : memOps) {
    Region *memRegion = memOp ? memOp->getParentRegion() : nullptr;
    if (!memRegion)
      continue;
    if (memRegion != &forRegion && !forRegion.isAncestor(memRegion))
      continue;

    std::optional<DbUtils::MemoryAccessInfo> access =
        DbUtils::getMemoryAccessInfo(memOp);
    if (!access || access->indices.empty())
      continue;

    AccessIndexInfo info;
    info.dbRefPrefix = 0;
    info.indexChain.append(access->indices.begin(), access->indices.end());
    accesses.push_back(std::move(info));
  }

  if (accesses.empty())
    return std::nullopt;

  AccessBoundsResult bounds =
      analyzeAccessBoundsFromIndices(accesses, loopIV, loopIV, mappedDim);
  if (!bounds.valid || (bounds.minOffset == 0 && bounds.maxOffset == 0))
    return std::nullopt;
  return bounds;
}

///===----------------------------------------------------------------------===///
/// LoopInfo - Information about a loop within a parallel EDT
///
/// LoopInfo encapsulates worker partitioning for arts.for inside a parallel
/// EDT. It implements a simple block distribution:
///   - blockSizeCeil = ceil(totalIterations / numWorkers)
///   - start = workerId * blockSizeCeil
///   - count = min(blockSizeCeil, max(0, totalIterations - start))
///   - hasWork = start < totalIterations
///===----------------------------------------------------------------------===///
class LoopInfo {
public:
  LoopInfo(ArtsCodegen *AC, ForOp forOp, Value numWorkers,
           Value runtimeBlockSizeHint = Value())
      : AC(AC), forOp(forOp) {
    lowerBound = forOp.getLowerBound()[0];
    upperBound = forOp.getUpperBound()[0];
    loopStep = forOp.getStep()[0];
    depPattern = getEffectiveDepPattern(forOp.getOperation());
    totalWorkers = numWorkers;
    LoopChunkPlan chunkPlan = WorkDistributionUtils::planLoopChunking(
        AC, forOp, runtimeBlockSizeHint);
    blockSize = chunkPlan.blockSize;
    chunkLowerBound = chunkPlan.chunkLowerBound;
    totalIterations = chunkPlan.totalIterations;
    totalChunks = chunkPlan.totalChunks;
    useAlignedLowerBound = chunkPlan.useAlignedLowerBound;
    useRuntimeBlockAlignment = chunkPlan.useRuntimeBlockAlignment;
    alignmentBlockSize = chunkPlan.alignmentBlockSize;
  }

  /// Attributes
  ArtsCodegen *AC;

  /// Loop information
  ForOp forOp;
  Value lowerBound, upperBound, loopStep;
  /// Base lower bound used for chunking (may be aligned to block size)
  Value chunkLowerBound;
  bool useAlignedLowerBound = false;
  bool useRuntimeBlockAlignment = false;
  std::optional<int64_t> alignmentBlockSize;
  /// Distribution information
  Value blockSize, totalWorkers, totalIterations, totalChunks;
  std::optional<ArtsDepPattern> depPattern;

  /// Distribution strategy and bounds from DistributionHeuristics
  DistributionStrategy strategy;
  DistributionBounds bounds;       ///  Set by computeBounds()
  DistributionBounds insideBounds; ///  Set by recomputeBoundsInside()
};

using ReductionInfo = ReductionLoweringInfo;
using ParallelRegionAnalysis = ParallelRegionSplitAnalysis;

/// Collect external values needed by operations in a block (including nested
/// regions).
static void collectExternalValues(Block &sourceBlock, Region *boundaryRegion,
                                  SetVector<Value> &externalValues,
                                  const DenseSet<Operation *> &opsToSkip) {
  Region *sourceRegion = sourceBlock.getParent();

  for (Operation &op : sourceBlock.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;

    op.walk([&](Operation *nestedOp) {
      for (Value operand : nestedOp->getOperands()) {
        if (auto blockArg = dyn_cast<BlockArgument>(operand)) {
          Region *ownerRegion = blockArg.getOwner()->getParent();
          if (sourceRegion->isAncestor(ownerRegion))
            continue;
          if (boundaryRegion->isAncestor(ownerRegion))
            continue;
        }
        if (Operation *defOp = operand.getDefiningOp()) {
          if (sourceRegion->isAncestor(defOp->getParentRegion()))
            continue;
          if (!boundaryRegion->isAncestor(defOp->getParentRegion()))
            externalValues.insert(operand);
        }
      }
    });
  }
}

/// Clone external stack allocations into the EDT region and remap uses.
static void cloneExternalAllocasIntoEdt(Region *taskEdtRegion, Block &taskBlock,
                                        IRMapping &mapper, OpBuilder &builder) {
  DenseMap<Operation *, SmallVector<Operation *, 4>> usesByAlloca;

  taskBlock.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      auto allocaOp = operand.getDefiningOp<memref::AllocaOp>();
      if (!allocaOp)
        continue;
      if (taskEdtRegion->isAncestor(allocaOp->getParentRegion()))
        continue;
      usesByAlloca[allocaOp.getOperation()].push_back(op);
    }
  });

  if (usesByAlloca.empty())
    return;

  ARTS_DEBUG("  - Cloning " << usesByAlloca.size()
                            << " external stack allocas into EDT");

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&taskBlock);

  for (const auto &entry : usesByAlloca) {
    auto allocaOp = cast<memref::AllocaOp>(entry.first);
    Value originalMem = allocaOp.getResult();

    bool hasStoreInEdt = false;
    for (Operation *user : entry.second) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() == originalMem) {
          hasStoreInEdt = true;
          break;
        }
      }
    }

    bool hasStoreOutsideEdt = false;
    for (Operation *user : allocaOp->getUsers()) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() != originalMem)
          continue;
        if (!taskEdtRegion->isAncestor(store->getParentRegion())) {
          hasStoreOutsideEdt = true;
          break;
        }
      }
    }

    /// If the alloca is initialized outside and only read inside the EDT,
    /// keep the original alloca to preserve initialized values.
    Region *allocaRegion = allocaOp->getParentRegion();
    bool allocaVisible =
        allocaRegion && allocaRegion->isAncestor(taskEdtRegion);

    /// Clone safe initialization stores for this alloca when available.
    SmallVector<memref::StoreOp, 4> initStores;
    bool hasUnsafeStore = false;
    for (Operation *user : allocaOp->getUsers()) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() != originalMem)
          continue;
        if (!canCloneAllocaInitStore(store, originalMem)) {
          hasUnsafeStore = true;
          break;
        }
        initStores.push_back(store);
      }
    }

    if (hasStoreOutsideEdt && !hasStoreInEdt && allocaVisible &&
        (hasUnsafeStore || initStores.empty())) {
      continue;
    }

    Operation *clonedOp = builder.clone(*allocaOp.getOperation(), mapper);
    auto newAlloca = cast<memref::AllocaOp>(clonedOp);
    Value clonedMem = newAlloca.getResult();

    for (Operation *user : entry.second)
      user->replaceUsesOfWith(originalMem, clonedMem);

    mapper.map(originalMem, clonedMem);

    if (!hasUnsafeStore && !initStores.empty()) {
      IRMapping storeMapper(mapper);
      storeMapper.map(allocaOp.getResult(), newAlloca.getResult());
      Operation *insertBefore = newAlloca->getNextNode();
      func::FuncOp parentFunc = taskEdtRegion->getParentOfType<func::FuncOp>();
      std::optional<DominanceInfo> domInfo;
      if (insertBefore && parentFunc)
        domInfo.emplace(parentFunc);

      builder.setInsertionPointAfter(newAlloca);
      for (memref::StoreOp store : initStores) {
        IRMapping thisStoreMapper(storeMapper);
        bool canCloneStore = true;
        if (insertBefore && domInfo) {
          for (Value operand : store->getOperands()) {
            if (operand == originalMem)
              continue;
            Value mapped = thisStoreMapper.lookupOrDefault(operand);
            if (!mapped) {
              canCloneStore = false;
              break;
            }
            if (domInfo->properlyDominates(mapped, insertBefore) ||
                domInfo->dominates(mapped, insertBefore))
              continue;
            Value rematerialized = ValueAnalysis::traceValueToDominating(
                mapped, insertBefore, builder, *domInfo, store.getLoc());
            if (!rematerialized) {
              canCloneStore = false;
              break;
            }
            thisStoreMapper.map(operand, rematerialized);
          }
        }
        if (!canCloneStore)
          continue;
        builder.clone(*store.getOperation(), thisStoreMapper);
      }
      builder.setInsertionPointToStart(&taskBlock);
    }
  }
}

/// ForLowering Pass Implementation
struct ForLoweringPass : public impl::ForLoweringBase<ForLoweringPass> {
  explicit ForLoweringPass(mlir::arts::AnalysisManager *AM = nullptr)
      : AM(AM) {}
  void runOnOperation() override;

private:
  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;

  void gatherLowerableEdts(SmallVectorImpl<EdtOp> &lowerableEdts);
  void lowerCollectedEdts(ArrayRef<EdtOp> lowerableEdts);

  /// Process a parallel EDT containing arts_for operations
  void lowerParallelEdt(EdtOp parallelEdt);

  /// Clone loop body into task EDT's scf.for
  void cloneLoopBody(ArtsCodegen *AC, ForOp forOp, scf::ForOp chunkLoop,
                     Value chunkOffset, IRMapping &mapper);

  /// Reduction Support

  /// Allocate partial accumulators for reductions (one per worker)
  /// If splitMode is true, skip creating DbAcquireOps as dependencies to the
  /// parallel EDT (used when the parallel will be erased in split pattern)
  ReductionInfo
  allocatePartialAccumulators(ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt,
                              Location loc, bool splitMode = false,
                              Value workerCountOverride = Value());

  void createResultEdt(ArtsCodegen *AC, ReductionInfo &redInfo, Location loc);

  Attribute getLoopMetadataAttr(ForOp forOp);

  /// Lower an arts.for with DB acquires rewired directly to DbAllocOp.
  /// This is used when splitting the parallel region - the for body is
  /// extracted outside the parallel EDT and acquires DBs directly.
  void lowerForWithDbRewiring(ArtsCodegen &AC, ForOp forOp,
                              EdtOp originalParallel,
                              ParallelRegionAnalysis &analysis, Location loc);

  /// Create task EDT with DB dependencies rewired to DbAllocOp.
  EdtOp createTaskEdtWithRewiring(ArtsCodegen *AC, LoopInfo &loopInfo,
                                  ForOp forOp, Value workerIdPlaceholder,
                                  EdtOp originalParallel,
                                  bool singleDispatchLane,
                                  ReductionInfo &redInfo);
};

} // namespace

/// LoopInfo computes how to distribute loop iterations across workers using
/// block distribution. This ensures balanced work with minimal overhead.

///===----------------------------------------------------------------------===///
/// Pass Entry Point
///===----------------------------------------------------------------------===///

void ForLoweringPass::runOnOperation() {
  module = getOperation();

  /// Phase 1: Gather candidate EDTs carrying arts.for operations.
  SmallVector<EdtOp, 4> lowerableEdts;
  gatherLowerableEdts(lowerableEdts);

  /// Skip pass bookkeeping/logging entirely when no arts.for is present.
  if (lowerableEdts.empty()) {
    ARTS_DEBUG("No arts.for operations found, skipping ForLowering");
    return;
  }

  ARTS_INFO_HEADER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););

  /// Phase 2-4: Evaluate lowering policy, build rewrite plans, and apply.
  lowerCollectedEdts(lowerableEdts);

  ARTS_INFO_FOOTER(ForLowering);
  ARTS_DEBUG_REGION(module.dump(););
}

void ForLoweringPass::gatherLowerableEdts(
    SmallVectorImpl<EdtOp> &lowerableEdts) {
  module.walk([&](EdtOp edt) {
    if (edt.getType() != EdtType::parallel && edt.getType() != EdtType::task)
      return;

    bool hasForOps = false;
    edt.getBody().walk([&](ForOp) { hasForOps = true; });
    if (hasForOps)
      lowerableEdts.push_back(edt);
  });
}

void ForLoweringPass::lowerCollectedEdts(ArrayRef<EdtOp> lowerableEdts) {
  for (EdtOp parallelEdt : lowerableEdts)
    lowerParallelEdt(parallelEdt);
}

void ForLoweringPass::lowerParallelEdt(EdtOp parallelEdt) {
  ARTS_INFO("Lowering parallel EDT with ALWAYS-SPLIT pattern");

  /// Analyze the parallel EDT structure to find arts.for operations
  /// and categorize operations as before/after the for loop
  ParallelRegionAnalysis analysis =
      ParallelRegionAnalysis::analyze(parallelEdt);

  if (!analysis.needsSplit()) {
    ARTS_DEBUG(" - No arts.for operations found, skipping");
    return;
  }

  ARTS_INFO(" - Found " << analysis.forOps.size() << " arts.for operation(s)");

  /// Analyze which DBs are used by for and post-for operations
  analysis.analyzeDependenciesForSplit(parallelEdt);

  bool hasPreFor = analysis.hasWorkBefore();
  bool hasPostFor = analysis.hasWorkAfter();

  ARTS_INFO(" - Pre-for work: " << (hasPreFor ? "yes" : "no")
                                << ", Post-for work: "
                                << (hasPostFor ? "yes" : "no"));
  ARTS_DEBUG(" - Deps used by for: " << analysis.depsUsedByFor.size());
  ARTS_DEBUG(" - Deps used after for: " << analysis.depsUsedAfterFor.size());

  ArtsCodegen AC(module);
  if (AM)
    AC.setAbstractMachine(&AM->getAbstractMachine());
  Location loc = parallelEdt.getLoc();

  /// Extract for-body outside the parallel EDT
  /// The transformation is:
  /// - Pre-for work stays in original parallel (if any)
  /// - For-body becomes: EpochOp { scf.for { task EDTs } + result EDT }
  /// - Post-for work goes into a new continuation parallel (if any)

  /// Step 1: Set insertion point after original parallel EDT
  AC.setInsertionPointAfter(parallelEdt);

  /// Step 2: Lower each arts.for with DB rewiring (create epoch + tasks)
  /// This creates the lowered for structure AFTER the parallel EDT
  for (ForOp forOp : analysis.forOps)
    lowerForWithDbRewiring(AC, forOp, parallelEdt, analysis, loc);

  /// Step 3: Create continuation parallel for post-for work (if any)
  /// Skip if opsAfterFor only contains DB release cleanup operations.
  /// Keep barrier-only continuations intact because barrier semantics may be
  /// required by timestep-style loops (e.g., stencil updates) between epochs.
  if (hasPostFor) {
    bool onlyCleanup = llvm::all_of(analysis.opsAfterFor, [](Operation *op) {
      return isa<DbReleaseOp>(op);
    });
    if (!onlyCleanup)
      createContinuationParallel(AC, parallelEdt, analysis, loc);
  }

  /// Step 4: Clean up original parallel EDT + dependencies.
  cleanupOriginalParallel(parallelEdt, analysis, hasPreFor);

  ARTS_INFO(" - Parallel EDT lowering complete");
}

Attribute ForLoweringPass::getLoopMetadataAttr(ForOp forOp) {
  Attribute attr = forOp->getAttr(AttrNames::LoopMetadata::Name);
  if (attr)
    return attr;
  MetadataManager manager(forOp.getContext());
  manager.ensureLoopMetadata(forOp);
  return forOp->getAttr(AttrNames::LoopMetadata::Name);
}

void ForLoweringPass::cloneLoopBody(ArtsCodegen *AC, ForOp forOp,
                                    scf::ForOp chunkLoop, Value chunkOffset,
                                    IRMapping &mapper) {
  Location loc = forOp.getLoc();
  AC->setInsertionPointToStart(chunkLoop.getBody());

  /// Compute global index from local iteration variable
  ///  - global_iter = chunkOffset + local_iter
  ///  - global_idx = lowerBound + global_iter * step
  Value localIter = chunkLoop.getInductionVar();
  Value globalIter = AC->create<arith::AddIOp>(loc, chunkOffset, localIter);
  Value globalIterScaled =
      AC->create<arith::MulIOp>(loc, globalIter, forOp.getStep()[0]);
  Value globalIdx = AC->create<arith::AddIOp>(loc, forOp.getLowerBound()[0],
                                              globalIterScaled);

  /// Map arts.for induction variable to computed global index
  Block &forBody = forOp.getRegion().front();
  if (forBody.getNumArguments() > 0) {
    BlockArgument forIV = forBody.getArgument(0);
    mapper.map(forIV, globalIdx);
  }

  /// Collect constants used in the loop body that are defined outside
  /// and clone them inside the EDT region
  SetVector<Value> constantsToClone;
  Region *taskEdtRegion = chunkLoop->getParentRegion();
  for (Operation &op : forBody.without_terminator()) {
    for (Value operand : op.getOperands()) {
      if (Operation *defOp = operand.getDefiningOp()) {
        if (defOp->hasTrait<OpTrait::ConstantLike>()) {
          /// Check if this constant is defined outside the task EDT region
          Region *defRegion = defOp->getParentRegion();
          if (!taskEdtRegion->isAncestor(defRegion)) {
            constantsToClone.insert(operand);
          }
        }
      }
    }
  }

  /// Clone constants inside the task EDT region before cloning operations
  /// Use the builder with the correct insertion point
  for (Value constant : constantsToClone) {
    if (Operation *defOp = constant.getDefiningOp()) {
      Operation *clonedConst = AC->getBuilder().clone(*defOp);
      mapper.map(constant, clonedConst->getResult(0));
    }
  }

  /// Clone all operations from arts.for body into chunk loop
  for (Operation &op : forBody.without_terminator())
    AC->clone(op, mapper);

  ARTS_DEBUG(
      "    Cloned " << std::distance(forBody.without_terminator().begin(),
                                     forBody.without_terminator().end())
                    << " operations into chunk loop");
}

ReductionInfo ForLoweringPass::allocatePartialAccumulators(
    ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt, Location loc,
    bool splitMode, Value workerCountOverride) {
  return mlir::arts::allocatePartialAccumulators(
      AC, forOp, parallelEdt, loc, getLoopMetadataAttr(forOp), splitMode,
      workerCountOverride);
}

void ForLoweringPass::createResultEdt(ArtsCodegen *AC, ReductionInfo &redInfo,
                                      Location loc) {
  arts::createResultEdt(AC, redInfo, loc);
}

///===----------------------------------------------------------------------===///
/// Parallel Region Splitting Implementation
///===----------------------------------------------------------------------===///
void ForLoweringPass::lowerForWithDbRewiring(ArtsCodegen &AC, ForOp forOp,
                                             EdtOp originalParallel,
                                             ParallelRegionAnalysis &analysis,
                                             Location loc) {
  ARTS_INFO(" - Lowering arts.for with DB rewiring (split pattern)");

  /// Read distribution strategy selected by EdtDistributionPass.
  DistributionStrategy strategy =
      AM ? AM->getEdtHeuristics().resolveLoweringStrategy(originalParallel,
                                                          forOp)
         : DistributionHeuristics::resolveLoweringStrategy(originalParallel,
                                                           forOp);

  Value zero;
  Value one;
  std::optional<LoopInfo> loopInfoStorage;
  Value dispatchWorkers;
  ReductionInfo redInfo;

  {
    OpBuilder::InsertionGuard guard(AC.getBuilder());
    /// Values consumed by split-mode reduction allocation must dominate the
    /// allocs inserted before the original parallel EDT.
    AC.setInsertionPoint(originalParallel);

    /// Get numWorkers from explicit attrs or runtime queries.
    Value numWorkers =
        WorkDistributionUtils::getTotalWorkers(&AC, loc, originalParallel);

    /// numDbPartitions: the number of DB partition blocks (= numNodes for
    /// internode). Used to align worker chunk boundaries to DB block
    /// boundaries.
    Value numDbPartitions;
    if (originalParallel.getConcurrency() == EdtConcurrency::internode) {
      numDbPartitions = AC.castToIndex(
          AC.create<RuntimeQueryOp>(loc, RuntimeQueryKind::totalNodes)
              .getResult(),
          loc);
    } else {
      numDbPartitions = numWorkers;
    }

    zero = AC.createIndexConstant(0, loc);
    one = AC.createIndexConstant(1, loc);

    /// Determine loop chunking before building the epoch so we can size the
    /// dispatch loop and any reduction temporaries to the worker lanes that can
    /// actually receive work.
    Value runtimeBlockSize =
        WorkDistributionUtils::computeDbAlignmentBlockSize(
            forOp, originalParallel, numDbPartitions, &AC, loc,
            AM ? &AM->getDbAnalysis() : nullptr);
    loopInfoStorage.emplace(&AC, forOp, numWorkers, runtimeBlockSize);
    LoopInfo &loopInfo = *loopInfoStorage;
    loopInfo.strategy = strategy;
    dispatchWorkers = WorkDistributionUtils::getForDispatchWorkerCount(
        &AC, loc, originalParallel, loopInfo.strategy, loopInfo.totalChunks,
        loopInfo.depPattern);

    /// Allocate reduction accumulators (same as before, but place BEFORE epoch)
    /// Use splitMode=true since we're splitting the parallel EDT and will
    /// create acquires directly in result/task EDTs.
    if (!forOp.getReductionAccumulators().empty()) {
      ARTS_INFO(" - Detected reduction(s), allocating partial accumulators");
      redInfo =
          allocatePartialAccumulators(&AC, forOp, originalParallel, loc,
                                      /*splitMode=*/true, dispatchWorkers);
    }
  }

  auto isWavefrontPattern = [](Operation *op) {
    return op && getEffectiveDepPattern(op) == ArtsDepPattern::wavefront_2d;
  };
  /// Treat both a truly single-worker topology and a one-lane dispatch clamp
  /// as the trivial lowering case. The dispatch worker count can remain
  /// symbolically wrapped in min/max arithmetic even when the topology has
  /// already collapsed to one worker, so check the resolved worker topology
  /// as well as the dispatch count itself.
  const bool singleDispatchLane =
      ValueAnalysis::isOneConstant(loopInfoStorage->totalWorkers) ||
      ValueAnalysis::isOneConstant(dispatchWorkers);
  const bool reuseEnclosingEpoch =
      forOp.getReductionAccumulators().empty() &&
      originalParallel->getParentOfType<EpochOp>() &&
      (isWavefrontPattern(forOp.getOperation()) ||
       isWavefrontPattern(originalParallel.getOperation()) ||
       isWavefrontPattern(originalParallel->getParentOp()));

  std::optional<EpochOp> forEpoch;
  Block *epochBlock = nullptr;
  if (reuseEnclosingEpoch) {
    epochBlock = originalParallel->getBlock();
    AC.setInsertionPoint(originalParallel);
  } else {
    /// Create EpochOp wrapper for the for-body at the caller-managed insertion
    /// point so multiple arts.for regions preserve source order.
    forEpoch = AC.create<EpochOp>(loc);
    Region &epochRegion = forEpoch->getRegion();
    if (epochRegion.empty())
      epochRegion.push_back(new Block());
    epochBlock = &epochRegion.front();
    AC.setInsertionPointToStart(epochBlock);
  }

  /// Worker dispatch loop - emit only the worker lanes that can receive at
  /// least one chunk for this loop/distribution strategy.
  auto workerLoop = AC.create<scf::ForOp>(loc, zero, dispatchWorkers, one);
  AC.setInsertionPointToStart(workerLoop.getBody());
  Value workerIV = workerLoop.getInductionVar();

  /// Create task EDT with DB rewiring
  createTaskEdtWithRewiring(&AC, *loopInfoStorage, forOp, workerIV,
                            originalParallel, singleDispatchLane, redInfo);
  if (forEpoch)
    transferOperationContract(forOp.getOperation(), forEpoch->getOperation());

  /// After worker loop, create result EDT (if reductions)
  AC.setInsertionPointAfter(workerLoop);
  if (!redInfo.reductionVars.empty()) {
    createResultEdt(&AC, redInfo, loc);
  }

  if (forEpoch) {
    /// Close epoch with yield
    AC.setInsertionPointToEnd(epochBlock);
    AC.create<YieldOp>(loc);
    /// Set insertion point after epoch so subsequent for loops insert
    /// correctly.
    AC.setInsertionPointAfter(*forEpoch);
  } else {
    AC.setInsertionPointAfter(workerLoop);
  }
  if (!redInfo.reductionVars.empty())
    finalizeReductionAfterEpoch(&AC, redInfo, loc);

  ARTS_INFO(" - arts.for lowering with DB rewiring complete");
}

EdtOp ForLoweringPass::createTaskEdtWithRewiring(
    ArtsCodegen *AC, LoopInfo &loopInfo, ForOp forOp, Value workerIdPlaceholder,
    EdtOp originalParallel, bool singleDispatchLane, ReductionInfo &redInfo) {
  Location loc = forOp.getLoc();

  ARTS_DEBUG("  Creating task EDT with DB rewiring");

  /// Recreate total workers using shared distribution helper.
  loopInfo.totalWorkers =
      WorkDistributionUtils::getTotalWorkers(AC, loc, originalParallel);

  /// Compute workers-per-node for internode routing and TwoLevel distribution.
  /// Flat may still run in internode mode during debugging/experiments.
  Value workersPerNode;
  if (loopInfo.strategy.kind == DistributionKind::TwoLevel ||
      originalParallel.getConcurrency() == EdtConcurrency::internode) {
    workersPerNode =
        WorkDistributionUtils::getWorkersPerNode(AC, loc, originalParallel);
  }

  /// Compute worker iteration bounds using DistributionHeuristics
  loopInfo.bounds = WorkDistributionUtils::computeBounds(
      AC, loc, loopInfo.strategy, workerIdPlaceholder, loopInfo.totalWorkers,
      workersPerNode, loopInfo.totalIterations, loopInfo.totalChunks,
      loopInfo.blockSize, loopInfo.depPattern);

  Value chunkOffset = loopInfo.bounds.iterStart;
  ValueRange parentDeps = originalParallel.getDependencies();
  Block &parallelBlock = originalParallel.getRegion().front();

  IRMapping mapper;

  /// Create scf.if to conditionally create task EDT only if worker has work
  auto ifOp = AC->create<scf::IfOp>(loc, loopInfo.bounds.hasWork,
                                    /*withElseRegion=*/false);
  AC->setInsertionPointToStart(&ifOp.getThenRegion().front());

  /// Detect reduction block arguments
  DenseSet<Value> reductionBlockArgs = redInfo.reductionVars.empty()
                                           ? DenseSet<Value>{}
                                           : detectReductionBlockArgs(forOp);

  Value one = AC->createIndexConstant(1, loc);
  Value taskWorkerId = workerIdPlaceholder;
  TaskLoopLoweringInput taskLoopInput{AC,
                                      loc,
                                      loopInfo.strategy,
                                      loopInfo.bounds,
                                      loopInfo.depPattern,
                                      taskWorkerId,
                                      loopInfo.totalWorkers,
                                      workersPerNode,
                                      loopInfo.upperBound,
                                      loopInfo.lowerBound,
                                      loopInfo.chunkLowerBound
                                          ? loopInfo.chunkLowerBound
                                          : forOp.getLowerBound()[0],
                                      loopInfo.loopStep,
                                      loopInfo.blockSize,
                                      loopInfo.totalIterations,
                                      loopInfo.totalChunks,
                                      loopInfo.alignmentBlockSize,
                                      loopInfo.useRuntimeBlockAlignment,
                                      loopInfo.useAlignedLowerBound};

  std::unique_ptr<EdtTaskLoopLowering> taskLoopLowering =
      EdtTaskLoopLowering::create(loopInfo.strategy.kind);
  TaskAcquirePlanningResult acquirePlanning =
      taskLoopLowering->planAcquireRewrite(taskLoopInput, chunkOffset);
  Value workerOffsetVal = acquirePlanning.workerBaseOffset;
  Value acquireOffsetVal = acquirePlanning.acquireOffset;
  Value acquireSizeVal = acquirePlanning.acquireSize;
  Value acquireHintSizeVal = acquirePlanning.acquireHintSize;
  Value stepVal = acquirePlanning.step;
  bool stepIsUnit = acquirePlanning.stepIsUnit;
  std::optional<Tiling2DWorkerGrid> tiling2DGrid = acquirePlanning.tiling2DGrid;

  /// Acquire worker-local partial accumulator slot for reductions
  SmallVector<Value> reductionTaskDeps;
  DenseMap<Value, uint64_t> reductionVarIndex;
  std::optional<SmallVector<int64_t, 4>> refinedTaskBlockShape;

  for (uint64_t i = 0; i < redInfo.reductionVars.size(); i++) {
    /// Use partialAccumPtrs (from DbAllocOp) instead of partialAccumArgs
    /// (BlockArguments inside parallel EDT) to avoid use-after-free when
    /// erasing the parallel EDT in split mode
    Value partialPtr = i < redInfo.partialAccumPtrs.size()
                           ? redInfo.partialAccumPtrs[i]
                           : Value();
    Value partialGuid = i < redInfo.partialAccumGuids.size()
                            ? redInfo.partialAccumGuids[i]
                            : Value();
    if (!partialPtr) {
      ARTS_ERROR("Missing partial accumulator ptr for reduction " << i);
      continue;
    }

    auto partialPtrType = cast<MemRefType>(partialPtr.getType());
    auto innerMemrefType = cast<MemRefType>(partialPtrType.getElementType());

    /// Acquire the worker's slice of the partial accumulator array
    /// Source directly from DbAllocOp (partialGuid, partialPtr)
    /// Use chunked partition mode since we're acquiring by worker offset
    /// offsets=[workerId] and sizes=[1] to get this worker's slot
    auto partialAcqOp = AC->create<DbAcquireOp>(
        loc, ArtsMode::inout, partialGuid, partialPtr, innerMemrefType,
        PartitionMode::block, /*indices=*/SmallVector<Value>{},
        /*offsets=*/SmallVector<Value>{workerIdPlaceholder},
        /*sizes=*/SmallVector<Value>{one},
        /*partition_indices=*/SmallVector<Value>{},
        /*partition_offsets=*/SmallVector<Value>{},
        /*partition_sizes=*/SmallVector<Value>{});
    /// Worker-local partial reduction acquires already have the exact slot
    /// access mode and must not be re-inferred later.
    partialAcqOp.setPreserveAccessMode();

    reductionTaskDeps.push_back(partialAcqOp.getResult(1));
    reductionVarIndex[redInfo.reductionVars[i]] = i;

    ARTS_DEBUG("  - Acquired worker-local partial accumulator slot");
  }

  SmallVector<Value> taskDeps;
  SmallVector<std::pair<BlockArgument, Value>> parallelArgToAcquire;
  bool forceOwnerRoute = false;
  ARTS_DEBUG("  - Processing " << parentDeps.size()
                               << " parent dependencies with DB rewiring");

  for (auto [idx, parentDep] : llvm::enumerate(parentDeps)) {
    auto parentAcqOp = parentDep.getDefiningOp<DbAcquireOp>();
    if (!parentAcqOp) {
      ARTS_DEBUG("    - Dep " << idx << ": Not a DbAcquireOp, skipping");
      continue;
    }

    BlockArgument parallelArg = parallelBlock.getArgument(idx);
    if (shouldSkipReductionArg(parallelArg, redInfo, reductionBlockArgs))
      continue;

    /// Rewiring: Trace to DbAllocOp and acquire from there
    auto allocInfo = DbUtils::traceToDbAlloc(parentDep);
    if (!allocInfo) {
      ARTS_ERROR("Could not trace dependency " << idx << " to DbAllocOp");
      continue;
    }
    auto [rootGuid, rootPtr] = *allocInfo;
    Value rootGuidValue = rootGuid;
    Value rootPtrValue = rootPtr;

    /// Check if parent acquire already has partition hints (from CreateDbs via
    /// DbControlOp). If hints exist, the user provided explicit partitioning,
    /// so ForLowering RESPECTS the user's intent and does NOT override.
    bool parentHasPartitionInfo = parentAcqOp.hasExplicitPartitionHints();

    DbAcquireOp chunkAcqOp;
    bool chunkUsesStencilHalo = false;

    std::optional<EdtDistributionPattern> distributionPattern =
        AM ? AM->getEdtHeuristics().resolveDistributionPattern(forOp,
                                                               originalParallel)
           : DistributionHeuristics::resolveDistributionPattern(
                 nullptr, forOp, originalParallel);

    AcquireRewriteContract rewriteContract =
        resolveAcquireRewriteContract(AM, parentAcqOp);
    std::optional<unsigned> inferredMappedDim;
    if (rewriteContract.ownerDims.empty())
      if (auto mappedDim = inferAcquireMappedDim(
              AM ? &AM->getDbAnalysis() : nullptr, parentAcqOp, forOp)) {
        rewriteContract.ownerDims.push_back(
            static_cast<int64_t>(*mappedDim));
        inferredMappedDim = *mappedDim;
      }

    std::optional<unsigned> ownerDimForHalo;
    if (rewriteContract.ownerDims.size() == 1 && rewriteContract.ownerDims[0] >= 0)
      ownerDimForHalo = static_cast<unsigned>(rewriteContract.ownerDims[0]);
    else if (inferredMappedDim)
      ownerDimForHalo = *inferredMappedDim;

    if (parentAcqOp.getMode() == ArtsMode::in && ownerDimForHalo &&
        rewriteContract.ownerDims.size() <= 1 &&
        rewriteContract.haloMinOffsets.empty() &&
        rewriteContract.haloMaxOffsets.empty() && AM) {
      if (func::FuncOp func = parentAcqOp->getParentOfType<func::FuncOp>()) {
        DbGraph &graph = AM->getDbAnalysis().getOrCreateGraph(func);
        if (DbAcquireNode *acqNode = graph.getDbAcquireNode(parentAcqOp))
          if (auto bounds =
                  inferLoopHaloBounds(acqNode, forOp, *ownerDimForHalo)) {
            rewriteContract.haloMinOffsets.push_back(bounds->minOffset);
            rewriteContract.haloMaxOffsets.push_back(bounds->maxOffset);
            rewriteContract.applyStencilHalo = true;
            rewriteContract.preserveParentDepRange = false;
            rewriteContract.usePartitionSliceAsDepWindow = false;
          }
      }
      if (!rewriteContract.applyStencilHalo)
        if (auto bounds = inferLoopHaloBoundsFromValue(parentAcqOp.getPtr(),
                                                       forOp,
                                                       *ownerDimForHalo)) {
          rewriteContract.haloMinOffsets.push_back(bounds->minOffset);
          rewriteContract.haloMaxOffsets.push_back(bounds->maxOffset);
          rewriteContract.applyStencilHalo = true;
          rewriteContract.preserveParentDepRange = false;
          rewriteContract.usePartitionSliceAsDepWindow = false;
        }
    }

    TaskAcquireRewritePlanInput planningInput{
        AC,
        loc,
        parentAcqOp,
        rootGuidValue,
        rootPtrValue,
        /*forceCoarseRewrite=*/singleDispatchLane && !parentHasPartitionInfo,
        loopInfo.strategy.kind,
        distributionPattern,
        tiling2DGrid,
        rewriteContract,
        acquireOffsetVal,
        acquireSizeVal,
        acquireHintSizeVal,
        stepVal,
        stepIsUnit};

    auto createPlannedAcquire =
        [&]() -> std::pair<DbAcquireOp, TaskAcquireRewritePlan> {
      TaskAcquireRewritePlan rewritePlan = planTaskAcquireRewrite(planningInput);
      DbAcquireOp rewritten = rewriteAcquire(rewritePlan.rewriteInput,
                                             rewritePlan.useStencilRewriter);
      return {rewritten, std::move(rewritePlan)};
    };
    std::optional<SmallVector<int64_t, 4>> plannedTaskBlockShape;

    if (parentHasPartitionInfo) {
      /// USER HINT EXISTS - ForLowering RESPECTS it!
      /// Use parent's partition mode and operands (from DbControlOp via
      /// CreateDbs)
      ARTS_DEBUG("    - Respecting existing DbControlOp hint on allocation");

      /// Reuse the parent acquire's partitioning clause which came from
      /// CreateDbs
      auto parentPartMode = parentAcqOp.getPartitionMode();
      SmallVector<Value> parentIndices(parentAcqOp.getIndices().begin(),
                                       parentAcqOp.getIndices().end());
      SmallVector<Value> parentOffsets(parentAcqOp.getOffsets().begin(),
                                       parentAcqOp.getOffsets().end());
      SmallVector<Value> parentSizes(parentAcqOp.getSizes().begin(),
                                     parentAcqOp.getSizes().end());

      SmallVector<Value> parentPartIndices(
          parentAcqOp.getPartitionIndices().begin(),
          parentAcqOp.getPartitionIndices().end());
      SmallVector<Value> parentPartOffsets(
          parentAcqOp.getPartitionOffsets().begin(),
          parentAcqOp.getPartitionOffsets().end());
      SmallVector<Value> parentPartSizes(
          parentAcqOp.getPartitionSizes().begin(),
          parentAcqOp.getPartitionSizes().end());
      PartitionMode mode = parentPartMode.value_or(PartitionMode::coarse);
      bool hasExplicitRangeHints =
          !parentOffsets.empty() && !parentSizes.empty();

      /// Hinted ranges are usually element-space and still need DB-space slice
      /// planning for rec_dep/depCount correctness.
      if (mode == PartitionMode::block || mode == PartitionMode::stencil ||
          hasExplicitRangeHints) {
        auto [plannedAcquire, rewritePlan] = createPlannedAcquire();
        chunkAcqOp = plannedAcquire;
        chunkUsesStencilHalo = rewritePlan.useStencilRewriter;
        plannedTaskBlockShape = rewritePlan.refinedTaskBlockShape;
        if (parentPartMode)
          setPartitionMode(chunkAcqOp.getOperation(), mode);
        if (chunkAcqOp.getIndices().empty() && !parentIndices.empty())
          chunkAcqOp.getIndicesMutable().assign(parentIndices);
        if (chunkAcqOp.getPartitionIndices().empty() &&
            !parentPartIndices.empty())
          chunkAcqOp.getPartitionIndicesMutable().assign(parentPartIndices);
        if (chunkAcqOp.getPartitionOffsets().empty() &&
            !parentPartOffsets.empty())
          chunkAcqOp.getPartitionOffsetsMutable().assign(parentPartOffsets);
        if (chunkAcqOp.getPartitionSizes().empty() && !parentPartSizes.empty())
          chunkAcqOp.getPartitionSizesMutable().assign(parentPartSizes);
        chunkAcqOp.copyPartitionSegmentsFrom(parentAcqOp);
      } else {
        chunkAcqOp = AC->create<DbAcquireOp>(
            loc, parentAcqOp.getMode(), rootGuidValue, rootPtrValue,
            parentAcqOp.getPtr().getType(), mode, parentIndices, parentOffsets,
            parentSizes, parentPartIndices, parentPartOffsets, parentPartSizes);
        transferContract(parentAcqOp.getOperation(), chunkAcqOp.getOperation(),
                         parentAcqOp.getPtr(), chunkAcqOp.getPtr(),
                         AC->getBuilder(), loc);
        chunkAcqOp.copyPartitionSegmentsFrom(parentAcqOp);
      }

    } else {
      /// NO USER HINT - plan strategy-specific acquire rewriting externally.
      /// When the effective dispatch collapses to a single worker, keep the
      /// compiler-generated task acquire coarse. This preserves the original
      /// whole-DB contract instead of manufacturing a one-lane block slice
      /// that later passes may misinterpret as a real partitioning decision.
      auto [plannedAcquire, rewritePlan] = createPlannedAcquire();
      chunkAcqOp = plannedAcquire;
      chunkUsesStencilHalo = rewritePlan.useStencilRewriter;
      plannedTaskBlockShape = rewritePlan.refinedTaskBlockShape;
    }

    applyTaskAcquireContractMetadata(forOp.getOperation(), chunkAcqOp,
                                     planningInput.rewriteContract,
                                     plannedTaskBlockShape,
                                     AC->getBuilder(), loc);
    if (plannedTaskBlockShape)
      refinedTaskBlockShape = *plannedTaskBlockShape;

    applyTaskAcquireSlicePlan(TaskAcquireSlicePlanInput{
        AC,
        loc,
        parentAcqOp,
        chunkAcqOp,
        rootGuidValue,
        rootPtrValue,
        loopInfo.strategy.kind,
        distributionPattern,
        acquireOffsetVal,
        loopInfo.bounds.iterCount,
        acquireHintSizeVal,
        chunkUsesStencilHalo,
        planningInput.rewriteContract});

    if (originalParallel.getConcurrency() == EdtConcurrency::internode &&
        chunkAcqOp.getMode() != ArtsMode::in &&
        DbAnalysis::isCoarse(chunkAcqOp)) {
      auto allocOp = dyn_cast_or_null<DbAllocOp>(
          DbUtils::getUnderlyingDbAlloc(chunkAcqOp.getSourcePtr()));
      if (allocOp && !hasDistributedDbAllocation(allocOp.getOperation())) {
        forceOwnerRoute = true;
        ARTS_DEBUG("    - Coarse write dependency requires owner-local route");
      }
    }

    Value acquirePtr = chunkAcqOp.getResult(1);

    taskDeps.push_back(acquirePtr);
    parallelArgToAcquire.push_back({parallelArg, acquirePtr});
    ARTS_DEBUG("    - Created rewired acquire for dep " << idx);
  }

  /// When the effective dispatch collapses to one non-reduction lane, keep
  /// all distribution/acquire planning but execute the lowered loop body
  /// directly in the dispatch path instead of outlining a task EDT. This
  /// avoids per-iteration task/epoch overhead while preserving the same
  /// partitioning and contract decisions.
  if (singleDispatchLane && redInfo.reductionVars.empty()) {
    Block &forBody = forOp.getRegion().front();
    Region *directRegion = &ifOp.getThenRegion();
    Block &directBlock = directRegion->front();

    IRMapping directMapper;
    for (auto [parallelArg, acquirePtr] : parallelArgToAcquire)
      directMapper.map(parallelArg, acquirePtr);

    Value insideTotalWorkers =
        WorkDistributionUtils::getTotalWorkers(AC, loc, originalParallel);
    taskLoopInput.totalWorkers = insideTotalWorkers;

    DenseSet<Operation *> opsToSkip;
    SetVector<Value> externalValues;
    collectExternalValues(forBody, directRegion, externalValues, opsToSkip);

    Value origStep = forOp.getStep()[0];
    Value origLowerBound = forOp.getLowerBound()[0];
    Value origUpperBound = forOp.getUpperBound()[0];
    Value chunkLowerBoundVal =
        loopInfo.chunkLowerBound ? loopInfo.chunkLowerBound : origLowerBound;
    Region *forBodyRegion = forBody.getParent();
    std::function<void(Value)> collectWithDeps = [&](Value val) {
      if (externalValues.contains(val))
        return;
      if (Operation *defOp = val.getDefiningOp()) {
        if (forBodyRegion->isAncestor(defOp->getParentRegion()))
          return;
        if (!directRegion->isAncestor(defOp->getParentRegion())) {
          for (Value operand : defOp->getOperands())
            collectWithDeps(operand);
          externalValues.insert(val);
        }
      }
    };

    collectWithDeps(origStep);
    collectWithDeps(origLowerBound);
    collectWithDeps(origUpperBound);
    collectWithDeps(chunkLowerBoundVal);
    collectWithDeps(workerOffsetVal);
    taskLoopLowering->collectExtraExternalValues(taskLoopInput, externalValues);

    SmallVector<Value> valuesToProcess(externalValues.begin(),
                                       externalValues.end());
    for (Value val : valuesToProcess)
      if (Operation *defOp = val.getDefiningOp())
        for (Value operand : defOp->getOperands())
          collectWithDeps(operand);

    auto extraCloneableOps = [](Operation *op) {
      return isa<arts::DbRefOp, memref::LoadOp>(op);
    };
    if (!ValueAnalysis::cloneValuesIntoRegion(
            externalValues, directRegion, directMapper, AC->getBuilder(),
            /*allowMemoryEffectFree=*/true, extraCloneableOps)) {
      for (Value external : externalValues) {
        if (directMapper.contains(external))
          continue;
        if (Operation *defOp = external.getDefiningOp())
          ARTS_DEBUG("  - Uncloned external value op: " << defOp->getName());
        else
          ARTS_DEBUG("  - Uncloned external value (no defining op)");
      }
      ARTS_WARN("Some external values could not be cloned into the single-lane "
                "inline lowering");
    }

    Value stepValMapped = directMapper.lookupOrDefault(origStep);
    Value origLowerBoundVal = directMapper.lookupOrDefault(origLowerBound);
    Value origUpperBoundVal = directMapper.lookupOrDefault(origUpperBound);
    TaskLoopLoweringMappedValues mappedLoopValues;
    mappedLoopValues.step = stepValMapped;
    mappedLoopValues.lowerBound = origLowerBoundVal;
    mappedLoopValues.upperBound = origUpperBoundVal;
    mappedLoopValues.workerBaseOffset =
        directMapper.lookupOrDefault(workerOffsetVal);
    mappedLoopValues.blockSize = directMapper.lookupOrDefault(loopInfo.blockSize);
    mappedLoopValues.totalIterations =
        directMapper.lookupOrDefault(loopInfo.totalIterations);
    mappedLoopValues.totalChunks =
        directMapper.lookupOrDefault(loopInfo.totalChunks);

    TaskLoopLoweringResult loweredLoop =
        taskLoopLowering->lower(taskLoopInput, mappedLoopValues);
    loopInfo.insideBounds = loweredLoop.insideBounds;
    scf::ForOp iterLoop = loweredLoop.iterLoop;

    if (Attribute loopAttr = getLoopMetadataAttr(forOp))
      iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);

    AC->setInsertionPointToStart(iterLoop.getBody());

    Value localIter = iterLoop.getInductionVar();
    Value globalIterScaled =
        AC->create<arith::MulIOp>(loc, localIter, stepValMapped);
    Value globalIdx =
        AC->create<arith::AddIOp>(loc, loweredLoop.globalBase, globalIterScaled);

    for (Operation &op : forBody.without_terminator()) {
      for (Value operand : op.getOperands()) {
        if (auto undef = operand.getDefiningOp<LLVM::UndefOp>()) {
          Value undefVal = undef.getResult();
          Type elemType = undefVal.getType();
          Value identity = AC->createZeroValue(elemType, loc);
          if (identity)
            directMapper.map(undefVal, identity);
        }
      }
    }

    if (forBody.getNumArguments() > 0)
      directMapper.map(forBody.getArgument(0), globalIdx);

    for (Operation &op : forBody.without_terminator()) {
      if (opsToSkip.contains(&op))
        continue;
      if (isa<memref::AllocaOp>(op))
        AC->clone(op, directMapper);
    }

    for (Operation &op : forBody.without_terminator()) {
      if (opsToSkip.contains(&op))
        continue;
      if (isa<memref::AllocaOp>(op))
        continue;
      AC->clone(op, directMapper);
    }

    TaskLoopPostCloneInput postCloneInput{AC,
                                          loc,
                                          iterLoop,
                                          globalIdx,
                                          loweredLoop.innerStripeLane,
                                          loweredLoop.innerStripeCount};
    taskLoopLowering->postCloneAdjust(postCloneInput);

    cloneExternalAllocasIntoEdt(directRegion, directBlock, directMapper,
                                AC->getBuilder());

    AC->setInsertionPointAfter(iterLoop);
    for (Value dep : taskDeps)
      AC->create<DbReleaseOp>(loc, dep);

    AC->setInsertionPointAfter(ifOp);
    return EdtOp();
  }

  /// Create task EDT
  /// Inherit concurrency from parent parallel EDT - if internode, route to
  /// worker node
  EdtConcurrency taskConcurrency = originalParallel.getConcurrency();
  Value routeValue;
  if (taskConcurrency == EdtConcurrency::internode) {
    if (forceOwnerRoute) {
      routeValue = createCurrentNodeRoute(AC->getBuilder(), loc);
      ARTS_DEBUG("  - Keeping coarse write task on the current owner node");
    } else {
      /// Route to destination node from the global worker id:
      ///   nodeId = globalWorkerId / workersPerNode
      /// workersPerNode is always materialized for internode routing.
      if (!workersPerNode)
        workersPerNode = WorkDistributionUtils::getWorkersPerNode(
            AC, loc, originalParallel);
      Value nodeId =
          AC->create<arith::DivUIOp>(loc, workerIdPlaceholder, workersPerNode);
      routeValue = AC->castToInt(AC->Int32, nodeId, loc);
      ARTS_DEBUG("  - Using internode routing by workers-per-node");
    }
  } else {
    routeValue = createCurrentNodeRoute(AC->getBuilder(), loc);
  }
  auto taskEdt = AC->create<EdtOp>(loc, EdtType::task, taskConcurrency,
                                   routeValue, ValueRange{});
  transferOperationContract(forOp.getOperation(), taskEdt.getOperation());
  if (refinedTaskBlockShape)
    setStencilBlockShape(taskEdt.getOperation(), *refinedTaskBlockShape);

  Block &taskBlock = taskEdt.getBody().front();
  AC->setInsertionPointToStart(&taskBlock);

  /// Track where reduction dependencies start
  uint64_t reductionArgStart = taskDeps.size();

  /// Combine regular and reduction dependencies
  taskDeps.append(reductionTaskDeps.begin(), reductionTaskDeps.end());

  /// Add block arguments to task EDT
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    Value dep = taskDeps[i];
    if (!dep) {
      ARTS_ERROR("Null dependency at index " << i);
      continue;
    }
    BlockArgument taskArg = taskBlock.addArgument(dep.getType(), loc);
    if (i < reductionArgStart)
      mapper.map(dep, taskArg);
  }

  taskEdt.setDependencies(taskDeps);

  /// Map parallelArg → taskArg for cloning pre-for operations
  for (auto [parallelArg, acquirePtr] : parallelArgToAcquire) {
    if (Value taskArg = mapper.lookupOrNull(acquirePtr))
      mapper.map(parallelArg, taskArg);
  }

  Value insideTotalWorkers =
      WorkDistributionUtils::getTotalWorkers(AC, loc, originalParallel);
  taskLoopInput.totalWorkers = insideTotalWorkers;

  /// Map reduction variables to worker's accumulator slot
  IRMapping redMapper = mapper;
  DenseSet<Operation *> opsToSkip;

  for (auto [redVar, idx] : reductionVarIndex) {
    uint64_t argIndex = reductionArgStart + idx;
    if (argIndex >= taskBlock.getNumArguments())
      continue;

    BlockArgument partialArg = taskBlock.getArgument(argIndex);
    auto zeroIndex = AC->createIndexConstant(0, loc);
    Value myAccumulator =
        AC->create<DbRefOp>(loc, partialArg, SmallVector<Value>{zeroIndex});
    redMapper.map(redVar, myAccumulator);

    collectOldAccumulatorDbRefs(forOp, parallelBlock, reductionBlockArgs,
                                opsToSkip, redMapper, myAccumulator);
  }

  Block &forBody = forOp.getRegion().front();
  Region *taskEdtRegion = taskBlock.getParent();

  /// Collect external values needed by the loop body and induction variable
  SetVector<Value> externalValues;
  collectExternalValues(forBody, taskEdtRegion, externalValues, opsToSkip);

  Value origStep = forOp.getStep()[0];
  Value origLowerBound = forOp.getLowerBound()[0];
  Value origUpperBound = forOp.getUpperBound()[0];
  Value chunkLowerBoundVal =
      loopInfo.chunkLowerBound ? loopInfo.chunkLowerBound : origLowerBound;

  Region *forBodyRegion = forBody.getParent();
  std::function<void(Value)> collectWithDeps = [&](Value val) {
    if (externalValues.contains(val))
      return;
    if (Operation *defOp = val.getDefiningOp()) {
      if (forBodyRegion->isAncestor(defOp->getParentRegion()))
        return;
      if (!taskEdtRegion->isAncestor(defOp->getParentRegion())) {
        for (Value operand : defOp->getOperands())
          collectWithDeps(operand);
        externalValues.insert(val);
      }
    }
  };

  collectWithDeps(origStep);
  collectWithDeps(origLowerBound);
  collectWithDeps(origUpperBound);
  collectWithDeps(chunkLowerBoundVal);
  collectWithDeps(workerOffsetVal);

  taskLoopLowering->collectExtraExternalValues(taskLoopInput, externalValues);

  /// Collect transitive dependencies
  SmallVector<Value> valuesToProcess(externalValues.begin(),
                                     externalValues.end());
  for (Value val : valuesToProcess) {
    if (Operation *defOp = val.getDefiningOp()) {
      for (Value operand : defOp->getOperands())
        collectWithDeps(operand);
    }
  }

  auto extraCloneableOps = [](Operation *op) {
    return isa<arts::DbRefOp, memref::LoadOp>(op);
  };
  if (!ValueAnalysis::cloneValuesIntoRegion(
          externalValues, taskEdtRegion, redMapper, AC->getBuilder(),
          /*allowMemoryEffectFree=*/true, extraCloneableOps)) {
    for (Value external : externalValues) {
      if (redMapper.contains(external))
        continue;
      if (Operation *defOp = external.getDefiningOp())
        ARTS_DEBUG("  - Uncloned external value op: " << defOp->getName());
      else
        ARTS_DEBUG("  - Uncloned external value (no defining op)");
    }
    ARTS_WARN("Some external values could not be cloned - they may need to be "
              "passed as EDT dependencies");
  }

  Value stepValMapped = redMapper.lookupOrDefault(origStep);
  Value origLowerBoundVal = redMapper.lookupOrDefault(origLowerBound);
  Value origUpperBoundVal = redMapper.lookupOrDefault(origUpperBound);
  TaskLoopLoweringMappedValues mappedLoopValues;
  mappedLoopValues.step = stepValMapped;
  mappedLoopValues.lowerBound = origLowerBoundVal;
  mappedLoopValues.upperBound = origUpperBoundVal;
  mappedLoopValues.workerBaseOffset =
      redMapper.lookupOrDefault(workerOffsetVal);
  mappedLoopValues.blockSize = redMapper.lookupOrDefault(loopInfo.blockSize);
  mappedLoopValues.totalIterations =
      redMapper.lookupOrDefault(loopInfo.totalIterations);
  mappedLoopValues.totalChunks =
      redMapper.lookupOrDefault(loopInfo.totalChunks);

  TaskLoopLoweringResult loweredLoop =
      taskLoopLowering->lower(taskLoopInput, mappedLoopValues);
  loopInfo.insideBounds = loweredLoop.insideBounds;
  chunkOffset = loweredLoop.iterStart;
  scf::ForOp iterLoop = loweredLoop.iterLoop;

  /// Set loop metadata, marking as parallel if only reduction deps existed
  if (Attribute loopAttr = getLoopMetadataAttr(forOp)) {
    if (!reductionVarIndex.empty()) {
      if (auto origMeta = dyn_cast<LoopMetadataAttr>(loopAttr)) {
        int64_t memrefDeps = 0;
        if (auto attr = origMeta.getMemrefsWithLoopCarriedDeps())
          memrefDeps = attr.getInt();

        if (memrefDeps == 0) {
          auto parallelMeta = LoopMetadata::createParallelizedMetadata(
              forOp.getContext(), origMeta);
          iterLoop->setAttr(AttrNames::LoopMetadata::Name, parallelMeta);
          ARTS_DEBUG("  Updated loop metadata: potentiallyParallel=true "
                     "(reduction-only deps, no stencil patterns)");
        } else {
          iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
          ARTS_DEBUG("  Keeping original metadata: memrefsWithLoopCarriedDeps="
                     << memrefDeps << " (stencil patterns detected)");
        }
      } else {
        iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
      }
    } else {
      iterLoop->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
    }
  }

  AC->setInsertionPointToStart(iterLoop.getBody());

  /// Map induction variable
  Value localIter = iterLoop.getInductionVar();
  Value globalIterScaled =
      AC->create<arith::MulIOp>(loc, localIter, stepValMapped);
  Value globalIdx =
      AC->create<arith::AddIOp>(loc, loweredLoop.globalBase, globalIterScaled);

  /// Map undef to identity value
  for (Operation &op : forBody.without_terminator()) {
    for (Value operand : op.getOperands()) {
      if (auto undef = operand.getDefiningOp<LLVM::UndefOp>()) {
        Value undefVal = undef.getResult();
        Type elemType = undefVal.getType();
        Value identity = AC->createZeroValue(elemType, loc);
        if (identity)
          redMapper.map(undefVal, identity);
      }
    }
  }

  if (forBody.getNumArguments() > 0)
    redMapper.map(forBody.getArgument(0), globalIdx);

  /// No per-iteration bounds check needed: loop bounds already clamped.

  /// Clone stack allocations first so subsequent ops can map to them.
  for (Operation &op : forBody.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;
    if (isa<memref::AllocaOp>(op))
      AC->clone(op, redMapper);
  }

  /// Clone remaining loop body operations.
  for (Operation &op : forBody.without_terminator()) {
    if (opsToSkip.contains(&op))
      continue;
    if (isa<memref::AllocaOp>(op))
      continue;
    AC->clone(op, redMapper);
  }

  TaskLoopPostCloneInput postCloneInput{AC,
                                        loc,
                                        iterLoop,
                                        globalIdx,
                                        loweredLoop.innerStripeLane,
                                        loweredLoop.innerStripeCount};
  taskLoopLowering->postCloneAdjust(postCloneInput);

  /// Ensure stack allocations used inside the EDT are cloned locally.
  cloneExternalAllocasIntoEdt(taskEdtRegion, taskBlock, redMapper,
                              AC->getBuilder());

  /// Add yield terminator
  AC->setInsertionPointToEnd(&taskBlock);
  if (taskBlock.empty() || !taskBlock.back().hasTrait<OpTrait::IsTerminator>())
    AC->create<YieldOp>(loc);

  /// Release all DB dependencies before terminator
  AC->setInsertionPoint(taskBlock.getTerminator());
  for (uint64_t i = 0; i < taskDeps.size(); i++) {
    BlockArgument dbPtrArg = taskBlock.getArgument(i);
    AC->create<DbReleaseOp>(loc, dbPtrArg);
  }

  AC->setInsertionPointAfter(ifOp);

  return taskEdt;
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createForLoweringPass() {
  return std::make_unique<ForLoweringPass>();
}

std::unique_ptr<Pass> createForLoweringPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<ForLoweringPass>(AM);
}
} // namespace arts
} // namespace mlir
