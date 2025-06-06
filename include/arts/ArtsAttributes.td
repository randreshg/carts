///==========================================================================
/// File: ArtsAttrs.td
/// This file declares the ARTS dialect attributes.
///==========================================================================

#ifndef ARTS_ATTRS
#define ARTS_ATTRS

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/BuiltinAttributeInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "ArtsDialect.td"

//===----------------------------------------------------------------------===//
// ARTS Attributes
//===----------------------------------------------------------------------===//
class Arts_Attr<string attrName, string attrMnemonic, list<Trait> traits = []>
    : AttrDef<ArtsDialect, attrName, traits> {
  let mnemonic = attrMnemonic;
}

//===----------------------------------------------------------------------===//
// arts.domain
// A spatial domain attribute carrying:
//   - bounds: per-dimension [lower, upper] (IntAttr or SymbolRefAttr)
//   - symbols: SSA values used symbolically
//   - kind: "exact" or "bbox"
//===----------------------------------------------------------------------===//
def DomainAttr : Arts_Attr<"Domain", "domain"> {
  let summary = "Attribute encoding a memref's spatial domain";
  let description = [{
    The `arts.domain` attribute attaches to an `arts.db` op and
    encodes the exact spatial domain of the underlying memref. Fields:
      - `bounds`: ArrayAttr<[lower, upper]> per dimension, where
            each of `lower` and `upper` is either an `IntegerAttr`
            (constant) or `SymbolRefAttr` (loop‐IV or memref.dim).
      - `symbols`: ArrayAttr<SymbolRefAttr> listing the SSA symbols
            used in those bounds
      - `version`: IntegerAttr, for schema versioning (e.g. 1).
      - `kind`: StringAttr, either "exact" or "bbox".
  }];
  let parameters = (
    ins
      ArrayRefParameter<"Attribute"> : $bounds,
      ArrayRefParameter<"SymbolRefAttr"> : $symbols,
      StringRefParameter<""> : $kind
  );
  let assemblyFormat = "`<` `bounds` `=` $bounds `symbols` `=` $symbols `kind` `=` $kind `>`";
}


#endif // ARTS_ATTRS