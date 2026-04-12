# Cost Model and Heuristic Architecture

## Current State: The Heuristic Maze

CARTS currently uses **22 named rules** in `PartitioningHeuristics.cpp` (H1.C0-H1.F)
plus **12 distribution thresholds**, all with hardcoded constants. Passes frequently
undo each other's work, with entire pipeline stages existing solely to fix damage
from earlier stages.

### Passes That Undo Each Other (8 Confirmed Cases)

| Pass A (creates) | Pass B (undoes/fixes) | Root Cause |
|---|---|---|
| `EdtDistribution` distributes loops | `PostDistributionCleanup` re-runs `EdtStructuralOpt` twice + `EpochOpt` | Distribution creates degenerate EDTs that structural opt must remove |
| `DbPartitioning` assigns block/element-wise | `PostDbRefinement` re-runs `DbModeTightening` + `EdtTransforms` + `DbTransforms` | Partitioning changes access patterns that need re-tightening |
| `ForLowering` lowers arts.for loops | `PostDistributionCleanup` structural cleanup | ForLowering creates empty/degenerate EDT bodies |
| `DbModeTightening` (stage 8) | `DbModeTightening` (stage 14) | Must re-tighten after partitioning changes access patterns |
| `EdtStructuralOpt` (stages 6, 9, 12x2) | Itself at each later stage | Each structural change enables another round of cleanup |
| `EdtAllocaSinking` (stage 15) | `EdtAllocaSinking` (stage 17) | Must re-sink after pre-lowering introduces new allocas |
| `DataPtrHoisting` (stage 17) | `DataPtrHoisting` (stage 18) | Must re-hoist after Arts-to-LLVM materializes new loads |
| `DbTransformsPass` (stage 13) | `DbTransformsPass` (stage 14) | EDT dep pruning creates new cleanup-only chains |

### Analysis Invalidation Inconsistency

`DbTransformsPass` declares it only invalidates `DbAnalysis`, but actually calls
`AM->getEdtAnalysis().invalidate()` at runtime. `EpochOpt` reads 1 analysis but
invalidates **6** (`DbAnalysis`, `EdtAnalysis`, `EpochAnalysis`, `LoopAnalysis`,
`DbHeuristics`, `EdtHeuristics`).

---

## Hardcoded Thresholds (21 across 8 files)

| Name | Value | File | What It Decides | What It Ignores |
|---|---|---|---|---|
| Tiny stencil table | <=8 elements | PartitioningHeuristics:29 | Force coarse for small stencil coefficients | Cache locality, compute intensity |
| `kMaxElements` | 65,536 | PartitioningHeuristics:207 | Preserve block for small vectors | Memory bandwidth, dep chain length |
| `kMaxBlocks` | 64 | PartitioningHeuristics:208 | Block fan-in limit | Allocation overhead vs parallelism |
| `kPreferredMinCellsPerTask` | 32,768 | DistributionHeuristics:153 | Wavefront task granularity | Kernel flop/byte ratio, task spawn cost |
| `kAbsoluteMinCellsPerTask` | 8,192 | DistributionHeuristics:173 | Hard floor for wavefront tiles | Load imbalance, network latency |
| `kPreferredMaxTilesPerWorker` | 10 | DistributionHeuristics:197 | Ready-tile budget | System saturation, dep graph branching |
| `kMinOwnedOuterItersFloor` | 8 | DistributionHeuristics:228 | Stencil strip minimum | Halo exchange cost, L3 cache size |
| `kAmortizationPressureStride` | 8 | DistributionHeuristics:229 | Stencil coarsening step | Convergence pattern |
| `kOwnedSpanStridePenalty` | 2 | DistributionHeuristics:230 | Owned strip penalty multiplier | Whether 2x is always right |
| `kMaxAmortizationMultiplier` | 4 | DistributionHeuristics:231 | Coarsening cap | Bandwidth saturation, spawn latency |
| `kStencilIterationWorkEstimateCap` | 2M | DistributionHeuristics:384 | Upper stencil work bound | Memory hierarchy |
| `minItersPerWorker` | 8 (base) | DistributionHeuristics:409 | Loop coarsening floor | Scheduler overhead, false sharing |
| Persistent region overhead gate | >0.3 | PersistentRegionCostModel:54 | Region worthiness | Target system spawner latency |
| Persistent region benefit gate | >0.2 | PersistentRegionCostModel:55 | Amortization threshold | Convergence speed |
| Persistent region stability gate | >0.4 | PersistentRegionCostModel:56 | Slice independence | Dependency graph structure |
| `kSmallTaskThreshold` | 64 | EdtTransformsPass:98 | Warn on tiny EDTs | Spawn overhead on target |
| `kLoopDepthMultiplier` | 8 | EdtTransformsPass:102 | Loop nesting cost factor | Cache blocking, vectorization |
| `kAtomicWorkerThreshold` | 16 | EdtTransformsPass:107 | Tree reduction trigger | Cache ping-pong, contention |
| `kMaxOuterDBs` | 1,024 | DbHeuristics.h | Partition count cap | Worker count, allocation overhead |
| `kMaxDepsPerEDT` | 8 | DbHeuristics.h | EDT fanout limit | Actual dep resolution cost |
| `kMinInnerBytes` | 64 | DbHeuristics.h | Minimum chunk size | Cache line size, alignment |

---

## Proposed: Unified Cost Model

Replace the 22 ad-hoc heuristic rules with a cost function backed by a
calibratable runtime cost table.

### ArtsRuntimeCostTable

```cpp
struct ArtsRuntimeCostTable {
  // Calibrated from ARTS runtime measurements (or conservative defaults)
  double edtCreationCycles = 2000;       // arts_edt_create + queue
  double dbAcquireCycles = 500;          // arts_db_acquire (local)
  double dbReleaseCycles = 200;          // arts_db_release
  double epochCreateCycles = 1500;       // arts_epoch_create
  double epochWaitCycles = 3000;         // arts_wait_on_handle (blocking!)
  double finishContinuationCycles = 200; // async continuation (15x cheaper)
  double cdagFrontierCycles = 100;       // per-frontier progression
  double remoteDbAcquireCycles = 5000;   // remote node acquire
  double haloExchangePerByte = 0.5;      // ESD byte-slice transfer

  // DERIVED thresholds (not hardcoded -- computed from above)
  int64_t maxOuterDBs() const {
    return static_cast<int64_t>(1e9 / (dbAcquireCycles + dbReleaseCycles));
  }
  int64_t minUsefulEdtWorkCycles() const {
    return static_cast<int64_t>(edtCreationCycles * 10); // 10x overhead rule
  }
};
```

### Cost Equations

Each domain queries the cost model:

**Compute**: "Is it cheaper to fuse these two loops (fewer EDTs) or keep them
separate (more parallelism)?"
```
edtCreation * savedEDTs  vs  parallelismLoss
```

**Memory**: "Is block partitioning cheaper than coarse for this array?"
```
blockCost  = numBlocks * (dbAlloc + dbAcquire) + haloBytes * haloExchangeRate
coarseCost = 1 * dbAlloc + frontierSerializationCycles * writerCount
```

**Sync**: "Is CPS chain cheaper than blocking epoch wait?"
```
cpsChainCost  = iterations * continuationEdtCost
blockingCost  = iterations * epochWaitCycles
```

### Current vs Proposed Decision Flow

```
CURRENT (H1 cascade):
  if (stencil && singleNode && readOnly && !canBlock) -> coarse
  else if (canBlock && hasStencilReads && ...) -> block
  else if ... (22 pattern-specific if-else rules)

PROPOSED (cost model):
  candidates = [coarse, block, elementWise, stencil]
  for each candidate:
    cost = costModel.evaluate(candidate, context)
  return argmin(costs)
```

### Calibration Strategy

The cost table can be populated three ways:

1. **Conservative defaults** (what CARTS does today, implicitly)
2. **Machine calibration** at install time (`dekk carts install` runs microbenchmarks)
3. **Profile-guided** feedback from benchmark runs

### HeuristicOutcome Tracking

```cpp
struct HeuristicOutcome {
  StringRef heuristicId;     // e.g., "H1.B2"
  StringRef strategyChosen;  // e.g., "block"
  double predictedCost;      // from cost model
  double actualCost;         // measured post-lowering or from benchmark
  bool requiredFallback;     // true if downstream pass overrode
  StringRef fallbackReason;
};
```

Over compilations, this data adjusts cost model coefficients and heuristic priorities.

---

## SDE Information Fragmentation (7 Critical Loss Points)

These are the information boundaries where the current pipeline loses data,
forcing downstream passes to reconstruct from scratch:

| # | What's Lost | Where | Consequence |
|---|---|---|---|
| 1 | Graph dependencies | Only in analysis cache, not IR | Any IR mutation forces full rebuild |
| 2 | DB partition facts | `DbAcquirePartitionFacts` computed in DbGraph but not on IR | DbPartitioning reconstructs from scratch |
| 3 | Loop access patterns | Computed 3x by CollectMetadata, EdtAnalysis, DbAnalysis | Redundant O(loops * arrays) walks |
| 4 | CPS chain state | Legacy attrs (CPSParamPerm etc.) coexist with new StateSchema/DepSchema | Dual codepath in EpochLowering |
| 5 | Distribution decisions | Stored as ephemeral attributes only | ForLowering must query AM to recover |
| 6 | Analysis invalidation | Monolithic `invalidate()` destroys everything | 218 module-level walks, 35% redundant |
| 7 | State transitions | ForLowering splits EDTs without materializing new state | Downstream can't reason about equivalence |

The SDE contract architecture addresses this by making all three aspects
(State, Dependencies, Effects) explicit in the IR, not just in analysis caches.

---

## Domain-Local Convergence Framework

Instead of 18 rigid pipeline stages with repeated passes, use bounded
domain-local convergence with cross-domain contracts:

```
Phase A: Compute Domain (openmp-to-arts through edt-opt)
  repeat up to K=2:
    1. Discover/refine patterns
    2. Apply compute transforms (reorder, reshape, fuse)
    3. Check: did anything change? If not, exit.
  Output: FROZEN compute contract

Phase B: Memory Domain (create-dbs through db-partitioning)
  repeat up to K=2:
    1. Create/refine DB layout from frozen compute contract
    2. Optimize modes, partition DBs
    3. Check: did anything change? If not, exit.
  Output: FROZEN memory contract

Phase C: Sync Domain (epochs through pre-lowering)
  repeat up to K=2:
    1. Create epochs from frozen compute + memory contracts
    2. Optimize structure (fusion, CPS, continuation)
    3. Check: did anything change? If not, exit.
  Output: FROZEN sync contract
```

**Convergence guarantee**: Each domain's loop converges because:
- Compute transforms are in canonical order (discover -> normalize -> reorder -> refine)
- Memory decisions use monotonic lattice operations (mode tightening only narrows)
- Sync decisions depend on frozen compute and memory contracts
- K=2 suffices: first pass makes primary decision, second cleans up secondary effects

**Expected improvements**:
- Pipeline stages: 18 rigid -> 6 logical phases with 3 convergence loops
- Pass executions per compilation: ~80 -> ~40 (fewer repeated passes)
- Heuristic rules: 22 pattern-specific -> 1 cost function
- Hardcoded thresholds: 21 -> 0 (all derived from cost table)

---

## Analysis Dependency Graph

```
DbAnalysis (1750 LOC)
  Readers: DbPartitioning, DbModeTightening, CreateDbs,
           EdtDistribution, ForLowering, Concurrency
  Invalidators: DbPartitioning, DbModeTightening, EdtDistribution

EdtAnalysis (572 LOC)
  Readers: EdtStructuralOpt, EdtTransformsPass, EpochOpt, Concurrency
  Invalidators: EpochOpt, DbTransformsPass (undeclared!)

LoopAnalysis (241 LOC)
  Readers: DbPartitioning, LoopReordering, DbModeTightening, ForLowering
  Invalidators: None

MetadataManager (278 LOC)
  Readers: LoopReordering, ConvertOpenMPToArts, CollectMetadata, ForLowering
  Invalidators: None (preserved across stages)

AbstractMachine
  Readers: DistributionHeuristics, Concurrency, EdtDistribution, ForLowering
  Immutable (set at pipeline start)

DbHeuristics (227 LOC) -> Readers: DbPartitioning
DistributionHeuristics (588 LOC) -> Readers: ForLowering
PartitioningHeuristics (743 LOC) -> Readers: DbPartitioning
EpochHeuristics (850 LOC) -> Readers: EpochOpt
```

### Cross-Cutting Pass Dependencies

**Passes touching BOTH Compute and Memory:**
- `ForLowering` -- creates EDTs + rewires DB acquires
- `EdtDistribution` -- distribution based on DB access patterns
- `Concurrency` -- EDT/DB affinity + DB mode adjustment
- `EdtTransformsPass` -- dep narrowing analyzes DB usage
- `DbPartitioning` -- analyzes EDT consumers of each DB
- `ConvertOpenMPToArts` -- OMP depend -> DB control ops
- `EdtLowering` -- records EDT deps on DB GUIDs

**Passes touching BOTH Compute and Sync:**
- `CreateEpochs` -- groups EDTs into epochs
- `EpochOpt` -- restructures epochs; marks EDT continuation
- `ForLowering` -- wraps EDT loops in epoch structure
- `ParallelEdtLowering` -- parallel EDT -> epoch

**Passes touching BOTH Memory and Sync:**
- `EpochOptCpsChain` -- CPS chain emits DB scratch via dep routing
- `DbScratchElimination` -- eliminates epoch-local scratch DBs

---

## ARTS Runtime Awareness Gaps

The current branch no longer has zero cost modeling for runtime behavior.
There is now an SDE-facing `SDECostModel` interface plus an `ARTSCostModel`
implementation, and SDE passes already use that interface for scope, schedule,
chunk, reduction-strategy, and tensor/linalg decisions. The gap that remains is
that many core ARTS heuristics still rely on hardcoded thresholds instead of
consuming that same interface consistently.

What is still missing in the broader ARTS/core heuristic layer:

| Decision | Compiler Status | What ARTS Actually Does |
|---|---|---|
| EDT creation cost | Partially modeled in SDE-facing heuristics, but many core thresholds still hardcoded | ~2000 cycles per EDT + queue insert |
| DB acquire cost | Not modeled consistently across core heuristics | ~500 cycles local, ~5000 cycles remote |
| Epoch creation | Not modeled | ~1500 cycles + TD counter setup |
| `arts_wait_on_handle` | Treated as free | Blocking wait -- suspends worker thread |
| Frontier progression | Not modeled | ~100 cycles per frontier advance |
| `arts_add_dependence` | Treated as free | Hash table insert + potential remote msg |

The **frontier-based concurrency model** is load-bearing:
- Each DB maintains a linked-list of frontiers (phases) grouping compatible consumers
- Multiple reads share a frontier; writes get serialized through frontier progression
- Different DB GUIDs can be acquired concurrently (frontier-independent)
- Partitioning breaks a single write target into disjoint GUIDs -> eliminates serialization

**Three-phase Termination Detection** per epoch:
1. PHASE_1: Initial quiescence check
2. PHASE_2: Counter stabilization (packed `active(hi32)|finished(lo32)`)
3. PHASE_3: Termination confirmed

Each phase involves atomic operations; multi-node adds global reduction.
