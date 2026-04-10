///==========================================================================///
/// File: SdeToArtsPatterns.cpp
///
/// Converts SDE dialect ops into ARTS dialect ops, producing the same IR
/// that ConvertOpenMPToArts currently generates.
///
/// Mapping:
///   sde.cu_region parallel  ->  arts.edt <parallel, internode>
///   sde.cu_region single    ->  arts.edt <single, intranode>
///   sde.cu_task             ->  arts.edt <task, intranode> + arts.db_control
///   sde.su_iterate          ->  arts.for
///   sde.su_barrier          ->  arts.barrier
///   sde.cu_atomic           ->  arts.atomic_add
///   sde.mu_dep              ->  arts.db_control
///   sde.yield               ->  arts.yield
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_CONVERTSDETOARTS
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/ValueAnalysis.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(convert_sde_to_arts);

#include "llvm/ADT/Statistic.h"
static llvm::Statistic numCuRegionConverted{
    "convert_sde_to_arts", "NumCuRegionConverted",
    "Number of sde.cu_region converted to arts.edt"};
static llvm::Statistic numSuIterateConverted{
    "convert_sde_to_arts", "NumSuIterateConverted",
    "Number of sde.su_iterate converted to arts.for"};
static llvm::Statistic numCuTaskConverted{
    "convert_sde_to_arts", "NumCuTaskConverted",
    "Number of sde.cu_task converted to arts.edt"};

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

/// Map SDE schedule kind to ARTS ForScheduleKind.
static ForScheduleKindAttr
convertSchedule(MLIRContext *ctx, sde::SdeScheduleKindAttr sdeSchedule) {
  if (!sdeSchedule)
    return nullptr;
  switch (sdeSchedule.getValue()) {
  case sde::SdeScheduleKind::static_:
    return ForScheduleKindAttr::get(ctx, ForScheduleKind::Static);
  case sde::SdeScheduleKind::dynamic:
    return ForScheduleKindAttr::get(ctx, ForScheduleKind::Dynamic);
  case sde::SdeScheduleKind::guided:
    return ForScheduleKindAttr::get(ctx, ForScheduleKind::Guided);
  case sde::SdeScheduleKind::auto_:
    return ForScheduleKindAttr::get(ctx, ForScheduleKind::Auto);
  case sde::SdeScheduleKind::runtime:
    return ForScheduleKindAttr::get(ctx, ForScheduleKind::Runtime);
  }
  return nullptr;
}

/// Map SDE access mode to ARTS ArtsMode.
static ArtsMode convertAccessMode(sde::SdeAccessMode mode) {
  switch (mode) {
  case sde::SdeAccessMode::read:
    return ArtsMode::in;
  case sde::SdeAccessMode::write:
    return ArtsMode::out;
  case sde::SdeAccessMode::readwrite:
    return ArtsMode::inout;
  }
  return ArtsMode::inout;
}

//===----------------------------------------------------------------------===//
// Conversion Patterns
//===----------------------------------------------------------------------===//

/// sde.cu_region -> arts.edt
struct CuRegionToArtsPattern : public OpRewritePattern<sde::SdeCuRegionOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeCuRegionOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();

    // Map SDE kind to ARTS EdtType and EdtConcurrency
    EdtType edtType;
    EdtConcurrency concurrency;
    switch (op.getKind()) {
    case sde::SdeCuKind::parallel:
      edtType = EdtType::parallel;
      concurrency = EdtConcurrency::internode;
      break;
    case sde::SdeCuKind::single:
      edtType = EdtType::single;
      concurrency = EdtConcurrency::intranode;
      break;
    case sde::SdeCuKind::task:
      edtType = EdtType::task;
      concurrency = EdtConcurrency::intranode;
      break;
    }

    // Override concurrency if SDE scope is explicitly local
    if (auto scope = op.getConcurrencyScope()) {
      if (*scope == sde::SdeConcurrencyScope::local)
        concurrency = EdtConcurrency::intranode;
    }

    auto edtOp = EdtOp::create(rewriter, loc, edtType, concurrency);
    edtOp.setNoVerifyAttr(NoVerifyAttr::get(ctx));

    Block &old = op.getBody().front();
    Block &blk = edtOp.getBody().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    ++numCuRegionConverted;
    rewriter.eraseOp(op);
    return success();
  }
};

/// sde.cu_task -> arts.edt <task> with db_control deps
struct CuTaskToArtsPattern : public OpRewritePattern<sde::SdeCuTaskOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeCuTaskOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();

    // Convert SDE deps to ARTS db_control ops
    SmallVector<Value> artsDeps;
    for (Value dep : op.getDeps()) {
      auto muDep = dep.getDefiningOp<sde::SdeMuDepOp>();
      if (!muDep)
        continue;

      ArtsMode mode = convertAccessMode(muDep.getMode());
      SmallVector<Value> offsets(muDep.getOffsets().begin(),
                                 muDep.getOffsets().end());
      SmallVector<Value> sizes(muDep.getSizes().begin(),
                               muDep.getSizes().end());

      // Separate pinned dims (size=1) from chunk dims
      SmallVector<Value> pinnedIndices, chunkOffsets, blockSizes;
      if (!offsets.empty() && sizes.empty()) {
        pinnedIndices = offsets;
      } else {
        for (size_t i = 0; i < offsets.size() && i < sizes.size(); ++i) {
          if (ValueAnalysis::isOneConstant(sizes[i])) {
            pinnedIndices.push_back(offsets[i]);
          } else {
            chunkOffsets.push_back(offsets[i]);
            blockSizes.push_back(sizes[i]);
          }
        }
      }

      rewriter.setInsertionPoint(op);
      auto dbControl =
          DbControlOp::create(rewriter, loc, mode, muDep.getSource(),
                              pinnedIndices, chunkOffsets, blockSizes);
      artsDeps.push_back(dbControl);
    }

    auto edtOp = EdtOp::create(rewriter, loc, EdtType::task,
                               EdtConcurrency::intranode, artsDeps);
    edtOp.setNoVerifyAttr(NoVerifyAttr::get(ctx));

    Block &old = op.getBody().front();
    Block &blk = edtOp.getBody().front();
    blk.getOperations().splice(blk.end(), old.getOperations());

    ++numCuTaskConverted;
    rewriter.eraseOp(op);
    return success();
  }
};

/// sde.su_iterate -> arts.for
struct SuIterateToArtsPattern : public OpRewritePattern<sde::SdeSuIterateOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeSuIterateOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();

    auto schedAttr = convertSchedule(ctx, op.getScheduleAttr());

    auto artsFor =
        ForOp::create(rewriter, loc, op.getLowerBounds(), op.getUpperBounds(),
                      op.getSteps(), schedAttr, op.getReductionAccumulators());

    copyArtsMetadataAttrs(op, artsFor);

    // Move the body
    Region &dstRegion = artsFor.getRegion();
    Region &srcRegion = op.getBody();
    if (!srcRegion.empty()) {
      if (dstRegion.empty())
        dstRegion.push_back(new Block());
      Block &dst = dstRegion.front();
      Block &src = srcRegion.front();

      // Add IV arg if needed
      if (dst.getNumArguments() == 0 && src.getNumArguments() > 0)
        dst.addArgument(rewriter.getIndexType(), loc);

      // Map block args and clone body
      IRMapping mapper;
      if (!src.getArguments().empty() && !dst.getArguments().empty())
        mapper.map(src.getArgument(0), dst.getArgument(0));

      OpBuilder::InsertionGuard IG(rewriter);
      rewriter.setInsertionPointToStart(&dst);
      for (Operation &srcOp : src.without_terminator())
        rewriter.clone(srcOp, mapper);
      YieldOp::create(rewriter, loc);
    }

    ++numSuIterateConverted;
    rewriter.eraseOp(op);
    return success();
  }
};

/// sde.su_barrier -> arts.barrier
struct SuBarrierToArtsPattern : public OpRewritePattern<sde::SdeSuBarrierOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeSuBarrierOp op,
                                PatternRewriter &rewriter) const override {
    BarrierOp::create(rewriter, op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

/// sde.cu_atomic -> arts.atomic_add
struct CuAtomicToArtsPattern : public OpRewritePattern<sde::SdeCuAtomicOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeCuAtomicOp op,
                                PatternRewriter &rewriter) const override {
    // Currently ARTS only supports atomic_add
    if (op.getReductionKind() != sde::SdeReductionKind::add)
      return failure();
    rewriter.replaceOpWithNewOp<AtomicAddOp>(op, op.getAddr(), op.getValue());
    return success();
  }
};

/// sde.yield -> arts.yield
struct SdeYieldToArtsPattern : public OpRewritePattern<sde::SdeYieldOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeYieldOp op,
                                PatternRewriter &rewriter) const override {
    YieldOp::create(rewriter, op.getLoc());
    rewriter.eraseOp(op);
    return success();
  }
};

/// sde.mu_dep (standalone) -> arts.omp_dep
/// These are mu_deps not consumed by a cu_task — recreate as OmpDepOp
/// for downstream passes (CreateDbs, etc.) to consume.
struct MuDepToArtsPattern : public OpRewritePattern<sde::SdeMuDepOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeMuDepOp op,
                                PatternRewriter &rewriter) const override {
    // Only convert standalone mu_deps (those consumed by cu_task are
    // handled by CuTaskToArtsPattern)
    for (Operation *user : op.getDep().getUsers()) {
      if (isa<sde::SdeCuTaskOp>(user))
        return failure(); // Let CuTaskToArtsPattern handle this
    }

    ArtsMode mode = convertAccessMode(op.getMode());
    SmallVector<Value> offsets(op.getOffsets().begin(), op.getOffsets().end());
    SmallVector<Value> sizes(op.getSizes().begin(), op.getSizes().end());

    auto ompDep = OmpDepOp::create(rewriter, op.getLoc(), mode, op.getSource(),
                                   offsets, sizes);
    rewriter.replaceOp(op, ompDep.getResult());
    return success();
  }
};

/// sde.cu_reduce -> handled inline (reduce to atomic_add for now)
struct CuReduceToArtsPattern : public OpRewritePattern<sde::SdeCuReduceOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeCuReduceOp op,
                                PatternRewriter &rewriter) const override {
    // For add reductions on memref accumulators, lower to atomic_add
    if (op.getReductionKind() != sde::SdeReductionKind::add)
      return failure();
    auto acc = op.getAccumulator();
    if (!isa<MemRefType>(acc.getType()))
      return failure();

    auto atomicAdd =
        AtomicAddOp::create(rewriter, op.getLoc(), acc, op.getPartial());
    rewriter.eraseOp(op);
    return success();
  }
};

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

namespace {
struct ConvertSdeToArtsPass
    : public arts::impl::ConvertSdeToArtsBase<ConvertSdeToArtsPass> {

  explicit ConvertSdeToArtsPass(mlir::arts::AnalysisManager *AM = nullptr)
      : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(ConvertSdeToArtsPass);
    MLIRContext *context = &getContext();
    if (!AM) {
      module.emitError() << "ConvertSdeToArtsPass requires AnalysisManager";
      signalPassFailure();
      return;
    }
    RewritePatternSet patterns(context);
    // Process cu_task before mu_dep since cu_task consumes mu_dep results
    patterns.add<CuTaskToArtsPattern>(context);
    patterns.add<CuRegionToArtsPattern>(context);
    patterns.add<SuIterateToArtsPattern>(context);
    patterns.add<SuBarrierToArtsPattern>(context);
    patterns.add<CuAtomicToArtsPattern>(context);
    patterns.add<CuReduceToArtsPattern>(context);
    patterns.add<SdeYieldToArtsPattern>(context);
    patterns.add<MuDepToArtsPattern>(context);
    GreedyRewriteConfig config;
    (void)applyPatternsGreedily(module, std::move(patterns), config);

    ARTS_INFO_FOOTER(ConvertSdeToArtsPass);
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
createConvertSdeToArtsPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<ConvertSdeToArtsPass>(AM);
}
} // namespace sde
} // namespace arts
} // namespace mlir
