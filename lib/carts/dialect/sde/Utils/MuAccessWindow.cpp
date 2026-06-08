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

llvm::SmallVector<RaisedWindowPlan, 4> planAccessWindows(SdeMuAllocOp mu) {
  llvm::SmallVector<RaisedWindowPlan, 4> plans;

  auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
  if (!muType || !muType.hasStaticShape())
    return plans; // dynamic / non-memref -> conservative

  SdeSuIterateOp si = findCommittedBlockPlanWriter(mu);
  if (!si)
    return plans; // no committed writer / conflicting plans

  // Classification gate: elementwise/stencil only.
  std::optional<SdeStructuredClassification> cls =
      si.getStructuredClassification();
  if (!cls)
    return plans;
  switch (*cls) {
  case SdeStructuredClassification::elementwise:
  case SdeStructuredClassification::elementwise_pipeline:
  case SdeStructuredClassification::stencil:
    break;
  default:
    return plans; // matmul / reduction -> conservative
  }

  // Recognize the single-owner rank-expanded block-grid form and prove that it
  // encodes the committed grain without deriving facts from the window itself.
  std::optional<ExpandedBlockGridMu> exp =
      recognizeExpandedBlockGridMu(si, muType);
  if (!exp)
    return plans;

  ArrayRef<int64_t> tiles = muType.getShape().drop_front(); // single grid dim
  if (tiles[exp->ownerDim] != exp->blockExtent)
    return plans;

  std::optional<int64_t> ownerExtent =
      findOwnerIterationExtent(si, exp->blockExtent, exp->gridCount);
  if (!ownerExtent)
    return plans;

  SmallVector<int64_t, 4> logicalShape(tiles.begin(), tiles.end());
  logicalShape[exp->ownerDim] = *ownerExtent;
  std::optional<SmallVector<unsigned, 2>> recovered =
      recoverOwnerDims(muType, logicalShape);
  SmallVector<unsigned, 2> committed{exp->ownerDim};
  if (!recovered || *recovered != committed)
    return plans;

  // Pre-scan uses: only direct load/store/dealloc on the MU root, plus any
  // already-raised window (so the planner is idempotent across re-runs).
  for (Operation *user : mu.getMemref().getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
            SdeMuAccessWindowOp>(user))
      continue;
    return plans; // subview / cast / capture / escape -> conservative
  }

  struct CuAccess {
    SdeCuRegionOp cu;
    bool hasRead = false;
    bool hasWrite = false;
  };

  SmallVector<CuAccess, 4> accesses;
  auto getOrCreateAccess = [&](SdeCuRegionOp cu) -> CuAccess * {
    for (CuAccess &access : accesses)
      if (access.cu == cu)
        return &access;
    accesses.push_back({cu, false, false});
    return &accesses.back();
  };

  // Determine each enclosing CU's access mode independently. Initialization
  // CUs can write an MU while a later compute CU reads it; each single-mode CU
  // still has a canonical window. Only same-CU in-place read/write accesses are
  // skipped because they have no single read-or-write window.
  for (Operation *user : mu.getMemref().getUsers()) {
    bool isLoad = isa<memref::LoadOp>(user);
    bool isStore = isa<memref::StoreOp>(user);
    if (!isLoad && !isStore)
      continue;
    auto userCu = user->getParentOfType<SdeCuRegionOp>();
    if (!userCu)
      return {}; // access outside any CU -> conservative for the whole MU
    CuAccess *access = getOrCreateAccess(userCu);
    access->hasRead |= isLoad;
    access->hasWrite |= isStore;
  }

  for (const CuAccess &access : accesses) {
    if (access.hasRead && access.hasWrite)
      continue;
    if (!access.hasRead && !access.hasWrite)
      continue;
    RaisedWindowPlan plan;
    plan.cu = access.cu;
    plan.mu = mu.getMemref();
    plan.mode = access.hasWrite ? SdeAccessMode::write : SdeAccessMode::read;
    plan.ownerDimCount = 1;
    plan.blockLo.assign(1, /*value=*/0);
    plan.blockHi.assign(1, exp->gridCount);
    plan.validExtents.assign(tiles.begin(), tiles.end());
    plans.push_back(std::move(plan));
  }
  return plans;
}

std::optional<RaisedWindowPlan> planAccessWindow(SdeMuAllocOp mu) {
  llvm::SmallVector<RaisedWindowPlan, 4> plans = planAccessWindows(mu);
  if (plans.size() != 1)
    return std::nullopt;
  return plans.front();
}

} // namespace mlir::carts::sde
