///==========================================================================///
/// File: ConvertOpenMPToSde.cpp
///
/// This file implements a module pass that converts OpenMP ops into SDE ops,
/// preserving information that the current OMP-to-ARTS conversion loses:
/// - Reduction combiner kind + identity
/// - Nowait semantics
/// - Schedule + chunk size
/// - Task completion tokens
///
/// Example:
///   Before:
///     omp.parallel {
///       omp.wsloop reduction(+: sum) schedule(static, 4) {
///         omp.loop_nest (%i) : index = (%c0) to (%N) step (%c1) { ... }
///       }
///     }
///
///   After:
///     arts_sde.cu_region parallel scope(<distributed>) {
///       arts_sde.su_iterate (%c0) to (%N) step (%c1)
///           schedule(<static>, %c4)
///           reduction [#arts_sde<reduction_kind<add>>] (%sum : f64) {
///         ...
///         arts_sde.yield
///       }
///       arts_sde.yield
///     }
///==========================================================================///

#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"

#define GEN_PASS_DEF_CONVERTOPENMPTOSDE
#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/dialect/core/Analysis/metadata/MetadataManager.h"
#include "arts/dialect/sde/Transforms/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
#include "arts/utils/ValueAnalysis.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(convert_openmp_to_sde);

#include "llvm/ADT/Statistic.h"
static llvm::Statistic numParallelConverted{
    "convert_openmp_to_sde", "NumParallelConverted",
    "Number of omp.parallel regions converted to sde.cu_region"};
static llvm::Statistic numWsloopsConverted{
    "convert_openmp_to_sde", "NumWsloopsConverted",
    "Number of omp.wsloop converted to sde.su_iterate"};
static llvm::Statistic numTasksConverted{
    "convert_openmp_to_sde", "NumTasksConverted",
    "Number of omp.task converted to sde.cu_task"};
static llvm::Statistic numAtomicsConverted{
    "convert_openmp_to_sde", "NumAtomicsConverted",
    "Number of omp.atomic.update converted to sde.cu_atomic"};

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

/// Map OMP schedule kind to SDE schedule kind.
static std::optional<sde::SdeScheduleKind>
convertScheduleKind(omp::ClauseScheduleKind kind) {
  switch (kind) {
  case omp::ClauseScheduleKind::Static:
    return sde::SdeScheduleKind::static_;
  case omp::ClauseScheduleKind::Dynamic:
    return sde::SdeScheduleKind::dynamic;
  case omp::ClauseScheduleKind::Guided:
    return sde::SdeScheduleKind::guided;
  case omp::ClauseScheduleKind::Auto:
    return sde::SdeScheduleKind::auto_;
  case omp::ClauseScheduleKind::Runtime:
    return sde::SdeScheduleKind::runtime;
  default:
    return std::nullopt;
  }
}

/// Infer SDE reduction kind from OMP DeclareReductionOp combiner body.
static sde::SdeReductionKind inferReductionKind(omp::DeclareReductionOp decl) {
  Block &combinerBlock = decl.getReductionRegion().front();
  for (Operation &op : combinerBlock.without_terminator()) {
    if (isa<arith::AddFOp, arith::AddIOp>(op))
      return sde::SdeReductionKind::add;
    if (isa<arith::MulFOp, arith::MulIOp>(op))
      return sde::SdeReductionKind::mul;
    if (isa<arith::MinimumFOp, arith::MinSIOp, arith::MinUIOp>(op))
      return sde::SdeReductionKind::min;
    if (isa<arith::MaximumFOp, arith::MaxSIOp, arith::MaxUIOp>(op))
      return sde::SdeReductionKind::max;
  }
  return sde::SdeReductionKind::custom;
}

/// Helper to create a UnitAttr when nowait is true, nullptr otherwise.
static UnitAttr nowaitAttr(MLIRContext *ctx, bool nowait) {
  return nowait ? UnitAttr::get(ctx) : nullptr;
}

/// Ensure an op's region has at least one block (SDE ops with SizedRegion
/// don't auto-create blocks in their builder, only in the parser).
static Block &ensureBlock(Region &region) {
  if (region.empty())
    region.emplaceBlock();
  return region.front();
}

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//

/// omp.parallel -> sde.cu_region parallel
struct OMPParallelToSdePattern : public OpRewritePattern<omp::ParallelOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.parallel to sde.cu_region parallel");
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();

    auto cuRegion = sde::SdeCuRegionOp::create(
        rewriter, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel),
        sde::SdeConcurrencyScopeAttr::get(
            ctx, sde::SdeConcurrencyScope::distributed),
        /*nowait=*/nullptr);

    Block &old = op.getRegion().front();
    Block &blk = ensureBlock(cuRegion.getBody());
    blk.getOperations().splice(blk.end(), old.getOperations());

    ++numParallelConverted;
    rewriter.eraseOp(op);
    return success();
  }
};

/// omp.master -> sde.cu_region single
struct MasterToSdePattern : public OpRewritePattern<omp::MasterOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::MasterOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();
    auto cuRegion = sde::SdeCuRegionOp::create(
        rewriter, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::single),
        sde::SdeConcurrencyScopeAttr::get(ctx, sde::SdeConcurrencyScope::local),
        /*nowait=*/nullptr);
    Block &old = op.getRegion().front();
    Block &blk = ensureBlock(cuRegion.getBody());
    blk.getOperations().splice(blk.end(), old.getOperations());
    rewriter.eraseOp(op);
    return success();
  }
};

struct SingleToSdePattern : public OpRewritePattern<omp::SingleOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::SingleOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();
    auto cuRegion = sde::SdeCuRegionOp::create(
        rewriter, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::single),
        sde::SdeConcurrencyScopeAttr::get(ctx, sde::SdeConcurrencyScope::local),
        nowaitAttr(ctx, op.getNowait()));
    Block &old = op.getRegion().front();
    Block &blk = ensureBlock(cuRegion.getBody());
    blk.getOperations().splice(blk.end(), old.getOperations());
    rewriter.eraseOp(op);
    return success();
  }
};

/// omp.wsloop + omp.loop_nest -> sde.su_iterate
struct WsloopToSdePattern : public OpRewritePattern<omp::WsloopOp> {
  WsloopToSdePattern(MLIRContext *context, MetadataManager &metadataManager)
      : OpRewritePattern(context), metadataManager(metadataManager) {}

  LogicalResult matchAndRewrite(omp::WsloopOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.wsloop to sde.su_iterate");
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();
    auto loopNest = cast<omp::LoopNestOp>(op.getWrappedLoop());
    auto lb = loopNest.getLoopLowerBounds()[0];
    auto ub = loopNest.getLoopUpperBounds()[0];
    auto step = loopNest.getLoopSteps()[0];

    // Schedule
    sde::SdeScheduleKindAttr schedAttr;
    if (auto sched = op.getScheduleKind()) {
      if (auto kind = convertScheduleKind(*sched))
        schedAttr = sde::SdeScheduleKindAttr::get(ctx, *kind);
    }

    // Chunk size
    Value chunkSize;
    if (auto chunk = op.getScheduleChunk())
      chunkSize = chunk;

    // Nowait
    bool nw = op.getNowait();

    // Reduction metadata
    SmallVector<Value> redAccs;
    SmallVector<Attribute> reductionKinds;
    if (auto reds = op.getReductionSyms()) {
      auto reductionVars = op.getReductionVars();
      ModuleOp module = op->getParentOfType<ModuleOp>();
      for (auto [attr, value] : llvm::zip(*reds, reductionVars)) {
        redAccs.push_back(ValueAnalysis::getUnderlyingValue(value));
        if (auto symRef = dyn_cast<SymbolRefAttr>(attr)) {
          auto decl = dyn_cast_or_null<omp::DeclareReductionOp>(
              module.lookupSymbol(symRef.getLeafReference()));
          if (decl) {
            auto kind = inferReductionKind(decl);
            reductionKinds.push_back(sde::SdeReductionKindAttr::get(ctx, kind));
          }
        }
      }
    }

    auto suIter = sde::SdeSuIterateOp::create(
        rewriter, loc, ValueRange{lb}, ValueRange{ub}, ValueRange{step},
        schedAttr, chunkSize, nowaitAttr(ctx, nw), ValueRange{redAccs},
        reductionKinds.empty() ? nullptr
                               : rewriter.getArrayAttr(reductionKinds));

    metadataManager.rewriteMetadata(op, suIter);

    // Create body
    Region &dstRegion = suIter.getBody();
    if (dstRegion.empty())
      dstRegion.push_back(new Block());
    Block &dst = dstRegion.front();
    if (dst.getNumArguments() == 0)
      dst.addArgument(rewriter.getIndexType(), loc);

    // Clone loop body
    OpBuilder::InsertionGuard IG(rewriter);
    rewriter.setInsertionPointToStart(&dst);
    IRMapping mapper;
    Block &src = loopNest.getRegion().front();
    if (!src.getArguments().empty())
      mapper.map(src.getArgument(0), dst.getArgument(0));
    for (Operation &srcOp : src.without_terminator())
      rewriter.clone(srcOp, mapper);
    sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

    // Barrier if not nowait and work follows
    if (!nw && hasWorkAfterInParentBlock(op.getOperation())) {
      rewriter.setInsertionPointAfter(suIter);
      sde::SdeSuBarrierOp::create(rewriter, loc, ValueRange{});
    }

    ++numWsloopsConverted;
    rewriter.eraseOp(op);
    return success();
  }

private:
  MetadataManager &metadataManager;
};

/// omp.task -> sde.cu_task
struct TaskToSdePattern : public OpRewritePattern<omp::TaskOp> {
  TaskToSdePattern(MLIRContext *ctx, const llvm::DenseSet<Value> *writerSources)
      : OpRewritePattern(ctx), writerSources(writerSources) {}

  LogicalResult matchAndRewrite(omp::TaskOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.task to sde.cu_task");
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();

    // Collect task dependencies as sde.mu_dep ops
    SmallVector<Value> deps;
    auto dependList = op.getDependKindsAttr();
    if (dependList) {
      for (unsigned i = 0, e = dependList.size(); i < e; ++i) {
        auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
        if (!depClause)
          return failure();

        Value depVar = op.getDependVars()[i];
        auto ompDepOp = depVar.getDefiningOp<OmpDepOp>();
        if (!ompDepOp)
          return failure();

        // Map OMP dep mode to SDE access mode
        sde::SdeAccessMode sdeMode;
        switch (ompDepOp.getMode()) {
        case ArtsMode::in:
          sdeMode = sde::SdeAccessMode::read;
          break;
        case ArtsMode::out:
          sdeMode = sde::SdeAccessMode::write;
          break;
        default:
          sdeMode = sde::SdeAccessMode::readwrite;
        }

        // Skip read-only deps with no writer
        bool hasWriter =
            !writerSources || writerSources->contains(ompDepOp.getSource());
        if (sdeMode == sde::SdeAccessMode::read && !hasWriter)
          continue;

        // Create sde.mu_dep
        rewriter.setInsertionPoint(op);
        SmallVector<Value> offsets(ompDepOp.getIndices().begin(),
                                   ompDepOp.getIndices().end());
        SmallVector<Value> sizes(ompDepOp.getSizes().begin(),
                                 ompDepOp.getSizes().end());
        auto muDep =
            sde::SdeMuDepOp::create(rewriter, loc, rewriter.getI64Type(),
                                    sde::SdeAccessModeAttr::get(ctx, sdeMode),
                                    ompDepOp.getSource(), offsets, sizes);
        deps.push_back(muDep.getDep());
      }
    }

    auto cuTask = sde::SdeCuTaskOp::create(rewriter, loc, deps);
    Block &blk = ensureBlock(cuTask.getBody());

    Block &old = op.getRegion().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    ++numTasksConverted;
    rewriter.eraseOp(op);
    return success();
  }

private:
  const llvm::DenseSet<Value> *writerSources;
};

/// omp.taskloop -> sde.su_iterate
struct TaskloopToSdePattern : public OpRewritePattern<omp::TaskloopOp> {
  TaskloopToSdePattern(MLIRContext *context, MetadataManager &metadataManager)
      : OpRewritePattern(context), metadataManager(metadataManager) {}

  LogicalResult matchAndRewrite(omp::TaskloopOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.taskloop to sde.su_iterate");
    auto loc = op.getLoc();
    auto loopNest = cast<omp::LoopNestOp>(op.getWrappedLoop());
    auto lb = loopNest.getLoopLowerBounds()[0];
    auto ub = loopNest.getLoopUpperBounds()[0];
    auto step = loopNest.getLoopSteps()[0];

    auto suIter = sde::SdeSuIterateOp::create(
        rewriter, loc, ValueRange{lb}, ValueRange{ub}, ValueRange{step},
        /*schedule=*/nullptr, /*chunkSize=*/Value(),
        /*nowait=*/nullptr,
        /*reductionAccumulators=*/ValueRange{},
        /*reductionKinds=*/nullptr);

    metadataManager.rewriteMetadata(op, suIter);

    Region &dstRegion = suIter.getBody();
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
    sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

    rewriter.eraseOp(op);
    return success();
  }

private:
  MetadataManager &metadataManager;
};

/// scf.parallel -> sde.cu_region parallel + sde.su_iterate
struct SCFParallelToSdePattern : public OpRewritePattern<scf::ParallelOp> {
  SCFParallelToSdePattern(MLIRContext *context,
                          MetadataManager &metadataManager)
      : OpRewritePattern(context), metadataManager(metadataManager) {}

  LogicalResult matchAndRewrite(scf::ParallelOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting scf.parallel to sde.cu_region + sde.su_iterate");
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();
    rewriter.setInsertionPoint(op);

    auto cuRegion = sde::SdeCuRegionOp::create(
        rewriter, loc, sde::SdeCuKindAttr::get(ctx, sde::SdeCuKind::parallel),
        sde::SdeConcurrencyScopeAttr::get(
            ctx, sde::SdeConcurrencyScope::distributed),
        /*nowait=*/nullptr);
    Block &parBlk = ensureBlock(cuRegion.getBody());

    rewriter.setInsertionPointToStart(&parBlk);
    Value lb = op.getLowerBound().front();
    Value ub = op.getUpperBound().front();
    Value st = op.getStep().front();

    auto suIter = sde::SdeSuIterateOp::create(
        rewriter, loc, ValueRange{lb}, ValueRange{ub}, ValueRange{st},
        /*schedule=*/nullptr, /*chunkSize=*/Value(),
        /*nowait=*/nullptr,
        /*reductionAccumulators=*/ValueRange{},
        /*reductionKinds=*/nullptr);

    metadataManager.rewriteMetadata(op, suIter);

    Region &dstRegion = suIter.getBody();
    if (dstRegion.empty())
      dstRegion.push_back(new Block());
    Block &dst = dstRegion.front();
    if (dst.getNumArguments() == 0)
      dst.addArgument(rewriter.getIndexType(), loc);

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
    sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

    rewriter.setInsertionPointToEnd(&parBlk);
    sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

    if (hasWorkAfterInParentBlock(op.getOperation())) {
      rewriter.setInsertionPointAfter(cuRegion);
      sde::SdeSuBarrierOp::create(rewriter, loc, ValueRange{});
    }

    rewriter.eraseOp(op);
    return success();
  }

private:
  MetadataManager &metadataManager;
};

/// omp.atomic.update -> sde.cu_atomic
struct AtomicUpdateToSdePattern : public OpRewritePattern<omp::AtomicUpdateOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::AtomicUpdateOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.atomic.update to sde.cu_atomic");
    auto &region = op.getRegion();
    if (!region.hasOneBlock())
      return failure();
    Block &blk = region.front();
    if (blk.getNumArguments() != 1)
      return failure();

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

    // Detect reduction kind from combiner
    sde::SdeReductionKind kind;
    if (isa<arith::AddIOp, arith::AddFOp>(def))
      kind = sde::SdeReductionKind::add;
    else if (isa<arith::MulIOp, arith::MulFOp>(def))
      kind = sde::SdeReductionKind::mul;
    else
      return failure();

    Value addr = op.getX();
    Value blockArg = blk.getArgument(0);
    Value inc;
    if (def->getOperand(0) == blockArg)
      inc = def->getOperand(1);
    else if (def->getOperand(1) == blockArg)
      inc = def->getOperand(0);
    else
      return failure();

    ++numAtomicsConverted;
    rewriter.replaceOpWithNewOp<sde::SdeCuAtomicOp>(
        op, sde::SdeReductionKindAttr::get(rewriter.getContext(), kind), addr,
        inc);
    return success();
  }
};

/// omp.terminator -> sde.yield
struct TerminatorToSdePattern : public OpRewritePattern<omp::TerminatorOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TerminatorOp op,
                                PatternRewriter &rewriter) const override {
    sde::SdeYieldOp::create(rewriter, op.getLoc(), ValueRange{});
    rewriter.eraseOp(op);
    return success();
  }
};

/// omp.barrier -> sde.su_barrier
struct BarrierToSdePattern : public OpRewritePattern<omp::BarrierOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::BarrierOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.barrier to sde.su_barrier");
    rewriter.replaceOpWithNewOp<sde::SdeSuBarrierOp>(op, ValueRange{});
    return success();
  }
};

/// omp.taskwait -> sde.su_barrier
struct TaskwaitToSdePattern : public OpRewritePattern<omp::TaskwaitOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(omp::TaskwaitOp op,
                                PatternRewriter &rewriter) const override {
    ARTS_INFO("Converting omp.taskwait to sde.su_barrier");
    rewriter.replaceOpWithNewOp<sde::SdeSuBarrierOp>(op, ValueRange{});
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

namespace {
struct ConvertOpenMPToSdePass
    : public impl::ConvertOpenMPToSdeBase<ConvertOpenMPToSdePass> {

  explicit ConvertOpenMPToSdePass(mlir::arts::AnalysisManager *AM = nullptr)
      : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(ConvertOpenMPToSdePass);
    MLIRContext *context = &getContext();
    if (!AM) {
      module.emitError() << "ConvertOpenMPToSdePass requires AnalysisManager";
      signalPassFailure();
      return;
    }
    MetadataManager &metadataManager = AM->getMetadataManager();

    // Pre-scan writer sources for dependency filtering
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

    RewritePatternSet patterns(context);
    patterns.add<OMPParallelToSdePattern>(context);
    patterns.add<SCFParallelToSdePattern>(context, metadataManager);
    patterns.add<MasterToSdePattern>(context);
    patterns.add<SingleToSdePattern>(context);
    patterns.add<TaskloopToSdePattern>(context, metadataManager);
    patterns.add<WsloopToSdePattern>(context, metadataManager);
    patterns.add<AtomicUpdateToSdePattern>(context);
    patterns.add<TerminatorToSdePattern>(context);
    patterns.add<BarrierToSdePattern>(context);
    patterns.add<TaskwaitToSdePattern>(context);
    patterns.add<TaskToSdePattern>(context, &writerDepSources);
    GreedyRewriteConfig config;
    (void)applyPatternsGreedily(module, std::move(patterns), config);

    ARTS_INFO_FOOTER(ConvertOpenMPToSdePass);
  }

private:
  mlir::arts::AnalysisManager *AM = nullptr;
};
} // namespace

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {
namespace sde {
std::unique_ptr<Pass>
createConvertOpenMPToSdePass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<ConvertOpenMPToSdePass>(AM);
}
} // namespace sde
} // namespace arts
} // namespace mlir
