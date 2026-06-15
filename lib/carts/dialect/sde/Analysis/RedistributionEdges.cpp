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

using namespace mlir;

namespace mlir::carts::sde {

using namespace mlir::carts::sde::redist;

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
    if (auto suIdIt = suIdOf.find(reader.getOperation());
        suIdIt != suIdOf.end())
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

      result.edges.push_back(std::move(edge));
    }
  });

  return result;
}

} // namespace mlir::carts::sde
