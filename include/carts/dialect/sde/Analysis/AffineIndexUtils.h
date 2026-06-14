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

/// Best-effort affine expression for `value` over dispatch `ivs`.
std::optional<AffineExpr> tryGetAffineExpr(Value value, ArrayRef<Value> ivs,
                                             MLIRContext *ctx);

} // namespace carts::sde
} // namespace mlir

#endif
