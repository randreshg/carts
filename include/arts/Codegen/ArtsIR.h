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
EdtOp createEdtOp(OpBuilder &builder, Location loc, types::EdtType type,
                  SmallVector<Value> deps = {});
types::EdtType getEdtType(EdtOp edtOp);

DbCreateOp createDbCreateOp(OpBuilder &builder, Location loc,
                            StringRef mode, Value address, 
                            SmallVector<Value> sizes = {});

DbControlOp createDbControlOp(OpBuilder &builder, Location loc,
                              types::DbAccessType mode, Value ptr,
                              SmallVector<Value> pinnedIndices = {}, bool coarseGrained = false);
} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_ARTSIR_H