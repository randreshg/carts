///===----------------------------------------------------------------------===///
/// File: MemrefNormalizationInternal.h
///
/// Private implementation boundary for SdeMemrefNormalization. This header is
/// intentionally scoped to Polygeist-to-SDE memref normalization support.
///===----------------------------------------------------------------------===///

#ifndef CARTS_DIALECT_SDE_CONVERSION_POLYGEISTTOSDE_MEMREFNORMALIZATIONINTERNAL_H
#define CARTS_DIALECT_SDE_CONVERSION_POLYGEISTTOSDE_MEMREFNORMALIZATIONINTERNAL_H

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"

namespace mlir::carts::sde::memref_normalization {

Operation *findNearestLoop(Operation *op);

Value getForwardedMemrefAliasSource(Value value);
Value getForwardedMemrefAliasResult(Operation *user, Value current);
bool isMemrefContainerValue(Value value);

bool allocEscapesIf(memref::AllocOp allocOp, scf::IfOp parentIf);

Value traceWrapperLoadToAlloc(Value val);
bool isInnerWrapperOfInlinedPattern(Value alloc);

} // namespace mlir::carts::sde::memref_normalization

#endif // CARTS_DIALECT_SDE_CONVERSION_POLYGEISTTOSDE_MEMREFNORMALIZATIONINTERNAL_H
