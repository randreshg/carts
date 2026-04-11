# Op and Attribute Classification

Complete classification of all current ARTS ops and attributes into the
three-dialect architecture: `sde.*`, `arts.*`, `arts_rt.*`.

Current branch note:
- The active frontend path is `ConvertOpenMPToSde -> ... -> ConvertSdeToArts`.
- SDE owns semantic pattern reasoning and semantic cost-model-driven decisions.
- After `ConvertSdeToArts`, ARTS consumes those contracts and performs
  ARTS-native structural/runtime optimization rather than a second semantic
  optimization pass.

## Op Classification

### Complete Classification Table

| Current Op | Target | New Name | Created By | Consumed By |
|---|---|---|---|---|
| `arts.undef` | `arts` | (unchanged) | EdtLowering | ConvertToLLVM |
| `arts.alloc` | `arts` | (unchanged) | EdtLowering | ConvertToLLVM |
| `arts.db_dim` | `arts` | (unchanged) | CreateDbs | Canonicalized |
| `arts.db_alloc` | `arts` | (unchanged) | CreateDbs, DbLowering | ConvertToLLVM |
| `arts.db_acquire` | `arts` | (unchanged) | CreateDbs, ForLowering | EdtLowering, ConvertToLLVM |
| `arts.db_ref` | `arts` | (unchanged) | DbPartitioning, ForLowering | ConvertToLLVM |
| `arts.db_release` | `arts` | (unchanged) | CreateDbs, ForLowering | ConvertToLLVM |
| `arts.db_free` | `arts` | (unchanged) | CreateDbs, DbLowering | ConvertToLLVM |
| `arts.db_num_elements` | `arts` | (unchanged) | EdtLowering | ConvertToLLVM |
| `arts.edt` | `arts` | (unchanged) | ConvertSdeToArts, ForLowering | EdtLowering |
| `arts.epoch` | `arts` | (unchanged) | CreateEpochs, ForLowering | EpochLowering |
| `arts.for` | `arts` | (unchanged) | ConvertSdeToArts | ForLowering |
| `arts.yield` | `arts` | (unchanged) | (all region creators) | (implicit) |
| `arts.barrier` | `arts` | (unchanged) | ConvertSdeToArts | ConvertToLLVM |
| `arts.atomic_add` | `arts` | (unchanged) | ConvertSdeToArts | ConvertToLLVM |
| `arts.runtime_query` | `arts` | (unchanged) | Concurrency, ForLowering | ConvertToLLVM |
| `arts.get_edt_epoch_guid` | `arts` | (unchanged) | EpochLowering | ConvertToLLVM |
| `arts.cps_advance` | `arts` | (unchanged) | EpochOptCpsChain | EpochLowering |
| `arts.lowering_contract` | **`sde`** | lowered to SDE metadata | PatternPipeline | ForLowering, DbPartitioning |
| `arts.omp_dep` | **`sde`** | `sde.mu_dep` | legacy ingress and narrow fallback bridge | SdeToArts |
| `arts.db_control` | **`sde`** | `sde.mu_dep` | ConvertOpenMPToSde, ConvertSdeToArts | SdeToArts |
| `arts.edt_create` | **`arts_rt`** | `arts_rt.edt_create` | EdtLowering, EpochLowering | ConvertToLLVM |
| `arts.edt_param_pack` | **`arts_rt`** | `arts_rt.edt_param_pack` | EdtLowering, EpochLowering | ConvertToLLVM |
| `arts.edt_param_unpack` | **`arts_rt`** | `arts_rt.edt_param_unpack` | EdtLowering, EpochLowering | ConvertToLLVM |
| `arts.rec_dep` | **`arts_rt`** | `arts_rt.rec_dep` | EdtLowering | ConvertToLLVM |
| `arts.inc_dep` | **`arts_rt`** | `arts_rt.inc_dep` | EdtLowering | ConvertToLLVM (no-op) |
| `arts.dep_gep` | **`arts_rt`** | `arts_rt.dep_gep` | EdtLowering, EpochLowering | ConvertToLLVM |
| `arts.dep_db_acquire` | **`arts_rt`** | `arts_rt.dep_db_acquire` | EdtLowering, EpochLowering | ConvertToLLVM |
| `arts.db_gep` | **`arts_rt`** | `arts_rt.db_gep` | EdtLowering | ConvertToLLVM |
| `arts.create_epoch` | **`arts_rt`** | `arts_rt.create_epoch` | EpochLowering | ConvertToLLVM |
| `arts.wait_on_epoch` | **`arts_rt`** | `arts_rt.wait_on_epoch` | EpochLowering | ConvertToLLVM |
| `arts.state_pack` | **`arts_rt`** | `arts_rt.state_pack` | EdtLowering | ConvertToLLVM |
| `arts.state_unpack` | **`arts_rt`** | `arts_rt.state_unpack` | EdtLowering | ConvertToLLVM |
| `arts.dep_bind` | **`arts_rt`** | `arts_rt.dep_bind` | EdtLowering, EpochLowering | ConvertToLLVM |
| `arts.dep_forward` | **`arts_rt`** | `arts_rt.dep_forward` | EdtLowering, EpochLowering | ConvertToLLVM |

**Final count:**
- `arts` core: 18 ops (structural, stable -- includes cross-boundary DB ops)
- `sde`: 11 ops (current CU/SU/MU surface plus `yield`)
- `arts_rt`: 14 ops (all migrated -- clean boundary, created only in pre-lowering)

### Key Classification Decisions

- **`DbAllocOp`/`DbAcquireOp` stay in core** -- they span stages 7-18, used
  by both optimization passes and final LLVM conversion
- **`RuntimeQueryOp` stays in core** -- used during Concurrency (stage 10)
  and ForLowering (stage 11), not just at final lowering
- **`BarrierOp`/`AtomicAddOp` stay in core** -- structural primitives with
  clear operational behavior, originating from OMP but used structurally

---

## Attribute Classification

| Attribute | Target Dialect | Rationale |
|---|---|---|
| `ArtsMode` enum | `arts` | Core access mode used by DbAlloc/DbAcquire |
| `DbAllocType` enum | `arts` | Allocation type -- structural |
| `EdtType` enum | `arts` | EDT execution type -- structural |
| `EdtConcurrency` enum | `arts` | Execution scope -- structural |
| `DbMode` enum | `arts` | DB access mode (matches runtime constants) |
| `PartitionMode` enum | `arts` | Partition strategy -- structural |
| `ForScheduleKind` enum | `arts` | Loop schedule -- structural |
| `DbReleaseType` enum | `arts` | Release type -- structural |
| `WorkersAttr` | `arts` | Worker count -- structural |
| `NoVerifyAttr` | `arts` | Verification bypass -- structural |
| `PreserveAccessModeAttr` | `arts` | Access mode preservation -- structural |
| `PreserveDepEdgeAttr` | `arts` | Dep edge preservation -- structural |
| `RuntimeQueryKind` enum | `arts` | Used by RuntimeQueryOp (stays in core) |
| `DepAccessMode` enum | **`arts_rt`** | Only used by RecordDepOp, IncrementDepOp |
| `PatternAttr` | **`patterns/`** | Semantic pattern family -- not dialect-specific |
| `ContractAttr` | **`patterns/`** | Static lowering contract -- not dialect-specific |
| `ArtsDepPattern` enum | **`sde`** | Dep family classification -- semantic |
| `EdtDistributionKind` | **`sde`** | Distribution strategy -- semantic |
| `EdtDistributionPattern` | **`sde`** | Distribution pattern -- semantic |
| `DbAccessPattern` enum | **`sde`** | Access pattern classification -- semantic |
| `LoopMetadataAttr` | **`sde`** | Loop analysis metadata -- semantic |
| `MemrefMetadataAttr` | **`sde`** | Memref analysis metadata -- semantic |
| `ValueMetadataAttr` | **`sde`** | Value provenance -- semantic |
| `MemrefDimInfoAttr` | **`sde`** | Dimensional recovery -- semantic |

---

## Cross-Dialect Type Dependencies

```
sde  --depends-->  (none -- runtime-agnostic, uses only standard MLIR types)
arts --depends-->  (none -- standalone structural dialect)
arts_rt --depends--> (none -- uses only standard MLIR types: index, i32, i64, memref)
```

`arts_rt` has ZERO type dependency on `arts` or `sde`. This is the cleanest
boundary and why Phase 1 (extract `arts_rt`) is the easiest first step.

In the current architecture, `arts.lowering_contract` and `arts.omp_dep`
reference `ArtsMode` and `PartitionMode` from core `arts`. In the new
architecture, SDE defines its own runtime-agnostic enums (`SdeAccessMode`,
`SdeConcurrencyScope`) and the SDE-to-ARTS conversion maps between them.

---

## Operation Lifecycle Map

Shows when each op is created and destroyed across pipeline stages:

Current branch note:
- `sde.*` ops are created in `openmp-to-arts` before `ConvertSdeToArts`.
- `arts.*` ops for the semantic path begin at `ConvertSdeToArts`, not at the
  legacy `ConvertOpenMPToArts` path.

```
PHASE:         Prep  Pattern  Structure  SDE-Opt  Validate  Cleanup  Lowering  LLVM
                |      |        |          |         |         |        |        |
SDE ops:        |      |        |          |         |         |        |        |
  sde.cu_*      C------+--------D         |         |         |        |        |
  sde.su_*      C------+--------D         |         |         |        |        |
  sde.mu_*      C------+--------D         |         |         |        |        |
                |      |        |          |         |         |        |        |
ARTS core:      |      |        |          |         |         |        |        |
  arts.edt      |      |  C-----+----------+---------+---------+--D     |        |
  arts.for      |      |  C-----+----------+---------+---------+--D     |        |
  arts.db_alloc |      |        C----------+---------+---------+--D     |        |
  arts.db_acq   |      |        C----------+---------+---------+--D     |        |
  arts.epoch    |      |        |          C---------+---------+--D     |        |
  arts.cps_adv  |      |        |          C---------+---------+--D     |        |
                |      |        |          |         |         |        |        |
ARTS RT:        |      |        |          |         |         |        |        |
  arts_rt.edt_c |      |        |          |         |         |  C-----+--D     |
  arts_rt.rec_d |      |        |          |         |         |  C-----+--D     |
  arts_rt.c_epc |      |        |          C---------+---------+--------+--D     |
  arts_rt.dep_* |      |        |          |         |         |  C-----+--D     |
                |      |        |          |         |         |        |        |
LLVM:           |      |        |          |         |         |        |        |
  func.call     |      |        |          |         |         |        |  C-----+-->
  @arts_*       |      |        |          |         |         |        |        |

C = Created    D = Destroyed/Lowered    --> = Survives to output
```

---

## IREE Patterns Applied to CARTS

6 patterns extracted from IREE's architecture:

### Pattern 1: Op Traits for Sub-Phase Classification

IREE's Stream dialect uses op traits (`TensorPhaseOp`, `AsyncPhaseOp`,
`CmdPhaseOp`) to classify ops into sub-levels. Verification passes check
traits, not specific op types.

**CARTS application**: `SemanticPhaseOp` (sde ops), `StructuralPhaseOp`
(core ops), `RuntimePhaseOp` (arts_rt ops). Verification barriers become
generic and extensible.

### Pattern 2: Interfaces Over Type Inheritance

IREE's `Util` dialect defines behavioral interfaces (`ReferenceType`,
`SizeAwareType`) that downstream types implement, avoiding cross-dialect
type imports.

**CARTS application**: `ArtsMemoryAccessInterface` (ops that access DB
memory), `ArtsDependencyInterface` (ops that create/consume dependencies),
`ArtsCostAnnotatable` (ops that carry cost annotations).

### Pattern 3: Enum Duplication with Value-Compatible Casting

IREE duplicates enums across dialects with identical numeric values and
uses `static_cast` at boundaries, avoiding circular TableGen dependencies.

**CARTS application**: If `arts_rt` needs `ArtsMode` or `DbMode`, define
value-compatible duplicates. However, since `arts_rt` ops mostly use
`i32`/`i64` directly, this may not be needed.

### Pattern 4: Conversion Directories in the Target Dialect

IREE places conversion patterns in the TARGET dialect:
`HAL/Conversion/StreamToHAL/`, not `Stream/Conversion/StreamToHAL/`.

**CARTS application**: `dialect/rt/Conversion/RtToLLVM/` (not in core/).
`dialect/core/Conversion/SdeToArts/` (SDE is source, core is target).

### Pattern 5: Deferred Parallelism Specification

IREE's Flow captures `workload` (abstract problem size) separately from
`workgroup_count` (concrete 3D grid). Decouples "what" from "how".

**CARTS application**: SDE captures loop bounds, access patterns, and cost
estimates. Concrete EDT decomposition and DB partitioning strategy are
deferred to optimization passes that read the contract.

### Pattern 6: HAL Has Optimization Passes Too

Despite being the "runtime" dialect, IREE's HAL has 30+ transform passes.
The "runtime" label means "close to runtime", not "no optimization".

**CARTS application**: `arts_rt` could have optimization passes (e.g.,
dependency batching, param pack coalescing) without violating the
architecture.
