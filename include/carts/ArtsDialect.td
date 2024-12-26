//===- ARTSDialect.td - ARTS Dialect ------------------------------*- tablegen -*-===//
#ifndef ARTS_DIALECT
#define ARTS_DIALECT

#include "mlir/IR/Dialect.h.td"

// The ARTS dialect
def ARTS_Dialect : Dialect {
  let name = "arts";
  let cppNamespace = "mlir::arts";
}

#endif // ARTS_DIALECT