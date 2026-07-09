///==========================================================================///
/// File: ArtsAttrs.td
/// This file declares the ARTS dialect attributes.
///==========================================================================///

#ifndef ARTS_ATTRS
#define ARTS_ATTRS

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"
include "carts/IR/CommonAttrs.td"
include "mlir/IR/BuiltinAttributeInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "ArtsDialect.td"

///===----------------------------------------------------------------------===///
// ARTS Enum Helpers
///===----------------------------------------------------------------------===///

class Arts_I32Enum<string name, string description, list<I32EnumAttrCase> cases>
    : I32EnumAttr<name, description, cases> {
  let cppNamespace = "::mlir::carts::arts";
  let genSpecializedAttr = 0;
}

class Arts_I32EnumAttr<string mnemonic, Arts_I32Enum enumInfo>
    : EnumAttr<ArtsDialect, enumInfo, mnemonic> {
  let assemblyFormat = "`<` $value `>`";
}

///===----------------------------------------------------------------------===///
// ARTS Attributes
///===----------------------------------------------------------------------===///
class Arts_Attr<string attrName, string attrMnemonic, list<Trait> traits = []>
    : AttrDef<ArtsDialect, attrName, traits> {
  let mnemonic = attrMnemonic;
}

// Enum info for ARTS access modes (in, out, inout, uninitialized)
def ArtsMode : Arts_I32Enum<"ArtsMode", "ARTS DataBlock access mode", [
  I32EnumAttrCase<"in", 0, "in">, I32EnumAttrCase<"out", 1, "out">,
  I32EnumAttrCase<"inout", 2, "inout">,
  I32EnumAttrCase<"uninitialized", 3, "uninitialized">
]>;

// Enum attribute for ARTS access modes
def ArtsModeAttr : Arts_I32EnumAttr<"mode", ArtsMode>;

/// Enum info for DB allocation types
def DbAllocType : Arts_I32Enum<"DbAllocType", "ARTS DB allocation type", [
  I32EnumAttrCase<"stack", 0, "stack">, I32EnumAttrCase<"heap", 1, "heap">,
  I32EnumAttrCase<"global", 2, "global">,
  I32EnumAttrCase<"unknown", 3, "unknown">
]>;

/// Enum attribute for DB allocation types
def DbAllocTypeAttr : Arts_I32EnumAttr<"alloc_type", DbAllocType>;

/// Enum info for EDT types
def EdtType : Arts_I32Enum<"EdtType", "ARTS EDT type", [
  I32EnumAttrCase<"single", 1, "single">, I32EnumAttrCase<"sync", 2, "sync">,
  I32EnumAttrCase<"task", 3, "task">
]>;

/// Enum attribute for EDT types
def EdtTypeAttr : Arts_I32EnumAttr<"edt_type", EdtType>;

/// Enum info for compiler DB access modes.
/// Only `read` and `write` are actively used in the compiler.
/// Integer values intentionally match ARTS runtime dependency modes
/// `DB_MODE_RO` (1) and `DB_MODE_EW` (2) so dependency lowering can pass them
/// through directly without an extra translation layer.
def DbMode : Arts_I32Enum<"DbMode", "ARTS DB access mode", [
  I32EnumAttrCase<"read", 1, "read">, I32EnumAttrCase<"write", 2, "write">
]>;

/// Enum attribute for DB allocation modes
def DbModeAttr : Arts_I32EnumAttr<"db_mode", DbMode>;

/// Enum info for runtime DB dependency mode verdicts.
/// Integer values intentionally match the ARTS runtime dependency constants:
/// `DB_MODE_RO` (1), `DB_MODE_EW` (2), and `DB_MODE_RW` (3).
def RuntimeDbMode
    : Arts_I32Enum<"RuntimeDbMode", "ARTS runtime DB dependency mode verdict", [
      I32EnumAttrCase<"ro", 1, "ro">,
      I32EnumAttrCase<"ew", 2, "ew">,
      I32EnumAttrCase<"rw", 3, "rw">
    ]>;

/// Enum attribute for runtime DB dependency mode verdicts.
def RuntimeDbModeAttr
    : Arts_I32EnumAttr<"runtime_db_mode", RuntimeDbMode>;

/// Enum info for DB release types
def DbReleaseType : Arts_I32Enum<"DbReleaseType", "ARTS DB release type", [
  I32EnumAttrCase<"normal", 0, "normal">,
  I32EnumAttrCase<"dynamic_free", 1, "dynamic_free">
]>;

/// Enum attribute for DB release types
def DbReleaseTypeAttr : Arts_I32EnumAttr<"release_type", DbReleaseType>;

/// Enum info for storage-bridge tags on `arts.db_alloc`. Marks an allocation
/// as a bridge between two storage views in the SDE->ARTS lowering.
def StorageBridge : Arts_I32Enum<"StorageBridge", "ARTS storage-bridge tag", [
  I32EnumAttrCase<"host_whole_to_compute_block", 0,
                  "host_whole_to_compute_block">
]>;

/// Enum attribute for storage-bridge tags on `arts.db_alloc`.
def StorageBridgeAttr : Arts_I32EnumAttr<"storage_bridge", StorageBridge>;

def ArtsReductionStrategy
    : Arts_I32Enum<"ArtsReductionStrategy",
                   "ARTS reduction execution strategy",
                   CartsReductionStrategyCases>;
def ArtsReductionStrategyAttr
    : Arts_I32EnumAttr<"reduction_strategy", ArtsReductionStrategy>;

def ArtsBarrierReason
    : Arts_I32Enum<"ArtsBarrierReason",
                   "ARTS barrier preservation or elimination reason",
                   CartsBarrierReasonCases>;
def ArtsBarrierReasonAttr
    : Arts_I32EnumAttr<"barrier_reason", ArtsBarrierReason>;

/// Enum info for runtime query kinds (consolidated from 5 individual ops)
def RuntimeQueryKind
    : Arts_I32Enum<"RuntimeQueryKind", "ARTS runtime query kind", [
      I32EnumAttrCase<"totalWorkers", 0, "total_workers">,
      I32EnumAttrCase<"totalNodes", 1, "total_nodes">,
      I32EnumAttrCase<"currentWorker", 2, "current_worker">,
      I32EnumAttrCase<"currentNode", 3, "current_node">
    ]>;

/// Enum attribute for runtime query kinds
def RuntimeQueryKindAttr : Arts_I32EnumAttr<"runtime_query_kind", RuntimeQueryKind>;

/// Enum info for EDT concurrency types
def EdtConcurrency
    : Arts_I32Enum<"EdtConcurrency", "ARTS EDT concurrency type", [
      I32EnumAttrCase<"intranode", 0, "intranode">,
      I32EnumAttrCase<"internode", 1, "internode">
    ]>;

/// Enum attribute for EDT concurrency types
def EdtConcurrencyAttr : Arts_I32EnumAttr<"concurrency", EdtConcurrency>;

/// Enum info for DB partition modes
/// Used to track how a DbAllocOp was partitioned and how DbAcquireOp accesses:
///   - coarse: Single DB for entire array (outerRank=0), full array access
///   - block: Fixed-size blocks, contiguous block by offset/size
///   - fine_grained: One DB per element/row (outerRank>0), element-wise access
///   - stencil: Halo pattern access (offset + size for neighboring elements)
def PartitionMode
    : Arts_I32Enum<"PartitionMode", "DB allocation partition mode", [
      I32EnumAttrCase<"coarse", 0, "coarse">,
      I32EnumAttrCase<"block", 1, "block">,
      I32EnumAttrCase<"fine_grained", 2, "fine_grained">,
      I32EnumAttrCase<"stencil", 3, "stencil">
    ]>;

/// Enum attribute for DB partition modes
def PartitionModeAttr : Arts_I32EnumAttr<"partition_mode", PartitionMode>;

/// Distribution strategy carried by SDE-authored ARTS distribution facts.
def EdtDistributionKind
    : Arts_I32Enum<"EdtDistributionKind", "EDT distribution strategy kind", [
      I32EnumAttrCase<"block", 0, "block">,
      I32EnumAttrCase<"block_cyclic", 2, "block_cyclic">,
      I32EnumAttrCase<"tiling_2d", 3, "tiling_2d">,
      I32EnumAttrCase<"replicated", 4, "replicated">
    ]>;
def EdtDistributionKindAttr
    : Arts_I32EnumAttr<"distribution_kind", EdtDistributionKind>;

/// Loop pattern classification used for strategy selection diagnostics.
def EdtDistributionPattern
    : Arts_I32Enum<"EdtDistributionPattern", "EDT loop pattern kind", [
      I32EnumAttrCase<"unknown", 0, "unknown">,
      I32EnumAttrCase<"uniform", 1, "uniform">,
      I32EnumAttrCase<"stencil", 2, "stencil">,
      I32EnumAttrCase<"matmul", 3, "matmul">,
      I32EnumAttrCase<"triangular", 4, "triangular">
    ]>;
def EdtDistributionPatternAttr
    : Arts_I32EnumAttr<"distribution_pattern", EdtDistributionPattern>;

/// Canonical dep/schedule family recognized by ARTS transforms.
/// Unlike distribution_pattern, this records the semantic rewrite family so
/// downstream passes do not need to rediscover it from raw memory accesses.
def ArtsDepPattern
    : Arts_I32Enum<"ArtsDepPattern",
                   "Canonical dep/schedule family", [
      I32EnumAttrCase<"unknown", 0, "unknown">,
      I32EnumAttrCase<"uniform", 1, "uniform">,
      I32EnumAttrCase<"stencil", 2, "stencil">,
      I32EnumAttrCase<"matmul", 3, "matmul">,
      I32EnumAttrCase<"triangular", 4, "triangular">,
      I32EnumAttrCase<"wavefront_2d", 5, "wavefront_2d">,
      I32EnumAttrCase<"alternating_buffer_stencil", 6,
                      "alternating_buffer_stencil">,
      I32EnumAttrCase<"elementwise_pipeline", 7, "elementwise_pipeline">,
      I32EnumAttrCase<"stencil_tiling_nd", 8, "stencil_tiling_nd">,
      I32EnumAttrCase<"cross_dim_stencil_3d", 9, "cross_dim_stencil_3d">,
      I32EnumAttrCase<"higher_order_stencil", 10, "higher_order_stencil">,
      I32EnumAttrCase<"reduction", 11, "reduction">
    ]>;
def ArtsDepPatternAttr
    : Arts_I32EnumAttr<"dep_pattern", ArtsDepPattern>;

/// Marker on DbAcquireOp to preserve authoritative access-mode semantics.
/// This only protects the acquire's in/out/inout mode from later
/// re-inference/optimization. It does not by itself imply that the dependency
/// edge must stay live.
def PreserveAccessModeAttr : AttrDef<ArtsDialect, "PreserveAccessMode"> {
  let summary = "Preserve explicit access-mode semantics on a DbAcquireOp";
  let description = [{
    Marks DbAcquireOps whose access mode should be preserved rather than
    inferred from loads/stores. This attribute is only set when the compiler
    already knows the exact mode and later passes should not re-check or
    optimize it.
  }];
  let mnemonic = "preserve_access_mode";
  let assemblyFormat = "";
}

/// Marker on DbAcquireOp to preserve an explicit dependency edge.
/// This is set for acquires created from explicit SDE task dependencies
/// whose EDT dependency must not be DCE'd away just because the local
/// pointer/block argument is otherwise unused.
def PreserveDepEdgeAttr : AttrDef<ArtsDialect, "PreserveDepEdge"> {
  let summary = "Preserve explicit dependency edge semantics on a DbAcquireOp";
  let description = [{
    Marks DbAcquireOps whose EDT dependency edge is semantically required even
    if the acquired pointer has no local memory uses. This is used for
    explicit task-control dependencies and is intentionally separate from
    `preserve_access_mode`, which only protects access-mode inference.
  }];
  let mnemonic = "preserve_dep_edge";
  let assemblyFormat = "";
}

/// Stencil-reach metadata on a halo DbAcquireOp.
/// Records the lower/upper element extent per stencil spatial dim for
/// verification and diagnostics. Explicit element_offsets/element_sizes remain
/// the runtime byte-window authority.
def HaloSliceAttr : Arts_Attr<"HaloSlice", "halo_slice"> {
  let summary = "Stencil-reach metadata on a halo acquire";
  let parameters = (ins
    "::mlir::DenseI64ArrayAttr":$lower,
    "::mlir::DenseI64ArrayAttr":$upper
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

/// Shared block layout facts for ARTS DB/EDT/epoch ops. Phase A typed contract;
/// wired onto db_alloc/db_acquire/edt/epoch (replaces stencil_* + distribution_* attrs).
def ArtsBlockLayoutAttr : Arts_Attr<"ArtsBlockLayout", "block_layout"> {
  let summary = "Shared block layout facts for ARTS distributed DB/EDT ops";
  let description = [{
    Canonical owner-dim, block-shape, optional halo reach, and distribution
    kind for one committed DB grid. Intended to be shared across db_alloc,
    db_acquire, edt, and epoch so cross-op layout divergence is unrepresentable.
  }];
  let parameters = (ins
    "::mlir::DenseI64ArrayAttr":$owner_dims,
    "::mlir::DenseI64ArrayAttr":$block_shape,
    OptionalParameter<"::mlir::DenseI64ArrayAttr">:$halo_reach,
    "EdtDistributionKindAttr":$distribution_kind
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

/// Placement strategy for a DB allocation; one enum replacing the six
/// mutually constrained placement unit flags on `arts.db_alloc`.
def ArtsDbPlacement : Arts_I32Enum<"ArtsDbPlacement", "ARTS DB placement strategy", [
  I32EnumAttrCase<"default_", 0, "default">,
  I32EnumAttrCase<"distributed", 1, "distributed">,
  I32EnumAttrCase<"local_only", 2, "local_only">,
  I32EnumAttrCase<"per_block_replicated", 3, "per_block_replicated">,
  I32EnumAttrCase<"per_block_single_writer_stencil", 4,
                  "per_block_single_writer_stencil">,
  I32EnumAttrCase<"compact_halo_payload", 5, "compact_halo_payload">,
  I32EnumAttrCase<"read_only_after_init", 6, "read_only_after_init">
]>;
def ArtsDbPlacementAttr : Arts_I32EnumAttr<"db_placement", ArtsDbPlacement>;

/// EDT movement role; one enum replacing the four movement role unit flags on
/// `arts.edt`.
def EdtRole : Arts_I32Enum<"EdtRole", "ARTS EDT movement role", [
  I32EnumAttrCase<"none", 0, "none">,
  I32EnumAttrCase<"storage_bridge_copy", 1, "storage_bridge_copy">,
  I32EnumAttrCase<"per_block_all_gather", 2, "per_block_all_gather">,
  I32EnumAttrCase<"compact_halo_pack", 3, "compact_halo_pack">,
  I32EnumAttrCase<"owner_local_writer_split", 4, "owner_local_writer_split">
]>;
def EdtRoleAttr : Arts_I32EnumAttr<"edt_role", EdtRole>;

/// Halo window facts for `arts.db_acquire`; folds element window operands and
/// halo-view dependency markers into one typed attr (Phase A stub).
def ArtsHaloWindowAttr : Arts_Attr<"ArtsHaloWindow", "halo_window"> {
  let summary = "Halo window facts for a db_acquire dependency slot";
  let parameters = (ins
    "::mlir::DenseI64ArrayAttr":$lower,
    "::mlir::DenseI64ArrayAttr":$upper,
    OptionalParameter<"::mlir::DenseI64ArrayAttr">:$elementOffsets,
    OptionalParameter<"::mlir::DenseI64ArrayAttr">:$elementSizes,
    DefaultValuedParameter<"bool", "false">:$haloViewDependency,
    DefaultValuedParameter<"bool", "false">:$replicatedRead
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

/// Runtime dependency verdict plus explicit preservation flags for
/// `arts.db_acquire`; folds `runtime_db_mode`, `preserve_access_mode`, and
/// `preserve_dep_edge` into one typed attr (Phase A stub).
def ArtsDepVerdictAttr : Arts_Attr<"ArtsDepVerdict", "dep_verdict"> {
  let summary = "Runtime DB dependency verdict and preservation flags";
  let parameters = (ins
    "RuntimeDbModeAttr":$runtimeDbMode,
    DefaultValuedParameter<"bool", "false">:$preserveAccessMode,
    DefaultValuedParameter<"bool", "false">:$preserveDepEdge
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

#endif // ARTS_ATTRS
