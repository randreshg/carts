///==========================================================================///
/// File: SdeOps.cpp
/// Defines SDE dialect operation helpers and verifiers.
///==========================================================================///

#include "arts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/OpImplementation.h"

using namespace mlir;
using namespace mlir::arts::sde;

#define GET_OP_CLASSES
#include "arts/dialect/sde/IR/SdeOps.cpp.inc"
