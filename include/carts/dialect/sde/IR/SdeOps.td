///==========================================================================///
/// File: SdeOps.td
/// This file defines the SDE dialect operations: CU, SU, MU, and yield.
///==========================================================================///

#ifndef ARTS_SDE_OPS
#define ARTS_SDE_OPS

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"
include "carts/IR/CommonAttrs.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/Interfaces/LoopLikeInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

include "carts/dialect/sde/IR/SdeDialect.td"
include "carts/dialect/sde/IR/SdeTypes.td"

def SdeDepType
    : Type<CPred<"::llvm::isa<::mlir::carts::sde::DepType>($_self)">,
           "SDE dependency token">;
def SdeCompletionType
    : Type<CPred<"::llvm::isa<::mlir::carts::sde::CompletionType>($_self)">,
           "SDE completion token">;
def SdeTokenType
    : Type<CPred<"::llvm::isa<::mlir::carts::sde::TokenType>($_self)">,
           "SDE memref access token">;

//===----------------------------------------------------------------------===//
// Enum helpers
//===----------------------------------------------------------------------===//

class CartsSde_I32Enum<string name, string description,
                      list<I32EnumAttrCase> cases>
    : I32EnumAttr<name, description, cases> {
  let cppNamespace = "::mlir::carts::sde";
  let genSpecializedAttr = 0;
}

class CartsSde_I32EnumAttr<string mnemonic, CartsSde_I32Enum enumInfo>
    : EnumAttr<CartsSdeDialect, enumInfo, mnemonic> {
  let assemblyFormat = "`<` $value `>`";
}

//===----------------------------------------------------------------------===//
// Enums
//===----------------------------------------------------------------------===//

def SdeCuKind
    : CartsSde_I32Enum<"SdeCuKind", "Compute unit kind", [
      I32EnumAttrCase<"parallel", 0, "parallel">,
      I32EnumAttrCase<"single", 1, "single">,
      I32EnumAttrCase<"task", 2, "task">
    ]>;
def SdeCuKindAttr : CartsSde_I32EnumAttr<"cu_kind", SdeCuKind>;

def SdeAccessMode
    : CartsSde_I32Enum<"SdeAccessMode", "Memory access mode", [
      I32EnumAttrCase<"read", 0, "read">,
      I32EnumAttrCase<"write", 1, "write">,
      I32EnumAttrCase<"readwrite", 2, "readwrite">
    ]>;
def SdeAccessModeAttr : CartsSde_I32EnumAttr<"access_mode", SdeAccessMode>;

def SdeReductionKind
    : CartsSde_I32Enum<"SdeReductionKind", "Reduction operation kind", [
      I32EnumAttrCase<"add", 0, "add">,
      I32EnumAttrCase<"mul", 1, "mul">,
      I32EnumAttrCase<"min", 2, "min">,
      I32EnumAttrCase<"max", 3, "max">,
      I32EnumAttrCase<"land", 4, "and">,
      I32EnumAttrCase<"lor", 5, "or">,
      I32EnumAttrCase<"lxor", 6, "xor">,
      I32EnumAttrCase<"custom", 7, "custom">
    ]>;
def SdeReductionKindAttr : CartsSde_I32EnumAttr<"reduction_kind", SdeReductionKind>;

def SdeReductionStrategy
    : CartsSde_I32Enum<"SdeReductionStrategy",
                      "Reduction execution strategy",
                      CartsReductionStrategyCases>;
def SdeReductionStrategyAttr
    : CartsSde_I32EnumAttr<"reduction_strategy", SdeReductionStrategy>;

def SdeStructuredClassification
    : CartsSde_I32Enum<"SdeStructuredClassification",
                      "Structured loop classification", [
      I32EnumAttrCase<"elementwise", 0, "elementwise">,
      I32EnumAttrCase<"stencil", 1, "stencil">,
      I32EnumAttrCase<"matmul", 2, "matmul">,
      I32EnumAttrCase<"reduction", 3, "reduction">,
      I32EnumAttrCase<"elementwise_pipeline", 4, "elementwise_pipeline">
    ]>;
def SdeStructuredClassificationAttr
    : CartsSde_I32EnumAttr<"structured_classification",
                          SdeStructuredClassification>;

def SdePattern
    : CartsSde_I32Enum<"SdePattern",
                      "SDE dependency and schedule family", [
      I32EnumAttrCase<"uniform", 0, "uniform">,
      I32EnumAttrCase<"stencil_tiling_nd", 1, "stencil_tiling_nd">,
      I32EnumAttrCase<"cross_dim_stencil_3d", 2, "cross_dim_stencil_3d">,
      I32EnumAttrCase<"higher_order_stencil", 3, "higher_order_stencil">,
      I32EnumAttrCase<"wavefront_2d", 4, "wavefront_2d">,
      I32EnumAttrCase<"alternating_buffer_stencil", 5,
                      "alternating_buffer_stencil">,
      I32EnumAttrCase<"matmul", 6, "matmul">,
      I32EnumAttrCase<"elementwise_pipeline", 7, "elementwise_pipeline">,
      I32EnumAttrCase<"reduction", 8, "reduction">
    ]>;
def SdePatternAttr
    : CartsSde_I32EnumAttr<"pattern", SdePattern>;

def SdeScheduleKind
    : CartsSde_I32Enum<"SdeScheduleKind", "Scheduling policy", [
      I32EnumAttrCase<"static_", 0, "static">,
      I32EnumAttrCase<"dynamic", 1, "dynamic">,
      I32EnumAttrCase<"guided", 2, "guided">,
      I32EnumAttrCase<"auto_", 3, "auto">,
      I32EnumAttrCase<"runtime", 4, "runtime">
    ]>;
def SdeScheduleKindAttr : CartsSde_I32EnumAttr<"schedule", SdeScheduleKind>;

def SdeDistributionKind
    : CartsSde_I32Enum<"SdeDistributionKind", "Distribution advisory hint", [
      I32EnumAttrCase<"owner_compute", 0, "owner_compute">,
      I32EnumAttrCase<"blocked", 1, "blocked">,
      I32EnumAttrCase<"cyclic", 2, "cyclic">
    ]>;
def SdeDistributionKindAttr
    : CartsSde_I32EnumAttr<"distribution_kind", SdeDistributionKind>;

def SdeIterationTopology
    : CartsSde_I32Enum<"SdeIterationTopology",
                      "SDE element-space iteration topology",
                      CartsIterationTopologyCases>;
def SdeIterationTopologyAttr
    : CartsSde_I32EnumAttr<"iteration_topology", SdeIterationTopology>;

def SdeRepetitionStructure
    : CartsSde_I32Enum<"SdeRepetitionStructure",
                      "SDE repeated-wave or timestep structure", [
      I32EnumAttrCase<"none", 0, "none">,
      I32EnumAttrCase<"pair_step", 1, "pair_step">,
      I32EnumAttrCase<"k_step", 2, "k_step">,
      I32EnumAttrCase<"full_timestep", 3, "full_timestep">
    ]>;
def SdeRepetitionStructureAttr
    : CartsSde_I32EnumAttr<"repetition_structure",
                          SdeRepetitionStructure>;

def SdeAsyncStrategy
    : CartsSde_I32Enum<"SdeAsyncStrategy",
                      "SDE async task-shape strategy", [
      I32EnumAttrCase<"blocking", 0, "blocking">,
      I32EnumAttrCase<"advance_stage", 1, "advance_stage">
    ]>;
def SdeAsyncStrategyAttr
    : CartsSde_I32EnumAttr<"async_strategy", SdeAsyncStrategy>;

def SdeBarrierReason
    : CartsSde_I32Enum<"SdeBarrierReason",
                      "SDE barrier preservation or elimination reason",
                      CartsBarrierReasonCases>;
def SdeBarrierReasonAttr
    : CartsSde_I32EnumAttr<"barrier_reason", SdeBarrierReason>;

def SdeResourceQueryKind
    : CartsSde_I32Enum<"SdeResourceQueryKind",
                      "Target-neutral SDE resource query kind", [
      I32EnumAttrCase<"logicalWorkers", 0, "logical_workers">
    ]>;
def SdeResourceQueryKindAttr
    : CartsSde_I32EnumAttr<"resource_query_kind", SdeResourceQueryKind>;

// Geometric redistribution movement family authored by SDE from committed
// per-array layout facts. It names a GEOMETRIC movement shape (how owner blocks
// move between two layouts), never a concrete collective, transport algorithm,
// route, or runtime object — CODIR maps the family to a transport mechanically.
def SdeMovementFamily
    : CartsSde_I32Enum<"SdeMovementFamily",
                      "SDE geometric redistribution movement family", [
      I32EnumAttrCase<"broadcast_like", 0, "broadcast_like">,
      I32EnumAttrCase<"all_gather_like", 1, "all_gather_like">,
      I32EnumAttrCase<"all_to_all_like", 2, "all_to_all_like">,
      I32EnumAttrCase<"reduce_scatter_like", 3, "reduce_scatter_like">,
      I32EnumAttrCase<"allreduce_like", 4, "allreduce_like">,
      I32EnumAttrCase<"halo_like", 5, "halo_like">,
      I32EnumAttrCase<"phase_redist", 6, "phase_redist">
    ]>;
def SdeMovementFamilyAttr
    : CartsSde_I32EnumAttr<"movement_family", SdeMovementFamily>;

//===----------------------------------------------------------------------===//
// Resource Ops
//===----------------------------------------------------------------------===//

def SdeResourceQueryOp : CartsSde_Op<"resource_query", [Pure]> {
  let summary = "Query target-neutral SDE execution resources";
  let description = [{
    Queries a runtime-neutral execution resource needed to build SDE-level
    symbolic grain expressions. The result is target-neutral SDE intent; the
    dialect boundary binds it to the concrete target mechanism.

    `logical_workers` is the number of logical execution slots available to an
    SDE scheduling unit. It does not name a concrete runtime worker,
    placement, or task object.
  }];
  let arguments = (ins SdeResourceQueryKindAttr:$kind);
  let results = (outs Index:$result);
  let assemblyFormat = "$kind attr-dict";
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins "SdeResourceQueryKind":$kind), [{
      $_state.addTypes($_builder.getIndexType());
      $_state.addAttribute("kind",
          SdeResourceQueryKindAttr::get($_builder.getContext(), kind));
    }]>
  ];
}

//===----------------------------------------------------------------------===//
// CU (Compute Unit) Ops
//===----------------------------------------------------------------------===//

def SdeCuRegionOp
    : CartsSde_Op<"cu_region", [
      SingleBlock,
      RecursiveMemoryEffects
    ]> {
  let summary = "Parallel or single execution region";
  let description = [{
    Captures `omp.parallel`, `omp.single`, `omp.master`. Represents a
    source-level compute region. The region kind records OpenMP execution
    semantics such as parallel, single, or task; concrete machine topology and
    distributed placement are resolved later.

    The `nowait` attribute preserves OpenMP nowait semantics. The
    Optional `iter_args` thread loop-carried SSA values through the region body.
    When present, the body block receives matching block arguments and the
    region must terminate with `sde.yield` returning updated values. When
    absent, the body may omit a terminator or yield empty.
  }];

  let arguments = (ins
    SdeCuKindAttr:$kind,
    UnitAttr:$nowait,
    Variadic<AnyType>:$iterArgs
  );

  let results = (outs Variadic<AnyType>:$results);

  let regions = (region SizedRegion<1>:$body);

  let hasVerifier = 1;
  let hasCustomAssemblyFormat = 1;
}

def SdeCuTaskOp
    : CartsSde_Op<"cu_task", [
      SingleBlock, NoTerminator,
      RecursiveMemoryEffects
    ]> {
  let summary = "Task with explicit data dependencies";
  let description = [{
    Captures `omp.task` with `depend(in/out/inout)` clauses. Dependencies
    are specified via `sde.mu_dep` results.
  }];

  let arguments = (ins
    Variadic<SdeDepType>:$deps,
    OptionalAttr<SdePatternAttr>:$pattern
  );

  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    (`deps` `(` $deps^ `:` type($deps) `)`)? $body attr-dict
  }];
}

def SdeControlTokenOp
    : CartsSde_Op<"control_token", [MemoryEffects<[MemWrite]>]> {
  let summary = "SDE completion/control token";
  let description = [{
    Produces an SDE-level completion token for task and wait control edges.
    The token is consumed by SDE synchronization surfaces such as
    `sde.su_barrier` and is erased during boundary materialization after those
    control edges have been consumed.
  }];

  let results = (outs SdeCompletionType:$token);

  let assemblyFormat = "attr-dict `:` type($token)";
}

def SdeCuReduceOp
    : CartsSde_Op<"cu_reduce", [MemoryEffects<[MemRead, MemWrite]>]> {
  let summary = "Reduction accumulation";
  let description = [{
    Captures reduction semantics from `omp.wsloop reduction(+: sum)`.
    PRESERVES the combiner semantics that the current pipeline loses.

    The reduction kind (add, mul, min, max, etc.) and identity value are
    explicit. For custom reductions, the combiner region carries the
    reduction body.
  }];

  let arguments = (ins
    SdeReductionKindAttr:$reduction_kind,
    AnyType:$accumulator,
    AnyType:$partial,
    AnyType:$identity
  );

  let results = (outs AnyType:$result);

  let regions = (region AnyRegion:$combiner);

  let assemblyFormat = [{
    $reduction_kind `(` $accumulator `,` $partial `:` type($accumulator) `,` type($partial) `)`
    `identity` `(` $identity `:` type($identity) `)`
    ($combiner^)? attr-dict `->` type($result)
  }];
}

def SdeCuAtomicOp
    : CartsSde_Op<"cu_atomic", [MemoryEffects<[MemWrite]>]> {
  let summary = "Atomic memory update";
  let description = [{
    Captures `omp.atomic.update`. Currently only supports add{i,f}.
    Memref-only since atomics are inherently side-effecting operations
    on mutable memory locations.
  }];

  let arguments = (ins
    SdeReductionKindAttr:$reduction_kind,
    AnyMemRef:$addr,
    AnyType:$value
  );

  let assemblyFormat = [{
    $reduction_kind `(` $addr `,` $value `:` type($addr) `,` type($value) `)`
    attr-dict
  }];
}

//===----------------------------------------------------------------------===//
// SU (Scheduling Unit) Ops
//===----------------------------------------------------------------------===//

def SdeSuIterateOp
    : CartsSde_Op<"su_iterate", [
      SingleBlock,
      RecursiveMemoryEffects,
      AttrSizedOperandSegments,
      DeclareOpInterfaceMethods<LoopLikeOpInterface,
        ["getLoopRegions", "getLoopInductionVars", "getLoopLowerBounds",
         "getLoopUpperBounds", "getLoopSteps"]>
    ]> {
  let summary = "Parallel iteration space";
  let description = [{
    Captures `omp.wsloop + omp.loop_nest`, `omp.taskloop`, `scf.parallel`.
    Represents a parallel iteration space with optional schedule, chunk size,
    nowait, and reduction metadata.

    Reduction accumulators can carry the combiner kind and identity value,
    preserving information lost by the current pipeline.

    Optional `iter_args` thread loop-carried SSA values through the loop body.
    When present, the body block receives matching block arguments (after
    induction variables) and the region must terminate with `sde.yield`
    returning updated values.  Results match iter_args 1:1.

    When iter_args are absent the op produces no results and the body
    terminates with an empty `sde.yield`.
  }];

  let arguments = (ins
    Variadic<Index>:$lowerBounds,
    Variadic<Index>:$upperBounds,
    Variadic<Index>:$steps,
    OptionalAttr<SdeScheduleKindAttr>:$schedule,
    Optional<Index>:$chunkSize,
    UnitAttr:$nowait,
    Variadic<AnyType>:$reductionAccumulators,
    OptionalAttr<ArrayAttr>:$reductionKinds,
    OptionalAttr<SdeReductionStrategyAttr>:$reductionStrategy,
    UnitAttr:$partialReduction,
    OptionalAttr<ArrayAttr>:$partialReductionDims,
    OptionalAttr<ArrayAttr>:$partialReductionOwnerDims,
    OptionalAttr<SdeStructuredClassificationAttr>:$structuredClassification,
    OptionalAttr<SdePatternAttr>:$pattern,
    OptionalAttr<ArrayAttr>:$accessMinOffsets,
    OptionalAttr<ArrayAttr>:$accessMaxOffsets,
    OptionalAttr<ArrayAttr>:$ownerDims,
    OptionalAttr<ArrayAttr>:$spatialDims,
    OptionalAttr<ArrayAttr>:$writeFootprint,
    // Element-space/storage layout facts authored by SDE. These do not encode
    // target nodes, routes, or concrete worker placement.
    OptionalAttr<ArrayAttr>:$physicalOwnerDims,
    OptionalAttr<ArrayAttr>:$physicalBlockShape,
    OptionalAttr<ArrayAttr>:$logicalWorkerSlice,
    OptionalAttr<ArrayAttr>:$physicalHaloShape,
    OptionalAttr<SdeIterationTopologyAttr>:$iterationTopology,
    OptionalAttr<SdeRepetitionStructureAttr>:$repetitionStructure,
    OptionalAttr<SdeAsyncStrategyAttr>:$asyncStrategy,
    OptionalAttr<SdeDistributionKindAttr>:$distributionKind,
    UnitAttr:$inPlaceSafe,
    UnitAttr:$inPlaceSharedState,
    // --- Module-scoped layout assignment. Placed last so the positional
    // op builder stays additive: existing create() sites that stop at
    // inPlaceSharedState keep compiling. ---
    // Per-array BLOCK layout authored by the `sde-layout-assignment` pass. Pure
    // DATA-LAYOUT intent in the spirit of HPF DISTRIBUTE/ALIGN: for every array
    // root this scheduling unit accesses (writer AND readers — the key
    // generalization over the output-only physical owner plan), a dictionary
    // entry records the chosen BLOCK layout. Each entry is a DictionaryAttr
    // with keys:
    //   - `arrayId`    (I64): a module-stable id joining the same array root
    //                         across the writer and all reader su_iterates.
    //   - `kind`       (StringAttr): "block_parallel", "block_contraction", or
    //                         "replicated" — geometric layout family only,
    //                         geometric layout family only, no target object,
    //                         no runtime communication operation, no pattern.
    //   - `ownerDims`  (ArrayAttr<I64>): the element-space owner (distributed)
    //                         positions of the array.
    //   - `blockShape` (ArrayAttr<I64>): element-space per-block extents
    //                         (full extent on non-owner positions).
    //   - `muBlockCount` (I64): number of memory-unit blocks implied by the
    //                         owner dims and block shape.
    // Element-space only: encodes NO target nodes, routes, concrete worker
    // placement, communication primitive, or split factor. Downstream consumers use the
    // facts through boundary planning carriers; storage-view,
    // communication-family, and target-object realization remain outside SDE.
    OptionalAttr<ArrayAttr>:$arrayLayout,
    // Per-edge geometric layouts-disagree marker. An ArrayAttr of the
    // `arrayId`s (I64) whose chosen layout on this scheduling unit is a pure
    // GEOMETRIC mismatch against the array's home/writer layout (e.g. an input
    // read on a non-owner-aligned window, or a contraction-dim read of a
    // row-block intermediate). Records WHERE a redistribution edge exists, as
    // geometry only and does not pick a concrete remedy.
    OptionalAttr<ArrayAttr>:$layoutsDisagree,
    // Abstract communication-volume estimate (bytes) for this scheduling unit
    // under the chosen layouts. Advisory only.
    OptionalAttr<I64Attr>:$commVolumeBytes
  );

  let results = (outs Variadic<AnyType>:$results);

  let regions = (region SizedRegion<1>:$body);

  let hasVerifier = 1;
  let hasCustomAssemblyFormat = 1;
}

def SdeSuDistributeOp
    : CartsSde_Op<"su_distribute", [
      SingleBlock, NoTerminator,
      RecursiveMemoryEffects
    ]> {
  let summary = "Distribution advisory wrapper for SDE work";
  let description = [{
    Wraps a scheduling-unit region with a runtime-neutral distribution hint.
    This is an SDE-level advisory carrier only; it does not encode concrete
    worker, route, or node identifiers.
  }];

  let arguments = (ins SdeDistributionKindAttr:$kind);
  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    $kind $body attr-dict
  }];
}

def SdeSuBarrierOp
    : CartsSde_Op<"su_barrier", [MemoryEffects<[MemWrite]>]> {
  let summary = "Synchronization point";
  let description = [{
    Captures `omp.barrier` (implicit) and `omp.taskwait` (explicit tokens).
    When tokens are provided, waits only for those specific tasks.
  }];

  let arguments = (ins
    Variadic<SdeCompletionType>:$tokens,
    UnitAttr:$barrierEliminated,
    OptionalAttr<SdeBarrierReasonAttr>:$barrierReason
  );

  let assemblyFormat = [{
    (`(` $tokens^ `:` type($tokens) `)`)? attr-dict
  }];
}

//===----------------------------------------------------------------------===//
// MU (Memory Unit) Ops
//===----------------------------------------------------------------------===//

def SdeMuDepOp
    : CartsSde_Op<"mu_dep", [Pure, AttrSizedOperandSegments]> {
  let summary = "SDE memref dependency declaration";
  let description = [{
    Declares a memory-unit dependency with access mode and region: the SDE
    representation of source dependency slices from `omp.task depend(...)` and of
    SDE-authored owner-slice dependencies for raw memref work. It is distinct
    from `sde.control_token` (ordering/completion) and `sde.mu_token` (an MU
    access token).

    `sde.mu_dep` is an SDE-only declaration consumed by the SDE-to-CODIR/ARTS
    conversion, which materializes concrete storage and access requests; it does
    not survive the dialect boundary.
  }];

  let arguments = (ins
    SdeAccessModeAttr:$mode,
    AnyMemRef:$source,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs SdeDepType:$dep);

  let assemblyFormat = [{
    $mode $source (`[` $offsets^ `]`)? (`size` `[` $sizes^ `]`)?
    `:` type($source) `->` type($dep) attr-dict
  }];
}

def SdeMuTokenOp
    : CartsSde_Op<"mu_token", [Pure, AttrSizedOperandSegments]> {
  let summary = "Produce an MU access token (whole storage or slice).";
  let description = [{
    Names a memref (or a rectangular slice of one) for consumption by a
    `sde.cu_work`, with an explicit access mode. When `offsets` / `sizes`
    are omitted the token addresses the whole storage object.

    Two tokens with disjoint slices of the same source expose slice
    parallelism that would be serialized if compute units took raw storage
    operands; the SSA dep edge still flows through the parent storage value,
    but analyses see the disjointness.
  }];

  let arguments = (ins
    SdeAccessModeAttr:$mode,
    AnyMemRef:$source,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );

  let results = (outs SdeTokenType:$token);

  let assemblyFormat = [{
    $mode $source (`[` $offsets^ `]`)? (`size` `[` $sizes^ `]`)?
    `:` type($source) `->` type($token) attr-dict
  }];

  let hasVerifier = 1;
}

def SdeCuWorkOp
    : CartsSde_Op<"cu_work", [
      IsolatedFromAbove,
      SingleBlock,
      RecursiveMemoryEffects,
      AttrSizedOperandSegments
    ]> {
  let summary = "Isolated compute unit consumed by the SDE-to-CODIR conversion.";
  let description = [{
    The isolated compute-unit form that `convert-sde-to-codir` lowers into a
    `codir.codelet`. Its only communication with the enclosing scope is through
    `sde.mu_token` operands and explicit capture operands. Memref tokens pass
    mutable views directly and do not require yielded replacement values.

    Operand shape: token operands are `!sde.token<memref<...>>`; scalar
    captures are ordinary SSA values made available to the isolated compute
    body. The body's block arguments first list one memref view per token,
    typed as the token's `slice_type`, followed by one block argument per
    explicit capture.

    `IsolatedFromAbove` guarantees the body references no SSA value from
    outside its block arguments. Dependencies are SSA-visible via the token
    operand list; firstprivate-style scalar values are SSA-visible via the
    capture operand list.
  }];

  let arguments = (ins
    Variadic<SdeTokenType>:$tokens,
    Variadic<AnyType>:$captures
  );
  let regions   = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    `(` ($tokens^ `:` type($tokens))? `)`
    (`captures` `(` $captures^ `:` type($captures) `)`)?
    $body attr-dict
  }];

  let hasVerifier = 1;
}

def SdeMuAllocOp : CartsSde_Op<"mu_alloc", [Pure]> {
  let summary = "Allocate an SDE memref value with given shape.";
  let description = [{
    Creates a memref SSA value for an SDE memory unit. This is the SDE-level
    declaration of storage with static or dynamic memref dimensions.

    Examples:
      %arr = sde.mu_alloc : memref<100xi32>
      %mat = sde.mu_alloc(%n, %m) : memref<?x?xf64>
  }];
  let arguments = (ins Variadic<Index>:$dynamicSizes);
  let results = (outs AnyMemRef:$memref);
  let assemblyFormat = [{
    (`(` $dynamicSizes^ `)`)? attr-dict `:` type($memref)
  }];
  let hasVerifier = 1;
}

def SdeMuAccessWindowOp : CartsSde_Op<"mu_access_window", [
      // NOT Pure: a zero-result structural fact must survive DCE until a
      // verifier or lowering consumes it.
      MemoryEffects<[MemWrite]>
    ]> {
  let summary = "Per-CU MU access-window fact in block-grid coordinates";
  let description = [{
    Raised by `raise-to-mu-access-window` after the final SDE shape rewrites.
    Declares, for one rank-expanded MU root accessed by the enclosing CU, the
    canonical access window in the SAME block-grid coordinate system that
    `sde-rank-expand-mu` established on the `sde.mu_alloc` result type.

    Placed at the top of the enclosing `sde.cu_region` body, one per
    (MU root, access mode). The `mu` operand is the rank-expanded `sde.mu_alloc`
    result (visible because `sde.cu_region` is NOT `IsolatedFromAbove`). It names
    no collective, DB, EDT, owner map, route, GUID, or runtime policy — it is a
    pure SDE structural fact, consumed (not reinterpreted) by mechanical
    SDE-to-CODIR lowering.

    Attributes (block-grid coordinates, all static `I64 ArrayAttr`):
      - `mode`          : SdeAccessMode, `read` or `write`. A CU that both reads
                          and writes the same MU (in-place) is out of scope
                          and gets no window.
      - `ownerDimCount` : number of leading owner (grid) dims.
      - `blockLo`/`blockHi` : per-owner-dim half-open block range [lo, hi) the CU
                          touches; `blockHi[k]` == grid dim k of the expanded
                          type. Length == `ownerDimCount`.
      - `validExtents`  : per-logical-dim valid element extent within a block
                          (== the tile dims of the expanded type). Length ==
                          logical rank == `muType.getRank() - ownerDimCount`.

    For a ceil/partial last block the per-block valid extent is uniform
    `blockExtent`; the partial last-block extent is derivable by a consumer from
    the writer `sde.su_iterate` iteration domain (the committed source of truth),
    not recomputed here. Stencil halo growth is NOT encoded (deferred to a later
    halo-redistribution transform); `validExtents` is the in-tile extent.

    This op is NOT part of the default `-O3` pipeline.
  }];

  let arguments = (ins
    AnyMemRef:$mu,
    SdeAccessModeAttr:$mode,
    I64Attr:$ownerDimCount,
    ArrayAttr:$blockLo,
    ArrayAttr:$blockHi,
    ArrayAttr:$validExtents
  );

  // No results: a structural fact op, not a value producer.
  let results = (outs);

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

def SdeRedistOp : CartsSde_Op<"redist", [
      // NOT Pure: a zero-result structural fact must survive DCE until a
      // verifier or lowering consumes it.
      MemoryEffects<[MemWrite]>
    ]> {
  let summary = "SDE layout-to-layout redistribution boundary on one array";
  let description = [{
    Makes an array's redistribution edge explicit SDE structure. For one array
    root whose committed home (writer) BLOCK layout differs from a consumer's
    required layout (the geometric `layoutsDisagree` edge `sde-layout-assignment`
    records), `sde.redist` commits the concrete source layout, target layout, and
    geometric movement family, derived VERBATIM from the committed `arrayLayout`
    facts — never re-deriving owner dims, block shape, or the family.

    It names a GEOMETRIC movement family only (`broadcast_like`,
    `all_gather_like`, `all_to_all_like`, `reduce_scatter_like`,
    `allreduce_like`, `halo_like`, `phase_redist`). It names no collective,
    transport algorithm, DB, EDT, owner map, route, GUID, or runtime policy:
    CODIR maps the family to a concrete collective/bridge mechanically. Like
    `sde.mu_dep`/`sde.mu_access_window` it is consumed (not reinterpreted) by
    SDE-to-CODIR lowering and does NOT survive the dialect boundary.

    Attributes (element-space geometry, all static `I64 ArrayAttr`):
      - `family`            : the geometric movement family.
      - `sourceOwnerDims`/`sourceBlockShape` : the home (writer) layout. Empty
                              `sourceOwnerDims` with a full-extent block means a
                              replicated source.
      - `targetOwnerDims`/`targetBlockShape` : the consumer's required layout.
                              Empty `targetOwnerDims` means a replicated target.
      - `haloShape` (opt)   : per-logical-dim ghost width; present only for
                              `halo_like`.
      - `commVolumeBytes` (opt) : the committed abstract edge cost, advisory.

    A reduction family (`reduce_scatter_like`/`allreduce_like`) may keep an
    identical source/target layout (the movement is the reduction over the owned
    axis, not a re-layout); a re-layout family requires the two layouts to
    differ. The `mu` operand is the array root (an `sde.mu_alloc` result or a
    block-argument memref); `sde.cu_region`/`su_iterate` are not
    `IsolatedFromAbove`, so it is visible at the redistribution site.

    This op is NOT part of the default `-O3` pipeline.
  }];

  let arguments = (ins
    AnyMemRef:$mu,
    SdeMovementFamilyAttr:$family,
    ArrayAttr:$sourceOwnerDims,
    ArrayAttr:$sourceBlockShape,
    ArrayAttr:$targetOwnerDims,
    ArrayAttr:$targetBlockShape,
    OptionalAttr<ArrayAttr>:$haloShape,
    OptionalAttr<I64Attr>:$commVolumeBytes
  );

  // No results: a structural fact op, not a value producer.
  let results = (outs);

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

def SdeMuDataOp : CartsSde_Op<"mu_data", [Pure]> {
  let summary = "Declarative SDE shared-data handle.";
  let description = [{
    Canonical anchor for data that crosses SDE tasks. Replaces the pattern
    of inheriting Polygeist `memref.alloca` for OMP-shared variables.
    Produced by `ConvertOpenMPToSde` for every variable named by an OMP
    `shared(...)` clause (or otherwise shared across tasks), consumed by SDE
    MU/token compute-unit planning.
  }];

  let arguments = (ins
    OptionalAttr<AnyAttr>:$init,
    UnitAttr:$shared
  );

  let results = (outs AnyType:$handle);

  let assemblyFormat = [{
    (`shared` $shared^)? (`init` `(` $init^ `)`)?
    attr-dict `:` type($handle)
  }];
}

def SdeMuReductionDeclOp
    : CartsSde_Op<"mu_reduction_decl", [IsolatedFromAbove, Symbol]> {
  let summary = "Module-level SDE reduction declaration";
  let description = [{
    Declares a named SDE reduction symbol with its element type, reduction
    classification, optional identity attribute, and an optional combiner
    region for custom reductions.
  }];

  let arguments = (ins
    SymbolNameAttr:$sym_name,
    TypeAttr:$type,
    SdeReductionKindAttr:$reduction_kind,
    OptionalAttr<AnyAttr>:$identity
  );

  let regions = (region AnyRegion:$combiner);
  let hasVerifier = 1;

  let assemblyFormat = [{
    $sym_name `:` $type `kind` `(` $reduction_kind `)`
    (`identity` `(` $identity^ `)` )?
    ($combiner^)?
    attr-dict
  }];
}

//===----------------------------------------------------------------------===//
// Yield
//===----------------------------------------------------------------------===//

def SdeYieldOp
    : CartsSde_Op<"yield", [Pure, ReturnLike, Terminator]> {
  let summary = "SDE region terminator";
  let description = [{
    Terminates all SDE regions. Can optionally return values for reductions
    and iter_args.
  }];

  let arguments = (ins Variadic<AnyType>:$values);

  let assemblyFormat = [{ ($values^ `:` type($values))? attr-dict }];
}

#endif // ARTS_SDE_OPS
