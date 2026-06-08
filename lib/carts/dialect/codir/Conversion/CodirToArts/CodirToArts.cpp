///==========================================================================///
/// File: CodirToArts.cpp
///
/// Materializes CODIR codelets as abstract ARTS DB/EDT objects.
///==========================================================================///
#include "ArtsMaterializationUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/codir/Conversion/Passes.h"
namespace mlir::carts::codir {
#define GEN_PASS_DEF_CONVERTCODIRTOARTS
#include "carts/dialect/codir/Conversion/Passes.h.inc"
} // namespace mlir::carts::codir
namespace {

// CODIR carries committed dependency and schedule facts on the codelet. This
// conversion only restates them onto the ARTS task in ARTS form; deriving the
// EDT distribution plan (family, version, block-halo capability) from those
// committed facts is the ARTS realize-edt-distribution-plan pass.
static arts::ArtsDepPattern convertPattern(codir::CodirPattern pattern) {
  switch (pattern) {
  case codir::CodirPattern::uniform:
    return arts::ArtsDepPattern::uniform;
  case codir::CodirPattern::stencil_tiling_nd:
    return arts::ArtsDepPattern::stencil_tiling_nd;
  case codir::CodirPattern::cross_dim_stencil_3d:
    return arts::ArtsDepPattern::cross_dim_stencil_3d;
  case codir::CodirPattern::higher_order_stencil:
    return arts::ArtsDepPattern::higher_order_stencil;
  case codir::CodirPattern::wavefront_2d:
    return arts::ArtsDepPattern::wavefront_2d;
  case codir::CodirPattern::alternating_buffer_stencil:
    return arts::ArtsDepPattern::alternating_buffer_stencil;
  case codir::CodirPattern::matmul:
    return arts::ArtsDepPattern::matmul;
  case codir::CodirPattern::elementwise_pipeline:
    return arts::ArtsDepPattern::elementwise_pipeline;
  case codir::CodirPattern::reduction:
    return arts::ArtsDepPattern::reduction;
  }
  return arts::ArtsDepPattern::unknown;
}

static arts::EdtDistributionKind
convertDistributionKind(codir::CodirDistributionKind kind) {
  switch (kind) {
  case codir::CodirDistributionKind::owner_compute:
    return arts::EdtDistributionKind::block;
  case codir::CodirDistributionKind::blocked:
    return arts::EdtDistributionKind::block;
  case codir::CodirDistributionKind::cyclic:
    return arts::EdtDistributionKind::block_cyclic;
  }
  return arts::EdtDistributionKind::block;
}

static arts::ArtsPlanRepetitionStructure
convertRepetitionStructure(codir::CodirRepetitionStructure structure) {
  switch (structure) {
  case codir::CodirRepetitionStructure::none:
    return arts::ArtsPlanRepetitionStructure::none;
  case codir::CodirRepetitionStructure::pair_step:
    return arts::ArtsPlanRepetitionStructure::pair_step;
  case codir::CodirRepetitionStructure::k_step:
    return arts::ArtsPlanRepetitionStructure::k_step;
  case codir::CodirRepetitionStructure::full_timestep:
    return arts::ArtsPlanRepetitionStructure::full_timestep;
  }
  return arts::ArtsPlanRepetitionStructure::none;
}

// Mechanically restate the committed codelet plan facts onto the ARTS task.
// This forwards already-committed CODIR/SDE facts in ARTS form; it does not
// classify movement and does not derive the distribution family/version/halo
// capability (the ARTS realize-edt-distribution-plan pass owns that).
static void forwardCommittedEdtPlan(codir::CodeletOp codelet,
                                    arts::EdtOp task) {
  if (!codelet || !task)
    return;
  MLIRContext *ctx = codelet.getContext();
  Operation *taskOp = task.getOperation();
  if (auto pattern = codelet.getPatternAttr()) {
    arts::ArtsDepPattern depPattern = convertPattern(pattern.getValue());
    if (depPattern != arts::ArtsDepPattern::unknown)
      arts::setDepPattern(taskOp, depPattern);
  }
  if (auto kind = codelet.getDistributionKindAttr())
    arts::setEdtDistributionKind(taskOp,
                                 convertDistributionKind(kind.getValue()));
  if (auto topology = codelet.getIterationTopologyAttr())
    arts::setPlanIterationTopologyAttr(
        taskOp, arts::ArtsPlanIterationTopologyAttr::get(
                    ctx, static_cast<arts::ArtsPlanIterationTopology>(
                             topology.getValue())));
  if (auto repetition = codelet.getRepetitionStructureAttr())
    arts::setPlanRepetitionStructureAttr(
        taskOp, arts::ArtsPlanRepetitionStructureAttr::get(
                    ctx, convertRepetitionStructure(repetition.getValue())));
  if (auto strategy = codelet.getReductionStrategyAttr())
    task.setReductionStrategyAttr(arts::ArtsReductionStrategyAttr::get(
        ctx, static_cast<arts::ArtsReductionStrategy>(strategy.getValue())));
  if (codelet.getPartialReductionAttr())
    task.setPartialReductionAttr(UnitAttr::get(ctx));
  if (auto dims = codelet.getPartialReductionDimsAttr())
    task.setPartialReductionDimsAttr(dims);
  if (auto ownerDims = codelet.getPartialReductionOwnerDimsAttr())
    task.setPartialReductionOwnerDimsAttr(ownerDims);
  if (auto depMaps = codelet.getPartialReductionDepResultDimMapsAttr())
    task.setPartialReductionDepResultDimMapsAttr(depMaps);
  if (codelet.getPartialReductionSplitRequiredAttr())
    task.setPartialReductionSplitRequiredAttr(UnitAttr::get(ctx));
  if (auto splitDims = codelet.getPartialReductionSplitDimsAttr())
    task.setPartialReductionSplitDimsAttr(splitDims);
  if (auto splitFactor = codelet.getPartialReductionSplitFactorAttr())
    task.setPartialReductionSplitFactorAttr(splitFactor);
  if (auto ownerTaskCount =
          codelet.getPartialReductionSplitOwnerTaskCountAttr())
    task.setPartialReductionSplitOwnerTaskCountAttr(ownerTaskCount);
  if (auto targetWorkerCount =
          codelet.getPartialReductionSplitTargetWorkerCountAttr())
    task.setPartialReductionSplitTargetWorkerCountAttr(targetWorkerCount);
  ArrayAttr tileShape = codelet.getTileShapeAttr();
  if (tileShape) {
    if (auto tileOwnerDims = codelet.getTileOwnerDimsAttr())
      arts::setPlanOwnerDimsAttr(taskOp, tileOwnerDims);
    arts::setPlanPhysicalBlockShapeAttr(taskOp, tileShape);
  } else if (auto ownerDims = codelet.getPlanOwnerDimsAttr()) {
    arts::setPlanOwnerDimsAttr(taskOp, ownerDims);
  }
  if (auto workerSlice = codelet.getLogicalWorkerSliceAttr())
    arts::setPlanLogicalWorkerSliceAttr(taskOp, workerSlice);
  if (auto haloShape = codelet.getHaloShapeAttr())
    arts::setPlanHaloShapeAttr(taskOp, haloShape);
  if (auto minOffsets = codelet.getAccessMinOffsetsAttr())
    task->setAttr(task.getStencilMinOffsetsAttrName(), minOffsets);
  if (auto maxOffsets = codelet.getAccessMaxOffsetsAttr())
    task->setAttr(task.getStencilMaxOffsetsAttrName(), maxOffsets);
  if (auto ownerDims = getCodirStencilOwnerDimsAttr(codelet))
    task->setAttr(task.getStencilOwnerDimsAttrName(), ownerDims);
  if (auto spatialDims = codelet.getSpatialDimsAttr())
    task->setAttr(task.getStencilSpatialDimsAttrName(), spatialDims);
  if (auto writeFootprint = codelet.getWriteFootprintAttr())
    task->setAttr(task.getStencilWriteFootprintAttrName(), writeFootprint);
  if (codelet.getInPlaceSafeAttr())
    task.setInPlaceSafeAttr(UnitAttr::get(ctx));
  if (codelet.getInPlaceSharedStateAttr())
    task.setInPlaceSharedStateAttr(UnitAttr::get(ctx));
}

static LogicalResult rejectResidualSdeOps(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (!op->getDialect() || op->getDialect()->getNamespace() != "sde")
      return;
    op->emitError() << "SDE operation reached CODIR-to-ARTS; run "
                       "`materialize-sde-boundary-to-arts` before "
                       "`convert-codir-to-arts`";
    found = true;
  });
  return failure(found);
}

struct PlannedBlockDepAccessPlan {
  SmallVector<unsigned, 4> ownerDims;
  SmallVector<int64_t, 4> blockSizes;
  SmallVector<Value, 4> ownerParams;
  SmallVector<Value, 4> ownerDomainBases;
  SmallVector<int64_t, 4> groupBlockCounts;
  bool grouped = false;

  bool empty() const { return ownerDims.empty(); }
};

static std::optional<SmallVector<int64_t, 4>>
getCodirLogicalOwnerBlockCounts(codir::CodeletOp codelet, unsigned depIndex,
                                unsigned memrefRank,
                                ArrayRef<int64_t> blockSizes) {
  std::optional<SmallVector<unsigned, 4>> ownerDims =
      getCodirDepOwnerDims(codelet, depIndex);
  if (!ownerDims || ownerDims->empty() ||
      ownerDims->size() != blockSizes.size())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> logicalSlice =
      readI64ArrayAttr(codelet.getLogicalWorkerSliceAttr());
  SmallVector<int64_t, 4> groupBlocks;
  groupBlocks.reserve(ownerDims->size());
  for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
    int64_t blockSize = blockSizes[slot];
    if (blockSize <= 0)
      return std::nullopt;
    int64_t logicalExtent = blockSize;
    if (logicalSlice && logicalSlice->size() == memrefRank) {
      if (ownerDim >= logicalSlice->size())
        return std::nullopt;
      logicalExtent = (*logicalSlice)[ownerDim];
    } else if (logicalSlice && logicalSlice->size() == ownerDims->size()) {
      logicalExtent = (*logicalSlice)[slot];
    }
    if (logicalExtent <= 0)
      return std::nullopt;
    logicalExtent = std::max<int64_t>(logicalExtent, blockSize);
    groupBlocks.push_back(llvm::divideCeil(logicalExtent, blockSize));
  }
  return groupBlocks;
}

struct ConvertCodirToArtsPass
    : public codir::impl::ConvertCodirToArtsBase<ConvertCodirToArtsPass> {
  llvm::SmallDenseSet<Operation *, 16> loopCompletionBarriers;

  LogicalResult requireOnePlanningEntryPerDependency(codir::CodeletOp codelet,
                                                     ArrayAttr attr,
                                                     StringRef attrName) {
    if (codelet.getDeps().size() == (attr ? attr.size() : 0))
      return success();
    return codelet.emitOpError()
           << "requires one " << attrName
           << " entry per dependency before CODIR-to-ARTS materialization";
  }

  LogicalResult requireDepModes(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepModesAttrName();
    ArrayAttr modes = codelet.getDepModesAttr();
    if (failed(requireOnePlanningEntryPerDependency(codelet, modes, attrName)))
      return failure();
    if (!modes)
      return success();
    for (auto [index, attr] : llvm::enumerate(modes)) {
      if (isa<codir::CodirAccessModeAttr>(attr))
        continue;
      return codelet.emitOpError()
             << attrName << " entry #" << index
             << " must be a CODIR access_mode attribute, got " << attr;
    }
    return success();
  }

  LogicalResult requireDepStorageViews(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepStorageViewsAttrName();
    ArrayAttr storageViews = codelet.getDepStorageViewsAttr();
    if (failed(requireOnePlanningEntryPerDependency(codelet, storageViews,
                                                    attrName)))
      return failure();
    if (!storageViews)
      return success();
    for (auto [index, attr] : llvm::enumerate(storageViews)) {
      if (isa<codir::CodirStorageViewKindAttr>(attr))
        continue;
      return codelet.emitOpError()
             << attrName << " entry #" << index
             << " must be a CODIR storage_view attribute, got " << attr;
    }
    return success();
  }

  LogicalResult requireDepOwnerDims(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepOwnerDimsAttrName();
    ArrayAttr ownerDims = codelet.getDepOwnerDimsAttr();
    if (failed(
            requireOnePlanningEntryPerDependency(codelet, ownerDims, attrName)))
      return failure();
    if (!ownerDims)
      return success();
    for (auto [index, attr] : llvm::enumerate(ownerDims)) {
      auto dims = dyn_cast<ArrayAttr>(attr);
      if (!dims)
        return codelet.emitOpError()
               << attrName << " entry #" << index
               << " must be an array attribute, got " << attr;
      for (Attribute dim : dims)
        if (!isa<IntegerAttr>(dim))
          return codelet.emitOpError()
                 << attrName << " entry #" << index
                 << " must contain integer attributes, got " << dim;
    }
    return success();
  }

  LogicalResult requireDepCollectives(codir::CodeletOp codelet) {
    StringRef attrName = codelet.getDepCollectivesAttrName();
    ArrayAttr collectives = codelet.getDepCollectivesAttr();
    if (failed(requireOnePlanningEntryPerDependency(codelet, collectives,
                                                    attrName)))
      return failure();
    if (!collectives)
      return success();
    for (auto [index, attr] : llvm::enumerate(collectives)) {
      if (isa<codir::CodirCollectiveKindAttr>(attr))
        continue;
      return codelet.emitOpError()
             << attrName << " entry #" << index
             << " must be a CODIR collective attribute, got " << attr;
    }
    return success();
  }

  LogicalResult requireFinalizedPlanningFacts(codir::CodeletOp codelet) {
    if (failed(requireDepModes(codelet)))
      return failure();
    if (failed(requireDepStorageViews(codelet)))
      return failure();
    if (failed(requireDepOwnerDims(codelet)))
      return failure();
    if (failed(requireDepCollectives(codelet)))
      return failure();
    return success();
  }

  bool isRankExpandedOwnerStripReadOnlyHaloDep(codir::CodeletOp codelet,
                                               unsigned depIdx) {
    auto topology = codelet.getIterationTopology();
    if (!topology || *topology != codir::CodirIterationTopology::owner_strip)
      return false;
    if (!codirDepRequiresComputeBlockStorage(codelet, depIdx))
      return false;
    if (getFinalizedCodirDepCollectiveKind(codelet, depIdx) !=
        codir::CodirCollectiveKind::halo)
      return false;
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIdx);
    if (!mode || !codirAccessMayRead(*mode) || codirAccessMayWrite(*mode))
      return false;
    std::optional<SmallVector<unsigned, 4>> depOwnerDims =
        getCodirDepOwnerDims(codelet, depIdx);
    std::optional<SmallVector<int64_t, 4>> tileOwnerDims =
        readI64ArrayAttr(codelet.getTileOwnerDimsAttr());
    if (!depOwnerDims || !tileOwnerDims || depOwnerDims->empty() ||
        depOwnerDims->size() != tileOwnerDims->size())
      return false;
    for (auto [slot, depOwnerDim] : llvm::enumerate(*depOwnerDims)) {
      int64_t tileOwnerDim = (*tileOwnerDims)[slot];
      if (tileOwnerDim < 0 ||
          static_cast<unsigned>(tileOwnerDim) != depOwnerDim)
        return true;
    }
    return false;
  }

  // Movement that derives from committed structure must have the structure it
  // needs. A committed halo collective on a compute-block dep is realized from
  // access-window halo facts; fail closed when those facts are missing instead
  // of silently degrading to coarse storage.
  LogicalResult requireMaterializableMovement(codir::CodeletOp codelet) {
    for (unsigned depIdx = 0, e = codelet.getDeps().size(); depIdx < e;
         ++depIdx) {
      if (codirDepRequiresComputeBlockStorage(codelet, depIdx) &&
          getFinalizedCodirDepCollectiveKind(codelet, depIdx) ==
              codir::CodirCollectiveKind::halo &&
          !codirDepHasHaloWindow(codelet, depIdx))
        return codelet.emitOpError()
               << "dependency #" << depIdx
               << " commits a halo collective but has no access-window halo "
                  "facts to materialize block-native halo storage";
      if (isRankExpandedOwnerStripReadOnlyHaloDep(codelet, depIdx))
        return codelet.emitOpError()
               << "dependency #" << depIdx
               << " commits a rank-expanded owner-strip read-only halo; "
                  "CODIR/ARTS owner-strip RO halo materialization is not yet "
                  "implemented, so this path fails closed instead of "
                  "materializing a partial halo";
    }
    return success();
  }

  bool hasGenericWorkerPlan(codir::CodeletOp codelet) const {
    if (!codelet)
      return false;
    return codelet.getDistributionKindAttr() ||
           codelet.getIterationTopologyAttr() ||
           codelet.getLogicalWorkerSliceAttr() || codelet.getTileShapeAttr();
  }

  bool isSmallReadOnlyCoarseDep(codir::CodeletOp codelet, unsigned depIndex,
                                arts::DbAllocOp alloc) const {
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIndex);
    return mode && *mode == codir::CodirAccessMode::read &&
           arts::DbUtils::isSmallCoarseUserDataDb(alloc);
  }

  bool isReplicatedReadDep(codir::CodeletOp codelet, unsigned depIndex) const {
    std::optional<codir::CodirAccessMode> mode =
        getCodirDepAccessMode(codelet, depIndex);
    std::optional<codir::CodirStorageViewKind> view =
        getCodirDepStorageViewKind(codelet, depIndex);
    return mode && *mode == codir::CodirAccessMode::read && view &&
           *view == codir::CodirStorageViewKind::replicated_read;
  }

  bool hasDistributedLaunchStoragePlan(codir::CodeletOp codelet) const {
    if (!hasGenericWorkerPlan(codelet))
      return false;
    if (codelet.getDeps().empty())
      return true;
    if (!hasCodirTileOwnerSlicePlan(codelet))
      return false;

    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (isSmallReadOnlyCoarseDep(codelet, static_cast<unsigned>(idx), alloc))
        continue;
      if (isReplicatedReadDep(codelet, static_cast<unsigned>(idx)))
        continue;
      if (!codirDepAllowsComputeBlockStorage(codelet,
                                             static_cast<unsigned>(idx)))
        return false;
      if (!canUseCodirOwnerSliceForAlloc(codelet, static_cast<unsigned>(idx),
                                         alloc))
        return false;
      if (!codirDepCanUseBlockStorageAccess(codelet,
                                            static_cast<unsigned>(idx)))
        return false;
    }
    return true;
  }

  scf::ForOp findGenericWorkerDispatchLoop(codir::CodeletOp codelet) const {
    scf::ForOp nearestLoop;
    for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (!loop)
        continue;
      if (!nearestLoop)
        nearestLoop = loop;
      if (containsValue(codelet.getParams(), loop.getInductionVar()))
        return loop;
    }
    // Flattened owner-tile dispatch rematerializes owner bases from a block
    // ordinal, so the dispatch IV is not a codelet parameter. It is still the
    // ARTS launch ordinal for the committed tile-owner plan.
    if (hasCodirTileOwnerSlicePlan(codelet))
      return nearestLoop;
    return {};
  }

  Operation *getCompletionBarrierAnchor(codir::CodeletOp codelet,
                                        arts::EdtOp task) {
    Operation *nearestLoop = nullptr;
    Operation *dispatchAnchor = nullptr;
    bool matchedDispatchLoop = false;

    for (Operation *parent = task->getParentOp(); parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (!loop) {
        if (matchedDispatchLoop)
          break;
        continue;
      }

      if (!nearestLoop)
        nearestLoop = parent;

      if (containsValue(codelet.getParams(), loop.getInductionVar())) {
        dispatchAnchor = parent;
        matchedDispatchLoop = true;
        continue;
      }

      if (matchedDispatchLoop)
        break;
    }

    if (dispatchAnchor)
      return dispatchAnchor;
    if (nearestLoop)
      return nearestLoop;
    return task.getOperation();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    if (failed(rejectResidualSdeOps(module))) {
      signalPassFailure();
      return;
    }

    SmallVector<codir::CodeletOp> codelets;
    module.walk([&](codir::CodeletOp op) { codelets.push_back(op); });
    for (codir::CodeletOp codelet : codelets) {
      if (failed(requireFinalizedPlanningFacts(codelet)) ||
          failed(requireMaterializableMovement(codelet))) {
        signalPassFailure();
        return;
      }
    }

    for (codir::CodeletOp codelet : codelets) {
      for (auto [depIndex, dep] : llvm::enumerate(codelet.getDeps())) {
        unsigned depIdx = static_cast<unsigned>(depIndex);
        if (findBackingDbAlloc(dep)) {
          if (failed(
                  materializeExistingDbComputeBlockIfNeeded(codelet, depIdx))) {
            codelet.emitOpError()
                << "dependency #" << depIdx
                << " requests compute-block storage but cannot be materialized "
                   "from its existing DB view";
            signalPassFailure();
            return;
          }
          if (failed(
                  materializeExistingDbHostBridgeIfNeeded(codelet, depIdx))) {
            codelet.emitOpError()
                << "dependency #" << depIdx
                << " requests compute-block storage but cannot be bridged "
                   "from its host-whole DB view";
            signalPassFailure();
            return;
          }
          continue;
        }
        if (failed(materializeRawCodirDependency(dep, codelet, depIdx))) {
          codelet.emitOpError()
              << "dependency is not backed by SDE/CODIR DB materialization "
                 "and cannot be materialized from a local memref allocation";
          signalPassFailure();
          return;
        }
      }
    }

    for (codir::CodeletOp codelet : codelets) {
      if (failed(lowerCodelet(codelet))) {
        signalPassFailure();
        return;
      }
    }
  }

  LogicalResult lowerCodelet(codir::CodeletOp codelet) {
    Location loc = codelet.getLoc();
    OpBuilder builder(codelet);

    ArrayAttr depModes = codelet.getDepModesAttr();
    if (failed(requireFinalizedPlanningFacts(codelet)))
      return failure();

    SmallVector<Value> taskDeps;
    SmallVector<Type> blockArgTypes;
    SmallVector<CodirDepSlice, 4> depSlices;
    SmallVector<PlannedBlockDepAccessPlan, 4> plannedBlockAccessPlans;
    SmallVector<Operation *, 4> depViewCleanup;
    taskDeps.reserve(codelet.getDeps().size());
    blockArgTypes.reserve(codelet.getDeps().size());
    depSlices.reserve(codelet.getDeps().size());
    plannedBlockAccessPlans.reserve(codelet.getDeps().size());

    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      if (isCodirViewDep(dep))
        depViewCleanup.push_back(dep.getDefiningOp());

      auto modeAttr = cast<codir::CodirAccessModeAttr>(depModes[idx]);
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (!alloc)
        return codelet.emitOpError()
               << "dependency #" << idx
               << " is not backed by SDE/CODIR DB materialization";
      unsigned depIdx = static_cast<unsigned>(idx);

      CodirDepSlice slice = getCodirDepSlice(dep, builder, loc);
      std::optional<arts::PartitionMode> partitionMode;
      SmallVector<Value> partitionOffsets;
      SmallVector<Value> partitionSizes;
      if (slice.sliced) {
        partitionMode = arts::PartitionMode::block;
        partitionOffsets.assign(slice.offsets.begin(), slice.offsets.end());
        partitionSizes.assign(slice.sizes.begin(), slice.sizes.end());
      }

      Value zero = createZeroIndex(builder, loc);
      SmallVector<Value> dbOffsets(alloc.getSizes().size(), zero);
      SmallVector<Value> dbSizes(alloc.getSizes().begin(),
                                 alloc.getSizes().end());
      if (dbOffsets.empty()) {
        dbOffsets.push_back(zero);
        dbSizes.push_back(createOneIndex(builder, loc));
      }
      PlannedBlockDepAccessPlan plannedAccess;
      if (codirDepAllowsComputeBlockStorage(codelet, depIdx) &&
          canUseCodirOwnerSliceForAlloc(codelet, depIdx, alloc) &&
          codirDepCanUseBlockStorageAccess(codelet, depIdx) &&
          !codelet.getParams().empty()) {
        std::optional<SmallVector<unsigned, 4>> ownerDims =
            getCodirDepOwnerDims(codelet, depIdx);
        std::optional<SmallVector<int64_t, 4>> blockSizes =
            getCodirTileOwnerBlockSizes(
                codelet, depIdx,
                static_cast<unsigned>(alloc.getElementSizes().size()));
        SmallVector<Value, 4> ownerParams =
            getCodirDepOwnerParamValues(codelet, depIdx);
        if (ownerDims && blockSizes &&
            ownerDims->size() == blockSizes->size() &&
            ownerParams.size() == ownerDims->size() &&
            alloc.getSizes().size() == ownerDims->size()) {
          std::optional<SmallVector<int64_t, 4>> groupBlockCounts =
              getCodirLogicalOwnerBlockCounts(
                  codelet, depIdx,
                  static_cast<unsigned>(alloc.getElementSizes().size()),
                  *blockSizes);
          if (!groupBlockCounts ||
              groupBlockCounts->size() != ownerDims->size())
            return codelet.emitOpError()
                   << "failed to derive logical block window for dependency #"
                   << depIdx;

          bool hasHaloWindow = false;
          for (unsigned ownerDim : *ownerDims) {
            CodirOwnerHaloWindow halo = getCodirBlockStorageHaloWindowForDim(
                codelet, depIdx, ownerDim,
                static_cast<unsigned>(alloc.getElementSizes().size()));
            hasHaloWindow |= !halo.empty();
          }
          bool grouped = llvm::any_of(*groupBlockCounts,
                                      [](int64_t count) { return count > 1; });
          // Grouped halo-backed deps are only allowed when the body remains
          // owner-slice rooted. Concrete index bounds are still proven by the
          // block-local rewrite before any db_ref is materialized.
          if (grouped && hasHaloWindow &&
              !codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIdx))
            return codelet.emitOpError()
                   << "grouped compute over halo block dependency #" << depIdx
                   << " requires lane-specific halo acquire materialization";

          dbOffsets.clear();
          dbSizes.clear();
          for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
            Value blockSizeValue =
                createConstantIndex(builder, loc, (*blockSizes)[slot]);
            Value base = ownerParams[slot];
            Value domainBase =
                materializeCodirOwnerDomainBase(builder, loc, codelet, base);
            Value relativeBase =
                ::mlir::carts::ValueAnalysis::sameValue(base, domainBase)
                    ? createZeroIndex(builder, loc)
                    : arith::SubIOp::create(builder, loc, base, domainBase)
                          .getResult();
            Value blockIndex = arith::DivUIOp::create(
                builder, loc, relativeBase, blockSizeValue);
            dbOffsets.push_back(blockIndex);
            int64_t groupBlockCount = (*groupBlockCounts)[slot];
            if (groupBlockCount <= 1) {
              dbSizes.push_back(createOneIndex(builder, loc));
            } else {
              Value requestedBlocks =
                  createConstantIndex(builder, loc, groupBlockCount);
              Value remainingBlocks = arith::SubIOp::create(
                  builder, loc, alloc.getSizes()[slot], blockIndex);
              dbSizes.push_back(arith::MinUIOp::create(
                  builder, loc, remainingBlocks, requestedBlocks));
            }
            plannedAccess.ownerDims.push_back(ownerDim);
            plannedAccess.blockSizes.push_back((*blockSizes)[slot]);
            plannedAccess.ownerParams.push_back(ownerParams[slot]);
            plannedAccess.ownerDomainBases.push_back(domainBase);
            plannedAccess.groupBlockCounts.push_back(groupBlockCount);
          }
          plannedAccess.grouped = grouped;
        }
      }
      auto acquire = arts::DbAcquireOp::create(
          builder, loc, convertAccessMode(modeAttr.getValue()), alloc.getGuid(),
          alloc.getPtr(), partitionMode,
          /*indices=*/SmallVector<Value>{}, std::move(dbOffsets),
          std::move(dbSizes),
          /*partitionIndices=*/SmallVector<Value>{},
          std::move(partitionOffsets), std::move(partitionSizes),
          /*boundsValid=*/Value{},
          /*elementOffsets=*/SmallVector<Value>{},
          /*elementSizes=*/SmallVector<Value>{});
      acquire.setPreserveAccessMode();
      if (isReplicatedReadDep(codelet, depIdx))
        acquire.setReplicatedReadAttr(UnitAttr::get(codelet.getContext()));
      taskDeps.push_back(acquire.getPtr());
      blockArgTypes.push_back(acquire.getPtr().getType());
      depSlices.push_back(std::move(slice));
      plannedBlockAccessPlans.push_back(std::move(plannedAccess));
    }

    SmallVector<Value> taskParams(codelet.getParams().begin(),
                                  codelet.getParams().end());
    SmallVector<Value> codeletDeps(codelet.getDeps().begin(),
                                   codelet.getDeps().end());
    appendDynamicCodirDepSliceParams(codeletDeps, taskParams);
    for (Value dep : codelet.getDeps()) {
      arts::DbAllocOp alloc = findBackingDbAlloc(dep);
      if (!alloc)
        continue;
      for (Value elementSize : alloc.getElementSizes()) {
        if (!isCodirScalarParamType(elementSize.getType()) ||
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(elementSize) ||
            containsValue(taskParams, elementSize))
          continue;
        taskParams.push_back(elementSize);
      }
    }
    for (const PlannedBlockDepAccessPlan &accessPlan :
         plannedBlockAccessPlans) {
      for (Value domainBase : accessPlan.ownerDomainBases) {
        if (!domainBase || !isCodirScalarParamType(domainBase.getType()) ||
            ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(domainBase) ||
            containsValue(taskParams, domainBase))
          continue;
        taskParams.push_back(domainBase);
      }
    }
    // CODIR carries only generic worker-plan facts. The ARTS boundary is the
    // first place where runtime topology can turn that plan into inter-node
    // EDT placement and routing.
    arts::ArtsLaunchPolicy launch = arts::resolveArtsLaunchPolicy(
        codelet->getParentOfType<ModuleOp>(),
        findGenericWorkerDispatchLoop(codelet),
        hasDistributedLaunchStoragePlan(codelet), builder, loc);
    auto task =
        launch.route
            ? arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  launch.concurrency, launch.route, taskDeps,
                                  taskParams)
            : arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  launch.concurrency, taskDeps, taskParams);
    forwardCommittedEdtPlan(codelet, task);
    bool isTaskDepend = static_cast<bool>(codelet.getTaskDependAttr());
    bool requiresOrderedDependBarrier =
        static_cast<bool>(codelet.getOrderedTaskDependAttr());
    bool requiresCompletionBarrier =
        static_cast<bool>(codelet.getCompletionBarrierAttr());
    Block &taskBlock = task.getBody().front();
    for (Type type : blockArgTypes)
      taskBlock.addArgument(type, loc);
    DenseMap<Value, Value> paramBlockArgs;
    DenseMap<Value, Value> sourceByBlockArgument;
    for (auto [idx, param] : llvm::enumerate(taskParams)) {
      Value arg = taskBlock.addArgument(param.getType(), loc);
      paramBlockArgs.try_emplace(param, arg);
      sourceByBlockArgument.try_emplace(arg, param);
    }

    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(&taskBlock);

    IRMapping mapper;
    Block &codeletBlock = codelet.getBody().front();
    unsigned numDeps = codelet.getDeps().size();
    SmallVector<PlannedBlockLocalAccessRewrite, 4> localAccessRewrites;
    for (unsigned idx = 0; idx < numDeps; ++idx) {
      Value payload =
          materializeInnerPayload(builder, loc, taskBlock.getArgument(idx));
      const CodirDepSlice &slice = depSlices[idx];
      if (slice.sliced) {
        auto depType = cast<MemRefType>(codelet.getDeps()[idx].getType());
        if (slice.subindex) {
          Value subindex = slice.subindexIndex;
          auto it = paramBlockArgs.find(subindex);
          if (it != paramBlockArgs.end())
            subindex = it->second;
          else if (std::optional<int64_t> constant =
                       ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                           subindex))
            subindex = createConstantIndex(builder, loc, *constant);
          payload = polygeist::SubIndexOp::create(builder, loc, depType,
                                                  payload, subindex);
        } else {
          SmallVector<OpFoldResult> offsets = remapIndexFoldResults(
              builder, loc, slice.mixedOffsets, paramBlockArgs);
          SmallVector<OpFoldResult> sizes = remapIndexFoldResults(
              builder, loc, slice.mixedSizes, paramBlockArgs);
          SmallVector<OpFoldResult> strides = remapIndexFoldResults(
              builder, loc, slice.mixedStrides, paramBlockArgs);
          auto resultType = memref::SubViewOp::inferResultType(
              cast<MemRefType>(payload.getType()), offsets, sizes, strides);
          payload = memref::SubViewOp::create(builder, loc, resultType, payload,
                                              offsets, sizes, strides);
        }
      } else if (!plannedBlockAccessPlans[idx].empty()) {
        auto payloadType = dyn_cast<MemRefType>(payload.getType());
        if (!payloadType)
          return codelet.emitOpError()
                 << "planned block-local dependency payload is not a memref";
        const PlannedBlockDepAccessPlan &accessPlan =
            plannedBlockAccessPlans[idx];
        if (accessPlan.ownerParams.size() != accessPlan.ownerDims.size() ||
            accessPlan.blockSizes.size() != accessPlan.ownerDims.size() ||
            accessPlan.ownerDomainBases.size() != accessPlan.ownerDims.size() ||
            accessPlan.groupBlockCounts.size() != accessPlan.ownerDims.size())
          return codelet.emitOpError()
                 << "failed to materialize owner-base parameters for planned "
                    "block-local access rewrite";
        arts::DbAllocOp blockAlloc = findBackingDbAlloc(codelet.getDeps()[idx]);
        for (auto [slot, ownerDim] : llvm::enumerate(accessPlan.ownerDims)) {
          Value ownerBase = paramBlockArgs.lookup(accessPlan.ownerParams[slot]);
          if (!ownerBase)
            return codelet.emitOpError()
                   << "failed to materialize owner-base parameter for planned "
                      "block-local access rewrite";
          Value ownerDomainBase = accessPlan.ownerDomainBases[slot];
          if (std::optional<int64_t> folded =
                  ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(
                      ownerDomainBase)) {
            ownerDomainBase = createConstantIndex(builder, loc, *folded);
          } else if (Value mappedDomainBase =
                         paramBlockArgs.lookup(ownerDomainBase)) {
            ownerDomainBase = mappedDomainBase;
          } else {
            return codelet.emitOpError()
                   << "failed to materialize owner-domain base parameter for "
                      "planned block-local access rewrite";
          }
          Value localOrigin = materializeBlockLocalOrigin(
              builder, loc, ownerBase, ownerDomainBase,
              accessPlan.blockSizes[slot]);
          CodirOwnerHaloWindow ownerHalo = getCodirBlockStorageHaloWindowForDim(
              codelet, idx, ownerDim,
              static_cast<unsigned>(payloadType.getRank()));
          // Keep producer stores aligned with the DB's storage halo.
          if (ownerHalo.lower <= 0) {
            CodirOwnerHaloWindow allocHalo =
                blockAllocStorageHaloForDim(blockAlloc, ownerDim);
            if (allocHalo.lower > 0)
              ownerHalo = allocHalo;
          }
          localAccessRewrites.push_back(
              {payload, ownerDim, ownerBase, localOrigin, ownerHalo.lower,
               taskBlock.getArgument(idx), static_cast<unsigned>(slot),
               accessPlan.blockSizes[slot], accessPlan.groupBlockCounts[slot],
               accessPlan.grouped});
        }
      }
      mapper.map(codeletBlock.getArgument(idx), payload);
    }

    for (auto [idx, param] : llvm::enumerate(codelet.getParams()))
      mapper.map(codeletBlock.getArgument(numDeps + idx),
                 taskBlock.getArgument(numDeps + idx));
    for (Operation &nested : codeletBlock.without_terminator())
      builder.insert(nested.clone(mapper));

    if (shouldLowerReductionsToAtomics(codelet))
      lowerIntegerAddReductionsToAtomics(task.getBody(), sourceByBlockArgument);

    if (failed(rewritePlannedBlockLocalAccesses(task, localAccessRewrites)))
      return codelet.emitOpError()
             << "failed to rewrite planned block dependency accesses to "
                "block-local indices";

    arts::YieldOp::create(builder, loc);

    Operation *barrierAnchor = nullptr;
    if (requiresOrderedDependBarrier) {
      OpBuilder barrierBuilder(task);
      barrierBuilder.setInsertionPointAfter(task);
      auto reason = arts::ArtsBarrierReasonAttr::get(
          codelet.getContext(), arts::ArtsBarrierReason::required_memory);
      arts::BarrierOp::create(barrierBuilder, loc, reason);
    } else if (isTaskDepend || requiresCompletionBarrier) {
      barrierAnchor = getCompletionBarrierAnchor(codelet, task);

      if (barrierAnchor == task.getOperation() ||
          loopCompletionBarriers.insert(barrierAnchor).second) {
        OpBuilder barrierBuilder(barrierAnchor);
        barrierBuilder.setInsertionPointAfter(barrierAnchor);
        auto reason = arts::ArtsBarrierReasonAttr::get(
            codelet.getContext(), arts::ArtsBarrierReason::required_memory);
        arts::BarrierOp::create(barrierBuilder, loc, reason);
      }
    }

    codelet.erase();
    for (Operation *view : depViewCleanup)
      if (view && view->use_empty())
        view->erase();
    return success();
  }
};
} // namespace
std::unique_ptr<Pass> mlir::carts::codir::createConvertCodirToArtsPass() {
  return std::make_unique<ConvertCodirToArtsPass>();
}
