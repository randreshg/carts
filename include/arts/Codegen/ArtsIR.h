///==========================================================================
/// File: ArtsIR.h
///==========================================================================

#ifndef MLIR_CODEGEN_ARTSIR_H
#define MLIR_CODEGEN_ARTSIR_H

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsTypes.h"
#include "mlir/IR/Location.h"

namespace mlir {
namespace arts {
EdtOp createEdtOp(OpBuilder &builder, Location loc, types::EdtType type,
                  SmallVector<Value> deps = {});
types::EdtType getEdtType(EdtOp edtOp);

DataBlockOp createDatablockOp(OpBuilder &builder, Location loc,
                              types::DatablockAccessType mode, Value ptr,
                              SmallVector<Value> pinnedIndices = {}, bool singleSize = false);
} // namespace arts
} // namespace mlir

#endif // MLIR_CODEGEN_ARTSIR_H