# Plan: SDE Cost-Model-Driven Optimization Pipeline

## Context

SDE (State/Dependency/Effect Execution Contract) sits between OpenMP and ARTS
core. It analyzes **State** (data layout, placement), **Dependencies** (access
patterns, ordering), and **Effects** (sync, reductions, distribution) to stamp
**cost-model-driven canonical contracts** so ARTS core materializes them.

Today SDE has 14 passes but most decisions are hardcoded, and 48% of benchmarks
(all stencils) get **ZERO SDE value** because the structured analysis requires
perfectly nested loops. Additionally, the ARTS PatternPipeline (stage 5)
redundantly re-derives patterns that SDE already computed.

**Branch**: `architecture/sde-restructuring`

**Architectural constraints** (NON-NEGOTIABLE):
1. SDE works at the **canonical level** — access offsets, dimensions, footprints.
   No pattern-specific concepts like "StencilContract" in SDE code. The enum
   values (stencil/matmul/elementwise/reduction) describe structural properties.
2. ARTS should **trust SDE contracts** — PatternPipeline passes must defer to
   existing SDE contracts instead of re-deriving patterns.
3. SDE works at the **tensor level** with transient linalg.generic carriers and
   attributes. The carrier model is correct; the analysis coverage is the gap.

## Status Snapshot (2026-04-12)

- Implemented in the current worktree: Phases `-1`, `-2`, `0`, `1`, `2`, `3`,
  and `4A`/`4B`/the planned dead-code portion of `4C`.
- Phase `-3` is still planning-only, but the original text below was too
  aggressive and did not match the repo's current `ArtsToLLVM` / `RtToLLVM`
  split. It has been rewritten to describe cleanup of the existing boundary,
  not deletion of `core/Conversion/ArtsToLLVM/`.
- Phase `5` previously used hardcoded test counts that drifted. It now tracks
  the remaining coverage gaps qualitatively instead of asserting stale totals.

---

## Current SDE Pipeline & Gaps

```
14 SDE passes, organized by S/D/E pillar:

STATE (data & layout):
  - RaiseToLinalg          — creates linalg carriers (skips stencils entirely)
  - RaiseToTensor           — lifts to tensor-backed carriers
  - SdeStructuredSummaries  — stamps canonical access descriptors

DEPENDENCY (access patterns):
  - SdeTensorOptimization   — cost-model strip-mining (elem+matmul only, 1-D only)
  - SdeElementwiseFusion    — fuses sibling elementwise (no cost gate)
  - SdeIterationSpaceDecomp — peels stencil boundaries (0 tests)

EFFECT (sync, reduction, distribution):
  - SdeReductionStrategy    — atomic/tree/local_accumulate selection
  - SdeScopeSelection       — local vs distributed scope
  - SdeScheduleRefinement   — static/dynamic/guided selection
  - SdeChunkOptimization    — chunk size for dynamic/guided
  - SdeDistributionPlanning — blocked/owner_compute (incomplete scope coverage)

STRUCTURAL:
  - ConvertOpenMPToSde      — OMP→SDE conversion
  - ConvertSdeToArts        — SDE→ARTS with contract stamping
  - VerifySdeLowered        — verification barrier
```

### Key Gaps Identified (5-agent investigation)

**Analysis**: `analyzeStructuredLoop()` fails on imperfect nests (scf.if/scf.for
mix) → 48% of benchmarks get zero classification.

**Loop transforms**: SdeTensorOptimization only does 1-D outer strip-mining for
elementwise+matmul. No multi-level tiling, no loop interchange, no stencil
tiling, no reduction tiling. Reductions explicitly excluded (line 253).

**Cost model underused**: 5 of 14 SDECostModel methods have 0 callers:
`getLocalDataAccessCost()`, `getRemoteDataAccessCost()`,
`getHaloExchangeCostPerByte()`, `getTaskCreationCost()` (only derived),
`getTaskSyncCost()` (only in formula).

**Sync**: No barrier elimination, no nowait inference, no task coarsening. All
barriers preserved unconditionally.

**ARTS redundancy**: 4 PatternPipeline passes (3,212 LOC) re-derive what SDE
already computes, mostly without checking for existing SDE contracts.

---

## TIER 0: DIRECTORY ORGANIZATION (Phase -1)

Reorganize the flat `lib/arts/dialect/sde/Transforms/` directory into
subdirectories matching the S/D/E pillars, mirroring how `core/Transforms/`
uses `db/`, `edt/`, `dep/`, `kernel/`, `loop/`, `pattern/`.

### Phase -1: Organize SDE Transforms into Pillar Subdirectories

**Status**: Implemented in the current worktree.

**Current state**: 13 `.cpp` files in a flat directory.

**Proposed structure** (by SDE pillar):

```
lib/arts/dialect/sde/Transforms/
  state/                          ← State: data layout, carrier creation, summaries
    RaiseToLinalg.cpp
    RaiseToTensor.cpp
    SdeStructuredSummaries.cpp
  dep/                            ← Dependencies: access patterns, loop transforms
    SdeTensorOptimization.cpp
    SdeElementwiseFusion.cpp
    SdeIterationSpaceDecomposition.cpp
    SdeLoopInterchange.cpp          (NEW — Phase 7)
  effect/                         ← Effects: sync, reduction, distribution, scheduling
    SdeReductionStrategy.cpp
    SdeScopeSelection.cpp
    SdeScheduleRefinement.cpp
    SdeChunkOptimization.cpp
    SdeDistributionPlanning.cpp
    SdeBarrierElimination.cpp       (NEW — Phase 10)
  ConvertOpenMPToSde.cpp            ← stays at root (entry boundary)
  VerifySdeLowered.cpp              ← stays at root (exit barrier)
  CMakeLists.txt
```

**Rationale for root-level files**:
- `ConvertOpenMPToSde.cpp` — dialect boundary conversion, not a pillar transform
- `VerifySdeLowered.cpp` — verification barrier, not a pillar transform
- These mirror `core/Conversion/` being separate from `core/Transforms/`

**Note**: `ConvertSdeToArts` already lives in `core/Conversion/SdeToArts/`,
not in `sde/Transforms/`, so it is unaffected.

#### Build system change

Update `CMakeLists.txt` to glob subdirectories (same pattern as core):

```cmake
file(GLOB CARTS_SDE_TRANSFORMS_SOURCE_FILES CONFIGURE_DEPENDS
    "*.cpp"
    "state/*.cpp"
    "dep/*.cpp"
    "effect/*.cpp"
)
set(CARTS_SDE_TRANSFORMS_SOURCE_FILES ${CARTS_SDE_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)
```

#### No header/Passes.td changes needed

- `Passes.td` and `Passes.h` are in `include/arts/dialect/sde/Transforms/` —
  they declare passes, not source locations. No changes needed.
- Each `.cpp` file's `#include` paths are absolute (`arts/dialect/sde/...`) —
  moving the `.cpp` within the same CMake target doesn't affect includes.

#### Git move commands (preserves history)

```bash
mkdir -p lib/arts/dialect/sde/Transforms/{state,dep,effect}
git mv lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp         lib/arts/dialect/sde/Transforms/state/
git mv lib/arts/dialect/sde/Transforms/RaiseToTensor.cpp          lib/arts/dialect/sde/Transforms/state/
git mv lib/arts/dialect/sde/Transforms/SdeStructuredSummaries.cpp lib/arts/dialect/sde/Transforms/state/
git mv lib/arts/dialect/sde/Transforms/SdeTensorOptimization.cpp  lib/arts/dialect/sde/Transforms/dep/
git mv lib/arts/dialect/sde/Transforms/SdeElementwiseFusion.cpp   lib/arts/dialect/sde/Transforms/dep/
git mv lib/arts/dialect/sde/Transforms/SdeIterationSpaceDecomposition.cpp lib/arts/dialect/sde/Transforms/dep/
git mv lib/arts/dialect/sde/Transforms/SdeReductionStrategy.cpp   lib/arts/dialect/sde/Transforms/effect/
git mv lib/arts/dialect/sde/Transforms/SdeScopeSelection.cpp      lib/arts/dialect/sde/Transforms/effect/
git mv lib/arts/dialect/sde/Transforms/SdeScheduleRefinement.cpp  lib/arts/dialect/sde/Transforms/effect/
git mv lib/arts/dialect/sde/Transforms/SdeChunkOptimization.cpp   lib/arts/dialect/sde/Transforms/effect/
git mv lib/arts/dialect/sde/Transforms/SdeDistributionPlanning.cpp lib/arts/dialect/sde/Transforms/effect/
```

#### Verification

1. `dekk carts build` — must succeed
2. Run the relevant SDE contract tests to confirm the pure file move did not
   change behavior. Avoid hardcoding suite totals here; the exact test count
   changes over time.

---

### Phase -2: Delete Dead ConvertOpenMPToArts (Legacy OMP Bypass Path)

**Status**: Implemented in the current worktree.

**Finding**: `ConvertOpenMPToArts` (legacy OMP→ARTS direct path) is **dead code**.
`ConvertOpenMPToSde` already handles 100% of OMP operations with superior
semantic preservation (reduction kinds, nowait, schedule+chunk, task deps).
The legacy pass is declared in `Passes.h`/`Passes.td` but **never registered
in the active pipeline** (`Compile.cpp` only calls `ConvertOpenMPToSde`).

**Files to delete/modify**:
- DELETE `lib/arts/dialect/core/Conversion/OmpToArts/ConvertOpenMPToArts.cpp`
- DELETE `OmpToArts/` directory
- Remove pass declaration from `include/arts/passes/Passes.td` (`ConvertOpenMPToArts` def)
- Remove `createConvertOpenMPToArtsPass()` from `include/arts/passes/Passes.h`
- Update `lib/arts/dialect/core/Conversion/CMakeLists.txt` — remove `OmpToArts/*.cpp` glob

**No pipeline stage rename needed** — the active CLI/pipeline stage remains
`openmp-to-arts`; only the dead bypass implementation disappears.

#### Verification
1. `dekk carts build` — must succeed (no callers of the deleted pass)
2. `dekk carts test` — all tests pass

---

### Phase -3: Tighten the Existing ArtsToLLVM / RtToLLVM Split

**Status**: Planning only. The original version of this phase was internally
inconsistent with the current repo architecture and has been corrected here.

**Correct architectural baseline**:
- Keep the single `ConvertArtsToLLVM` driver and the `arts-to-llvm` pipeline
  stage.
- Keep the existing `ArtsToLLVM` directory. The remaining work is boundary
  cleanup, not deleting `core/Conversion/ArtsToLLVM/`.
- Treat this phase as an untangling pass over the current `ArtsToLLVM` and
  `RtToLLVM` responsibilities, not as a from-scratch RT-only rewrite.

#### Scope of the cleanup

- Audit the current split between:
  - `lib/arts/dialect/core/Conversion/ArtsToLLVM/`
  - `lib/arts/dialect/core/Conversion/ArtsToRt/`
  - `lib/arts/dialect/rt/Conversion/RtToLLVM/`
- Move patterns only when they are genuinely RT-owned and do not require core
  ARTS semantics.
- Preserve core ARTS ops that are still structural or are created by multiple
  core passes.

#### What this phase should NOT do

- Do not delete `core/Conversion/ArtsToLLVM/`.
- Do not assume all runtime-adjacent core ops should become new `arts_rt.*`
  ops.
- Do not split `Compile.cpp` into a brand-new `ArtsToRt` stage followed by a
  separate top-level `RtToLLVM` stage unless the surrounding pipeline
  architecture is changed first.

#### Plausible completion criteria

- Fewer duplicated lowering patterns across `ArtsToLLVM` and `RtToLLVM`
- Clearer ownership comments for the ops that remain core-only versus RT-only
- No pipeline-stage churn for users of `dekk carts compile`

#### Verification
1. `dekk carts build` — must succeed after each ownership adjustment
2. `dekk carts test` — all tests pass
3. `Compile.cpp` still exposes the same top-level `arts-to-llvm` behavior

---

## TIER 1: IMMEDIATE FIXES (Phases 0-5)

These fix bugs, unblock benchmarks, and establish SDE contract authority.

### Phase 0: Robust Structured Loop Analysis (THE ROOT FIX)

**Status**: Implemented in the current worktree.

**Benchmarks unblocked**: ALL stencils (jacobi-for, jacobi2d, poisson-for,
sw4lite), stream, specfem3d

#### 0A. Extend `collectInner` to handle semi-perfect nests

**File**: `lib/arts/dialect/sde/Analysis/StructuredOpAnalysis.cpp` (lines 32-44)

**Problem**: When `ops.size() != 1`, current code immediately marks the block as
innermost. If an `scf.for` exists alongside effect-free ops (arith constants,
index casts), the inner loop's IV is never extracted.

**Change**: When `ops.size() > 1`, scan for a single `scf::ForOp` among the ops.
If found AND all other ops are `isMemoryEffectFree`, treat as semi-perfect nest:
push its IV, recurse into it. If no single inner for (or multiple exist), fall
through to existing behavior.

#### 0B. Extend `collectMemrefAccesses` to walk `scf.if` bodies

**File**: `lib/arts/dialect/sde/Analysis/StructuredOpAnalysis.cpp` (lines 116-149)

**Problem**: `scf::IfOp` is not memory-effect-free → line 142-145 returns false,
aborting analysis for any loop with boundary guards.

**Change**: Add `scf::IfOp` handling between lines 140 and 142:
- Recursively call `collectMemrefAccesses` on both `then` and `else` blocks
- Union reads/writes from both branches (envelope of all accessed locations)
- If a branch has no loads/stores, skip silently
- If a branch has ops that fail analysis, return false for the whole block

Also handle nested `scf::ForOp` inside innermost body (reduction loops) —
collect accesses from its body recursively but don't add its IV to the IVs.

#### 0C. No changes needed in SdeStructuredSummaries or SdeToArtsPatterns

Once `analyzeStructuredLoop()` succeeds for imperfect nests:
- `SdeStructuredSummaries.cpp` already stamps all neighborhood attrs (lines 55-64)
- `SdeToArtsPatterns.cpp` already reads via `getSdeNeighborhoodSummary()` (lines 675-681)

#### Tests
1. `tests/contracts/sde/structured_analysis_stencil_with_boundary_guard.mlir`
2. `tests/contracts/sde/structured_analysis_stencil_with_invariant_prefix.mlir`
3. `tests/contracts/sde/structured_analysis_imperfect_nest.mlir`

---

### Phase 1: Reduction Correctness

**Status**: Implemented in the current worktree.

#### 1A. Fix float-add atomic fallback (BUG)

**File**: `lib/arts/dialect/sde/Transforms/SdeReductionStrategy.cpp`

`isAtomicCapable()` (line 25) checks only KIND, not element type. Float-add
selects atomic → `EdtReductionLowering.cpp:276` falls back to O(n) linear.

**Fix**: `isAtomicCapable(kind, elemType)` returns false for float. Update call
site at line 87 to extract memref element type per accumulator.

**Test**: `tests/contracts/sde/sde_reduction_strategy_rejects_atomic_for_float_add.mlir`

#### 1B. Bitwise reduction inference (and/or/xor)

**Files**:
- `ConvertOpenMPToSde.cpp:117` — add `AndIOp→land`, `OrIOp→lor`, `XOrIOp→lxor`
- `EdtReductionLowering.h:29` — extend enum: `land=4, lor=5, lxor=6`
- `EdtReductionLowering.cpp:162,187,431` — combiners, identities, guard `val <= 6`

**Test**: `tests/contracts/sde/openmp_to_arts_infers_bitwise_and_reduction.mlir`

---

### Phase 2: Distribution Planning Completeness

**Status**: Implemented in the current worktree.

**File**: `lib/arts/dialect/sde/Transforms/SdeDistributionPlanning.cpp`

#### 2A. Elementwise for distributed scope (lines 74-78)
Replace scope check with `hasEnoughDistributedWork()` gate.

#### 2B. Matmul for distributed scope (lines 84-87)
Gate on `hasEnoughDistributedWork()`.

#### 2C. Reduction for distributed scope (lines 88-92)
Gate on `op.getReductionStrategyAttr() && hasEnoughDistributedWork()`.

#### 2D. Preserve elementwise/matmul classification after tensor strip-mining

**File**: `lib/arts/dialect/sde/Transforms/state/SdeStructuredSummaries.cpp`

When `SdeTensorOptimization` rewrites an elementwise or matmul loop into a
tiled scalar form, the refreshed summary can look reduction-shaped after the
carrier is erased. Preserve the existing non-reduction classification when the
loop has no reduction accumulators so `SdeDistributionPlanning` still sees the
intended contract.

#### Tests
1. `tests/contracts/sde/openmp_to_arts_sde_distribution_planning_elementwise_distributed.mlir`
2. `tests/contracts/sde/openmp_to_arts_sde_distribution_planning_matmul_distributed.mlir`
3. `tests/contracts/sde/sde_distribution_planning_reduction_distributed.mlir`

---

### Phase 3: ARTS PatternPipeline Deference to SDE

**Status**: Implemented in the current worktree for the existing stencil and
matmul authority checks.

**Goal**: Stage-5 passes SKIP re-derivation when SDE already stamped a contract.

#### 3A. StencilTilingNDPattern — skip on existing contract
**File**: `lib/arts/dialect/core/Transforms/kernel/StencilTilingNDPattern.cpp`
Add an early exit based on `getEffectiveDepPattern(...)` when the loop already
carries a stencil-family contract.

#### 3B. MatmulReductionPattern — skip on existing contract
**File**: `lib/arts/dialect/core/Transforms/kernel/MatmulReductionPattern.cpp`

#### 3C. LoopNormalization — skip on existing contract
**File**: `lib/arts/dialect/core/Transforms/LoopNormalization.cpp`

#### 3D. LoopReordering — skip on existing contract
**File**: `lib/arts/dialect/core/Transforms/LoopReordering.cpp`

#### Tests
1. `tests/contracts/sde/pattern_pipeline_defers_to_sde_stencil.mlir`
2. `tests/contracts/sde/pattern_pipeline_defers_to_sde_matmul.mlir`
3. `tests/contracts/sde/pattern_pipeline_falls_back_without_sde.mlir`

---

### Phase 4: Cleanup & Verification

**Status**: `4A`, `4B`, and the dead-code slice of `4C` are implemented in the
current worktree.

#### 4A. Delete dead SdeMuAccessOp
- `SdeOps.td` — remove definition
- `SdeToArtsPatterns.cpp` — remove `MuAccessToArtsPattern` + registration

#### 4B. Fix VerifySdeLowered — catch ALL orphaned linalg carriers
```cpp
if (auto linalgOp = dyn_cast<linalg::LinalgOp>(op))
  return op->getParentOfType<arts::ForOp>() != nullptr;
```

Also reject `bufferization.to_tensor` and `tensor.empty` when they survive
inside `arts.for`, since those are part of the same transient carrier chain.

#### 4C. Dead code removal
- `SDECostModel.h` — remove `getMinUsefulTaskWork()` (0 callers)
- `CreateDbs.cpp` — remove unused heuristic helpers (~40 LOC)

---

### Phase 5: Test Coverage Expansion

**Status**: The previous table in this section drifted and is no longer
accurate. Track the remaining coverage work as gaps, not frozen counts.

**Covered by the current worktree**:
- Structured analysis: boundary guards, invariant prefixes, imperfect nests
- Reduction correctness: float-add non-atomic fallback, bitwise reduction kinds
- Distribution planning: distributed elementwise, matmul, and reduction cases
- Pattern-pipeline authority: stencil contract survives stage 5 unchanged
- VerifySdeLowered: orphaned `linalg` carrier rejection

**Still worth adding**:
- `SdeIterationSpaceDecomposition` regression tests
- More `SdeElementwiseFusion` coverage, especially mismatched-bounds cases
- Additional PatternPipeline fallback tests when no SDE contract is present

---

## TIER 2: COMPUTE OPTIMIZATIONS (Phases 6-9)

Cost-model-driven loop and **tensor dimensionality** transforms at the SDE
level. SDE's transient linalg.generic carriers are the vehicle: MLIR provides
rich dimension APIs that operate directly on carriers, and ConvertSdeToArts
stamps contracts from the transformed carrier before erasing it.

**Key principle**: SDE can **add, remove, or reorder** carrier dimensions —
the same concepts as block partitioning (expand dims) and loop linearization
(collapse dims) but expressed at the tensor/linalg level and driven by the
cost model. The actual `scf.for` loop nest is transformed in tandem.

**Available MLIR APIs** (all in `mlir/Dialect/Linalg/Transforms/Transforms.h`):

| API | What it does | Dimension change |
|-----|-------------|-----------------|
| `collapseOpIterationDims(op, reassoc, rw)` | Fuse contiguous dims in linalg.generic | N-D → (N-k)-D |
| `interchangeGenericOp(rw, generic, perm)` | Permute iterator order | reorder dims |
| `splitReduction(rw, op, controlFn)` | Split reduction into outer-par + inner-red | N-D → (N+1)-D |
| `tileLinalgOp(rw, op, opts)` | Tile linalg with nested loops | creates tile loops |
| `linalg::pack(rw, op, sizes)` | Tile + reorder data layout | N-D → (N+k)-D |

**Carrier lifecycle** (how dimension changes flow):
```
RaiseToLinalg → creates carrier matching original loop structure
    ↓
SdeTensorOptimization → transforms carrier dims + rewrites scf.for to match
    ↓
ConvertSdeToArts → reads transformed carrier → stamps contracts → erases carrier
```

### Phase 6: N-Dimensional Tiling & Dimension Transforms

**File**: `lib/arts/dialect/sde/Transforms/dep/SdeTensorOptimization.cpp`

**Current state**: Hardcoded to 1-D (`su_iterate` bounds `size() == 1`, lines
257-259). Only tiles the outermost dimension. Only elementwise + matmul.
Reductions excluded (line 253). Uses ZERO MLIR dimension APIs.

#### 6A. Generalize to N-dimensional tile bands

Rewrite the core tiling logic to handle N parallel dimensions:

1. **Read dimension count from carrier**: `tensorGeneric.getNumLoops()` gives N.
   Extract parallel dims from `getIteratorTypesArray()`.

2. **Compute per-dimension tile sizes** (cost-model-driven):
   ```
   For N parallel dims with workerCount W:
     workers_per_dim[d] = distribute W across dims proportionally to trip counts
     tile[d] = max(ceil(trip[d] / workers_per_dim[d]), minItersPerWorker)
   ```
   For uniform distribution: `workers_per_dim[d] = ceil(W^(1/N))`.
   For weighted: allocate more workers to larger dimensions.

3. **Create nested tile loops**: Instead of one `scf.for` tile loop, create
   N nested `scf.for` tile loops (one per parallel dim). The innermost tile
   loop contains the cloned scalar body.

4. **Rewrite `su_iterate` bounds**: The outermost `su_iterate` step becomes
   `original_step[0] * tile[0]` for the first dimension. Inner tile loops
   handle dimensions 1..N-1.

**Current code flow to modify**:
- `buildTripCountValue()` (line 53): Generalize to return `SmallVector<Value>`
  for N dimensions, computing trip count per dim.
- `buildTileIterationValue()` (line 88): Return `SmallVector<Value>` of tile
  sizes, one per parallel dimension.
- `isTensorOptimizationCandidate()` (line 247): Remove 1-D restriction (lines
  257-259). Instead, require `lowerBounds.size() >= 1`.
- Rewrite loop (line 309): Create N-level nested `scf.for` tile structure.

**Example**: 2D elementwise `su_iterate(i: 0..128) { for j: 0..256 { A[i,j] = B[i,j] + 1 } }`
with 64 workers → `workers_per_dim = [8, 8]`, `tile = [16, 32]`:
```
su_iterate(i: 0..128 step 16) {
  scf.for j_tile = 0 to 256 step 32 {
    scf.for i_inner = i to min(i+16, 128) {
      scf.for j_inner = j_tile to min(j_tile+32, 256) {
        A[i_inner, j_inner] = B[i_inner, j_inner] + 1
      }
    }
  }
}
```

#### 6B. Enable stencil tiling (all classifications)

Remove the `switch` that only handles elementwise + matmul (lines 263-270).
Use the carrier's iterator types to determine which dimensions are tileable.
For stencils, tile size must exceed max halo offset:
```
tile[d] = max(cost_model_tile, accessMaxOffsets[d] - accessMinOffsets[d] + 1)
```

After Phase 0 fixes analysis, stencils WILL have `structuredClassification` +
neighborhood attrs. Cost model: `getLocalDataAccessCost()` drives
cache-aware tile size. Halo-aware: `accessMinOffsets/accessMaxOffsets` enforce
minimum tile per dim.

#### 6C. Enable reduction outer-dim tiling

Remove the reduction exclusion (line 253). For reductions, only tile the
PARALLEL dimensions (not the reduction dimension). The iterator types from
the carrier distinguish parallel from reduction dims. Reduction accumulators
are preserved — the tile loop wraps the parallel dims only.

#### 6D. Add elementwise fusion cost gate

`SdeElementwiseFusion.cpp` always fuses when structural criteria met. Add
`SDECostModel*` parameter. Before fusing, check combined body ops <
`getMinIterationsPerWorker() * 2`. This prevents excessive per-task work.

#### 6E. Dimension Collapsing — reduce carrier rank (N-D → fewer dims)

**API**: `linalg::collapseOpIterationDims(generic, reassociation, rewriter)`

**Concept**: Like `PerfectNestLinearizationPattern` (collapses 2D→1D via
div/rem at ARTS `ForOp` level with hardcoded 2048 threshold), but at the
SDE carrier level, driven by cost model, applicable to any N-D carrier.

**When to collapse**: Contiguous inner parallel dims where:
- All indexing maps for those dims are contiguous in memory (identity/projected
  permutation with adjacent result dims)
- Collapsing improves vectorization: `innerDimProduct < vectorizationThreshold`
- No halo offsets on collapsed dims (stencil dims must NOT be collapsed)

**Precondition check**: `areDimSequencesPreserved(indexingMaps, reassociation)`
— verifies dimension grouping is valid for all operands.

**Transform steps**:
1. Identify collapsible dim groups from carrier indexing maps
2. Apply `collapseOpIterationDims()` on the tensor-backed carrier
3. Fuse the corresponding `scf.for` loops into one loop with `trip = trip_j * trip_k`
4. Replace IV references: `j = linearIV / trip_k`, `k = linearIV % trip_k`
5. Update `su_iterate` if the outermost dim was involved

**Example**: 3D elementwise with row-major contiguous inner dims:
```
BEFORE: linalg.generic {parallel, parallel, parallel}  // 3 dims
        for i: for j: for k: A[i,j,k] = B[i,j,k]

AFTER:  linalg.generic {parallel, parallel}             // 2 dims (j,k collapsed)
        for i: for jk: A[i, jk/K, jk%K] = B[i, jk/K, jk%K]
```

**Cost model gate**: `getVectorWidth()` — collapse when inner dim product <
vector width × 4 (too short for efficient vectorization without collapsing).

#### 6F. Dimension Expansion — increase carrier rank (N-D → more dims)

**Concept**: Like `DbPartitionPlanner` (adds block-index dimensions to memrefs
via `memref<100x10> → memref<4x25x10>`), but at the SDE carrier level using
`tensor::ExpandShapeOp`.

**When to expand**: A dimension is too large for cache-friendly tiling:
- `tripCount[d] * elementSize > getL2CacheSize()` → split into tiles
- The expanded carrier has an extra "tile index" dim, enabling 2-level tiling

**Transform steps**:
1. Choose dimension to split and tile size from cost model
2. Apply `tensor::ExpandShapeOp` on carrier operands: `dim_d → (tile_idx, tile_inner)`
3. Update carrier indexing maps: `d → (d0 * tileSize + d1)` decomposition
4. Split the corresponding `scf.for` into outer tile + inner loops
5. The carrier now has N+1 dimensions — downstream tiling sees the tile structure

**Example**: 1D elementwise with huge trip count:
```
BEFORE: linalg.generic {parallel}           // 1 dim, trip=10000
        for i: A[i] = B[i]

AFTER:  linalg.generic {parallel, parallel}  // 2 dims (tile_outer, tile_inner)
        for i_outer: for i_inner: A[i_outer*100+i_inner] = B[...]
```

**Cost model gate**: `getL2CacheSize()` — expand when working set exceeds L2.

**Relationship to Phase 6A**: Phase 6A tiles the existing dims from the carrier.
Phase 6F adds NEW dims to the carrier first, then 6A can tile the expanded
structure. Expansion happens first in the pipeline.

#### 6G. Data Packing — cache-friendly layout via `linalg::pack`

**API**: `linalg::pack(rewriter, linalgOp, packedSizes)`

**Concept**: Tiles selected dimensions AND reorganizes data layout for
cache-line-aligned, contiguous access. Creates `linalg.pack` + transformed
`linalg.generic` + `linalg.unpack` chain.

**When to pack**: Matmul/stencil where inner-loop stride > 1:
- Matmul `C[i,j] += A[i,k] * B[k,j]`: B accessed with stride `j_size` on k
  → pack B as `B_packed[j/tile_j][k][tile_j]` for contiguous k-access
- Stencil with non-unit stride access patterns

**Transform steps**:
1. Identify operands with non-unit stride in innermost loops
2. Compute pack tile sizes from cost model (cache line aligned)
3. Apply `linalg::pack()` on the tensor carrier
4. The packed carrier has N+k dimensions (k = number of tiled operand dims)
5. ConvertSdeToArts reads the packed structure → stamps tiled contract

**Cost model gate**: `getLocalDataAccessCost()` > threshold — pack when
stride-N access cost exceeds pack overhead.

**Files**: `dep/SdeTensorOptimization.cpp`, `dep/SdeElementwiseFusion.cpp`,
`Passes.h`, `Compile.cpp`

---

### Phase 7: Loop Interchange at SDE Level

**New pass**: `SdeLoopInterchange` (in `dep/`)

**Current state**: Loop interchange only in ARTS LoopReordering.cpp (matmul
only, no cost model). After Phase 3D makes it defer to SDE, SDE owns this.

**API**: `linalg::interchangeGenericOp(rewriter, generic, interchangeVector)`
— permutes iterator types and indexing maps of the carrier directly.

#### 7A. N-dimensional access-cost-driven reordering

For any N-dimensional carrier:
1. Read indexing maps from the tensor-backed `linalg.generic`
2. For each adjacent dim pair, estimate cache miss cost if swapped
3. Bubble-sort dims by cost (not full permutation search)
4. Apply `interchangeGenericOp()` on the carrier
5. Permute the corresponding `scf.for` nesting to match

Use `getLocalDataAccessCost()` to weight stride-1 vs stride-N accesses.

#### 7B. Classification-specific heuristics

- **Matmul**: Move reduction dim (k) outermost when `trip_k >> trip_i*trip_j`
  to enable k-tiling in Phase 8.
- **Stencil**: Reorder so dimension with smallest halo width is outermost
  (minimizes total halo volume for distribution).

**Files**: New `lib/arts/dialect/sde/Transforms/dep/SdeLoopInterchange.cpp`
**Pipeline position**: After SdeStructuredSummaries, before SdeTensorOptimization
**Registration**: `Passes.td`, `Compile.cpp`

---

### Phase 8: Reduction Dimension Splitting

**File**: `lib/arts/dialect/sde/Transforms/dep/SdeTensorOptimization.cpp`

**Current state**: Reductions explicitly excluded (line 253).

**API**: `linalg::splitReduction(rewriter, op, controlFn)`

**Concept**: This is a DIMENSION EXPANSION specific to reductions — it
adds a new parallel dimension by splitting the reduction dim.

#### 8A. Enable reduction splitting via `linalg::splitReduction()`

For reduction-classified carriers:
1. Check if reduction trip count > `workerCount * getMinIterationsPerWorker()`
2. Compute split factor from cost model:
   `splitFactor = min(tripCount / minItersPerWorker, ceil(log2(workerCount)) + 1)`
3. Apply `splitReduction()` with control function returning the factor
4. Result: carrier gains an outer parallel dim + inner reduction dim
5. Phase 6's N-dim tiling can then tile the new parallel dimension

**Dimension change**: `{parallel, reduction}` → `{parallel, parallel, reduction}`
```
BEFORE: linalg.generic {parallel, reduction}       // sum over k
        for i: for k: C[i] += A[i,k]

AFTER:  linalg.generic {parallel, parallel, reduction}  // split k
        for i: for k_outer: for k_inner: partial[i,k_outer] += A[i, k_outer*S+k_inner]
        + linalg.generic {parallel, reduction}           // combine partials
        for i: C[i] = reduce(partial[i,:])
```

**Result type**: `SplitReductionResult` gives us `splitLinalgOp` +
`resultCombiningLinalgOp` — both are new carriers that ConvertSdeToArts will
read for contracts.

**Cost model**: New method `getReductionSplitFactor(tripCount)`.

---

### Phase 9: SDE Vectorization Hints & Dimension Collapsing for SIMD

**Attributes on `sde.su_iterate`** (consumed by ConvertSdeToArts → LLVM):

#### 9A. Vectorization width hint

For elementwise inner loops where trip count is statically known, stamp
`sde.vectorize_width` attr. Cost model: `getVectorWidth()`.

#### 9B. Inner-dim collapsing for vectorization

Use Phase 6E's collapsing capability specifically for vectorization:
when the innermost carrier dim has trip count < `getVectorWidth()`, collapse
it with the next inner dim to create a longer contiguous access vector.

This is a **targeted application** of dimension collapsing (6E) — the
decision is driven specifically by `getVectorWidth()` vs inner trip count.

#### 9C. Unroll factor hint

For small-trip-count inner loops, stamp `sde.unroll_factor` attr. Factor
derived from trip count and element byte width.

**Files**: `state/SdeStructuredSummaries.cpp` (stamp attrs), `SdeToArtsPatterns.cpp`
(forward to `arts.for` attrs), `LoopVectorizationHints.cpp` (read them)

---

## TIER 3: SYNC & EFFECT OPTIMIZATIONS (Phases 10-12)

Synchronization elimination and effect optimization at SDE level.

### Phase 10: Barrier Elimination

**New pass**: `SdeBarrierElimination` (runs after SdeReductionStrategy)

#### 10A. Prove barrier is unnecessary

For each `sde.su_barrier` between two `sde.su_iterate` ops:
1. Collect write-set of loop A (memref bases + indexing maps from analysis)
2. Collect read-set of loop B
3. If disjoint (no RAW dependency) → mark `barrier_eliminated`

**Analysis source**: Reuse `StructuredLoopSummary.reads[]/writes[]` from
`analyzeStructuredLoop()` — already computes per-loop access maps.

#### 10B. Forward `barrier_eliminated` to ConvertSdeToArts

`SdeToArtsPatterns.cpp:SuBarrierToArtsPattern` — skip creating `arts.barrier`
if attr is set.

**Cost model**: `getTaskSyncCost()` — only eliminate if barrier cost > 0
(always true in practice).

**Expected impact**: 2-15% on stencil codes with decomposed boundary/interior.

---

### Phase 11: Nowait Inference

**Enhancement to**: `ConvertOpenMPToSde.cpp`

When converting `omp.wsloop` without explicit `nowait`:
1. Check if next operation in parent region depends on this loop's output
2. If no dependency → set `nowait` on `sde.su_iterate`

**Analysis**: Compare write-set of current loop with read-set of next op.
Reuse `analyzeStructuredLoop()` infrastructure.

**Expected impact**: 3-8% by eliminating implicit barriers.

---

### Phase 12: Halo-Aware Distribution Cost

**Enhancement to**: `SdeDistributionPlanning.cpp`

#### 12A. Use `getHaloExchangeCostPerByte()` in distribution decision

For stencil loops, estimate halo volume from neighborhood offsets:
```
haloVolume = sum_over_dims(max_offset - min_offset) * tripCount_other_dims * elemSize
haloCost = haloVolume * getHaloExchangeCostPerByte()
```
Scale `hasEnoughDistributedWork()` threshold by `1 + haloCost/computeCost`.

#### 12B. Use `getRemoteDataAccessCost()` for scope selection

In `SdeScopeSelection.cpp`, when deciding local vs distributed, factor in
remote access cost for data that won't be local:
```
distributedCost = estimatedRemoteAccesses * getRemoteDataAccessCost()
localCost = tripCount * getLocalDataAccessCost()
```

---

## TIER 4: DATA/STATE OPTIMIZATIONS (Phases 13-15)

### Phase 13: Dependency Distance Stamping

**Enhancement to**: `SdeStructuredSummaries.cpp`

After stamping classification + neighborhood, also compute and stamp
per-dimension loop-carried dependency distance:

```
sde.su_iterate (...)
    dependency_distance = [0, 2]  // dim 0 parallel, dim 1 has distance-2 dep
```

Source: `StructuredLoopSummary.reads[]/writes[]` + affine analysis from
`extractDimOffset()`. A dimension is parallel if no read accesses the
same dim with a non-zero offset that matches a write dimension.

**Downstream use**: Distribution planning (don't distribute if distance < block),
schedule selection (prefer static if provably independent).

---

### Phase 14: Data Reuse Distance Estimation

**Enhancement to**: `SdeStructuredSummaries.cpp`

Compute temporal reuse distance per operand from access maps. Stamp as
`sde.reuse_footprint_bytes` attr. This drives tile size selection in
`SdeTensorOptimization` — tile should keep reuse footprint in L2 cache.

**Cost model**: New `getL2CacheSize()` method in `SDECostModel`.

---

### Phase 15: In-Place Operation Detection

**Enhancement to**: `SdeStructuredSummaries.cpp`

Detect when output operand safely overwrites an input (write map is identity
on output dims, no other reader aliases the output). Stamp `in_place_safe`
attr. `ConvertSdeToArts` can elide temporary allocation.

**Analysis**: Check write indexing map vs read indexing maps. If write map is
projected-permutation and no read aliases the write memref, safe.

---

## SDECostModel Extensions Required

Current interface (14 methods). Methods to add for Tiers 2-4:

| Method | Used By | Value |
|--------|---------|-------|
| `getVectorWidth()` | Phase 6E (collapse gate), 9A (vec hint) | SIMD lanes (e.g., 4/8/16) |
| `getCacheLineSize()` | Phase 6G (pack alignment) | bytes (e.g., 64) |
| `getL1CacheSize()` | Phase 6F (innermost tile sizing) | bytes |
| `getL2CacheSize()` | Phase 6F (expand gate), 14 (reuse tiling) | bytes |
| `getReductionSplitFactor(trip)` | Phase 8 (reduction splitting) | factor |

**Source for new values**: `AbstractMachine` needs extension OR hardcode in
`ARTSCostModel` (vector width from target triple, cache from DLTI spec or
constants). GPU path already queries `getGpuMaxMemory()` — same pattern.

Other methods currently unused but available for Tiers 2-4:
- `getLocalDataAccessCost()` → Phase 6B (stencil tile), Phase 6G (pack gate), Phase 7 (interchange)
- `getRemoteDataAccessCost()` → Phase 12B (scope cost)
- `getHaloExchangeCostPerByte()` → Phase 12A (halo distribution)
- `getTaskCreationCost()` → Phase 10 (barrier elimination threshold)

---

## Execution Order

```
TIER 0 (directory + conversion cleanup):
  Phase -1: implemented                            ← SDE FOLDER STRUCTURE
  Phase -2: implemented                            ← DEAD CODE REMOVAL
  Phase -3: cleanup existing ArtsToLLVM / RtToLLVM boundary
            (do not delete the top-level ArtsToLLVM stage)

TIER 1 (immediate — bugs, analysis, contracts):
  Phase 0: implemented                         ← ROOT FIX
  Phase 3: implemented                         ← ARTS trusts SDE
  Phase 1: implemented                         ← Reduction correctness
  Phase 2: implemented                         ← Distribution completeness
  Phase 4: implemented/partially exhausted     ← Cleanup
  Phase 5: fill the remaining coverage gaps    ← Coverage

TIER 2 (compute — dimension transforms + loop transforms):
  Phase 6: 6A-6D → tests → commit            ← N-dim tiling + fusion gate
           6E → 6F → tests → commit          ← Dim collapse + expand
           6G → tests → commit               ← Data packing
  Phase 7: 7A → 7B → tests → commit          ← Loop interchange (via carrier)
  Phase 8: 8A → tests → commit               ← Reduction splitting (dim expansion)
  Phase 9: 9A → 9B → 9C → tests → commit     ← Vec hints + SIMD-driven collapse

TIER 3 (sync — barrier/effect optimization):
  Phase 10: 10A → 10B → tests → commit       ← Barrier elimination
  Phase 11: tests → commit                    ← Nowait inference
  Phase 12: 12A → 12B → tests → commit       ← Halo cost distribution

TIER 4 (data — state/dependency analysis):
  Phase 13: tests → commit                    ← Dependency distance
  Phase 14: tests → commit                    ← Reuse distance
  Phase 15: tests → commit                    ← In-place detection
```

Tiers are independent after Tier 1. Within each tier, phases are sequential.
Tier 1 is ~2-3 days. Tiers 2-4 are ongoing improvement work.

## Files Modified (Summary)

### Tier 0 (Directory + Conversion Cleanup)
| File | Phase | Change |
|------|-------|--------|
| `sde/Transforms/CMakeLists.txt` | -1 | Add state/, dep/, effect/ glob patterns |
| `sde/Transforms/state/` | -1 | NEW dir: RaiseToLinalg, RaiseToTensor, SdeStructuredSummaries |
| `sde/Transforms/dep/` | -1 | NEW dir: SdeTensorOptimization, SdeElementwiseFusion, SdeIterationSpaceDecomposition |
| `sde/Transforms/effect/` | -1 | NEW dir: SdeReductionStrategy, SdeScopeSelection, SdeScheduleRefinement, SdeChunkOptimization, SdeDistributionPlanning |
| `core/Conversion/OmpToArts/` | -2 | DELETE entire directory (dead code) |
| `Passes.td` / `Passes.h` | -2 | Remove ConvertOpenMPToArts declaration |
| `core/Conversion/ArtsToLLVM/` | -3 | Keep directory; audit which patterns really belong here |
| `core/Conversion/ArtsToRt/` | -3 | Tighten the ownership boundary for structural ARTS→RT lowering |
| `rt/Conversion/RtToLLVM/` | -3 | Move only genuinely RT-owned lowering patterns |
| `Compile.cpp` | -3 | Preserve the top-level `arts-to-llvm` stage while cleaning up internals |

### Tier 1 (Immediate)
| File | Phase | Change |
|------|-------|--------|
| `StructuredOpAnalysis.cpp` | 0 | Walk scf.if + semi-perfect nests |
| `effect/SdeReductionStrategy.cpp` | 1A | Float guard on atomic |
| `ConvertOpenMPToSde.cpp` | 1B | Bitwise reduction inference |
| `EdtReductionLowering.h` | 1B | Extend enum (land/lor/lxor) |
| `EdtReductionLowering.cpp` | 1B | Bitwise combiner + identity + guard |
| `effect/SdeDistributionPlanning.cpp` | 2 | All scope/classification combos |
| `state/SdeStructuredSummaries.cpp` | 2D | Preserve non-reduction classification after tensor strip-mining |
| `StencilTilingNDPattern.cpp` | 3A | Skip on existing SDE contract |
| `MatmulReductionPattern.cpp` | 3B | Skip on existing SDE contract |
| `LoopNormalization.cpp` | 3C | Skip on existing SDE contract |
| `LoopReordering.cpp` | 3D | Skip on existing SDE contract |
| `SdeOps.td` | 4A | Remove dead SdeMuAccessOp |
| `SdeToArtsPatterns.cpp` | 4A | Remove MuAccessToArtsPattern |
| `VerifySdeLowered.cpp` | 4B | Catch all orphaned carriers |
| `SDECostModel.h` | 4C | Remove dead method |
| `CreateDbs.cpp` | 4C | Remove dead heuristic helpers |

### Tier 2 (Compute — Dimension Transforms)
| File | Phase | Change |
|------|-------|--------|
| `dep/SdeTensorOptimization.cpp` | 6A-C | N-dim tiling (all classifications) |
| `dep/SdeTensorOptimization.cpp` | 6E | Dim collapsing via `collapseOpIterationDims()` |
| `dep/SdeTensorOptimization.cpp` | 6F | Dim expansion via `tensor::ExpandShapeOp` |
| `dep/SdeTensorOptimization.cpp` | 6G | Data packing via `linalg::pack()` |
| `dep/SdeTensorOptimization.cpp` | 8 | Reduction splitting via `splitReduction()` |
| `dep/SdeElementwiseFusion.cpp` | 6D | Cost-model fusion gate |
| `dep/SdeLoopInterchange.cpp` | 7 | NEW — via `interchangeGenericOp()` on carrier |
| `state/SdeStructuredSummaries.cpp` | 9 | Vec/unroll hints |
| `SDECostModel.h` | 6E-G | Add `getVectorWidth()`, cache size methods |
| `ARTSCostModel.h` | 6E-G | Implement new methods (hardcoded or from target) |
| `Passes.td` (sde) | 7 | Register SdeLoopInterchange pass |
| `Compile.cpp` | 6D,7 | Wire new passes |

### Tier 3 (Sync)
| File | Phase | Change |
|------|-------|--------|
| `effect/SdeBarrierElimination.cpp` | 10 | NEW — data-flow barrier elimination |
| `ConvertOpenMPToSde.cpp` | 11 | Nowait inference |
| `effect/SdeDistributionPlanning.cpp` | 12 | Halo cost in threshold |
| `effect/SdeScopeSelection.cpp` | 12B | Remote access cost in scope selection |

### Tier 4 (Data)
| File | Phase | Change |
|------|-------|--------|
| `state/SdeStructuredSummaries.cpp` | 13-15 | Dependency distance, reuse, in-place attrs |
| `SDECostModel.h` | 14 | Add `getL2CacheSize()` |

### Tests
- `tests/contracts/sde/` — primary coverage for Phases 0-12
- `tests/contracts/verify/` — verifier-barrier coverage for Phase 4B
- `tests/contracts/core/` — PatternPipeline and lowering-side contract checks
- Do not freeze file counts in this summary; add/remove rows as coverage lands

## Verification

After each commit:
1. `dekk carts build` — must succeed
2. `dekk carts test` — all tests pass
3. Phase-specific checks:
   - Phase -1: pure file moves, zero code changes, build + targeted contract tests green
   - Phase 0: jacobi-like stencil with scf.if gets `classification(<stencil>)`
   - Phase 1A: float-add test selects a non-atomic strategy
   - Phase 1B: `arith.andi` reduction → `land` kind, identity all-ones
   - Phase 2: distributed elementwise/matmul/reduction preserve the intended contract and gain blocked distribution where applicable
   - Phase 3: `arts.for` with an existing effective `depPattern` is left alone by PatternPipeline
   - Phase 4A: build+test pass with SdeMuAccessOp removed
   - Phase 4B: orphaned `linalg`, `bufferization.to_tensor`, and `tensor.empty` carriers are rejected under `arts.for`
   - Phase 6: matmul gets N-D nested tile loops, stencil gets halo-aware tiles
   - Phase 10: barrier between independent loops is eliminated
