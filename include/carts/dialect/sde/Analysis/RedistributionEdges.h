///==========================================================================///
/// File: RedistributionEdges.h
///
/// Committed SDE redistribution-edge query.
///
/// Detects producer/consumer layout disagreement from committed layout facts
/// and rank-expanded `mu_alloc` types, then resolves each edge into a
/// concrete redistribution fact that `sde-redistribute` consumes into
/// movement ops, or a fail-closed failure with evidence.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_REDISTRIBUTIONEDGES_H
#define CARTS_DIALECT_SDE_ANALYSIS_REDISTRIBUTIONEDGES_H

#include "carts/dialect/sde/IR/SdeDialect.h"

#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <string>

namespace mlir {
class Operation;
} // namespace mlir

namespace mlir::carts::sde {

/// One committed, legally representable redistribution edge.
struct RedistributionEdge {
  Value root;              ///< the array root being redistributed
  int64_t arrayId = -1;    ///< its module-stable arrayId
  SdeSuIterateOp consumer; ///< the disagreeing reader (diagnostic site)
  SdeMovementFamily family = SdeMovementFamily::phase_redist;
  SmallVector<int64_t, 4> sourceOwnerDims; ///< committed home owner dims
  SmallVector<int64_t, 4> sourceBlockShape;
  SmallVector<int64_t, 4> targetOwnerDims; ///< target determined by the family
  SmallVector<int64_t, 4> targetBlockShape;
  SmallVector<int64_t, 4> haloShape;
  int64_t commVolumeBytes = 0; ///< committed abstract edge cost (0 = none)
};

/// A committed disagreement edge that cannot be represented as legal
/// redistribution structure (dynamic / in-place / aliasing / no committed home
/// / no representable family). Evidence for a fail-closed diagnostic.
struct RedistributionEdgeFailure {
  SdeSuIterateOp consumer;
  int64_t arrayId = -1;
  std::string reason;
};

/// Representable edges + fail-closed failures under `moduleOp`.
struct RedistributionEdges {
  SmallVector<RedistributionEdge, 4> edges;
  SmallVector<RedistributionEdgeFailure, 4> failures;
};

/// Collect every committed redistribution edge, partitioned into representable
/// edges and fail-closed failures.
RedistributionEdges collectRedistributionEdges(Operation *moduleOp);

/// True iff `redist` faithfully represents `edge` (same root, family, and
/// source
/// + target geometry). Shared idempotence/grounding predicate.
bool redistMatchesEdge(SdeRedistOp redist, const RedistributionEdge &edge);

/// True iff an already-emitted `sde.redist` is grounded in committed SDE
/// writer layout/provenance.
bool redistGroundedInCommittedLayout(SdeRedistOp redist, std::string &reason);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_REDISTRIBUTIONEDGES_H
