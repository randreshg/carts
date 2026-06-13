#ifndef CARTS_DIALECT_SDE_ANALYSIS_REDISTRIBUTIONEDGES_H
#define CARTS_DIALECT_SDE_ANALYSIS_REDISTRIBUTIONEDGES_H
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <string>
namespace mlir { class Operation; }
namespace mlir::carts::sde {
enum class RedistributionEdgeKind { Halo, ReduceScatter, AllToAll };
struct RedistributionEdge {
  Value root; int64_t arrayId = -1; SdeSuIterateOp consumer;
  RedistributionEdgeKind kind = RedistributionEdgeKind::ReduceScatter;
  SmallVector<int64_t, 4> sourceOwnerDims, sourceBlockShape;
  SmallVector<int64_t, 4> targetOwnerDims, targetBlockShape, haloShape;
};
struct RedistributionEdgeFailure { SdeSuIterateOp consumer; int64_t arrayId = -1; std::string reason; };
struct RedistributionEdges {
  SmallVector<RedistributionEdge, 4> edges;
  SmallVector<RedistributionEdgeFailure, 4> failures;
};
RedistributionEdges collectRedistributionEdges(Operation *moduleOp);
bool movementEndpointGroundedInCommittedLayout(
    Operation *scopeOp, Value movementRoot, int64_t arrayId,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> blockShape,
    bool allowExpandedFull, std::string &reason);
}
#endif
