///==========================================================================///
/// File: RankExpandedEdgeProject.cpp
///
/// SDE redistribution edge geometry projection over rank-expanded block-grid
/// MUs, plus the committed-layout endpoint grounding check
/// (movementEndpointGroundedInCommittedLayout). Logic carved verbatim from the
/// correctness-base @782988ad1 RedistributionEdges analysis.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RankExpandedEdgeProject.h"

#include "carts/dialect/sde/Analysis/EdgeClassify.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;

namespace mlir::carts::sde::redist {

std::optional<HomeLayout> homeLayoutFromCommittedPhysical(SdeSuIterateOp su) {
  std::optional<CommittedSuPhysicalLayout> committed =
      recoverCommittedPhysicalLayout(su);
  if (!committed || committed->ownerDims.empty() ||
      committed->blockShape.empty())
    return std::nullopt;
  HomeLayout home;
  home.writer = su;
  home.layoutKind = ArrayLayoutKind::blockParallel;
  home.ownerDims.assign(committed->ownerDims.begin(),
                        committed->ownerDims.end());
  home.blockShape.assign(committed->blockShape.begin(),
                         committed->blockShape.end());
  return home;
}

std::optional<int64_t> getOwnerHaloRadius(ArrayRef<int64_t> haloShape,
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

std::optional<SmallVector<int64_t, 4>>
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

bool projectRankExpandedHaloEdge(RedistributionEdge &edge,
                                 SdeSuIterateOp reader, MemRefType rootType,
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
    if (!radius || *radius < 0) {
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

bool logicalEndpointFitsRoot(const HomeLayout &home, MemRefType muType) {
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

std::optional<RedistEndpoint>
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

std::string repartitionMovementReplacement(const HomeLayout &home,
                                           const LayoutGraphFact &readerFact) {
  if (home.ownerDims.empty() && !readerFact.ownerDims.empty())
    return "sde.su_broadcast";
  if (!home.ownerDims.empty() && readerFact.ownerDims.empty())
    return "sde.su_gather";
  if (!sameOwnerDimSet(home.ownerDims, readerFact.ownerDims))
    return "sde.su_all_to_all";
  return "a future first-class SU movement op";
}

void commitConsumerTargetGeometry(RedistributionEdge &edge,
                                  const LayoutGraphFact &readerFact) {
  ArrayRef<int64_t> targetBlock = committedBlockShape(readerFact);
  edge.targetOwnerDims.assign(readerFact.ownerDims.begin(),
                              readerFact.ownerDims.end());
  edge.targetBlockShape.assign(targetBlock.begin(), targetBlock.end());
}

void commitOwnerPreservingTarget(RedistributionEdge &edge) {
  edge.targetOwnerDims.assign(edge.sourceOwnerDims.begin(),
                              edge.sourceOwnerDims.end());
  edge.targetBlockShape.assign(edge.sourceBlockShape.begin(),
                               edge.sourceBlockShape.end());
}

std::optional<RedistEndpoint>
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

} // namespace mlir::carts::sde::redist

namespace mlir::carts::sde {

bool movementEndpointGroundedInCommittedLayout(
    Operation *scopeOp, Value movementRoot, int64_t arrayId,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape,
    bool allowExpandedFull, std::string &reason) {
  auto operandMuType = dyn_cast<MemRefType>(movementRoot.getType());
  movementRoot = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(movementRoot);
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

  std::optional<redist::HomeLayout> home;
  bool conflictingHome = false;
  scopeOp->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (layout) {
      for (const LayoutGraphFact &f : parseArrayLayoutFacts(layout)) {
        if (f.id != arrayId || f.role != LayoutGraphRole::write)
          continue;
        redist::HomeLayout candidate;
        candidate.writer = su;
        candidate.layoutKind = f.layoutKind;
        candidate.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
        ArrayRef<int64_t> homeBlock =
            f.budgetBlockShape.empty() ? ArrayRef<int64_t>(f.blockShape)
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
      if (std::optional<redist::HomeLayout> candidate =
              redist::homeLayoutFromCommittedPhysical(su)) {
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

  bool sourceMatchesHome = ownerDims == ArrayRef<int64_t>(home->ownerDims) &&
                           blockShape == ArrayRef<int64_t>(home->blockShape);
  bool sourceMatchesExpandedReduction = false;
  if (std::optional<redist::RedistEndpoint> expanded =
          redist::getRankExpandedReductionEndpoint(*home, muType))
    sourceMatchesExpandedReduction =
        ownerDims == ArrayRef<int64_t>(expanded->ownerDims) &&
        blockShape == ArrayRef<int64_t>(expanded->blockShape);
  bool sourceMatchesExpandedFull = false;
  if (allowExpandedFull)
    if (std::optional<redist::RedistEndpoint> expanded =
            redist::getRankExpandedFullEndpoint(*home, muType))
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
