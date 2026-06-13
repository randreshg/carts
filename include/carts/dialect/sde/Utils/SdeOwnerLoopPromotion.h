#ifndef CARTS_DIALECT_SDE_UTILS_SDEOWNERLOOPPROMOTION_H
#define CARTS_DIALECT_SDE_UTILS_SDEOWNERLOOPPROMOTION_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/BuiltinOps.h"

namespace mlir::carts::sde {

/// Promote rank-1 `su_iterate` owner loops to rank-N when analysis proves it
/// safe (emit-as-structure). Returns the (possibly rebuilt) scheduling unit.
SdeSuIterateOp promoteSuIterateOwnerLoops(SdeSuIterateOp op);

/// Walk every `sde.su_iterate` under `moduleOp` and apply owner-loop promotion.
void promoteModuleOwnerLoops(ModuleOp moduleOp);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDEOWNERLOOPPROMOTION_H
