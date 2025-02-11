///==========================================================================
/// File: ArtsUtils.h
///==========================================================================

#ifndef MLIR_UTILS_ARTSUTILS_H
#define MLIR_UTILS_ARTSUTILS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/Operation.h"
#include <optional>

namespace mlir {
namespace arts {
void recursivelyRemoveOp(mlir::Operation *op);
void removeUndefOps(mlir::ModuleOp module);
void replaceWithUndef(mlir::Operation *op, OpBuilder &builder);
void replaceUses(mlir::Value from, mlir::Value to, DominanceInfo &domInfo,
                 Operation *dominatingOp);
void replaceInRegion(mlir::Region &region, mlir::Value from, mlir::Value to);
void replaceInRegion(mlir::Region &region, mlir::Value from, mlir::Value to,
                     SetVector<Value> &ignoreSet);
void replaceInRegion(mlir::Region &region,
                     llvm::DenseMap<mlir::Value, mlir::Value> &rewireMap,
                     SetVector<Value> &ignoreSet);
std::optional<int64_t> computeConstant(Value val);
int64_t tryParseIndexConstant(Value val);
} // namespace arts
} // namespace mlir

#endif // MLIR_UTILS_ARTSUTILS_H