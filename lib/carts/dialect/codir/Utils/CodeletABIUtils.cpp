#include "carts/dialect/codir/Utils/CodeletABIUtils.h"

#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"

namespace mlir::carts::codir {

namespace {
inline bool codirAccessMayRead(CodirAccessMode mode) {
  return mode == CodirAccessMode::read || mode == CodirAccessMode::readwrite;
}
inline bool codirAccessMayWrite(CodirAccessMode mode) {
  return mode == CodirAccessMode::write || mode == CodirAccessMode::readwrite;
}
} // namespace

bool isCodirDependencyType(Type type) {
  return isa<MemRefType, UnrankedMemRefType>(type);
}

bool isCodirScalarParamType(Type type) { return type.isIntOrIndexOrFloat(); }

bool isMemrefForwardingOp(Operation *op) {
  if (!op || op->getNumRegions() != 0)
    return false;
  return llvm::any_of(op->getResults(), [](Value result) {
    return isa<MemRefType>(result.getType());
  });
}

std::optional<CodirAccessMode> getDepAccessMode(CodeletOp codelet,
                                                unsigned depIndex) {
  ArrayAttr modes = codelet ? codelet.getDepModesAttr() : ArrayAttr{};
  if (!modes || depIndex >= modes.size())
    return std::nullopt;
  auto mode = dyn_cast<CodirAccessModeAttr>(modes[depIndex]);
  if (!mode)
    return std::nullopt;
  return mode.getValue();
}

std::optional<CodirStorageViewKind> getDepStorageViewKind(CodeletOp codelet,
                                                          unsigned depIndex) {
  ArrayAttr views = codelet ? codelet.getDepStorageViewsAttr() : ArrayAttr{};
  if (!views || depIndex >= views.size())
    return std::nullopt;
  auto view = dyn_cast<CodirStorageViewKindAttr>(views[depIndex]);
  if (!view)
    return std::nullopt;
  return view.getValue();
}

bool coarseBridgeTargetHasReplicatedReadConsumer(CodeletOp producer,
                                                 unsigned depIndex) {
  if (!producer || depIndex >= producer.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      producer.getDeps()[depIndex]);
  if (!root)
    return false;
  Operation *scope = producer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;

  bool found = false;
  scope->walk([&](CodeletOp consumer) {
    if (found || consumer == producer)
      return;
    for (auto [idx, dep] : llvm::enumerate(consumer.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      unsigned consumerDep = static_cast<unsigned>(idx);
      std::optional<CodirAccessMode> mode =
          getDepAccessMode(consumer, consumerDep);
      if (!mode || !codirAccessMayRead(*mode))
        continue;
      std::optional<CodirStorageViewKind> view =
          getDepStorageViewKind(consumer, consumerDep);
      if (view && *view == CodirStorageViewKind::replicated_read) {
        found = true;
        return;
      }
    }
  });
  return found;
}

bool codeletIsCrossOwnerTransposeReduce(CodeletOp consumer) {
  if (!consumer || !consumer.getPartialReductionAttr())
    return false;
  ArrayAttr ownerDims = consumer.getPartialReductionOwnerDimsAttr();
  if (!ownerDims || ownerDims.empty())
    return false;
  auto isResultOwnerDim = [&](int64_t dim) {
    for (Attribute owner : ownerDims)
      if (auto intAttr = dyn_cast<IntegerAttr>(owner))
        if (intAttr.getInt() == dim)
          return true;
    return false;
  };
  ArrayAttr depMaps = consumer.getPartialReductionDepResultDimMapsAttr();
  if (!depMaps)
    return false;
  for (Attribute mapAttr : depMaps) {
    auto depMap = dyn_cast<ArrayAttr>(mapAttr);
    if (!depMap || depMap.size() != 2)
      continue;
    auto leading = dyn_cast<IntegerAttr>(depMap[0]);
    auto trailing = dyn_cast<IntegerAttr>(depMap[1]);
    if (!leading || !trailing)
      continue;
    if (leading.getInt() < 0 && isResultOwnerDim(trailing.getInt()))
      return true;
  }
  return false;
}

bool coarseBridgeTargetHasCrossOwnerReduceConsumer(CodeletOp producer,
                                                   unsigned depIndex) {
  if (!producer || depIndex >= producer.getDeps().size())
    return false;
  Value root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(
      producer.getDeps()[depIndex]);
  if (!root)
    return false;
  Operation *scope = producer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;

  bool found = false;
  scope->walk([&](CodeletOp consumer) {
    if (found || consumer == producer)
      return;
    if (!codeletIsCrossOwnerTransposeReduce(consumer))
      return;
    for (auto [idx, dep] : llvm::enumerate(consumer.getDeps())) {
      if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep) != root)
        continue;
      std::optional<CodirAccessMode> mode =
          getDepAccessMode(consumer, static_cast<unsigned>(idx));
      if (mode && codirAccessMayRead(*mode)) {
        found = true;
        return;
      }
    }
  });
  return found;
}

CodirCollectiveKind chooseCollective(CodeletOp codelet, unsigned depIndex) {
  if (!codelet || depIndex >= codelet.getDeps().size())
    return CodirCollectiveKind::none;
  // The all-gather and cross-owner reduce gates only fire for a dep the codelet
  // WRITES (the coarse copy-out producer); the materializer evaluates them on
  // write-mode participants only. Mirror that here so a read dep that happens to
  // share a root with a written one is never mislabeled.
  std::optional<CodirAccessMode> mode = getDepAccessMode(codelet, depIndex);
  if (!mode || !codirAccessMayWrite(*mode))
    return CodirCollectiveKind::none;
  if (coarseBridgeTargetHasReplicatedReadConsumer(codelet, depIndex))
    return CodirCollectiveKind::all_gather;
  if (codeletIsCrossOwnerTransposeReduce(codelet) ||
      coarseBridgeTargetHasCrossOwnerReduceConsumer(codelet, depIndex))
    return CodirCollectiveKind::reduce_scatter;
  return CodirCollectiveKind::none;
}

} // namespace mlir::carts::codir
