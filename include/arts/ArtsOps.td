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
// Arts DB Operations
//===----------------------------------------------------------------------===//
def DbAllocOp : Arts_Op<"db_alloc", [
    AttrSizedOperandSegments, MemoryEffects<[MemAlloc]>]> {
  let summary = "Allocate an ARTS DataBlock and get its pointer and GUID.";
  let description = [{
    This operation corresponds to the `artsDbCreate` runtime function.
    It allocates a DataBlock and returns its pointer and GUID.
    - mode: "in", "out", "inout"
    - ptr: address that will be copied to the DataBlock
    - sizes: sizes of the DataBlock
    - allocType: "stack", "heap", "global", "unknown" - tracks original allocation type
  }];
  let arguments = (ins
    StrAttr:$mode,
    Optional<AnyMemRef>:$address,
    Variadic<Index>:$sizes,
    OptionalAttr<StrAttr>:$allocType
  );

  let results = (outs AnyMemRef:$ptr);
  let hasCustomAssemblyFormat = 1;
}

def DbDepOp : Arts_Op<"db_dep", 
  [AttrSizedOperandSegments]> {
  let summary = "Creates a datablock dependency for arts.edt";
  let description = [{
    This operation creates a datablock dependency for the `arts.edt` operation.
    It represents deping a portion of a datablock with specific indices, offsets and sizes.
    - mode: "in", "out", "inout"
    - source: DbAllocOp that this dependency is based on
    - indices: indices for the dependency
    - offsets: offsets for the dependency
    - sizes: sizes for the dependency
  }];
  let arguments = (ins
    StrAttr:$mode,
    AnyMemRef:$source,
    Variadic<Index>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs AnyMemRef:$result);
  
  let assemblyFormat = [{
    $mode `(` $source `:` type($source) `)` 
    `indices` `[` $indices `]` 
    `offsets` `[` $offsets `]` 
    `sizes` `[` $sizes `]` 
    attr-dict `:` type($result)
  }];
}

def DbControlOp : Arts_Op<"db_control", 
  [AttrSizedOperandSegments]> {
  let summary = "Creates a control dependency for arts.edt";
  let description = [{
    This operation creates a control dependency for the `arts.edt` operation.
    It is used to ensure that the DataBlock is ready before the EDT is executed.
    - mode: "in", "out", "inout"
    - ptr: base address of the DataBlock
    - elementType: type of the DataBlock
    - elementTypeSize: size of the element type
    - indices: indices for the subview
    - offsets: offsets for the subview
    - sizes: sizes for the subview
  }];
  let arguments = (ins
    StrAttr:$mode,
    AnyMemRef:$ptr,
    TypeAttr:$elementType,
    Index:$elementTypeSize,
    Variadic<Index>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs AnyMemRef:$subview);
  let hasCustomAssemblyFormat = 1;

  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins
      "Type":$subview, "StringRef":$mode, "Value":$ptr, "Type":$elementType,
      "Value":$elementTypeSize, "ValueRange":$indices, "ValueRange":$sizes)>,
    OpBuilder<(ins
      "Type":$subview, "StringRef":$mode, "Value":$ptr, "Type":$elementType, 
      "Value":$elementTypeSize, "ValueRange":$indices, "ValueRange":$offsets, 
      "ValueRange":$sizes)>
  ];

}

//===----------------------------------------------------------------------===//
// arts.edt
//===----------------------------------------------------------------------===//
def EdtOp : Arts_Op<"edt",
  [DeclareOpInterfaceMethods<MemoryEffectsOpInterface>, 
   SingleBlockImplicitTerminator<"arts::YieldOp">, RecursiveMemoryEffects]> {

  let summary = "Event-Driven Task in ARTS";
  let description = [{
    Defines an ARTS event-driven task (EDT) with:
      - `dependencies` that specify input/output memory dependencies.

    The single-block region contains the task body.
    - if 'parallel' attribute is set, it can spawn multiple tasks.
    - if 'single' attribute is set, it is guaranteed to run in a single
      task, even if it is called from a parallel region.
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
    The `arts.event` operation creates an event (or array of events) that 
    can be used for inter-loop or cross-task dependencies within the 
    ARTS dialect.
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
  let regions = (region SizedRegion<1>:$body);

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
    The `arts.yield` operation serves as a terminator for various 
    ARTS operations, such as `arts.epoch`, `arts.parallel`, `arts.single`, 
    and `arts.edt`. It ensures proper control flow within the ARTS dialect 
    by marking the end of a region.
  }];
  let assemblyFormat = [{
    attr-dict-with-keyword
  }];
}

//===----------------------------------------------------------------------===//
// arts.barrier
//===----------------------------------------------------------------------===//
def BarrierOp : Arts_Op<"barrier", [MemoryEffects<[MemWrite]>]> {
  let summary = "Barrier synchronization in ARTS";
  let description = [{
    Barrier synchronization operation in ARTS. It synchronizes all previous
    spawned EDTs of the parent region. This operation has memory write effects
    as it affects the synchronization state of the runtime system.
  }];
  let assemblyFormat = "attr-dict";
}

//===----------------------------------------------------------------------===//
// arts.get_total_workers
//===----------------------------------------------------------------------===//
def GetTotalWorkersOp : Arts_Op<"get_total_workers", 
  [MemoryEffects<[MemWrite]>, RecursiveMemoryEffects]> {
  let summary = "Get the total number of workers in ARTS";
  let description = [{
    The `arts.get_total_workers` operation retrieves the total number of 
    workers available in the ARTS runtime environment.
  }];
  let results = (outs I32:$result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins), [{
      $_state.addTypes(IntegerType::get($_builder.getContext(), 32));
    }]>
  ];
}

//===----------------------------------------------------------------------===//
// arts.get_total_nodes
//===----------------------------------------------------------------------===//
def GetTotalNodesOp : Arts_Op<"get_total_nodes",
  [MemoryEffects<[MemWrite]>, RecursiveMemoryEffects]> {
  let summary = "Get the total number of nodes in ARTS";
  let description = [{
    The `arts.get_total_nodes` operation retrieves the total number of 
    nodes available in the ARTS runtime environment.
  }];
  let results = (outs I32:$result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins), [{
      $_state.addTypes(IntegerType::get($_builder.getContext(), 32));
    }]>
  ];
}

//===----------------------------------------------------------------------===//
// arts.get_current_worker
//===----------------------------------------------------------------------===//
def GetCurrentWorkerOp : Arts_Op<"get_current_worker",
  [MemoryEffects<[MemWrite]>, RecursiveMemoryEffects]> {
  let summary = "Get the current worker ID in ARTS";
  let description = [{
    The `arts.get_current_worker` operation retrieves the ID of the 
    current worker executing the operation in the ARTS runtime environment.
  }];
  let results = (outs I32:$result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins), [{
      $_state.addTypes(IntegerType::get($_builder.getContext(), 32));
    }]>
  ];
}

//===----------------------------------------------------------------------===//
// arts.get_current_node
//===----------------------------------------------------------------------===//
def GetCurrentNodeOp : Arts_Op<"get_current_node", 
  [MemoryEffects<[MemWrite]>, RecursiveMemoryEffects]> {
  let summary = "Get the current node ID in ARTS";
  let description = [{
    The `arts.get_current_node` operation retrieves the ID of the 
    current node executing the operation in the ARTS runtime environment.
  }];
  let results = (outs I32:$result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins), [{
      $_state.addTypes(IntegerType::get($_builder.getContext(), 32));
    }]>
  ];
}

#endif // ARTS_OPS