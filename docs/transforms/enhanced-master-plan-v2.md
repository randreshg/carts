# Enhanced Master Plan v2: Post-DB/EDT Optimization & ARTS-Aware Transforms

**Date**: 2026-03-14
**Status**: FINAL -- ready for implementation
**Based on**: 10-agent deep investigation of ARTS runtime, CARTS compiler, MLIR ecosystem, and 50+ HPC papers

---

## 1. Executive Summary

CARTS has a solid compiler foundation but **leaves massive ARTS runtime capability on the table**. The compiler only emits `ARTS_DB_DEFAULT` (1 of 9 modes), `ARTS_EVENT_LATCH` (1 of 6 event types), and never generates `arts_add_dependence_at`, `arts_guid_reserve_range`, atomic reductions, or priority hints. Two existing optimizations are disabled in production. The contract system works but has a critical persistence gap.

This plan organizes fixes into **7 phases** across **4 architectural layers**: Contract Infrastructure, DB Transforms, EDT Transforms, and Runtime Unlocks.

---

## 2. Architectural Layers

### Layer 1: Contract Infrastructure (the information backbone)

The contract system flows information from pattern detection (stage 5) through partitioning (stage 12) to lowering (stage 14). It consists of:

**IR Representation:**
- `LoweringContractOp` -- first-class MLIR op on DB pointer values
- `depPattern` enum (11 families: unknown, uniform, stencil, matmul, triangular, wavefront_2d, jacobi_alternating_buffers, elementwise_pipeline, stencil_tiling_nd, cross_dim_stencil_3d, higher_order_stencil)
- SSA operands: `blockShape`, `minOffsets`, `maxOffsets`, `writeFootprint`
- Static attrs: `owner_dims`, `spatial_dims`, `distribution_*`, `supported_block_halo`

**In-Memory Analysis:**
- `LoweringContractInfo` -- struct in `LoweringContractUtils.h:15-41`
- `AcquireContractSummary` -- post-DB refined view in `DbAnalysis.h:58-91`
- `DbAcquirePartitionFacts` -- graph-canonical facts in `DbNode.h:74-91`
- `AcquireRewriteContract` -- thinned view for lowering in `LoweringContractUtils.h:43-50`

**Utilities:**
- `upsertLoweringContract()` -- create/replace contract op
- `getLoweringContract()` -- extract contract from IR
- `deriveAcquireRewriteContract()` -- specialized for acquire rewriting
- `copyPatternAttrs()`, `copyStencilContractAttrs()` -- preservation during rewrites

**CRITICAL GAP (FIX-3):** `refineContractWithFacts()` in `DbAnalysis.cpp:104-146` computes `depPattern`, `distributionPattern`, `ownerDims`, `supportedBlockHalo` but only in-memory. Lowering reads the original, unrefined version. This single gap blocks ALL downstream transforms from seeing refined information.

### Layer 2: DB Transforms (data-centric optimizations)

Operate on `DbAllocOp` and `DbAcquireOp` after partitioning decisions are final. Current passes: DbPartitioning (3-phase heuristic), DbDistributedOwnership, DbModeTightening (mode tightening), DbScratchElimination (scratch elimination).

**Key files:**
- `lib/arts/passes/opt/db/DbPartitioning.cpp` (2230 lines, main partitioner)
- `lib/arts/analysis/db/DbAnalysis.cpp` (contract refinement)
- `lib/arts/analysis/heuristics/PartitioningHeuristics.cpp` (20 named heuristics)
- `lib/arts/analysis/graphs/db/PartitionBoundsAnalyzer.cpp` (bounds analysis)
- `lib/arts/transforms/db/DbBlockPlanResolver.cpp` (block plan resolution)

### Layer 3: EDT Transforms (task-centric optimizations)

Operate on `EdtOp` after DB analysis provides context. Current passes: EdtStructuralOptPass (7 stages), EpochOpt (epoch fusion + worker loop fusion), EdtICM, EdtPtrRematerialization, ArtsHoisting.

**Key files:**
- `lib/arts/passes/opt/edt/EdtStructuralOpt.cpp` (7-stage optimizer, 332 lines)
- `lib/arts/passes/opt/edt/EpochOpt.cpp` (epoch/worker fusion)
- `lib/arts/analysis/edt/EdtInfo.h` (computed but underused metrics)
- `lib/arts/analysis/graphs/edt/EdtGraph.cpp` (dependency graph)

### Layer 4: Runtime Unlocks (compiler generating more ARTS APIs)

The code generation pipeline (`lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`) emits ARTS runtime calls. Currently only uses: `ARTS_DB_DEFAULT`, `DB_MODE_RO`/`DB_MODE_RW`, `arts_guid_reserve()` (individual), `ARTS_EVENT_LATCH`.

**Unused ARTS APIs the compiler should generate:**
- `arts_guid_reserve_range()` + `arts_guid_from_index()` -- batch GUID allocation
- `arts_add_dependence_at()` -- sub-region dependencies (ESD)
- `artsAtomicAddInArrayDb()` -- lock-free reductions
- `ARTS_DB_PIN` / `ARTS_DB_ONCE` -- specialized DB modes
- `ARTS_EVENT_CHANNEL` / `ARTS_EVENT_COUNTED` -- richer synchronization
- EDT priority via `deque_type=1` priority BST scheduler
- `arts_get_current_numa_domain()` -- NUMA-aware placement

---

## 3. Pipeline Integration (Stage 12: concurrency-opt)

**Current pipeline** (Compile.cpp:415-445):
```
EdtStructuralOptPass(runAnalysis=false)     # EDT cleanup
DCE, Canonicalize, CSE
EdtStructuralOptPass(runAnalysis=false)     # Post-cleanup EDT pass
EpochOpt                       # Epoch fusion
Canonicalize, CSE
DbPartitioning                 # Partition mode selection
DbDistributedOwnership         # (optional) distributed eligibility
DbModeTightening                         # Mode tightening
DbScratchElimination           # Scratch elimination
Canonicalize, CSE
BlockLoopStripMining           # Block loop transformation
ArtsHoisting                   # Read-only acquire hoisting
Canonicalize, CSE
EdtAllocaSinking, DCE, Mem2Reg
```

**Enhanced pipeline** (new passes marked with >>>):
```
EdtStructuralOptPass(runAnalysis=false)     # EDT cleanup + IMM-1 (re-enable inlineNoDepEdts)
                               #             + IMM-2 (re-enable removeBarriers)
DCE, Canonicalize, CSE
EdtStructuralOptPass(runAnalysis=false)     # Post-cleanup EDT pass
EpochOpt                       # Epoch fusion + EXT-EPOCH-1
Canonicalize, CSE
DbPartitioning                 # + FIX-2,3,6 + EXT-PART-1..5
DbDistributedOwnership         # + EXT-DIST-1,2
>>> DbTransformsPass           # NEW: DT-1..7 (contract persistence, ESD, GUID ranges)
>>> EdtTransformsPass           # NEW: ET-1..7 (granularity, affinity, critical path)
DbModeTightening                         # + EXT-DB-1 (DB mode inference: PIN/ONCE/READ)
DbScratchElimination
Canonicalize, CSE
BlockLoopStripMining
ArtsHoisting
Canonicalize, CSE
EdtAllocaSinking, DCE, Mem2Reg
```

**Ordering constraints (all validated):**
1. DbTransforms AFTER DbPartitioning: reads final partition_mode
2. DbTransforms BEFORE DbModeTightening: writes contracts consumed by mode tightening
3. EdtTransforms AFTER DbTransforms: consumes ESD annotations and contract refinements
4. Analysis invalidation: safe (ARTS uses custom AnalysisManager, not MLIR's)

---

## 4. Implementation Phases

### Phase 0: Immediate Wins (IMM-1, IMM-2, IMM-3)

**IMM-1: Re-enable `inlineNoDepEdts()` for production**
- File: `lib/arts/passes/opt/edt/EdtStructuralOpt.cpp:290-308`
- Current: `inlineNoDepEdts()` only runs in `if (runAnalysis)` branch (line 290)
- But line 306 calls it in the `else` branch too -- **wait, re-read shows it IS called in the else branch at line 306**
- Real issue: `removeBarriers()` is commented out at line 292
- Also: `inlineNoDepEdts` only handles task/sync EDTs (line 338-339), not parallel

**IMM-2: Re-enable barrier removal**
- File: `lib/arts/passes/opt/edt/EdtStructuralOpt.cpp:292`
- Current: `/// removeBarriers();` is commented out
- Fix: Uncomment, ensure EdtGraph is available when `runAnalysis=true`

**IMM-3: Verify CASE 2 fix**
- Per memory notes: CASE 2 removed in commit 3110377
- Verify `EdtRewriter.cpp` no longer has global coarse fallback

### Phase 1: Contract Infrastructure Fixes (FIX-2, FIX-3, FIX-4, FIX-6)

**FIX-3: Persist post-DB contract refinements (CRITICAL)**
- File: `lib/arts/analysis/db/DbAnalysis.cpp`
- After `refineContractWithFacts()` returns true, call `upsertLoweringContract()` to write back to IR
- ~20-30 lines of code

**FIX-2: Feed AcquireContractSummary to heuristics**
- File: `lib/arts/analysis/heuristics/PartitioningHeuristics.cpp`
- Change interface to accept `const AcquireContractSummary&` alongside raw facts
- ~60-80 lines

**FIX-4: Cache DbAnalysis& in DbPartitioning**
- File: `lib/arts/passes/opt/db/DbPartitioning.cpp`
- Avoid redundant analysis construction
- ~10 lines

**FIX-6: Standardize fallback ordering**
- File: `lib/arts/passes/opt/db/DbPartitioning.cpp:1449,1793,1898`
- Unify to: summary > contract > node > facts > raw attrs
- ~30-50 lines

### Phase 2: New Contract Fields

Extend `LoweringContractInfo` in `include/arts/utils/LoweringContractUtils.h`:

```cpp
struct LoweringContractInfo {
  // ... existing fields ...

  // NEW: Dimension-aware stencil analysis (EXT-PART-1)
  SmallVector<int64_t, 4> stencilIndependentDims;

  // NEW: ESD annotation (DT-3)
  std::optional<int64_t> esdByteOffset;
  std::optional<int64_t> esdByteSize;

  // NEW: Cached block window (DT-7)
  std::optional<int64_t> cachedStartBlock;
  std::optional<int64_t> cachedBlockCount;

  // NEW: Post-DB refinement marker (DT-1)
  bool postDbRefined = false;

  // NEW: Inferred DB type for runtime unlock (EXT-DB-1)
  std::optional<int64_t> inferredDbMode;  // maps to ARTS_DB_PIN/ONCE/etc.

  // NEW: Task cost estimate (ET-1)
  std::optional<int64_t> estimatedTaskCost;

  // NEW: Critical path distance (ET-6)
  std::optional<int64_t> criticalPathDistance;
};
```

Also update:
- `LoweringContractOp` in `include/arts/Ops.td` (add optional attrs)
- `upsertLoweringContract()` in `lib/arts/utils/LoweringContractUtils.cpp` (marshal new fields)
- `getLoweringContract()` (extract new fields)
- `empty()` method (check new fields)

### Phase 3: DbTransformsPass (NEW PASS)

Create new pass shell at `lib/arts/passes/opt/db/DbTransformsPass.cpp`:

```cpp
std::unique_ptr<Pass> createDbTransformsPass(AnalysisManager *AM);
```

Individual transforms (in `lib/arts/transforms/db/transforms/`):

| ID | File | What |
|----|------|------|
| DT-1 | ContractPersistence.cpp | Walk DbAcquireOps, read AcquireContractSummary, write back via upsertLoweringContract |
| DT-2 | StencilHaloConsolidation.cpp | Unify halo bounds from 3 sources into contract minOffsets/maxOffsets |
| DT-3 | ESDAnnotation.cpp | Analyze EDT access footprint, annotate esdByteOffset/esdByteSize |
| DT-4 | GUIDRangeAllocation.cpp | Detect N-GUID loops, mark for arts_guid_reserve_range(N) |
| DT-7 | BlockWindowCaching.cpp | Cache block window in contract |

### Phase 4: EdtTransformsPass (NEW PASS)

Create new pass shell at `lib/arts/passes/opt/edt/EdtTransformsPass.cpp`:

```cpp
std::unique_ptr<Pass> createEdtTransformsPass(AnalysisManager *AM);
```

Individual transforms (in `lib/arts/transforms/edt/transforms/`):

| ID | File | What |
|----|------|------|
| ET-1 | TaskGranularityControl.cpp | Use EdtInfo computeToMemRatio/totalOps for cost estimation |
| ET-2 | DataAffinityPlacement.cpp | Set EDT route to owner node of primary write-target DB |
| ET-6 | CriticalPathAnalysis.cpp | Topological sort of EDT DAG, annotate critical_path_distance |
| EXT-EDT-2 | DeadDependencyElimination.cpp | Remove unused dependency slots |

### Phase 5: Partition Quality Extensions

Extend existing passes (no new pass shells):

| ID | File | What |
|----|------|------|
| EXT-PART-1 | PartitionBoundsAnalyzer.cpp + PartitioningHeuristics.cpp | Per-dim stencil analysis using isLeadingDynamic |
| EXT-PART-2 | PartitionBoundsAnalyzer.cpp | Check indirect access on non-partitioned dims |
| EXT-PART-3 | DbBlockPlanResolver.cpp | Wire StencilBounds to block size decisions |
| EXT-PART-4 | DbBlockPlanResolver.cpp:198-284 | Per-dim independent merge instead of global MIN |
| EXT-PART-5 | DbPartitioning.cpp | Write blockShape/ownerDims back to contract |
| EXT-DB-1 | Db.cpp | Infer ARTS_DB_PIN/ONCE/READ from access analysis |

### Phase 6: Advanced Optimizations

| ID | What | Inspiration |
|----|------|-------------|
| DT-5 | DB replication for read-heavy DBs | artsAddDbDuplicate() |
| DT-6 | Atomic reduction recognition | artsAtomicAddInArrayDb() |
| ET-3 | Dependency chain narrowing for wavefronts | arts_add_dependence_at for halo-only |
| ET-4 | Comm/comp overlap (split stencil EDTs) | Interior + boundary EDT splitting |
| ET-5 | Reduction strategy selection | local/tree/remote atomic based on scale |
| NEW-1 | Early dependency release | OmpSs-2 inspired |
| NEW-2 | Transitive dependency reduction | MLIR-AIR inspired |

### Phase 7: Polish & Validation

| ID | What |
|----|------|
| EXT-EPOCH-1 | Epoch scope narrowing |
| EXT-DIST-1 | Allow read-only stencil distributed DBs |
| EXT-DIST-2 | Writer-aware route selection |
| VOCAB | Rename stencil_* attrs to general names |
| VALID | Contract validation pass (debug builds) |
| FIX-5 | Cache DbAnalysis in LoopAnalysis |

---

## 5. MLIR Async Dialect Decision

**VERDICT: DO NOT USE.** The ARTS EDT model is strictly more powerful for CARTS's HPC use case.

| Aspect | MLIR Async | ARTS EDT |
|--------|-----------|----------|
| Memory model | Write-once values | Mutable shared DataBlocks |
| Partitioning | None | 5 modes |
| Distribution | None | Cross-node frontier coherence |
| Iterative | Cannot express | Natural via epochs |

**Ideas to adapt (not adopt):**
1. Transitive reduction from MLIR-AIR for EDT DAG pruning (NEW-2)
2. Coroutine-style suspension for non-blocking remote DB fetches (future)
3. Static reference counting for GUID lifetime (future)

---

## 6. Competitive Positioning

| System | Their Strength | CARTS Advantage | Gap to Close |
|--------|---------------|-----------------|-------------|
| PaRSEC | O(1) parametric deps via JDF | Works from OpenMP, not DSL | Parametric dep emission (future) |
| OmpSs-2 | Early dep release, taskiter | Full MLIR stack + polyhedral | Early release (NEW-1), graph reuse (future) |
| Legion/Diffuse | Cross-task fusion (1.86x) | Single compilation flow | EDT fusion (future) |
| StarPU | Cost-model scheduling | Complete compiler | Cost hints via ET-1 |

---

## 7. File Organization for New Code

```
include/arts/transforms/db/transforms/     # NEW: individual DB transform headers
  ContractPersistence.h
  StencilHaloConsolidation.h
  ESDAnnotation.h
  GUIDRangeAllocation.h
  BlockWindowCaching.h

lib/arts/transforms/db/transforms/          # NEW: individual DB transform impls
  ContractPersistence.cpp
  StencilHaloConsolidation.cpp
  ESDAnnotation.cpp
  GUIDRangeAllocation.cpp
  BlockWindowCaching.cpp

include/arts/transforms/edt/transforms/    # NEW: individual EDT transform headers
  TaskGranularityControl.h
  DataAffinityPlacement.h
  CriticalPathAnalysis.h
  DeadDependencyElimination.h

lib/arts/transforms/edt/transforms/         # NEW: individual EDT transform impls
  TaskGranularityControl.cpp
  DataAffinityPlacement.cpp
  CriticalPathAnalysis.cpp
  DeadDependencyElimination.cpp

lib/arts/passes/opt/db/DbTransformsPass.cpp  # NEW: pass shell
lib/arts/passes/opt/edt/EdtTransformsPass.cpp # NEW: pass shell
```

---

## 8. Impact Summary (Top 10 by ROI)

| Rank | ID | Transform | Speedup | Effort |
|------|-----|-----------|---------|--------|
| 1 | IMM-2 | Re-enable barrier removal | 3-5% | Trivial |
| 2 | FIX-3 | Persist contract refinements | Unlocks all | Low |
| 3 | DT-4 | GUID range allocation | 10-50x alloc | Medium |
| 4 | DT-3 | ESD annotation | 1.5-3x stencils | Medium |
| 5 | DT-6 | Atomic reductions | 2-10x reductions | Medium |
| 6 | EXT-DB-1 | DB mode inference | 1.2-2x general | Medium |
| 7 | EXT-PART-4 | Per-dim block merge | 5-15% block | Low |
| 8 | ET-2 | Data affinity placement | 5-30% NUMA | Medium |
| 9 | ET-1 | Task granularity control | 10-50% EDT | Medium |
| 10 | ET-6 | Critical path analysis | 5-20% sched | Medium |
