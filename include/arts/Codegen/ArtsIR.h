///==========================================================================
/// File: ArtsIR.h
///==========================================================================

#ifndef CARTS_CODEGEN_ARTSIR_H
#define CARTS_CODEGEN_ARTSIR_H

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsTypes.h"
#include "mlir/IR/Location.h"

namespace mlir {
namespace arts {

/// Event Operations
EventOp createEventOp(OpBuilder &builder, Location loc, ArrayRef<Value> sizes);

/// DataBlock Operations
DbAllocOp createDbAllocOp(OpBuilder &builder, Location loc, ArtsMode mode,
                          Value address = nullptr,
                          SmallVector<Value> sizes = {});
DbAcquireOp createDbAcquireOp(OpBuilder &builder, Location loc, ArtsMode mode,
                              Value source, SmallVector<Value> pinnedIndices,
                              SmallVector<Value> offsets = {},
                              SmallVector<Value> sizes = {});
DbReleaseOp createDbReleaseOp(OpBuilder &builder, Location loc,
                              Value source);
DbControlOp createDbControlOp(OpBuilder &builder, Location loc, ArtsMode mode,
                              Value ptr, SmallVector<Value> pinnedIndices = {});

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_ARTSIR_H