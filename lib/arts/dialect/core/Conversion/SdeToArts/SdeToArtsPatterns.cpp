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
#include "arts/dialect/core/Transforms/pattern/PatternTransform.h"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Linalg/Transforms/Transforms.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(convert_sde_to_arts);

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
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

/// Map an SDE reduction strategy to the persisted ARTS contract string.
static StringAttr
convertReductionStrategy(MLIRContext *ctx,
                         sde::SdeReductionStrategyAttr strategy) {
  if (!strategy)
    return nullptr;

  namespace ReductionStrategyValue =
      AttrNames::Operation::Contract::ReductionStrategyValue;
  switch (strategy.getValue()) {
  case sde::SdeReductionStrategy::atomic:
    return StringAttr::get(ctx, ReductionStrategyValue::Atomic);
  case sde::SdeReductionStrategy::tree:
    return StringAttr::get(ctx, ReductionStrategyValue::Tree);
  case sde::SdeReductionStrategy::local_accumulate:
    return StringAttr::get(ctx, ReductionStrategyValue::LocalAccumulate);
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Linalg-derived contract classification helpers
//===----------------------------------------------------------------------===//

/// Map an SDE loop classification to an ArtsDepPattern.
static std::optional<ArtsDepPattern>
classifyFromSde(sde::SdeLinalgClassification classification) {
  switch (classification) {
  case sde::SdeLinalgClassification::stencil:
    return ArtsDepPattern::stencil_tiling_nd;
  case sde::SdeLinalgClassification::matmul:
    return ArtsDepPattern::matmul;
  case sde::SdeLinalgClassification::elementwise:
    return ArtsDepPattern::uniform;
  case sde::SdeLinalgClassification::reduction:
    break;
  }
  return std::nullopt;
}

/// Structured stencil footprint recovered from a linalg.generic indexing map.
struct LinalgStencilContractInfo {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> spatialDims;
  SmallVector<int64_t, 4> minOffsets;
  SmallVector<int64_t, 4> maxOffsets;
  SmallVector<int64_t, 4> writeFootprint;
};

/// Affine expression normalized to one loop dim plus a constant offset.
struct AffineDimOffset {
  std::optional<unsigned> dim;
  int64_t offset = 0;
};

/// Extract a single-dim + constant form from an affine expression.
static std::optional<AffineDimOffset> extractDimOffset(AffineExpr expr) {
  if (auto dimExpr = dyn_cast<AffineDimExpr>(expr))
    return AffineDimOffset{dimExpr.getPosition(), 0};
  if (auto cstExpr = dyn_cast<AffineConstantExpr>(expr))
    return AffineDimOffset{std::nullopt, cstExpr.getValue()};

  auto binExpr = dyn_cast<AffineBinaryOpExpr>(expr);
  if (!binExpr)
    return std::nullopt;

  auto lhs = extractDimOffset(binExpr.getLHS());
  auto rhs = extractDimOffset(binExpr.getRHS());
  if (!lhs || !rhs)
    return std::nullopt;

  switch (binExpr.getKind()) {
  case AffineExprKind::Add: {
    if (lhs->dim && rhs->dim)
      return std::nullopt;
    return AffineDimOffset{lhs->dim ? lhs->dim : rhs->dim,
                           lhs->offset + rhs->offset};
  }
  default:
    return std::nullopt;
  }
}

/// Check whether an indexing map contains any constant stencil offsets.
static bool hasConstantOffsets(AffineMap map) {
  for (AffineExpr result : map.getResults()) {
    auto dimOffset = extractDimOffset(result);
    if (dimOffset && dimOffset->dim && dimOffset->offset != 0)
      return true;
  }
  return false;
}

/// Recover a stencil footprint from the linalg.generic read maps.
static std::optional<LinalgStencilContractInfo>
extractStencilContract(linalg::GenericOp generic) {
  unsigned numLoops = generic.getNumLoops();
  if (numLoops == 0)
    return std::nullopt;

  auto maps = generic.getIndexingMapsArray();
  LinalgStencilContractInfo info;
  info.minOffsets.assign(numLoops, 0);
  info.maxOffsets.assign(numLoops, 0);
  info.writeFootprint.assign(numLoops, 1);
  for (unsigned dim = 0; dim < numLoops; ++dim) {
    info.ownerDims.push_back(dim);
    info.spatialDims.push_back(dim);
  }

  bool sawStencilOffset = false;
  for (unsigned inputIdx = 0; inputIdx < generic.getNumDpsInputs(); ++inputIdx) {
    AffineMap map = maps[inputIdx];
    for (AffineExpr result : map.getResults()) {
      auto dimOffset = extractDimOffset(result);
      if (!dimOffset || !dimOffset->dim)
        continue;

      unsigned dim = *dimOffset->dim;
      if (dim >= numLoops)
        continue;

      info.minOffsets[dim] =
          std::min(info.minOffsets[dim], dimOffset->offset);
      info.maxOffsets[dim] =
          std::max(info.maxOffsets[dim], dimOffset->offset);
      sawStencilOffset |= dimOffset->offset != 0;
    }
  }

  if (!sawStencilOffset)
    return std::nullopt;
  return info;
}

/// Merge two stencil summaries across multiple linalg.generic ops.
static void mergeStencilContract(LinalgStencilContractInfo &dst,
                                 const LinalgStencilContractInfo &src) {
  if (dst.minOffsets.empty()) {
    dst = src;
    return;
  }

  if (dst.minOffsets.size() != src.minOffsets.size())
    return;

  for (auto [idx, minOffset] : llvm::enumerate(src.minOffsets))
    dst.minOffsets[idx] = std::min(dst.minOffsets[idx], minOffset);
  for (auto [idx, maxOffset] : llvm::enumerate(src.maxOffsets))
    dst.maxOffsets[idx] = std::max(dst.maxOffsets[idx], maxOffset);
}

/// Classify a linalg.generic directly from its iterator structure.
static std::optional<ArtsDepPattern>
classifyFromLinalg(linalg::GenericOp generic) {
  auto iterTypes = generic.getIteratorTypesArray();
  if (iterTypes.empty())
    return std::nullopt;

  bool allParallel = llvm::all_of(iterTypes, [](utils::IteratorType type) {
    return type == utils::IteratorType::parallel;
  });
  if (allParallel) {
    auto maps = generic.getIndexingMapsArray();
    for (unsigned inputIdx = 0; inputIdx < generic.getNumDpsInputs(); ++inputIdx) {
      if (hasConstantOffsets(maps[inputIdx]))
        return ArtsDepPattern::stencil_tiling_nd;
    }
    return ArtsDepPattern::uniform;
  }

  unsigned numParallel = 0, numReduction = 0;
  for (utils::IteratorType type : iterTypes) {
    if (type == utils::IteratorType::parallel)
      ++numParallel;
    else
      ++numReduction;
  }

  if (numParallel == 2 && numReduction == 1 && iterTypes.size() == 3 &&
      generic.getNumDpsInputs() >= 2 && generic.getNumDpsInits() >= 1)
    return ArtsDepPattern::matmul;

  return std::nullopt;
}

/// Stamp a pattern contract on an arts.for and its parent EdtOp if present.
static void stampPatternContract(Operation *artsForOp, ArtsDepPattern pattern,
                                 int64_t revision = 1) {
  SimplePatternContract contract(pattern, revision);
  contract.stamp(artsForOp);
  if (auto parentEdt = artsForOp->getParentOfType<EdtOp>())
    contract.stamp(parentEdt.getOperation());
}

/// Stamp a recovered stencil contract on an arts.for and its parent EdtOp.
static void stampStencilContract(Operation *artsForOp, ArtsDepPattern pattern,
                                 const LinalgStencilContractInfo &info,
                                 int64_t revision = 1) {
  StencilNDPatternContract contract(
      pattern, info.ownerDims, info.minOffsets, info.maxOffsets,
      info.writeFootprint, revision, /*blockShape=*/{}, info.spatialDims);
  contract.stamp(artsForOp);
  if (auto parentEdt = artsForOp->getParentOfType<EdtOp>())
    contract.stamp(parentEdt.getOperation());
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
/// If the body contains transient linalg.generic carriers from RaiseToLinalg,
/// derive and stamp the matching contract from them. Otherwise, fall back to
/// the classification stamped on the source sde.su_iterate. The transient
/// carriers are erased after stamping so downstream passes keep the loop/memref
/// IR shape.
struct SuIterateToArtsPattern : public OpRewritePattern<sde::SdeSuIterateOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeSuIterateOp op,
                                PatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto *ctx = rewriter.getContext();

    auto schedAttr = convertSchedule(ctx, op.getScheduleAttr());

    auto artsFor =
        ForOp::create(rewriter, loc, op.getLowerBounds(), op.getUpperBounds(),
                      op.getSteps(), schedAttr, op.getChunkSize(),
                      op.getReductionAccumulators());

    copyArtsMetadataAttrs(op, artsFor);
    if (auto strategyAttr =
            convertReductionStrategy(ctx, op.getReductionStrategyAttr())) {
      artsFor->setAttr(AttrNames::Operation::Contract::ReductionStrategy,
                       strategyAttr);
      if (auto parentEdt = artsFor->getParentOfType<EdtOp>();
          parentEdt &&
          !parentEdt->hasAttr(AttrNames::Operation::Contract::ReductionStrategy))
        parentEdt->setAttr(AttrNames::Operation::Contract::ReductionStrategy,
                           strategyAttr);
    }

    // Move the body
    Region &dstRegion = artsFor.getRegion();
    Region &srcRegion = op.getBody();
    if (!srcRegion.empty()) {
      if (dstRegion.empty())
        dstRegion.push_back(new Block());
      Block &dst = dstRegion.front();
      Block &src = srcRegion.front();

      // Mirror the source IV list so cloned linalg/scf bodies keep using the
      // same induction variable ordering after SDE->ARTS conversion.
      while (dst.getNumArguments() < src.getNumArguments())
        dst.addArgument(rewriter.getIndexType(), loc);

      // Map block args and clone body
      IRMapping mapper;
      for (auto [srcArg, dstArg] : llvm::zip(src.getArguments(),
                                             dst.getArguments()))
        mapper.map(srcArg, dstArg);

      OpBuilder::InsertionGuard IG(rewriter);
      rewriter.setInsertionPointToStart(&dst);
      for (Operation &srcOp : src.without_terminator())
        rewriter.clone(srcOp, mapper);
      YieldOp::create(rewriter, loc);
    }

    SmallVector<linalg::GenericOp> linalgGenerics;
    artsFor.getRegion().walk([&](linalg::GenericOp generic) {
      linalgGenerics.push_back(generic);
    });

    linalg::GenericOp contractSource;
    std::optional<ArtsDepPattern> selectedPattern;
    std::optional<LinalgStencilContractInfo> selectedStencilContract;
    int64_t selectedRevision = 1;
    bool selectedFromExplicitContract = false;

    auto considerLinalgContract = [&](linalg::GenericOp generic) {
      std::optional<ArtsDepPattern> candidatePattern;
      bool fromExplicitContract = false;

      if (auto depPattern = getDepPattern(generic.getOperation());
          depPattern && *depPattern != ArtsDepPattern::unknown) {
        candidatePattern = *depPattern;
        fromExplicitContract = true;
      } else {
        candidatePattern = classifyFromLinalg(generic);
      }

      if (!candidatePattern)
        return;

      std::optional<LinalgStencilContractInfo> candidateStencil;
      if (isStencilFamilyDepPattern(*candidatePattern)) {
        candidateStencil = extractStencilContract(generic);
        if (!candidateStencil && getStencilMinOffsets(generic.getOperation()) &&
            getStencilMaxOffsets(generic.getOperation()) &&
            getStencilOwnerDims(generic.getOperation()) &&
            getStencilWriteFootprint(generic.getOperation())) {
          LinalgStencilContractInfo info;
          info.ownerDims = *getStencilOwnerDims(generic.getOperation());
          info.spatialDims =
              getStencilSpatialDims(generic.getOperation()).value_or(
                  info.ownerDims);
          info.minOffsets = *getStencilMinOffsets(generic.getOperation());
          info.maxOffsets = *getStencilMaxOffsets(generic.getOperation());
          info.writeFootprint =
              *getStencilWriteFootprint(generic.getOperation());
          candidateStencil = std::move(info);
        }
      }

      if (!selectedPattern) {
        selectedPattern = candidatePattern;
        selectedStencilContract = candidateStencil;
        selectedRevision = getPatternRevision(generic.getOperation()).value_or(1);
        selectedFromExplicitContract = fromExplicitContract;
        contractSource = generic;
        return;
      }

      if (*selectedPattern == *candidatePattern) {
        if (candidateStencil) {
          if (selectedStencilContract)
            mergeStencilContract(*selectedStencilContract, *candidateStencil);
          else
            selectedStencilContract = candidateStencil;
        }

        if (fromExplicitContract && !selectedFromExplicitContract) {
          selectedRevision =
              getPatternRevision(generic.getOperation()).value_or(1);
          selectedFromExplicitContract = true;
          contractSource = generic;
        }
        return;
      }

      if (fromExplicitContract && !selectedFromExplicitContract) {
        selectedPattern = candidatePattern;
        selectedStencilContract = candidateStencil;
        selectedRevision = getPatternRevision(generic.getOperation()).value_or(1);
        selectedFromExplicitContract = true;
        contractSource = generic;
        return;
      }

      ARTS_DEBUG("conflicting linalg.generic contracts in arts.for body: keeping "
                 << stringifyArtsDepPattern(*selectedPattern)
                 << ", ignoring " << stringifyArtsDepPattern(*candidatePattern));
    };

    for (linalg::GenericOp generic : linalgGenerics)
      considerLinalgContract(generic);

    if (contractSource) {
      copySemanticContractAttrs(contractSource.getOperation(),
                                artsFor.getOperation());
      if (auto parentEdt = artsFor.getOperation()->getParentOfType<EdtOp>())
        copySemanticContractAttrs(contractSource.getOperation(),
                                  parentEdt.getOperation());
    }

    // Fallback to RaiseToLinalg's SDE-owned loop classification when no
    // linalg.generic survived into the cloned body.
    if (!selectedPattern) {
      if (auto classAttr = op.getLinalgClassificationAttr())
        selectedPattern = classifyFromSde(classAttr.getValue());
    }

    if (selectedPattern) {
      ARTS_DEBUG("stamping linalg-derived contract: "
                 << stringifyArtsDepPattern(*selectedPattern));
      if (selectedStencilContract &&
          isStencilFamilyDepPattern(*selectedPattern)) {
        stampStencilContract(artsFor.getOperation(), *selectedPattern,
                             *selectedStencilContract, selectedRevision);
      } else {
        stampPatternContract(artsFor.getOperation(), *selectedPattern,
                             selectedRevision);
      }
    }

    // RaiseToLinalg leaves the original loop body in place and uses
    // linalg.generic as a transient carrier for contract stamping. Drop the
    // cloned generics after consuming them so downstream passes keep seeing
    // the cloned loop/memref IR shape.
    for (linalg::GenericOp generic : llvm::reverse(linalgGenerics)) {
      rewriter.eraseOp(generic);
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

    (void)AtomicAddOp::create(rewriter, op.getLoc(), acc, op.getPartial());
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

  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(ConvertSdeToArtsPass);
    MLIRContext *context = &getContext();
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
};
} // namespace

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {
namespace sde {
std::unique_ptr<Pass> createConvertSdeToArtsPass() {
  return std::make_unique<ConvertSdeToArtsPass>();
}
} // namespace sde
} // namespace arts
} // namespace mlir
