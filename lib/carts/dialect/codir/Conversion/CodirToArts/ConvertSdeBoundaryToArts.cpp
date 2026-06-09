///==========================================================================///
/// File: ConvertSdeBoundaryToArts.cpp
///
/// Converts remaining SDE storage/control boundary ops before CODIR-to-ARTS.
///==========================================================================///
#include "CodirToArtsHostBridgeMaterialization.h"
#include "carts/dialect/codir/Conversion/Passes.h"

namespace mlir::carts::codir {
#define GEN_PASS_DEF_CONVERTSDEBOUNDARYTOARTS
#include "carts/dialect/codir/Conversion/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

static LogicalResult lowerMuData(sde::SdeMuDataOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getHandle().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref handle before SDE-to-ARTS boundary conversion";
  if (memrefType.getNumDynamicDims() != 0)
    return op.emitOpError()
           << "has dynamic dimensions but carries no dynamic size operands";

  OpBuilder builder(op);
  Value replacement;
  if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                  ValueRange{}, replacement)))
    return failure();
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  op.getHandle().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

struct MuAllocCodirStorageFact {
  codir::CodeletOp codelet;
  unsigned depIndex = 0;
};

struct MuAllocCodirStorageScan {
  std::optional<MuAllocCodirStorageFact> selected;
  std::optional<MuAllocCodirStorageFact> partitionedWithoutBlockStorage;
};

static bool sameCodirStorageFact(const MuAllocCodirStorageFact &lhs,
                                 const MuAllocCodirStorageFact &rhs) {
  return getCodirDepOwnerDimsAttr(lhs.codelet, lhs.depIndex) ==
             getCodirDepOwnerDimsAttr(rhs.codelet, rhs.depIndex) &&
         codir::getDepPhysicalBlockShapeAttr(lhs.codelet, lhs.depIndex) ==
             codir::getDepPhysicalBlockShapeAttr(rhs.codelet, rhs.depIndex);
}

static LogicalResult
requireMaterializableCodirStorageFact(codir::CodeletOp codelet,
                                      unsigned depIndex) {
  if (failed(
          requireFinalizedCodirDepOwnerDimsForMaterialization(codelet, depIndex)))
    return failure();
  if (!hasCodirTileOwnerSlicePlan(codelet))
    return codelet.emitOpError()
           << "dependency #" << depIndex
           << " commits compute-block storage without a tile owner-slice plan";
  if (!codirDepCanUseBlockStorageAccess(codelet, depIndex))
    return codelet.emitOpError()
           << "dependency #" << depIndex
           << " commits compute-block storage but cannot be materialized as a "
              "block DB";
  if (!getCodirDepOwnerDimsAttr(codelet, depIndex) ||
      !codir::getDepPhysicalBlockShapeAttr(codelet, depIndex))
    return codelet.emitOpError()
           << "dependency #" << depIndex
           << " commits compute-block storage without owner dims and physical "
              "block shape";
  return success();
}

static bool codirDepCommitsPartitionedMuFact(codir::CodeletOp codelet,
                                             unsigned depIndex) {
  if (codirDepRequiresPlannedOwnerDims(codelet, depIndex))
    return true;
  return getCodirDepOwnerDims(codelet, depIndex).has_value();
}

static LogicalResult
processMuAllocCodirDep(MuAllocCodirStorageScan &scan,
                       codir::CodeletOp codelet, unsigned depIndex) {
  if (!codirDepRequiresComputeBlockStorage(codelet, depIndex)) {
    if (!scan.partitionedWithoutBlockStorage &&
        codirDepCommitsPartitionedMuFact(codelet, depIndex))
      scan.partitionedWithoutBlockStorage =
          MuAllocCodirStorageFact{codelet, depIndex};
    return success();
  }
  if (failed(requireMaterializableCodirStorageFact(codelet, depIndex)))
    return failure();

  MuAllocCodirStorageFact candidate{codelet, depIndex};
  if (!scan.selected) {
    scan.selected = candidate;
    return success();
  }
  if (!sameCodirStorageFact(*scan.selected, candidate)) {
    codelet.emitOpError()
        << "dependency #" << depIndex
        << " commits storage facts that conflict with another dependency "
           "of the same sde.mu_alloc";
    return failure();
  }
  return success();
}

static FailureOr<MuAllocCodirStorageScan>
scanMuAllocCodirStorageFacts(sde::SdeMuAllocOp op) {
  ModuleOp module = op->getParentOfType<ModuleOp>();
  if (!module)
    return MuAllocCodirStorageScan{};

  Value root = op.getMemref();
  MuAllocCodirStorageScan scan;
  WalkResult result = module.walk([&](codir::CodeletOp codelet) {
    for (auto [idx, dep] : llvm::enumerate(codelet.getDeps())) {
      if (!::mlir::carts::ValueAnalysis::sameValue(
              ::mlir::carts::ValueAnalysis::stripMemrefViewOps(dep), root))
        continue;
      unsigned depIndex = static_cast<unsigned>(idx);
      if (failed(processMuAllocCodirDep(scan, codelet, depIndex)))
        return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  if (result.wasInterrupted())
    return failure();

  for (OpOperand &use : root.getUses()) {
    auto codelet = dyn_cast<codir::CodeletOp>(use.getOwner());
    if (!codelet || use.getOperandNumber() >= codelet.getDeps().size())
      continue;
    if (failed(processMuAllocCodirDep(
            scan, codelet, static_cast<unsigned>(use.getOperandNumber()))))
      return failure();
  }
  return scan;
}

static bool hasResidualPartitionedSdeFact(sde::SdeMuAllocOp op) {
  ModuleOp module = op->getParentOfType<ModuleOp>();
  if (!module)
    return false;

  Value root = op.getMemref();
  bool found = false;
  module.walk([&](Operation *candidate) {
    if (found)
      return WalkResult::interrupt();
    if (auto window = dyn_cast<sde::SdeMuAccessWindowOp>(candidate)) {
      if (window.getMu() == root)
        found = true;
      return found ? WalkResult::interrupt() : WalkResult::advance();
    }
    if (auto redist = dyn_cast<sde::SdeRedistOp>(candidate)) {
      if (redist.getMu() == root)
        found = true;
      return found ? WalkResult::interrupt() : WalkResult::advance();
    }
    return WalkResult::advance();
  });
  return found;
}

static void eraseDeallocUsers(Value memref) {
  SmallVector<memref::DeallocOp> deallocs;
  for (Operation *user : llvm::make_early_inc_range(memref.getUsers())) {
    auto dealloc = dyn_cast<memref::DeallocOp>(user);
    if (dealloc && dealloc.getMemref() == memref)
      deallocs.push_back(dealloc);
  }
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
}

static LogicalResult lowerMuAlloc(sde::SdeMuAllocOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref result before SDE-to-ARTS boundary conversion";

  if (hasResidualPartitionedSdeFact(op))
    return op.emitOpError()
           << "has unconsumed SDE partition or movement facts; run "
              "`convert-sde-to-codir` before `convert-sde-boundary-to-arts`";

  FailureOr<MuAllocCodirStorageScan> codirStorage =
      scanMuAllocCodirStorageFacts(op);
  if (failed(codirStorage))
    return failure();

  std::optional<MuAllocCodirStorageFact> selectedStorage =
      codirStorage->selected;
  if (!selectedStorage && codirStorage->partitionedWithoutBlockStorage) {
    const MuAllocCodirStorageFact &fact =
        *codirStorage->partitionedWithoutBlockStorage;
    return op.emitOpError()
           << "has CODIR dependency #" << fact.depIndex
           << " with committed partitioned MU facts but no compute-block "
              "storage fact; refusing coarse DB fallback";
  }

  if (selectedStorage && rawCodirDependencyNeedsHostBridge(op.getMemref())) {
    FailureOr<Value> bridged = materializeHostWholeToComputeBlockBridge(
        selectedStorage->codelet, selectedStorage->depIndex, op.getMemref());
    return failed(bridged) ? failure() : success();
  }

  OpBuilder builder(op);
  Value replacement;
  if (selectedStorage) {
    if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                    op.getDynamicSizes(), replacement,
                                    selectedStorage->codelet,
                                    selectedStorage->depIndex)))
      return failure();
  } else {
    if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                    op.getDynamicSizes(), replacement)))
      return failure();
  }
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  eraseDeallocUsers(op.getMemref());
  op.getMemref().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static LogicalResult lowerSdeResourceQuery(sde::SdeResourceQueryOp op) {
  OpBuilder builder(op);
  switch (op.getKind()) {
  case sde::SdeResourceQueryKind::logicalWorkers: {
    auto runtimeQuery = arts::RuntimeQueryOp::create(
        builder, op.getLoc(), arts::RuntimeQueryKind::totalWorkers);
    Value asIndex = arith::IndexCastOp::create(
        builder, op.getLoc(), builder.getIndexType(), runtimeQuery.getResult());
    op.getResult().replaceAllUsesWith(asIndex);
    op.erase();
    return success();
  }
  }
  return op.emitOpError() << "unsupported SDE resource query kind";
}

static LogicalResult lowerSdeControlBarrier(sde::SdeSuBarrierOp op) {
  OpBuilder builder(op);
  auto reasonAttr = op.getBarrierReasonAttr();
  arts::BarrierOp::create(
      builder, op.getLoc(),
      reasonAttr ? arts::ArtsBarrierReasonAttr::get(
                       op.getContext(), static_cast<arts::ArtsBarrierReason>(
                                            reasonAttr.getValue()))
                 : arts::ArtsBarrierReasonAttr{});
  op.erase();
  return success();
}

static LogicalResult eraseConsumedSdeControlToken(sde::SdeControlTokenOp op) {
  if (!op.getToken().use_empty())
    return op.emitOpError()
           << "survived SDE-to-ARTS boundary conversion with live users; "
              "control tokens must be consumed by SDE barriers before the "
              "ARTS boundary";
  op.erase();
  return success();
}

struct ConvertSdeBoundaryToArtsPass
    : public codir::impl::ConvertSdeBoundaryToArtsBase<
          ConvertSdeBoundaryToArtsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    SmallVector<sde::SdeMuDataOp> muDatas;
    module.walk([&](sde::SdeMuDataOp op) { muDatas.push_back(op); });
    for (sde::SdeMuDataOp op : muDatas) {
      if (failed(lowerMuData(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeMuAllocOp> muAllocs;
    module.walk([&](sde::SdeMuAllocOp op) { muAllocs.push_back(op); });
    for (sde::SdeMuAllocOp op : muAllocs) {
      if (failed(lowerMuAlloc(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeResourceQueryOp> resourceQueries;
    module.walk(
        [&](sde::SdeResourceQueryOp op) { resourceQueries.push_back(op); });
    for (sde::SdeResourceQueryOp op : resourceQueries) {
      if (failed(lowerSdeResourceQuery(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeSuBarrierOp> controlBarriers;
    module.walk([&](sde::SdeSuBarrierOp op) { controlBarriers.push_back(op); });
    for (sde::SdeSuBarrierOp op : controlBarriers) {
      if (failed(lowerSdeControlBarrier(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeControlTokenOp> controlTokens;
    module.walk(
        [&](sde::SdeControlTokenOp op) { controlTokens.push_back(op); });
    for (sde::SdeControlTokenOp op : controlTokens) {
      if (failed(eraseConsumedSdeControlToken(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeMuTokenOp> tokens;
    module.walk([&](sde::SdeMuTokenOp op) { tokens.push_back(op); });
    for (sde::SdeMuTokenOp token : tokens) {
      if (!token.getToken().use_empty()) {
        token.emitOpError()
            << "survived SDE boundary conversion; run "
               "`convert-sde-to-codir` before "
               "`convert-sde-boundary-to-arts` so SDE codelets become "
               "CODIR codelets before ARTS lowering";
        signalPassFailure();
        return;
      }
      token.erase();
    }
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::codir::createConvertSdeBoundaryToArtsPass() {
  return std::make_unique<ConvertSdeBoundaryToArtsPass>();
}
