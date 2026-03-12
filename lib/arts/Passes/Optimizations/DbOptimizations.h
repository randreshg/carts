#ifndef CARTS_PASSES_OPTIMIZATIONS_DBOPTIMIZATIONS_H
#define CARTS_PASSES_OPTIMIZATIONS_DBOPTIMIZATIONS_H

#include "mlir/IR/BuiltinOps.h"

namespace mlir::arts {

bool eliminateEdtScratchDbs(ModuleOp module);

} // namespace mlir::arts

#endif
