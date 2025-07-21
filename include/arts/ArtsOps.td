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
include "mlir/Interfaces/InferTypeOpInterface.td"

include "mlir/Dialect/LLVMIR/LLVMOpBase.td"
include "mlir/Dialect/LLVMIR/LLVMInterfaces.td"

/// ARTS Dialect
include "arts/ArtsDialect.td"
include "arts/ArtsAttributes.td"
include "arts/ArtsTypes.td"

//===----------------------------------------------------------------------===//
// Custom Traits for ARTS
//===----------------------------------------------------------------------===//

class Arts_InferShapedTypeOpAdaptor<string mnemonic, list<Trait> traits = []>
    : Arts_Op<mnemonic, !listconcat(traits, [InferShapedTypeOpAdaptor, Pure])> {
  let assemblyFormat =
      "operands attr-dict `:` functional-type(operands, results)";
}

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
    AttrSizedOperandSegments, MemoryEffects<[MemAlloc]>, DeclareOpInterfaceMethods<InferTypeOpInterface, ["inferReturnTypes"]>
    ]> {
  let summary = "Allocate an ARTS DataBlock and get its pointer and GUID.";
  let description = [{
    This operation corresponds to the `artsDbCreate` runtime function.
    It allocates a DataBlock and returns its pointer and GUID.
    - mode: enum for "in", "out", "inout"
    - ptr: address that will be copied to the DataBlock
    - sizes: sizes of the DataBlock
    - allocType: "stack", "heap", "global", "unknown" - tracks original allocation type
  }];
  let arguments = (ins
    ArtsModeAttr:$mode,
    Optional<AnyMemRef>:$address,
    Variadic<Index>:$sizes,
    OptionalAttr<DbAllocTypeAttr>:$allocType
  );

  let results = (outs AnyMemRef:$ptr);
  let hasCustomAssemblyFormat = 1;
}

def DbAcquireOp : Arts_Op<"db_acquire", 
  [AttrSizedOperandSegments, DeclareOpInterfaceMethods<ViewLikeOpInterface>, DeclareOpInterfaceMethods<InferTypeOpInterface, ["inferReturnTypes"]>
  ]> {
  let summary = "Acquires access to an ARTS DataBlock.";
  let description = [{
    This operation acquires a local view of the DataBlock for safe access within EDTs.
    It subsumes dependency creation with mode and subregion support.
    - mode: enum for "in", "out", "inout"
    - source: DbAllocOp or previous acquire
    - indices: indices for subregion
    - offsets: offsets for subregion
    - sizes: sizes for subregion
  }];
  let arguments = (ins
    ArtsModeAttr:$mode,
    AnyMemRef:$source,
    Variadic<Index>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs AnyMemRef:$view);
  
  let assemblyFormat = [{
    $mode `(` $source `:` type($source) `)` 
    `indices` `[` $indices `]` 
    `offsets` `[` $offsets `]` 
    `sizes` `[` $sizes `]` 
    attr-dict `:` type($view)
  }];
}

def DbReleaseOp : Arts_Op<"db_release", []> {
  let summary = "Releases access to an ARTS DataBlock source.";
  let description = [{
    This operation releases the acquired source, signaling completion to the runtime.
  }];
  let arguments = (ins Variadic<AnyMemRef>:$sources);
  let assemblyFormat = [{
    $sources `:` type($sources) attr-dict
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
    Note: Currently ignored for execution, but retained for future correctness enforcement.
  }];
  let arguments = (ins
    ArtsModeAttr:$mode,
    AnyMemRef:$ptr,
    TypeAttr:$elementType,
    Index:$elementTypeSize,
    Variadic<Index>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs AnyMemRef:$subview);
  let assemblyFormat = [{
    $mode `(` $ptr `:` type($ptr) `)` 
    `elementType` `(` $elementType `)` 
    `elementTypeSize` `(` $elementTypeSize `:` type($elementTypeSize) `)` 
    `indices` `[` $indices `]` 
    `offsets` `[` $offsets `]` 
    `sizes` `[` $sizes `]` 
    attr-dict `:` type($subview)
  }];
}

//===----------------------------------------------------------------------===//
// arts.edt
//===----------------------------------------------------------------------===//
def EdtOp : Arts_Op<"edt",
  [DeclareOpInterfaceMethods<MemoryEffectsOpInterface>, 
   SingleBlockImplicitTerminator<"arts::YieldOp">, 
   RecursiveMemoryEffects]> {

  let summary = "Event-Driven Task in ARTS";
  let description = [{
    Defines an ARTS event-driven task (EDT) with:
      - `dependencies` that specify input/output memory dependencies (acquired views).

    The single-block region contains the task body.
    - if 'parallel' attribute is set, it can spawn multiple tasks.
    - if 'single' attribute is set, it is guaranteed to run in a single
      task, even if it is called from a parallel region.
  }];

  let arguments = (ins
    EdtTypeAttr:$type,
    Variadic<AnyMemRef>:$dependencies
  );

  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = [{
    $type $dependencies attr-dict `:` type($dependencies)
    $body
  }];

  let extraClassDeclaration = [{
    SmallVector<Value> getDependenciesVector();
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


//===----------------------------------------------------------------------===//
// EDT Lowering Operations
//===----------------------------------------------------------------------===//

def EdtOutlineFnOp : Arts_Op<"edt_outline_fn", [NoSideEffect]> {
  let summary = "Outlined EDT call with packed arrays.";
  let description = [{
    Intermediate op for calling outlined EDT function with param/dep arrays. 
    Returns EDT guid.
  }];
  let arguments = (ins MemRefType:$param_memref, MemRefType:$dep_memref);
  let results = (outs I64Type:$guid);
  let attributes = (attr StringAttr:$outlined_func);
  let assemblyFormat = "$param_memref, $dep_memref attr-dict";
}

def EdtParamPackOp : Arts_Op<"edt_param_pack", [NoSideEffect]> {
  let summary = "Pack parameters into memref<?xi64>.";
  let description = [{
    Packs multiple parameters (cast to i64) into a memref for ARTS paramv.
  }];
  let arguments = (ins Variadic<AnyType>:$params);
  let results = (outs
    MemRefType:$memref { let type = MemRefType::get({ShapedType::kDynamic}, I64Type); }
  );
  let assemblyFormat = "$params : type($params) -> $memref";
}

def EdtParamUnpackOp : Arts_Op<"edt_param_unpack", [NoSideEffect]> {
  let summary = "Unpack parameters from memref<?xi64>.";
  let description = [{
    Unpacks memref to multiple original types (load i64, cast back).
  }];
  let arguments = (ins MemRefType:$memref);
  let results = (variadic AnyType:$unpacked);
  let assemblyFormat = "$memref -> type($unpacked)";
}

def EdtDepUnpackOp : Arts_Op<"edt_dep_unpack", [NoSideEffect]> {
  let summary = "Unpack dependencies from memref<?xi64>.";
  let description = [{
    Unpacks depv guids, calls artsGetFromDb for DB ptrs, casts others.
  }];
  let arguments = (ins MemRefType:$memref);
  let results = (variadic AnyType:$unpacked);
  let assemblyFormat = "$memref -> type($unpacked)";
}

def RecordInDepOp : Arts_Op<"record_in_dep", [NoSideEffect]> {
  let summary = "Record in-dependency for EDT.";
  let description = [{
    Records an in-dep (e.g., DB acquire) for the EDT at a slot.
  }];
  let arguments = (ins I64Type:$edt_guid, I64Type:$dep);
  let attributes = (attr
    I32Attr:$slot,
    StringAttr:$mode {default_value = "in"}
  );
  let assemblyFormat = "$edt_guid, $dep attr-dict";
}

def IncrementOutLatchOp : Arts_Op<"increment_out_latch", [NoSideEffect]> {
  let summary = "Increment out-latch for dependency.";
  let description = [{
    Increments latch count for out-dep (e.g., DB release).
  }];
  let arguments = (ins I64Type:$dep);
  let attributes = (attr StringAttr:$mode {default_value = "out"});
  let assemblyFormat = "$dep attr-dict";
}

#endif // ARTS_OPS