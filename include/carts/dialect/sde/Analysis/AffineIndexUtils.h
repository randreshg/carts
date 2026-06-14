///==========================================================================///
/// File: AffineIndexUtils.h
///
/// Shared reconstruction of affine index expressions from loop IVs and lowered
/// `memref` index arithmetic. Used by SDE layout analysis and ARTS boundary
/// owner-slot derivation (S4/S4b).
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_AFFINEINDEXUTILS_H
#define CARTS_DIALECT_SDE_ANALYSIS_AFFINEINDEXUTILS_H

#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include <optional>

namespace mlir {
namespace carts::sde {

/// Affine expression normalized to one loop dim plus a constant offset.
struct AffineDimOffset {
  std::optional<unsigned> dim;
  int64_t offset = 0;
};

/// Extract a single-dim + constant form from an affine expression.
std::optional<AffineDimOffset> extractDimOffset(AffineExpr expr);

/// Check whether an indexing map contains any non-zero constant stencil
/// offsets of the form `dim + c` where c != 0.
bool hasConstantOffsets(AffineMap map);

/// Best-effort affine expression for `value` over dispatch `ivs`.
std::optional<AffineExpr> tryGetAffineExpr(Value value, ArrayRef<Value> ivs,
                                             MLIRContext *ctx);

} // namespace carts::sde
} // namespace mlir

#endif
