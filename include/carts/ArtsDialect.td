//===- ARTSDialect.td - ARTS Dialect ------------------------------*- tablegen -*-===//
#ifndef ARTS_DIALECT
#define ARTS_DIALECT

include "mlir/IR/OpBase.td"

//===----------------------------------------------------------------------===//
// ARTS Dialect definition.
//===----------------------------------------------------------------------===//
def ARTS_Dialect : Dialect {
  let name = "arts";
  let cppNamespace = "::mlir::arts";
}

//===----------------------------------------------------------------------===//
// ARTS operation definition.
//===----------------------------------------------------------------------===//
class ARTS_Op<string mnemonic, list<Trait> traits = []>
    : Op<ARTS_Dialect, mnemonic, traits>;

#endif // ARTS_DIALECT