///==========================================================================///
/// File: RedistributionEdges.cpp
///
/// See RedistributionEdges.h.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
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
          readI64ArrayAttr(reader.getPhysicalHaloShapeAttr()))
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
  if (!sameOwnerDimSet(edge.sourceOwnerDims, committedOwner) ||
      !sameOwnerDimSet(edge.targetOwnerDims, committedOwner)) {
    failReason = "rank-expanded halo edge owner dims do not match the "
                 "committed expanded owner grid";
    return false;
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
        committedHaloShape, expanded->logicalRank, rawSourceOwner,
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
  moduleOp->walk([&](SdeMuAllocOp mu) {
    if (IntegerAttr arrayId = mu.getArrayIdAttr())
      recordRoot(arrayId.getInt(), mu.getMemref());
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
    std::optional<SmallVector<int64_t, 4>> disagree =
        readI64ArrayAttr(reader.getLayoutsDisagreeAttr());
    if (!disagree)
      return;
    auto suIdIt = suIdOf.find(reader.getOperation());

    for (int64_t arrayId : *disagree) {
      auto fail = [&](StringRef reason) {
        result.failures.push_back({reader, arrayId, reason.str()});
      };

      if (conflictingHome.contains(arrayId)) {
        fail("conflicting committed writer layouts; home is non-reconcilable");
        continue;
      }
      auto homeIt = homeByArrayId.find(arrayId);
      if (homeIt == homeByArrayId.end()) {
        fail("redistribution edge has no committed writer layout to "
             "redistribute from");
        continue;
      }
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
      auto muType = dyn_cast<MemRefType>(root.getType());
      if (!muType || !muType.hasStaticShape()) {
        fail("dynamic array shape has no static redistribution layout");
        continue;
      }
      if (muRootHasUnsupportedUse(root)) {
        fail("aliasing array-root use blocks redistribution");
        continue;
      }

      // Movement family from the consumer's grounded access kind. Only a
      // cross-OWNER reduction read is currently representable from committed
      // facts: the consumer reduces the home's owned axis, so the target is the
      // home block layout itself (the movement is the reduction, not a
      // re-layout) and no geometry is invented. A re-layout edge
      // (transpose/gather/halo), or a reduction on a non-owner axis, would need
      // the consumer's required target layout, which `sde-layout-assignment`
      // discards — those fail closed.
      bool hasOwnerReduction = false;
      if (suIdIt != suIdOf.end()) {
        if (const ArrayAccessProfile *profile =
                findProfileForRoot(relations, root))
          for (auto [pos, posUses] : llvm::enumerate(profile->positionUses))
            for (const ArrayPositionUse &use : posUses)
              if (use.suId == suIdIt->second && !use.isWrite &&
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
          (reader.getPartialReductionAttr() ||
           reader.getReductionStrategyAttr())) {
        if (geometryFitsRoot || expandedEndpoint)
          hasOwnerReduction = true;
        else {
          fail(
              "cross-owner reduction of a rank-expanded distributed "
              "intermediate is recognized but not yet realizable as sde.redist "
              "(the home block geometry does not fit the expanded root)");
          continue;
        }
      }
      if (hasOwnerReduction && !geometryFitsRoot && !expandedEndpoint) {
        fail("cross-owner reduction of a rank-expanded distributed "
             "intermediate is recognized but not yet realizable as sde.redist "
             "(the home block geometry does not fit the expanded root)");
        continue;
      }
      bool committedContractionLayout =
          home.layoutKind == ArrayLayoutKind::blockContraction ||
          (readerFact &&
           readerFact->layoutKind == ArrayLayoutKind::blockContraction);
      bool committedHaloLayout =
          haloShape && readerFact &&
          sameOwnerDimSet(home.ownerDims, readerFact->ownerDims);
      if (committedContractionLayout && !geometryFitsRoot &&
          !expandedEndpoint) {
        fail("contraction redistribution of a rank-expanded distributed "
             "intermediate is not representable");
        continue;
      }
      if (!hasOwnerReduction && !committedContractionLayout) {
        if (!committedHaloLayout) {
          fail("redistribution edge is not a cross-owner reduction or halo; "
               "the target layout would have to be invented");
          continue;
        }
      }

      RedistributionEdge edge;
      edge.root = root;
      edge.arrayId = arrayId;
      edge.consumer = reader;
      edge.family = committedHaloLayout && !hasOwnerReduction &&
                            !committedContractionLayout
                        ? SdeMovementFamily::halo_like
                        : SdeMovementFamily::reduce_scatter_like;
      if ((hasOwnerReduction || committedContractionLayout) &&
          expandedEndpoint) {
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
      edge.targetOwnerDims = edge.sourceOwnerDims;
      edge.targetBlockShape = edge.sourceBlockShape;
      if (edge.family == SdeMovementFamily::halo_like) {
        if (recognizeExpandedBlockGridMu(reader, muType)) {
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

bool redistMatchesEdge(SdeRedistOp redist, const RedistributionEdge &edge) {
  if (redist.getMu() != edge.root || redist.getFamily() != edge.family)
    return false;
  IntegerAttr arrayId = redist.getArrayIdAttr();
  if (!arrayId || arrayId.getInt() != edge.arrayId)
    return false;
  std::optional<SmallVector<int64_t, 4>> so =
      readI64ArrayAttr(redist.getSourceOwnerDims());
  std::optional<SmallVector<int64_t, 4>> sb =
      readI64ArrayAttr(redist.getSourceBlockShape());
  std::optional<SmallVector<int64_t, 4>> to =
      readI64ArrayAttr(redist.getTargetOwnerDims());
  std::optional<SmallVector<int64_t, 4>> tb =
      readI64ArrayAttr(redist.getTargetBlockShape());
  std::optional<SmallVector<int64_t, 4>> halo =
      readI64ArrayAttr(redist.getHaloShapeAttr());
  return so && sb && to && tb &&
         ArrayRef<int64_t>(*so) == ArrayRef<int64_t>(edge.sourceOwnerDims) &&
         ArrayRef<int64_t>(*sb) == ArrayRef<int64_t>(edge.sourceBlockShape) &&
         ArrayRef<int64_t>(*to) == ArrayRef<int64_t>(edge.targetOwnerDims) &&
         ArrayRef<int64_t>(*tb) == ArrayRef<int64_t>(edge.targetBlockShape) &&
         (edge.family != SdeMovementFamily::halo_like ||
          (halo &&
           ArrayRef<int64_t>(*halo) == ArrayRef<int64_t>(edge.haloShape)));
}

bool redistGroundedInCommittedLayout(SdeRedistOp redist, std::string &reason) {
  IntegerAttr arrayIdAttr = redist.getArrayIdAttr();
  if (!arrayIdAttr) {
    reason = "missing array_id";
    return false;
  }
  int64_t arrayId = arrayIdAttr.getInt();
  Value redistRoot =
      ::mlir::carts::ValueAnalysis::stripMemrefViewOps(redist.getMu());
  if (!redistRoot) {
    reason = "missing redistribution root";
    return false;
  }

  ModuleOp module = redist->getParentOfType<ModuleOp>();
  if (!module) {
    reason = "not inside a module";
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
  module.walk([&](SdeArrayLayoutRootOp provenance) {
    if (static_cast<int64_t>(provenance.getArrayId()) == arrayId)
      recordRoot(provenance.getRoot());
  });
  module.walk([&](SdeMuAllocOp mu) {
    if (IntegerAttr id = mu.getArrayIdAttr(); id && id.getInt() == arrayId)
      recordRoot(mu.getMemref());
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
                                                    redistRoot)) {
    reason = "redistribution root does not match committed array provenance";
    return false;
  }

  std::optional<HomeLayout> home;
  bool conflictingHome = false;
  module.walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (!layout)
      return;
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
  });
  if (conflictingHome) {
    reason = "conflicting committed writer layouts";
    return false;
  }
  if (!home) {
    reason = "missing committed writer layout";
    return false;
  }

  auto muType = dyn_cast<MemRefType>(redistRoot.getType());
  if (!muType || !muType.hasStaticShape()) {
    reason = "redistribution root has no static memref shape";
    return false;
  }

  std::optional<SmallVector<int64_t, 4>> sourceOwner =
      readI64ArrayAttr(redist.getSourceOwnerDims());
  std::optional<SmallVector<int64_t, 4>> sourceBlock =
      readI64ArrayAttr(redist.getSourceBlockShape());
  std::optional<SmallVector<int64_t, 4>> targetOwner =
      readI64ArrayAttr(redist.getTargetOwnerDims());
  std::optional<SmallVector<int64_t, 4>> targetBlock =
      readI64ArrayAttr(redist.getTargetBlockShape());
  if (!sourceOwner || !sourceBlock || !targetOwner || !targetBlock) {
    reason = "redistribution geometry is not static";
    return false;
  }

  bool sourceMatchesHome =
      ArrayRef<int64_t>(*sourceOwner) == ArrayRef<int64_t>(home->ownerDims) &&
      ArrayRef<int64_t>(*sourceBlock) == ArrayRef<int64_t>(home->blockShape);
  bool sourceMatchesExpandedReduction = false;
  if (std::optional<RedistEndpoint> expanded =
          getRankExpandedReductionEndpoint(*home, muType))
    sourceMatchesExpandedReduction =
        ArrayRef<int64_t>(*sourceOwner) ==
            ArrayRef<int64_t>(expanded->ownerDims) &&
        ArrayRef<int64_t>(*sourceBlock) ==
            ArrayRef<int64_t>(expanded->blockShape);
  bool sourceMatchesExpandedFull = false;
  if (std::optional<RedistEndpoint> expanded =
          getRankExpandedFullEndpoint(*home, muType))
    sourceMatchesExpandedFull = ArrayRef<int64_t>(*sourceOwner) ==
                                    ArrayRef<int64_t>(expanded->ownerDims) &&
                                ArrayRef<int64_t>(*sourceBlock) ==
                                    ArrayRef<int64_t>(expanded->blockShape);

  if (!sourceMatchesHome && !sourceMatchesExpandedReduction &&
      !(redist.getFamily() == SdeMovementFamily::halo_like &&
        sourceMatchesExpandedFull)) {
    reason = "source geometry is not grounded in the committed writer layout";
    return false;
  }
  if (redist.getFamily() == SdeMovementFamily::reduce_scatter_like ||
      redist.getFamily() == SdeMovementFamily::halo_like) {
    if (ArrayRef<int64_t>(*sourceOwner) != ArrayRef<int64_t>(*targetOwner) ||
        ArrayRef<int64_t>(*sourceBlock) != ArrayRef<int64_t>(*targetBlock)) {
      reason = "target geometry does not match the committed source geometry";
      return false;
    }
  }
  return true;
}

} // namespace mlir::carts::sde
