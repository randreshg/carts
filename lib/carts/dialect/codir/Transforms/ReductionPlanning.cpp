///==========================================================================///
/// File: ReductionPlanning.cpp
///
/// CODIR-owned partial-reduction metadata planning.
///==========================================================================///

#include "carts/dialect/codir/Transforms/Passes.h"

#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ExecutionResourceAttrs.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/SmallSet.h"

#include <cassert>
#include <limits>

namespace mlir::carts::codir {
#define GEN_PASS_DEF_REDUCTIONPLANNING
#include "carts/dialect/codir/Transforms/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

struct CodirAccessInfo {
  unsigned depIndex = 0;
  SmallVector<Value, 4> indices;
  bool isWrite = false;
};

static std::optional<codir::CodirAccessMode>
getDepMode(codir::CodeletOp codelet, unsigned depIndex) {
  ArrayAttr modes = codelet.getDepModesAttr();
  if (!modes || depIndex >= modes.size())
    return std::nullopt;
  auto modeAttr = dyn_cast<codir::CodirAccessModeAttr>(modes[depIndex]);
  if (!modeAttr)
    return std::nullopt;
  return modeAttr.getValue();
}

static bool depMayWrite(codir::CodeletOp codelet, unsigned depIndex) {
  std::optional<codir::CodirAccessMode> mode = getDepMode(codelet, depIndex);
  return mode && (*mode == codir::CodirAccessMode::write ||
                  *mode == codir::CodirAccessMode::readwrite);
}

static std::optional<unsigned> findDepArgumentIndex(codir::CodeletOp codelet,
                                                    Value memref) {
  auto blockArg = dyn_cast<BlockArgument>(memref);
  if (!blockArg || blockArg.getOwner() != &codelet.getBody().front())
    return std::nullopt;
  if (blockArg.getArgNumber() >= codelet.getDeps().size())
    return std::nullopt;
  return blockArg.getArgNumber();
}

static SmallVector<CodirAccessInfo, 8>
collectDirectDepAccesses(codir::CodeletOp codelet) {
  SmallVector<CodirAccessInfo, 8> accesses;
  if (!codelet || codelet.getBody().empty())
    return accesses;

  codelet.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      std::optional<unsigned> depIndex =
          findDepArgumentIndex(codelet, load.getMemRef());
      if (!depIndex)
        return;
      accesses.push_back({*depIndex,
                          SmallVector<Value, 4>(load.getIndices().begin(),
                                                load.getIndices().end()),
                          /*isWrite=*/false});
      return;
    }

    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      std::optional<unsigned> depIndex =
          findDepArgumentIndex(codelet, store.getMemref());
      if (!depIndex)
        return;
      accesses.push_back({*depIndex,
                          SmallVector<Value, 4>(store.getIndices().begin(),
                                                store.getIndices().end()),
                          /*isWrite=*/true});
    }
  });

  return accesses;
}

static std::optional<CodirAccessInfo>
selectResultAccess(codir::CodeletOp codelet,
                   ArrayRef<CodirAccessInfo> accesses) {
  for (const CodirAccessInfo &access : accesses) {
    if (!access.isWrite || access.indices.empty())
      continue;
    if (!depMayWrite(codelet, access.depIndex))
      continue;
    return access;
  }
  return std::nullopt;
}

static const CodirAccessInfo *
findRepresentativeAccess(unsigned depIndex,
                         ArrayRef<CodirAccessInfo> accesses) {
  const CodirAccessInfo *firstRead = nullptr;
  const CodirAccessInfo *firstWrite = nullptr;
  for (const CodirAccessInfo &access : accesses) {
    if (access.depIndex != depIndex || access.indices.empty())
      continue;
    if (access.isWrite && !firstWrite)
      firstWrite = &access;
    if (!access.isWrite && !firstRead)
      firstRead = &access;
  }
  return firstRead ? firstRead : firstWrite;
}

static int64_t findResultDim(Value depIndex, ValueRange resultIndices) {
  for (auto [resultDim, resultIndex] : llvm::enumerate(resultIndices))
    if (ValueAnalysis::areValuesEquivalent(depIndex, resultIndex))
      return static_cast<int64_t>(resultDim);
  return -1;
}

static ArrayAttr buildDepResultDimMaps(codir::CodeletOp codelet) {
  MLIRContext *ctx = codelet.getContext();
  SmallVector<CodirAccessInfo, 8> accesses = collectDirectDepAccesses(codelet);
  std::optional<CodirAccessInfo> resultAccess =
      selectResultAccess(codelet, accesses);

  SmallVector<Attribute> maps;
  maps.reserve(codelet.getDeps().size());
  for (unsigned depIndex = 0, e = codelet.getDeps().size(); depIndex < e;
       ++depIndex) {
    const CodirAccessInfo *access =
        findRepresentativeAccess(depIndex, accesses);
    if (!access || !resultAccess) {
      maps.push_back(ArrayAttr::get(ctx, {}));
      continue;
    }

    SmallVector<int64_t, 4> dimMap;
    dimMap.reserve(access->indices.size());
    for (Value index : access->indices)
      dimMap.push_back(findResultDim(index, resultAccess->indices));
    maps.push_back(buildI64ArrayAttr(ctx, dimMap));
  }
  return ArrayAttr::get(ctx, maps);
}

static std::optional<int64_t> getPlanningWorkerCapacity(ModuleOp module) {
  return mlir::carts::getLogicalTotalWorkers(module);
}

static std::optional<int64_t> multiplyPositive(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return std::nullopt;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::nullopt;
  return lhs * rhs;
}

static int64_t ceilDivPositive(int64_t lhs, int64_t rhs) {
  assert(lhs > 0 && rhs > 0 && "ceilDivPositive expects positive operands");
  return (lhs + rhs - 1) / rhs;
}

static std::optional<int64_t>
computeStaticResultOwnerTaskCount(codir::CodeletOp codelet,
                                  const CodirAccessInfo &resultAccess) {
  auto resultType =
      dyn_cast<MemRefType>(codelet.getDeps()[resultAccess.depIndex].getType());
  if (!resultType || !resultType.hasStaticShape())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(codelet.getTileOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> tileShape =
      readI64ArrayAttr(codelet.getTileShapeAttr());
  if (!ownerDims || !tileShape || ownerDims->empty() ||
      ownerDims->size() != tileShape->size())
    return std::nullopt;

  int64_t taskCount = 1;
  for (auto [idx, rawDim] : llvm::enumerate(*ownerDims)) {
    if (rawDim < 0 || static_cast<int64_t>(resultType.getRank()) <= rawDim)
      return std::nullopt;
    int64_t dimSize = resultType.getDimSize(rawDim);
    int64_t blockSize = (*tileShape)[idx];
    if (dimSize <= 0 || blockSize <= 0)
      return std::nullopt;
    std::optional<int64_t> product =
        multiplyPositive(taskCount, ceilDivPositive(dimSize, blockSize));
    if (!product)
      return std::nullopt;
    taskCount = *product;
  }
  return taskCount;
}

static std::optional<int64_t>
computeStaticReductionVolume(codir::CodeletOp codelet, ArrayAttr depMaps,
                             SmallVectorImpl<int64_t> &splitDims) {
  if (!depMaps || depMaps.size() != codelet.getDeps().size())
    return std::nullopt;

  int64_t volume = 1;
  llvm::SmallSet<int64_t, 8> seenDims;
  bool foundReductionDim = false;
  for (auto [depIndex, mapAttr] : llvm::enumerate(depMaps)) {
    std::optional<codir::CodirAccessMode> mode =
        getDepMode(codelet, static_cast<unsigned>(depIndex));
    if (!mode || *mode != codir::CodirAccessMode::read)
      continue;

    auto depType = dyn_cast<MemRefType>(codelet.getDeps()[depIndex].getType());
    auto dimMap = dyn_cast<ArrayAttr>(mapAttr);
    if (!depType || !depType.hasStaticShape() || !dimMap ||
        dimMap.size() != static_cast<size_t>(depType.getRank()))
      continue;

    for (auto [dim, attr] : llvm::enumerate(dimMap)) {
      auto mappedDim = dyn_cast<IntegerAttr>(attr);
      if (!mappedDim || mappedDim.getInt() != -1)
        continue;
      int64_t dimSize = depType.getDimSize(static_cast<unsigned>(dim));
      if (dimSize <= 1)
        continue;
      foundReductionDim = true;
      if (seenDims.insert(static_cast<int64_t>(dim)).second)
        splitDims.push_back(static_cast<int64_t>(dim));
      std::optional<int64_t> product = multiplyPositive(volume, dimSize);
      if (!product)
        return std::nullopt;
      volume = *product;
    }
  }

  if (!foundReductionDim)
    return std::nullopt;
  return volume;
}

static ArrayAttr getDeclaredPartialReductionDims(codir::CodeletOp codelet) {
  return codelet.getPartialReductionDimsAttr();
}

static LogicalResult setOrVerifyReductionPlanAttr(Operation *op,
                                                  StringRef attrName,
                                                  Attribute desired,
                                                  StringRef planName) {
  Attribute existing = op->getAttr(attrName);
  if (desired) {
    if (existing && existing != desired)
      return op->emitError()
             << "existing " << planName
             << " does not match recomputed CODIR reduction plan";
    if (!existing)
      op->setAttr(attrName, desired);
    return success();
  }

  if (existing)
    return op->emitError()
           << "existing " << planName
           << " is set but recomputed CODIR reduction plan has no value";
  return success();
}

static LogicalResult
stampPartialReductionSplitPlan(codir::CodeletOp codelet,
                               ArrayAttr depResultDimMaps,
                               std::optional<int64_t> targetWorkerCount) {
  MLIRContext *ctx = codelet.getContext();
  Attribute splitRequired;
  Attribute splitDims;
  Attribute splitFactorAttr;
  Attribute ownerTaskCountAttr;
  Attribute targetWorkerCountAttr;

  auto verifyAll = [&]() -> LogicalResult {
    if (failed(setOrVerifyReductionPlanAttr(
            codelet.getOperation(),
            codelet.getPartialReductionSplitOwnerTaskCountAttrName(),
            ownerTaskCountAttr, "partial-reduction owner task count")))
      return failure();
    if (failed(setOrVerifyReductionPlanAttr(
            codelet.getOperation(),
            codelet.getPartialReductionSplitTargetWorkerCountAttrName(),
            targetWorkerCountAttr, "partial-reduction target worker count")))
      return failure();
    if (failed(setOrVerifyReductionPlanAttr(
            codelet.getOperation(),
            codelet.getPartialReductionSplitRequiredAttrName(), splitRequired,
            "partial-reduction split-required flag")))
      return failure();
    if (failed(setOrVerifyReductionPlanAttr(
            codelet.getOperation(),
            codelet.getPartialReductionSplitDimsAttrName(), splitDims,
            "partial-reduction split dims")))
      return failure();
    if (failed(setOrVerifyReductionPlanAttr(
            codelet.getOperation(),
            codelet.getPartialReductionSplitFactorAttrName(), splitFactorAttr,
            "partial-reduction split factor")))
      return failure();
    return success();
  };

  if (!targetWorkerCount || *targetWorkerCount <= 1)
    return verifyAll();

  SmallVector<CodirAccessInfo, 8> accesses = collectDirectDepAccesses(codelet);
  std::optional<CodirAccessInfo> resultAccess =
      selectResultAccess(codelet, accesses);
  if (!resultAccess)
    return verifyAll();

  // The split requires a rank-1 per-owner partial tile. Owner-tiled
  // multi-dimensional results keep the unsplit per-owner reduction shape.
  auto resultType =
      dyn_cast<MemRefType>(codelet.getDeps()[resultAccess->depIndex].getType());
  if (!resultType || resultType.getRank() != 1)
    return verifyAll();

  std::optional<int64_t> computedOwnerTaskCount =
      computeStaticResultOwnerTaskCount(codelet, *resultAccess);
  if (!computedOwnerTaskCount || *computedOwnerTaskCount <= 0)
    return verifyAll();

  ownerTaskCountAttr =
      IntegerAttr::get(IntegerType::get(ctx, 64), *computedOwnerTaskCount);
  targetWorkerCountAttr =
      IntegerAttr::get(IntegerType::get(ctx, 64), *targetWorkerCount);

  if (*computedOwnerTaskCount >= *targetWorkerCount)
    return verifyAll();

  SmallVector<int64_t, 4> inferredSplitDims;
  std::optional<int64_t> reductionVolume = computeStaticReductionVolume(
      codelet, depResultDimMaps, inferredSplitDims);
  if (!reductionVolume || *reductionVolume <= 1)
    return verifyAll();

  int64_t requestedFactor =
      ceilDivPositive(*targetWorkerCount, *computedOwnerTaskCount);
  int64_t splitFactor = std::min<int64_t>(requestedFactor, *reductionVolume);
  if (splitFactor <= 1)
    return verifyAll();

  splitRequired = UnitAttr::get(ctx);
  if (ArrayAttr declaredDims = getDeclaredPartialReductionDims(codelet))
    splitDims = declaredDims;
  else
    splitDims = buildI64ArrayAttr(ctx, inferredSplitDims);
  splitFactorAttr = IntegerAttr::get(IntegerType::get(ctx, 64), splitFactor);
  return verifyAll();
}

struct ReductionPlanningPass
    : public codir::impl::ReductionPlanningBase<ReductionPlanningPass> {
  void runOnOperation() override {
    std::optional<int64_t> targetWorkerCount =
        getPlanningWorkerCapacity(getOperation());
    getOperation().walk([&](codir::CodeletOp codelet) {
      if (!codelet.getPartialReductionAttr())
        return;

      if (!codelet.getReductionStrategyAttr()) {
        codelet.setReductionStrategyAttr(codir::CodirReductionStrategyAttr::get(
            codelet.getContext(),
            codir::CodirReductionStrategy::local_accumulate));
      }

      ArrayAttr depMaps = buildDepResultDimMaps(codelet);
      if (failed(setOrVerifyReductionPlanAttr(
              codelet.getOperation(),
              codelet.getPartialReductionDepResultDimMapsAttrName(), depMaps,
              "partial-reduction dep/result dimension maps"))) {
        signalPassFailure();
        return;
      }
      if (failed(stampPartialReductionSplitPlan(codelet, depMaps,
                                                targetWorkerCount))) {
        signalPassFailure();
        return;
      }
    });
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::codir::createReductionPlanningPass() {
  return std::make_unique<ReductionPlanningPass>();
}
