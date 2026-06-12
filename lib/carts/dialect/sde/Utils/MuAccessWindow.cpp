///==========================================================================///
/// File: MuAccessWindow.cpp
///
/// Implementation of the SDE access-window query (see MuAccessWindow.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/ArrayRef.h"

using namespace mlir;

namespace mlir::carts::sde {

static bool hasUnsupportedCommittedWriter(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    SdeSuIterateOp writer = user->getParentOfType<SdeSuIterateOp>();
    while (writer && !(writer.getPhysicalOwnerDimsAttr() &&
                       writer.getPhysicalBlockShapeAttr()))
      writer = writer->getParentOfType<SdeSuIterateOp>();
    if (writer && !supportsRankExpandedAccessWindows(writer))
      return true;
  }
  return false;
}

llvm::SmallVector<RaisedWindowSpec, 4> queryAccessWindows(SdeMuAllocOp mu) {
  llvm::SmallVector<RaisedWindowSpec, 4> specs;

  auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
  if (!muType || !muType.hasStaticShape())
    return specs; // dynamic / non-memref -> conservative

  SdeSuIterateOp si = findCommittedBlockLayoutWriter(mu);
  if (!si)
    return specs; // no committed writer / conflicting specs

  if (!supportsRankExpandedAccessWindows(si))
    return specs; // accumulator reductions -> conservative
  if (hasUnsupportedCommittedWriter(mu))
    return specs; // mixed writers for one MU -> conservative

  // Recognize the ND rank-expanded block-grid form and prove that it encodes
  // the committed grain without deriving facts from the window itself. The
  // recognized struct carries K owner dims (ascending, parallel blockExtents)
  // and the K leading grid counts in owner order.
  std::optional<ExpandedBlockGridMu> exp = recognizeExpandedBlockGridMu(mu);
  if (!exp)
    return specs;

  const unsigned ownerDimCount = exp->ownerDims.size();
  if (ownerDimCount == 0)
    return specs; // no owner grid -> conservative

  // The L trailing tile dims (one per logical dim); the K leading dims are the
  // owner grid.
  ArrayRef<int64_t> tiles = muType.getShape().drop_front(ownerDimCount);
  if (tiles.size() != exp->logicalRank)
    return specs; // shape disagreement -> conservative, never partial-raise

  // Each owner tile dim must equal its committed block extent verbatim.
  for (unsigned i = 0; i < ownerDimCount; ++i) {
    unsigned od = exp->ownerDims[i];
    if (od >= tiles.size() || tiles[od] != exp->blockExtents[i])
      return specs;
  }

  // Each owner grid count must be ceilDiv(extent, block) of a distinct
  // committed iteration extent (with the existing halo-widening allowance).
  std::optional<SmallVector<int64_t, 4>> ownerExtents =
      findOwnerIterationExtents(si, exp->blockExtents, exp->gridCounts);
  if (!ownerExtents || ownerExtents->size() != ownerDimCount)
    return specs;

  // Reconstruct the logical shape from the proven owner extents and require it
  // to recover EXACTLY the committed (ascending) owner dims.
  SmallVector<int64_t, 4> logicalShape(tiles.begin(), tiles.end());
  for (unsigned i = 0; i < ownerDimCount; ++i)
    logicalShape[exp->ownerDims[i]] = (*ownerExtents)[i];
  std::optional<SmallVector<unsigned, 2>> recovered =
      recoverOwnerDims(muType, logicalShape);
  if (!recovered ||
      ArrayRef<unsigned>(*recovered) != ArrayRef<unsigned>(exp->ownerDims))
    return specs;

  // Pre-scan uses: only direct load/store/dealloc on the MU root, plus any
  // already-raised window (so the query is idempotent across re-runs).
  for (Operation *user : mu.getMemref().getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
            SdeArrayLayoutRootOp, SdeMuAccessWindowOp>(user))
      continue;
    return specs; // subview / cast / capture / escape -> out of scope
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

  auto hasPositiveI64ArrayEntry = [](ArrayAttr attr) {
    if (!attr)
      return false;
    return llvm::any_of(attr, [](Attribute value) {
      auto integer = dyn_cast<IntegerAttr>(value);
      return integer && integer.getInt() > 0;
    });
  };

  auto i64ArrayContains = [](ArrayAttr attr, int64_t needle) {
    if (!attr)
      return false;
    return llvm::any_of(attr, [&](Attribute value) {
      auto integer = dyn_cast<IntegerAttr>(value);
      return integer && integer.getInt() == needle;
    });
  };

  auto cuNeedsSplitHaloRead = [&](SdeCuRegionOp cu) {
    IntegerAttr muArrayId = mu.getArrayIdAttr();
    if (!muArrayId)
      return false;
    int64_t arrayId = muArrayId.getInt();
    auto parentSu = cu->getParentOfType<SdeSuIterateOp>();
    if (!parentSu ||
        !hasPositiveI64ArrayEntry(parentSu.getPhysicalHaloShapeAttr()))
      return false;
    ArrayAttr layoutsDisagree = parentSu.getLayoutsDisagreeAttr();
    if (layoutsDisagree && !i64ArrayContains(layoutsDisagree, arrayId))
      return false;
    for (SdeArrayLayoutRootOp root :
         parentSu.getBody().getOps<SdeArrayLayoutRootOp>())
      if (root.getRoot() == mu.getMemref() &&
          root.getMode() == SdeAccessMode::read &&
          static_cast<int64_t>(root.getArrayId()) == arrayId)
        return true;
    return false;
  };

  // Determine each enclosing CU's access mode independently. Initialization
  // CUs can write an MU while a later compute CU reads it. Same-CU in-place
  // accesses stay readwrite unless the MU also has a committed halo read; that
  // case must be split into read-halo and write windows before ARTS.
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
    if (!access.hasRead && !access.hasWrite)
      continue;
    auto appendSpec = [&](SdeAccessMode mode) {
      RaisedWindowSpec spec;
      spec.cu = access.cu;
      spec.mu = mu.getMemref();
      spec.mode = mode;
      if (IntegerAttr arrayId = mu.getArrayIdAttr())
        spec.arrayId = arrayId.getInt();
      spec.ownerDimCount = static_cast<int64_t>(ownerDimCount);
      spec.blockLo.assign(ownerDimCount, /*value=*/0);
      spec.blockHi.assign(exp->gridCounts.begin(), exp->gridCounts.end());
      spec.validExtents.assign(tiles.begin(), tiles.end());
      specs.push_back(std::move(spec));
    };

    if (access.hasRead && access.hasWrite && cuNeedsSplitHaloRead(access.cu)) {
      appendSpec(SdeAccessMode::read);
      appendSpec(SdeAccessMode::write);
      continue;
    }
    if (access.hasRead && access.hasWrite)
      appendSpec(SdeAccessMode::readwrite);
    else
      appendSpec(access.hasWrite ? SdeAccessMode::write : SdeAccessMode::read);
  }
  return specs;
}

std::optional<RaisedWindowSpec> queryAccessWindow(SdeMuAllocOp mu) {
  llvm::SmallVector<RaisedWindowSpec, 4> specs = queryAccessWindows(mu);
  if (specs.size() != 1)
    return std::nullopt;
  return specs.front();
}

} // namespace mlir::carts::sde
