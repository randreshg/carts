# SDE Op / Type / Attribute Reference

Source of truth: `include/carts/dialect/sde/IR/SdeOps.td` (ops + enums),
`SdeTypes.td` (types), `SdeDialect.td` (dialect), with op-local verifiers in
`lib/carts/dialect/sde/IR/SdeOps.cpp`. Line numbers below are into those files
for the current tree.

Ops are grouped by the three units. The `SdeOps.td` header itself states the
file "defines the SDE dialect operations: CU, SU, MU, and yield."

## CU — Compute Unit ops

Executable leaves. `verifyCuContainsNoScheduling` (`SdeOps.cpp:34-49`) forbids
any nested SU op (`isCuForbiddenSchedulingOp = isSuOp || su_barrier || redist`,
`Utils/SdeCuStructure.h:51-53`) in `cu_region`/`cu_task`/`cu_reduce`/`cu_work`.

| Op | Role | Source |
| --- | --- | --- |
| `sde.cu_region` | Executable-leaf region capturing `omp.parallel`/`single`/`master`; `kind` (parallel/single/task) + `nowait` + variadic `iterArgs`/results; `SingleBlock`, **not** `IsolatedFromAbove` (so `mu_access_window`/`redist` operands stay visible). | `SdeOps.td:228-265` |
| `sde.cu_task` | Task-body container capturing `omp.task`; `NoTerminator`; source `depend(...)` slices are local `sde.mu_dep` in the body, not edges on the op. | `SdeOps.td:267-287` |
| `sde.cu_reduce` | Reduction accumulation preserving the OpenMP combiner; `reduction_kind` + accumulator/partial/identity + optional combiner region. | `SdeOps.td:304-333` |
| `sde.cu_atomic` | Atomic memory update capturing `omp.atomic.update`; memref-only. Dual-classified: a CU op that is also a legal **direct child of `su_iterate`**. | `SdeOps.td:335-354` |
| `sde.cu_work` | **Isolated** compute unit (`IsolatedFromAbove`) consumed by SDE→ARTS dep realization; communicates only via `!sde.token` operands + scalar captures; verifier checks slice-type block args and rejects conflicting modes on statically-overlapping slices. | `SdeOps.td:574-613` |
| `sde.control_token` | Produces an `!sde.completion` for task/wait control edges; consumed by `su_barrier`, erased at boundary realization. (CU-section op producing an SU-consumed handle.) | `SdeOps.td:289-302` |

## SU — Scheduling Unit ops

Scheduling-only bodies that compose CUs.

| Op | Role | Source |
| --- | --- | --- |
| `sde.su_iterate` | Parallel iteration space (`omp.wsloop+loop_nest`, `taskloop`, `scf.parallel`). Carries the large committed-fact surface (see below). `LoopLikeOpInterface`. Verifier restricts the body to `cu_region`/`cu_atomic`/`array_layout_root`/`su_barrier`/`yield` and enforces `arrayLayout` ↔ `array_layout_root` provenance. | `SdeOps.td:360-467` |
| `sde.su_distribute` | Runtime-neutral distribution-fact wrapper; `distributionKind` ∈ {`owner_compute`, `blocked`, `cyclic`}; direct children are nested SUs / `redist` / `su_barrier`. "Does not encode concrete worker, route, or node identifiers." | `SdeOps.td:469-489` |
| `sde.su_barrier` | Synchronization point (`omp.barrier`/`taskwait`); optional `!sde.completion` tokens narrow the wait; optional `barrierReason`. | `SdeOps.td:491-507` |

`su_iterate`'s committed-fact attributes (all element-space, no target objects)
include: `schedule`/`chunkSize`/`nowait`; reduction (`reductionAccumulators`,
`reductionKinds`, `reductionStrategy`, `partialReduction*`);
`structuredClassification`/`pattern`; access summaries
(`accessMin/MaxOffsets`, `ownerDims`, `spatialDims`, `writeFootprint`);
physical layout (`physicalOwnerDims`, `physicalBlockShape`,
`logicalWorkerSlice`, `physicalHaloShape`, `iterationTopology`,
`repetitionStructure`, `asyncStrategy`, `distributionKind`, `inPlaceSafe`,
`inPlaceSharedState`); and the **module-scoped HPF layout dictionary**
`arrayLayout` + `layoutsDisagree` + `commVolumeBytes` (`SdeOps.td:424-459`).
The optional attrs are appended positionally to keep the op builder additive.

## MU — Memory Unit ops

State/storage shape, access windows, and movement. Several are **zero-result
`MemWrite`-but-not-`Pure`** structural facts that must survive DCE until a
verifier or the boundary consumes them, and that **do not survive the SDE→ARTS
boundary**.

| Op | Role | Survives boundary? | Source |
| --- | --- | --- | --- |
| `sde.mu_alloc` | Target-neutral memory-unit storage producing a memref SSA value; optional `arrayId` provenance. | lowered to ARTS DB | `SdeOps.td:644-668` |
| `sde.mu_data` | Declarative shared-data handle for OMP `shared(...)` vars (replaces inherited Polygeist `memref.alloca`). Produced by `convert-openmp-to-sde`, lowered at `SdeToArtsBoundary.cpp:493` (`lowerMuData`). | lowered | `SdeOps.td:799-820` |
| `sde.mu_access_window` | Per-CU access-window fact in block-grid coords, raised by `raise-to-mu-access-window` after final shape rewrites; `blockLo`/`blockHi`/`validExtents` I64 arrays; must sit directly in a `cu_region` body and name a `mu_alloc` result. | consumed + erased | `SdeOps.td:670-734` |
| `sde.redist` | One array's layout→layout redistribution edge as a **geometric movement family** only; source/target owner+block shapes + optional `haloShape`/`commVolumeBytes`. Names no collective/transport/DB/EDT. | consumed + erased | `SdeOps.td:736-797` |
| `sde.array_layout_root` | Binds a memref root to a module-stable `arrayId` committed in the enclosing `su_iterate` `arrayLayout`; `mode` ∈ {read, write} (readwrite rejected). | consumed | `SdeOps.td:615-642` |
| `sde.mu_reduction_decl` | Module-level named reduction symbol (`Symbol`, `IsolatedFromAbove`). **Currently dormant** — no producer/consumer pass in this tree. | n/a | `SdeOps.td:822-847` |
| `sde.mu_dep` | **Deprecated-but-present.** SDE form of `omp.task depend(...)` slices → `!sde.dep`; SDE-only, consumed by SDE→ARTS. | no | `SdeOps.td:513-541` |
| `sde.mu_token` | **Deprecated-but-present.** MU access-token producer (whole storage or slice) → `!sde.token`, feeding `cu_work`. | no | `SdeOps.td:543-572` |

## Shared / support ops

| Op | Role | Source |
| --- | --- | --- |
| `sde.yield` | Terminator for all SDE regions; `ParentOneOf{cu_region, cu_work, su_iterate, cu_reduce, mu_reduction_decl}`; optional values for reductions/iter_args. | `SdeOps.td:853-875` |
| `sde.resource_query` | Queries a target-neutral resource; only kind `logical_workers` → `index`. "Does not name a concrete runtime worker, placement, or task object." | `SdeOps.td:200-222` |

## Types (`SdeTypes.td`)

- `!sde.completion` — SDE completion/wait token; produced by `control_token`,
  consumed by `su_barrier`. The only CU-produced, SU-consumed handle.
- `!sde.dep` — result of `mu_dep`.
- `!sde.token<memref<...>>` — handle to a (sub)region of MU-backed storage with
  an access mode; the `slice_type` parameter types `cu_work` block args directly.

## Key enums (`SdeOps.td:50-194`)

- `SdeCuKind` — parallel / single / task.
- `SdeAccessMode` — read / write / readwrite.
- `SdeReductionKind` — add/mul/min/max/and/or/xor/custom; `SdeReductionStrategy`
  (shared) — atomic / tree / local_accumulate.
- `SdeStructuredClassification` — elementwise / stencil / matmul / reduction /
  elementwise_pipeline.
- `SdePattern` — uniform / stencil_tiling_nd / cross_dim_stencil_3d /
  higher_order_stencil / wavefront_2d / alternating_buffer_stencil / matmul /
  elementwise_pipeline / reduction.
- `SdeScheduleKind` — static / dynamic / guided / auto / runtime.
- `SdeDistributionKind` — owner_compute / blocked / cyclic.
- `SdeMovementFamily` — broadcast_like / all_gather_like / all_to_all_like /
  reduce_scatter_like / allreduce_like / halo_like / phase_redist. **Geometric
  movement shapes only**, never a concrete collective. Authored by the
  State-axis `sde-redistribute` (not by the effect distribution pass).
- `SdeIterationTopology`, `SdeRepetitionStructure`, `SdeAsyncStrategy`,
  `SdeBarrierReason`, `SdeResourceQueryKind`.

`redist` couples family ↔ geometry in its verifier (`SdeOps.cpp:1202-1234`):
reduction families may keep an identical source/target layout (the movement is
the reduction); re-layout families must differ; `halo_like` requires identical
owner dims plus a `haloShape`; broadcast/all-gather require a replicated target.
