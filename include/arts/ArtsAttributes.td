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
  let cppNamespace = "::mlir::arts";
  let genSpecializedAttr = 0;
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
      I32EnumAttrCase<"in", 0, "in">,
      I32EnumAttrCase<"out", 1, "out">,
      I32EnumAttrCase<"inout", 2, "inout">
    ]>;

// Enum attribute for ARTS access modes
def ArtsModeAttr : Arts_I32EnumAttr<"mode", ArtsMode>;

/// Enum info for DB allocation types
def DbAllocType : Arts_I32Enum<"DbAllocType",
    "ARTS DB allocation type",
    [
      I32EnumAttrCase<"stack", 0, "stack">,
      I32EnumAttrCase<"heap", 1, "heap">,
      I32EnumAttrCase<"global", 2, "global">,
      I32EnumAttrCase<"unknown", 3, "unknown">
    ]>;

/// Enum attribute for DB allocation types
def DbAllocTypeAttr : Arts_I32EnumAttr<"alloc_type", DbAllocType>;

/// Enum info for EDT types
def EdtType : Arts_I32Enum<"EdtType",
    "ARTS EDT type",
    [
      I32EnumAttrCase<"parallel", 0, "parallel">,
      I32EnumAttrCase<"single", 1, "single">,
      I32EnumAttrCase<"sync", 2, "sync">,
      I32EnumAttrCase<"task", 3, "task">
    ]>;

/// Enum attribute for EDT types
def EdtTypeAttr : Arts_I32EnumAttr<"edt_type", EdtType>;

/// Enum info for DB allocation modes
def DbMode : Arts_I32Enum<"DbMode",
    "ARTS DB allocation mode",
    [
      I32EnumAttrCase<"read", 8, "read">,
      I32EnumAttrCase<"write", 9, "write">,
      I32EnumAttrCase<"pinned", 10, "pinned">,
      I32EnumAttrCase<"once", 11, "once">,
      I32EnumAttrCase<"once_local", 12, "once_local">,
      I32EnumAttrCase<"gpu_read", 13, "gpu_read">,
      I32EnumAttrCase<"gpu_write", 14, "gpu_write">,
      I32EnumAttrCase<"lc", 15, "lc">,
      I32EnumAttrCase<"lc_sync", 17, "lc_sync">,
      I32EnumAttrCase<"lc_no_copy", 18, "lc_no_copy">,
      I32EnumAttrCase<"gpu_memset", 19, "gpu_memset">
    ]>;

/// Enum attribute for DB allocation modes
def DbModeAttr : Arts_I32EnumAttr<"db_mode", DbMode>;

#endif // ARTS_ATTRS