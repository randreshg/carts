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
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisDependencies.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#define GEN_PASS_DEF_CONVERTOPENMPTOARTS
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/Utils.h"
#include "mlir/Pass/Pass.h"
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

using mlir::arts::AnalysisDependencyInfo;
using mlir::arts::AnalysisKind;

static const AnalysisKind kConvertOpenMPToArts_reads[] = {
    AnalysisKind::MetadataManager};
[[maybe_unused]] static const AnalysisDependencyInfo kConvertOpenMPToArts_deps =
    {kConvertOpenMPToArts_reads, {}};

#include "llvm/ADT/Statistic.h"
static llvm::Statistic numParallelRegionsConverted{
    "convert_openmp_to_arts", "NumParallelRegionsConverted",
    "Number of omp.parallel regions converted to arts.edt"};
static llvm::Statistic numTaskRegionsConverted{
    "convert_openmp_to_arts", "NumTaskRegionsConverted",
    "Number of omp.task regions converted to arts.edt"};
static llvm::Statistic numWsloopsConverted{
    "convert_openmp_to_arts", "NumWsloopsConverted",
    "Number of omp.wsloop operations converted to arts.for"};
static llvm::Statistic numScfParallelsConverted{
    "convert_openmp_to_arts", "NumScfParallelsConverted",
    "Number of scf.parallel operations converted to arts.edt + arts.for"};
static llvm::Statistic numAtomicUpdatesConverted{
    "convert_openmp_to_arts", "NumAtomicUpdatesConverted",
    "Number of omp.atomic.update operations converted to arts.atomic_add"};

using namespace mlir;
using namespace arts;

static void carryRewriteMetadata(Operation *sourceOp, Operation *targetOp,
                                 MetadataManager &metadataManager) {
  if (!sourceOp || !targetOp)
    return;
  metadataManager.rewriteMetadata(sourceOp, targetOp);
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
    auto parOp = EdtOp::create(rewriter, loc, EdtType::parallel,
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
    ++numParallelRegionsConverted;
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `scf.parallel` with an `arts.edt` with `parallel`
/// attribute
struct SCFParallelToArtsPattern : public OpRewritePattern<scf::ParallelOp> {
  SCFParallelToArtsPattern(MLIRContext *context,
                           MetadataManager &metadataManager)
      : OpRewritePattern<scf::ParallelOp>(context),
        metadataManager(metadataManager) {}

  LogicalResult matchAndRewrite(scf::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting scf.parallel to arts.edt<parallel> + arts.for");
    rewriter.setInsertionPoint(op);

    /// Create `arts.edt` with `parallel` type and internode concurrency
    auto parEdt = EdtOp::create(rewriter, loc, EdtType::parallel,
                                EdtConcurrency::internode);
    parEdt.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));
    Block &parBlk = parEdt.getBody().front();

    /// Insert `arts.for` inside the parallel EDT with same bounds/step
    rewriter.setInsertionPointToStart(&parBlk);
    Value lb = op.getLowerBound().front();
    Value ub = op.getUpperBound().front();
    Value st = op.getStep().front();

    auto artsFor = arts::ForOp::create(
        rewriter, loc, ValueRange{lb}, ValueRange{ub}, ValueRange{st},
        /*schedule*/ nullptr,
        /*reductionAccumulators*/ ValueRange{});

    carryRewriteMetadata(op, artsFor, metadataManager);

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
    arts::YieldOp::create(rewriter, loc);

    /// Terminate parallel EDT body
    rewriter.setInsertionPointToEnd(&parBlk);
    arts::YieldOp::create(rewriter, loc);

    if (hasWorkAfterInParentBlock(op.getOperation())) {
      rewriter.setInsertionPointAfter(parEdt);
      arts::BarrierOp::create(rewriter, loc);
    }

    /// Remove original scf.parallel
    ++numScfParallelsConverted;
    rewriter.eraseOp(op);
    return success();
  }

private:
  MetadataManager &metadataManager;
};

/// Pattern to replace `omp.master` with `arts.edt` with `single` attribute
struct MasterToARTSPattern : public OpRewritePattern<omp::MasterOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::MasterOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();

    /// Create a new `arts.single` operation with intranode concurrency.
    auto artsSingle =
        EdtOp::create(rewriter, loc, EdtType::single, EdtConcurrency::intranode);
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
        EdtOp::create(rewriter, loc, EdtType::single, EdtConcurrency::intranode);
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
    return arts::convertOmpMode(taskClause);
  }

  LogicalResult collectTaskDependencies(SmallVector<Value> &deps,
                                        omp::TaskOp task,
                                        PatternRewriter &rewriter,
                                        Location loc) const {

    /// Collect the task deps clause
    auto dependList = task.getDependKindsAttr();
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
        ARTS_ERROR(
            "Expected arts.omp_dep for dependency "
            << i << ", but got " << *depVar.getDefiningOp()
            << ". Make sure RaiseMemRefDimensionality runs before this pass.");
        return failure();
      }

      /// Clone arts.omp_dep inside the task region for local use
      auto &region = task.getRegion();
      {
        OpBuilder::InsertionGuard IG(rewriter);
        rewriter.setInsertionPointToStart(&region.front());

        auto newOmpDep = OmpDepOp::create(
            rewriter, loc, depMode, ompDepOp.getSource(),
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
            bool isPinned = ValueAnalysis::isOneConstant(allSizes[i]);

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

        Value dbControl = DbControlOp::create(
            rewriter, ompDepOp.getLoc(), depMode, ompDepOp.getSource(), pinnedIndices,
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
    auto edtOp = EdtOp::create(rewriter, loc, EdtType::task,
                               EdtConcurrency::intranode, deps);
    edtOp.setNoVerifyAttr(NoVerifyAttr::get(rewriter.getContext()));
    Block &blk = edtOp.getBody().front();

    /// Move the region's operations.
    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    /// Remove the original `omp.task`.
    ++numTaskRegionsConverted;
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.wsloop` with `arts.for` loop
struct WsloopToARTSPattern : public OpRewritePattern<omp::WsloopOp> {
  WsloopToARTSPattern(MLIRContext *context, MetadataManager &metadataManager)
      : OpRewritePattern<omp::WsloopOp>(context),
        metadataManager(metadataManager) {}

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

  LogicalResult matchAndRewrite(omp::WsloopOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.wsloop");

    /// Get the loop bounds and step via the nested LoopNestOp
    auto loopNest = cast<omp::LoopNestOp>(op.getWrappedLoop());
    auto lowerBound = loopNest.getLoopLowerBounds()[0];
    auto upperBound = loopNest.getLoopUpperBounds()[0];
    auto step = loopNest.getLoopSteps()[0];

    /// Nested omp.wsloop inside serial loop nests cannot become nested
    /// arts.for directly because ForLowering only rewrites top-level arts.for
    /// in a parallel EDT body. Lower to scf.parallel instead so the dedicated
    /// SCFParallelToArts pattern can materialize a nested parallel EDT.
    bool nestedInSerialLoop = hasSerialLoopAncestorInParallelRegion(op);
    bool hasReductions =
        op.getReductionSyms() && !op.getReductionSyms()->empty();
    if (nestedInSerialLoop && !hasReductions) {
      ARTS_INFO("  - Nested wsloop fallback: lowering to scf.parallel");
      auto scfParallel = scf::ParallelOp::create(
          rewriter, loc, ValueRange{lowerBound}, ValueRange{upperBound},
          ValueRange{step});
      carryRewriteMetadata(op, scfParallel, metadataManager);

      OpBuilder::InsertionGuard IG(rewriter);
      Block &parallelBody = scfParallel.getRegion().front();
      rewriter.setInsertionPointToStart(&parallelBody);
      IRMapping mapper;
      Block &src = loopNest.getRegion().front();
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
    if (auto sched = op.getScheduleKind()) {
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
    if (auto chunk = op.getScheduleChunk()) {
      int64_t chunkVal;
      if (ValueAnalysis::getConstantIndex(chunk, chunkVal) && chunkVal > 0)
        staticBlockSize = chunkVal;
    }

    /// Collect reduction metadata if present on omp.wsloop
    SmallVector<Value> redAccs;
    DenseMap<Value, omp::DeclareReductionOp> reductionDecls;
    collectReductionMetadata(op, rewriter, redAccs, reductionDecls);

    /// Create `arts.for` and move `omp.wsloop` body
    auto forOp = arts::ForOp::create(
        rewriter, loc, ValueRange{lowerBound}, ValueRange{upperBound}, ValueRange{step},
        schedAttr, ValueRange{redAccs});

    carryRewriteMetadata(op, forOp, metadataManager);

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
    Block &src = loopNest.getRegion().front();
    if (!src.getArguments().empty())
      mapper.map(src.getArgument(0), dst.getArgument(0));
    moveOps(src, dst, rewriter, mapper, reductionDecls);
    arts::YieldOp::create(rewriter, loc);

    /// OpenMP wsloop has an implicit barrier unless nowait is present.
    /// Emit an explicit ARTS barrier only when there is following work in
    /// the same region that depends on that synchronization.
    if (!op.getNowait() && hasWorkAfterInParentBlock(op.getOperation())) {
      rewriter.setInsertionPointAfter(forOp);
      arts::BarrierOp::create(rewriter, loc);
    }

    /// Remove the original wsloop
    ++numWsloopsConverted;
    rewriter.eraseOp(op);
    return success();
  }

private:
  /// Collect reduction metadata from the OpenMP wsloop operation
  void collectReductionMetadata(
      omp::WsloopOp op, PatternRewriter &rewriter, SmallVector<Value> &redAccs,
      DenseMap<Value, omp::DeclareReductionOp> &reductionDecls) const {
    auto reds = op.getReductionSyms();
    if (!reds)
      return;

    /// Get the reduction variables
    auto reductionVars = op.getReductionVars();
    ModuleOp module = op->getParentOfType<ModuleOp>();
    assert(module && "Module is required");

    for (auto [attr, value] : llvm::zip(*reds, reductionVars)) {
      redAccs.push_back(ValueAnalysis::getUnderlyingValue(value));
      if (auto symRef = dyn_cast<SymbolRefAttr>(attr)) {
        auto decl = dyn_cast_or_null<omp::DeclareReductionOp>(
            module.lookupSymbol(symRef.getLeafReference()));
        assert(decl && "Failed to resolve reduction declaration");
        reductionDecls.try_emplace(value, decl);
      }
    }
  }

  /// Move non-terminator operations from src block to dst block.
  /// In LLVM 23 the old omp::ReductionOp was removed; reductions are now
  /// represented via block arguments on the enclosing wsloop/parallel op,
  /// so no special inlining is required here.
  void moveOps(Block &src, Block &dst, PatternRewriter &rewriter,
               IRMapping &mapper,
               DenseMap<Value, omp::DeclareReductionOp> &reductionDecls) const {
    for (Operation &srcOp : src.without_terminator()) {
      rewriter.clone(srcOp, mapper);
    }

    /// Analyzes the reductionDecls and removes the ones that are not used
    for (auto [value, decl] : reductionDecls) {
      if (decl.use_empty())
        rewriter.eraseOp(decl);
    }
  }

  /// Clone combiner region for reduction operation
  Value cloneCombinerRegion(omp::DeclareReductionOp decl, Value lhs, Value rhs,
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

  MetadataManager &metadataManager;
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

    ++numAtomicUpdatesConverted;
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
    arts::YieldOp::create(rewriter, loc);
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
    arts::BarrierOp::create(rewriter, loc);
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
    arts::BarrierOp::create(rewriter, loc);
    rewriter.eraseOp(op);
    return success();
  }
};

/// Pattern to replace `omp.taskloop` with `arts.for` and EDT creation
struct TaskloopToARTSPattern : public OpRewritePattern<omp::TaskloopOp> {
  TaskloopToARTSPattern(MLIRContext *context, MetadataManager &metadataManager)
      : OpRewritePattern<omp::TaskloopOp>(context),
        metadataManager(metadataManager) {}

  LogicalResult matchAndRewrite(omp::TaskloopOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    ARTS_INFO("Converting omp.taskloop to arts.for with EDTs");

    /// Get the loop bounds and step via the nested LoopNestOp
    auto loopNest = cast<omp::LoopNestOp>(op.getWrappedLoop());
    auto lowerBound = loopNest.getLoopLowerBounds()[0];
    auto upperBound = loopNest.getLoopUpperBounds()[0];
    auto step = loopNest.getLoopSteps()[0];

    /// Create arts.for and move taskloop body
    auto forOp = arts::ForOp::create(
        rewriter, loc, ValueRange{lowerBound}, ValueRange{upperBound}, ValueRange{step},
        /*schedule*/ nullptr,
        /*reductionAccumulators*/ ValueRange{});

    carryRewriteMetadata(op, forOp, metadataManager);

    Region &dstRegion = forOp.getRegion();
    if (dstRegion.empty())
      dstRegion.push_back(new Block());
    Block &dst = dstRegion.front();
    if (dst.getNumArguments() == 0)
      dst.addArgument(rewriter.getIndexType(), loc);

    OpBuilder::InsertionGuard IG(rewriter);
    rewriter.setInsertionPointToStart(&dst);
    IRMapping mapper;
    Block &src = loopNest.getRegion().front();
    if (!src.getArguments().empty())
      mapper.map(src.getArgument(0), dst.getArgument(0));
    for (Operation &srcOp : src.without_terminator())
      rewriter.clone(srcOp, mapper);
    arts::YieldOp::create(rewriter, loc);

    /// Remove the original taskloop
    rewriter.eraseOp(op);
    return success();
  }

private:
  MetadataManager &metadataManager;
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
    : public impl::ConvertOpenMPToArtsBase<ConvertOpenMPToArtsPass> {
  explicit ConvertOpenMPToArtsPass(mlir::arts::AnalysisManager *AM = nullptr)
      : AM(AM) {}
  void runOnOperation() override;

private:
  mlir::arts::AnalysisManager *AM = nullptr;
};
} // namespace

/// Pass to convert OpenMP operations to Arts operations
void ConvertOpenMPToArtsPass::runOnOperation() {
  ModuleOp module = getOperation();
  ARTS_INFO_HEADER(ConvertOpenMPToArtsPass);
  ARTS_DEBUG_REGION(module.dump(););
  MLIRContext *context = &getContext();
  if (!AM) {
    module.emitError()
        << "ConvertOpenMPToArtsPass requires AnalysisManager/MetadataManager";
    signalPassFailure();
    return;
  }
  MetadataManager &metadataManager = AM->getMetadataManager();

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
  patterns.add<OMPParallelToArtsPattern>(context);
  patterns.add<SCFParallelToArtsPattern>(context, metadataManager);
  patterns.add<MasterToARTSPattern>(context);
  patterns.add<SingleToARTSPattern>(context);
  patterns.add<TaskloopToARTSPattern>(context, metadataManager);
  patterns.add<WsloopToARTSPattern>(context, metadataManager);
  patterns.add<AtomicUpdateToArtsPattern>(context);
  patterns.add<TerminatorToARTSPattern>(context);
  patterns.add<BarrierToARTSPattern>(context);
  patterns.add<TaskwaitToARTSPattern>(context);
  patterns.add<CallToARTSPattern>(context);
  patterns.add<TaskToARTSPattern>(context, &writerDepSources);
  GreedyRewriteConfig config;
  (void)applyPatternsGreedily(module, std::move(patterns), config);

  RemovalUtils::removeUndefOps(module);
  ARTS_INFO_FOOTER(ConvertOpenMPToArtsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createConvertOpenMPToArtsPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<ConvertOpenMPToArtsPass>(AM);
}
} // namespace arts
} // namespace mlir
