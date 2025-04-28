///==========================================================================
/// File: ArtsOps.td
/// This file defines multiple operations for the ARTS Dialect.
///==========================================================================

#ifndef ARTS_OPS
#define ARTS_OPS

include "mlir/IR/OpBase.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Interfaces/ViewLikeInterface.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"

include "mlir/Dialect/LLVMIR/LLVMOpBase.td"
include "mlir/Dialect/LLVMIR/LLVMInterfaces.td"

/// ARTS Dialect
include "arts/ArtsDialect.td"
include "arts/ArtsAttributes.td"
include "arts/ArtsTypes.td"

//===----------------------------------------------------------------------===//
// arts.undef
//===----------------------------------------------------------------------===//
def UndefOp: Arts_Op<"undef", [Pure]> {
  let summary = "More flexible undef op";
  let skipDefaultBuilders = 1;
  let results = (outs AnyType:$result);
  let builders = [
    OpBuilder<(ins "Type":$type), [{
      $_state.types.push_back(type);
    }]>];
  let hasCanonicalizer = true;
}

//===----------------------------------------------------------------------===//
// arts.alloc
//===----------------------------------------------------------------------===//
def AllocOp : Arts_Op<"alloc", []> {
  let summary = "Allocate a memref in ARTS dialect";
  let description = [{
    The `arts.alloc` operation allocates a memref with a given shape.
    It mirrors `memref.alloc` but is customized for the ARTS dialect to enable
    specific behavior during code generation and lowering.
  }];

  let arguments = (ins Variadic<Index>:$dynamicSizes);
  let results = (outs AnyMemRef:$result);
  let assemblyFormat = [{
    `(` $dynamicSizes `)` attr-dict `:` type($result)
  }];
}

//===----------------------------------------------------------------------===//
// arts.datablock
//===----------------------------------------------------------------------===//
def DataBlockOp : Arts_Op<"datablock", 
  [DeclareOpInterfaceMethods<MemoryEffectsOpInterface>, AttrSizedOperandSegments]> {
  let summary = "Creates a subview dependency for arts.edt or arts.parallel";
  let arguments = (ins
    StrAttr:$mode,                             // Dependency mode (e.g., "in", "out", "inout")
    AnyMemRef:$ptr,                            // Base ptr
    TypeAttr:$elementType,                     // Element type for the subview (or memref type)
    Index:$elementTypeSize,                    // Size (number of elements) associated with that type
    Variadic<Index>:$indices,                  // Indices for the subview
    Variadic<Index>:$sizes,                    // Sizes for the subview
    Optional<AnyType>:$inEvent,                // Input event associated with the DB
    Optional<AnyType>:$outEvent,               // Output event associated with the DB
    OptionalAttr<DomainAttr>:$domain           // Domain attribute
  );

  let results = (outs AnyMemRef:$subview);     // Produces a memref
  let hasCustomAssemblyFormat = 1;

  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins
      "Type":$subview, "StringRef":$mode, "Value":$ptr, "Type":$elementType, "Value":$elementTypeSize,
      "ValueRange":$indices, "ValueRange":$sizes)>,
    OpBuilder<(ins
      "Type":$subview, "StringRef":$mode, "Value":$ptr, "Type":$elementType, "Value":$elementTypeSize,
      "ValueRange":$indices, "ValueRange":$sizes, "Value":$inEvent, "Value":$outEvent)>,
    OpBuilder<(ins
      "Type":$subview, "StringRef":$mode, "Value":$ptr, "Type":$elementType, "Value":$elementTypeSize,
      "ValueRange":$indices, "ValueRange":$sizes, "Value":$inEvent, "Value":$outEvent,
      "DomainAttr":$domain)>
  ];

  let extraClassDeclaration = [{
    bool hasPtrDb();
    void setHasPtrDb();
    bool hasGuid();
    void setHasGuid();
    bool hasSingleSize();
    void setHasSingleSize();
  }];
}

//===----------------------------------------------------------------------===//
// arts.edt
//===----------------------------------------------------------------------===//
def EdtOp : Arts_Op<"edt",
  [DeclareOpInterfaceMethods<MemoryEffectsOpInterface>, 
   SingleBlockImplicitTerminator<"arts::YieldOp">]> {

  let summary = "Event-Driven Task in ARTS";
  let description = [{
    Defines an ARTS event-driven task (EDT) with:
      - `dependencies` that specify input/output memory dependencies.

    The single-block region contains the task body.
    - if 'parallel' attribute is set, it can spawn multiple tasks.
    - if 'single' attribute is set, it is guaranteed to run in a single task, 
      even if it is called from a parallel region.
  }];

  let arguments = (ins
    Variadic<AnyMemRef>:$dependencies
  );

  let regions = (region SizedRegion<1>:$body);
  let hasCustomAssemblyFormat = 1;

  let extraClassDeclaration = [{
    SmallVector<Value> getDependenciesVector();
    bool isParallel();
    bool isSingle();
    bool isSync();
    bool isTask();
    void setIsParallelAttr();
    void setIsSingleAttr();
    void setIsSyncAttr();
    void setIsTaskAttr();
    void clearIsParallelAttr();
    void clearIsSingleAttr();
    void clearIsSyncAttr();
    void clearIsTaskAttr();
  }];
}

//===----------------------------------------------------------------------===//
// arts.event
//===----------------------------------------------------------------------===//
def EventOp : Arts_Op<"event", []> {
  let summary = "Create an event or array of events in ARTS";
  let description = [{
    The `arts.event` operation creates an event (or array of events) that can be
    used for inter-loop or cross-task dependencies within the ARTS dialect.
  }];

  let arguments = (ins Variadic<Index>:$sizes);
  let results = (outs AnyMemRef:$event);
  let assemblyFormat = [{
    `[` $sizes `]` `->` attr-dict `:` type($event)
  }];
  let extraClassDeclaration = [{
    bool hasSingleSize();
    void setHasSingleSize();
  }];
}

//===----------------------------------------------------------------------===//
// arts.epoch
//===----------------------------------------------------------------------===//
def EpochOp : Arts_Op<"epoch", 
    [SingleBlockImplicitTerminator<"arts::YieldOp">]> {
  let summary = "Epoch region in ARTS";
  let description = [{
    An ARTS epoch region. It can spawn multiple tasks and
    synchronize them at the end of the epoch.
  }];
  let regions = (region AnyRegion:$body);
  let assemblyFormat = [{
    attr-dict-with-keyword $body
  }];
}

//===----------------------------------------------------------------------===//
// arts.yield
//===----------------------------------------------------------------------===//
def YieldOp : Arts_Op<"yield", 
  [Pure, ReturnLike, Terminator, 
   ParentOneOf<["EdtOp", "EpochOp"]>]> {

  let summary = "ARTS dialect yield terminator";
  let description = [{
    The `arts.yield` operation serves as a terminator for various ARTS operations, 
    such as `arts.epoch`, `arts.parallel`, `arts.single`, and `arts.edt`.
    It ensures proper control flow within the ARTS dialect by marking the end of a region.
  }];
  let assemblyFormat = [{
    attr-dict-with-keyword
  }];
}

//===----------------------------------------------------------------------===//
// arts.barrier
//===----------------------------------------------------------------------===//
/// TODO: Add memory effects
def BarrierOp : Arts_Op<"barrier"> {
  let summary = "Barrier synchronization in ARTS";
  let description = [{
    Barrier synchronization operation in ARTS. It synchronizes all previous
    spawned EDTs of the parent region.
  }];
  let assemblyFormat = "attr-dict";
}


#endif // ARTS_OPS