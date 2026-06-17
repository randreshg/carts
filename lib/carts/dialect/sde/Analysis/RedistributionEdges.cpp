///==========================================================================///
/// File: RedistributionEdges.cpp
///
/// See RedistributionEdges.h.
///
/// Orchestrator for the SDE redistribution edge analysis. Edge classification
/// (movement family + A1 same-owner fail-closed) lives in EdgeClassify; the
/// rank-expanded geometry projection and committed-layout endpoint grounding
/// live in RankExpandedEdgeProject. This file walks the module, grounds each
/// reader candidate to its committed home, and assembles the final edges by
/// calling those two libraries. Behavior is byte-identical to the correctness
/// base @782988ad1.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"

#include "carts/dialect/sde/Analysis/EdgeClassify.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RankExpandedEdgeProject.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include <numeric>

using namespace mlir;

namespace mlir::carts::sde {

using namespace mlir::carts::sde::redist;

static bool movementEndpointFitsRoot(MemRefType muType,
                                     ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> blockShape) {
  if (!muType || !muType.hasStaticShape() || ownerDims.empty() ||
      blockShape.empty())
    return false;
  int64_t rank = muType.getRank();
  ArrayRef<int64_t> shape = muType.getShape();
  if (static_cast<int64_t>(blockShape.size()) == rank) {
    SmallVector<bool, 4> isOwner(rank, false);
    for (int64_t d : ownerDims) {
      if (d < 0 || d >= rank || isOwner[d])
        return false;
      isOwner[d] = true;
    }
    for (int64_t d = 0; d < rank; ++d) {
      if (blockShape[d] <= 0 || blockShape[d] > shape[d])
        return false;
      if (!isOwner[d] && blockShape[d] != shape[d])
        return false;
    }
    return true;
  }
  if (static_cast<int64_t>(blockShape.size()) !=
      static_cast<int64_t>(ownerDims.size()))
    return false;
  for (auto [i, d] : llvm::enumerate(ownerDims)) {
    if (blockShape[i] <= 0 || blockShape[i] > shape[d])
      return false;
  }
  return true;
}

static bool isAllZero(ArrayRef<int64_t> values) {
  return llvm::all_of(values, [](int64_t value) { return value == 0; });
}

static ArrayRef<int64_t> committedLayoutGrain(const LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty()
             ? ArrayRef<int64_t>(fact.blockShape)
             : ArrayRef<int64_t>(fact.budgetBlockShape);
}

static void unifyHomeBlockShapeFromCommittedFacts(
    Operation *moduleOp, llvm::DenseMap<int64_t, HomeLayout> &homeByArrayId) {
  struct GrainState {
    SmallVector<int64_t, 4> ownerDims;
    SmallVector<int64_t, 4> unified;
    bool seen = false;
    bool eligible = true;
  };
  llvm::DenseMap<int64_t, GrainState> byId;
  moduleOp->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const LayoutGraphFact &fact : parseArrayLayoutFacts(layout)) {
      if (fact.id < 0 || fact.layoutKind != ArrayLayoutKind::blockParallel ||
          fact.ownerDims.empty())
        continue;
      ArrayRef<int64_t> grain = committedLayoutGrain(fact);
      if (grain.empty()) {
        byId[fact.id].eligible = false;
        continue;
      }
      GrainState &state = byId[fact.id];
      if (!state.seen) {
        state.seen = true;
        state.ownerDims.assign(fact.ownerDims.begin(), fact.ownerDims.end());
        state.unified.assign(grain.begin(), grain.end());
        continue;
      }
      if (ArrayRef<int64_t>(state.ownerDims) !=
              ArrayRef<int64_t>(fact.ownerDims) ||
          state.unified.size() != grain.size()) {
        state.eligible = false;
        continue;
      }
      for (size_t i = 0; i < grain.size(); ++i) {
        int64_t lhs = state.unified[i];
        int64_t rhs = grain[i];
        if (lhs <= 0)
          state.unified[i] = rhs;
        else if (rhs <= 0)
          continue;
        else if (lhs == rhs)
          continue;
        else {
          int64_t lo = std::min(lhs, rhs);
          int64_t hi = std::max(lhs, rhs);
          state.unified[i] = (hi % lo == 0) ? hi : std::gcd(lhs, rhs);
        }
      }
    }
  });
  for (auto &entry : homeByArrayId) {
    auto it = byId.find(entry.first);
    if (it == byId.end() || !it->second.eligible || !it->second.seen)
      continue;
    entry.second.blockShape.assign(it->second.unified.begin(),
                                   it->second.unified.end());
  }
}

static SdeSuIterateOp findStoreProducer(Operation *moduleOp, Value root) {
  root = ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root);
  if (!moduleOp || !root)
    return SdeSuIterateOp();
  SdeSuIterateOp producer;
  moduleOp->walk([&](memref::StoreOp store) {
    if (producer)
      return WalkResult::interrupt();
    if (::mlir::carts::ValueAnalysis::stripMemrefViewOps(store.getMemref()) !=
        root)
      return WalkResult::advance();
    producer = store->getParentOfType<SdeSuIterateOp>();
    return producer ? WalkResult::interrupt() : WalkResult::advance();
  });
  return producer;
}

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
  moduleOp->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const LayoutGraphFact &f : parseArrayLayoutFacts(layout)) {
      if (f.role != LayoutGraphRole::read ||
          f.layoutKind != ArrayLayoutKind::blockParallel ||
          f.ownerDims.empty() || f.blockShape.empty() || f.id < 0)
        continue;
      if (homeByArrayId.contains(f.id) || conflictingHome.contains(f.id))
        continue;
      Value root = findArrayLayoutRoot(su, f.id, SdeAccessMode::read);
      SdeSuIterateOp producer = findStoreProducer(moduleOp, root);
      if (!producer)
        continue;
      HomeLayout home;
      home.writer = producer;
      home.layoutKind = f.layoutKind;
      home.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
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
  unifyHomeBlockShapeFromCommittedFacts(moduleOp, homeByArrayId);

  moduleOp->walk([&](SdeSuIterateOp reader) {
    llvm::SmallVector<int64_t, 4> candidateArrayIds;
    collectReaderRedistributionCandidates(reader, candidateArrayIds);
    if (candidateArrayIds.empty())
      return;

    std::optional<unsigned> readerSuId;
    if (auto suIdIt = suIdOf.find(reader.getOperation());
        suIdIt != suIdOf.end())
      readerSuId = suIdIt->second;

    for (int64_t arrayId : candidateArrayIds) {
      auto fail = [&](StringRef reason) {
        result.failures.push_back({reader, arrayId, reason.str()});
      };

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
      auto muType = dyn_cast<MemRefType>(root.getType());
      std::optional<LayoutGraphFact> readerFact =
          findLayoutFact(reader, arrayId);
      bool readerMatchesSelectedHome = false;
      if (readerFact) {
        ArrayRef<int64_t> readerGrain = committedLayoutGrain(*readerFact);
        readerMatchesSelectedHome =
            readerFact->ownerDims == home.ownerDims &&
            readerGrain == ArrayRef<int64_t>(home.blockShape);
      }
      std::optional<SmallVector<int64_t, 4>> haloShape =
          getCommittedHaloShape(reader);
      if (!haloShape && readerFact &&
          sameOwnerDimSet(home.ownerDims, readerFact->ownerDims) && muType &&
          muType.hasStaticShape()) {
        haloShape = recoverRankExpandedOwnerHaloFromLoads(
            root, reader, home.ownerDims, muType);
        if (haloShape && isAllZero(*haloShape))
          haloShape.reset();
      }
      bool needsMovement = readerNeedsRedistributionMovement(
          reader, arrayId, home, relations, readerSuId, root);
      if (conflictingHome.contains(arrayId) && !readerMatchesSelectedHome) {
        fail("conflicting committed writer layouts; home is non-reconcilable");
        continue;
      }
      if (!needsMovement &&
          !(haloShape && readerFact &&
            sameOwnerDimSet(home.ownerDims, readerFact->ownerDims)))
        continue;
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
          fail("cross-owner reduction of a rank-expanded distributed "
               "intermediate is recognized but not yet realizable as "
               "sde.su_reduce_scatter");
          continue;
        }
      }
      if (hasOwnerReduction && !geometryFitsRoot && !expandedEndpoint) {
        fail("cross-owner reduction of a rank-expanded distributed "
             "intermediate is recognized but not yet realizable as "
             "sde.su_reduce_scatter");
        continue;
      }
      bool committedContractionLayout =
          home.layoutKind == ArrayLayoutKind::blockContraction ||
          (readerFact &&
           readerFact->layoutKind == ArrayLayoutKind::blockContraction);
      bool committedHaloLayout =
          haloShape && readerFact &&
          sameOwnerDimSet(home.ownerDims, readerFact->ownerDims);
      EdgeClassify edgeClass = classifyRedistributionEdge(
          home, readerFact, hasOwnerReduction, committedContractionLayout,
          committedHaloLayout);
      if (committedContractionLayout && !geometryFitsRoot &&
          !expandedEndpoint) {
        fail("contraction redistribution of a rank-expanded distributed "
             "intermediate is not representable");
        continue;
      }
      if (!hasOwnerReduction && !committedContractionLayout) {
        if (edgeClass == EdgeClassify::AllToAll) {
          // Genuine cross-owner repartition: emit an all_to_all edge instead of
          // failing closed once the SU movement op exists.
        } else if (edgeClass == EdgeClassify::None &&
                   readerOnlyRetilesSameOwners(readerFact, home)) {
          fail("same-owner block-grain mismatch must be reconciled before "
               "redistribution; refusing to emit degenerate all_to_all");
          continue;
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
      edge.kind = edgeClass == EdgeClassify::AllToAll
                      ? RedistributionEdgeKind::AllToAll
                  : edgeClass == EdgeClassify::Halo
                      ? RedistributionEdgeKind::Halo
                      : RedistributionEdgeKind::ReduceScatter;
      bool needsExpandedEndpoint =
          expandedEndpoint &&
          (!geometryFitsRoot ||
           home.blockShape.size() != static_cast<size_t>(muType.getRank()));
      if ((hasOwnerReduction || committedContractionLayout) &&
          needsExpandedEndpoint) {
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
          if (!projectRankExpandedHaloEdge(edge, home.writer, reader, muType,
                                           *haloShape, haloFail)) {
            fail(haloFail);
            continue;
          }
        } else if (recognizeExpandedBlockGridMu(reader, muType)) {
          std::string haloFail;
          if (!projectRankExpandedHaloEdge(edge, reader, reader, muType,
                                           *haloShape, haloFail)) {
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
        if (isAllZero(edge.haloShape))
          continue;
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
        auto muType = dyn_cast<MemRefType>(root.getType());
        if (!muType ||
            !movementEndpointFitsRoot(muType, edge.sourceOwnerDims,
                                      edge.sourceBlockShape) ||
            !movementEndpointFitsRoot(muType, edge.targetOwnerDims,
                                      edge.targetBlockShape)) {
          fail("all_to_all redistribution geometry is not representable on "
               "the grounded root");
          continue;
        }
      }

      result.edges.push_back(std::move(edge));
    }
  });

  return result;
}

} // namespace mlir::carts::sde
