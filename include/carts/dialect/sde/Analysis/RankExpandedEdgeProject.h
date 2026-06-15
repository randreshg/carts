///==========================================================================///
/// File: RankExpandedEdgeProject.h
///
/// SDE redistribution edge geometry projection over rank-expanded block-grid
/// MUs: home/endpoint recovery, halo-shape expansion, and the source/target
/// owner-grid + halo projection used to build RedistributionEdge geometry.
/// Carved verbatim from the correctness-base @782988ad1 RedistributionEdges
/// analysis.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_RANK_EXPANDED_EDGE_PROJECT_H
#define CARTS_DIALECT_SDE_ANALYSIS_RANK_EXPANDED_EDGE_PROJECT_H

#include "carts/dialect/sde/Analysis/EdgeClassify.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"

#include "mlir/IR/BuiltinTypes.h"

#include <cstdint>
#include <optional>
#include <string>

namespace mlir::carts::sde::redist {

struct RedistEndpoint {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

std::optional<HomeLayout> homeLayoutFromCommittedPhysical(SdeSuIterateOp su);

std::optional<int64_t> getOwnerHaloRadius(ArrayRef<int64_t> haloShape,
                                          unsigned logicalRank,
                                          ArrayRef<int64_t> ownerDims,
                                          unsigned ownerSlot,
                                          unsigned logicalOwnerDim);

std::optional<SmallVector<int64_t, 4>>
expandHaloShapeToRootRank(ArrayRef<int64_t> haloShape,
                          ArrayRef<int64_t> ownerDims, unsigned rootRank);

bool projectRankExpandedHaloEdge(RedistributionEdge &edge,
                                 SdeSuIterateOp reader, MemRefType rootType,
                                 ArrayRef<int64_t> committedHaloShape,
                                 std::string &failReason);

bool logicalEndpointFitsRoot(const HomeLayout &home, MemRefType muType);

std::optional<RedistEndpoint>
getRankExpandedReductionEndpoint(const HomeLayout &home, MemRefType muType);

std::string repartitionMovementReplacement(const HomeLayout &home,
                                           const LayoutGraphFact &readerFact);

void commitConsumerTargetGeometry(RedistributionEdge &edge,
                                  const LayoutGraphFact &readerFact);

void commitOwnerPreservingTarget(RedistributionEdge &edge);

std::optional<RedistEndpoint>
getRankExpandedFullEndpoint(const HomeLayout &home, MemRefType muType);

} // namespace mlir::carts::sde::redist

#endif // CARTS_DIALECT_SDE_ANALYSIS_RANK_EXPANDED_EDGE_PROJECT_H
