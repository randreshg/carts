///==========================================================================///
/// File: ArtsOps.td
/// This file defines multiple operations for the ARTS Dialect.
///==========================================================================///

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

///===----------------------------------------------------------------------===///
/// Custom Traits for ARTS
///===----------------------------------------------------------------------===///

class Arts_InferShapedTypeOpAdaptor<string mnemonic, list<Trait> traits = []>
    : Arts_Op<mnemonic, !listconcat(traits, [InferShapedTypeOpAdaptor, Pure])> {
  let assemblyFormat =
      "operands attr-dict `:` functional-type(operands, results)";
}

///===----------------------------------------------------------------------===///
/// arts.undef
///===----------------------------------------------------------------------===///
def UndefOp : Arts_Op<"undef", [Pure]> {
  let summary = "More flexible undef op";
  let results = (outs AnyType : $result);
  let hasCanonicalizer = true;
  let assemblyFormat = "attr-dict `:` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [OpBuilder<(ins "Type"
                             : $type),
                            [{ $_state.addTypes(type); }]>];
}

///===----------------------------------------------------------------------===///
/// arts.alloc
///===----------------------------------------------------------------------===///
def AllocOp : Arts_Op<"alloc", []> {
  let summary = "Allocate a memref in ARTS dialect";
  let description = [{
    The `arts.alloc` operation allocates a memref with a given shape.
    It mirrors `memref.alloc` but is customized for the ARTS dialect to enable
    specific behavior during code generation and lowering.
  }];

  let arguments = (ins Variadic<Index> : $dynamicSizes);
  let results = (outs AnyMemRef : $result);
  let assemblyFormat =
      [{
    `(` $dynamicSizes `)` attr-dict `:` type($result)}];
}

///===----------------------------------------------------------------------===///
/// Arts DB Operations
///===----------------------------------------------------------------------===///
def DbDimOp : Arts_Op<"db_dim", [Pure]> {
  let summary = "Query dimension of a datablock-backed memref";
  let description =
      [{The `arts.db_dim` op queries the size of the specified dimension of a
            datablock -
        backed memref.It is preferred over `memref.dim` so DB -
        oriented canonicalization can fold it to explicit values coming
            from `arts.db_alloc` or `arts.db_acquire` slice sizes.}];
  let arguments = (ins AnyMemRef : $source, Index : $dim);
  let results = (outs Index : $result);
  let assemblyFormat =
      "$source `,` $dim attr-dict `:` type($source) `->` type($result)";
  let hasCanonicalizer = 1;
  let builders = [OpBuilder<(ins "Value" : $source, "int64_t" : $dim)>];
}

def DbAllocOp : Arts_Op<"db_alloc",
                        [AttrSizedOperandSegments, MemoryEffects<[MemAlloc]>]> {
  let summary = "Allocate an ARTS DataBlock and get its pointer and GUID.";
  let description = [{
    This operation corresponds to the `artsDbCreate` runtime function.
    It allocates a DataBlock and returns its pointer and GUID.
    - mode: enum for
    "in", "out",
        "inout" -
            ptr
        : address that will be copied to the DataBlock -
          sizes : outer sizes(number of datablocks along each dimension) -
                  elementSizes
        : logical dimensions of the datablock element payload -
          allocType : "stack",
        "heap", "global", "unknown" - tracks original allocation type
  }];
  let arguments = (ins
    ArtsModeAttr:$mode,
    I32:$route,
    DbAllocTypeAttr:$allocType,
    DbModeAttr:$dbMode,
    TypeAttr:$elementType,
    Optional<AnyMemRef>:$address,
    Variadic<Index>:$sizes,
    Variadic<Index>:$elementSizes,
    OptionalAttr<PartitionModeAttr>:$partition_mode,
    OptionalAttr<DbAccessPatternAttr>:$access_pattern
  );
  let results = (outs MemRefOf<[I64]> : $guid, AnyMemRef : $ptr);
  let assemblyFormat = [{
    `[` $mode `,` $allocType `,` $dbMode (`,` $partition_mode^)? (`,` $access_pattern^)? `]`
    `route` `(` $route `:` type($route) `)`
    ( `(` $address^ `:` type($address) `)` )?
    `sizes` `[` $sizes `]`
    `elementType` `(` $elementType `)`
    `elementSizes` `[` $elementSizes `]`
    attr-dict `:` `(` type($guid) `,` type($ptr) `)`
  }];
  let builders = [
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$route,
                   "DbAllocType":$allocType,
                   "DbMode":$dbMode,
                   "Type":$elementType,
                   CArg<"Value", "nullptr">:$address,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$elementSizes)>,
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$route,
                   "DbAllocType":$allocType,
                   "DbMode":$dbMode,
                   "Type":$elementType,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$elementSizes)>,
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$route,
                   "DbAllocType":$allocType,
                   "DbMode":$dbMode,
                   "Type":$elementType,
                   "Type":$pointerType,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$elementSizes)>,
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$route,
                   "DbAllocType":$allocType,
                   "DbMode":$dbMode,
                   "Type":$elementType,
                   CArg<"Value", "nullptr">:$address,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$elementSizes,
                   CArg<"PartitionMode", "PartitionMode::coarse">:$partitionMode)>,
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$route,
                   "DbAllocType":$allocType,
                   "DbMode":$dbMode,
                   "Type":$elementType,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$elementSizes,
                   CArg<"PartitionMode", "PartitionMode::coarse">:$partitionMode)>,
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$route,
                   "DbAllocType":$allocType,
                   "DbMode":$dbMode,
                   "Type":$elementType,
                   "Type":$pointerType,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$elementSizes,
                   CArg<"PartitionMode", "PartitionMode::coarse">:$partitionMode)>
  ];
  let extraClassDeclaration = [{
    MemRefType getAllocatedElementType();
    LogicalResult verify();
  }];
}

def DbAcquireOp : Arts_Op<"db_acquire", [AttrSizedOperandSegments]> {
  let summary = "Acquires access to an ARTS DataBlock.";
  let description = [{
    This operation acquires a local view of the DataBlock for safe access within EDTs.
    It subsumes dependency creation with mode and subregion support.
    - mode: enum for "in", "out", "inout"
    - sourceGuid: GUID memref from DbAllocOp or previous DbAcquireOp (optional)
    - sourcePtr: Ptr memref from DbAllocOp or previous DbAcquireOp

    DB-space fields: indices, offsets and sizes

    Partition hints (element-space, for DbPartitioning pass):
    - partition_mode: coarse, chunked, fine_grained, or stencil - describes access pattern
    - partition_indices: element indices for fine_grained access (from depend(inout: A[i][j]))
    - partition_offsets: element offsets for chunked/stencil access (from depend(in: A[off:sz]))
    - partition_sizes: element sizes for chunked/stencil access

    Multiple partition entries (for multiple depend clause entries):
    When an OpenMP task has multiple depend entries like `depend(in: A[i][j], A[i-1][j])`,
    each entry becomes a separate partition entry. The segment attributes track how many
    values belong to each entry:
    - partition_indices_segments: [2, 2] means entry 0 has 2 indices, entry 1 has 2 indices
    - partition_entry_modes: per-entry partition mode (fine_grained, chunked, etc.)

    When source_guid is absent (nullptr), it means this acquire is chaining from
    a parent EDT's acquire where only the ptr was passed through block arguments.

    ESD partial acquisition (optional, for stencil halo fetching):
    - element_offsets: Offset within the chunk (element-level, not byte-level)
    - element_sizes: Size within the chunk (element-level)
    When these are set, EdtLowering computes byte_offset/byte_size using the
    allocation's elementSizes and generates RecordDepOp with byte_offsets for
    ESD (Ephemeral Slice Dependencies) partial chunk acquisition.

    Returns (guid, ptr) tuple for the acquired datablock.
  }];
  let arguments = (ins
    ArtsModeAttr:$mode,
    Optional<MemRefOf<[I64]>>:$source_guid,
    AnyMemRef:$source_ptr,
    OptionalAttr<PartitionModeAttr>:$partition_mode,
    Variadic<Index>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes,
    Variadic<Index>:$partition_indices,
    Variadic<Index>:$partition_offsets,
    Variadic<Index>:$partition_sizes,
    Optional<I1>:$bounds_valid,
    Variadic<Index>:$element_offsets,
    Variadic<Index>:$element_sizes,
    /// Segment attributes for multiple partition entries
    OptionalAttr<DenseI32ArrayAttr>:$partition_indices_segments,
    OptionalAttr<DenseI32ArrayAttr>:$partition_offsets_segments,
    OptionalAttr<DenseI32ArrayAttr>:$partition_sizes_segments,
    /// Per-entry partition modes
    OptionalAttr<DenseI32ArrayAttr>:$partition_entry_modes
  );
  let results = (outs MemRefOf<[I64]> : $guid, AnyMemRef : $ptr);
  let assemblyFormat = [{
    `[` $mode `]`
    `(` ($source_guid^ `:` type($source_guid) `,`)? $source_ptr `:` type($source_ptr) `)`
    ( `partitioning` `(`
        $partition_mode^
        (`,` `indices` `[` $partition_indices^ `]`)?
        (`,` `offsets` `[` $partition_offsets^ `]`)?
        (`,` `sizes` `[` $partition_sizes^ `]`)?
      `)`
    )?
    (`,` `indices` `[` $indices^ `]`)?
    (`,` `offsets` `[` $offsets^ `]`)?
    (`,` `sizes` `[` $sizes^ `]`)?
    ( `bounds_valid` `(` $bounds_valid^ `)` )?
    ( `element_offsets` `[` $element_offsets^ `]` )?
    ( `element_sizes` `[` $element_sizes^ `]` )?
    attr-dict `->` `(` type($guid) `,` type($ptr) `)`
  }];
  let hasVerifier = 1;
  let hasCanonicalizer = 1;
  let skipDefaultBuilders = 1;
  let builders = [
    /// Builder for CreateDbs: sets partition hints, leaves DB-space empty
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$source_guid,
                   "Value":$source_ptr,
                   CArg<"std::optional<PartitionMode>", "std::nullopt">:$partitionMode,
                   CArg<"SmallVector<Value>", "{}">:$indices,
                   CArg<"SmallVector<Value>", "{}">:$offsets,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$partition_indices,
                   CArg<"SmallVector<Value>", "{}">:$partition_offsets,
                   CArg<"SmallVector<Value>", "{}">:$partition_sizes,
                   CArg<"Value", "{}">:$bounds_valid,
                   CArg<"SmallVector<Value>", "{}">:$element_offsets,
                   CArg<"SmallVector<Value>", "{}">:$element_sizes)>,
    /// Builder with partition_mode and explicit ptrType
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$source_guid,
                   "Value":$source_ptr,
                   "Type":$ptrType,
                   CArg<"std::optional<PartitionMode>", "std::nullopt">:$partitionMode,
                   CArg<"SmallVector<Value>", "{}">:$indices,
                   CArg<"SmallVector<Value>", "{}">:$offsets,
                   CArg<"SmallVector<Value>", "{}">:$sizes,
                   CArg<"SmallVector<Value>", "{}">:$partition_indices,
                   CArg<"SmallVector<Value>", "{}">:$partition_offsets,
                   CArg<"SmallVector<Value>", "{}">:$partition_sizes,
                   CArg<"Value", "{}">:$bounds_valid,
                   CArg<"SmallVector<Value>", "{}">:$element_offsets,
                   CArg<"SmallVector<Value>", "{}">:$element_sizes)>
  ];
  let extraClassDeclaration = [{
    /// Check if this acquire has element-level offset/size info (ESD partial
    /// acquisition)
    bool hasElementOffsets() { return !getElementOffsets().empty(); }

    /// Check if this acquire has a partition mode set
    bool hasPartitionMode() { return getPartitionMode().has_value(); }

    /// Get partition mode, returns coarse if not set
    PartitionMode getPartitionModeOr(PartitionMode defaultMode =
                                         PartitionMode::coarse) {
      return getPartitionMode().value_or(defaultMode);
    }

    /// Check if this is a fine-grained (element-wise) acquire
    bool isFineGrained() {
      return getPartitionModeOr() == PartitionMode::fine_grained;
    }

    /// Check if this is a block acquire
    bool isBlock() { return getPartitionModeOr() == PartitionMode::block; }

    /// Check if this is a stencil acquire
    bool isStencil() { return getPartitionModeOr() == PartitionMode::stencil; }

    /// Check if this is a coarse acquire
    bool isCoarse() { return getPartitionModeOr() == PartitionMode::coarse; }

    /// Check if this acquire has partition hints (element-space
    /// indices/offsets/sizes)
    bool hasPartitionHints() { return hasExplicitPartitionHints(); }

    /// Check if this acquire has explicit partition hints (including sizes)
    bool hasExplicitPartitionHints();

    /// Copy partition segment metadata from another acquire
    void copyPartitionSegmentsFrom(DbAcquireOp source);

    //===------------------------------------------------------------------===//
    // Multiple Partition Entry Support
    //===------------------------------------------------------------------===//

    /// Check if this acquire has multiple partition entries
    bool hasMultiplePartitionEntries();

    /// Get number of partition entries (returns 1 for backward compat if no
    /// segments)
    size_t getNumPartitionEntries();

    /// Get the partition mode for a specific entry (uses partition_entry_modes
    /// if set)
    PartitionMode getPartitionEntryMode(size_t entryIdx);

    /// Get partition indices for a specific entry
    SmallVector<Value> getPartitionIndicesForEntry(size_t entryIdx);

    /// Get partition offsets for a specific entry
    SmallVector<Value> getPartitionOffsetsForEntry(size_t entryIdx);

    /// Get partition sizes for a specific entry
    SmallVector<Value> getPartitionSizesForEntry(size_t entryIdx);

    /// Set segment attributes for multi-entry partition hints.
    /// Only sets non-empty arrays, skips empty ones.
    void setPartitionSegments(
        ArrayRef<int32_t> indicesSegments, ArrayRef<int32_t> offsetsSegments,
        ArrayRef<int32_t> sizesSegments, ArrayRef<int32_t> entryModes);

    /// Get all partition entries as unified PartitionInfo structs.
    /// Returns one PartitionInfo per depend entry.
    SmallVector<PartitionInfo> getPartitionInfos();

    /// Clear all partition hints (operands and segment attributes).
    void clearPartitionHints();

    /// Check if all partition entries have fine_grained mode.
    /// Returns true if hasMultiplePartitionEntries() and all entry modes are
    /// fine_grained. Used to distinguish task dependency patterns from stencil
    /// patterns.
    bool hasAllFineGrainedEntries();
  }];
}

def DbCopyOp : Arts_Op<"db_copy", [MemoryEffects<[MemWrite]>]> {
  let summary = "Create a copy of a datablock with different partitioning";
  let description = [{
    Creates a copy of an existing datablock and returns the new GUID and
    pointer. The `dest_partition` attribute specifies the desired partitioning
    for the copy (e.g., fine_grained for element-wise access).

    The source partition mode is inferred from the source allocation.
    Only source_ptr is needed - source_guid is not used in lowering.
  }];
  let arguments = (ins AnyMemRef
                   : $source_ptr, PartitionModeAttr
                   : $dest_partition);
  let results = (outs MemRefOf<[I64]> : $guid, AnyMemRef : $ptr);
  let assemblyFormat = [{
    `(` $source_ptr `:` type($source_ptr) `)`
    `dest_partition` `=` $dest_partition attr-dict
    `->` `(` type($guid) `,` type($ptr) `)`
  }];
  let builders = [OpBuilder<(ins "Value"
                             : $source_ptr, "PartitionMode"
                             : $dest_partition)>];
}

def DbSyncOp : Arts_Op<"db_sync",
                       [AttrSizedOperandSegments, MemoryEffects<[MemWrite]>]> {
  let summary = "Copy data from source to dest datablock";
  let description = [{
    Copies data from source datablock to dest datablock. Direction is implicit:
    - source_ptr = where we READ from
    - dest_ptr = where we WRITE to

    The optional element_offsets/element_sizes describe a logical subrange
    for partial sync.

    During lowering, this creates a DbAcquireOp on source to establish
    the dependency chain for distributed execution.

    Only ptrs are needed - guids can be obtained from the allocation.
  }];
  let arguments = (ins
    AnyMemRef:$dest_ptr,
    AnyMemRef:$source_ptr,
    Variadic<Index>:$element_offsets,
    Variadic<Index>:$element_sizes,
    DefaultValuedAttr<BoolAttr, "true">:$full_sync
  );
  let assemblyFormat = [{
    `(` $dest_ptr `:` type($dest_ptr) `,` $source_ptr `:` type($source_ptr) `)`
    `element_offsets` `[` $element_offsets `]`
    `element_sizes` `[` $element_sizes `]` attr-dict
  }];
  let builders = [
    OpBuilder<(ins "Value":$dest_ptr, "Value":$source_ptr,
                   CArg<"ArrayRef<Value>", "{}">:$element_offsets,
                   CArg<"ArrayRef<Value>", "{}">:$element_sizes)>
  ];
}

def DbRefOp : Arts_Op<"db_ref", [Pure]> {
  let summary = "References an outer datablock element.";
  let description = [{
    References an element from a datablock handle using only the outer
    dimensions. The result type must match the element type of the source
    memref.
  }];
  let arguments = (ins AnyMemRef : $source, Variadic<Index> : $indices);
  let results = (outs AnyType : $result);
  let assemblyFormat = [{
    $source `[` $indices `]` attr-dict `:` type($source) `->` type($result)
  }];
  let builders = [
    OpBuilder<(ins "Value":$source,
                   CArg<"ArrayRef<Value>", "{}">:$indices)>
  ];
  let hasVerifier = 1;
}

def DbReleaseOp : Arts_Op<"db_release", []> {
  let summary = "Releases access to an ARTS DataBlock source.";
  let description = [{
    This operation releases the acquired source, signaling completion to the runtime.
    - release_type: specifies whether to perform memory freeing for dynamic allocations
  }];
  let arguments = (ins
    AnyMemRef:$source,
    OptionalAttr<DbReleaseTypeAttr>:$release_type
  );
  let assemblyFormat = [{
    `(` $source `)` `:` type($source) attr-dict}];

  let builders = [
    OpBuilder<(ins "Value"
               : $source)>,
    OpBuilder<(ins "Value"
               : $source, "DbReleaseType"
               : $release_type)>
  ];
}

def DbFreeOp : Arts_Op<"db_free", []> {
  let summary = "Frees a DataBlock allocated by arts.db_alloc.";
  let description = [{
    This operation frees a DataBlock that was previously allocated with arts.db_alloc.
    It is used for memory management of dynamically allocated datablocks.
  }];
  let arguments = (ins AnyMemRef : $source);
  let assemblyFormat = [{
    `(` $source `)` `:` type($source) attr-dict}];
}

def DbControlOp : Arts_Op<"db_control", [AttrSizedOperandSegments]> {
  let summary = "Creates a control dependency for arts.edt";
  let description = [{
    This operation creates a control dependency for the `arts.edt` operation.
    It is used to ensure that the DataBlock is ready before the EDT is executed.
    - mode: "in", "out", "inout"
    - ptr: base address of the DataBlock
    - indices: pinned indices for the subview (dimensions with size=1)
    - offsets: chunk offsets for the subview (dimensions with size>1)
    - sizes: block sizes for the subview (dimensions with size>1)
    Note: Currently ignored for execution, but retained for future correctness enforcement.
  }];
  let arguments = (ins
    ArtsModeAttr:$mode,
    AnyMemRef:$ptr,
    Variadic<Index>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs MemRefOf<[I64]> : $subview);
  let assemblyFormat = [{
    `[` $mode `]` `(` $ptr `:` type($ptr) `)`
    `indices` `[` $indices `]`
    `offsets` `[` $offsets `]`
    `sizes` `[` $sizes `]`
    attr-dict `:` type($subview)
  }];

  let builders = [
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$ptr,
                   CArg<"SmallVector<Value>", "{}">:$indices,
                   CArg<"SmallVector<Value>", "{}">:$offsets,
                   CArg<"SmallVector<Value>", "{}">:$sizes)>
  ];
}

def DbNumElementsOp : Arts_Op<"db_num_elements", [Pure]> {
  let summary = "Compute the number of elements in a datablock.";
  let description = [{
    Computes the total number of elements in a datablock. This is used
    to calculate dependency counts for EDT creation.
  }];
  let arguments = (ins Variadic<Index> : $sizes);
  let results = (outs Index : $count);
  let assemblyFormat = "`sizes` `(` $sizes `)` attr-dict `:` `->` type($count)";
  let hasCanonicalizer = 1;

  let builders = [
    OpBuilder<(ins "DbAllocOp"
               : $allocOp)>,
    OpBuilder<(ins "DbAcquireOp"
               : $acquireOp)>
  ];
}

def OmpDepOp : Arts_Op<"omp_dep", [Pure, AttrSizedOperandSegments]> {
  let summary =
      "OpenMP dependency descriptor for chunk-based task dependencies";
  let description = [{
    Captures dependency information from OpenMP task depend clauses during
    CanonicalizeMemrefs pass. This operation simplifies the conversion pipeline
    by avoiding complex subview/collapse operations that only exist to satisfy
    OpenMP's memref requirement. Instead, it directly captures:
    - Base array (canonicalized N-D array)
    - Access mode (in/out/inout)
    - Starting indices for each dimension
    - Block size for each dimension

    This operation is created in CanonicalizeMemrefs when transforming nested
    arrays to N-D arrays, and is consumed in ConvertOpenMPToArts to create
    DbControlOp operations with proper chunk support.

    Examples:
      /// Point dependency: depend(in: A[i][j])
      %dep = arts.omp_dep <in> %A_2D[%i, %j] size [%c1, %c1] : memref<?x?xf64>

      /// Chunk dependency: depend(out: A[i:BLOCK_SIZE][j:BLOCK_SIZE])
      %dep = arts.omp_dep <out> %A_2D[%i, %j] size [%bs, %bs] : memref<?x?xf64>

      /// Mixed: depend(in: A[i][j:COLS])
      %dep = arts.omp_dep <in> %A_2D[%i, %j] size [%c1, %cols] : memref<?x?xf64>

      /// Full array: depend(in: B[0:N][0:N])
      %dep = arts.omp_dep <in> %B[%c0, %c0] size [%N, %N] : memref<?x?xf64>
  }];

  let arguments = (ins
    ArtsModeAttr:$mode,   
    AnyMemRef:$source,    
    Variadic<Index>:$indices,        
    Variadic<Index>:$sizes           
  );

  let results = (outs AnyMemRef : $result);

  let assemblyFormat = [{
    `<` $mode `>` $source `[` $indices `]` `size` `[` $sizes `]`
    attr-dict `:` type($source) `->` type($result)
  }];

  let builders = [
    OpBuilder<(ins "ArtsMode":$mode,
                   "Value":$source,
                   CArg<"SmallVector<Value>", "{}">:$indices,
                   CArg<"SmallVector<Value>", "{}">:$sizes)>
  ];
}

///===----------------------------------------------------------------------===///
/// arts.edt
///===----------------------------------------------------------------------===///
def EdtOp : Arts_Op<"edt", [
  DeclareOpInterfaceMethods<MemoryEffectsOpInterface>,
  SingleBlockImplicitTerminator<"arts::YieldOp">, RecursiveMemoryEffects
]> {
  let summary = "Event-Driven Task in ARTS";
  let description = [{
    Defines an ARTS event -
        driven task(EDT) with : - `dependencies` that specify input /
                                output memory dependencies(acquired views)
                                    .

                                The single -
        block region contains the task body.- if 'parallel' attribute is set,
        it can spawn multiple tasks.- if 'single' attribute is set,
        it is guaranteed to run in a single task,
        even if it is called from a parallel region.-
            'concurrency' specifies the execution scope
        : intranode or internode.-
            'workers' specifies the number of parallel workers to use(optional)
                .-
            'nowait' indicates the EDT doesn't require an epoch wrapper (
                typically used when EDT is already inside an epoch)
                .
  }];
  let arguments = (ins
    EdtTypeAttr:$type,
    EdtConcurrencyAttr:$concurrency,
    I32:$route,
    OptionalAttr<WorkersAttr>:$workers,
    OptionalAttr<NoVerifyAttr>:$no_verify,
    OptionalAttr<UnitAttr>:$nowait,
    Variadic<AnyMemRef>:$dependencies
  );
  let regions = (region SizedRegion<1> : $body);
  let assemblyFormat = [{
    $type $concurrency `route` `(` $route `)` (`(` $dependencies^ `)` `:` type($dependencies))? attr-dict-with-keyword
    $body
  }];
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins "EdtType"
               : $type, "EdtConcurrency"
               : $concurrency)>,
    OpBuilder<(ins "EdtType"
               : $type, "EdtConcurrency"
               : $concurrency, "ValueRange"
               : $dependencies)>,
    OpBuilder<(ins "EdtType"
               : $type, "EdtConcurrency"
               : $concurrency, "Value"
               : $route, "ValueRange"
               : $dependencies)>
  ];
  let hasVerifier = 1;
  let extraClassDeclaration = [{
    SmallVector<Value> getDependenciesAsVector();
    void setDependencies(ValueRange newDeps);
    void appendDependency(Value dep);
  }];
}

///===----------------------------------------------------------------------===///
/// arts.epoch
///===----------------------------------------------------------------------===///
def EpochOp
    : Arts_Op<"epoch", [SingleBlockImplicitTerminator<"arts::YieldOp">]> {

  let summary = "Epoch region in ARTS";
  let description = [{
    An ARTS epoch region. It can spawn multiple tasks and
    synchronize them at the end of the epoch. Returns the epoch GUID
    for use by contained EDTs.
  }];
  let regions = (region SizedRegion<1> : $body);
  let results = (outs I64 : $epochGuid);

  let assemblyFormat =
      [{attr-dict-with-keyword $body `:` type($epochGuid)}];
}

///===----------------------------------------------------------------------===///
/// arts.create_epoch
///===----------------------------------------------------------------------===///
def CreateEpochOp : Arts_Op<"create_epoch", [MemoryEffects<[MemWrite]>]> {
  let summary = "Create an ARTS epoch and return its GUID";
  let description = [{
    Creates an ARTS epoch and returns its GUID. Unlike arts.epoch which wraps
    a region, this operation simply creates an epoch that can be passed around
    as a value. EDTs can be added to this epoch using EdtCreateOp with the
    epoch parameter. Use WaitOnEpochOp to wait for completion.
  }];
  let results = (outs I64 : $epochGuid);
  let assemblyFormat = "attr-dict `:` type($epochGuid)";
}

///===----------------------------------------------------------------------===///
/// arts.wait_on_epoch
///===----------------------------------------------------------------------===///
def WaitOnEpochOp : Arts_Op<"wait_on_epoch", [MemoryEffects<[MemWrite]>]> {
  let summary = "Wait for an ARTS epoch to complete";
  let description = [{
    Blocks until the specified epoch completes. All EDTs added to the epoch
    must finish before this operation returns.

    This is the counterpart to CreateEpochOp for barrier synchronization.
  }];
  let arguments = (ins I64 : $epochGuid);
  let assemblyFormat = "$epochGuid `:` type($epochGuid) attr-dict";
}

///===----------------------------------------------------------------------===///
/// arts.yield
///===----------------------------------------------------------------------===///
def YieldOp : Arts_Op<"yield", [
  Pure, ReturnLike, Terminator, ParentOneOf<["EdtOp", "EpochOp", "ForOp"]>
]> {

  let summary = "ARTS dialect yield terminator";
  let description = [{
    The `arts.yield` operation serves as a terminator for various 
    ARTS operations, such as `arts.epoch`, `arts.parallel`, `arts.single`, 
    and `arts.edt`. It ensures proper control flow within the ARTS dialect 
    by marking the end of a region.
  }];
  let assemblyFormat = [{attr-dict-with-keyword}];
}

///===----------------------------------------------------------------------===///
/// arts.barrier
///===----------------------------------------------------------------------===///
def BarrierOp : Arts_Op<"barrier", [MemoryEffects<[MemWrite]>]> {
  let summary = "Barrier synchronization in ARTS";
  let description =
      [{Barrier synchronization operation in
            ARTS.It synchronizes all previous spawned EDTs of the parent region
                .This operation has memory write effects as it affects the
                    synchronization state of the runtime system.}];
  let assemblyFormat = "attr-dict";
}

///===----------------------------------------------------------------------===///
/// arts.atomic_add
///===----------------------------------------------------------------------===///
def AtomicAddOp : Arts_Op<"atomic_add", [MemoryEffects<[MemWrite]>]> {
  let summary = "Atomic add to a scalar memref element";
  let description = [{
    Atomically adds a value to the element at the given address. This is the
    ARTS representation for reductions when converting from OpenMP.
    Only scalar memrefs (rank-0) are supported for now.
  }];

  let arguments = (ins AnyType : $addr, AnyType : $value);
  let assemblyFormat =
      "`(` $addr `,` $value `:` type($addr) `,` type($value) `)` attr-dict";
}

///===----------------------------------------------------------------------===///
/// arts.get_total_workers
///===----------------------------------------------------------------------===///
def GetTotalWorkersOp : Arts_Op<"get_total_workers", [Pure]> {
  let summary = "Get the total number of workers in ARTS";
  let description =
      [{The `arts
            .get_total_workers` operation retrieves the total number of workers
                available in the ARTS runtime environment.}];
  let results = (outs I32 : $result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [OpBuilder<(ins), [{
                              $_state.addTypes(
                                  IntegerType::get($_builder.getContext(), 32));
                            }]>];
}

///===----------------------------------------------------------------------===///
/// arts.get_total_nodes
///===----------------------------------------------------------------------===///
def GetTotalNodesOp : Arts_Op<"get_total_nodes", [Pure]> {
  let summary = "Get the total number of nodes in ARTS";
  let description =
      [{The `arts
            .get_total_nodes` operation retrieves the total number of nodes
                available in the ARTS runtime environment.}];
  let results = (outs I32 : $result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [OpBuilder<(ins), [{
                              $_state.addTypes(
                                  IntegerType::get($_builder.getContext(), 32));
                            }]>];
}

///===----------------------------------------------------------------------===///
/// arts.get_current_worker
///===----------------------------------------------------------------------===///
def GetCurrentWorkerOp : Arts_Op<"get_current_worker", [Pure]> {
  let summary = "Get the current worker ID in ARTS";
  let description = [{
      The `arts
          .get_current_worker` operation retrieves the ID of the current worker
              executing the operation in the ARTS runtime environment.}];
  let results = (outs I32 : $result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [OpBuilder<(ins), [{
                              $_state.addTypes(
                                  IntegerType::get($_builder.getContext(), 32));
                            }]>];
}

///===----------------------------------------------------------------------===///
/// arts.get_current_node
///===----------------------------------------------------------------------===///
def GetCurrentNodeOp : Arts_Op<"get_current_node", [Pure]> {
  let summary = "Get the current node ID in ARTS";
  let description =
      [{The `arts
            .get_current_node` operation retrieves the ID of the current node
                executing the operation in the ARTS runtime environment.}];
  let results = (outs I32 : $result);
  let assemblyFormat = "attr-dict `->` type($result)";
  let skipDefaultBuilders = 1;
  let builders = [OpBuilder<(ins), [{
                              $_state.addTypes(
                                  IntegerType::get($_builder.getContext(), 32));
                            }]>];
}

///===----------------------------------------------------------------------===///
/// arts.get_parallel_worker_id
///===----------------------------------------------------------------------===///
def GetParallelWorkerIdOp : Arts_Op<"get_parallel_worker_id", []> {
  let summary = "Get the parallel worker ID placeholder";
  let description = [{
    The `arts.get_parallel_worker_id` operation is a placeholder that will be
    replaced by ParallelEdtLowering with the actual worker ID (scf.for induction
    variable). This allows ForLowering pass to use the worker ID symbolically
    before the parallel loop structure is created.

    This operation is created by ForLowering and consumed by ParallelEdtLowering.
    It should not appear in the final lowered IR.
  }];
  let results = (outs Index : $result);
  let assemblyFormat = "attr-dict";
  let skipDefaultBuilders = 1;
  let builders =
      [OpBuilder<(ins), [{
                   $_state.addTypes(IndexType::get($_builder.getContext()));
                 }]>];
}

///===----------------------------------------------------------------------===///
/// EDT Lowering Operations
///===----------------------------------------------------------------------===///
def EdtCreateOp : Arts_Op<"edt_create", [MemoryEffects<[MemWrite]>]> {
  let summary = "Outlined EDT call with parameter pack and dependency count.";
  let description = [{
    Intermediate op for calling outlined EDT function with param array and total dependency count.
    The depCount is the sum of number of elements of all dependencies.
    Optional epochGuid parameter for EDT creation with specific epoch.
    Returns EDT guid.
  }];
  let arguments = (ins AnyMemRef:$param_memref, I32:$depCount, I32:$route, Optional<I64>:$epochGuid);
  let results = (outs I64 : $guid);

  let assemblyFormat = [{
    `(` $param_memref `:` type($param_memref) `)` 
    `depCount` `(` $depCount `)`
    `route` `(` $route `)` (`epoch` `(` $epochGuid^ `:` type($epochGuid) `)`)? attr-dict
  }];

  let builders = [
    OpBuilder<(ins "Value"
               : $param_memref, "Value"
               : $depCount)>,
    OpBuilder<(ins "Value"
               : $param_memref, "Value"
               : $depCount, "Value"
               : $epochGuid)>
  ];
}

def EdtParamPackOp : Arts_Op<"edt_param_pack", [MemoryEffects<[MemWrite]>]> {
  let summary = "Pack parameters into memref<?xi64>.";
  let description = [{
    Packs multiple parameters (cast to i64) into a memref for ARTS paramv.
  }];
  let arguments = (ins Variadic<AnyType> : $params);
  let results = (outs AnyMemRef : $memref);
  let assemblyFormat =
      "`(` $params `)` attr-dict `:` type($params) `:` type($memref)";
}

def EdtParamUnpackOp : Arts_Op<"edt_param_unpack", [Pure]> {
  let summary = "Unpack parameters from memref<?xi64>.";
  let description =
      [{Unpacks memref to multiple original types(load i64, cast back).}];
  let arguments = (ins AnyMemRef : $memref);
  let results = (outs Variadic<AnyType> : $unpacked);
  let assemblyFormat =
      "$memref `:` type($memref) attr-dict `:` `(` type($unpacked) `)`";
}

def RecordDepOp
    : Arts_Op<"rec_dep",
              [MemoryEffects<[MemWrite]>, AttrSizedOperandSegments]> {
  let summary = "Record in-dependencies for EDT.";
  let description = [{
    Records all in-dependencies (datablocks) for the EDT in order.
    Each datablock will be recorded as a dependency for the EDT and latches will
    be updated by the runtime as needed.
    The access_mode attribute specifies how to access the GUID values:
    - direct: GUIDs accessed via memref.load from DbAcquireOp results
    - from_depv: GUIDs accessed via dep_gep from EDT depv structure

    The acquire_modes array stores the DbMode for each acquire operation.
    When acquire mode differs from alloc mode, artsAddDependenceWithMode is used
    to enable runtime optimizations (e.g., skip frontier progression for read-only EDTs).

    The optional bounds_valids operand list contains i1 values corresponding to
    each datablock. When present and false for a datablock, artsRecordDep is
    skipped for that dependency (used for out-of-bounds stencil indices).

    The optional byte_offsets and byte_sizes operand lists enable ESD (Ephemeral
    Slice Dependencies) for stencil patterns. When a datablock has a non-zero
    byte_offset, the runtime uses artsRecordDepAt which:
    - Waits for the DB to be ready (proper dependency semantics)
    - Signals the EDT slot with db_ptr + byte_offset as ARTS_PTR
    - Preserves the original DB GUID for tracking
    When byte_offset is 0, standard artsRecordDep is used.
  }];
  let arguments = (ins
    I64:$edt_guid,
    Variadic<AnyType>:$datablocks,
    Variadic<I1>:$bounds_valids,
    Variadic<Index>:$byte_offsets,
    Variadic<Index>:$byte_sizes,
    DefaultValuedAttr<DepAccessModeAttr, "::mlir::arts::DepAccessMode::direct">:$access_mode,
    OptionalAttr<DenseI32ArrayAttr>:$acquire_modes
  );
  let hasVerifier = 1;
  let assemblyFormat = [{
    $edt_guid `(` $datablocks `:` type($datablocks) `)`
    ( `bounds_valids` `(` $bounds_valids^ `)` )?
    ( `byte_offsets` `(` $byte_offsets^ `)` )?
    ( `byte_sizes` `(` $byte_sizes^ `)` )?
    attr-dict
  }];
}

def IncrementDepOp : Arts_Op<"inc_dep", [MemoryEffects<[MemWrite]>]> {
  let summary = "Increment out-latches for EDT dependencies.";
  let description = [{
    Increments latch count for all out-dependencies (datablocks) of the EDT.
    Each datablock will have its output latch incremented.
    The access_mode attribute specifies how to access the GUID values:
    - direct: GUIDs accessed via memref.load from DbAcquireOp results
    - from_depv: GUIDs accessed via dep_gep from EDT depv structure
  }];
  let arguments = (ins
    I64:$edt_guid,
    Variadic<AnyType>:$datablocks,
    DefaultValuedAttr<DepAccessModeAttr, "::mlir::arts::DepAccessMode::direct">:$access_mode
  );
  let assemblyFormat =
      "$edt_guid `(` $datablocks `:` type($datablocks) `)` attr-dict";
}

///===----------------------------------------------------------------------===///
/// Dependency Structure Access Operations
///===----------------------------------------------------------------------===///

def DepGepOp : Arts_Op<"dep_gep", [Pure, AttrSizedOperandSegments]> {
  let summary = "Extract pointer field from an EDT dependency structure";
  let description =
      [{Extracts the pointer field from an EDT dependency structure
            .The input can be a pointer to a dependency structure or a memref of
                dependency structures
            .Optional indices specify which dependency entry to access when the
                input is an array of dependencies(
                    e.g., when a DbAlloc / Acquire returned multiple datablocks)
            .}];
  let arguments = (ins AnyType:$dep_struct, Index:$offset,
                       Variadic<AnyType>:$indices, Variadic<Index>:$strides);
  let results = (outs AnyType : $guid, AnyType : $ptr);
  let assemblyFormat = [{
    `(` $dep_struct `)`
     `offset` `[` $offset `:` type($offset) `]`
    ( `indices` `[` $indices^ `:` type($indices) `]` )?
    ( `strides` `[` $strides^ `]` )?
    attr-dict `:` type($dep_struct) `->` type($guid) `,` type($ptr)
  }];
}

def DepDbAcquireOp
    : Arts_Op<"dep_db_acquire", [Pure, AttrSizedOperandSegments]> {
  let summary = "Acquires a datablock from EDT dependency structure with "
                "subregion access.";
  let description = [{
    Extracts GUID and pointer from an EDT dependency structure at the specified
    offset with optional subregion access (indices, offsets, sizes).
    Used within outlined EDT functions to access datablocks that were
    passed as dependencies, similar to DbAcquireOp but from dependency structures.

    The optional bounds_valid operand indicates whether the indices are within
    bounds of the source datablock. When present and false, the dependency
    recording should be skipped (used for stencil boundary conditions).
  }];
  let arguments = (ins
    AnyType:$dep_struct,
    Index:$offset,
    Variadic<AnyType>:$indices,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes,
    Optional<I1>:$bounds_valid
  );
  let results = (outs AnyType : $guid, AnyType : $ptr);
  let assemblyFormat = [{
    `(` $dep_struct `)`
    `offset` `[` $offset `:` type($offset) `]`
    ( `indices` `[` $indices^ `:` type($indices) `]` )?
    ( `offsets` `[` $offsets^ `]` )?
    ( `sizes` `[` $sizes^ `]` )?
    ( `bounds_valid` `(` $bounds_valid^ `)` )?
    attr-dict `:` type($dep_struct) `->` type($guid) `,` type($ptr)
  }];
}

def DbGepOp : Arts_Op<"db_gep", [Pure, AttrSizedOperandSegments]> {
  let summary = "Compute element pointer (LLVM ptr) using byte strides.";
  let description = [{Computes a raw LLVM pointer to an element from a base
                          pointer using indices and strides
                              .Always produces an LLVM pointer result regardless
                                  of the input type.}];
  let arguments = (
    ins AnyType:$base_ptr, 
    Variadic<Index>:$indices, 
    Variadic<Index>:$strides
  );
  let results = (outs AnyType : $ptr);
  let assemblyFormat = [{
    `(` $base_ptr `:` type($base_ptr) `)`
    `indices` `[` $indices `]` 
    `strides` `[` $strides `]`
    attr-dict `:` type($ptr)
  }];
  let builders = [OpBuilder<(ins "Type"
                             : $ptr, "Value"
                             : $base_ptr, "SmallVector<Value>"
                             : $indices, "SmallVector<Value>"
                             : $strides)>];
}

def ForOp : Arts_Op<"for", [
  AttrSizedOperandSegments, SingleBlockImplicitTerminator<"arts::YieldOp">
]> {
  let summary = "ARTS parallel for loop operation";
  let description = [{
    The `arts.for` operation represents a parallel for loop in the ARTS dialect.
    It preserves the parallel semantics from OpenMP worksharing loops while
    providing a native ARTS construct for parallel iteration.
    
    This operation takes lower bound, upper bound, and step values, and executes
    the loop body in parallel across available workers.
  }];

  let arguments = (ins Variadic<Index>:$lowerBound, 
                      Variadic<Index>:$upperBound, 
                      Variadic<Index>:$step,
                      OptionalAttr<ForScheduleKindAttr>:$schedule,
                      Variadic<AnyType>:$reductionAccumulators);
  let results = (outs);
  let regions = (region AnyRegion : $region);

  let assemblyFormat = [{
    `(` $lowerBound `)` `to` `(` $upperBound `)` `step` `(` $step `)`
    ( `schedule` `(` $schedule^ `)` )?
    ( `reduction` `(` $reductionAccumulators^ `:` type($reductionAccumulators) `)` )?
    `{` $region `}` attr-dict
  }];

  let hasCanonicalizer = true;
}

#endif // ARTS_OPS
