///==========================================================================///
/// File: StringUtils.h
///
/// Utility queries for string-backed memrefs.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_UTILS_STRINGUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_STRINGUTILS_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseSet.h"

namespace mlir {
namespace carts::arts::StringUtils {

void collectStringMemRefs(ModuleOp module, DenseSet<Value> &stringMemRefs);
bool isStringMemRef(ModuleOp module, Value value);

} // namespace carts::arts::StringUtils
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_STRINGUTILS_H
