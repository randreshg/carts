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

#include "arts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_CONVERTSDETOARTS
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts
#include "arts/dialect/core/Transforms/pattern/PatternTransform.h"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Linalg/Transforms/Transforms.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(convert_sde_to_arts);

#include "llvm/ADT/STLExtras.h"
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

static std::optional<ArtsDepPattern>
mapStructuredClassificationToArtsDepPattern(
    sde::SdeStructuredClassification classification) {
  switch (classification) {
  case sde::SdeStructuredClassification::stencil:
    return ArtsDepPattern::stencil_tiling_nd;
  case sde::SdeStructuredClassification::matmul:
    return ArtsDepPattern::matmul;
  case sde::SdeStructuredClassification::elementwise:
    return ArtsDepPattern::uniform;
  case sde::SdeStructuredClassification::elementwise_pipeline:
    return ArtsDepPattern::elementwise_pipeline;
  case sde::SdeStructuredClassification::reduction:
    break;
  }
  return std::nullopt;
}

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

static std::optional<EdtDistributionKind>
convertDistributionKind(sde::SdeDistributionKindAttr kind) {
  if (!kind)
    return std::nullopt;

  switch (kind.getValue()) {
  case sde::SdeDistributionKind::owner_compute:
  case sde::SdeDistributionKind::blocked:
    return EdtDistributionKind::block;
  case sde::SdeDistributionKind::cyclic:
    return EdtDistributionKind::block_cyclic;
  }
  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// Linalg-derived contract classification helpers
//===----------------------------------------------------------------------===//

/// Runtime-neutral structured neighborhood summary recovered at the
/// SDE-to-ARTS boundary.
struct StructuredNeighborhoodSummary {
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

/// Recover a structured neighborhood summary from linalg.generic read maps.
static std::optional<StructuredNeighborhoodSummary>
extractNeighborhoodSummaryFromLinalg(linalg::GenericOp generic) {
  unsigned numLoops = generic.getNumLoops();
  if (numLoops == 0)
    return std::nullopt;

  auto maps = generic.getIndexingMapsArray();
  StructuredNeighborhoodSummary info;
  info.minOffsets.assign(numLoops, 0);
  info.maxOffsets.assign(numLoops, 0);
  info.writeFootprint.assign(numLoops, 1);
  for (unsigned dim = 0; dim < numLoops; ++dim) {
    info.ownerDims.push_back(dim);
    info.spatialDims.push_back(dim);
  }

  bool sawNeighborhoodOffset = false;
  for (unsigned inputIdx = 0; inputIdx < generic.getNumDpsInputs();
       ++inputIdx) {
    AffineMap map = maps[inputIdx];
    for (AffineExpr result : map.getResults()) {
      auto dimOffset = extractDimOffset(result);
      if (!dimOffset || !dimOffset->dim)
        continue;

      unsigned dim = *dimOffset->dim;
      if (dim >= numLoops)
        continue;

      info.minOffsets[dim] = std::min(info.minOffsets[dim], dimOffset->offset);
      info.maxOffsets[dim] = std::max(info.maxOffsets[dim], dimOffset->offset);
      sawNeighborhoodOffset |= dimOffset->offset != 0;
    }
  }

  if (!sawNeighborhoodOffset)
    return std::nullopt;
  return info;
}

/// Merge two neighborhood summaries across multiple linalg.generic ops.
static void mergeNeighborhoodSummary(StructuredNeighborhoodSummary &dst,
                                     const StructuredNeighborhoodSummary &src) {
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

/// Recover an already-materialized neighborhood summary from an op.
static std::optional<StructuredNeighborhoodSummary>
getStampedNeighborhoodSummary(Operation *op) {
  if (!op)
    return std::nullopt;
  if (!getStencilMinOffsets(op) || !getStencilMaxOffsets(op) ||
      !getStencilOwnerDims(op) || !getStencilWriteFootprint(op))
    return std::nullopt;

  StructuredNeighborhoodSummary info;
  info.ownerDims = *getStencilOwnerDims(op);
  info.spatialDims = getStencilSpatialDims(op).value_or(info.ownerDims);
  info.minOffsets = *getStencilMinOffsets(op);
  info.maxOffsets = *getStencilMaxOffsets(op);
  info.writeFootprint = *getStencilWriteFootprint(op);
  return info;
}

static std::optional<StructuredNeighborhoodSummary>
getSdeNeighborhoodSummary(sde::SdeSuIterateOp op) {
  if (!op.getAccessMinOffsetsAttr() || !op.getAccessMaxOffsetsAttr() ||
      !op.getOwnerDimsAttr() || !op.getWriteFootprintAttr())
    return std::nullopt;

  auto readArrayAttr =
      [](ArrayAttr attr) -> std::optional<SmallVector<int64_t, 4>> {
    if (!attr)
      return std::nullopt;
    SmallVector<int64_t, 4> values;
    values.reserve(attr.size());
    for (Attribute element : attr) {
      auto intAttr = dyn_cast<IntegerAttr>(element);
      if (!intAttr)
        return std::nullopt;
      values.push_back(intAttr.getInt());
    }
    return values;
  };

  auto minOffsets = readArrayAttr(op.getAccessMinOffsetsAttr());
  auto maxOffsets = readArrayAttr(op.getAccessMaxOffsetsAttr());
  auto ownerDims = readArrayAttr(op.getOwnerDimsAttr());
  auto writeFootprint = readArrayAttr(op.getWriteFootprintAttr());
  if (!minOffsets || !maxOffsets || !ownerDims || !writeFootprint)
    return std::nullopt;

  StructuredNeighborhoodSummary info;
  info.minOffsets = std::move(*minOffsets);
  info.maxOffsets = std::move(*maxOffsets);
  info.ownerDims = std::move(*ownerDims);
  info.writeFootprint = std::move(*writeFootprint);
  if (auto spatialDims = readArrayAttr(op.getSpatialDimsAttr()))
    info.spatialDims = std::move(*spatialDims);
  else
    info.spatialDims = info.ownerDims;
  return info;
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
    for (unsigned inputIdx = 0; inputIdx < generic.getNumDpsInputs();
         ++inputIdx) {
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

static StencilNDPatternContract
materializeStencilContract(ArtsDepPattern pattern,
                           const StructuredNeighborhoodSummary &info,
                           int64_t revision = 1) {
  return StencilNDPatternContract(
      pattern, info.ownerDims, info.minOffsets, info.maxOffsets,
      info.writeFootprint, revision, /*blockShape=*/{}, info.spatialDims);
}

/// Stamp a recovered stencil-family contract on an arts.for and its parent
/// EdtOp.
static void stampStencilContract(Operation *artsForOp, ArtsDepPattern pattern,
                                 const StructuredNeighborhoodSummary &info,
                                 int64_t revision = 1) {
  StencilNDPatternContract contract =
      materializeStencilContract(pattern, info, revision);
  contract.stamp(artsForOp);
  if (auto parentEdt = artsForOp->getParentOfType<EdtOp>())
    contract.stamp(parentEdt.getOperation());
}

static void stampDistributionKind(Operation *op, EdtDistributionKind kind,
                                  int64_t version = 1) {
  if (!op)
    return;

  setEdtDistributionKind(op, kind);
  op->setAttr(
      AttrNames::Operation::DistributionVersion,
      IntegerAttr::get(IntegerType::get(op->getContext(), 32), version));
}

static bool isTensorCarrierOp(Operation *op) {
  if (!op)
    return false;
  if (isa<bufferization::ToTensorOp>(op))
    return true;
  if (isa<tensor::TensorDialect>(op->getDialect()))
    return true;
  if (auto linalgOp = dyn_cast<linalg::LinalgOp>(op))
    return linalgOp.hasPureTensorSemantics();
  return false;
}

/// Drop dead transient tensor carriers after their linalg owner has been
/// consumed for contract stamping.
static void eraseDeadTensorCarriers(ForOp artsFor, PatternRewriter &rewriter) {
  bool changed = true;
  while (changed) {
    changed = false;

    SmallVector<Operation *> deadCarriers;
    artsFor.getRegion().walk([&](Operation *nestedOp) {
      if (!isTensorCarrierOp(nestedOp))
        return;
      if (isOpTriviallyDead(nestedOp))
        deadCarriers.push_back(nestedOp);
    });

    for (Operation *deadCarrier : llvm::reverse(deadCarriers)) {
      rewriter.eraseOp(deadCarrier);
      changed = true;
    }
  }
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

/// arts_sde.su_distribute -> inline wrapped body
struct SuDistributeToArtsPattern
    : public OpRewritePattern<sde::SdeSuDistributeOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeSuDistributeOp op,
                                PatternRewriter &rewriter) const override {
    Region &body = op.getBody();
    if (body.empty())
      return failure();

    bool hasNestedSdeIterate = false;
    body.walk([&](sde::SdeSuIterateOp) -> WalkResult {
      hasNestedSdeIterate = true;
      return WalkResult::interrupt();
    });
    if (hasNestedSdeIterate)
      return failure();

    Block &block = body.front();
    if (block.getNumArguments() != 0)
      return failure();

    if (!block.empty())
      if (auto yield = dyn_cast<sde::SdeYieldOp>(&block.back()))
        rewriter.eraseOp(yield);

    rewriter.inlineBlockBefore(&block, op, ValueRange{});
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
    auto enclosingDistributeOp = op->getParentOfType<sde::SdeSuDistributeOp>();
    auto enclosingDistribution =
        enclosingDistributeOp ? enclosingDistributeOp.getKindAttr() : nullptr;

    auto artsFor = ForOp::create(
        rewriter, loc, op.getLowerBounds(), op.getUpperBounds(), op.getSteps(),
        schedAttr, op.getChunkSize(), op.getReductionAccumulators());

    copyArtsMetadataAttrs(op, artsFor);
    if (auto strategyAttr =
            convertReductionStrategy(ctx, op.getReductionStrategyAttr())) {
      artsFor->setAttr(AttrNames::Operation::Contract::ReductionStrategy,
                       strategyAttr);
      if (auto parentEdt = artsFor->getParentOfType<EdtOp>();
          parentEdt && !parentEdt->hasAttr(
                           AttrNames::Operation::Contract::ReductionStrategy))
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
      for (auto [srcArg, dstArg] :
           llvm::zip(src.getArguments(), dst.getArguments()))
        mapper.map(srcArg, dstArg);

      OpBuilder::InsertionGuard IG(rewriter);
      rewriter.setInsertionPointToStart(&dst);
      for (Operation &srcOp : src.without_terminator())
        rewriter.clone(srcOp, mapper);
      YieldOp::create(rewriter, loc);
    }

    SmallVector<linalg::GenericOp> linalgGenerics;
    artsFor.getRegion().walk(
        [&](linalg::GenericOp generic) { linalgGenerics.push_back(generic); });

    linalg::GenericOp contractSource;
    std::optional<ArtsDepPattern> selectedPattern;
    std::optional<StructuredNeighborhoodSummary> selectedStencilContract;
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

      std::optional<StructuredNeighborhoodSummary> candidateStencil;
      if (isStencilFamilyDepPattern(*candidatePattern)) {
        candidateStencil = extractNeighborhoodSummaryFromLinalg(generic);
        if (!candidateStencil)
          candidateStencil =
              getStampedNeighborhoodSummary(generic.getOperation());
      }

      if (!selectedPattern) {
        selectedPattern = candidatePattern;
        selectedStencilContract = candidateStencil;
        selectedRevision =
            getPatternRevision(generic.getOperation()).value_or(1);
        selectedFromExplicitContract = fromExplicitContract;
        contractSource = generic;
        return;
      }

      if (*selectedPattern == *candidatePattern) {
        if (candidateStencil) {
          if (selectedStencilContract)
            mergeNeighborhoodSummary(*selectedStencilContract,
                                     *candidateStencil);
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
        selectedRevision =
            getPatternRevision(generic.getOperation()).value_or(1);
        selectedFromExplicitContract = true;
        contractSource = generic;
        return;
      }

      ARTS_DEBUG(
          "conflicting linalg.generic contracts in arts.for body: keeping "
          << stringifyArtsDepPattern(*selectedPattern) << ", ignoring "
          << stringifyArtsDepPattern(*candidatePattern));
    };

    if (auto classAttr = op.getStructuredClassificationAttr()) {
      selectedPattern =
          mapStructuredClassificationToArtsDepPattern(classAttr.getValue());
      selectedRevision = getPatternRevision(op.getOperation()).value_or(1);
      if (selectedPattern && isStencilFamilyDepPattern(*selectedPattern))
        selectedStencilContract = getSdeNeighborhoodSummary(op);
    }

    // Fallback to transient carrier inference when no SDE-owned structured
    // classification or summary exists on the source loop.
    if (!selectedPattern) {
      for (linalg::GenericOp generic : linalgGenerics)
        considerLinalgContract(generic);
      if (contractSource) {
        copySemanticContractAttrs(contractSource.getOperation(),
                                  artsFor.getOperation());
        if (auto parentEdt = artsFor.getOperation()->getParentOfType<EdtOp>())
          copySemanticContractAttrs(contractSource.getOperation(),
                                    parentEdt.getOperation());
      }
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

    if (auto distributionKind =
            convertDistributionKind(enclosingDistribution)) {
      stampDistributionKind(artsFor.getOperation(), *distributionKind);
      if (auto parentEdt = artsFor.getOperation()->getParentOfType<EdtOp>())
        stampDistributionKind(parentEdt.getOperation(), *distributionKind);
    }

    // RaiseToLinalg leaves the original loop body in place and uses
    // linalg.generic as a transient carrier for contract stamping. Drop the
    // cloned memref-backed carriers directly; tensor-backed carriers are
    // removed by the dead-carrier sweep below so any dead tensor chain rooted
    // at their results disappears without teaching ARTS about tensor IR.
    for (linalg::GenericOp generic : llvm::reverse(linalgGenerics)) {
      if (generic.hasPureTensorSemantics())
        continue;
      rewriter.eraseOp(generic);
    }
    eraseDeadTensorCarriers(artsFor, rewriter);

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

/// arts_sde.mu_access -> annotation only
struct MuAccessToArtsPattern : public OpRewritePattern<sde::SdeMuAccessOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeMuAccessOp op,
                                PatternRewriter &rewriter) const override {
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

/// arts_sde.mu_reduction_decl -> semantic declaration only
struct MuReductionDeclToArtsPattern
    : public OpRewritePattern<sde::SdeMuReductionDeclOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(sde::SdeMuReductionDeclOp op,
                                PatternRewriter &rewriter) const override {
    rewriter.eraseOp(op);
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
    patterns.add<SuDistributeToArtsPattern>(context);
    patterns.add<SuIterateToArtsPattern>(context);
    patterns.add<SuBarrierToArtsPattern>(context);
    patterns.add<CuAtomicToArtsPattern>(context);
    patterns.add<CuReduceToArtsPattern>(context);
    patterns.add<SdeYieldToArtsPattern>(context);
    patterns.add<MuAccessToArtsPattern>(context);
    patterns.add<MuDepToArtsPattern>(context);
    patterns.add<MuReductionDeclToArtsPattern>(context);
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
