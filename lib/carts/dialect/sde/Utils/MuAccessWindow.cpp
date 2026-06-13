///==========================================================================///
/// File: MuAccessWindow.cpp
///
/// Implementation of the SDE access-window query (see MuAccessWindow.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/ArrayRef.h"

using namespace mlir;

namespace mlir::carts::sde {

static std::optional<LayoutGraphFact>
findArrayLayoutFact(SdeSuIterateOp si, int64_t arrayId, LayoutGraphRole role) {
  if (!si)
    return std::nullopt;
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(si.getArrayLayoutAttr()))
    if (fact.id == arrayId && fact.role == role)
      return fact;
  return std::nullopt;
}

static SdeSuIterateOp findAccessWindowWitness(SdeMuAllocOp mu) {
  return findCommittedBlockLayoutWitness(mu);
}

static SdeSuIterateOp findReaderAccessWindowWitness(SdeMuAllocOp mu) {
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(mu);
  if (!arrayId)
    return SdeSuIterateOp();
  SdeSuIterateOp witness;
  for (Operation *user : mu.getMemref().getUsers()) {
    auto root = dyn_cast<SdeArrayLayoutRootOp>(user);
    if (!root || root.getMode() != SdeAccessMode::read ||
        static_cast<int64_t>(root.getArrayId()) != *arrayId)
      continue;
    SdeSuIterateOp reader = root->getParentOfType<SdeSuIterateOp>();
    if (!reader || !supportsRankExpandedAccessWindows(reader))
      continue;
    if (!findArrayLayoutFact(reader, *arrayId, LayoutGraphRole::read))
      continue;
    if (!witness)
      witness = reader;
  }
  return witness;
}

static SdeSuIterateOp resolveAccessWindowWitness(SdeMuAllocOp mu) {
  SdeSuIterateOp witness = findAccessWindowWitness(mu);
  if (witness && supportsRankExpandedAccessWindows(witness))
    return witness;
  return findReaderAccessWindowWitness(mu);
}

static bool hasUnsupportedCommittedWriter(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    SdeSuIterateOp writer = user->getParentOfType<SdeSuIterateOp>();
    while (writer && !recoverCommittedPhysicalLayout(writer))
      writer = writer->getParentOfType<SdeSuIterateOp>();
    if (writer && !supportsRankExpandedAccessWindows(writer))
      return true;
  }
  return false;
}

static bool muHasOnlyDirectMemoryUses(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers())
    if (!isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
             SdeArrayLayoutRootOp, SdeMuAccessWindowOp>(user))
      return false;
  return true;
}

static bool hasReplicatedReadFact(SdeCuRegionOp cu, int64_t arrayId, Value mu) {
  SdeSuIterateOp si = cu->getParentOfType<SdeSuIterateOp>();
  if (!si)
    return false;
  std::optional<LayoutGraphFact> fact =
      findArrayLayoutFact(si, arrayId, LayoutGraphRole::read);
  if (!fact || fact->layoutKind != ArrayLayoutKind::replicated ||
      !fact->ownerDims.empty())
    return false;
  for (SdeArrayLayoutRootOp root : si.getBody().getOps<SdeArrayLayoutRootOp>())
    if (root.getRoot() == mu && root.getMode() == SdeAccessMode::read &&
        static_cast<int64_t>(root.getArrayId()) == arrayId)
      return true;
  return false;
}

static bool hasReplicatedReadFact(SdeMuAllocOp mu, int64_t arrayId) {
  for (Operation *user : mu.getMemref().getUsers()) {
    auto root = dyn_cast<SdeArrayLayoutRootOp>(user);
    if (!root || root.getMode() != SdeAccessMode::read ||
        static_cast<int64_t>(root.getArrayId()) != arrayId)
      continue;
    SdeSuIterateOp si = root->getParentOfType<SdeSuIterateOp>();
    std::optional<LayoutGraphFact> fact =
        findArrayLayoutFact(si, arrayId, LayoutGraphRole::read);
    if (fact && fact->layoutKind == ArrayLayoutKind::replicated &&
        fact->ownerDims.empty())
      return true;
  }
  return false;
}

static llvm::SmallVector<RaisedWindowSpec, 4>
queryReplicatedReadAccessWindows(SdeMuAllocOp mu, MemRefType muType) {
  llvm::SmallVector<RaisedWindowSpec, 4> specs;
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(mu);
  if (!arrayId || !muHasOnlyDirectMemoryUses(mu) ||
      !hasReplicatedReadFact(mu, *arrayId))
    return specs;

  struct CuAccess {
    SdeCuRegionOp cu;
    bool hasRead = false;
    bool hasWrite = false;
  };
  llvm::SmallVector<CuAccess, 4> accesses;
  auto getOrCreateAccess = [&](SdeCuRegionOp cu) -> CuAccess * {
    for (CuAccess &access : accesses)
      if (access.cu == cu)
        return &access;
    accesses.push_back({cu, false, false});
    return &accesses.back();
  };

  for (Operation *user : mu.getMemref().getUsers()) {
    bool isLoad = isa<memref::LoadOp>(user);
    bool isStore = isa<memref::StoreOp>(user);
    if (!isLoad && !isStore)
      continue;
    SdeCuRegionOp cu = user->getParentOfType<SdeCuRegionOp>();
    if (!cu)
      return {};
    if (isLoad && !hasReplicatedReadFact(cu, *arrayId, mu.getMemref()))
      return {};
    if (isStore && cu->getParentOfType<SdeSuIterateOp>())
      return {};
    CuAccess *access = getOrCreateAccess(cu);
    access->hasRead |= isLoad;
    access->hasWrite |= isStore;
  }

  for (const CuAccess &access : accesses) {
    RaisedWindowSpec spec;
    spec.cu = access.cu;
    spec.mu = mu.getMemref();
    if (access.hasRead && access.hasWrite)
      spec.mode = SdeAccessMode::readwrite;
    else
      spec.mode = access.hasWrite ? SdeAccessMode::write : SdeAccessMode::read;
    spec.arrayId = *arrayId;
    specs.push_back(std::move(spec));
  }
  return specs;
}

llvm::SmallVector<RaisedWindowSpec, 4> queryAccessWindows(SdeMuAllocOp mu) {
  llvm::SmallVector<RaisedWindowSpec, 4> specs;

  auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
  if (!muType || !muType.hasStaticShape())
    return specs; // dynamic / non-memref -> conservative

  SdeSuIterateOp si = resolveAccessWindowWitness(mu);
  if (!si)
    return queryReplicatedReadAccessWindows(mu, muType);

  if (!supportsRankExpandedAccessWindows(si))
    return specs; // accumulator reductions -> conservative
  const bool blockWriteWindows = hasUnsupportedCommittedWriter(mu);

  // Recognize the ND rank-expanded block-grid form and prove that it encodes
  // the committed grain without deriving facts from the window itself. The
  // recognized struct carries K owner dims (ascending, parallel blockExtents)
  // and the K leading grid counts in owner order.
  std::optional<ExpandedBlockGridMu> exp = recognizeExpandedBlockGridMu(mu);
  if (!exp)
    return queryReplicatedReadAccessWindows(mu, muType);

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

  if (!muHasOnlyDirectMemoryUses(mu))
    return specs; // subview / cast / capture / escape -> out of scope

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
    std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(mu);
    if (!arrayId)
      return false;
    auto parentSu = cu->getParentOfType<SdeSuIterateOp>();
    if (!parentSu || !deriveCommittedHaloShape(parentSu))
      return false;
    bool hasReadOnlyRoot = false;
    for (SdeArrayLayoutRootOp root :
         parentSu.getBody().getOps<SdeArrayLayoutRootOp>())
      if (root.getRoot() == mu.getMemref() &&
          root.getMode() == SdeAccessMode::read &&
          static_cast<int64_t>(root.getArrayId()) == *arrayId)
        hasReadOnlyRoot = true;
    return hasReadOnlyRoot;
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
      if (std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(mu))
        spec.arrayId = *arrayId;
      specs.push_back(std::move(spec));
    };

    if (access.hasRead && access.hasWrite && cuNeedsSplitHaloRead(access.cu)) {
      appendSpec(SdeAccessMode::read);
      if (!blockWriteWindows)
        appendSpec(SdeAccessMode::write);
      continue;
    }
    if (access.hasRead && access.hasWrite) {
      if (!blockWriteWindows)
        appendSpec(SdeAccessMode::readwrite);
      else
        appendSpec(SdeAccessMode::read);
      continue;
    }
    if (access.hasWrite) {
      if (!blockWriteWindows)
        appendSpec(SdeAccessMode::write);
      continue;
    }
    appendSpec(SdeAccessMode::read);
  }
  return specs;
}

std::optional<RaisedWindowSpec> queryAccessWindow(SdeMuAllocOp mu) {
  llvm::SmallVector<RaisedWindowSpec, 4> specs = queryAccessWindows(mu);
  if (specs.size() != 1)
    return std::nullopt;
  return specs.front();
}

std::optional<MuAccessWindowGeometry>
deriveMuAccessWindowGeometry(SdeMuAccessWindowOp window) {
  auto muType = dyn_cast<MemRefType>(window.getMu().getType());
  if (!muType || !muType.hasStaticShape())
    return std::nullopt;

  auto fillFromExpanded = [&](unsigned ownerDimCount,
                              ArrayRef<int64_t> validExtents) {
    MuAccessWindowGeometry geom;
    ArrayRef<int64_t> shape = muType.getShape();
    geom.ownerDimCount = static_cast<int64_t>(ownerDimCount);
    geom.blockLo.assign(ownerDimCount, 0);
    geom.blockHi.assign(shape.begin(), shape.begin() + ownerDimCount);
    geom.validExtents.assign(validExtents.begin(), validExtents.end());
    return geom;
  };

  if (auto muAlloc = window.getMu().getDefiningOp<SdeMuAllocOp>()) {
    if (std::optional<ExpandedBlockGridMu> exp =
            recognizeExpandedBlockGridMu(muAlloc)) {
      return fillFromExpanded(exp->ownerDims.size(),
                              muType.getShape().drop_front(exp->ownerDims.size()));
    }
  }

  // Hand-written rank-expanded boundary IR may omit writer physical attrs; recover
  // the grid prefix structurally when the committed MU type is rank-expanded.
  if (muType.getRank() >= 2) {
    if (std::optional<RecoveredMuPhysicalLayout> recovered =
            recoverMuPhysicalLayoutFromExpandedType(muType))
      return fillFromExpanded(recovered->ownerDims.size(),
                              muType.getShape().drop_front(
                                  recovered->ownerDims.size()));
  }

  MuAccessWindowGeometry geom;
  geom.ownerDimCount = 0;
  geom.validExtents.assign(muType.getShape().begin(), muType.getShape().end());
  return geom;
}

} // namespace mlir::carts::sde
