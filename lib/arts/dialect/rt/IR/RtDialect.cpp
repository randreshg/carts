///==========================================================================///
/// File: RtDialect.cpp
/// Defines the ARTS runtime dialect.
///==========================================================================///

#include "arts/dialect/rt/IR/RtDialect.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::arts::rt;

void ArtsRtDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "arts/dialect/rt/IR/RtOps.cpp.inc"
      >();

  addAttributes<
#define GET_ATTRDEF_LIST
#include "arts/dialect/rt/IR/RtOpsAttributes.cpp.inc"
      >();
}

#include "arts/dialect/rt/IR/RtOpsDialect.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "arts/dialect/rt/IR/RtOpsAttributes.cpp.inc"

#include "arts/dialect/rt/IR/RtOpsEnums.cpp.inc"
