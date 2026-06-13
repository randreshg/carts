///==========================================================================///
/// File: MovementLoweringUtils.h
///
/// Shared SDE movement → ARTS lowering helpers for the SDE-to-ARTS boundary.
/// Fact structs, owner-geometry predicates, and reusable acquire/copy emitters
/// live here so boundary passes stay orchestration-only.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_UTILS_MOVEMENTLOWERINGUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_MOVEMENTLOWERINGUTILS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace carts::arts {

struct HaloRedistFacts {
  ArrayAttr ownerDims;
  ArrayAttr blockShape;
  ArrayAttr haloShape;
};

struct ReduceScatterRedistFacts {
  IntegerAttr arrayId;
  ArrayAttr sourceOwnerDims;
  ArrayAttr sourceBlockShape;
  ArrayAttr targetOwnerDims;
  ArrayAttr targetBlockShape;
};

struct AllToAllRedistFacts {
  IntegerAttr arrayId;
  ArrayAttr sourceOwnerDims;
  ArrayAttr sourceBlockShape;
  ArrayAttr targetOwnerDims;
  ArrayAttr targetBlockShape;
};

FailureOr<ReduceScatterRedistFacts>
buildReduceScatterRedistFacts(sde::SdeSuReduceScatterOp reduce);

FailureOr<AllToAllRedistFacts>
buildAllToAllRedistFacts(sde::SdeSuAllToAllOp allToAll);

bool isOrderPreservingAllToAllOwnerDims(ArrayRef<int64_t> sourceOwner,
                                        ArrayRef<int64_t> targetOwner);

bool isPermutedAllToAllOwnerDims(ArrayRef<int64_t> sourceOwner,
                                 ArrayRef<int64_t> targetOwner);

FailureOr<sde::SdeSuIterateOp>
findAllToAllConsumerIterate(sde::SdeSuAllToAllOp allToAll);

FailureOr<Value> findAllToAllTargetMemref(sde::SdeSuAllToAllOp allToAll,
                                          sde::SdeSuIterateOp consumer);

LogicalResult emitAllToAllBlockCopy(
    OpBuilder &builder, Location loc, ArrayRef<Value> sourcePayloads,
    Value targetPayload, ArrayRef<int64_t> sourceBlockShape,
    ArrayRef<int64_t> targetBlockShape, ArrayRef<int64_t> targetBlockCoords,
    int64_t coveringSourceBlockBase, Type elementType);

LogicalResult convertAllToAllMovement(sde::SdeSuAllToAllOp allToAll);

LogicalResult realizeAllToAllMovements(ModuleOp module);

LogicalResult validateAndCollectStorageRedists(
    ModuleOp module, llvm::DenseMap<Value, HaloRedistFacts> &haloFacts,
    SmallVectorImpl<Operation *> &redists);

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_MOVEMENTLOWERINGUTILS_H
