#ifndef CARTS_DIALECT_SDE_UTILS_SDECUNORMALIZATIONUTILS_H
#define CARTS_DIALECT_SDE_UTILS_SDECUNORMALIZATIONUTILS_H

#include "mlir/IR/BuiltinOps.h"

namespace mlir::carts::sde {

/// Wrap residual source-executable work into conservative `sde.cu_region
/// <single>` containers and normalize `sde.su_iterate` bodies to leaf CUs.
/// Returns false when a span cannot be safely wrapped (caller should fail closed).
bool normalizeSdeCuStructure(mlir::ModuleOp module);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECUNORMALIZATIONUTILS_H
