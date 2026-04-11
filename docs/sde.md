# SDE as Optimization Layer: CostModel + RaiseToLinalg + Pattern Contracts

## Context

CARTS has a layering problem: optimization decisions (tiling, buffer strategy,
reduction strategy, distribution) are made deep inside ARTS-specific passes
using 21 hardcoded thresholds. SDE — designed as the runtime-agnostic layer —
is currently a pass-through: it captures OpenMP semantics and immediately
converts to ARTS.

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

**Current problems**:
1. `RaiseToLinalg.cpp` walks `arts::ForOp` (layering violation), is diagnostic-only
2. No cost model — 21 hardcoded thresholds across 8 files
3. Pattern contracts stamped at Stage 5 (PatternDiscovery) on `arts.for` — too late
4. SDE-captured metadata (reduction combiner, schedule kind) is LOST during conversion
5. No SDE-level optimization pass — purely semantic capture

---

## Pipeline Change

```
CURRENT:
  buildOpenMPToArtsPipeline:
    ConvertOpenMPToSde → ConvertSdeToArts → VerifySdeLowered → ...
  buildPatternPipeline:
    RaiseToLinalg (diagnostic, on arts::ForOp) → PatternDiscovery(seed) → ...

PROPOSED:
  buildOpenMPToArtsPipeline:
    ConvertOpenMPToSde → RaiseToLinalg (on sde.su_iterate) → ConvertSdeToArts → ...
  buildPatternPipeline:
    DepTransforms → LoopNormalization → ... → KernelTransforms → ...
    (PatternDiscovery REMOVED — linalg contracts + independent detection suffice)
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

**ConvertOpenMPToSde.cpp** currently includes `AnalysisManager.h` but
NEVER USES IT (verified — AM is stored but never accessed). Fix:
- Remove `#include "arts/dialect/core/Analysis/AnalysisManager.h"`
- Change constructor param from `AnalysisManager*` to `SDECostModel*`
- SDE passes now receive `SDECostModel*`, never see ARTS types

**Compile.cpp** injection:
```cpp
auto &costModel = AM->getCostModel();
pm.addPass(sde::createConvertOpenMPToSdePass(&costModel));
pm.addPass(sde::createRaiseToLinalgPass());
pm.addPass(sde::createConvertSdeToArtsPass(AM));
```

### Step 1E: CMake

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

### Step 2C: Create `linalg.generic` from Loop Body

**File**: `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp`

After existing analysis (collect nest, build indexing maps, classify):

1. **Operand collection**: Each `memref.load` → `ins`, each `memref.store` → `outs`.
   Stencils: each distinct `(memref, indexingMap)` pair = separate input.

2. **Body reconstruction**: Map `memref.load` results → block args, clone
   arithmetic, create `linalg.yield` with stored values.

3. **Op creation**: `builder.create<linalg::GenericOp>(loc, TypeRange{},
   inputMemrefs, outputMemrefs, allMaps, linalgIterTypes, bodyBuilder)`

4. **Erase original body**: Remove memref.load/store, inner scf.for ops.
   Keep `sde.yield` terminator.

Best-effort: non-raiseable bodies (indirect indexing, control flow, non-affine)
stay as memref.load/store — downstream passes (DepTransforms, KernelTransforms,
DbAnalysis) handle them via independent IR analysis and graceful fallback.

### Step 2D: Reduction Raising

Handle `sde.su_iterate` with `reductionAccumulators`:
- Remove `iter_args` bailout in `collectInner`
- Accumulator becomes 0-D `memref.alloca()` outs operand
- Iterator type = `reduction` for dims not in output maps
- Load result after generic, feed to `sde.yield`

### Step 2E: Move Pass in Pipeline

**File**: `tools/compile/Compile.cpp`

Remove from `buildPatternPipeline` (line 707).
Add to `buildOpenMPToArtsPipeline` between OMP→SDE and SDE→ARTS:
```cpp
pm.addPass(arts::sde::createConvertOpenMPToSdePass(AM));
pm.addPass(arts::sde::createRaiseToLinalgPass());    // NEW
pm.addPass(arts::sde::createConvertSdeToArtsPass(AM));
```

---

## Part 3: SDE→ARTS Contract Stamping + Linalg Lowering

**File**: `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp`

In `SuIterateToArtsPattern::matchAndRewrite()`, after creating `arts.for`:

### Step 3A: Classify from linalg structure

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
  if (nRed > 0) return ArtsDepPattern::reduction;
  return std::nullopt;
}
```

### Step 3B: Stamp contracts

- Walk cloned body for `linalg::GenericOp`
- If found: classify → stamp `depPattern`, `distributionPattern`, stencil
  offsets via `StencilNDPatternContract` (from `PatternTransform.h`)
- If not found: no stamp — DepTransforms/KernelTransforms detect independently,
    DbAnalysis falls back to `classifyDistributionPattern()` graph analysis

### Step 3C: Lower linalg back to loops

```cpp
IRRewriter rewriter(generic.getContext());
rewriter.setInsertionPoint(generic);
(void)linalg::linalgOpToLoops(rewriter, generic);
rewriter.eraseOp(generic);
```

After this: `arts.for` body has `scf.for + memref.load/store` — exactly
what all downstream passes (DepTransforms, KernelTransforms, stages 7-18)
expect. **No linalg.generic survives past SDE→ARTS.**

---

## Part 4: Remove PatternDiscovery Entirely

Investigation confirmed PatternDiscovery is a **hint/cache**, not a requirement.
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

### Step 4A: Delete PatternDiscovery.cpp

**Delete**: `lib/arts/dialect/core/Transforms/PatternDiscovery.cpp`
**Delete**: `include/arts/dialect/core/Transforms/pattern/PatternDiscovery.h` (if exists)

### Step 4B: Remove from Pipeline

**File**: `tools/compile/Compile.cpp`

Remove BOTH invocations:
- `PatternDiscovery(seed)` from `buildPatternPipeline` (line ~708)
- `PatternDiscovery(refine)` from `buildPatternPipeline` (line ~713)

### Step 4C: Remove Pass Registration

**File**: `include/arts/passes/Passes.td`
- Remove `PatternDiscovery` pass definition

**File**: `include/arts/passes/Passes.h`
- Remove `createPatternDiscoveryPass()` declaration

### Step 4D: Update ContractValidation (if needed)

Check if `ContractValidation.cpp` expects pre-DB patterns to exist.
If so, relax the check — patterns are now optional at that stage.

### Pattern contract flow after removal

```
Stage 2: ConvertOpenMPToSde → RaiseToLinalg → ConvertSdeToArts
         (linalg-derived: stamps depPattern for ~60% of loops)
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

## Part 5: Future — Cost-Aware SDE Optimization (Architecture Only)

This part is NOT implemented now but documents the architecture for future
cost-model-driven SDE optimization passes. The SDECostModel interface
(Part 1) is designed to support these use cases.

### SDE Optimization Decisions Enabled by SDECostModel

Each decision uses SDE-level concepts and queries the abstract cost model.
The ARTS costs change dramatically by topology:

| Decision | SDE Op | Cost Query | Local Cost | Distributed Cost | Ratio |
|----------|--------|-----------|------------|-----------------|-------|
| Reduction strategy | cu_reduce | `getReductionCost(N)` vs `getAtomicUpdateCost()*N` | tree@16+ | tree@4+ | 4x sync |
| Chunk sizing | su_iterate.chunkSize | `getTaskCreationCost()` vs work/chunk | 1800 cy | 2500 cy | 1.4x |
| Distribution scope | cu_region.scope | `getTaskSyncCost()` distributed vs local | 3000 cy | 5000 cy | 1.7x |
| Halo width | mu_dep offsets | `getHaloExchangeCostPerByte()` * bytes | 0.01/B | 0.5/B | **50x** |
| Schedule override | su_iterate.schedule | `getSchedulingOverhead(kind, trip)` | ~0 static | ~250 dynamic | schedule-dependent |
| Buffer strategy | linalg ins/outs | `getAllocationCost()` vs copy bandwidth | 1500 cy | 2000 cy | 1.3x |

**The 50x halo exchange cost difference is the most impactful**: on
single-node, narrow halos are cheap (shared memory). On distributed,
wider halos amortize network round-trips. The cost model makes this
decision automatic.

### Future SDE Passes (each a standalone commit)

1. **SdeReductionStrategy**: Queries `getReductionCost(workerCount)` vs
   `getAtomicUpdateCost() * workerCount`. Stamps
   `sde.reduction_strategy = tree|linear|atomic` on `cu_reduce`/`su_iterate`.

2. **SdeScheduleRefinement**: Queries `getSchedulingOverhead(kind, trip)`.
   Overrides OMP schedule hint when cost model disagrees (e.g., static→dynamic
   for irregular work).

3. **SdeChunkOptimization**: Queries `getTaskCreationCost()` and estimates
   work/chunk. Adjusts `su_iterate.chunkSize` for optimal granularity.

4. **SdeScopeSelection**: Queries `getTaskSyncCost()` for local vs distributed.
   Stamps `cu_region.concurrency_scope` based on cost (currently hardcoded
   in ConvertOpenMPToSde.cpp:135-137).

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
| `lib/arts/dialect/sde/Transforms/ConvertOpenMPToSde.cpp` | Remove AnalysisManager import, take `SDECostModel*` |
| `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp` | Core rewrite: `SdeSuIterateOp`, create `linalg.generic`, reductions |
| `include/arts/dialect/sde/Transforms/Passes.td` | Add `LinalgDialect`, remove `ArtsDialect` |
| `include/arts/dialect/sde/Transforms/Passes.h` | Add linalg include |
| `lib/arts/passes/CMakeLists.txt` | Add `MLIRLinalgDialect`, `MLIRLinalgTransforms` |
| `tools/compile/Compile.cpp` | Move RaiseToLinalg, remove PatternDiscovery (seed+refine) |
| `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp` | `classifyFromLinalg` + stamp + `linalgOpToLoops` |
| `lib/arts/dialect/core/Transforms/PatternDiscovery.cpp` | **DELETE** |
| `include/arts/passes/Passes.td` | Remove PatternDiscovery pass definition |
| `include/arts/passes/Passes.h` | Remove `createPatternDiscoveryPass()` declaration |

---

## Verification

1. `dekk carts build` — must compile cleanly
2. `dekk carts test` — 161/161 tests pass
3. `grep -r "arts::ForOp" lib/arts/dialect/sde/` → **zero results**
4. `grep -r "PatternDiscovery" lib/ tools/` → **zero results** (fully removed)
5. `AM->getCostModel()` returns `SDECostModel&` — accessible from any pass
6. `grep -r "AnalysisManager" lib/arts/dialect/sde/` → **zero results** (layering fixed)
7. `SDECostModel::isDistributed()` returns correct value based on AbstractMachine
6. Run with `-mlir-print-ir-after-all` to verify:
   - After RaiseToLinalg: raiseable bodies have `linalg.generic`
   - After SDE→ARTS: `arts.for` bodies have `scf.for + memref` (lowered back),
     pattern contracts stamped as attributes
   - DepTransforms/KernelTransforms still detect and stamp patterns independently
7. Spot-check benchmarks: jacobi → stencil contract (from DepTransforms),
   2mm → uniform contract (from linalg), stencil → stencil_tiling_nd (from KernelTransforms)

---

## Part 6: Future — Tensor-First Optimization Path (Architecture Only)

Investigation of upstream MLIR tensor infrastructure revealed significant
optimization opportunities. This documents the architecture for a future
tensor-first path that would replace much of the current metadata pipeline.

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

### Proposed Future Pipeline Extension

```
CURRENT (this commit):
  OMP→SDE → RaiseToLinalg(memref) → SDE→ARTS → [18 stages]

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

### Why Not Now

- All 18 downstream stages expect `scf.for + memref.load/store` in `arts.for`
- Tensor path requires One-Shot Bufferization BEFORE SDE→ARTS conversion
- Testing surface: every benchmark needs tensor-path validation
- The memref-based RaiseToLinalg (Part 2) is the prerequisite — it establishes
  the linalg infrastructure that the tensor path builds on

---

## NOT Doing (This Commit)

- **Cost-aware SDE optimization passes** (Part 5): Architecture documented, interface ready, passes are future work
- **Tensor-first path** (Part 6): Architecture documented, requires RaiseToTensor + Bufferization — separate commit after Part 2 proves stable
- **Replacing hardcoded thresholds**: Future commits consume `CostModel` in existing heuristic files
- **Metadata removal**: Independent of linalg — separate commit
- **Machine calibration**: `dekk carts install --calibrate` — future
