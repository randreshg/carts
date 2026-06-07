///==========================================================================///
/// File: MuAccessWindow.cpp
///
/// Implementation of the SDE access-window planner (see MuAccessWindow.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/ArrayRef.h"

using namespace mlir;

namespace mlir::carts::sde {

std::optional<RaisedWindowPlan> planAccessWindow(SdeMuAllocOp mu) {
  auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
  if (!muType || !muType.hasStaticShape())
    return std::nullopt; // dynamic / non-memref -> conservative

  SdeSuIterateOp si = findCommittedBlockPlanWriter(mu);
  if (!si)
    return std::nullopt; // no committed writer / conflicting plans

  // Classification gate: elementwise/stencil only.
  std::optional<SdeStructuredClassification> cls =
      si.getStructuredClassification();
  if (!cls)
    return std::nullopt;
  switch (*cls) {
  case SdeStructuredClassification::elementwise:
  case SdeStructuredClassification::elementwise_pipeline:
  case SdeStructuredClassification::stencil:
    break;
  default:
    return std::nullopt; // matmul / reduction -> conservative
  }

  // Recognize the single-owner rank-expanded block-grid form and prove that it
  // encodes the committed grain without deriving facts from the window itself.
  std::optional<ExpandedBlockGridMu> exp =
      recognizeExpandedBlockGridMu(si, muType);
  if (!exp)
    return std::nullopt;

  ArrayRef<int64_t> tiles = muType.getShape().drop_front(); // single grid dim
  if (tiles[exp->ownerDim] != exp->blockExtent)
    return std::nullopt;

  std::optional<int64_t> ownerExtent =
      findOwnerIterationExtent(si, exp->blockExtent, exp->gridCount);
  if (!ownerExtent)
    return std::nullopt;

  SmallVector<int64_t, 4> logicalShape(tiles.begin(), tiles.end());
  logicalShape[exp->ownerDim] = *ownerExtent;
  std::optional<SmallVector<unsigned, 2>> recovered =
      recoverOwnerDims(muType, logicalShape);
  SmallVector<unsigned, 2> committed{exp->ownerDim};
  if (!recovered || *recovered != committed)
    return std::nullopt;

  // Pre-scan uses: only direct load/store/dealloc on the MU root, plus any
  // already-raised window (so the planner is idempotent across re-runs).
  for (Operation *user : mu.getMemref().getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
            SdeMuAccessWindowOp>(user))
      continue;
    return std::nullopt; // subview / cast / capture / escape -> conservative
  }

  // Determine the single enclosing CU and the access mode. An in-place
  // (read+write) access, or accesses spanning zero or multiple CUs, are out of
  // scope (no clean single canonical window) -> conservative skip.
  SdeCuRegionOp cu;
  bool hasRead = false, hasWrite = false;
  for (Operation *user : mu.getMemref().getUsers()) {
    bool isLoad = isa<memref::LoadOp>(user);
    bool isStore = isa<memref::StoreOp>(user);
    if (!isLoad && !isStore)
      continue;
    auto userCu = user->getParentOfType<SdeCuRegionOp>();
    if (!userCu || (cu && cu != userCu))
      return std::nullopt; // access outside any CU, or spanning multiple CUs
    cu = userCu;
    hasRead |= isLoad;
    hasWrite |= isStore;
  }
  if (!cu || (hasRead && hasWrite))
    return std::nullopt; // no access, or in-place

  RaisedWindowPlan plan;
  plan.cu = cu;
  plan.mu = mu.getMemref();
  plan.mode = hasWrite ? SdeAccessMode::write : SdeAccessMode::read;
  plan.ownerDimCount = 1;
  plan.blockLo.assign(1, /*value=*/0);
  plan.blockHi.assign(1, exp->gridCount);
  plan.validExtents.assign(tiles.begin(), tiles.end());
  return plan;
}

} // namespace mlir::carts::sde
