///==========================================================================
/// File: ArtsAttrs.td
/// This file declares the ARTS dialect attributes.
///==========================================================================

#ifndef ARTS_ATTRS
#define ARTS_ATTRS

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/BuiltinAttributeInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "ArtsDialect.td"

//===----------------------------------------------------------------------===//
// ARTS Enum Helpers
//===----------------------------------------------------------------------===//

class Arts_I32Enum<string name, string description, list<I32EnumAttrCase> cases>
    : I32EnumAttr<name, description, cases> {
  let genSpecializedAttr = 0;
  let cppNamespace = "::mlir::arts";
}
class Arts_I32EnumAttr<string mnemonic, Arts_I32Enum enumInfo> :
    EnumAttr<ArtsDialect, enumInfo, mnemonic> {
  let assemblyFormat = "`<` $value `>`";
}

//===----------------------------------------------------------------------===//
// ARTS Attributes
//===----------------------------------------------------------------------===//
class Arts_Attr<string attrName, string attrMnemonic, list<Trait> traits = []>
    : AttrDef<ArtsDialect, attrName, traits> {
  let mnemonic = attrMnemonic;
}

// Enum info for ARTS access modes (in, out, inout)
def ArtsMode : Arts_I32Enum<"ArtsMode",
    "ARTS DataBlock access mode",
    [
      I32EnumAttrCase<"in", 0, "Read-only access">,
      I32EnumAttrCase<"out", 1, "Write-only access">,
      I32EnumAttrCase<"inout", 2, "Read-write access">
    ]>;

// Enum attribute for ARTS access modes
def ArtsModeAttr : Arts_I32EnumAttr<"mode", ArtsMode>;

/// Enum info for DB allocation types
def DbAllocType : Arts_I32Enum<"DbAllocType",
    "ARTS DB allocation type",
    [
      I32EnumAttrCase<"stack", 0, "Stack allocation">,
      I32EnumAttrCase<"heap", 1, "Heap allocation">,
      I32EnumAttrCase<"global", 2, "Global allocation">,
      I32EnumAttrCase<"unknown", 3, "Unknown allocation type">
    ]>;

/// Enum attribute for DB allocation types
def DbAllocTypeAttr : Arts_I32EnumAttr<"alloc_type", DbAllocType>;

/// Enum info for EDT types
def EdtType : Arts_I32Enum<"EdtType",
    "ARTS EDT type",
    [
      I32EnumAttrCase<"parallel", 0, "Parallel EDT">,
      I32EnumAttrCase<"single", 1, "Single EDT">,
      I32EnumAttrCase<"sync", 2, "Sync EDT">,
      I32EnumAttrCase<"task", 3, "Task EDT">
    ]>;

/// Enum attribute for EDT types
def EdtTypeAttr : Arts_I32EnumAttr<"edt_type", EdtType>;


#endif // ARTS_ATTRS