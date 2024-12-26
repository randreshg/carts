#pragma once

#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/OpImplementation.h"

// Generated dialect and ops headers:
#include "arts/ARTSDialect.h.inc"   // from your Dialect.td
#include "arts/ARTSOpsEnums.h.inc"  // if you have enums in TableGen
#include "arts/ARTSOpsAttributes.h.inc" // if you have attr defns
#include "arts/ARTSOps.h.inc"       // from your Ops.td (auto-generated)

// This file typically declares any additional C++ classes or helper 
// methods for the ARTS ops, if you choose to do custom parse/print.

namespace mlir {
namespace arts {

// Forward declare operation classes if needed:
class EpochOp;
class SingleOp;
class ParallelOp;
class EdtOp;
class DataBlockCreateOp;

/// Example: Provide a build function or other utility for EdtOp.
void buildEdtOp(OpBuilder &builder, OperationState &state,
                ArrayAttr parameters, ArrayAttr dependencies);

} // end namespace arts
} // end namespace mlir