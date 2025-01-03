//===- ArtsTypes.td - ARTS dialect types -------------------*- tablegen -*-===//
//
// This file declares the ARTS dialect types.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_DIALECT_ARTS_IR_ARTSTYPES
#define MLIR_DIALECT_ARTS_IR_ARTSTYPES

include "mlir/IR/AttrTypeBase.td"
include "ArtsDialect.td"

//===----------------------------------------------------------------------===//
// ARTS Types
//===----------------------------------------------------------------------===//

class Arts_Type<string name, string typeMnemonic> : TypeDef<ArtsDialect, name> {
  let mnemonic = typeMnemonic;
}

/// Dependency type in the ARTS dialect
def DepType : Arts_Type<"Dep", "dep"> {
  let summary = "represents a dependency in the ARTS dialect";
  let description = [{
    `arts.dep` encapsulates information about dependencies, including mode
    (e.g., in, out, inout), memrefs, and indices. It is used in dependency 
    operations and converted to events in the backend.
  }];
}

/// EDT token type in the ARTS dialect
def EDTTokenType : Arts_Type<"EDTToken", "EDT_token"> {
  let summary = "represents a EDT token in the ARTS dialect";
  let description = [{
    `arts.EDT_token` represents a unique identifier for a EDT that allows
    synchronization and dependency tracking in ARTS-based EDT operations.
  }];
}

/// Event type in the ARTS dialect
def EventType : Arts_Type<"Event", "event"> {
  let summary = "represents an event in the ARTS dialect";
  let description = [{
    `arts.event` is a type used to represent events in the ARTS dialect.
    Events are associated with EDT completion or specific dependency states.
  }];
}

/// Group type in the ARTS dialect
def GroupType : Arts_Type<"Group", "group"> {
  let summary = "represents a group of EDTs in the ARTS dialect";
  let description = [{
    `arts.group` represents a collection of EDT tokens or events. It enables
    bulk operations on the group, such as waiting for all EDTs to complete.
  }];
}

/// Any value type or token type for ARTS operations
def AnyValueOrEDTTokenType : AnyTypeOf<[EDTTokenType, DepType]>;

/// Any ARTS type
def AnyArtsType : AnyTypeOf<[EDTTokenType, DepType, EventType, GroupType]>;

#endif // MLIR_DIALECT_ARTS_IR_ARTSTYPES