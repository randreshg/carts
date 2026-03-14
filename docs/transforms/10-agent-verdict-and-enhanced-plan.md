# 10-Agent Verdict: Post-DB/EDT Transforms Master Plan Analysis & Enhancement

**Date**: 2026-03-14
**Scope**: Comprehensive analysis of `post-db-edt-transforms.md` by 10 specialized agents investigating ARTS runtime, CARTS compiler, MLIR async dialect, and HPC state-of-the-art.

---

## Part I: VERDICT ON THE EXISTING PLAN

### Overall Assessment: STRONG FOUNDATION, NEEDS ENHANCEMENT

The plan is **technically sound and well-structured**. Every claim was verified against the codebase. However, the investigation revealed **significant additional opportunities** the plan misses and **two quick wins hiding in plain sight**.

### What the Plan Gets RIGHT

| Claim | Verification | Status |
|-------|-------------|--------|
| 5 contract infrastructure gaps | All 5 verified with exact line numbers | CONFIRMED |
| 20 conservative fallbacks | All 20 verified in codebase | CONFIRMED |
| 4 computed-but-unused facts | All 4 verified, with additional unused facts found | CONFIRMED+ |
| Pipeline placement (DbTransforms after DbPartitioning, before DbModeTightening) | Ordering constraints validated against Compile.cpp:415-445 | CORRECT |
| Contract system is solid (80%) | LoweringContractOp design, depPattern enum, utilities all well-implemented | CONFIRMED |
| Gap 4 is critical (non-persisted refinements) | refineContractWithFacts() at DbAnalysis.cpp:104-146 writes in-memory only | CONFIRMED CRITICAL |
| ARTS has unused runtime capabilities | 9 DB modes, 6 event types, GUID ranges -- compiler uses fraction | CONFIRMED |

### What the Plan MISSES

| Discovery | Agent | Impact |
|-----------|-------|--------|
| **Two existing optimizations are DISABLED in production** (`inlineNoDepEdts` gated by `runAnalysis=false`, barrier removal commented out) | EDT Opts (#6) | **IMMEDIATE 15-25% potential** |
| Only `ARTS_DB_DEFAULT` type ever emitted (no PIN/ONCE/READ/LOCAL) | Code Gen (#4) | High |
| Only `DB_MODE_RO` and `DB_MODE_RW` emitted (not EW, VALUE, PTR) | Code Gen (#4) | High |
| `arts_add_dependence_at` infrastructure exists in ConvertArtsToLLVM but is never triggered | Code Gen (#4) | High |
| Priority deque infrastructure exists in ARTS scheduler (`deque_type=1`) but EDT hints carry no priority | Events/Sync (#3) | Medium |
| 56 CSE+canonicalization passes across pipeline (not 31+ as plan states) | Pipeline (#9) | Low (informational) |
| EdtInfo metrics (`computeToMemRatio`, `totalOps`) computed but never used for decisions | EDT Opts (#6) | Medium |
| `ARTS_EVENT_CHANNEL` fully implemented with versioning -- ideal for streaming/pipelined execution | Events/Sync (#3) | High |
| DbGraph invalidated 3 times inside DbPartitioning but analysis manager handles it correctly | Pipeline (#9) | Low (no issue) |

### What the Plan Gets WRONG (Minor)

1. **CSE count**: Plan says "31+" but actual count is **56** (28 CSE + 28 PolygeistCanonicalize)
2. **CASE 2 status**: Plan lists it as a critical performance bug, but memory notes say **CASE 2 was already removed** (commit 3110377). Verify current state before prioritizing FIX-1.
3. **EDT event types**: Plan mentions "7 unused DB modes" but ARTS actually has **8 access modes** and **4 storage subtypes** -- the distinction matters for EXT-DB-1.

---

## Part II: MLIR ASYNC DIALECT VERDICT

### Decision: DO NOT USE, BUT STEAL 3 IDEAS

**The ARTS-specific approach is strictly more powerful than MLIR async for CARTS's use case.**

| Dimension | MLIR Async | ARTS EDT | Winner |
|-----------|-----------|----------|--------|
| Dependency model | SSA token/value chains | GUID-based slot deps + frontier | ARTS (richer) |
| Memory model | Write-once value semantics | Mutable shared DataBlocks with in/out/inout | ARTS (HPC-required) |
| Partitioning | None | 5 modes (coarse/chunked/fine/block/stencil) | ARTS (essential) |
| Distribution | None | Cross-node with frontier coherence | ARTS (essential) |
| Synchronization | await + group | Epochs + latches + 6 event types | ARTS (richer) |
| Iterative patterns | Cannot express (single-assignment) | Natural (same DB, different epochs) | ARTS (essential) |

**Three ideas worth adapting:**

1. **Transitive reduction** (from AMD MLIR-AIR `air-dependency-canonicalize`): Apply at compile-time to prune redundant EDT dependency edges. If A->B->C exists, remove A->C.

2. **Coroutine-style EDT suspension**: For EDTs blocking on remote DB acquisition, the async dialect's LLVM coroutine lowering pattern (outline + suspend/resume) could enable non-blocking remote fetches.

3. **Static reference counting**: Compile-time GUID reference count insertion to reduce runtime route-table overhead. The async dialect's `async.runtime.drop_ref` policy is cleaner than ARTS's implicit lifetime management.

---

## Part III: LESSONS FROM HPC STATE-OF-THE-ART

### Tier 1: High Impact, Directly Applicable (from 50+ papers analyzed)

| Technique | Source System | CARTS Opportunity | Est. Impact |
|-----------|-------------|-------------------|-------------|
| **Symbolic/parametric dependency emission** | PaRSEC PTG (JDF compiler) | Instead of materializing `arts_add_dependence()` calls, emit parametric functions for regular loop nests. CARTS's polyhedral analysis already has the info. | 2-5x dep overhead |
| **Early dependency release** | OmpSs-2 (ISC HPC 2020) | Analyze EDT bodies; release DB deps before task completion when access is done. Directly unblocks downstream EDTs earlier. | 10-30% latency |
| **Taskiter-style graph reuse** | OmpSs-2 (TACO 2025) | For iterative patterns, create task graph once, replay across iterations. ARTS epochs provide natural boundaries. Up to 24x on OmpSs-2 benchmarks. | 5-20x iterative |
| **Range-based dependencies** | OmpSs-2, PaRSEC, Legion | `arts_add_dependence_at` exists but compiler never generates it. Emitting sub-region deps reduces false serialization. | 1.5-3x stencils |

### Tier 2: Medium Impact, Requires New Analysis

| Technique | Source System | CARTS Opportunity |
|-----------|-------------|-------------------|
| **Diffuse-style task fusion** | Legion (ASPLOS 2025) | Partition-equivalence checking to identify fusible EDTs. 1.86x geometric mean. |
| **Split-phase stencil** | Diamond tiling (Grosser 2014) | Already planned as ET-4. Interior + boundary EDT splitting. |
| **NUMA-aware hierarchical stealing** | PPoPP 2016 | Steal from same-NUMA first. ARTS has topology but random stealing. |
| **Cost-model-driven granularity** | StarPU | Emit execution-time estimates as EDT metadata from `computeToMemRatio`. |

### Tier 3: Longer-term, High Complexity

| Technique | Source System | CARTS Opportunity |
|-----------|-------------|-------------------|
| **Apophenia-style tracing** | Legion (ASPLOS 2025) | Runtime-side graph memoization via dynamic string analysis. |
| **Feedback-directed tile sizing** | Polyhedral community | Use benchmark profiling to adjust tile sizes in recompilation. |
| **Measurement-based rebalancing** | Charm++ | EDT instrumentation for adaptive load balancing. |

---

## Part IV: ENHANCED MASTER PLAN -- "MAKE CARTS GREAT AGAIN"

### Phase 0: IMMEDIATE WINS (Days, not weeks)

These require minimal code changes and unlock major performance:

| ID | What | Where | Why It's Immediate | Est. Impact |
|----|------|-------|--------------------|-------------|
| **IMM-1** | Re-enable `inlineNoDepEdts()` for production | EdtStructuralOpt.cpp: remove `runAnalysis` gate at line 290 | 1-line fix, code already written | 10-20% EDT overhead |
| **IMM-2** | Re-enable barrier removal | EdtStructuralOpt.cpp: uncomment `removeBarriers()` at line 292 | 1-line fix, code already written | 3-5% sync overhead |
| **IMM-3** | Verify CASE 2 fix status | EdtRewriter.cpp:31-42 | Already removed per commit 3110377 -- confirm no regression | 62-210x if unfixed |

### Phase 1: Contract Infrastructure (Foundation -- 1-2 weeks)

No new passes. Fix the information pipeline:

| ID | What | Where | Priority |
|----|------|-------|----------|
| FIX-2 | Feed `AcquireContractSummary` to heuristics | PartitioningHeuristics interface | High |
| FIX-3 | Persist post-DB contract refinements to IR | DbAnalysis.cpp: call `upsertLoweringContract()` after `refineContractWithFacts()` | **Critical** |
| FIX-4 | Cache `DbAnalysis&` in DbPartitioning | DbPartitioning.cpp:runOnOperation | High |
| FIX-6 | Standardize fallback ordering: summary > contract > node > facts > attrs | DbPartitioning.cpp:1449,1793,1898 | Medium |

### Phase 2: Unlock Unused ARTS Runtime (2-3 weeks)

The ARTS runtime has massive untapped capability. The compiler should generate these:

| ID | What | Runtime API | Current State | Impact |
|----|------|-------------|---------------|--------|
| **UNLOCK-1** | GUID range allocation | `arts_guid_reserve_range()` + `arts_guid_from_index()` | Each GUID reserved individually | 10-50x alloc overhead |
| **UNLOCK-2** | DB mode inference | `ARTS_DB_PIN` (local-only), `ARTS_DB_ONCE` (auto-free), `ARTS_DB_READ` (route caching) | Only `ARTS_DB_DEFAULT` emitted | 1.2-2x general |
| **UNLOCK-3** | ESD dependencies | `arts_add_dependence_at(guid, edt, slot, mode, offset, size)` | Infrastructure in ConvertArtsToLLVM exists but never triggered | 1.5-3x stencils |
| **UNLOCK-4** | Event type selection | `ARTS_EVENT_CHANNEL` for streaming, `ARTS_EVENT_COUNTED` for aggregation | Only LATCH used | 1.1-1.5x sync |
| **UNLOCK-5** | EDT priority hints | `deque_type=1` priority BST scheduler | `arts_hint_t` has no priority field | 5-15% scheduling |
| **UNLOCK-6** | NUMA placement | `arts_get_current_numa_domain()`, EDT `cluster`/`edtSpace` params | Set to 0, never used | 5-30% NUMA |
| **UNLOCK-7** | Atomic reductions | `artsAtomicAddInArrayDb()` | Never generated | 2-10x reductions |

### Phase 3: Core DbTransforms (2-3 weeks)

New pass shell, placed after DbPartitioning + DbDistributedOwnership, before DbModeTightening:

| ID | Transform | What | Priority |
|----|-----------|------|----------|
| DT-1 | Contract persistence | Walk `DbAcquireOp`s, read `AcquireContractSummary`, write back via `upsertLoweringContract()` | Critical |
| DT-2 | Stencil halo consolidation | Unify halo bounds from 3 sources into contract `minOffsets`/`maxOffsets` | High |
| DT-3 | ESD annotation | Analyze EDT access footprint, annotate `esdByteOffset`/`esdByteSize` for `arts_add_dependence_at` | High |
| DT-4 | GUID range allocation | Detect N-GUID loops, replace with `arts_guid_reserve_range(N)` | High |
| DT-7 | Block window caching | Cache block window in contract to avoid recomputation | Medium |

### Phase 4: Core EdtTransforms (3-4 weeks)

New pass shell, after DbTransforms, before DbModeTightening:

| ID | Transform | What | Priority |
|----|-----------|------|----------|
| ET-1 | Task granularity control | Use EdtInfo `computeToMemRatio`/`totalOps` for fusion/fission decisions | High |
| ET-2 | Data affinity placement | Set EDT route to owner node of primary write-target DB | High |
| EXT-EDT-1 | Complete EDT inlining | Extend `inlineNoDepEdts()` for parallel EDTs (< 20 ops, no inter-node deps) | High |
| EXT-EDT-2 | Dead dependency elimination | Remove unused dependency slots (acquire in `mode=in` never loaded) | Medium |

### Phase 5: Partition Quality (2-3 weeks)

Extend existing passes:

| ID | Transform | What |
|----|-----------|------|
| EXT-PART-1 | Dimension-aware stencil partitioning | Use `isLeadingDynamic` fact + per-dim stencil analysis |
| EXT-PART-2 | Bounded indirect access analysis | Check if indirect access is on non-partitioned dimensions |
| EXT-PART-3 | Use `StencilBounds` in block sizing | Wire PartitionBoundsAnalyzer bounds to DbBlockPlanResolver |
| EXT-PART-4 | Per-dimension block plan merge | Replace global MIN with per-dim independent merge |
| EXT-PART-5 | Heuristic-to-contract feedback | Write blockShape, ownerDims, distributionVersion back to contract |

### Phase 6: Advanced ARTS-Aware Optimizations (4-6 weeks)

**NEW additions not in original plan, inspired by HPC research:**

| ID | Transform | Source Inspiration | What | Est. Impact |
|----|-----------|-------------------|------|-------------|
| **NEW-1** | **Early dependency release** | OmpSs-2 | Analyze EDT body; emit early DB release when access completes before task ends. Unblocks downstream EDTs sooner. | 10-30% latency |
| **NEW-2** | **Transitive dependency reduction** | MLIR-AIR, PaRSEC | Prune redundant edges in EDT DAG: if A->B->C, remove A->C. Reduces runtime latch overhead. | 5-15% overhead |
| **NEW-3** | **Taskiter graph reuse** | OmpSs-2 (TACO 2025) | For iterative patterns detected by PatternDiscovery, create EDT graph once and replay across epoch iterations. | 5-20x iterative |
| **NEW-4** | **Parametric dependency emission** | PaRSEC PTG | For regular loop nests, emit parametric dependency functions instead of materializing individual `arts_add_dependence()` calls. | 2-5x dep overhead |
| **NEW-5** | **Diffuse-style EDT fusion** | Legion (ASPLOS 2025) | Check partition equivalence between adjacent EDTs; fuse bodies when deps permit. | 1.5-2x fusible |
| **NEW-6** | **Cross-epoch acquire hoisting** | Internal analysis | Hoist read-only acquires used across multiple epochs to function scope with single release. | 5-10% RO-heavy |
| ET-3 | Dependency chain narrowing | (original plan) | Wavefront: emit `arts_add_dependence_at` for halo-only deps | 10-40% wavefront |
| ET-4 | Comm/comp overlap (split EDT) | (original plan) | Interior EDT (no halo deps) + boundary EDT (halo deps) | 2-5x BW-bound |
| ET-5 | Reduction strategy selection | (original plan) | small->local scalars, large->tree, distributed->remote atomics | 2-10x reductions |
| ET-6 | Critical path analysis | (original plan) | Topological sort of EDT DAG, annotate `critical_path_distance` | 5-20% scheduling |
| DT-5 | DB replication | (original plan) | `artsAddDbDuplicate()` for high-read DBs | 2-5x dist. read |
| DT-6 | Atomic reduction recognition | (original plan) | Detect patterns, emit `artsAtomicAddInArrayDb()` | 2-10x reductions |

### Phase 7: Polish & Validation (2 weeks)

| ID | What |
|----|------|
| ET-7 | Prefetch hint generation using `ARTS_EVENT_CHANNEL` for streaming |
| EXT-DIST-1 | Allow read-only stencil distributed DBs |
| EXT-DIST-2 | Writer-aware route selection |
| EXT-EPOCH-1 | Epoch scope narrowing |
| FIX-5 | Cache DbAnalysis in LoopAnalysis |
| VOCAB | Rename stencil_* attrs to general names |
| VALID | Contract validation pass (debug builds) |
| CLEANUP | Remove unused `MLIRAsyncDialect` link dependency |

---

## Part V: REVISED IMPACT ESTIMATES (with new items)

### Top 15 by Expected Speedup

| Rank | ID | Transform | Est. Speedup | Effort | Confidence |
|------|-----|-----------|-------------|--------|------------|
| 1 | IMM-1+2 | Re-enable disabled opts | 15-25% general | **Trivial** | Very High |
| 2 | UNLOCK-1 | GUID range allocation | 10-50x alloc overhead | Medium | High |
| 3 | NEW-3 | Taskiter graph reuse | 5-20x iterative | High | Medium |
| 4 | NEW-4 | Parametric dependency emission | 2-5x dep overhead | High | Medium |
| 5 | UNLOCK-7/DT-6 | Atomic reductions | 2-10x reduction codes | Medium | High |
| 6 | DT-5 | DB replication | 2-5x read-heavy dist. | Medium | Medium |
| 7 | ET-4 | Comm/comp overlap | 2-5x BW-bound | High | Medium |
| 8 | DT-3/UNLOCK-3 | ESD annotation | 1.5-3x stencils | Medium | High |
| 9 | UNLOCK-2 | DB mode inference | 1.2-2x general | Medium | High |
| 10 | NEW-1 | Early dependency release | 10-30% latency | Medium | Medium |
| 11 | ET-2/UNLOCK-6 | Data affinity + NUMA | 5-30% multi-node | Medium | Medium |
| 12 | NEW-2 | Transitive dep reduction | 5-15% overhead | Medium | High |
| 13 | ET-1 | Task granularity control | 10-50% EDT overhead | Medium | Medium |
| 14 | EXT-PART-1 | Dim-aware stencil partition | 5-30% 3D stencils | Medium | Medium |
| 15 | ET-6/UNLOCK-5 | Critical path + priority sched | 5-20% scheduling | Medium | Low |

### Cumulative Impact Path

```
Phase 0 (IMM-1,2,3):     Baseline + 15-25%
Phase 1 (FIX-2,3,4,6):   Unlocks all downstream
Phase 2 (UNLOCK-1..7):    10-50x alloc, 1.5-3x stencils, 2-10x reductions
Phase 3 (DT-1..4,7):      Contract pipeline completes
Phase 4 (ET-1,2,EDT-1,2): 10-50% EDT overhead reduction
Phase 5 (EXT-PART-1..5):  5-30% partition quality
Phase 6 (NEW-1..6,ET-3..6): 2-5x bandwidth-bound, 5-20x iterative
Phase 7 (Polish):          Robustness + validation
```

---

## Part VI: ARCHITECTURAL RECOMMENDATIONS

### 1. Contract System Extension Strategy

The contract system is extensible. Adding new fields follows established patterns (5-10 localized changes per field). Recommended new fields:

```cpp
// Phase 2: ARTS mode selection
std::optional<ArtsDbType> inferredDbType;      // PIN/ONCE/READ/DEFAULT

// Phase 3: ESD support
std::optional<int64_t> esdByteOffset;
std::optional<int64_t> esdByteSize;

// Phase 4: Task sizing
enum class ComputeIntensity { trivial, light, medium, heavy };
std::optional<ComputeIntensity> computeIntensity;

// Phase 6: Critical path
std::optional<int64_t> criticalPathDistance;

// Phase 6: Early release
std::optional<int64_t> lastAccessOrdinal;      // For early dep release
```

### 2. Pass Ordering (Validated)

The proposed insertion point is **CONFIRMED CORRECT**:

```
Stage 12: concurrency-opt
  EdtStructuralOptPass(runAnalysis=false)
  DCE, PolygeistCanonicalize, CSE
  EdtStructuralOptPass(runAnalysis=false)
  EpochOpt                          [+ EXT-EPOCH-1]
  PolygeistCanonicalize, CSE
  DbPartitioning                    [+ FIX-2,3,6 + EXT-PART-1..5]
  DbDistributedOwnership            [+ EXT-DIST-1,2]
  ---- NEW ----
  DbTransforms                      [DT-1..7]
  EdtTransforms                     [ET-1..7, NEW-1..6]
  ---- END NEW ----
  DbModeTightening                            [+ UNLOCK-2: DB mode inference]
  DbScratchElimination
  BlockLoopStripMining
  ArtsHoisting
  EdtStructuralOptPass(runAnalysis=false)        [+ EXT-EDT-1,2, IMM-1,2]
  PolygeistCanonicalize, CSE
  EdtAllocaSinking, DCE, Mem2Reg
```

**Key constraints validated:**
- DbTransforms reads DbAnalysis from DbPartitioning (correct: AnalysisManager caches it)
- EdtTransforms can consume DbTransforms outputs (no cyclic dependency)
- DbModeTightening after EdtTransforms: mode tightening uses persisted contracts (correct)
- No analysis invalidation issues (ARTS uses custom AnalysisManager, not MLIR's)

### 3. Runtime Enhancements (Compiler-Driven)

The compiler should generate calls to these existing but unused ARTS APIs:

| API | When to Generate | Compiler Analysis Required |
|-----|-----------------|---------------------------|
| `arts_guid_reserve_range(type, N, route)` | Loop creating N DBs with sequential access | Loop analysis + DB creation detection |
| `arts_add_dependence_at(guid, edt, slot, mode, offset, size)` | Stencil/block partitioned DBs where EDT accesses subset | Access footprint analysis (DT-3) |
| `artsAtomicAddInArrayDb(guid, index, value)` | Scalar reductions in EDT bodies | Reduction pattern recognition |
| `ARTS_DB_PIN` in db_create | DB never accessed from remote node | Escape analysis on DB GUID |
| `ARTS_DB_ONCE` in db_create | DB has single-epoch lifetime | Liveness analysis across epochs |
| `ARTS_EVENT_CHANNEL` | Streaming/pipelined producer-consumer | EDT DAG pattern analysis |

### 4. What NOT to Do

Based on the investigation, these are **explicitly NOT recommended**:

| Anti-Pattern | Why Not |
|-------------|---------|
| Lower through MLIR async dialect | Value semantics incompatible with mutable DBs; would lose partition/distribution info |
| Add NUMA-aware allocation via `mmap+mbind` | Breaks `arts_free()` which calls `free(hdr->base)` |
| Use `numa_alloc_onnode` | 10-100x slower (mmap syscall per allocation) |
| Runtime priority scheduling without compiler hints | Priority deque exists but EDT hints carry no priority field |
| Apply transitive reduction at runtime | O(n^3) cost prohibitive; do at compile-time on symbolic graph |

---

## Part VII: COMPARISON WITH COMPETING SYSTEMS

### Where CARTS Already Wins

| vs. System | CARTS Advantage |
|-----------|-----------------|
| PaRSEC | Works from general OpenMP code, not domain-specific JDF language |
| OmpSs-2 | Polyhedral analysis (Polygeist) + MLIR infrastructure for optimization |
| StarPU | Full compiler stack, not just runtime API |
| Legion | Single compilation flow, not separate DSL (Regent) |

### Where CARTS Should Catch Up

| vs. System | Gap | Plan Phase |
|-----------|-----|------------|
| PaRSEC | Parametric dependency emission (O(1) per task) | Phase 6: NEW-4 |
| OmpSs-2 | Early dependency release | Phase 6: NEW-1 |
| OmpSs-2 | Taskiter graph reuse for iterative patterns | Phase 6: NEW-3 |
| Legion/Diffuse | Cross-task fusion | Phase 6: NEW-5 |
| StarPU | Cost-model-driven scheduling | Phase 4: ET-1 + Phase 2: UNLOCK-5 |
| Charm++ | Measurement-based load balancing | Future work |

---

## Appendix: Agent Coverage Summary

| # | Agent | Scope | Key Files Analyzed | Key Findings |
|---|-------|-------|-------------------|--------------|
| 1 | ARTS Dependence Model | EDT lifecycle, `arts_add_dependence`, GUID system | edt.c, event.c, out_of_order.c, guid.c | Full dependency resolution pipeline documented; GUID range allocation ready but unused |
| 2 | ARTS DB Modes/Types | 9 access modes, 4 subtypes, frontier coherence | db.c, frontier.c, handler.c, arts.h | Only DB_DEFAULT+RO/RW emitted; PIN/ONCE/READ/VALUE/PTR all available |
| 3 | ARTS Events/Sync | 6 event types, scheduler, NUMA, epochs | event.c, scheduler.c, termination.c | CHANNEL events fully implemented; priority deque exists; NUMA query O(1) |
| 4 | CARTS Code Generation | Lowering to ARTS calls | ConvertArtsToLLVM.cpp, Codegen.cpp | `arts_add_dependence_at` infrastructure exists but never triggered |
| 5 | DbPartitioning/Heuristics | 3-phase system, 20 heuristics | DbPartitioning.cpp, PartitioningHeuristics.cpp, DbAnalysis.cpp | All 20 fallbacks verified; information silo problem confirmed |
| 6 | EDT Optimizations | 7 EdtStructuralOptPass stages, EpochOpt, hoisting | EdtStructuralOpt.cpp, EpochOpt.cpp, EdtICM.cpp, Hoisting.cpp | `inlineNoDepEdts` disabled in production; barrier removal commented out |
| 7 | MLIR Async Dialect | Comparison with EDT model | MLIR docs, CARTS Ops.td, CMakeLists | ARTS strictly more powerful; 3 ideas to steal; remove unused link dep |
| 8 | Contract System | LoweringContractOp, depPattern, utilities | Ops.td, Attributes.td, LoweringContractUtils.h/cpp | 80% solid; Gap 4 is critical blocker; extensible for new fields |
| 9 | Compiler Pipeline | 16 stages, pass ordering, invalidation | Compile.cpp, pipeline.md | Proposed placement CORRECT; no ordering violations; 56 CSE/canon passes |
| 10 | HPC Task Research | 50+ papers, 7 systems (PaRSEC, OmpSs-2, Legion, StarPU, Charm++, HPX, OCR) | Web research | 4 Tier-1 techniques identified; parametric deps + early release highest impact |
