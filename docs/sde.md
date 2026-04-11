# SDE as Optimization Layer: CostModel + RaiseToLinalg + Pattern Contracts

## Context

CARTS had a layering problem: optimization decisions (tiling, buffer strategy,
reduction strategy, distribution) were made deep inside ARTS-specific passes
using 21 hardcoded thresholds. This branch moves the first part of that work
up into SDE: OpenMP now lowers into SDE, SDE classifies loop structure, and
SDE->ARTS stamps early contracts before the rest of the ARTS pipeline runs.

**Key insight**: SDE CAN know about computation patterns (stencil, matmul,
reduction) — these are mathematical structures, not runtime-specific. SDE
defines its own **`SDECostModel` interface** using SDE-level concepts (tasks,
barriers, data movement). `ARTSCostModel` is one implementation that maps
SDE concepts to ARTS primitives (EDT→task, DB acquire→data access, epoch→sync)
and whose costs **change based on topology** (local: halo 0.01/byte vs
distributed: halo 0.5/byte — 50x difference). This lets SDE do ARTS-quality
optimizations without importing ARTS types, and future runtimes (GPU, Legion)
can provide their own cost tables.

**Three-layer abstraction**:
```
sde.cu_region / sde.cu_task  =  PARALLELISM  (what runs in parallel)
sde.su_iterate               =  SCHEDULING   (how iterations are distributed)
linalg.generic               =  COMPUTATION  (what the math is)
```

**Current state on this branch**:
1. `RaiseToLinalg` now walks `sde.su_iterate`, stamps structural classification,
   and materializes transient `linalg.generic` carriers for the supported subset.
2. `RaiseToTensor` now rewrites the supported transient carriers into
   tensor-backed `linalg.generic` form inside SDE, keeping the original
   loop/memref body authoritative and ARTS-unaware.
3. `SdeTensorOptimization` now performs the first real tensor-driven SDE
   transformation: eligible one-dimensional elementwise loops are strip-mined
   into cost-model-sized tiles, and the surrounding `sde.su_iterate` is
   rewritten so the tiled shape survives into `arts.for`.
4. `PatternDiscovery` has been removed from executable pipeline paths; early
   contracts are seeded during SDE->ARTS conversion and refined later by
   `DepTransforms` / `KernelTransforms`.
5. `SDECostModel` and `ARTSCostModel` exist and are wired through
   `AnalysisManager`. `SdeScopeSelection`, `SdeChunkOptimization`, and
   `SdeReductionStrategy` now use that interface in limited form to stamp
   parallel region scope, synthesize missing chunk sizes, and annotate
   eligible reduction-bearing `sde.su_iterate` ops with a strategy
   recommendation.
6. The stabilized carrier design keeps the original loop/memref IR in place;
   transient `linalg.generic`, `bufferization.to_tensor`, and `tensor.empty`
   are used only for contract recovery and are erased during SDE->ARTS
   conversion.
7. Task-dependency ingestion still bridges from upstream `arts.omp_dep`
   carriers created before SDE. Removing that residual ARTS-core boundary needs
   a separate upstream dependency-carrier redesign.
8. Reduction and stencil raising are not fully implemented. Those cases stay on
   the fallback path today, but `RaiseToLinalg` now stamps an SDE-owned
   classification directly on `sde.su_iterate` so SDE->ARTS can still recover
   the intended contract without relying on ARTS-namespaced strings.
9. SDE chunk logic is validated at the SDE IR level: tests check
   `arts_sde.su_iterate schedule(..., chunk)` immediately after
   `SdeChunkOptimization`, before SDE->ARTS lowering rewrites the loop.
10. SDE scope logic is validated at the SDE IR level: tests check
   `arts_sde.cu_region <parallel> scope(<local|distributed>)` immediately
   after `SdeScopeSelection`, before SDE->ARTS lowering rewrites the region.
11. The first tensor path is validated at the SDE IR level: tests check the
    memref-backed carrier after `RaiseToLinalg`, the tensor-backed carrier
    after `RaiseToTensor`, the tiled `sde.su_iterate` after
    `SdeTensorOptimization`, and a clean carrier-free `arts.for` body after
    `ConvertSdeToArts`.

---

## Pipeline Change

```
CURRENT IMPLEMENTATION:
 buildOpenMPToArtsPipeline:
    ConvertOpenMPToSde
      → SdeScopeSelection
      → SdeChunkOptimization
      → SdeReductionStrategy
      → RaiseToLinalg (on sde.su_iterate)
      → RaiseToTensor (tensor-backed transient carrier rewrite)
      → SdeTensorOptimization (cost-model-driven tensor tiling)
      → ConvertSdeToArts
      → VerifySdeLowered
      → DCE
      → CSE
      → VerifyEdtCreated

  buildPatternPipeline:
    DepTransforms
      → LoopNormalization
      → StencilBoundaryPeeling
      → LoopReordering
      → KernelTransforms
      → CSE
```

---

## Part 1: SDECostModel Interface (SDE-Owned, Runtime-Agnostic)

The cost model uses **SDE-level concepts** (tasks, barriers, data movement,
reductions) — NOT ARTS concepts (EDTs, DBs, epochs). This is the novelty:
SDE optimizes for ANY runtime by programming against an abstract interface.

### Step 1A: Define SDECostModel Interface

**Create**: `include/arts/utils/costs/SDECostModel.h`

Lives in `utils/` (shared layer) — accessible from both SDE and core without
layering violation. SDE passes import this, never `AnalysisManager`.

```cpp
namespace mlir::arts::sde {

/// Runtime-agnostic cost model for SDE optimization decisions.
/// Every target runtime (ARTS, Legion, StarPU, GPU) provides a concrete
/// implementation. SDE passes see ONLY this interface.
class SDECostModel {
public:
  virtual ~SDECostModel() = default;

  // --- Task lifecycle costs (normalized cycles) ---
  virtual double getTaskCreationCost() const = 0;
  virtual double getTaskSyncCost() const = 0;       // barrier / join

  // --- Reduction costs ---
  virtual double getReductionCost(int64_t workerCount) const = 0;
  virtual double getAtomicUpdateCost() const = 0;

  // --- Data movement costs ---
  virtual double getLocalDataAccessCost() const = 0;
  virtual double getRemoteDataAccessCost() const = 0;
  virtual double getHaloExchangeCostPerByte() const = 0;
  virtual double getAllocationCost() const = 0;

  // --- Scheduling costs ---
  virtual double getSchedulingOverhead(SdeScheduleKind kind,
                                       int64_t tripCount) const = 0;

  // --- Machine topology (abstract) ---
  virtual int getWorkerCount() const = 0;
  virtual int getNodeCount() const = 0;
  virtual bool isDistributed() const { return getNodeCount() > 1; }

  // --- Derived thresholds (computed, not hardcoded) ---
  virtual int64_t getMinUsefulTaskWork() const {
    return static_cast<int64_t>(getTaskCreationCost() * 10);
  }
  virtual int64_t getMinIterationsPerWorker() const {
    return std::max<int64_t>(1,
        static_cast<int64_t>(getTaskCreationCost() /
                             (getLocalDataAccessCost() + 1.0)));
  }
};

} // namespace mlir::arts::sde
```

**Key design**: Every method uses SDE concepts (tasks, barriers, data access).
No EDT, DB, epoch, CDAG, or GUID terminology. A `GPUCostModel` or
`LegionCostModel` could implement the same interface with different costs.

### Step 1B: ARTSCostModel — Topology-Aware Implementation

**Create**: `include/arts/utils/costs/ARTSCostModel.h`

ARTSCostModel provides ARTS-specific costs that **change based on
AbstractMachine topology** (local vs distributed):

```cpp
namespace mlir::arts {

class ARTSCostModel : public sde::SDECostModel {
  const AbstractMachine &machine;
public:
  explicit ARTSCostModel(const AbstractMachine &am) : machine(am) {}

  // --- Task lifecycle (maps SDE tasks → ARTS EDTs) ---
  double getTaskCreationCost() const override {
    // Distributed: EDT may trigger remote spawn message
    return machine.isDistributed() ? 2500.0 : 1800.0;
  }
  double getTaskSyncCost() const override {
    // Distributed: global epoch reduction across nodes
    // Local: atomic counter in shared memory
    return machine.isDistributed() ? 5000.0 : 3000.0;
  }

  // --- Reduction (maps SDE reductions → ARTS atomics/trees) ---
  double getReductionCost(int64_t workerCount) const override {
    // Tree: log2(N) barriers. Linear: N atomics.
    double treeCost = std::log2(workerCount) * getTaskSyncCost();
    double linearCost = workerCount * getAtomicUpdateCost();
    return std::min(treeCost, linearCost);
  }
  double getAtomicUpdateCost() const override {
    return machine.isDistributed() ? 500.0 : 100.0;
  }

  // --- Data movement (maps SDE data access → ARTS DB acquire) ---
  double getLocalDataAccessCost() const override {
    return 500.0;  // arts_db_acquire (local, zero-copy)
  }
  double getRemoteDataAccessCost() const override {
    return 5000.0;  // arts_db_acquire (remote, network RTT)
  }
  double getHaloExchangeCostPerByte() const override {
    // Local: memory copy (~0.01 cycles/byte)
    // Distributed: network transfer (~0.5 cycles/byte)
    return machine.isDistributed() ? 0.5 : 0.01;
  }
  double getAllocationCost() const override {
    return machine.isDistributed() ? 2000.0 : 1500.0;
  }

  // --- Scheduling ---
  double getSchedulingOverhead(sde::SdeScheduleKind kind,
                               int64_t tripCount) const override {
    // Dynamic/guided: per-steal overhead
    // Static: zero runtime overhead
    switch (kind) {
    case sde::SdeScheduleKind::static_: return 0.0;
    case sde::SdeScheduleKind::dynamic: return getTaskCreationCost() * 0.1;
    case sde::SdeScheduleKind::guided:  return getTaskCreationCost() * 0.05;
    default: return 0.0;
    }
  }

  // --- Topology ---
  int getWorkerCount() const override {
    return machine.getRuntimeTotalWorkers();
  }
  int getNodeCount() const override {
    return machine.getNodeCount();
  }
};

} // namespace mlir::arts
```

**Critical topology awareness**: Halo exchange cost is 50x different between
local (0.01) and distributed (0.5). Task sync cost is 1.7x different.
These differences drive fundamentally different optimization decisions.

### Step 1C: Inject Through AnalysisManager

**Modify**: `include/arts/dialect/core/Analysis/AnalysisManager.h`

```cpp
#include "arts/utils/costs/SDECostModel.h"
// ...
std::unique_ptr<sde::SDECostModel> costModel;

// Public accessor (SDE passes receive this, never AnalysisManager):
sde::SDECostModel &getCostModel() { return *costModel; }
const sde::SDECostModel &getCostModel() const { return *costModel; }
```

Constructor creates `ARTSCostModel(abstractMachine)` by default.

### Step 1D: Fix SDE Layering Violations

`ConvertOpenMPToSde.cpp` no longer depends on `AnalysisManager`. It now
receives `SDECostModel *` directly so SDE passes do not depend on ARTS analysis
plumbing for cost access.
- Remove `#include "arts/dialect/core/Analysis/AnalysisManager.h"`
- Change constructor param from `AnalysisManager*` to `SDECostModel*`
- SDE passes now receive `SDECostModel*`, never see `AnalysisManager`

Residual layering note: task dependencies still arrive as `arts.omp_dep`
because upstream preprocessing (`RaiseMemRefDimensionality` / `HandleDeps`)
materializes that carrier before SDE conversion. `ConvertOpenMPToSde` bridges
from that input today, so this boundary is reduced but not fully eliminated.

**Compile.cpp** injection:
```cpp
auto &costModel = AM->getCostModel();
pm.addPass(sde::createConvertOpenMPToSdePass(&costModel));
pm.addPass(sde::createSdeScopeSelectionPass(&costModel));
pm.addPass(sde::createSdeChunkOptimizationPass(&costModel));
pm.addPass(sde::createSdeReductionStrategyPass(&costModel));
pm.addPass(sde::createRaiseToLinalgPass());
pm.addPass(sde::createConvertSdeToArtsPass());
```

### Step 1E: Limited Cost-Driven SDE Scope Selection

`SdeScopeSelection` is now implemented in a deliberately narrow form and is
wired into `buildOpenMPToArtsPipeline` immediately after `ConvertOpenMPToSde`.
Its current contract is:

- It only rewrites `sde.cu_region <parallel>`
- It uses the active `SDECostModel` topology directly
- Single-node builds become `scope(<local>)`
- Multi-node builds become `scope(<distributed>)`

This moves parallel region scope out of the hardcoded OMP->SDE conversion path
and validates the decision directly on SDE IR.

### Step 1F: Limited Cost-Driven SDE Chunk Optimization

`SdeChunkOptimization` is now implemented in a deliberately narrow form and is
wired into `buildOpenMPToArtsPipeline` immediately after `ConvertOpenMPToSde`.
Its current contract is:

- It only rewrites `sde.su_iterate`
- It preserves explicit source chunk sizes
- It only synthesizes chunks for one-dimensional `dynamic` / `guided` loops
  with constant trip counts
- It computes a chunk from `getWorkerCount()` and
  `getMinIterationsPerWorker()`, then materializes the result as an SDE-level
  `chunkSize` operand before SDE->ARTS conversion

This keeps chunk logic validated at the SDE layer rather than inferring it back
from lowered `arts.for` IR.

### Step 1G: CMake

**Create**: `lib/arts/utils/costs/CMakeLists.txt` (GLOB pattern)
**Modify**: `lib/arts/utils/CMakeLists.txt` — add subdirectory

---

## Part 2: RaiseToLinalg Transformation

### Step 2A: Fix Layering — Switch to `sde::SdeSuIterateOp`

**File**: `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp`

Replace all `arts::ForOp` references with `sde::SdeSuIterateOp`:
- `LoopNestInfo::rootForOp` → `sde::SdeSuIterateOp rootIterOp`
- `collectPerfectNest(arts::ForOp)` → `collectPerfectNest(sde::SdeSuIterateOp)`
- `module.walk([&](arts::ForOp ...)` → `module.walk([&](sde::SdeSuIterateOp ...))`

Remove `#include "arts/passes/Passes.h"` and all ARTS core includes.

`sde.su_iterate` structure (SdeOps.td:193-229):
- `Variadic<Index>` lowerBounds/upperBounds/steps (multi-dim)
- Single-block `$body` region, block args are IVs
- Optional: `schedule`, `chunkSize`, `nowait`, `reductionAccumulators`

### Step 2B: Add linalg Dependencies

**Modify** `include/arts/dialect/sde/Transforms/Passes.td`:
- Remove `ArtsDialect` from RaiseToLinalg dependentDialects
- Add `LinalgDialect`

**Modify** `include/arts/dialect/sde/Transforms/Passes.h`:
- Add `#include "mlir/Dialect/Linalg/IR/Linalg.h"`

**Modify** `lib/arts/passes/CMakeLists.txt`:
- Add `MLIRLinalgDialect`, `MLIRLinalgTransforms` to LINK_LIBS

### Step 2C: Create Transient `linalg.generic` Carriers

**File**: `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp`

After existing analysis (collect nest, build indexing maps, classify):

1. **Operand collection**: Each `memref.load` → `ins`, each `memref.store` → `outs`.
   Stencils: each distinct `(memref, indexingMap)` pair = separate input.

2. **Body reconstruction**: Map `memref.load` results → block args, clone
   arithmetic, create `linalg.yield` with stored values.

3. **Op creation**: `builder.create<linalg::GenericOp>(loc, TypeRange{},
   inputMemrefs, outputMemrefs, allMaps, linalgIterTypes, bodyBuilder)`

4. **Keep original loop body in place**: insert the `linalg.generic` carrier
   next to the original loop/memref body instead of replacing it. SDE->ARTS
   consumes the carrier later and erases it after stamping contracts.

Best-effort: non-raiseable bodies (indirect indexing, control flow, non-affine)
stay as memref.load/store — downstream passes (DepTransforms, KernelTransforms,
DbAnalysis) handle them via independent IR analysis and graceful fallback.

### Step 2D: Reduction Raising

This is still future work. The current implementation bails out when
`sde.su_iterate` has `reductionAccumulators`, stamps only structural
classification on the source loop, and does not materialize a transient
`linalg.generic` carrier for reductions.

### Step 2E: Move Pass in Pipeline

**File**: `tools/compile/Compile.cpp`

Status: completed. `RaiseToLinalg` now runs between OMP→SDE and SDE→ARTS:
```cpp
auto &costModel = AM->getCostModel();
pm.addPass(arts::sde::createConvertOpenMPToSdePass(&costModel));
pm.addPass(arts::sde::createSdeScopeSelectionPass(&costModel));
pm.addPass(arts::sde::createSdeChunkOptimizationPass(&costModel));
pm.addPass(arts::sde::createSdeReductionStrategyPass(&costModel));
pm.addPass(arts::sde::createRaiseToLinalgPass());    // NEW
pm.addPass(arts::sde::createConvertSdeToArtsPass());
```

---

## Part 3: SDE→ARTS Contract Stamping + Transient Carrier Consumption

**File**: `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp`

In `SuIterateToArtsPattern::matchAndRewrite()`, after creating `arts.for`:

### Step 3A: Classify from linalg structure when a carrier is present

```cpp
static std::optional<ArtsDepPattern>
classifyFromLinalg(linalg::GenericOp generic) {
  auto iterTypes = generic.getIteratorTypesArray();
  bool allParallel = llvm::all_of(iterTypes, [](auto t) {
    return t == utils::IteratorType::parallel;
  });
  if (allParallel) {
    for (auto map : generic.getIndexingMapsArray())
      if (hasConstantOffsets(map)) return ArtsDepPattern::stencil_tiling_nd;
    return ArtsDepPattern::uniform;
  }
  unsigned nPar = 0, nRed = 0;
  for (auto t : iterTypes)
    t == utils::IteratorType::parallel ? ++nPar : ++nRed;
  if (nPar == 2 && nRed == 1 && iterTypes.size() == 3)
    return ArtsDepPattern::matmul;
  return std::nullopt;
}
```

Important current limitation: the stabilized path only relies on direct
`linalg.generic` classification for the currently supported subset. Generic
reductions are not converted into `ArtsDepPattern::reduction` here.

### Step 3B: Stamp contracts

- Walk the cloned `arts.for` body for transient `linalg::GenericOp`
- If found: prefer any explicit contract already present on the generic;
  otherwise classify from linalg structure, then stamp `depPattern` and
  stencil offsets via `StencilNDPatternContract`
- If no carrier is present: fall back to the SDE-owned
  `classification(<...>)` stamped on the source `sde.su_iterate`
- After stamping: erase the transient cloned generics

This is intentionally **not** `linalgOpToLoops`. `RaiseToLinalg` leaves the
original loop/memref body in place, so SDE->ARTS consumes transient carriers
only for contract recovery and then drops them. Downstream passes still see
the loop/memref IR shape they already expect. **No `linalg.generic` survives
past SDE→ARTS.**

---

## Part 4: PatternDiscovery Removal

Investigation confirmed `PatternDiscovery` is a **hint/cache**, not a
requirement. It has now been removed from executable pipeline paths.
Every downstream consumer has graceful fallback behavior:

### Why removal is safe

| Consumer | Without depPattern | Impact |
|----------|-------------------|--------|
| DbAnalysis | `classifyDistributionPattern()` via graph analysis | Safe — full IR fallback |
| DbPartitioning | Defaults to `unknown` → block partition | Safe — conservative |
| EdtOrchestrationOpt | Early return (skips wave grouping) | Optimization loss only |
| ForLowering grouping | Comparison fails → no EDT grouping | Optimization loss only |
| DepTransforms | **Does NOT read depPattern** — independent IR matching | Unaffected |
| KernelTransforms | **Does NOT read depPattern** — independent IR matching | Unaffected |

**DepTransforms and KernelTransforms are 100% independent** of PatternDiscovery.
They perform their own IR-based pattern matching and stamp contracts themselves:
- `JacobiAlternatingBuffersPattern`: matches 2 parallel EDTs with copy+stencil
- `Seidel2DWavefrontPattern`: matches in-place 2D stencil with ±1 offsets
- `StencilTilingNDPattern`: full affine index analysis, computes halo offsets
- `MatmulReductionPattern`: matches k-loop reduction with mul+add chain

Status: completed in executable code paths. The current pattern pipeline is:

```cpp
DepTransforms
  -> LoopNormalization
  -> StencilBoundaryPeeling
  -> LoopReordering
  -> KernelTransforms
  -> CSE
```

### Pattern contract flow after removal

```
Stage 2: ConvertOpenMPToSde → RaiseToLinalg → ConvertSdeToArts
         (transient linalg-derived contracts for supported cases; stamped
          attribute fallback for the rest)
Stage 5: DepTransforms (independent: stamps jacobi/seidel)
         KernelTransforms (independent: stamps stencil_tiling_nd/matmul)
Stage 6: DbAnalysis (fallback: classifyDistributionPattern() for unstamped loops)
Stage 7+: Downstream passes read depPattern if present, use fallback if absent
```

### What stays in core

- **DepTransforms** (JacobiAlternatingBuffers, Seidel2DWavefront)
- **KernelTransforms** (MatmulReduction, StencilTilingND)
- These operate on `arts.for`, do independent pattern matching, and are in core/

---

## Part 5: Cost-Aware SDE Optimization (Limited Implementation + Future Architecture)

This part documents the currently implemented cost-model-backed SDE scope/chunk
passes and the remaining architecture for broader SDE-level optimization. The
`SDECostModel` interface (Part 1) already supports these use cases.

### Implemented today: `SdeScopeSelection` + `SdeChunkOptimization` + `SdeReductionStrategy`

`SdeScopeSelection` is the first cost-driven SDE region pass in the tree. It
runs on `sde.cu_region <parallel>` before `RaiseToLinalg` / `ConvertSdeToArts`
and currently maps machine topology directly onto SDE scope:

- single-node cost model -> `scope(<local>)`
- multi-node cost model -> `scope(<distributed>)`

This keeps concurrency scope as an SDE-owned decision rather than hardcoding it
inside OMP->SDE conversion.

`SdeChunkOptimization` is the first cost-driven SDE optimization pass in the
tree. It runs on `sde.su_iterate` before `RaiseToLinalg` / `ConvertSdeToArts`
and currently handles only a narrow but real synthesis subset:

- missing chunk size only
- one-dimensional loops only
- `dynamic` / `guided` schedules only
- constant or symbolic trip count only

It preserves explicit source chunk sizes and rewrites the SDE op in place with
a synthesized `chunkSize` operand. For symbolic trip counts, the pass
materializes the chunk computation as `arith` SSA directly in the SDE IR. This
is intentionally validated at the SDE layer: contract tests inspect the IR dump
after `SdeChunkOptimization` and check the resulting
`arts_sde.su_iterate(... schedule(<kind>, %chunk))` directly, before ARTS
lowering.

`SdeReductionStrategy` is a limited cost-driven SDE reduction pass. It also
runs on `sde.su_iterate` before `RaiseToLinalg` / `ConvertSdeToArts` and
currently handles only the narrow producer-backed case:

- exactly one preserved reduction accumulator
- exactly one preserved reduction kind
- known non-custom reduction kind
- annotation-only behavior today

The pass stamps an SDE-owned `reduction_strategy(<atomic|tree>)` attribute on
eligible loops. Today it uses `add` as the only atomic-capable reduction kind
in SDE and keeps validation at the SDE IR layer: contract tests inspect the
IR dump after `SdeReductionStrategy` rather than inferring the choice back from
later ARTS IR. `ConvertSdeToArts` now also persists that recommendation onto
the lowered ARTS loop / EDT as `arts.reduction_strategy`, but there is still no
lowering consumer for the ARTS attribute yet.

### SDE Optimization Decisions Enabled by SDECostModel

Each decision uses SDE-level concepts and queries the abstract cost model.
The ARTS costs change dramatically by topology:

| Decision | SDE Op | Cost Query | Local Cost | Distributed Cost | Ratio |
|----------|--------|-----------|------------|-----------------|-------|
| Reduction strategy | su_iterate.reduction_strategy | `getReductionCost(N)` vs `getAtomicUpdateCost()*N` | atomic/tree crossover | earlier tree crossover | 5x atomic |
| Chunk sizing | su_iterate.chunkSize | `getTaskCreationCost()` vs work/chunk | 1800 cy | 2500 cy | 1.4x |
| Distribution scope | cu_region.scope | `getTaskSyncCost()` distributed vs local | 3000 cy | 5000 cy | 1.7x |
| Halo width | mu_dep offsets | `getHaloExchangeCostPerByte()` * bytes | 0.01/B | 0.5/B | **50x** |
| Schedule override | su_iterate.schedule | `getSchedulingOverhead(kind, trip)` | ~0 static | ~250 dynamic | schedule-dependent |
| Buffer strategy | linalg ins/outs | `getAllocationCost()` vs copy bandwidth | 1500 cy | 2000 cy | 1.3x |

**The 50x halo exchange cost difference is the most impactful**: on
single-node, narrow halos are cheap (shared memory). On distributed,
wider halos amortize network round-trips. The cost model makes this
decision automatic.

### Remaining Future SDE Passes

1. **Broaden `SdeReductionStrategy`**: Extend the current annotation-only
   implementation beyond the initial single-reduction `sde.su_iterate` subset,
   decide whether it should also cover `sde.cu_reduce`, and eventually map the
   recommendation into downstream lowering when there is a real consumer.

2. **SdeScheduleRefinement**: Queries `getSchedulingOverhead(kind, trip)`.
   Overrides OMP schedule hint when cost model disagrees (e.g., static→dynamic
   for irregular work).

3. **Broaden `SdeChunkOptimization`**: Extend the current implementation
   beyond the current one-dimensional dynamic/guided subset. Future work can
   incorporate richer work estimates, more schedule kinds, and multi-dim loop
   handling while preserving the same SDE-level validation model.

4. **Broaden `SdeScopeSelection`**: Extend the current topology-based
   implementation to make richer scope decisions from cost-model tradeoffs
   instead of only mirroring single-node vs multi-node topology.

### Multi-Target Vision

```
SDECostModel (abstract interface — SDE-owned)
  ├── ARTSCostModel (local)   — low sync, shared memory, reclaimed threads
  ├── ARTSCostModel (distrib) — high sync, network costs, reserved threads
  ├── GPUCostModel (future)   — 10x task creation, PCIe transfer, warp shuffle
  └── LegionCostModel (future) — dependent task costs, region access
```

Same SDE optimization passes, different cost tables → different decisions.
This is the architectural novelty.

---

## Files Modified

| File | Change |
|------|--------|
| `include/arts/utils/costs/SDECostModel.h` | **NEW** — abstract SDE cost interface (runtime-agnostic) |
| `include/arts/utils/costs/ARTSCostModel.h` | **NEW** — ARTS impl (topology-aware: local vs distributed) |
| `lib/arts/utils/costs/CMakeLists.txt` | **NEW** — build stub |
| `lib/arts/utils/CMakeLists.txt` | Add costs/ subdirectory |
| `include/arts/dialect/core/Analysis/AnalysisManager.h` | Add `SDECostModel` member + accessor |
| `lib/arts/dialect/core/Analysis/AnalysisManager.cpp` | Create `ARTSCostModel` in constructor |
| `include/arts/dialect/sde/IR/SdeOps.td` | Add SDE-owned `reduction_strategy` enum/attr on `sde.su_iterate` |
| `lib/arts/dialect/sde/Transforms/ConvertOpenMPToSde.cpp` | Remove AnalysisManager import, take `SDECostModel*`; still bridges upstream `arts.omp_dep` |
| `lib/arts/dialect/sde/Transforms/SdeScopeSelection.cpp` | **NEW** — limited cost-model-backed scope selection on `sde.cu_region <parallel>` |
| `lib/arts/dialect/sde/Transforms/SdeChunkOptimization.cpp` | **NEW** — limited cost-model-backed chunk synthesis on `sde.su_iterate` |
| `lib/arts/dialect/sde/Transforms/SdeReductionStrategy.cpp` | **NEW** — limited cost-model-backed reduction strategy annotation on eligible `sde.su_iterate` ops |
| `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp` | Core rewrite: `SdeSuIterateOp`, stamp classification, create transient `linalg.generic` carriers for supported cases |
| `include/arts/dialect/sde/Transforms/Passes.td` | Add `LinalgDialect`, remove `ArtsDialect`, declare `SdeScopeSelection`, `SdeChunkOptimization`, and `SdeReductionStrategy` |
| `include/arts/dialect/sde/Transforms/Passes.h` | Add linalg include |
| `include/arts/passes/Passes.h` | Add SDE reduction strategy pass factory declaration |
| `lib/arts/passes/CMakeLists.txt` | Add `MLIRLinalgDialect`, `MLIRLinalgTransforms` |
| `tools/compile/Compile.cpp` | Insert `SdeScopeSelection`, `SdeChunkOptimization`, and `SdeReductionStrategy`, move `RaiseToLinalg`, remove PatternDiscovery, and keep SDE->ARTS independent of `AnalysisManager` |
| `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp` | `classifyFromLinalg` + stamp + erase transient generics after consumption, and persist SDE reduction strategy onto lowered ARTS ops |
| `lib/arts/dialect/core/Transforms/EdtTransformsPass.cpp` | Preserve an existing `arts.reduction_strategy` from SDE instead of recomputing over it, and use centralized reduction-strategy value constants |
| `include/arts/utils/OperationAttributes.h` | Centralize `arts.reduction_strategy` value strings alongside the attribute name |
| `lib/arts/dialect/core/Transforms/PatternDiscovery.cpp` | **DELETE** |
| `include/arts/passes/Passes.td` | Remove PatternDiscovery pass definition |
| `include/arts/passes/Passes.h` | Remove `createPatternDiscoveryPass()` declaration |

---

## Verification

1. `dekk carts build` — must compile cleanly
2. `dekk carts test` — 167/167 tests pass
3. `grep -r "arts::ForOp" lib/arts/dialect/sde/` → **zero results**
4. `grep -r "PatternDiscovery" lib/ tools/` → **zero results** (fully removed)
5. `AM->getCostModel()` returns `SDECostModel&` — accessible from any pass
6. `grep -r "AnalysisManager" lib/arts/dialect/sde/` → **zero results** (SDE pass plumbing fixed; residual `arts.omp_dep` input boundary remains upstream)
7. `SDECostModel::isDistributed()` returns correct value based on AbstractMachine
8. Run with `-mlir-print-ir-after-all` to verify:
   - After `SdeScopeSelection`: `arts_sde.cu_region <parallel>` has
     `scope(<local>)` on single-node configs and `scope(<distributed>)` on
     multi-node configs
   - After `SdeChunkOptimization`: eligible dynamic/guided loops have an
     SDE-level `chunkSize` on `arts_sde.su_iterate`; explicit source chunks
     remain unchanged
   - After `SdeReductionStrategy`: eligible single-reduction
     `arts_sde.su_iterate` ops have an SDE-owned
     `reduction_strategy(<atomic|tree>)` annotation chosen from the active
     cost model
   - After RaiseToLinalg: supported bodies have transient `linalg.generic`
     alongside the original loop/memref body
   - After SDE→ARTS: `arts.for` bodies still contain the loop/memref IR shape,
     transient generics are erased, and pattern contracts are stamped as
     attributes
   - DepTransforms/KernelTransforms still detect and stamp patterns independently
9. SDE scope tests: `openmp_to_arts_selects_local_scope.mlir` and
   `openmp_to_arts_selects_distributed_scope.mlir` validate scope behavior on
   SDE IR, not reconstructed ARTS IR
10. SDE chunk tests: `openmp_to_arts_synthesizes_schedule_chunk.mlir` and
   `openmp_to_arts_preserves_schedule_chunk.mlir` validate chunk behavior on
   SDE IR, not reconstructed ARTS IR
11. SDE reduction tests:
    `openmp_to_arts_selects_atomic_reduction_strategy.mlir`,
    `openmp_to_arts_selects_tree_reduction_strategy.mlir`, and
    `openmp_to_arts_avoids_atomic_for_mul_reduction_strategy.mlir` validate
    reduction strategy selection on SDE IR, not reconstructed ARTS IR
12. Spot-check benchmarks: jacobi → stencil contract (from DepTransforms),
   2mm → uniform contract (from linalg), stencil → stencil_tiling_nd (from KernelTransforms)

---

## Part 6: Tensor-Backed SDE Carriers (Initial Implementation + Future Path)

The first tensor step is now live: after `RaiseToLinalg` materializes a
transient memref-backed `linalg.generic`, `RaiseToTensor` rewrites the same
carrier into tensor-backed form inside SDE. This keeps tensor IR fully on the
SDE side of the boundary: `ConvertSdeToArts` still lowers from the original
loop/memref body, recovers contracts from the transient carrier, and erases
the carrier chain before downstream ARTS passes run.

### Why Tensors (SSA Immutability = Free Dependency Analysis)

Tensors are SSA values — immutable, no aliasing, dependencies are def-use
chains. This eliminates the need for the 6,477 LOC metadata pipeline that
pre-computes alias analysis. Key advantages:

- **ElementwiseOpFusion** (tensor-only): Fuses producer-consumer linalg ops
  via SSA chains. Memref can't do this without alias analysis.
- **TilingInterface**: Generic tiling that produces `scf.forall` (parallel
  loops equivalent to `sde.su_iterate`). Replaces custom BlockLoopStripMining.
- **One-Shot Bufferization**: Proven-optimal buffer allocation (in-place vs
  copy). Could replace heuristic DbPartitioning decisions.
- **scf.forall**: MLIR's parallel loop with `mapping` attribute for
  thread/block/node distribution — natural mapping to SDE concepts.

### Current Tensor Carrier Pipeline

```
CURRENT (this commit):
  OMP→SDE → RaiseToLinalg(memref) → RaiseToTensor(transient tensor carrier) →
  SdeTensorOptimization(tiled executable SDE loop rewrite) → SDE→ARTS →
  [18 stages]

FUTURE (separate commit):
  OMP→SDE → RaiseToLinalg(memref) → RaiseToTensor → SdeOpt(tensor) →
  Bufferize → SDE→ARTS → [downstream stages]

Where SdeOpt(tensor) includes:
  - ElementwiseOpFusion (upstream)
  - TileUsingInterface (upstream) → produces scf.forall
  - Cost-model-driven tile sizes via SDECostModel
```

### Coverage Estimate

- **~30-40%** of benchmarks fully raiseable to tensor path (uniform access,
  no pointer indirection, no I/O in loop body)
- **~60-70%** stay on memref path (indirect indexing, sparse, aliased pointers)
- Graceful fallback: non-raiseable loops keep `memref.load/store`, downstream
  passes handle them via existing IR analysis

### Key Upstream Transforms Available

| Transform | Tensor-only? | What it replaces |
|-----------|-------------|-----------------|
| ElementwiseOpFusion | Yes | Manual fusion analysis |
| TileUsingInterface | Dual | BlockLoopStripMining |
| One-Shot Bufferization | Tensor→memref | DbPartitioning heuristics |
| FoldTensorSubset | Yes | Manual reshape |
| PadOpVectorizationPattern | Dual | Manual vectorization |

### What Is Implemented Now

- `RaiseToTensor` rewrites ranked memref-backed transient carriers only.
- `SdeTensorOptimization` uses `SDECostModel` to choose a tile width from the
  worker count and minimum useful work, then strip-mines eligible 1-D
  elementwise `sde.su_iterate` loops so the transformation survives lowering.
- The first concrete subset is the currently supported `RaiseToLinalg` carrier
  subset. Elementwise loops use `tensor.empty` destinations; the pass stays
  conservative for the rest.
- `ConvertSdeToArts` now drops dead `bufferization.to_tensor` /
  `tensor.empty` carrier ops after consuming the transient `linalg.generic`.
- Validation stays at the pass boundary: contract tests inspect IR dumps after
  both `RaiseToTensor` and `ConvertSdeToArts`.

### Why The Full Tensor Path Is Not Done Yet

- All 18 downstream stages expect `scf.for + memref.load/store` in `arts.for`
- Tensor path requires One-Shot Bufferization BEFORE SDE→ARTS conversion
- Testing surface: every benchmark needs tensor-path validation
- The memref-based RaiseToLinalg (Part 2) is the prerequisite — it establishes
  the linalg infrastructure that the tensor path builds on

---

## NOT Doing (This Commit)

- **General cost-aware SDE optimization suite** (Part 5): only limited `SdeScopeSelection`, `SdeChunkOptimization`, and a narrow `SdeReductionStrategy` are implemented; broader schedule/scope/reduction work remains future work
- **Full tensor-first path** (Part 6): initial `RaiseToTensor` carrier rewrite is implemented, but tensor optimization + bufferization back to loops is still future work
- **Replacing hardcoded thresholds**: Future commits consume `CostModel` in existing heuristic files
- **Metadata removal**: Independent of linalg — separate commit
- **Machine calibration**: `dekk carts install --calibrate` — future
