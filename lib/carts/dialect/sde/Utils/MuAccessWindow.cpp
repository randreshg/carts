///==========================================================================///
/// File: MuAccessWindow.cpp
///
/// Implementation of the SDE access-window query (see MuAccessWindow.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
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

static bool suIterateAccessesMu(SdeSuIterateOp si, Value mu) {
  Block *computeBlock = getSuIterateComputeBlock(si);
  if (!computeBlock)
    return false;
  bool found = false;
  computeBlock->walk([&](Operation *op) {
    if (found)
      return WalkResult::interrupt();
    auto visit = [&](Value memref) {
      if (ValueAnalysis::stripMemrefViewOps(memref) == mu)
        found = true;
    };
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      visit(load.getMemref());
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      visit(store.getMemref());
    } else if (auto read = dyn_cast<affine::AffineReadOpInterface>(op)) {
      visit(read.getMemRef());
    } else if (auto write = dyn_cast<affine::AffineWriteOpInterface>(op)) {
      visit(write.getMemRef());
    }
    return WalkResult::advance();
  });
  return found;
}

static SdeSuIterateOp findMovementConsumerWitness(SdeMuAllocOp mu) {
  Value memref = mu.getMemref();
  for (Operation *user : memref.getUsers()) {
    if (!isa<SdeSuHaloOp, SdeSuAllToAllOp, SdeSuReduceScatterOp>(user))
      continue;
    auto distribute = user->getParentOfType<SdeSuDistributeOp>();
    if (!distribute)
      continue;
    for (SdeSuIterateOp si : distribute.getBody().getOps<SdeSuIterateOp>()) {
      if (!supportsRankExpandedAccessWindows(si))
        continue;
      if (suIterateAccessesMu(si, memref))
        return si;
    }
  }
  return SdeSuIterateOp();
}

static SdeSuIterateOp findStencilStoreWitness(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (!isa<memref::StoreOp, affine::AffineWriteOpInterface>(user))
      continue;
    SdeSuIterateOp si = user->getParentOfType<SdeSuIterateOp>();
    if (!si || !supportsRankExpandedAccessWindows(si))
      continue;
    return si;
  }
  return SdeSuIterateOp();
}

static SdeSuIterateOp resolveAccessWindowWitness(SdeMuAllocOp mu) {
  SdeSuIterateOp witness = findCommittedBlockLayoutWitness(mu);
  if (witness && supportsRankExpandedAccessWindows(witness))
    return witness;
  witness = findReaderAccessWindowWitness(mu);
  if (witness && supportsRankExpandedAccessWindows(witness))
    return witness;
  witness = findMovementConsumerWitness(mu);
  if (witness && supportsRankExpandedAccessWindows(witness))
    return witness;
  return findStencilStoreWitness(mu);
}

/// A residual `<single>` init writer (or an unlabeled writer SU) that stores
/// the whole rank-expanded MU proves the committed owner grid directly: every
/// owner block index is a loop-variant block coordinate, so the access spans
/// every block, not a fixed one. Returns true only if at least one store to
/// `mu` addresses each owner dim with a non-constant index. Read-only and
/// single-fixed-block accessors do not qualify, so this never authorizes a
/// window for a partial / contraction reader.
///
/// Gated on witness classification: a stencil witness already proves its own
/// owner grid through the stencil-store-witness path (findStencilStoreWitness).
/// Authoring a second committed-grid residual window for a stencil output
/// over-fires when the realized loop cannot map every owner coordinate. Genuine
/// non-stencil residual writers can still need this proof when their loop
/// bounds cannot otherwise prove the committed grid.
// A stencil SU is block-realizable at the SDE/ARTS boundary only if its
// realized `su_iterate` loop exposes at least one loop dim per committed owner
// dim of EVERY array it touches. When some touched array commits more owner
// dims than the stencil has loop dims, the boundary cannot map that array's
// owner window from the loop and falls it back to a coarse DB.
static bool stencilIsBlockRealizable(SdeSuIterateOp stencil) {
  unsigned loopRank = stencil.getLowerBounds().size();
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(stencil.getArrayLayoutAttr()))
    if (fact.ownerDims.size() > loopRank)
      return false;
  return true;
}

// True when `mu` is read or written by a stencil-classified SU that is not
// block-realizable (see stencilIsBlockRealizable). Such a stencil leaves at
// least one of its arrays coarse; mixing that coarse DB with any block DB in
// the same SU is unsupported by the direct (block) SDE/ARTS boundary path — it
// rejects the unmapped owner window with "cannot map SDE access-window block
// coordinate to a loop dimension" or "touches a DB without a committed SDE
// access-window dependency". So the WHOLE stencil neighborhood (every array the
// stencil reads or writes, including non-rank-expanded / replicated outputs and
// any sibling array a separate init/copy writer would otherwise block-author)
// must be realized coarse-consistently until the stencil itself can map its
// multi-D owner grid. Non-stencil residual writers remain on the block path.
static bool muUsedByUnmappableStencil(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (!isa<memref::LoadOp, memref::StoreOp, affine::AffineReadOpInterface,
             affine::AffineWriteOpInterface>(user))
      continue;
    SdeSuIterateOp accessor = user->getParentOfType<SdeSuIterateOp>();
    if (!accessor)
      continue;
    auto classification = accessor.getStructuredClassification();
    if (!classification ||
        *classification != SdeStructuredClassification::stencil)
      continue;
    if (!stencilIsBlockRealizable(accessor))
      return true;
  }
  return false;
}

static bool hasFullGridResidualWriter(SdeMuAllocOp mu, unsigned ownerDimCount,
                                      SdeSuIterateOp si) {
  if (ownerDimCount == 0)
    return false;
  for (Operation *user : mu.getMemref().getUsers()) {
    auto store = dyn_cast<memref::StoreOp>(user);
    if (!store ||
        ValueAnalysis::stripMemrefViewOps(store.getMemRef()) != mu.getMemref())
      continue;
    SdeSuIterateOp writer = store->getParentOfType<SdeSuIterateOp>();
    if (writer)
      if (auto classification = writer.getStructuredClassification())
        if (*classification == SdeStructuredClassification::stencil)
          continue;
    if (store.getIndices().size() < ownerDimCount)
      continue;
    bool everyOwnerDimVariant = true;
    for (unsigned i = 0; i < ownerDimCount; ++i)
      if (ValueAnalysis::tryFoldConstantIndex(store.getIndices()[i])) {
        everyOwnerDimVariant = false;
        break;
      }
    if (everyOwnerDimVariant)
      return true;
  }
  return false;
}

static bool isDirectMuMemoryAccess(Operation *user) {
  return isa<memref::LoadOp, memref::StoreOp>(user) ||
         isa<affine::AffineReadOpInterface, affine::AffineWriteOpInterface>(
             user);
}

static bool classifiesAsMuLoad(Operation *user) {
  return isa<memref::LoadOp>(user) || isa<affine::AffineReadOpInterface>(user);
}

static bool classifiesAsMuStore(Operation *user) {
  return isa<memref::StoreOp>(user) ||
         isa<affine::AffineWriteOpInterface>(user);
}

static bool gridIndexUsesOwnerIv(Value index, Value ownerIv,
                                 int64_t blockExtent) {
  if (blockExtent <= 0)
    return false;
  auto div =
      ValueAnalysis::stripNumericCasts(index).getDefiningOp<arith::DivUIOp>();
  if (!div)
    return false;
  std::optional<int64_t> divisor =
      ValueAnalysis::tryFoldConstantIndex(div.getRhs());
  if (!divisor || *divisor != blockExtent)
    return false;
  return ValueAnalysis::sameValue(
      ValueAnalysis::stripNumericCasts(div.getLhs()),
      ValueAnalysis::stripNumericCasts(ownerIv));
}

static bool cuAccessesUseSuOwnerGrid(SdeCuRegionOp cu, Value mu,
                                     unsigned ownerDimCount,
                                     ArrayRef<int64_t> blockExtents) {
  if (!cu || !mu || ownerDimCount == 0 || blockExtents.size() < ownerDimCount)
    return false;
  SdeSuIterateOp owner = cu->getParentOfType<SdeSuIterateOp>();
  SmallVector<Value, 4> ownerIvs = collectOwnerIndexValues(owner);
  if (ownerIvs.size() < ownerDimCount)
    return false;

  bool sawAccess = false;
  bool failed = false;
  cu.getBody().walk([&](Operation *op) {
    if (failed)
      return;
    Value memref;
    ValueRange indices;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      memref = load.getMemref();
      indices = load.getIndices();
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      memref = store.getMemref();
      indices = store.getIndices();
    } else {
      return;
    }
    if (ValueAnalysis::stripMemrefViewOps(memref) != mu)
      return;
    sawAccess = true;
    if (indices.size() < ownerDimCount) {
      failed = true;
      return;
    }
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      if (!gridIndexUsesOwnerIv(indices[slot], ownerIvs[slot],
                                blockExtents[slot])) {
        failed = true;
        return;
      }
    }
  });
  return sawAccess && !failed;
}

static bool mayAuthorReadWriteWindowForCu(SdeCuRegionOp cu, Value mu,
                                          unsigned ownerDimCount,
                                          ArrayRef<int64_t> blockExtents) {
  SdeSuIterateOp owner = cu ? cu->getParentOfType<SdeSuIterateOp>() : nullptr;
  if (!owner)
    return true;
  std::optional<SdeStructuredClassification> classification =
      owner.getStructuredClassification();
  if (!classification ||
      *classification != SdeStructuredClassification::reduction)
    return true;
  if (!owner.getReductionAccumulators().empty())
    return false;
  if (owner.getPartialReductionAttr())
    return true;
  return cuAccessesUseSuOwnerGrid(cu, mu, ownerDimCount, blockExtents);
}

static bool muHasOnlyDirectMemoryUses(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers())
    if (!isDirectMuMemoryAccess(user) &&
        !isa<memref::DeallocOp, SdeArrayLayoutRootOp, SdeSuHaloOp,
             SdeSuAllToAllOp, SdeSuReduceScatterOp>(user))
      return false;
  return true;
}

static std::optional<MuAccessWindowGeometry>
deriveCommittedPhysicalGeometry(SdeMuAllocOp muAlloc, MemRefType muType) {
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(muAlloc);
  if (!arrayId)
    return std::nullopt;
  SdeSuIterateOp writer = findCommittedBlockLayoutWriter(muAlloc);
  if (!writer)
    return std::nullopt;
  std::optional<LayoutGraphFact> fact =
      findArrayLayoutFact(writer, *arrayId, LayoutGraphRole::write);
  if (!fact || fact->ownerDims.empty() || fact->blockShape.empty() ||
      fact->blockShape.size() != static_cast<size_t>(muType.getRank()))
    return std::nullopt;
  if (fact->layoutKind != ArrayLayoutKind::blockParallel &&
      fact->layoutKind != ArrayLayoutKind::blockContraction)
    return std::nullopt;

  MuAccessWindowGeometry geom;
  geom.ownerDimCount = static_cast<int64_t>(fact->ownerDims.size());
  geom.blockLo.assign(fact->ownerDims.size(), 0);
  geom.validExtents.assign(fact->blockShape.begin(), fact->blockShape.end());
  geom.blockHi.reserve(fact->ownerDims.size());
  ArrayRef<int64_t> shape = muType.getShape();
  bool hasNonUnitOwnerBlock = false;
  for (int64_t ownerDim : fact->ownerDims) {
    if (ownerDim < 0 ||
        static_cast<size_t>(ownerDim) >= fact->blockShape.size())
      return std::nullopt;
    int64_t blockExtent = fact->blockShape[static_cast<size_t>(ownerDim)];
    if (blockExtent <= 0)
      return std::nullopt;
    hasNonUnitOwnerBlock |= blockExtent != 1;
    int64_t extent = shape[static_cast<size_t>(ownerDim)];
    if (extent < 0)
      return std::nullopt;
    geom.blockHi.push_back((extent + blockExtent - 1) / blockExtent);
  }
  if (!hasNonUnitOwnerBlock)
    return std::nullopt;
  return geom;
}

static bool hasUnsupportedCommittedWriter(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (!classifiesAsMuStore(user))
      continue;
    SdeSuIterateOp writer = user->getParentOfType<SdeSuIterateOp>();
    while (writer && !recoverCommittedPhysicalLayout(writer))
      writer = writer->getParentOfType<SdeSuIterateOp>();
    if (writer && !supportsRankExpandedAccessWindows(writer))
      return true;
  }
  return false;
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

// A committed replicated WRITE fact that SDE cannot block-distribute as
// single-writer is whole-array. Mirrors hasReplicatedReadFact so the type-shape
// fallback in deriveMuAccessWindowGeometry does not misread a square logical
// write MU as a 1-D owner grid.
static bool hasReplicatedWriteFact(SdeMuAllocOp mu, int64_t arrayId) {
  for (Operation *user : mu.getMemref().getUsers()) {
    auto root = dyn_cast<SdeArrayLayoutRootOp>(user);
    if (!root || root.getMode() != SdeAccessMode::write ||
        static_cast<int64_t>(root.getArrayId()) != arrayId)
      continue;
    SdeSuIterateOp si = root->getParentOfType<SdeSuIterateOp>();
    std::optional<LayoutGraphFact> fact =
        findArrayLayoutFact(si, arrayId, LayoutGraphRole::write);
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

  // A replicated array touched by a stencil whose owner grid the realized loop
  // cannot map must stay coarse-consistent with that stencil's other (coarse)
  // arrays; otherwise the stencil SU mixes a windowed and a coarse DB and the
  // direct boundary path rejects it. See muUsedByUnmappableStencil.
  if (muUsedByUnmappableStencil(mu))
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
    if (!isDirectMuMemoryAccess(user))
      continue;
    bool isLoad = classifiesAsMuLoad(user);
    bool isStore = classifiesAsMuStore(user);
    SdeCuRegionOp cu = user->getParentOfType<SdeCuRegionOp>();
    if (!cu)
      return {};
    if (isLoad && !hasReplicatedReadFact(cu, *arrayId, mu.getMemref()))
      return {};
    if (isStore) {
      // A fully-replicated write is legal (every node writes the whole array);
      // bail only on a committed BLOCK write fact, which contradicts
      // replication.
      if (SdeSuIterateOp writerSu = cu->getParentOfType<SdeSuIterateOp>()) {
        std::optional<LayoutGraphFact> wfact =
            findArrayLayoutFact(writerSu, *arrayId, LayoutGraphRole::write);
        if (wfact && wfact->layoutKind != ArrayLayoutKind::replicated &&
            !wfact->ownerDims.empty())
          return {};
      }
    }
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

  // Keep an array coarse when it participates in a stencil whose realized
  // su_iterate loop cannot map its committed owner grid (see
  // muUsedByUnmappableStencil). Any block window authored here — whether proven
  // from a 2-D init/copy witness below or from the residual-writer fallback —
  // would make the stencil SU a mixed block/coarse writer that the direct
  // SDE/ARTS boundary path cannot lower (it would reject the unmapped owner
  // window with "cannot map SDE access-window block coordinate to a loop
  // dimension"). Realizing such a stencil as block needs an inner-scf owner
  // loop plus RO halo; until then the whole stencil neighborhood is realized
  // coarse-consistently.
  if (muUsedByUnmappableStencil(mu))
    return specs;

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
  if (!ownerExtents || ownerExtents->size() != ownerDimCount) {
    // The resolved witness (typically a contraction reader) does not iterate
    // the array's full owner grid, so its loop bounds cannot prove the grid.
    // When a residual init writer stores every block with loop-variant owner
    // coordinates, the committed expanded grid IS the authoritative shape:
    // owner extent = gridCount * blockExtent. This authors the missing window
    // for the init-writer (and read windows for any residual readers) without
    // ever firing for a partial / single-fixed-block accessor.
    if (!hasFullGridResidualWriter(mu, ownerDimCount, si))
      return specs;
    SmallVector<int64_t, 4> gridExtents(ownerDimCount, 0);
    for (unsigned i = 0; i < ownerDimCount; ++i)
      gridExtents[i] = exp->gridCounts[i] * exp->blockExtents[i];
    ownerExtents = std::move(gridExtents);
  }

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
    if (!isDirectMuMemoryAccess(user))
      continue;
    auto userCu = user->getParentOfType<SdeCuRegionOp>();
    if (!userCu)
      return {}; // access outside any CU -> conservative for the whole MU
    CuAccess *access = getOrCreateAccess(userCu);
    access->hasRead |= classifiesAsMuLoad(user);
    access->hasWrite |= classifiesAsMuStore(user);
  }

  for (const CuAccess &access : accesses) {
    if (!access.hasRead && !access.hasWrite)
      continue;
    if (access.hasRead && access.hasWrite &&
        !mayAuthorReadWriteWindowForCu(access.cu, mu.getMemref(), ownerDimCount,
                                       exp->blockExtents))
      return {};
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

std::optional<MuAccessWindowGeometry> deriveMuAccessWindowGeometry(Value mu) {
  auto muType = dyn_cast<MemRefType>(mu.getType());
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

  if (auto muAlloc = mu.getDefiningOp<SdeMuAllocOp>()) {
    // A committed replicated array (read OR write) is whole-array
    // (ownerDimCount==0). The committed layout fact is authoritative: check it
    // BEFORE the structural type guesses below, which would otherwise misread a
    // square replicated logical shape as a 1-D owner grid via
    // recognizeExpandedBlockGridMu / the type-shape fallback.
    if (std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(muAlloc))
      if (hasReplicatedReadFact(muAlloc, *arrayId) ||
          hasReplicatedWriteFact(muAlloc, *arrayId)) {
        MuAccessWindowGeometry geom;
        geom.ownerDimCount = 0;
        geom.validExtents.assign(muType.getShape().begin(),
                                 muType.getShape().end());
        return geom;
      }
    if (std::optional<MuAccessWindowGeometry> geom =
            deriveCommittedPhysicalGeometry(muAlloc, muType))
      return geom;
    if (std::optional<ExpandedBlockGridMu> exp =
            recognizeExpandedBlockGridMu(muAlloc)) {
      return fillFromExpanded(
          exp->ownerDims.size(),
          muType.getShape().drop_front(exp->ownerDims.size()));
    }
  }

  // Hand-written rank-expanded boundary IR may omit writer physical attrs;
  // recover the grid prefix structurally when the committed MU type is
  // rank-expanded.
  if (muType.getRank() >= 2) {
    if (std::optional<RecoveredMuPhysicalLayout> recovered =
            recoverMuPhysicalLayoutFromExpandedType(muType))
      return fillFromExpanded(
          recovered->ownerDims.size(),
          muType.getShape().drop_front(recovered->ownerDims.size()));
  }

  MuAccessWindowGeometry geom;
  geom.ownerDimCount = 0;
  geom.validExtents.assign(muType.getShape().begin(), muType.getShape().end());
  return geom;
}

} // namespace mlir::carts::sde
