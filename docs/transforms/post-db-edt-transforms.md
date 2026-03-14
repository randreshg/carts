# Contract Infrastructure & Post-DB Optimization Master Plan

Consolidated design document covering contract system improvements, DbTransforms,
and EdtTransforms for the ARTS compiler.

---

## 1. Current State of the Art

### 1.1 Contract System (Working Well)

The core contract infrastructure is solid:

- **LoweringContractOp** -- first-class IR op with proper SSA operand/attr split
- **depPattern** -- enum with 11 families as the universal classifier
- **SSA operands** for dynamic values (blockShape, minOffsets, maxOffsets,
  writeFootprint) -- survives structural rewrites automatically
- **Pattern revision** mechanism prevents downgrades, allows seed-to-refine
- **Pre-DB patterns** (5 total) all use contract utilities consistently
- **Lowering passes** consume contracts cleanly via
  `deriveAcquireRewriteContract()`, no pattern re-detection

### 1.2 Existing Optimization Passes

The compiler already has 10+ optimization passes. Understanding them is
critical to avoid duplication.

**DB Passes:**

| Pass | Stage | What It Does Today |
| --- | --- | --- |
| DbModeTightening | db-opt (8), concurrency-opt (12) | Mode tightening: analyzes loads/stores per acquire, sets in/out/inout. Partial writes upgrade to inout. |
| DbScratchEliminationPass | concurrency-opt (12) | Replaces task-private single-element scratch DBs with local allocas. Very narrow scope. |
| DbPartitioning | concurrency-opt (12) | THE main DB optimizer. 3-phase heuristic-driven partition mode selection (coarse/block/stencil/fine-grained). Reads DbGraph + DbAnalysis + contracts. |
| DbDistributedOwnership | concurrency-opt (12) | Marks eligible DbAllocOps with `distributed` UnitAttr for multi-node round-robin allocation. |
| BlockLoopStripMining | concurrency-opt (12) | Transforms block-partitioned loops: creates outer block + inner element loops, eliminates O(N) div/rem to O(N/BS). |

**EDT Passes:**

| Pass | Stage | What It Does Today |
| --- | --- | --- |
| EdtStructuralOptPass | edt-transforms (6), edt-opt (9), concurrency-opt (12) | 7-stage EDT optimizer: alloca sinking, parallel EDT fusion, barrier removal, parallel-to-sync conversion, sync-to-epoch conversion, no-dep EDT inlining, graph-informed barrier removal. Runs 3+ times. |
| EpochOpt | concurrency-opt (12) | Epoch fusion (merge epochs with disjoint/read-only shared DBs) + worker loop fusion within epochs. |
| EdtICM | edt-transforms (6) | Hoists loop-invariant pure operations out of EDT body loops. |
| EdtPtrRematerialization | edt-transforms (6) | Clones pointer ops into EDT bodies for distributed execution. |
| ArtsHoisting | concurrency-opt (12) | Hoists read-only db_acquire out of worker loops + hoists loop-invariant db_ref/pure ops. |
| DataPtrHoisting | pre-lowering (14) | Hoists LLVM load operations for ARTS deps struct pointers. |

### 1.3 Contract Flow (Current)

```text
Stage 5:  Pattern Pipeline
          |-- PatternDiscovery(seed)   -> depPattern + revision on ForOp/EdtOp
          |-- DepTransforms            -> rewrites IR, preserves via copyPatternAttrs()
          |-- KernelTransforms         -> stamps stencil/matmul contracts
          +-- PatternDiscovery(refine) -> upgrades stencil -> stencil_tiling_nd

          [Semantic attrs on ForOp/EdtOp]

Stage 7:  CreateDbs
          +-- Extracts semantic contract -> LoweringContractOp on DB pointer

          [First-class LoweringContractOp on DB ptr values]

Stage 12: DbPartitioning (MAIN CONSUMER)
          |-- DbAnalysis refines contracts in-memory (NOT persisted)
          |-- Heuristics receive raw facts (NOT contract summaries)
          +-- Sets partition_mode on allocs

Stage 14: Lowering
          |-- DbLowering:  reads contract for halo normalization
          |-- ForLowering:  reads via deriveAcquireRewriteContract()
          +-- EdtLowering:  reads for block halo normalization
```

---

## 2. Problems Found

### 2.1 Contract Infrastructure Gaps (5)

**Gap 1: Heuristics starved of contract info.**
DbPartitioning.cpp:1544 -- `heuristics.chooseAcquirePolicies(acquireFacts)`
passes raw `DbAcquirePartitionFacts*` instead of the refined
`AcquireContractSummary` that DbAnalysis already built.

**Gap 2: Stencil bounds -- 3 independent sources.**
DbPartitioning.cpp:1847-1920 reads from graph analysis, raw IR inspection,
AND raw attributes with no unified path.

**Gap 3: Access pattern fallback ordering inconsistent.**
Same pass, three code paths, three different priority orderings:
line 1449 (contract > node > facts), line 1793 (raw attr > node),
line 1898 (contract > attr > node).

**Gap 4: Post-DB contract refinements not persisted.**
DbAnalysis.cpp:104-146 -- `refineContractWithFacts()` fills missing depPattern,
infers distributionPattern, fills ownerDims -- but only in-memory. Lowering
reads the original, unrefined version.

**Gap 5: CSE/canonicalization risk points.**
31+ PolygeistCanonicalize + CSE invocations. No contract validation pass
exists to detect loss.

### 2.2 Critical Performance Bug

**CASE 2 global coarse fallback** (EdtRewriter.cpp:31-42,
ForLowering.cpp:121-123): Forces coarse mode for ANY inout + stencil-classified
allocation *globally* per-allocation, not per-acquire. A copy loop (write-only)
gets coarse even though only the stencil loop (read-only) needs special handling.
Evidence: 62x slowdown (specfem3d), 210x slowdown (jacobi-for copy loop).

### 2.3 Conservative Fallbacks (20+)

See Appendix A for the full catalogue. The most impactful:

| # | Location | Decision | Impact |
| --- | --- | --- | --- |
| 1 | PartitionBoundsAnalyzer.cpp:686-696 | Full-range for ANY indirect access | High |
| 2 | PartitionBoundsAnalyzer.cpp:638-719 | Full-range for non-leading stencil dim | High |
| 3 | ForLowering.cpp:406-420 | BlockCyclic full-range acquire window | High |
| 4 | ForLowering.cpp:271-273 | 1-D block window computation (N-D ignored) | High |
| 5 | DbBlockPlanResolver.cpp:124-196 | MIN across block candidates | Medium |
| 6 | DbBlockPlanResolver.cpp:346-427 | One dissenting acquire blocks all | Medium |
| 7 | Concurrency.cpp:185-192 | Force intranode for any nested tasks | Medium |

### 2.4 Unused ARTS Runtime Capabilities

The runtime has many features the compiler never generates:

| Capability | Runtime API | Estimated Impact |
| --- | --- | --- |
| GUID range allocation | `arts_guid_reserve_range()` | 10-50x alloc overhead |
| DB replication | `artsAddDbDuplicate()` | 2-5x read-heavy distributed |
| Atomic reductions | `artsAtomicAddInArrayDb()` | 2-10x reduction codes |
| 7 unused DB modes | `ARTS_DB_PIN/ONCE/LC/...` | 1.2-2x overhead reduction |
| Event types | `ARTS_EVENT_COUNTED/CHANNEL/IDEM` | More efficient sync |
| Route table mgmt | `artsRemoteUpdateRouteTable()` | 1.1-1.3x first-access |
| DB rename/alias | `arts_db_rename()` | Mode change post-creation |
| Local buffers | `arts_allocate_local_buffer()` | Staging, aggregation |
| Worker-local init | `init_per_worker()` | NUMA setup |
| EDT cluster/space | `artsEdtCreateInternal()` params | Better placement |

### 2.5 Computed-But-Unused Facts

| Fact | Where Computed | Never Used By |
| --- | --- | --- |
| `StencilBounds::{minOffset,maxOffset,centerOffset}` | PartitionBoundsAnalyzer.cpp:378 | Block size decisions |
| `isLeadingDynamic` | DbDimAnalyzer.cpp:113-139 | Any decision-making |
| `selectedByPartitionEntry` | DbDimAnalyzer.cpp:377 | Any decision-making |
| `loopStep` (as block alignment hint) | ForLowering.cpp:361 | Block alignment decisions |

---

## 3. Master Plan: What to FIX, EXTEND, and ADD

### Key Principle

**Extend existing passes** when the optimization naturally belongs there.
**Add new transforms** only when the scope requires a dedicated framework.

### 3.1 FIX in Existing Code (No New Passes)

| ID | What | Where | Priority |
| --- | --- | --- | --- |
| FIX-1 | Per-acquire coarse fallback (CASE 2 bug) | DbPartitioning.cpp, EdtRewriter.cpp | **Critical** |
| FIX-2 | Feed `AcquireContractSummary` to heuristics | PartitioningHeuristics interface | High |
| FIX-3 | Persist post-DB contract refinements to IR | DbAnalysis.cpp:refineContractWithFacts | High |
| FIX-4 | Cache `DbAnalysis&` in DbPartitioning | DbPartitioning.cpp:runOnOperation | High |
| FIX-5 | Cache `DbAnalysis*` in LoopAnalysis | LoopAnalysis.cpp:221-235 | Low |
| FIX-6 | Standardize fallback ordering to: contract summary > raw contract > node > facts > raw attrs | DbPartitioning.cpp:1449,1793,1898 | Medium |

### 3.2 EXTEND Existing Passes

These optimizations belong in existing passes because the pass already owns
the relevant analysis and IR mutation.

#### Extend DbModeTightening (mode tightening)

| ID | What | Rationale |
| --- | --- | --- |
| EXT-DB-1 | **DB mode inference from access contracts.** After full partition/distribution analysis, refine DB allocation mode: read-only-after-write -> `ARTS_DB_READ` (enables routing table caching), local-only -> `ARTS_DB_PIN` (bypass distributed model), single-epoch lifetime -> `ARTS_DB_ONCE` (auto-free). | DbModeTightening already does mode tightening. Adding richer mode selection is a natural extension. |
| EXT-DB-2 | **Use computed `StencilBounds` in mode decisions.** If stencil bounds prove a DB is only accessed within a local neighborhood, the mode can be more restrictive. | DbModeTightening already reads access patterns from DbGraph. |

#### Extend DbPartitioning

| ID | What | Rationale |
| --- | --- | --- |
| EXT-PART-1 | **Dimension-aware stencil partitioning.** Extend `needsFullRange()` in PartitionBoundsAnalyzer to check per-dimension stencil presence instead of global binary flag. Add `stencilIndependentDims` to contract. | The analysis already exists here; it's just too conservative. |
| EXT-PART-2 | **Bounded indirect access analysis.** In PartitionBoundsAnalyzer, check if indirect access is confined to non-partitioned dimensions before forcing full-range. | Same: relaxing an existing conservative check. |
| EXT-PART-3 | **Use `StencilBounds` in block size decisions.** Feed PartitionBoundsAnalyzer's already-computed minOffset/maxOffset into DbBlockPlanResolver instead of ignoring them. | The facts exist, just need wiring. |
| EXT-PART-4 | **Per-dimension block plan merge.** Replace global MIN-merge across candidates with per-dimension independent merge. | Fix in DbBlockPlanResolver.cpp:198-284. |
| EXT-PART-5 | **Heuristic-to-contract feedback.** After partition decisions are final, write chosen blockShape, resolved ownerDims, and distributionVersion back into the contract. | Already in the right pass; just needs write-back. |

#### Extend DbDistributedOwnership

| ID | What | Rationale |
| --- | --- | --- |
| EXT-DIST-1 | **Allow read-only stencil allocations if producer is distributed.** Currently rejected by eligibility criteria. | Relax existing conservative check. |
| EXT-DIST-2 | **Writer-aware route selection.** Align DB ownership with known writer EDT location instead of round-robin. | Natural extension of the existing ownership analysis. |

#### Extend EdtStructuralOptPass

| ID | What | Rationale |
| --- | --- | --- |
| EXT-EDT-1 | **Complete `inlineNoDepEdts()`.** The function exists but is incomplete. Extend to inline trivial EDT bodies (< 20 ops, no inter-node deps). | EdtStructuralOptPass already has the scaffolding (Stage 6). |
| EXT-EDT-2 | **Dead dependency elimination.** Walk each EDT body, remove unused dependency slots (acquire in `mode=in` never loaded). | EdtStructuralOptPass already does barrier removal using EdtGraph; dependency elimination is similar analysis. |

#### Extend EpochOpt

| ID | What | Rationale |
| --- | --- | --- |
| EXT-EPOCH-1 | **Epoch scope narrowing.** Identify EDTs with no post-epoch dependents and move them to non-blocking epoch or remove from epoch. | EpochOpt already analyzes epoch boundaries and DB access sets. |

### 3.3 ADD New Transforms (New Pass Framework)

These optimizations require cross-cutting analysis that no single existing
pass owns. They go in new `DbTransforms` and `EdtTransforms` pass shells.

#### New DbTransforms Pass

Pipeline placement: after DbPartitioning + DbDistributedOwnership, before DbModeTightening.

```text
include/arts/transforms/db/DbTransforms.h     -- base class + context
lib/arts/transforms/db/transforms/             -- individual transforms (GLOBbed)
lib/arts/passes/opt/db/DbTransformsPass.cpp    -- pass shell
```

| ID | Transform | What It Does |
| --- | --- | --- |
| DT-1 | **Contract persistence** | Walk all `DbAcquireOp`s, read `AcquireContractSummary` from DbAnalysis, write refined contract back to IR via `upsertLoweringContract()`. |
| DT-2 | **Stencil halo consolidation** | Collect halo bounds from graph analysis + raw IR + raw attrs, take conservative union, write final bounds into contract's `minOffsets`/`maxOffsets`, remove redundant raw attrs. |
| DT-3 | **ESD annotation** | For each acquire in stencil/block partitioned DBs, analyze EDT's actual access footprint. If it touches only a subset, annotate contract with `esdByteOffset`/`esdByteSize` so lowering emits `arts_add_dependence_at`. |
| DT-4 | **GUID range allocation** | Detect loops creating N GUIDs, replace N `arts_guid_reserve()` with one `arts_guid_reserve_range(N)` + `arts_guid_from_index()` inside loop. |
| DT-5 | **DB replication** | Identify DBs with high read count from multiple nodes (using `dbAllocAccessCount` from EdtInfo), emit `artsAddDbDuplicate()` to replicate to consumer nodes. |
| DT-6 | **Atomic reduction recognition** | Detect scalar reductions in EDT bodies, emit `artsAtomicAddInArrayDb()` instead of per-worker accumulator + finalization EDT. |
| DT-7 | **Block window caching** | Cache computed block window (startBlock, blockCount) in contract so DbLowering doesn't recompute the same element-to-block mapping that ForLowering already computed. |

#### New EdtTransforms Pass

Pipeline placement: after DbTransforms, before DbModeTightening.

```text
include/arts/transforms/edt/EdtTransforms.h     -- base class + context
lib/arts/transforms/edt/transforms/              -- individual transforms (GLOBbed)
lib/arts/passes/opt/edt/EdtTransformsPass.cpp    -- pass shell
```

| ID | Transform | What It Does |
| --- | --- | --- |
| ET-1 | **Task granularity control** | Use EdtInfo `computeToMemRatio` and `totalOps` to estimate task cost. Coarsen trivial tasks (fuse adjacent iterations); consider strip-mining oversized tasks. |
| ET-2 | **Data affinity placement** | For each EDT, find primary write target via `dbAllocAccessBytes`, set EDT route to owner node. Use NUMA domain hints via `arts_get_current_numa_domain()`. |
| ET-3 | **Dependency chain narrowing** | For wavefront patterns, analyze whether EDT needs full DB or just halo strip. Emit `arts_add_dependence_at` for halo-only dependencies. |
| ET-4 | **Communication-computation overlap** | Split stencil EDTs into interior EDT (no halo deps, starts immediately) + boundary EDT (halo deps, starts when halos arrive). Both write non-overlapping regions. |
| ET-5 | **Reduction strategy selection** | Small worker count -> local scalars; large -> tree-reduction; distributed -> `artsRemoteAtomicAddInArrayDb()`. Pool temporaries by liveness. |
| ET-6 | **Critical path analysis** | Compute longest dependency chain via topological sort of EDT DAG. Annotate `critical_path_distance` for runtime priority scheduling. |
| ET-7 | **Prefetch hint generation** | For BlockCyclic/Tiling2D, identify next-iteration DB access pattern and emit early dependency registration or `ARTS_EVENT_CHANNEL` for streaming. |

---

## 4. Pipeline Integration (Revised)

```text
Stage 12: concurrency-opt
  EdtStructuralOptPass(runAnalysis=false)
  DCE, PolygeistCanonicalize, CSE
  EpochOpt                          [+ EXT-EPOCH-1: epoch scope narrowing]
  PolygeistCanonicalize, CSE
  DbPartitioning                    [+ FIX-1: per-acquire coarse]
                                    [+ FIX-2: summaries to heuristics]
                                    [+ FIX-3: persist refinements]
                                    [+ FIX-6: standardize fallbacks]
                                    [+ EXT-PART-1..5: analysis extensions]
  DbDistributedOwnership            [+ EXT-DIST-1..2: relaxed eligibility]
  ──────────────────────────────── NEW ────────────────────────────────
  DbTransforms                      [DT-1..7: contract refinement, ESD,
                                     GUID ranges, replication, atomics]
  EdtTransforms                     [ET-1..7: granularity, affinity,
                                     dep narrowing, overlap, reduction,
                                     critical path, prefetch]
  ──────────────────────────────────────────────────────────────────────
  DbModeTightening                            [+ EXT-DB-1..2: richer mode inference]
  DbScratchElimination
  BlockLoopStripMining
  ArtsHoisting
  EdtStructuralOptPass(runAnalysis=false)        [+ EXT-EDT-1..2: inlining + dead deps]
  PolygeistCanonicalize, CSE
  EdtAllocaSinking, DCE, Mem2Reg
```

---

## 5. Contract Changes

### 5.1 New Fields for LoweringContractInfo

```cpp
// Dimension-aware stencil analysis (EXT-PART-1)
SmallVector<int64_t, 4> stencilIndependentDims;

// Cached block window to avoid recomputation (DT-7)
std::optional<int64_t> cachedStartBlock;
std::optional<int64_t> cachedBlockCount;

// ESD annotation (DT-3)
std::optional<int64_t> esdByteOffset;
std::optional<int64_t> esdByteSize;

// Post-DB refinement marker (DT-1)
bool postDbRefined = false;

// Task sizing hint (ET-1)
enum class ComputeIntensity { trivial, light, medium, heavy };
std::optional<ComputeIntensity> computeIntensity;

// Critical path distance (ET-6)
std::optional<int64_t> criticalPathDistance;
```

### 5.2 Vocabulary Generalization

Rename stencil-prefixed semantic attrs for generality (non-breaking, the
contract struct fields are already general):

| Current Semantic Attr | Proposed | Rationale |
| --- | --- | --- |
| `stencil_spatial_dims` | `spatial_dims` | Already general in LoweringContractInfo |
| `stencil_owner_dims` | `owner_dims` | Already general in LoweringContractInfo |
| `stencil_min_offsets` | `dep_neighborhood_min` | Applies to any neighborhood |
| `stencil_max_offsets` | `dep_neighborhood_max` | Applies to any neighborhood |
| `stencil_write_footprint` | `write_footprint` | Already general in LoweringContractInfo |
| `supported_block_halo` | `supports_neighborhood_expansion` | Not stencil-specific |

---

## 6. Implementation Phases

### Phase 0: Critical Fix (Immediate)

```text
FIX-1  Per-acquire coarse fallback     [62-210x regression on affected benchmarks]
```

Change EdtRewriter.cpp:31-42 and ForLowering.cpp:121-123 to check per-acquire
overlapping read/write offsets instead of global allocation classification.

### Phase 1: Contract Infrastructure (Foundation)

```text
FIX-2  Feed AcquireContractSummary to heuristics
FIX-3  Persist post-DB refinements to IR via upsertLoweringContract()
FIX-4  Cache DbAnalysis& in DbPartitioning
FIX-6  Standardize fallback ordering
```

These unblock all subsequent work. Without FIX-3, no refinement reaches
lowering. Without FIX-2, heuristics are blind to contracts.

### Phase 2: Core DbTransforms

```text
DT-1   Contract persistence             [foundation for all downstream]
DT-2   Stencil halo consolidation        [one path for stencil bounds]
EXT-PART-5  Heuristic-to-contract feedback
EXT-DB-1    Richer DB mode inference (PIN/ONCE/READ)
DT-4   GUID range allocation             [10-50x alloc overhead reduction]
```

### Phase 3: Partition Quality

```text
EXT-PART-1  Dimension-aware stencil partitioning
EXT-PART-2  Bounded indirect access analysis
EXT-PART-3  Use StencilBounds in block size decisions
EXT-PART-4  Per-dimension block plan merge
EXT-DIST-1  Allow read-only stencil distributed DBs
```

### Phase 4: Core EdtTransforms

```text
EXT-EDT-1   Complete EDT inlining for trivial bodies
EXT-EDT-2   Dead dependency elimination
ET-1        Task granularity control
ET-2        Data affinity placement
EXT-EPOCH-1 Epoch scope narrowing
```

### Phase 5: Advanced ARTS-Aware Optimizations

```text
DT-3   ESD annotation                   [1.5-3x stencils]
DT-5   DB replication                    [2-5x read-heavy distributed]
DT-6   Atomic reduction recognition      [2-10x reduction codes]
ET-3   Dependency chain narrowing        [10-40% wavefront]
ET-4   Comm/comp overlap (split EDT)     [2-5x bandwidth-bound]
ET-5   Reduction strategy selection
ET-6   Critical path analysis
DT-7   Block window caching
```

### Phase 6: Polish

```text
ET-7          Prefetch hint generation
EXT-DIST-2    Writer-aware route selection
FIX-5         Cache DbAnalysis in LoopAnalysis
Vocabulary    Rename stencil_* attrs
Validation    Contract validation pass (debug builds)
```

---

## 7. Impact Estimates

| ID | Transform | Est. Speedup | Effort |
| --- | --- | --- | --- |
| FIX-1 | Per-acquire coarse fix | **62-210x** on affected | Low |
| DT-4 | GUID range allocation | 10-50x alloc overhead | Medium |
| DT-6 | Atomic reductions | 2-10x reduction codes | Medium |
| DT-5 | DB replication | 2-5x read-heavy dist. | Medium |
| ET-4 | Comm/comp overlap | 2-5x bandwidth-bound | High |
| DT-3 | ESD annotation | 1.5-3x stencils | Medium |
| EXT-DB-1 | DB mode inference | 1.2-2x general | Medium |
| ET-2 | Data affinity | 5-30% multi-node/NUMA | Medium |
| ET-1 | Task granularity | 10-50% EDT overhead | Medium |
| EXT-PART-1 | Dim-aware stencil | 5-30% 3D stencils | Medium |
| ET-3 | Dep chain narrowing | 10-40% wavefront | High |
| EXT-PART-4 | Per-dim block merge | 5-15% block sizing | Low |
| ET-6 | Critical path analysis | 5-20% scheduling | Medium |
| ET-7 | Prefetch hints | 10-30% cache-sensitive | Medium |

---

## 8. The Clean Question Each Layer Should Ask

| Layer | Question | Status | Fix |
| --- | --- | --- | --- |
| Pattern | "What family is this?" | Working | -- |
| Pre-DB Analysis | "What facts refine that family?" | Working | -- |
| Post-DB Analysis | "What ARTS-specific facts can I add?" | **Missing** | DbTransforms + contract persistence |
| Controller | "What policy follows from family + machine facts?" | **Partially broken** | Feed summaries to heuristics |
| Lowering | "How do I encode that chosen policy?" | Working | -- |

---

## Appendix A: Full Conservative Fallback Catalogue

| # | Location | Decision | Severity |
| --- | --- | --- | --- |
| 1 | EdtRewriter.cpp:31-42 | CASE 2: global coarse for inout+stencil | **Critical** |
| 2 | PartitionBoundsAnalyzer.cpp:686-696 | Full-range for ANY indirect access | High |
| 3 | PartitionBoundsAnalyzer.cpp:638-719 | Full-range for non-leading stencil dim | High |
| 4 | ForLowering.cpp:406-420 | BlockCyclic full-range acquire window | High |
| 5 | ForLowering.cpp:271-273 | 1-D block window (N-D ignored) | High |
| 6 | PartitioningHeuristics.cpp:215-228 | Full-range for read-only stencil (can't prove dim independence) | Medium |
| 7 | PartitioningHeuristics.cpp:269-273 | Block instead of stencil (peer-inferred dims) | Medium |
| 8 | DbBlockPlanResolver.cpp:124-196 | MIN across block candidates | Medium |
| 9 | DbBlockPlanResolver.cpp:346-427 | One dissenter blocks all partition dims | Medium |
| 10 | DbDimAnalyzer.cpp:275-296 | Peer dim fallback | Medium |
| 11 | Concurrency.cpp:185-192 | Force intranode for nested tasks | Medium |
| 12 | Concurrency.cpp:195-204 | Force intranode for non-internode consumer | Medium |
| 13 | DbBlockPlanResolver.cpp:36-60 | 2D stencil halving | Medium |
| 14 | ForLowering.cpp:167-168 | Full-extent stencil halo (ignores actual bounds) | Medium |
| 15 | ForLowering.cpp:121-123 | Force coarse for BlockCyclic | Medium |
| 16 | DbDistributedOwnership eligibility | Reject stencil read-only allocations | Medium |
| 17 | PartitioningHeuristics.cpp:308-320 | Min pinned dims (uneven requirements) | Low |
| 18 | DbBlockPlanResolver.cpp:286-344 | Static contract shape (ignores dynamic sizes) | Low |
| 19 | ForLowering.cpp:149-153 | Coarse for single-element arrays | Low |
| 20 | DbLowering.cpp:54-134 | Recompute block window (already computed in ForLowering) | Low |

## Appendix B: Unused ARTS Runtime Capabilities

| Category | API | Current | Potential |
| --- | --- | --- | --- |
| **Allocation** | `arts_guid_reserve_range()` | Individual allocs | Bulk O(1) |
| **Allocation** | `arts_guid_reserve_round_robin()` | Manual modular arith | Load-balanced |
| **Replication** | `artsAddDbDuplicate()` | None | Read-heavy caching |
| **Replication** | `artsRemoteUpdateRouteTable()` | None | Pre-populate routes |
| **Atomics** | `artsAtomicAddInArrayDb()` | None | Lock-free reductions |
| **Atomics** | `artsAtomicCompareAndSwapInArrayDb()` | None | Lock-free updates |
| **DB Modes** | `ARTS_DB_PIN` | Unused | Local-only bypass |
| **DB Modes** | `ARTS_DB_ONCE` / `ARTS_DB_ONCE_LOCAL` | Unused | Auto-free temporaries |
| **DB Modes** | `ARTS_DB_LC` / `ARTS_DB_LC_SYNC` | Unused | GPU-CPU coherence |
| **Events** | `ARTS_EVENT_COUNTED` | Unused (only LATCH) | Aggregation events |
| **Events** | `ARTS_EVENT_CHANNEL` | Unused | Versioned streaming |
| **Events** | `ARTS_EVENT_IDEM` | Unused | Idempotent ops |
| **Buffers** | `arts_allocate_local_buffer()` | Unused | Staging/aggregation |
| **Init** | `init_per_worker()` | Unused | NUMA/cache setup |
| **Placement** | EDT `cluster`, `edtSpace` params | Set to 0 | NUMA affinity |
| **DB Mgmt** | `arts_db_rename()` | Unused | Mode change post-create |
| **Introspection** | `arts_get_current_numa_domain()` | Unused | NUMA-aware placement |
| **Remote** | `artsRemotePutInDb()` | Unused | Explicit remote write |
| **Remote** | `artsRemoteGetFromDb()` | Unused | Explicit remote read |

## Related Documents

- [`pipeline.md`](../compiler/pipeline.md) -- full stage order
- [`dependence.md`](./dependence.md) -- dependence transform families
- [`kernel.md`](./kernel.md) -- kernel transform families
- [`../heuristics/partitioning.md`](../heuristics/partitioning.md) -- DB heuristics
- [`../heuristics/distribution.md`](../heuristics/distribution.md) -- distribution strategies
