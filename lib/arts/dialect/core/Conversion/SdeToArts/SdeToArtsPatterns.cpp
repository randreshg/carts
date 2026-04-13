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
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "arts/utils/Utils.h"
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

// StructuredNeighborhoodSummary, AffineDimOffset, extractDimOffset, and
// hasConstantOffsets are provided by StructuredOpAnalysis.h.
using StructuredNeighborhoodSummary = sde::StructuredNeighborhoodInfo;

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
      auto dimOffset = sde::extractDimOffset(result);
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
      if (sde::hasConstantOffsets(maps[inputIdx]))
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

/// Stamp a simple (non-stencil) pattern contract on an op.
static void stampSimple(Operation *op, ArtsDepPattern family, int64_t rev) {
  if (!op)
    return;
  setDepPattern(op, family);
  setPatternRevision(op, rev);
  if (auto dist = getDistributionPatternForDepPattern(family))
    setEdtDistributionPattern(op, *dist);
}

/// Stamp a pattern contract on an arts.for and its parent EdtOp if present.
static void stampPatternContract(Operation *artsForOp, ArtsDepPattern pattern,
                                 int64_t revision = 1) {
  stampSimple(artsForOp, pattern, revision);
  if (auto parentEdt = artsForOp->getParentOfType<EdtOp>())
    stampSimple(parentEdt.getOperation(), pattern, revision);
}

/// Stamp a stencil-family contract on an op with spatial metadata.
static void stampStencilOnOp(Operation *op, ArtsDepPattern family,
                             ArrayRef<int64_t> ownerDims,
                             ArrayRef<int64_t> minOffsets,
                             ArrayRef<int64_t> maxOffsets,
                             ArrayRef<int64_t> writeFootprint,
                             ArrayRef<int64_t> spatialDims,
                             ArrayRef<int64_t> blockShape, int64_t rev) {
  if (!op)
    return;
  setDepPattern(op, family);
  setPatternRevision(op, rev);
  setEdtDistributionPattern(op, EdtDistributionPattern::stencil);
  setSupportedBlockHalo(op);
  ArrayRef<int64_t> dims = spatialDims.empty() ? ownerDims : spatialDims;
  setStencilSpatialDims(op, dims);
  setStencilOwnerDims(op, ownerDims);
  setStencilMinOffsets(op, minOffsets);
  setStencilMaxOffsets(op, maxOffsets);
  setStencilWriteFootprint(op, writeFootprint);
  if (!blockShape.empty())
    setStencilBlockShape(op, blockShape);
}

/// Stamp a recovered stencil-family contract on an arts.for and its parent
/// EdtOp.
static void stampStencilContract(Operation *artsForOp, ArtsDepPattern pattern,
                                 const StructuredNeighborhoodSummary &info,
                                 int64_t revision = 1) {
  stampStencilOnOp(artsForOp, pattern, info.ownerDims, info.minOffsets,
                   info.maxOffsets, info.writeFootprint, info.spatialDims,
                   /*blockShape=*/{}, revision);
  if (auto parentEdt = artsForOp->getParentOfType<EdtOp>())
    stampStencilOnOp(parentEdt.getOperation(), pattern, info.ownerDims,
                     info.minOffsets, info.maxOffsets, info.writeFootprint,
                     info.spatialDims, /*blockShape=*/{}, revision);
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
    if (auto reductionKindsAttr = op.getReductionKindsAttr()) {
      // Convert SDE enum attrs to plain I32 array for consumption by core
      // passes without introducing an SDE dialect dependency.
      SmallVector<Attribute> intAttrs;
      for (auto attr : reductionKindsAttr) {
        if (auto enumAttr = dyn_cast<sde::SdeReductionKindAttr>(attr))
          intAttrs.push_back(rewriter.getI32IntegerAttr(
              static_cast<int32_t>(enumAttr.getValue())));
      }
      if (!intAttrs.empty())
        artsFor->setAttr(AttrNames::Operation::Contract::ReductionKinds,
                         rewriter.getArrayAttr(intAttrs));
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
      // For internode (distributed scope), don't stamp distribution_kind —
      // let ARTS DistributionHeuristics pick topology-aware kinds like
      // two_level based on runtime config. SDE only knows about semantic
      // distribution (blocked/cyclic/owner_compute), not node topology.
      bool isInternode = false;
      if (auto parentEdt = artsFor.getOperation()->getParentOfType<EdtOp>())
        isInternode =
            (parentEdt.getConcurrency() == EdtConcurrency::internode);
      if (!isInternode) {
        stampDistributionKind(artsFor.getOperation(), *distributionKind);
        if (auto parentEdt = artsFor.getOperation()->getParentOfType<EdtOp>())
          stampDistributionKind(parentEdt.getOperation(), *distributionKind);
      }
    }

    // Forward vectorization hints from SDE
    if (auto vecWidth = op->getAttr("sde.vectorize_width"))
      artsFor->setAttr("sde.vectorize_width", vecWidth);
    if (auto unrollFactor = op->getAttr("sde.unroll_factor"))
      artsFor->setAttr("sde.unroll_factor", unrollFactor);

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
    if (!op->hasAttr("barrier_eliminated"))
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

//===----------------------------------------------------------------------===//
// RFC `raise-memref-to-tensor` — Step 2 lowering patterns.
//
// The RFC adds three tensor-path SDE ops (see
// docs/compiler/raise-memref-to-tensor-rfc.md §4 and §6):
//   - sde.mu_data     : tensor<...>            -> arts.db_alloc
//   - sde.mu_token    <mode>                    -> arts.db_acquire <mode>
//   - sde.cu_codelet (tokens...) -> (...)        -> arts.edt
//
// The lowering here is the lowering-only half. Step 3 (RaiseMemrefToTensor)
// is responsible for generating these ops from existing `sde.cu_task` /
// shared-memref IR; this file handles them whenever the transform (or a
// hand-written lit test) produces them.
//
// DB shape convention used here: one logical DB per `mu_data` with
// `sizes=[1]`, `elementSizes=SHAPE`, `elementType=T`. The ptr type is
// `memref<?xmemref<SHAPE x T>>`. Downstream code recovers the inner
// `memref<SHAPE x T>` via `arts.db_ref %ptr[0]`, then projects into tensor
// land with `bufferization.to_tensor`.
//===----------------------------------------------------------------------===//

/// Trace the DbAllocOp that backs a tensor value. Expected chain:
///   `%ptr = arts.db_alloc ...`
///   `%elem = arts.db_ref %ptr[0] : memref<?xmemref<SHAPExT>> -> memref<SHAPExT>`
///   `%tensor = bufferization.to_tensor %elem`
/// Returns nullptr when the tensor is not backed by a DbAllocOp chain.
static DbAllocOp findBackingDbAlloc(Value tensor) {
  // Walk past any intervening tensor.cast ops. The raise / codelet lowering
  // chain inserts these to reconcile dynamic-shape DB tensors with the
  // (static) codelet operand / yielded types, so a token whose source is a
  // codelet result typically has the chain:
  //   `%cast = tensor.cast %to_tensor : tensor<?xT> to tensor<SHAPExT>`
  //   `%to_tensor = bufferization.to_tensor %db_ref`.
  Value cur = tensor;
  while (auto cast = cur.getDefiningOp<tensor::CastOp>())
    cur = cast.getSource();
  auto toTensor = cur.getDefiningOp<bufferization::ToTensorOp>();
  if (!toTensor)
    return nullptr;
  Value buffer = toTensor.getBuffer();
  if (auto alloc = buffer.getDefiningOp<DbAllocOp>())
    return alloc;
  if (auto ref = buffer.getDefiningOp<DbRefOp>())
    return ref.getSource().getDefiningOp<DbAllocOp>();
  return nullptr;
}

/// Build a `bufferization.to_tensor` for a memref, producing a ranked tensor
/// of the same shape/element type.
static Value makeToTensor(OpBuilder &builder, Location loc, Value memref,
                          bool writable = false) {
  auto mrType = dyn_cast<MemRefType>(memref.getType());
  assert(mrType && "expected memref type");
  auto tensorType =
      RankedTensorType::get(mrType.getShape(), mrType.getElementType());
  UnitAttr writableAttr =
      writable ? UnitAttr::get(builder.getContext()) : UnitAttr{};
  return bufferization::ToTensorOp::create(
      builder, loc, tensorType, memref, /*restrict=*/UnitAttr{}, writableAttr);
}

/// Given a DB `sourcePtr` whose element type is the payload memref
/// `memref<SHAPExT>`, materialize that inner memref via `arts.db_ref[0]`.
static Value materializeInnerPayload(OpBuilder &builder, Location loc,
                                     Value sourcePtr) {
  Value zero = arts::createZeroIndex(builder, loc);
  return DbRefOp::create(builder, loc, sourcePtr, SmallVector<Value>{zero});
}

/// sde.mu_data : tensor<SHAPE x T>  ->  arts.db_alloc + db_ref + to_tensor
///
/// RFC §6: "Tensor-typed `mu_data` -> `arts.db_alloc` with the tensor's
/// shape." The tensor SSA value that downstream code still expects is
/// preserved via a `bufferization.to_tensor` over the DB's inner element
/// memref (obtained via `arts.db_ref %ptr[0]`).
static LogicalResult lowerMuData(sde::SdeMuDataOp op) {
  auto tensorType = dyn_cast<RankedTensorType>(op.getHandle().getType());
  if (!tensorType || !tensorType.hasStaticShape()) {
    // Memref-typed mu_data belongs to the v3+ fallback path; leave it alone.
    return success();
  }

  OpBuilder builder(op);
  Location loc = op.getLoc();

  // One DB holds the entire tensor. Outer `sizes=[1]` + inner
  // `elementSizes=SHAPE` keeps the payload addressable as a single
  // `memref<SHAPE x T>` via `arts.db_ref %ptr[0]`.
  SmallVector<Value> sizes{arts::createOneIndex(builder, loc)};
  SmallVector<Value> elementSizes;
  elementSizes.reserve(tensorType.getRank());
  for (int64_t dim : tensorType.getShape())
    elementSizes.push_back(arts::createConstantIndex(builder, loc, dim));

  Value route = arts::createCurrentNodeRoute(builder, loc);

  // TODO(raise-memref-to-tensor): honor `init` attribute. The RFC leaves the
  // initializer optional; until `arts.db_alloc` grows a first-class init
  // operand, a declarative init attribute is dropped here and the DB comes
  // up uninitialized. Step 3 can synthesize an initializer EDT if required.
  auto dbAlloc = DbAllocOp::create(builder, loc, ArtsMode::inout, route,
                                   DbAllocType::heap, DbMode::write,
                                   tensorType.getElementType(),
                                   /*address=*/Value{}, std::move(sizes),
                                   std::move(elementSizes));

  Value inner = materializeInnerPayload(builder, loc, dbAlloc.getPtr());
  Value replacementTensor =
      makeToTensor(builder, loc, inner, /*writable=*/true);
  op.getResult().replaceAllUsesWith(replacementTensor);
  op.erase();
  return success();
}

/// sde.cu_codelet (+ associated sde.mu_token operands) -> arts.edt
///
/// Each token operand becomes an `arts.db_acquire` whose ptr is carried as
/// an EDT dependency. The codelet body runs inside the EDT, with
/// `bufferization.to_tensor` bridging the memref block arg back to the
/// tensor-typed value the codelet body expects. Writable tokens surface as
/// destination-passing-style results: the yielded tensor is materialized
/// into the acquired DB memref, and the codelet's SSA result becomes a
/// fresh `bufferization.to_tensor` over the same DB ptr (the update is
/// observable through the DB handle, not through a new tensor SSA value).
static LogicalResult lowerCuCodelet(sde::SdeCuCodeletOp codelet) {
  Location loc = codelet.getLoc();
  auto *ctx = codelet.getContext();
  OpBuilder rewriter(codelet);

    // Each codelet operand must be produced by a `sde.mu_token` whose source
    // tensor is backed by a concrete DbAllocOp. If any token is not DB-backed
    // we cannot lower this codelet — bail out so the invariant is enforced at
    // VerifySdeLowered time rather than producing half-converted IR.
    SmallVector<sde::SdeMuTokenOp> muTokens;
    muTokens.reserve(codelet.getTokens().size());
    SmallVector<DbAllocOp> backingAllocs;
    backingAllocs.reserve(codelet.getTokens().size());
    for (Value token : codelet.getTokens()) {
      auto muToken = token.getDefiningOp<sde::SdeMuTokenOp>();
      if (!muToken)
        return codelet.emitOpError(
            "token operand is not produced by sde.mu_token");
      DbAllocOp alloc = findBackingDbAlloc(muToken.getSource());
      if (!alloc)
        return codelet.emitOpError(
            "token source is not backed by arts.db_alloc; RaiseMemrefToTensor "
            "must produce db-backed tensors for codelets to lower");
      muTokens.push_back(muToken);
      backingAllocs.push_back(alloc);
    }

    // Build one db_acquire per token. Read-only tokens surface as
    // arts.db_acquire <in>; write tokens as <out>; readwrite as <inout>.
    //
    // Slice tokens encode their element-space offsets/sizes on the acquire's
    // `partition_*` channels (DB-space `offsets`/`sizes` are single-DB
    // coordinates — always trivial for the sizes=[1] convention we use).
    rewriter.setInsertionPoint(codelet);
    SmallVector<Value> acquirePtrs;
    SmallVector<Type> blockArgTypes;
    SmallVector<Location> blockArgLocs;
    SmallVector<bool> writableFlags;
    acquirePtrs.reserve(muTokens.size());
    blockArgTypes.reserve(muTokens.size());
    blockArgLocs.reserve(muTokens.size());
    writableFlags.reserve(muTokens.size());

    for (auto [idx, muToken] : llvm::enumerate(muTokens)) {
      ArtsMode mode = convertAccessMode(muToken.getMode());
      bool writable = muToken.getMode() == sde::SdeAccessMode::write ||
                      muToken.getMode() == sde::SdeAccessMode::readwrite;
      writableFlags.push_back(writable);

      SmallVector<Value> partitionOffsets(muToken.getOffsets().begin(),
                                          muToken.getOffsets().end());
      SmallVector<Value> partitionSizes(muToken.getSizes().begin(),
                                        muToken.getSizes().end());

      DbAllocOp alloc = backingAllocs[idx];
      std::optional<PartitionMode> partitionMode;
      if (!partitionOffsets.empty() || !partitionSizes.empty())
        partitionMode = PartitionMode::block;
      // The DB's outer rank is 1 (one partition holding the whole tensor);
      // mirror that on the acquire so db_ref can index into it later.
      SmallVector<Value> dbOffsets{
          arts::createZeroIndex(rewriter, muToken.getLoc())};
      SmallVector<Value> dbSizes{
          arts::createOneIndex(rewriter, muToken.getLoc())};
      auto acq = DbAcquireOp::create(
          rewriter, muToken.getLoc(), mode, alloc.getGuid(), alloc.getPtr(),
          partitionMode,
          /*indices=*/SmallVector<Value>{}, std::move(dbOffsets),
          std::move(dbSizes),
          /*partitionIndices=*/SmallVector<Value>{},
          std::move(partitionOffsets), std::move(partitionSizes),
          /*boundsValid=*/Value{},
          /*elementOffsets=*/SmallVector<Value>{},
          /*elementSizes=*/SmallVector<Value>{});
      acquirePtrs.push_back(acq.getPtr());
      blockArgTypes.push_back(acq.getPtr().getType());
      blockArgLocs.push_back(muToken.getLoc());
    }

    // Create the EDT. Task-intranode is the default codelet concurrency per
    // RFC §6 — the raise does not encode cross-node placement yet.
    auto edtOp = EdtOp::create(rewriter, loc, EdtType::task,
                               EdtConcurrency::intranode, acquirePtrs);

    Block &edtBlock = edtOp.getBody().front();
    for (auto [argTy, argLoc] : llvm::zip(blockArgTypes, blockArgLocs))
      edtBlock.addArgument(argTy, argLoc);

    // Inside the EDT, materialize a tensor view for each block argument.
    // The block arg is the acquire's ptr (memref<?xmemref<SHAPExT>>); we use
    // `arts.db_ref[0]` + `bufferization.to_tensor` to land in tensor space
    // with shape `SHAPE`. Slice tokens then take `tensor.extract_slice` to
    // reach the codelet block arg's slice_type.
    OpBuilder::InsertionGuard bodyGuard(rewriter);
    rewriter.setInsertionPointToStart(&edtBlock);

    IRMapping mapper;
    SmallVector<Value> innerPayloads; // per-token inner memref
    innerPayloads.reserve(muTokens.size());
    Block &codeletBlock = codelet.getBody().front();
    for (auto [idx, codeletArg] :
         llvm::enumerate(codeletBlock.getArguments())) {
      Value edtArg = edtBlock.getArgument(idx);
      Value inner =
          materializeInnerPayload(rewriter, codelet.getLoc(), edtArg);
      innerPayloads.push_back(inner);

      Value view = makeToTensor(rewriter, codelet.getLoc(), inner,
                                /*writable=*/writableFlags[idx]);

      // Map the codelet block arg to the tensor view, narrowing to the
      // slice_type when the token addressed a sub-region.
      auto slicedType = dyn_cast<RankedTensorType>(codeletArg.getType());
      auto viewType = cast<RankedTensorType>(view.getType());
      if (slicedType && slicedType != viewType) {
        sde::SdeMuTokenOp muToken = muTokens[idx];
        if (muToken.getOffsets().empty() && muToken.getSizes().empty()) {
          // Whole-tensor token; the only mismatch is dynamic vs. static
          // shape, which is a plain tensor.cast.
          view = tensor::CastOp::create(rewriter, codelet.getLoc(),
                                        slicedType, view);
        } else {
          // Slice token: carry explicit offsets/sizes through extract_slice.
          // Offsets/sizes are dynamic SSA values, so ExtractSliceOp infers a
          // fully dynamic result type. Cast to the codelet block arg's
          // (possibly static) slice type afterwards.
          SmallVector<OpFoldResult> offsets, sizes, strides;
          offsets.reserve(slicedType.getRank());
          sizes.reserve(slicedType.getRank());
          strides.reserve(slicedType.getRank());
          Value oneV = arts::createOneIndex(rewriter, codelet.getLoc());
          for (int dim = 0; dim < slicedType.getRank(); ++dim) {
            offsets.push_back(OpFoldResult(muToken.getOffsets()[dim]));
            sizes.push_back(OpFoldResult(muToken.getSizes()[dim]));
            strides.push_back(OpFoldResult(oneV));
          }
          Value sliced = tensor::ExtractSliceOp::create(
              rewriter, codelet.getLoc(), view, offsets, sizes, strides);
          if (sliced.getType() != slicedType)
            sliced = tensor::CastOp::create(rewriter, codelet.getLoc(),
                                            slicedType, sliced);
          view = sliced;
        }
      }
      mapper.map(codeletArg, view);
    }

    // Clone codelet body ops (except the terminator) into the EDT body. The
    // terminator is a `sde.yield` whose operands drive destination-passing
    // materialization; we handle it explicitly below.
    Operation *terminator = codeletBlock.getTerminator();
    for (Operation &nested : codeletBlock.without_terminator())
      rewriter.insert(nested.clone(mapper));

    // For each writable token, materialize the yielded tensor into the
    // corresponding block-arg memref. `sde.cu_codelet` has one result per
    // writable token (RFC §4.4 V7/V8), and the terminator is `sde.yield`
    // with one operand per result.
    SmallVector<Value> yieldedValues;
    if (auto yieldOp = dyn_cast<sde::SdeYieldOp>(terminator)) {
      for (Value operand : yieldOp.getOperands())
        yieldedValues.push_back(mapper.lookupOrDefault(operand));
    }

    // Pair each yielded tensor with the writable inner payload in positional
    // order (matches RFC V7/V8). If the yielded tensor is a slice, insert it
    // back into a whole-tensor view via `tensor.insert_slice` so the final
    // materialization target has the full shape.
    unsigned yieldIdx = 0;
    for (auto [idx, writable] : llvm::enumerate(writableFlags)) {
      if (!writable)
        continue;
      if (yieldIdx >= yieldedValues.size())
        break;
      Value src = yieldedValues[yieldIdx++];
      Value destMemref = innerPayloads[idx];
      auto destMrType = cast<MemRefType>(destMemref.getType());
      auto destTensorType = RankedTensorType::get(destMrType.getShape(),
                                                  destMrType.getElementType());
      auto srcTensorType = cast<RankedTensorType>(src.getType());
      if (srcTensorType != destTensorType) {
        sde::SdeMuTokenOp muToken = muTokens[idx];
        if (muToken.getOffsets().empty() && muToken.getSizes().empty()) {
          // Whole-tensor token: static-vs-dynamic mismatch only.
          src = tensor::CastOp::create(rewriter, codelet.getLoc(),
                                       destTensorType, src);
        } else {
          // Slice update: stitch the slice back into a whole-tensor view
          // before materializing, preserving unchanged elements.
          Value viewTensor =
              makeToTensor(rewriter, codelet.getLoc(), destMemref,
                           /*writable=*/true);
          SmallVector<OpFoldResult> offsets, sizes, strides;
          offsets.reserve(destTensorType.getRank());
          sizes.reserve(destTensorType.getRank());
          strides.reserve(destTensorType.getRank());
          Value oneV = arts::createOneIndex(rewriter, codelet.getLoc());
          for (int dim = 0; dim < destTensorType.getRank(); ++dim) {
            offsets.push_back(OpFoldResult(muToken.getOffsets()[dim]));
            sizes.push_back(OpFoldResult(muToken.getSizes()[dim]));
            strides.push_back(OpFoldResult(oneV));
          }
          src = tensor::InsertSliceOp::create(rewriter, codelet.getLoc(), src,
                                              viewTensor, offsets, sizes,
                                              strides);
        }
      }
      bufferization::MaterializeInDestinationOp::create(
          rewriter, codelet.getLoc(), /*result=*/TypeRange{}, src, destMemref,
          /*restrict=*/UnitAttr{}, /*writable=*/UnitAttr::get(ctx));
    }

    YieldOp::create(rewriter, loc);

    // Rewire codelet results: each SSA result corresponds to a writable
    // token's parent tensor. Re-materialize the parent tensor from the DB's
    // inner payload so tensor users continue to type-check.
    rewriter.setInsertionPointAfter(edtOp);
    SmallVector<Value> newResults;
    newResults.reserve(codelet.getNumResults());
    unsigned writableIdx = 0;
    for (auto [idx, writable] : llvm::enumerate(writableFlags)) {
      if (!writable)
        continue;
      if (writableIdx >= codelet.getNumResults())
        break;
      DbAllocOp alloc = backingAllocs[idx];
      Value inner = materializeInnerPayload(rewriter, codelet.getLoc(),
                                            alloc.getPtr());
      Value parentTensor =
          makeToTensor(rewriter, codelet.getLoc(), inner, /*writable=*/true);
      Value result = codelet.getResult(writableIdx);
      if (parentTensor.getType() != result.getType()) {
        if (auto dstType = dyn_cast<RankedTensorType>(result.getType()))
          parentTensor = tensor::CastOp::create(rewriter, codelet.getLoc(),
                                                dstType, parentTensor);
      }
      newResults.push_back(parentTensor);
      ++writableIdx;
    }

    // Erase the codelet and its mu_token producers.
  codelet.getResults().replaceAllUsesWith(newResults);
  codelet.erase();
  for (sde::SdeMuTokenOp tok : muTokens)
    if (tok->use_empty())
      tok.erase();
  return success();
}

/// Trace `tensor` back through `tensor.cast` / `tensor.extract_slice` /
/// `tensor.insert` ops to the underlying memref that backs the tensor view.
/// Returns nullptr if the tensor is not backed by a memref-view chain we
/// can lower (e.g., it came from a non-tensor-path producer).
static Value traceTensorToBackingMemref(Value tensor) {
  Value cur = tensor;
  for (;;) {
    if (auto cast = cur.getDefiningOp<tensor::CastOp>()) {
      cur = cast.getSource();
      continue;
    }
    if (auto insert = cur.getDefiningOp<tensor::InsertOp>()) {
      cur = insert.getDest();
      continue;
    }
    if (auto toTensor = cur.getDefiningOp<bufferization::ToTensorOp>())
      return toTensor.getBuffer();
    return nullptr;
  }
}

/// Fold the tensor-path ops introduced by codelet lowering down to memref
/// load/store semantics. See `lowerTensorPathOps` Step 4 for context.
///
/// We walk the module in program order and transform each op in place:
///   - `tensor.extract %t[idx]` becomes `memref.load %m[idx]` when %t
///     traces to a memref view.
///   - `tensor.insert %val into %t[idx]` becomes `memref.store %val, %m[idx]`
///     and the insert's SSA result is RAUW'd with its dest (so downstream
///     extracts re-trace to the SAME backing memref, reading the store we
///     just emitted).
///   - `bufferization.materialize_in_destination` whose source tensor traces
///     back to the same memref is a no-op (the stores already happened).
///   - Dead tensor.cast / tensor.extract_slice / bufferization.to_tensor are
///     erased.
///
/// Walk order matters: a later `tensor.extract` on a value produced by an
/// earlier `tensor.insert` must be translated AFTER that insert has become a
/// memref.store, so the emitted `memref.load` is placed AFTER the store and
/// reads the fresh value.
static void foldTensorPathToMemref(ModuleOp module) {
  // Collect target ops in IR pre-order so we can process them as they
  // appear in the instruction stream. We intentionally do not use
  // `module.walk` inline because we mutate the IR.
  SmallVector<Operation *> ordered;
  module.walk<WalkOrder::PreOrder>([&](Operation *op) {
    if (isa<tensor::ExtractOp, tensor::InsertOp,
            bufferization::MaterializeInDestinationOp>(op))
      ordered.push_back(op);
  });

  for (Operation *op : ordered) {
    if (auto extract = dyn_cast<tensor::ExtractOp>(op)) {
      Value memref = traceTensorToBackingMemref(extract.getTensor());
      if (!memref)
        continue;
      auto mrType = dyn_cast<MemRefType>(memref.getType());
      if (!mrType)
        continue;
      if (static_cast<int64_t>(extract.getIndices().size()) !=
          mrType.getRank())
        continue;
      OpBuilder builder(extract);
      Value loaded = memref::LoadOp::create(builder, extract.getLoc(), memref,
                                            extract.getIndices());
      extract.getResult().replaceAllUsesWith(loaded);
      extract.erase();
    } else if (auto insert = dyn_cast<tensor::InsertOp>(op)) {
      Value memref = traceTensorToBackingMemref(insert.getDest());
      if (!memref)
        continue;
      auto mrType = dyn_cast<MemRefType>(memref.getType());
      if (!mrType)
        continue;
      if (static_cast<int64_t>(insert.getIndices().size()) != mrType.getRank())
        continue;
      OpBuilder builder(insert);
      memref::StoreOp::create(builder, insert.getLoc(), insert.getScalar(),
                              memref, insert.getIndices());
      insert.getResult().replaceAllUsesWith(insert.getDest());
      insert.erase();
    } else if (auto mat =
                   dyn_cast<bufferization::MaterializeInDestinationOp>(op)) {
      if (!traceTensorToBackingMemref(mat.getSource()))
        continue;
      mat.erase();
    }
  }

  // Dead cleanup for view / cast / slice / extract_slice ops left over.
  SmallVector<Operation *> pending;
  module.walk([&](Operation *op) {
    if (isa<bufferization::ToTensorOp, tensor::CastOp,
            tensor::ExtractSliceOp, tensor::InsertSliceOp>(op))
      pending.push_back(op);
  });
  // Iterate a couple of times to propagate dead-ness up a chain.
  for (int i = 0; i < 4; ++i) {
    bool anyErased = false;
    for (Operation *&op : pending) {
      if (!op)
        continue;
      if (op->use_empty()) {
        op->erase();
        op = nullptr;
        anyErased = true;
      }
    }
    if (!anyErased)
      break;
  }
}

/// Drive the tensor-path lowerings ahead of the greedy rewriter. Order:
///   1. `sde.cu_codelet`  ->  arts.edt (acquires + body + materialize).
///   2. `sde.mu_token` leftovers (orphan or producer-less) -> erase.
///   3. `sde.mu_data`     ->  arts.db_alloc + db_ref + to_tensor chain.
///   4. Fold tensor-path ops to memref ops.
///
/// Steps 1/2 before step 3 ensures that when we tear down a codelet, all of
/// its `sde.mu_token` operands are still resolvable back to their originating
/// `sde.mu_data` — which remains an SSA producer until step 3 replaces it
/// with the ARTS-side chain.
static LogicalResult lowerTensorPathOps(ModuleOp module) {
  // Step 1: lower codelets (+ their tokens).
  //
  // `lowerMuData` erases its input op and RAUWs the tensor result with a
  // `bufferization.to_tensor` over the new DB chain. When two codelets (or
  // two tokens of the same codelet) share a single `sde.mu_data`, the first
  // lowering erases it and subsequent `muToken.getSource()` lookups see the
  // replacement `to_tensor` instead of the original `mu_data` — so
  // `getDefiningOp<SdeMuDataOp>()` naturally returns null. We still guard
  // with a seen-set to make the intent explicit and to prevent any future
  // re-entrancy (e.g., if lookup traversal ever changes) from dereferencing
  // an erased op.
  DenseSet<sde::SdeMuDataOp> loweredMuData;
  SmallVector<sde::SdeCuCodeletOp> codelets;
  module.walk([&](sde::SdeCuCodeletOp op) { codelets.push_back(op); });
  for (sde::SdeCuCodeletOp codelet : codelets) {
    // Lower mu_data producers feeding this codelet's tokens first so that
    // `findBackingDbAlloc` can see the DB chain when codelet lowering runs.
    for (Value token : codelet.getTokens()) {
      auto muToken = token.getDefiningOp<sde::SdeMuTokenOp>();
      if (!muToken)
        continue;
      auto muData = muToken.getSource().getDefiningOp<sde::SdeMuDataOp>();
      if (!muData)
        continue;
      if (!loweredMuData.insert(muData).second)
        continue;
      if (failed(lowerMuData(muData)))
        return failure();
    }
    if (failed(lowerCuCodelet(codelet)))
      return failure();
  }

  // Step 2: drop orphan `sde.mu_token`s. After step 1 they should be
  // use-less; anything else is malformed input.
  SmallVector<sde::SdeMuTokenOp> orphanTokens;
  module.walk([&](sde::SdeMuTokenOp op) { orphanTokens.push_back(op); });
  for (sde::SdeMuTokenOp op : orphanTokens) {
    if (!op.getToken().use_empty())
      return op.emitOpError(
          "sde.mu_token still has users after codelet lowering");
    op.erase();
  }

  // Step 3: any remaining `sde.mu_data` (unused by codelets) still needs
  // lowering so VerifySdeLowered passes.
  SmallVector<sde::SdeMuDataOp> muDatas;
  module.walk([&](sde::SdeMuDataOp op) { muDatas.push_back(op); });
  for (sde::SdeMuDataOp op : muDatas) {
    if (failed(lowerMuData(op)))
      return failure();
  }

  // Step 4: fold the tensor-op chain produced by codelet lowering down to
  // pure memref loads/stores. ConvertArtsToLLVM does not know how to
  // consume `bufferization.to_tensor` / `materialize_in_destination` /
  // `tensor.insert` / `tensor.extract`, so we translate each of them to
  // the equivalent memref.load / memref.store against the backing DB
  // memref. The folds are only valid for the tensor-path pattern
  // RaiseMemrefToTensor emits (to_tensor of a db_ref, scalar extract /
  // insert, final materialize_in_destination on the same memref); other
  // tensor IR in the module is left untouched.
  foldTensorPathToMemref(module);

  return success();
}

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

    // Tensor-path lowering (RFC step 2) must run before the greedy driver
    // so its tensor.insert / tensor.extract body ops are cloned into EDTs
    // before region simplification can fold them away.
    if (failed(lowerTensorPathOps(module))) {
      signalPassFailure();
      return;
    }

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
    patterns.add<MuDepToArtsPattern>(context);
    patterns.add<MuReductionDeclToArtsPattern>(context);
    // RFC `raise-memref-to-tensor` step 2 — tensor-path lowerings.
    //
    // The codelet/mu_token/mu_data triple is handled ahead of the greedy
    // driver by `lowerTensorPathOps()` below. Running them as plain
    // OpRewritePatterns inside the greedy driver lets its region-
    // simplification pass DCE the codelet bodies between matches
    // (tensor-world ops are pure and look dead to the simplifier), which
    // would drop the very IR we're trying to lower.
    GreedyRewriteConfig config;
    config.setRegionSimplificationLevel(
        mlir::GreedySimplifyRegionLevel::Disabled);
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
