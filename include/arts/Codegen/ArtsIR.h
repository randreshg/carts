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

/// EDT creation and manipulation functions
EdtOp createEdtOp(OpBuilder &builder, Location loc, types::EdtType type,
                  SmallVector<Value> deps = {});
types::EdtType getEdtType(EdtOp edtOp);

/// DB creation functions
DbAllocOp createDbAllocOp(OpBuilder &builder, Location loc,
                            StringRef mode, Value address = nullptr, 
                            SmallVector<Value> sizes = {});

DbDepOp createDbDepOp(OpBuilder &builder, Location loc,
                   types::DbDepType mode, Value source,
                   SmallVector<Value> pinnedIndices,
                   SmallVector<Value> offsets,
                   SmallVector<Value> sizes);

/// DB helper functions
bool isDbAllocOp(Operation *op);
bool isDbDepOp(Operation *op);
bool isStackAlloc(DbAllocOp dbAllocOp);
bool isDynamicAlloc(DbAllocOp dbAllocOp);
bool isGlobalAlloc(DbAllocOp dbAllocOp);

/// DB attribute helpers
types::DbAllocType getDbAllocType(DbAllocOp dbAllocOp);
void setDbAllocType(DbAllocOp dbAllocOp, types::DbAllocType type, OpBuilder &builder);

/// Allocation type analysis
types::DbAllocType getAllocType(Value ptr);

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_ARTSIR_H