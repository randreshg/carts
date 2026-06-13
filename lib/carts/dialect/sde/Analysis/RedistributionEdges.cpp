///==========================================================================///
/// File: RedistributionEdges.cpp
///
/// See RedistributionEdges.h.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include <cstdlib>

using namespace mlir;

namespace mlir::carts::sde {

namespace {

/// The committed home (producer) layout of one array root.
struct HomeLayout {
  SdeSuIterateOp writer;
  ArrayLayoutKind layoutKind = ArrayLayoutKind::replicated;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

struct RedistEndpoint {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};


static std::optional<HomeLayout>
homeLayoutFromCommittedPhysical(SdeSuIterateOp su) {
  std::optional<CommittedSuPhysicalLayout> committed =
      recoverCommittedPhysicalLayout(su);
  if (!committed || committed->ownerDims.empty() ||
      committed->blockShape.empty())
    return std::nullopt;
  HomeLayout home;
  home.writer = su;
  home.layoutKind = ArrayLayoutKind::blockParallel;
  home.ownerDims.assign(committed->ownerDims.begin(), committed->ownerDims.end());
  home.blockShape.assign(committed->blockShape.begin(),
                         committed->blockShape.end());
  return home;
}

static void recordHomeLayout(llvm::DenseMap<int64_t, HomeLayout> &homeByArrayId,
                             llvm::DenseSet<int64_t> &conflictingHome,
                             int64_t arrayId, HomeLayout home) {
  auto it = homeByArrayId.find(arrayId);
  if (it == homeByArrayId.end()) {
    homeByArrayId[arrayId] = std::move(home);
    return;
  }
  if (it->second.ownerDims != home.ownerDims ||
      it->second.blockShape != home.blockShape)
    conflictingHome.insert(arrayId);
}

static std::optional<LayoutGraphFact> findLayoutFact(SdeSuIterateOp su,
                                                     int64_t arrayId) {
  if (ArrayAttr layout = su.getArrayLayoutAttr())
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout))
      if (fact.id == arrayId)
        return fact;
  return std::nullopt;
}

static const ArrayAccessProfile *
findProfileForRoot(const ModuleSuAccessRelations &relations, Value root) {
  auto direct = relations.profiles.find(root);
  if (direct != relations.profiles.end())
    return &direct->second;
  for (const auto &entry : relations.profiles)
    if (::mlir::carts::ValueAnalysis::sameMemrefRoot(entry.first, root))
      return &entry.second;
  return nullptr;
}

static std::optional<SmallVector<int64_t, 4>>
getCommittedHaloShape(SdeSuIterateOp reader) {
  if (std::optional<SmallVector<int64_t, 4>> halo =
          deriveCommittedHaloShape(reader))
    return halo;

  std::optional<SmallVector<int64_t, 4>> mins =
      readI64ArrayAttr(reader.getAccessMinOffsetsAttr());
  std::optional<SmallVector<int64_t, 4>> maxs =
      readI64ArrayAttr(reader.getAccessMaxOffsetsAttr());
  if (!mins || !maxs || mins->size() != maxs->size())
    return std::nullopt;

  SmallVector<int64_t, 4> halo;
  halo.reserve(mins->size());
  bool nonZero = false;
  for (auto [minOffset, maxOffset] : llvm::zip_equal(*mins, *maxs)) {
    int64_t width =
        std::max<int64_t>(std::llabs(minOffset), std::llabs(maxOffset));
    nonZero |= width != 0;
    halo.push_back(width);
  }
  if (!nonZero)
    return std::nullopt;
  return halo;
}

static std::optional<int64_t> getOwnerHaloRadius(ArrayRef<int64_t> haloShape,
                                                 unsigned logicalRank,
                                                 ArrayRef<int64_t> ownerDims,
                                                 unsigned ownerSlot,
                                                 unsigned logicalOwnerDim) {
  if (haloShape.empty())
    return std::nullopt;
  // Committed halo shapes arrive in one of three forms: logical-rank-length
  // (index by the logical owner dim), owner-dim-length (index by the owner
  // slot), or scalar single-owner (the lone radius). All three are ND-general
  // once indexed by the (slot, logical dim) of the owner being projected.
  if (haloShape.size() == logicalRank && logicalOwnerDim < haloShape.size())
    return haloShape[logicalOwnerDim];
  if (haloShape.size() == ownerDims.size()) {
    for (auto [slot, ownerDim] : llvm::enumerate(ownerDims))
      if (ownerDim == static_cast<int64_t>(logicalOwnerDim) &&
          slot < haloShape.size())
        return haloShape[slot];
    if (ownerSlot < haloShape.size())
      return haloShape[ownerSlot];
  }
  if (haloShape.size() == 1 && ownerSlot == 0)
    return haloShape.front();
  return std::nullopt;
}

static bool sameOwnerDimSet(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  if (lhs.size() != rhs.size())
    return false;
  SmallVector<int64_t, 4> lhsSorted(lhs.begin(), lhs.end());
  SmallVector<int64_t, 4> rhsSorted(rhs.begin(), rhs.end());
  llvm::sort(lhsSorted);
  llvm::sort(rhsSorted);
  return lhsSorted == rhsSorted;
}

static std::optional<SmallVector<int64_t, 4>>
expandHaloShapeToRootRank(ArrayRef<int64_t> haloShape,
                          ArrayRef<int64_t> ownerDims, unsigned rootRank) {
  if (haloShape.size() == rootRank)
    return SmallVector<int64_t, 4>(haloShape.begin(), haloShape.end());

  SmallVector<int64_t, 4> expanded(rootRank, 0);
  if (haloShape.size() == ownerDims.size()) {
    for (auto [slot, ownerDim] : llvm::enumerate(ownerDims)) {
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= rootRank)
        return std::nullopt;
      expanded[ownerDim] = haloShape[slot];
    }
    return expanded;
  }

  if (haloShape.size() == 1 && ownerDims.size() == 1) {
    int64_t ownerDim = ownerDims.front();
    if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= rootRank)
      return std::nullopt;
    expanded[ownerDim] = haloShape.front();
    return expanded;
  }

  return std::nullopt;
}

static bool projectRankExpandedHaloEdge(RedistributionEdge &edge,
                                        SdeSuIterateOp reader,
                                        MemRefType rootType,
                                        ArrayRef<int64_t> committedHaloShape,
                                        std::string &failReason) {
  std::optional<ExpandedBlockGridMu> expanded =
      recognizeExpandedBlockGridMu(reader, rootType);
  if (!expanded) {
    failReason = "rank-expanded halo redistribution is not a recognized "
                 "block-grid MU shape";
    return false;
  }

  // Expanded coordinate system: K leading grid dims (one per committed owner
  // dim, in canonical ascending owner order) then L logical tile dims. Under
  // C0/C1 the committed source/target owner dims equal `expanded->ownerDims`,
  // and each owner dim's structural coordinate is its grid slot i in [0,K).
  const unsigned numGrid = expanded->ownerDims.size();
  SmallVector<int64_t, 4> committedOwner;
  committedOwner.reserve(numGrid);
  for (unsigned od : expanded->ownerDims)
    committedOwner.push_back(static_cast<int64_t>(od));
  SmallVector<int64_t, 4> rawSourceOwner(edge.sourceOwnerDims.begin(),
                                         edge.sourceOwnerDims.end());
  if (!sameOwnerDimSet(edge.sourceOwnerDims, committedOwner)) {
    failReason = "rank-expanded halo edge owner dims do not match the "
                 "committed expanded owner grid";
    return false;
  }

  ArrayRef<int64_t> ownerHaloShape = committedHaloShape;
  SmallVector<int64_t, 4> projectedOwnerHalo;
  if (committedHaloShape.size() == rootType.getRank()) {
    projectedOwnerHalo.reserve(rawSourceOwner.size());
    for (int64_t ownerDim : rawSourceOwner) {
      if (ownerDim < 0 ||
          ownerDim >= static_cast<int64_t>(committedHaloShape.size())) {
        failReason = "rank-expanded halo redistribution has no recoverable "
                     "ghost width for a committed owner grid dim";
        return false;
      }
      projectedOwnerHalo.push_back(committedHaloShape[ownerDim]);
    }
    ownerHaloShape = projectedOwnerHalo;
  }

  // The home block over the EXPANDED root is a single block per grid step:
  // block extent 1 on each of the K grid dims, full tile extent on every tile
  // dim (the trailing in-block extents are preserved verbatim).
  ArrayRef<int64_t> shape = rootType.getShape();
  SmallVector<int64_t, 4> blockShape(shape.begin(), shape.end());
  for (unsigned i = 0; i < numGrid; ++i)
    blockShape[i] = 1;

  // The halo is a per-owner-dim neighbor-block face/slab: a reach of the
  // committed radius on that owner's grid dim, zero on every tile dim. A
  // committed owner dim with no recoverable radius has no face to place ->
  // precise fail-closed (not a blanket multi-owner reject).
  SmallVector<int64_t, 4> halo(rootType.getRank(), 0);
  for (unsigned i = 0; i < numGrid; ++i) {
    std::optional<int64_t> radius = getOwnerHaloRadius(
        ownerHaloShape, expanded->logicalRank, rawSourceOwner,
        /*ownerSlot=*/i, /*logicalOwnerDim=*/expanded->ownerDims[i]);
    if (!radius || *radius <= 0) {
      failReason = "rank-expanded halo redistribution has no recoverable ghost "
                   "width for a committed owner grid dim";
      return false;
    }
    halo[i] = *radius;
  }

  SmallVector<int64_t, 4> ownerGrid;
  ownerGrid.reserve(numGrid);
  for (unsigned i = 0; i < numGrid; ++i)
    ownerGrid.push_back(static_cast<int64_t>(i));

  edge.sourceOwnerDims = ownerGrid;
  edge.targetOwnerDims = ownerGrid;
  edge.sourceBlockShape.assign(blockShape.begin(), blockShape.end());
  edge.targetBlockShape.assign(blockShape.begin(), blockShape.end());
  edge.haloShape = std::move(halo);
  return true;
}

static bool logicalEndpointFitsRoot(const HomeLayout &home, MemRefType muType) {
  for (auto [slot, d] : llvm::enumerate(home.ownerDims)) {
    int64_t blockExtent =
        home.blockShape.size() == home.ownerDims.size()
            ? home.blockShape[slot]
            : (d >= 0 && d < static_cast<int64_t>(home.blockShape.size())
                   ? home.blockShape[d]
                   : 0);
    if (d < 0 || d >= muType.getRank() || blockExtent <= 0 ||
        blockExtent > muType.getShape()[d])
      return false;
  }
  return true;
}

static std::optional<RedistEndpoint>
getRankExpandedReductionEndpoint(const HomeLayout &home, MemRefType muType) {
  if (!muType || home.ownerDims.empty())
    return std::nullopt;
  std::optional<ExpandedBlockGridMu> expanded =
      recognizeExpandedBlockGridMu(home.writer, muType);
  if (!expanded)
    return std::nullopt;

  llvm::DenseSet<int64_t> homeOwners;
  for (int64_t ownerDim : home.ownerDims)
    homeOwners.insert(ownerDim);

  SmallVector<unsigned, 4> projectedGridSlots;
  for (auto [slot, ownerDim] : llvm::enumerate(expanded->ownerDims))
    if (homeOwners.contains(static_cast<int64_t>(ownerDim)))
      projectedGridSlots.push_back(static_cast<unsigned>(slot));
  if (projectedGridSlots.empty() ||
      projectedGridSlots.size() != homeOwners.size())
    return std::nullopt;

  // Expanded reduction endpoint: home-owned logical dims project to their
  // leading grid slots; other expanded dims stay full-width in each block.
  const unsigned numGrid = expanded->ownerDims.size();
  RedistEndpoint endpoint;
  endpoint.ownerDims.reserve(projectedGridSlots.size());
  endpoint.blockShape.reserve(muType.getRank());
  ArrayRef<int64_t> shape = muType.getShape();
  for (unsigned i = 0; i < numGrid; ++i) {
    if (llvm::is_contained(projectedGridSlots, i)) {
      endpoint.ownerDims.push_back(static_cast<int64_t>(i));
      endpoint.blockShape.push_back(1);
    } else {
      endpoint.blockShape.push_back(shape[i]);
    }
  }
  for (unsigned d = 0; d < expanded->logicalRank; ++d)
    endpoint.blockShape.push_back(shape[numGrid + d]);
  return endpoint;
}

static ArrayRef<int64_t> committedBlockShape(const LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty() ? ArrayRef<int64_t>(fact.blockShape)
                                       : ArrayRef<int64_t>(fact.budgetBlockShape);
}

static std::string repartitionMovementReplacement(
    const HomeLayout &home, const LayoutGraphFact &readerFact) {
  if (home.ownerDims.empty() && !readerFact.ownerDims.empty())
    return "sde.su_broadcast";
  if (!home.ownerDims.empty() && readerFact.ownerDims.empty())
    return "sde.su_gather";
  if (!sameOwnerDimSet(home.ownerDims, readerFact.ownerDims))
    return "sde.su_all_to_all";
  return "a future first-class SU movement op";
}

static void commitConsumerTargetGeometry(RedistributionEdge &edge,
                                         const LayoutGraphFact &readerFact) {
  ArrayRef<int64_t> targetBlock = committedBlockShape(readerFact);
  edge.targetOwnerDims.assign(readerFact.ownerDims.begin(),
                              readerFact.ownerDims.end());
  edge.targetBlockShape.assign(targetBlock.begin(), targetBlock.end());
}

static void commitOwnerPreservingTarget(RedistributionEdge &edge) {
  edge.targetOwnerDims.assign(edge.sourceOwnerDims.begin(),
                              edge.sourceOwnerDims.end());
  edge.targetBlockShape.assign(edge.sourceBlockShape.begin(),
                              edge.sourceBlockShape.end());
}

static std::optional<RedistEndpoint>
getRankExpandedFullEndpoint(const HomeLayout &home, MemRefType muType) {
  if (!muType)
    return std::nullopt;
  std::optional<ExpandedBlockGridMu> expanded =
      recognizeExpandedBlockGridMu(home.writer, muType);
  if (!expanded)
    return std::nullopt;

  RedistEndpoint endpoint;
  ArrayRef<int64_t> shape = muType.getShape();
  unsigned numGrid = expanded->ownerDims.size();
  endpoint.ownerDims.reserve(numGrid);
  endpoint.blockShape.reserve(muType.getRank());
  for (unsigned i = 0; i < numGrid; ++i) {
    endpoint.ownerDims.push_back(static_cast<int64_t>(i));
    endpoint.blockShape.push_back(1);
  }
  for (unsigned d = 0; d < expanded->logicalRank; ++d)
    endpoint.blockShape.push_back(shape[numGrid + d]);
  return endpoint;
}

static void collectReaderRedistributionCandidates(
    SdeSuIterateOp reader, llvm::SmallVector<int64_t, 4> &arrayIds) {
  llvm::DenseSet<int64_t> seen;
  if (reader.getBody().empty())
    return;
  for (SdeArrayLayoutRootOp root :
       reader.getBody().front().getOps<SdeArrayLayoutRootOp>()) {
    if (root.getMode() != SdeAccessMode::read)
      continue;
    int64_t id = root.getArrayId();
    if (seen.insert(id).second)
      arrayIds.push_back(id);
  }
  if (ArrayAttr layout = reader.getArrayLayoutAttr()) {
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout)) {
      if (fact.role != LayoutGraphRole::read || fact.commVolumeBytes <= 0)
        continue;
      if (seen.insert(fact.id).second)
        arrayIds.push_back(fact.id);
    }
  }
}

static bool producerConsumerMuTypesDisagree(Value writerRoot, Value readerRoot) {
  writerRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(writerRoot);
  readerRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(readerRoot);
  if (!writerRoot || !readerRoot)
    return false;
  auto writerType = dyn_cast<MemRefType>(writerRoot.getType());
  auto readerType = dyn_cast<MemRefType>(readerRoot.getType());
  if (!writerType || !readerType || !writerType.hasStaticShape() ||
      !readerType.hasStaticShape())
    return false;
  return writerType != readerType;
}

static bool readerCommittedLayoutDisagreesWithHome(
    const std::optional<LayoutGraphFact> &readerFact, const HomeLayout &home) {
  if (!readerFact || readerFact->role != LayoutGraphRole::read)
    return false;
  if (readerFact->commVolumeBytes > 0)
    return true;
  if (!sameOwnerDimSet(readerFact->ownerDims, home.ownerDims))
    return true;
  return committedBlockShape(*readerFact) !=
         ArrayRef<int64_t>(home.blockShape);
}

static bool readerNeedsRedistributionMovement(
    SdeSuIterateOp reader, int64_t arrayId, const HomeLayout &home,
    const ModuleSuAccessRelations &relations, std::optional<unsigned> readerSuId,
    Value root) {
  if (home.writer == reader)
    return false;

  std::optional<LayoutGraphFact> readerFact = findLayoutFact(reader, arrayId);
  if (readerCommittedLayoutDisagreesWithHome(readerFact, home))
    return true;

  if (getCommittedHaloShape(reader) && readerFact &&
      sameOwnerDimSet(home.ownerDims, readerFact->ownerDims))
    return true;

  if (readerSuId) {
    if (const ArrayAccessProfile *profile = findProfileForRoot(relations, root))
      for (auto [pos, posUses] : llvm::enumerate(profile->positionUses))
        for (const ArrayPositionUse &use : posUses)
          if (use.suId == *readerSuId && !use.isWrite &&
              use.kind == ArrayDimKind::reductionIndexed &&
              llvm::is_contained(home.ownerDims, static_cast<int64_t>(pos)))
            return true;
  }

  if (readerFact && readerFact->ownerDims == home.ownerDims &&
      reader.getPartialReductionAttr())
    return true;

  Value writerRoot =
      findArrayLayoutRoot(home.writer, arrayId, SdeAccessMode::write);
  Value readerRoot =
      findArrayLayoutRoot(reader, arrayId, SdeAccessMode::read);
  return producerConsumerMuTypesDisagree(writerRoot, readerRoot);
}

} // namespace

RedistributionEdges collectRedistributionEdges(Operation *moduleOp) {
  RedistributionEdges result;

  // Module access relations are used only to classify the consumer access
  // family. Root-to-arrayId grounding is explicit SDE provenance.
  ModuleSuAccessRelations relations = buildModuleSuAccessRelations(moduleOp);
  llvm::DenseMap<int64_t, Value> rootByArrayId;
  llvm::DenseSet<int64_t> conflictingRoots;
  auto recordRoot = [&](int64_t arrayId, Value root) {
    root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
    if (!root || arrayId < 0)
      return;
    auto [it, inserted] = rootByArrayId.try_emplace(arrayId, root);
    if (!inserted &&
        !::mlir::carts::ValueAnalysis::sameMemrefRoot(it->second, root))
      conflictingRoots.insert(arrayId);
  };
  moduleOp->walk([&](SdeArrayLayoutRootOp provenance) {
    recordRoot(provenance.getArrayId(), provenance.getRoot());
  });
  llvm::DenseMap<Operation *, unsigned> suIdOf;
  for (auto [i, su] : llvm::enumerate(relations.schedulingUnits))
    suIdOf[su.getOperation()] = i;

  // Committed home (writer-role) layout per arrayId, read verbatim.
  llvm::DenseMap<int64_t, HomeLayout> homeByArrayId;
  llvm::DenseSet<int64_t> conflictingHome;
  moduleOp->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const LayoutGraphFact &f : parseArrayLayoutFacts(layout)) {
      if (f.role != LayoutGraphRole::write)
        continue;
      HomeLayout home;
      home.writer = su;
      home.layoutKind = f.layoutKind;
      home.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
      // budgetBlockShape is the committed authority grain; the abstract
      // blockShape can be a coarse pre-distribution shape. Author the home (and
      // thus the redist) at the budget grain so reads/redist match the writer's
      // owner_block grain; use blockShape only when budget is absent
      // (non-budget kernels).
      ArrayRef<int64_t> homeBlock = f.budgetBlockShape.empty()
                                        ? ArrayRef<int64_t>(f.blockShape)
                                        : ArrayRef<int64_t>(f.budgetBlockShape);
      home.blockShape.assign(homeBlock.begin(), homeBlock.end());
      auto it = homeByArrayId.find(f.id);
      if (it == homeByArrayId.end())
        homeByArrayId[f.id] = std::move(home);
      else if (it->second.ownerDims != home.ownerDims ||
               it->second.blockShape != home.blockShape)
        conflictingHome.insert(f.id);
    }
  });

  moduleOp->walk([&](SdeSuIterateOp reader) {
    llvm::SmallVector<int64_t, 4> candidateArrayIds;
    collectReaderRedistributionCandidates(reader, candidateArrayIds);
    if (candidateArrayIds.empty())
      return;

    std::optional<unsigned> readerSuId;
    if (auto suIdIt = suIdOf.find(reader.getOperation()); suIdIt != suIdOf.end())
      readerSuId = suIdIt->second;

    for (int64_t arrayId : candidateArrayIds) {
      auto fail = [&](StringRef reason) {
        result.failures.push_back({reader, arrayId, reason.str()});
      };

      if (conflictingHome.contains(arrayId)) {
        fail("conflicting committed writer layouts; home is non-reconcilable");
        continue;
      }
      auto homeIt = homeByArrayId.find(arrayId);
      if (homeIt == homeByArrayId.end())
        continue;
      const HomeLayout &home = homeIt->second;
      if (home.ownerDims.empty()) {
        fail("committed home layout is replicated; no partitioned source to "
             "redistribute");
        continue;
      }
      auto rootIt = rootByArrayId.find(arrayId);
      if (conflictingRoots.contains(arrayId)) {
        fail("conflicting explicit array root provenance");
        continue;
      }
      if (rootIt == rootByArrayId.end()) {
        fail("cannot ground the redistribution edge to an explicit array root");
        continue;
      }
      Value root = rootIt->second;
      if (!readerNeedsRedistributionMovement(reader, arrayId, home, relations,
                                             readerSuId, root))
        continue;
      auto muType = dyn_cast<MemRefType>(root.getType());
      if (!muType || !muType.hasStaticShape()) {
        fail("dynamic array shape has no static redistribution layout");
        continue;
      }
      if (muRootHasUnsupportedUse(root)) {
        fail("aliasing array-root use blocks redistribution");
        continue;
      }

      // Movement family from the consumer's grounded access kind. Cross-owner
      // reductions and owner-preserving halos keep source==target geometry;
      // repartition edges take target from the consumer's committed read layout
      // when it differs from the module home layout.
      bool hasOwnerReduction = false;
      if (readerSuId) {
        if (const ArrayAccessProfile *profile =
                findProfileForRoot(relations, root))
          for (auto [pos, posUses] : llvm::enumerate(profile->positionUses))
            for (const ArrayPositionUse &use : posUses)
              if (use.suId == *readerSuId && !use.isWrite &&
                  use.kind == ArrayDimKind::reductionIndexed &&
                  llvm::is_contained(home.ownerDims, static_cast<int64_t>(pos)))
                hasOwnerReduction = true;
      }
      std::optional<LayoutGraphFact> readerFact =
          findLayoutFact(reader, arrayId);
      std::optional<SmallVector<int64_t, 4>> haloShape =
          getCommittedHaloShape(reader);
      std::optional<RedistEndpoint> expandedEndpoint =
          getRankExpandedReductionEndpoint(home, muType);
      bool geometryFitsRoot = logicalEndpointFitsRoot(home, muType);
      // Rank expansion can hide reduction accesses behind block-local div/mod
      // indexing; committed SDE layout/MU facts remain the stable source.
      if (!hasOwnerReduction && readerFact && !home.ownerDims.empty() &&
          !haloShape && readerFact->ownerDims == home.ownerDims &&
          reader.getPartialReductionAttr()) {
        if (geometryFitsRoot || expandedEndpoint)
          hasOwnerReduction = true;
        else {
          fail(
              "cross-owner reduction of a rank-expanded distributed "
              "intermediate is recognized but not yet realizable as sde.su_reduce_scatter");
          continue;
        }
      }
      if (hasOwnerReduction && !geometryFitsRoot && !expandedEndpoint) {
        fail("cross-owner reduction of a rank-expanded distributed "
             "intermediate is recognized but not yet realizable as sde.su_reduce_scatter");
        continue;
      }
      bool committedContractionLayout =
          home.layoutKind == ArrayLayoutKind::blockContraction ||
          (readerFact &&
           readerFact->layoutKind == ArrayLayoutKind::blockContraction);
      bool committedHaloLayout =
          haloShape && readerFact &&
          sameOwnerDimSet(home.ownerDims, readerFact->ownerDims);
      bool committedRepartitionLayout =
          readerFact && !committedHaloLayout &&
          (!sameOwnerDimSet(home.ownerDims, readerFact->ownerDims) ||
           committedBlockShape(*readerFact) !=
               ArrayRef<int64_t>(home.blockShape));
      if (committedContractionLayout && !geometryFitsRoot &&
          !expandedEndpoint) {
        fail("contraction redistribution of a rank-expanded distributed "
             "intermediate is not representable");
        continue;
      }
      if (!hasOwnerReduction && !committedContractionLayout) {
        if (committedRepartitionLayout) {
          // Genuine cross-owner repartition: emit an all_to_all edge instead of
          // failing closed once the SU movement op exists.
        } else if (!committedHaloLayout) {
          fail("redistribution edge is not a cross-owner reduction or halo; "
               "the consumer required-read layout is not committed or does "
               "not differ from the home layout");
          continue;
        }
      }

      RedistributionEdge edge;
      edge.root = root;
      edge.arrayId = arrayId;
      edge.consumer = reader;
      edge.kind =
          committedRepartitionLayout && !hasOwnerReduction &&
                  !committedContractionLayout
              ? RedistributionEdgeKind::AllToAll
          : committedHaloLayout && !hasOwnerReduction &&
                    !committedContractionLayout
                ? RedistributionEdgeKind::Halo
                : RedistributionEdgeKind::ReduceScatter;
      if ((hasOwnerReduction || committedContractionLayout) &&
          expandedEndpoint && !geometryFitsRoot) {
        edge.sourceOwnerDims.assign(expandedEndpoint->ownerDims.begin(),
                                    expandedEndpoint->ownerDims.end());
        edge.sourceBlockShape.assign(expandedEndpoint->blockShape.begin(),
                                     expandedEndpoint->blockShape.end());
      } else {
        edge.sourceOwnerDims.assign(home.ownerDims.begin(),
                                    home.ownerDims.end());
        edge.sourceBlockShape.assign(home.blockShape.begin(),
                                     home.blockShape.end());
      }
      if (edge.kind == RedistributionEdgeKind::Halo) {
        if (recognizeExpandedBlockGridMu(home.writer, muType)) {
          std::string haloFail;
          if (!projectRankExpandedHaloEdge(edge, home.writer, muType,
                                           *haloShape, haloFail)) {
            fail(haloFail);
            continue;
          }
        } else if (recognizeExpandedBlockGridMu(reader, muType)) {
          std::string haloFail;
          if (!projectRankExpandedHaloEdge(edge, reader, muType, *haloShape,
                                           haloFail)) {
            fail(haloFail);
            continue;
          }
        } else {
          std::optional<SmallVector<int64_t, 4>> expandedHalo =
              expandHaloShapeToRootRank(
                  *haloShape, edge.sourceOwnerDims,
                  static_cast<unsigned>(muType.getRank()));
          if (!expandedHalo) {
            fail("halo redistribution has no rank-length halo shape for the "
                 "grounded root");
            continue;
          }
          edge.haloShape = std::move(*expandedHalo);
        }
      }

      if (edge.kind == RedistributionEdgeKind::Halo ||
          edge.kind == RedistributionEdgeKind::ReduceScatter) {
        if (edge.targetOwnerDims.empty())
          commitOwnerPreservingTarget(edge);
      } else if (edge.kind == RedistributionEdgeKind::AllToAll) {
        if (readerFact)
          commitConsumerTargetGeometry(edge, *readerFact);
        else {
          fail("all_to_all redistribution edge has no committed consumer "
               "required-read layout");
          continue;
        }
      }

      // Committed abstract edge cost, if the reader carries one.
      if (ArrayAttr readerLayout = reader.getArrayLayoutAttr())
        for (const LayoutGraphFact &f : parseArrayLayoutFacts(readerLayout))
          if (f.id == arrayId && f.role == LayoutGraphRole::read &&
              f.commVolumeBytes > 0)
            edge.commVolumeBytes = f.commVolumeBytes;

      result.edges.push_back(std::move(edge));
    }
  });

  return result;
}

bool movementEndpointGroundedInCommittedLayout(
    Operation *scopeOp, Value movementRoot, int64_t arrayId,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape,
    bool allowExpandedFull, std::string &reason) {
  auto operandMuType = dyn_cast<MemRefType>(movementRoot.getType());
  movementRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(movementRoot);
  if (!movementRoot) {
    reason = "missing redistribution root";
    return false;
  }
  if (!scopeOp) {
    reason = "not inside a verification scope";
    return false;
  }

  Value provenanceRoot;
  bool conflictingRoot = false;
  auto recordRoot = [&](Value root) {
    root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
    if (!root)
      return;
    if (!provenanceRoot) {
      provenanceRoot = root;
      return;
    }
    if (!::mlir::carts::ValueAnalysis::sameMemrefRoot(provenanceRoot, root))
      conflictingRoot = true;
  };
  scopeOp->walk([&](SdeArrayLayoutRootOp provenance) {
    if (static_cast<int64_t>(provenance.getArrayId()) == arrayId)
      recordRoot(provenance.getRoot());
  });
  if (conflictingRoot) {
    reason = "conflicting explicit array root provenance";
    return false;
  }
  if (!provenanceRoot) {
    reason = "missing explicit array root provenance";
    return false;
  }
  if (!::mlir::carts::ValueAnalysis::sameMemrefRoot(provenanceRoot,
                                                    movementRoot)) {
    reason = "redistribution root does not match committed array provenance";
    return false;
  }

  std::optional<HomeLayout> home;
  bool conflictingHome = false;
  scopeOp->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (layout) {
      for (const LayoutGraphFact &f : parseArrayLayoutFacts(layout)) {
        if (f.id != arrayId || f.role != LayoutGraphRole::write)
          continue;
        HomeLayout candidate;
        candidate.writer = su;
        candidate.layoutKind = f.layoutKind;
        candidate.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
        ArrayRef<int64_t> homeBlock = f.budgetBlockShape.empty()
                                          ? ArrayRef<int64_t>(f.blockShape)
                                          : ArrayRef<int64_t>(f.budgetBlockShape);
        candidate.blockShape.assign(homeBlock.begin(), homeBlock.end());
        if (!home) {
          home = std::move(candidate);
          continue;
        }
        if (home->ownerDims != candidate.ownerDims ||
            home->blockShape != candidate.blockShape)
          conflictingHome = true;
      }
    }
    if (home || conflictingHome || su.getBody().empty())
      return;
    for (SdeArrayLayoutRootOp root :
         su.getBody().front().getOps<SdeArrayLayoutRootOp>()) {
      if (root.getMode() != SdeAccessMode::write ||
          static_cast<int64_t>(root.getArrayId()) != arrayId)
        continue;
      if (std::optional<HomeLayout> candidate = homeLayoutFromCommittedPhysical(su)) {
        if (!home) {
          home = std::move(*candidate);
          continue;
        }
        if (home->ownerDims != candidate->ownerDims ||
            home->blockShape != candidate->blockShape)
          conflictingHome = true;
      }
    }
  });
  if (conflictingHome) {
    reason = "conflicting committed writer layouts";
    return false;
  }
  if (!home) {
    reason = "missing committed writer layout";
    return false;
  }

  auto muType = operandMuType;
  if (!muType || !muType.hasStaticShape())
    return true;

  bool sourceMatchesHome =
      ownerDims == ArrayRef<int64_t>(home->ownerDims) &&
      blockShape == ArrayRef<int64_t>(home->blockShape);
  bool sourceMatchesExpandedReduction = false;
  if (std::optional<RedistEndpoint> expanded =
          getRankExpandedReductionEndpoint(*home, muType))
    sourceMatchesExpandedReduction =
        ownerDims == ArrayRef<int64_t>(expanded->ownerDims) &&
        blockShape == ArrayRef<int64_t>(expanded->blockShape);
  bool sourceMatchesExpandedFull = false;
  if (allowExpandedFull)
    if (std::optional<RedistEndpoint> expanded =
            getRankExpandedFullEndpoint(*home, muType))
      sourceMatchesExpandedFull =
          ownerDims == ArrayRef<int64_t>(expanded->ownerDims) &&
          blockShape == ArrayRef<int64_t>(expanded->blockShape);

  if (!sourceMatchesHome && !sourceMatchesExpandedReduction &&
      !(allowExpandedFull && sourceMatchesExpandedFull)) {
    reason = "source geometry is not grounded in the committed writer layout";
    return false;
  }
  return true;
}

} // namespace mlir::carts::sde
