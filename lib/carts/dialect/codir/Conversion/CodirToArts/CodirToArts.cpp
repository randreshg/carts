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
      if (!codirDepAccessesStayWithinSingleOwnerSlice(
              codelet, static_cast<unsigned>(idx)))
        return false;
    }
    return true;
  }

  scf::ForOp findGenericWorkerDispatchLoop(codir::CodeletOp codelet) const {
    for (Operation *parent = codelet ? codelet->getParentOp() : nullptr; parent;
         parent = parent->getParentOp()) {
      auto loop = dyn_cast<scf::ForOp>(parent);
      if (loop && containsValue(codelet.getParams(), loop.getInductionVar()))
        return loop;
    }
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
      if (failed(requireFinalizedPlanningFacts(codelet))) {
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
    SmallVector<SmallVector<unsigned, 4>, 4> plannedBlockOwnerDims;
    SmallVector<Operation *, 4> depViewCleanup;
    taskDeps.reserve(codelet.getDeps().size());
    blockArgTypes.reserve(codelet.getDeps().size());
    depSlices.reserve(codelet.getDeps().size());
    plannedBlockOwnerDims.reserve(codelet.getDeps().size());

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
      SmallVector<unsigned, 4> plannedBlockOwnerDimsForDep;
      if (codirDepAllowsComputeBlockStorage(codelet, depIdx) &&
          canUseCodirOwnerSliceForAlloc(codelet, depIdx, alloc) &&
          codirDepAccessesStayWithinSingleOwnerSlice(codelet, depIdx) &&
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
            dbSizes.push_back(createOneIndex(builder, loc));
            plannedBlockOwnerDimsForDep.push_back(ownerDim);
          }
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
      if (isReplicatedReadDep(codelet, depIdx))
        acquire.setReplicatedReadAttr(UnitAttr::get(codelet.getContext()));
      taskDeps.push_back(acquire.getPtr());
      blockArgTypes.push_back(acquire.getPtr().getType());
      depSlices.push_back(std::move(slice));
      plannedBlockOwnerDims.push_back(std::move(plannedBlockOwnerDimsForDep));
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
    propagateCodirPlanToArts(codelet, task);
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
      } else if (!plannedBlockOwnerDims[idx].empty()) {
        auto payloadType = dyn_cast<MemRefType>(payload.getType());
        if (!payloadType)
          return codelet.emitOpError()
                 << "planned block-local dependency payload is not a memref";
        SmallVector<Value, 4> ownerParams =
            getCodirDepOwnerParamValues(codelet, idx);
        if (ownerParams.size() != plannedBlockOwnerDims[idx].size())
          return codelet.emitOpError()
                 << "failed to materialize owner-base parameters for planned "
                    "block-local access rewrite";
        for (auto [slot, ownerDim] :
             llvm::enumerate(plannedBlockOwnerDims[idx])) {
          Value ownerBase = paramBlockArgs.lookup(ownerParams[slot]);
          if (!ownerBase)
            return codelet.emitOpError()
                   << "failed to materialize owner-base parameter for planned "
                      "block-local access rewrite";
          CodirOwnerHaloWindow ownerHalo = getCodirBlockStorageHaloWindowForDim(
              codelet, idx, ownerDim,
              static_cast<unsigned>(payloadType.getRank()));
          localAccessRewrites.push_back(
              {payload, ownerDim, ownerBase, ownerHalo.lower});
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
