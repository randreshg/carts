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
/// as a bridge between two storage views in the CODIR->ARTS lowering.
def StorageBridge : Arts_I32Enum<"StorageBridge", "ARTS storage-bridge tag", [
  I32EnumAttrCase<"host_whole_to_compute_block", 0,
                  "host_whole_to_compute_block">
]>;

/// Enum attribute for storage-bridge tags on `arts.db_alloc`.
def StorageBridgeAttr : Arts_I32EnumAttr<"storage_bridge", StorageBridge>;

/// DB owner-map policy carried on distributed `arts.db_alloc` operations.
/// ARTS owns this because DB homes, writer routes, and GUID reservation are
/// runtime-realization facts, not SDE/CODIR layout facts.
def DbOwnerMapKind
    : Arts_I32Enum<"DbOwnerMapKind", "distributed DB owner-map kind", [
      I32EnumAttrCase<"linear_mod_nodes", 0, "linear_mod_nodes">,
      I32EnumAttrCase<"owner_dim_contiguous", 1, "owner_dim_contiguous">,
      I32EnumAttrCase<"owner_dim_grid", 2, "owner_dim_grid">,
      I32EnumAttrCase<"explicit_rank_table", 3, "explicit_rank_table">
    ]>;
def DbOwnerMapKindAttr
    : Arts_I32EnumAttr<"owner_map_kind", DbOwnerMapKind>;

/// Where the blocks of a DB physically live. ARTS owns this because DB homes are
/// a runtime-realization fact projected from committed owner dims, not an
/// SDE/CODIR layout fact.
///   - node_local:          all blocks stay on the creating node (the
///                          conservative coarse raw-memref bridge home).
///   - owner_scattered:     blocks are scattered across owner ranks per the
///                          owner map (the realized distributed home).
///   - per_block_replicated: every block is resident on every node (the
///                          all-gather replica home).
def DbMemoryPlacement
    : Arts_I32Enum<"DbMemoryPlacement", "distributed DB memory placement", [
      I32EnumAttrCase<"node_local", 0, "node_local">,
      I32EnumAttrCase<"owner_scattered", 1, "owner_scattered">,
      I32EnumAttrCase<"per_block_replicated", 2, "per_block_replicated">
    ]>;
def DbMemoryPlacementAttr
    : Arts_I32EnumAttr<"db_memory_placement", DbMemoryPlacement>;

def ArtsPlanIterationTopology
    : Arts_I32Enum<"ArtsPlanIterationTopology",
                   "ARTS physical iteration topology",
                   CartsIterationTopologyCases>;
def ArtsPlanIterationTopologyAttr
    : Arts_I32EnumAttr<"plan_iteration_topology",
                       ArtsPlanIterationTopology>;

def ArtsPlanRepetitionStructure
    : Arts_I32Enum<"ArtsPlanRepetitionStructure",
                   "ARTS repeated-wave structure", [
      I32EnumAttrCase<"none", 0, "none">,
      I32EnumAttrCase<"pair_step", 1, "pair_step">,
      I32EnumAttrCase<"k_step", 2, "k_step">,
      I32EnumAttrCase<"full_timestep", 3, "full_timestep">
    ]>;
def ArtsPlanRepetitionStructureAttr
    : Arts_I32EnumAttr<"plan_repetition_structure",
                       ArtsPlanRepetitionStructure>;

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

/// Distribution strategy carried by SDE/CODIR-authored ARTS materialization plans.
def EdtDistributionKind
    : Arts_I32Enum<"EdtDistributionKind", "EDT distribution strategy kind", [
      I32EnumAttrCase<"block", 0, "block">,
      I32EnumAttrCase<"two_level", 1, "two_level">,
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
    Marks DbAcquireOps whose access mode should be treated as an explicit
    dependency contract rather than inferred from loads/stores. This attribute
    is only set when the compiler already knows the exact mode and later passes
    should not re-check or optimize it.
  }];
  let mnemonic = "preserve_access_mode";
  let assemblyFormat = "";
}

/// Marker on DbAcquireOp to preserve an explicit dependency edge.
/// This is set for acquires materialized from explicit SDE task dependency
/// contracts whose EDT dependency must not be DCE'd away just because the
/// local pointer/block argument is otherwise unused.
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

/// Canonical semantic pattern payload shared by lowering contracts.
def PatternAttr : Arts_Attr<"Pattern", "pattern"> {
  let summary = "Canonical ARTS semantic pattern payload";
  let description = [{
    Bundles the semantic pattern family and distribution selection into one
    typed attribute so lowering contracts do not need to spread the same
    meaning across unrelated top-level attrs.
  }];
  let parameters = (ins
    OptionalParameter<"::mlir::carts::arts::ArtsDepPatternAttr">:$depPattern,
    OptionalParameter<"::mlir::carts::arts::EdtDistributionKindAttr">:$distributionKind,
    OptionalParameter<"::mlir::carts::arts::EdtDistributionPatternAttr">:$distributionPattern,
    OptionalParameter<"::mlir::IntegerAttr">:$distributionVersion
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

/// Canonical static lowering-contract payload shared by lowering contracts.
def ContractAttr : Arts_Attr<"Contract", "contract"> {
  let summary = "Canonical ARTS lowering-contract payload";
  let description = [{
    Bundles static lowering-contract refinements such as owner dimensions,
    stencil center/halo semantics, and post-DB analysis flags into one typed
    attribute. Dynamic quantities remain SSA operands on
    `arts.lowering_contract`.
  }];
  let parameters = (ins
    OptionalParameter<"::mlir::DenseI64ArrayAttr">:$ownerDims,
    OptionalParameter<"::mlir::IntegerAttr">:$centerOffset,
    OptionalParameter<"::mlir::DenseI64ArrayAttr">:$spatialDims,
    OptionalParameter<"::mlir::DenseI64ArrayAttr">:$stencilIndependentDims,
    OptionalParameter<"::mlir::BoolAttr">:$supportedBlockHalo,
    OptionalParameter<"::mlir::BoolAttr">:$narrowableDep,
    OptionalParameter<"::mlir::BoolAttr">:$postDbRefined,
    OptionalParameter<"::mlir::IntegerAttr">:$contractKind
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

/// Committed per-slot halo extents on a distributed stencil DbAcquireOp.
/// Records the realized halo window (lower/upper element extent per stencil
/// spatial dim) for this acquire's dependency slot, projected from the
/// committed stencil access window. Downstream consumers read this committed
/// fact instead of re-deriving halo bounds from raw stencil offsets.
def HaloSliceAttr : Arts_Attr<"HaloSlice", "halo_slice"> {
  let summary = "Committed per-slot halo extents on a distributed acquire";
  let parameters = (ins
    "::mlir::DenseI64ArrayAttr":$lower,
    "::mlir::DenseI64ArrayAttr":$upper
  );
  let assemblyFormat = "`<` struct(params) `>`";
}

#endif // ARTS_ATTRS
