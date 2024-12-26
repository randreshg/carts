//===- ARTSDialect.td - ARTS Dialect ------------------------------*- tablegen -*-===//
#ifndef ARTS_DIALECT
#define ARTS_DIALECT

include "mlir/IR/OpBase.td"

//===----------------------------------------------------------------------===//
// ARTS Dialect definition.
//===----------------------------------------------------------------------===//
def Arts_Dialect : Dialect {
  let name = "arts";
  let cppNamespace = "::mlir::arts";
}

//===----------------------------------------------------------------------===//
// ARTS operation definition.
//===---------------------------------`-------------------------------------===//
class Arts_Op<string mnemonic, list<Trait> traits = []>
    : Op<Arts_Dialect, mnemonic, traits>;

#endif // ARTS_DIALECT