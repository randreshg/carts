///==========================================================================///
/// File: ConvertOpenMPToArts.cpp
///
/// This file implements a module pass that converts OpenMP ops
/// (omp.parallel, omp.master, omp.task, etc.) into Arts ops
///
/// Example:
///   Before:
///     omp.parallel {
///       omp.task depend(in: A[i]) depend(out: B[i]) { ... }
///     }
///
///   After:
///     arts.edt <parallel> {
///       arts.edt <task> (...) {
///         arts.db_control ...
///         ...
///       }
///     }
///==========================================================================///

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
/// Arts
#include "arts/passes/PassDetails.h"
#include "arts/analysis/HeuristicsConfig.h"
#include "arts/ArtsDialect.h"
#include "arts/passes/Passes.h"
#include "arts/utils/Utils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/ValueUtils.h"
/// Others
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "arts/utils/Debug.h"
#include <optional>
ARTS_DEBUG_SETUP(convert_openmp_to_arts);

using namespace mlir;
using namespace arts;

static bool hasWorkAfterInParentBlock(Operation *op) {
  if (!op || !op->getBlock())
    return false;

  for (Operation *next = op->getNextNode(); next; next = next->getNextNode()) {
    if (next->hasTrait<OpTrait::IsTerminator>())
      continue;
    return true;
  }
  return false;
}
///===----------------------------------------------------------------------===///
/// Conversion Patterns
///===----------------------------------------------------------------------===///
/// Pattern to replace `omp.parallel` with `arts.edt` with `parallel` attribute
struct OMPParallelToArtsPattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.parallel to arts.parallel");

    /// Create a new `arts.edt` operation with parallel type
    auto parOp = rewriter.create<EdtOp>(loc, EdtType::parallel,
                                        EdtConcurrency::internode);
    parOp.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));
    Block &blk = parOp.getBody().front();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// No barrier here: ParallelEdtLowering wraps the parallel EDT in an
    /// epoch + wait_on_epoch, which provides the implicit join semantics
    /// required by OpenMP parallel regions.

    /// Remove the original operation.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `scf.parallel` with an `arts.edt` with `parallel`
/// attribute
struct SCFParallelToArtsPattern : public OpRewritePattern<scf::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(scf::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting scf.parallel to arts.edt<parallel> + arts.for");
    rewriter.setInsertionPoint(op);

    /// Create `arts.edt` with `parallel` type and internode concurrency
    auto parEdt = rewriter.create<EdtOp>(loc, EdtType::parallel,
                                         EdtConcurrency::internode);
    parEdt.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));
    Block &parBlk = parEdt.getBody().front();

    /// Insert `arts.for` inside the parallel EDT with same bounds/step
    rewriter.setInsertionPointToStart(&parBlk);
    Value lb = op.getLowerBound().front();
    Value ub = op.getUpperBound().front();
    Value st = op.getStep().front();

    auto artsFor = rewriter.create<arts::ForOp>(
        loc, ValueRange{lb}, ValueRange{ub}, ValueRange{st},
        /*schedule*/ nullptr,
        /*reductionAccumulators*/ ValueRange{});

    /// Transfer metadata attributes (arts.loop, arts.id, etc.) from source
    copyArtsMetadataAttrs(op, artsFor);

    Region &dstRegion = artsFor.getRegion();
    if (dstRegion.empty())
      dstRegion.push_back(new Block());
    Block &dst = dstRegion.front();
    if (dst.getNumArguments() == 0)
      dst.addArgument(rewriter.getIndexType(), loc);

    /// Map IV and clone original body into arts.for body
    OpBuilder::InsertionGuard IG(rewriter);
    rewriter.setInsertionPointToStart(&dst);
    IRMapping mapper;
    Block &src = op.getRegion().front();
    if (!op.getInductionVars().empty())
      mapper.map(op.getInductionVars().front(), dst.getArgument(0));
    if (!src.getArguments().empty())
      mapper.map(src.getArgument(0), dst.getArgument(0));
    for (Operation &srcOp : src.without_terminator())
      rewriter.clone(srcOp, mapper);
    rewriter.create<arts::YieldOp>(loc);

    /// Terminate parallel EDT body
    rewriter.setInsertionPointToEnd(&parBlk);
    rewriter.create<arts::YieldOp>(loc);

    if (hasWorkAfterInParentBlock(op.getOperation())) {
      rewriter.setInsertionPointAfter(parEdt);
      rewriter.create<arts::BarrierOp>(loc);
    }

    /// Remove original scf.parallel
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.master` with `arts.edt` with `single` attribute
struct MasterToARTSPattern : public OpRewritePattern<omp::MasterOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::MasterOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();

    /// Create a new `arts.single` operation with intranode concurrency.
    auto artsSingle =
        rewriter.create<EdtOp>(loc, EdtType::single, EdtConcurrency::intranode);
    artsSingle.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    Block &blk = artsSingle.getBody().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original operation.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.single` with `arts.edt` with `single` attribute
struct SingleToARTSPattern : public OpRewritePattern<omp::SingleOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::SingleOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.single to arts.edt<single>");

    /// Create a new `arts.single` operation with intranode concurrency.
    auto artsSingle =
        rewriter.create<EdtOp>(loc, EdtType::single, EdtConcurrency::intranode);
    artsSingle.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    Block &blk = artsSingle.getBody().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original operation.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.task` with `arts.edt`
struct TaskToARTSPattern : public OpRewritePattern<omp::TaskOp> {
  TaskToARTSPattern(MLIRContext *ctx,
                    const llvm::DenseSet<Value> *writerSources)
      : OpRewritePattern<omp::TaskOp>(ctx), writerSources(writerSources) {}

  const llvm::DenseSet<Value> *writerSources;

  ArtsMode getDbMode(omp::ClauseTaskDepend taskClause) const {
    switch (taskClause) {
    case omp::ClauseTaskDepend::taskdependin:
      return ArtsMode::in;
    case omp::ClauseTaskDepend::taskdependout:
      return ArtsMode::out;
    case omp::ClauseTaskDepend::taskdependinout:
      return ArtsMode::inout;
    }
    ARTS_UNREACHABLE("Unknown ClauseTaskDepend value");
  }

  LogicalResult collectTaskDependencies(SmallVector<Value> &deps,
                                        omp::TaskOp task,
                                        PatternRewriter &rewriter,
                                        Location loc) const {

    /// Collect the task deps clause
    auto dependList = task.getDependsAttr();
    if (!dependList) {
      ARTS_DEBUG(" - No dependencies found for task");
      return success();
    }
    ARTS_DEBUG(" - Processing " << dependList.size() << " dependencies");

    for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
      /// Get dependency clause and type.
      auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
      if (!depClause) {
        ARTS_ERROR("Missing ClauseTaskDependAttr for dependency " << i);
        return failure();
      }
      Value depVar = task.getDependVars()[i];
      ArtsMode depMode = getDbMode(depClause.getValue());

      /// All dependencies should be arts.omp_dep
      auto ompDepOp = depVar.getDefiningOp<OmpDepOp>();
      if (!ompDepOp) {
        ARTS_ERROR("Expected arts.omp_dep for dependency "
                   << i << ", but got " << *depVar.getDefiningOp()
                   << ". Make sure CanonicalizeMemrefs runs before this pass.");
        return failure();
      }

      /// Clone arts.omp_dep inside the task region for local use
      auto &region = task.getRegion();
      {
        OpBuilder::InsertionGuard IG(rewriter);
        rewriter.setInsertionPointToStart(&region.front());

        auto newOmpDep = rewriter.create<OmpDepOp>(
            loc, depMode, ompDepOp.getSource(),
            SmallVector<Value>(ompDepOp.getIndices().begin(),
                               ompDepOp.getIndices().end()),
            SmallVector<Value>(ompDepOp.getSizes().begin(),
                               ompDepOp.getSizes().end()));
        replaceInRegion(region, depVar, newOmpDep.getResult());
      }

      /// OpenMP depend(in: X) where no task ever writes X creates no ordering
      /// edge. Dropping these read-only dependency edges avoids unnecessary
      /// runtime bookkeeping and reduces sensitivity to dependency noise.
      bool hasWriter =
          !writerSources || writerSources->contains(ompDepOp.getSource());
      if (depMode == ArtsMode::in && !hasWriter) {
        ARTS_DEBUG("  - Skipping read-only dependency for source "
                   << ompDepOp.getSource());
        continue;
      }

      /// Extract indices and sizes from arts.omp_dep
      /// Separate pinned dims (size=1) from chunk dims (size>1)
      {
        OpBuilder::InsertionGuard IG(rewriter);
        rewriter.setInsertionPoint(ompDepOp);

        SmallVector<Value> allIndices(ompDepOp.getIndices().begin(),
                                      ompDepOp.getIndices().end());
        SmallVector<Value> allSizes(ompDepOp.getSizes().begin(),
                                    ompDepOp.getSizes().end());

        SmallVector<Value> pinnedIndices, chunkOffsets, blockSizes;

        /// If we have indices but no explicit sizes, treat indices as offsets
        /// and use them as pinned dimensions (sizes will be analyzed later)
        if (!allIndices.empty() && allSizes.empty()) {
          /// No explicit block sizes - use indices as pinned dimensions
          pinnedIndices = allIndices;
          ARTS_DEBUG("  - No explicit sizes, using "
                     << allIndices.size() << " indices as pinned dims");
        } else {
          /// We have explicit sizes - separate pinned from chunked dimensions
          for (size_t i = 0; i < allIndices.size() && i < allSizes.size();
               ++i) {
            /// Check if this dimension has size == 1 (pinned) or > 1 (chunk)
            bool isPinned = ValueUtils::isOneConstant(allSizes[i]);

            if (isPinned) {
              pinnedIndices.push_back(allIndices[i]);
            } else {
              chunkOffsets.push_back(allIndices[i]);
              blockSizes.push_back(allSizes[i]);
            }
          }
        }

        ARTS_DEBUG("  - Creating DbControlOp from arts.omp_dep: "
                   << allIndices.size() << " indices, " << pinnedIndices.size()
                   << " pinned, " << blockSizes.size() << " chunks");

        Value dbControl = rewriter.create<DbControlOp>(
            ompDepOp.getLoc(), depMode, ompDepOp.getSource(), pinnedIndices,
            chunkOffsets, blockSizes);
        deps.push_back(dbControl);
      }
    }
    return success();
  }

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.task to arts.edt");
    /// Collect dependencies
    SmallVector<Value> deps;
    if (failed(collectTaskDependencies(deps, op, rewriter, loc))) {
      ARTS_ERROR("Failed to collect task dependencies");
      return failure();
    }

    /// Create a new `arts.edt` operation with intranode concurrency.
    auto edtOp = rewriter.create<EdtOp>(loc, EdtType::task,
                                        EdtConcurrency::intranode, deps);
    edtOp.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));
    Block &blk = edtOp.getBody().front();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original `omp.task`.
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.wsloop` with `arts.for` loop
struct WsloopToARTSPattern : public OpRewritePattern<omp::WsLoopOp> {
  using OpRewritePattern::OpRewritePattern;

  /// Returns true when a serial loop is nested inside the current parallel
  /// region. We stop at the nearest parallel boundary (omp.parallel before
  /// conversion, arts.edt after conversion) so outer host loops do not force
  /// wsloop fallback.
  static bool hasSerialLoopAncestorInParallelRegion(Operation *op) {
    for (Operation *cur = op ? op->getParentOp() : nullptr; cur;
         cur = cur->getParentOp()) {
      if (isa<omp::ParallelOp, arts::EdtOp>(cur))
        break;
      if (isa<scf::ForOp>(cur))
        return true;
    }
    return false;
  }

  LogicalResult matchAndRewrite(omp::WsLoopOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.wsloop");

    /// Get the loop bounds and step from the wsloop
    auto lowerBound = op.getLowerBound()[0];
    auto upperBound = op.getUpperBound()[0];
    auto step = op.getStep()[0];

    /// Nested omp.wsloop inside serial loop nests cannot become nested
    /// arts.for directly because ForLowering only rewrites top-level arts.for
    /// in a parallel EDT body. Lower to scf.parallel instead so the dedicated
    /// SCFParallelToArts pattern can materialize a nested parallel EDT.
    bool nestedInSerialLoop = hasSerialLoopAncestorInParallelRegion(op);
    bool hasReductions =
        op.getReductionsAttr() && !op.getReductionsAttr().getValue().empty();
    if (nestedInSerialLoop && !hasReductions) {
      ARTS_INFO("  - Nested wsloop fallback: lowering to scf.parallel");
      auto scfParallel = rewriter.create<scf::ParallelOp>(
          loc, ValueRange{lowerBound}, ValueRange{upperBound},
          ValueRange{step});
      copyArtsMetadataAttrs(op, scfParallel);

      OpBuilder::InsertionGuard IG(rewriter);
      Block &parallelBody = scfParallel.getRegion().front();
      rewriter.setInsertionPointToStart(&parallelBody);
      IRMapping mapper;
      Block &src = op.getRegion().front();
      if (!src.getArguments().empty() &&
          !scfParallel.getInductionVars().empty())
        mapper.map(src.getArgument(0), scfParallel.getInductionVars().front());
      for (Operation &srcOp : src.without_terminator())
        rewriter.clone(srcOp, mapper);

      rewriter.eraseOp(op);
      return success();
    }
    ARTS_INFO("  - Lowering wsloop to arts.for");

    /// Map OpenMP schedule to Arts ForScheduleKindAttr if present
    arts::ForScheduleKindAttr schedAttr = nullptr;
    if (auto sched = op.getScheduleVal()) {
      switch (*sched) {
      case omp::ClauseScheduleKind::Static:
        schedAttr = arts::ForScheduleKindAttr::get(
            rewriter.getContext(), arts::ForScheduleKind::Static);
        break;
      case omp::ClauseScheduleKind::Dynamic:
        schedAttr = arts::ForScheduleKindAttr::get(
            rewriter.getContext(), arts::ForScheduleKind::Dynamic);
        break;
      case omp::ClauseScheduleKind::Guided:
        schedAttr = arts::ForScheduleKindAttr::get(
            rewriter.getContext(), arts::ForScheduleKind::Guided);
        break;
      case omp::ClauseScheduleKind::Auto:
        schedAttr = arts::ForScheduleKindAttr::get(rewriter.getContext(),
                                                   arts::ForScheduleKind::Auto);
        break;
      case omp::ClauseScheduleKind::Runtime:
        schedAttr = arts::ForScheduleKindAttr::get(
            rewriter.getContext(), arts::ForScheduleKind::Runtime);
        break;
      }
    }

    /// Capture OpenMP block size if it is a known constant so later passes can
    /// implement the desired scheduling policy. The loop step should remain the
    /// original iteration step irrespective of the block size.
    std::optional<int64_t> staticBlockSize;
    if (auto chunk = op.getScheduleChunkVar()) {
      int64_t chunkVal;
      if (ValueUtils::getConstantIndex(chunk, chunkVal) && chunkVal > 0)
        staticBlockSize = chunkVal;
    }

    /// Collect reduction metadata if present on omp.wsloop
    SmallVector<Value> redAccs;
    DenseMap<Value, omp::ReductionDeclareOp> reductionDecls;
    collectReductionMetadata(op, rewriter, redAccs, reductionDecls);

    /// Create `arts.for` and move `omp.wsloop` body
    auto forOp = rewriter.create<arts::ForOp>(
        loc, ValueRange{lowerBound}, ValueRange{upperBound}, ValueRange{step},
        schedAttr, ValueRange{redAccs});

    /// Transfer metadata attributes (arts.loop, arts.id, etc.) from source
    copyArtsMetadataAttrs(op, forOp);

    /// Set partitioning hint if block size is present
    if (staticBlockSize) {
      setPartitioningHint(forOp.getOperation(),
                          PartitioningHint::block(staticBlockSize));
    }

    /// Add region and argument to the forOp
    Region &dstRegion = forOp.getRegion();
    if (dstRegion.empty())
      dstRegion.push_back(new Block());
    Block &dst = dstRegion.front();
    if (dst.getNumArguments() == 0)
      dst.addArgument(rewriter.getIndexType(), loc);

    /// Move ops and inline reductions
    OpBuilder::InsertionGuard IG(rewriter);
    rewriter.setInsertionPointToStart(&dst);
    IRMapping mapper;
    Block &src = op.getRegion().front();
    if (!src.getArguments().empty())
      mapper.map(src.getArgument(0), dst.getArgument(0));
    moveOps(src, dst, rewriter, mapper, reductionDecls);
    rewriter.create<arts::YieldOp>(loc);

    /// OpenMP wsloop has an implicit barrier unless nowait is present.
    /// Emit an explicit ARTS barrier only when there is following work in
    /// the same region that depends on that synchronization.
    if (!op.getNowaitAttr() && hasWorkAfterInParentBlock(op.getOperation())) {
      rewriter.setInsertionPointAfter(forOp);
      rewriter.create<arts::BarrierOp>(loc);
    }

    /// Remove the original wsloop
    rewriter.eraseOp(op);
    return success();
  }

private:
  /// Collect reduction metadata from the OpenMP wsloop operation
  void collectReductionMetadata(
      omp::WsLoopOp op, PatternRewriter &rewriter, SmallVector<Value> &redAccs,
      DenseMap<Value, omp::ReductionDeclareOp> &reductionDecls) const {
    auto reds = op.getReductionsAttr();
    if (!reds)
      return;

    /// Get the reduction variables
    auto reductionVars = op.getReductionVars();
    ModuleOp module = op->getParentOfType<ModuleOp>();
    assert(module && "Module is required");

    for (auto [attr, value] : llvm::zip(reds.getValue(), reductionVars)) {
      redAccs.push_back(ValueUtils::getUnderlyingValue(value));
      if (auto symRef = dyn_cast<SymbolRefAttr>(attr)) {
        auto decl = dyn_cast_or_null<omp::ReductionDeclareOp>(
            module.lookupSymbol(symRef.getLeafReference()));
        assert(decl && "Failed to resolve reduction declaration");
        reductionDecls.try_emplace(value, decl);
      }
    }
  }

  /// Inline reduction operations and move non-reduction operations
  void moveOps(Block &src, Block &dst, PatternRewriter &rewriter,
               IRMapping &mapper,
               DenseMap<Value, omp::ReductionDeclareOp> &reductionDecls) const {
    for (Operation &srcOp : src.without_terminator()) {
      if (auto redOp = dyn_cast<omp::ReductionOp>(&srcOp)) {
        if (inlineReduction(redOp, rewriter, mapper, reductionDecls))
          continue;
      }
      rewriter.clone(srcOp, mapper);
    }

    /// Analyzes the reductionDecls and removes the ones that are not used
    for (auto [value, decl] : reductionDecls) {
      if (decl.use_empty())
        rewriter.eraseOp(decl);
    }
  }

  /// Inline a reduction operation
  bool inlineReduction(
      omp::ReductionOp redOp, PatternRewriter &rewriter, IRMapping &mapper,
      const DenseMap<Value, omp::ReductionDeclareOp> &reductionDecls) const {
    auto it = reductionDecls.find(redOp.getAccumulator());
    if (it == reductionDecls.end())
      return false;

    Value operand = mapper.lookupOrDefault(redOp.getOperand());
    Value accumulator = mapper.lookupOrDefault(redOp.getAccumulator());
    auto memType = accumulator.getType().dyn_cast<MemRefType>();
    if (!memType)
      return false;

    Location loc = redOp.getLoc();
    unsigned rank = memType.getRank();
    SmallVector<Value> zeroIndices;
    zeroIndices.reserve(rank);
    for (unsigned i = 0; i < rank; ++i)
      zeroIndices.push_back(arts::createZeroIndex(rewriter, loc));

    Value current =
        rewriter.create<memref::LoadOp>(loc, accumulator, zeroIndices);

    Value combined =
        cloneCombinerRegion(it->second, current, operand, rewriter, loc);
    if (!combined)
      return false;

    rewriter.create<memref::StoreOp>(loc, combined, accumulator, zeroIndices);
    return true;
  }

  /// Clone combiner region for reduction operation
  Value cloneCombinerRegion(omp::ReductionDeclareOp decl, Value lhs, Value rhs,
                            PatternRewriter &rewriter, Location loc) const {
    Block &combinerBlock = decl.getReductionRegion().front();
    IRMapping combinerMap;
    combinerMap.map(combinerBlock.getArgument(0), lhs);
    combinerMap.map(combinerBlock.getArgument(1), rhs);

    for (Operation &combOp : combinerBlock.without_terminator())
      rewriter.clone(combOp, combinerMap);

    auto yieldOp = dyn_cast<omp::YieldOp>(combinerBlock.getTerminator());
    if (!yieldOp || yieldOp.getNumOperands() != 1)
      return Value();
    return combinerMap.lookup(yieldOp.getOperand(0));
  }
};

/// Pattern to convert a subset of omp.atomic.update to arts.atomic_add
struct AtomicUpdateToArtsPattern
    : public OpRewritePattern<omp::AtomicUpdateOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::AtomicUpdateOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.atomic.update to arts.atomic_add");
    /// Only handle canonical form: one block, single arg, computes add of arg
    /// and a captured value
    auto &region = op.getRegion();
    if (!region.hasOneBlock())
      return failure();
    Block &blk = region.front();
    if (blk.getNumArguments() != 1)
      return failure();

    /// Heuristically detect add: last non-terminator must yield result of
    /// arith.add*
    Operation *yield = blk.getTerminator();
    if (!yield)
      return failure();
    Value yielded;
    if (auto y = dyn_cast<omp::YieldOp>(yield)) {
      if (y->getNumOperands() != 1)
        return failure();
      yielded = y->getOperand(0);
    } else {
      return failure();
    }

    Operation *def = yielded.getDefiningOp();
    if (!def)
      return failure();
    bool isAdd = isa<arith::AddIOp>(def) || isa<arith::AddFOp>(def);
    if (!isAdd)
      return failure();

    /// addr must be a memref (rank-0) for now
    Value addr = op.getX();

    /// Extract the non-block-arg operand as the increment value
    Value blockArg = blk.getArgument(0);
    Value inc;
    if (def->getOperand(0) == blockArg)
      inc = def->getOperand(1);
    else if (def->getOperand(1) == blockArg)
      inc = def->getOperand(0);
    else
      return failure();

    rewriter.replaceOpWithNewOp<arts::AtomicAddOp>(op, addr, inc);
    return success();
  }
};

/// Pattern to replace `omp.terminator` with `arts.yield`
struct TerminatorToARTSPattern : public OpRewritePattern<omp::TerminatorOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TerminatorOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    rewriter.create<arts::YieldOp>(loc);
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.barrier` with `arts.barrier`
struct BarrierToARTSPattern : public OpRewritePattern<omp::BarrierOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::BarrierOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.barrier to arts.barrier");
    auto loc = op.getLoc();
    rewriter.create<arts::BarrierOp>(loc);
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.taskwait` with `arts.barrier`
struct TaskwaitToARTSPattern : public OpRewritePattern<omp::TaskwaitOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TaskwaitOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.taskwait to arts.barrier");
    auto loc = op.getLoc();
    rewriter.create<arts::BarrierOp>(loc);
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.taskloop` with `arts.for` and EDT creation
struct TaskloopToARTSPattern : public OpRewritePattern<omp::TaskLoopOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TaskLoopOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.taskloop to arts.for with EDTs");

    /// Get the loop bounds and step from the taskloop
    auto lowerBound = op.getLowerBound()[0];
    auto upperBound = op.getUpperBound()[0];
    auto step = op.getStep()[0];

    /// Create arts.for and move taskloop body
    auto forOp = rewriter.create<arts::ForOp>(
        loc, ValueRange{lowerBound}, ValueRange{upperBound}, ValueRange{step},
        /*schedule*/ nullptr,
        /*reductionAccumulators*/ ValueRange{});

    /// Transfer metadata attributes (arts.loop, arts.id, etc.) from source
    copyArtsMetadataAttrs(op, forOp);

    Region &dstRegion = forOp.getRegion();
    if (dstRegion.empty())
      dstRegion.push_back(new Block());
    Block &dst = dstRegion.front();
    if (dst.getNumArguments() == 0)
      dst.addArgument(rewriter.getIndexType(), loc);

    OpBuilder::InsertionGuard IG(rewriter);
    rewriter.setInsertionPointToStart(&dst);
    IRMapping mapper;
    Block &src = op.getRegion().front();
    if (!src.getArguments().empty())
      mapper.map(src.getArgument(0), dst.getArgument(0));
    for (Operation &srcOp : src.without_terminator())
      rewriter.clone(srcOp, mapper);
    rewriter.create<arts::YieldOp>(loc);

    /// Remove the original taskloop
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace 'func.call' with an equivalent 'arts' call if exists.
struct CallToARTSPattern : public OpRewritePattern<func::CallOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(func::CallOp callOp,
                                PatternRewriter &rewriter) const override {
    auto callee = callOp.getCallee();
    if (callee == "omp_get_thread_num") {
      /// Use Worker by default - Concurrency pass will convert to Node for
      /// internode EDTs
      rewriter.replaceOpWithNewOp<arts::RuntimeQueryOp>(
          callOp, arts::RuntimeQueryKind::currentWorker);
      return success();
    }
    if (callee == "omp_get_num_threads" || callee == "omp_get_max_threads") {
      /// Use Worker by default - Concurrency pass will convert to Node for
      /// internode EDTs
      rewriter.replaceOpWithNewOp<arts::RuntimeQueryOp>(
          callOp, arts::RuntimeQueryKind::totalWorkers);
      return success();
    }
    /// nothing to do, leave the op as-is
    return failure();
  }
};

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///
namespace {
struct ConvertOpenMPToArtsPass
    : public arts::ConvertOpenMPToArtsBase<ConvertOpenMPToArtsPass> {
  void runOnOperation() override;
};
} // namespace

/// Pass to convert OpenMP operations to Arts operations
void ConvertOpenMPToArtsPass::runOnOperation() {
  ModuleOp module = getOperation();
  ARTS_INFO_HEADER(ConvertOpenMPToArtsPass);
  ARTS_DEBUG_REGION(module.dump(););
  MLIRContext *context = &getContext();

  /// Record sources that participate in writer dependencies anywhere in this
  /// module before rewrites mutate the task graph.
  llvm::DenseSet<Value> writerDepSources;
  module.walk([&](omp::TaskOp task) {
    for (Value depVar : task.getDependVars()) {
      auto dep = depVar.getDefiningOp<OmpDepOp>();
      if (!dep)
        continue;
      if (dep.getMode() == ArtsMode::out || dep.getMode() == ArtsMode::inout)
        writerDepSources.insert(dep.getSource());
    }
  });

  /// Add patterns to convert OpenMP operations to Arts operations
  RewritePatternSet patterns(context);
  patterns.add<OMPParallelToArtsPattern, SCFParallelToArtsPattern,
               MasterToARTSPattern, SingleToARTSPattern, TaskloopToARTSPattern,
               WsloopToARTSPattern, AtomicUpdateToArtsPattern,
               TerminatorToARTSPattern, BarrierToARTSPattern,
               TaskwaitToARTSPattern, CallToARTSPattern>(context);
  patterns.add<TaskToARTSPattern>(context, &writerDepSources);
  GreedyRewriteConfig config;
  (void)applyPatternsAndFoldGreedily(module, std::move(patterns), config);

  RemovalUtils::removeUndefOps(module);
  ARTS_INFO_FOOTER(ConvertOpenMPToArtsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertOpenMPtoArtsPass() {
  return std::make_unique<ConvertOpenMPToArtsPass>();
}
} // namespace arts
} // namespace mlir
