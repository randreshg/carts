# Phase 11 Pass-Split Re-Carve Plan (Workstream 3)

> Implementation plan for the Phase 11 SDE/ARTS pass-split. Source: WS3 planner
> run, 2026-06-15. Carve on the correctness base @782988ad1. The earlier
> "post-US8 prereq" is dropped: perf/US8 is being skipped per user direction, so
> all line ranges below (against @782988ad1) apply directly. Re-validate symbols
> with `git show 782988ad1:<path> | grep -n` before each carve step.

**Base:** `wt/integration-distribution-refactor` @ `782988ad1` (medium 18/21 Correct + 3 fail-closed, lit 79/79)
**Salvage template only:** v4 @ `0b9338f07` (T029/T030 + partial T028 boundary/BlockGrainPlanPass)

---

## 0. Pipeline context (correctness base)

Production SDE order in `tools/compile/Compile.cpp` `buildSdePlanningPipeline()` (@782988ad1):

```
layout-assignment -> interchange -> tiling -> raise-to-sde
-> distribution-planning
-> barrier-elimination -> memory-unit-realization -> ...
-> rank-expand-mu -> ... -> sde-redistribute -> coarse-avoidance -> verify-sde
```

v4 adds **only** `createBlockGrainPlanPass(costModel)` immediately after `distribution-planning` (line ~1154 in v4 `Compile.cpp`). Correctness base has **no** separate block-grain pass; grain unification lives inside `DistributionPlanningPass::runOnOperation` via `reconcileSameOwnerArrayGrain`.

---

## 1. Per-task carve map (correctness base @782988ad1)

### T027 - `RedistributionEdges.cpp` -> `EdgeClassify` + `RankExpandedEdgeProject`

**Parent file:** `lib/carts/dialect/sde/Analysis/RedistributionEdges.cpp` (880 lines)
**Public API today:** `include/carts/dialect/sde/Analysis/RedistributionEdges.h` - `collectRedistributionEdges`, `movementEndpointGroundedInCommittedLayout`

v4 did **not** create separate pass files; it inlined `committedRepartitionLayout` and **removed** `EdgeClassify` enum + `readerOnlyRetilesSameOwners` + `classifyRedistributionEdge` - **do not follow v4 here**; keep correctness-base A1 fail-closed.

#### 1a. `EdgeClassify` (decide movement family; A1 gate)

| Item | Detail |
|------|--------|
| New files | `lib/carts/dialect/sde/Analysis/EdgeClassify.cpp`, `include/carts/dialect/sde/Analysis/EdgeClassify.h` |
| Pass wrapper (lit) | `lib/carts/dialect/sde/Transforms/state/EdgeClassifyPass.cpp` |
| `.td` registration | `include/carts/dialect/sde/Transforms/StatePasses.td` - `def SdeEdgeClassify : Pass<"sde-edge-classify", "mlir::ModuleOp">` |
| Constructor | `createSdeEdgeClassifyPass()` in `include/carts/dialect/sde/Transforms/Passes.h` |

Move from `RedistributionEdges.cpp` anonymous namespace:

| Symbol | Lines @782988ad1 | Responsibility |
|--------|------------------|----------------|
| `enum class EdgeClassify` | 47-51 | Halo / ReduceScatter / AllToAll / None |
| `HomeLayout`, `recordHomeLayout` | 35-82 | Committed writer home |
| `findLayoutFact`, `findProfileForRoot` | 84-102 | Reader facts |
| `getCommittedHaloShape` | 104-129 | Halo shape derivation |
| `sameOwnerDimSet` | 157-165 | Owner-set equality |
| `readerCommittedLayoutDisagreesWithHome` | 428-435 | Cross-owner / grain disagreement |
| `readerOnlyRetilesSameOwners` | 437-445 | A1 same-owner re-tile detector |
| `classifyRedistributionEdge` | 447-460 | Movement-family decision |
| `readerNeedsRedistributionMovement` | 462-495 | Need-movement predicate |
| `collectReaderRedistributionCandidates` | 390-412 | Reader arrayId scan |
| `producerConsumerMuTypesDisagree` | 414-426 | MU-type disagreement |
| `committedBlockShape` | 334-338 | budgetBlockShape authority |

Move from `collectRedistributionEdges` (499-880):

| Region | Lines | Responsibility |
|--------|-------|----------------|
| Module setup + home/root maps | 499-554 | `buildModuleSuAccessRelations`, `homeByArrayId`, `rootByArrayId` |
| Reader walk + classification | 555-674 | Edge need, `hasOwnerReduction`, `committedContractionLayout`, `committedHaloLayout`, `classifyRedistributionEdge`, same-owner fail-closed (663-674) |
| Edge kind assignment (pre-projection) | 675-682 | `edge.kind` from `EdgeClassify` |

New exported API (header):

```cpp
struct ClassifiedRedistributionEdge { /* fields without final haloShape/target geometry */ };
struct EdgeClassifyResult { SmallVector<ClassifiedRedistributionEdge>; SmallVector<RedistributionEdgeFailure>; };
EdgeClassifyResult classifyRedistributionEdges(Operation *moduleOp);
```

Leave in `RedistributionEdges.cpp` temporarily: thin `collectRedistributionEdges` calling `classifyRedistributionEdges` + `projectRankExpandedEdges` until T027 step 2 lands.

#### 1b. `RankExpandedEdgeProject` (project geometry)

| Item | Detail |
|------|--------|
| New files | `lib/carts/dialect/sde/Analysis/RankExpandedEdgeProject.cpp`, `include/carts/dialect/sde/Analysis/RankExpandedEdgeProject.h` |
| Pass wrapper | `lib/carts/dialect/sde/Transforms/state/RankExpandedEdgeProjectPass.cpp` |
| `.td` registration | `StatePasses.td` - `def SdeRankExpandedEdgeProject : Pass<"sde-rank-expanded-edge-project", "mlir::ModuleOp">` |

Move symbols:

| Symbol | Lines @782988ad1 |
|--------|------------------|
| `RedistEndpoint` | 42-45 |
| `homeLayoutFromCommittedPhysical` | 54-69 |
| `getOwnerHaloRadius` | 131-155 |
| `expandHaloShapeToRootRank` | 167-192 |
| `projectRankExpandedHaloEdge` | 194-276 |
| `logicalEndpointFitsRoot` | 278-291 |
| `getRankExpandedReductionEndpoint` | 293-325 |
| `repartitionMovementReplacement` | 340-350 |
| `commitConsumerTargetGeometry` | 352-358 |
| `commitOwnerPreservingTarget` | 360-365 |
| `getRankExpandedFullEndpoint` | 367-388 |
| `collectRedistributionEdges` halo/all_to_all geometry block | 683-748 |
| `movementEndpointGroundedInCommittedLayout` | 751-877 |

New API:

```cpp
void projectRankExpandedEdges(ClassifiedRedistributionEdge &edge, ...);
RedistributionEdges projectAllEdges(EdgeClassifyResult &&classified);
```

#### T027 GATE

| Coverage | Lit file | Pass pipeline |
|----------|----------|---------------|
| A1 same-owner fail-closed | `lib/carts/dialect/sde/test/distribution/sde_redistribute_rejects_same_owner_retile.mlir` | Existing; add `sde-edge-classify` variant |
| Cross-owner all_to_all | `sde_redistribute_all_to_all.mlir` | EdgeClassify + project + redistribute |
| Halo | `sde_redistribute_rank_expanded_halo_owner_order.mlir`, `sde_owner_strip_ro_halo_carrier.mlir` | RankExpandedEdgeProject |
| Reduce-scatter | `sde_redistribute_reduce_scatter.mlir`, `sde_redistribute_cross_owner_reductions.mlir` | Both |
| Unrepresentable | `sde_redistribute_rejects_unrepresentable.mlir`, `sde_redistribute_ambiguous_write_result.mlir` | EdgeClassify |
| Endpoint grounding | `sde_verify_redistribute_ungrounded.mlir` | `movementEndpointGroundedInCommittedLayout` |

New lit needed:

- `sde_edge_classify_same_owner_retile_fail_closed.mlir` - `builtin.module(sde-edge-classify)` CHECK diagnostic without `sde-redistribute`
- `sde_rank_expanded_edge_project_halo.mlir` - isolated projection CHECK on `haloShape` / expanded endpoints

Production wiring (final T027): optional pipeline inserts before `sde-redistribute`; `Redistribute.cpp` calls libraries directly if passes are analysis-only wrappers.

---

### T028 remainder - `DistributionPlanning.cpp` -> four passes

**Parent:** `lib/carts/dialect/sde/Transforms/effect/distribution/DistributionPlanning.cpp` (2472 lines @782988ad1)
**`.td` group:** `include/carts/dialect/sde/Transforms/EffectPasses.td` (umbrella: `Passes.td`)

T028 partial on v4 = `BlockGrainPlanPass` only. Critical: v4 **deleted** `buildBudgetReconciledBlockGrainPlan`, `reconcileSameOwnerArrayGrain`, and `commitWriterLayoutFromCoiteratedRead`. Re-carve from **782988ad1 only**.

#### 1c. `OwnerDimSelect` (owner-dim + physical layout commit)

| Item | Detail |
|------|--------|
| New file | `lib/carts/dialect/sde/Transforms/effect/distribution/OwnerDimSelect.cpp` |
| `.td` | `EffectPasses.td` - `def OwnerDimSelect : Pass<"sde-owner-dim-select", "mlir::ModuleOp">` |

Move symbols (line ranges @782988ad1):

| Symbol block | Lines | Notes |
|--------------|-------|-------|
| `buildOwnerDimPlan` (both overloads) | 693-760 | Owner dim + block shape from output plan |
| `readStencilHaloForOwnerDim` | 154-171 | Used by stencil owner plan |
| `commitWriterLayoutFromCoiteratedRead` | 1137-1175 | A3 FEM writer inherit (keep on correctness base) |
| `selectCompatibleCoiteratedReadLayout`, `selectSingleAssignedWriteLayoutFact`, `assignedPhysicalBlockShape` | 1066-1135 | Helpers for co-iterated read |
| `commitStencilPhysicalLayout` | 1583-1683 | Stencil owner_tile commit |
| `commitUniformPhysicalLayout` | 1684-1823 | Elementwise/matmul-adjacent uniform commit |
| `commitMatmulPhysicalLayout`, `commitDirectRowMatmulPhysicalLayout` | 1824-1915 | Matmul grain (excluded from BlockGrainPlan) |
| `commitReductionTaskShape` | 1916-1967 | Reduction task shape |
| `commitInPlaceSharedStencilSerialSlice` | 1969-2002 | In-place serial slice |
| `commitPhysicalLayoutFromAssignedLayout` | 1335-1398 | Assigned-layout commit |
| `commitLoopStepRealizedReplicatedLayout` | 1203-1227 | Loop-step replicated |
| Wavefront realization block | 190-655 | `tryRealizeWavefrontSkew`, `realizeWavefrontSkew`, helpers - optional defer to MovementTagging/FailClosed boundary |

Shared utils to extract first (single job): `applyPhysicalLayoutIfRealized`, `buildLogicalWorkerSliceOrPhysical`, `physicalLayoutMatchesRealizedLoopSteps`, `hasCommittedPhysicalLayout`, `orderPhysicalOwnerDimsByLoop`, `allExternalStoresCoverOwnerDims` -> `lib/carts/dialect/sde/Utils/DistributionLayoutUtils.cpp` (new).

#### 1d. `BlockGrainPlan` (budget grain + A1 GCD reconcile) - correctness-base logic only

| Item | Detail |
|------|--------|
| New file | `lib/carts/dialect/sde/Transforms/effect/distribution/BlockGrainPlan.cpp` |
| `.td` | `EffectPasses.td` - `def BlockGrainPlan : Pass<"sde-block-grain-plan", "mlir::ModuleOp">` (copy description from v4 @0b9338f07, implementation from @782988ad1) |

Move symbols @782988ad1 (DO NOT use v4 inline `commitBudgetReconciledLayout`):

| Symbol | Lines | Notes |
|--------|-------|-------|
| `struct BlockGrainPlan` | 1400-1408 | Plan struct |
| `buildBudgetReconciledBlockGrainPlan` | 1410-1481 | Budget reconciliation |
| `commitBudgetReconciledLayout` | 1483-1492 | Physical commit wrapper |
| `reconcileSameOwnerArrayGrain` | 2157-2368 | Coprime/budget GCD fix - must stay with BlockGrainPlan |
| `alignLateOwnerShapeToExistingStep` | 1494-1529 | Budget step alignment |

Pipeline insert (after OwnerDimSelect, before MovementTagging):

```cpp
pm.addPass(sde::createOwnerDimSelectPass(costModel));
pm.addPass(sde::createBlockGrainPlanPass(costModel)); // runs reconcileSameOwnerArrayGrain at end
```

Remove from slimmed `DistributionPlanningPass`: all moved committers + `reconcileSameOwnerArrayGrain` call (line 2442).

#### 1e. `MovementTagging` (distribution wrapper + movement attrs)

| Item | Detail |
|------|--------|
| New file | `lib/carts/dialect/sde/Transforms/effect/distribution/MovementTagging.cpp` |
| `.td` | `EffectPasses.td` - `def MovementTagging : Pass<"sde-movement-tagging", "mlir::ModuleOp">` |

Move symbols:

| Symbol | Lines | Responsibility |
|--------|-------|----------------|
| `chooseDistributionKind` | 2064-2111 | blocked / owner_compute selection |
| `hasEnoughWorkForDistribution` | 2052-2062 | Work threshold |
| `struct DistributionRewrite` | 52-55 | Pending `su_distribute` wraps |
| `DistributionPlanningPass` distribute loop | 2444-2458 | Insert `sde.su_distribute` |
| Halo/movement attr stamping inside `buildBudgetReconciledBlockGrainPlan` halo loop | 1455-1470 | Per-owner halo in plan |
| Stencil offset attrs from `commitStencilPhysicalLayout` tail | (within 1583-1683) | `accessMinOffsets` / `accessMaxOffsets` / `ownerDims` |

#### 1f. `DistributionFailClosed` (A7 wavefront + capacity gates)

| Item | Detail |
|------|--------|
| New file | `lib/carts/dialect/sde/Transforms/effect/distribution/DistributionFailClosed.cpp` |
| `.td` | `EffectPasses.td` - `def DistributionFailClosed : Pass<"sde-distribution-fail-closed", "mlir::ModuleOp">` |

Move symbols:

| Symbol | Lines |
|--------|-------|
| `requiresInPlaceSelfRawWavefrontFailClosed` | 2027-2043 |
| `requiresUnimplementedStencilWavefront` | 2114-2118 |
| `emitStencilWavefrontFailClosed` | 2131-2155 |
| `formatI64Array` | 2120-2129 |
| Early fail-closed in pass loop | 2399-2403 |
| `chooseDistributionOrFailClosed` lambda fail branch | 2387-2391 |

Slim `DistributionPlanningPass` after T028: empty orchestrator or delete and replace pipeline with ordered passes in `Compile.cpp`.

#### T028 GATE

| Pass | Existing lit | New lit |
|------|--------------|---------|
| BlockGrainPlan | `sde_redistribute_rejects_same_owner_retile.mlir` (integration), `sde_writer_layout_commit_coiterated_stencil.mlir` | Port v4 `sde_block_grain_plan_budget_commit.mlir` but validate `reconcileSameOwnerArrayGrain` on stream/correlation fixture |
| DistributionFailClosed | `sde_seidel_in_place_wavefront_fail_closed.mlir` | `sde_distribution_fail_closed_wavefront.mlir` - `builtin.module(sde-distribution-fail-closed)` |
| OwnerDimSelect | `sde_writer_layout_commit_coiterated_stencil.mlir`, `sde_matmul_output_access_windows.mlir` | `sde_owner_dim_select_stencil.mlir` |
| MovementTagging | `sde_production_structural_pipeline.mlir` (full pipeline) | `sde_movement_tagging_distribute_wrap.mlir` |

Medium gate kernels tied to T028: stream, correlation, seidel-2d (fail-closed), 2mm/3mm (matmul grain exclusion).

---

### T031 - `SuLoopAccessAnalysis`, `LayoutAssignment`, `DistributedLaunchConsistency`

Per `.carts/findings/distribution-and-refactor-plan.md` §1 table.

#### 1g. `SuLoopAccessAnalysis.cpp` (1483 lines, Analysis)

**Parent:** `lib/carts/dialect/sde/Analysis/SuLoopAccessAnalysis.cpp`

| New unit | Lines to move | New file |
|----------|---------------|----------|
| `PerfectNestCollect` | 33-378 (`collectPerfectNest`, `collectInner`, `collectMemrefAccesses*`) | `PerfectNestCollect.cpp` |
| `MemrefAccessCollect` | (same block - split at `collectMemrefAccesses` boundary) | `MemrefAccessCollect.cpp` |
| `StructuredClassify` | 736-804, 1182-1240 (`classifyPattern`, `analyzeSuLoopAccesses` classify tail, `resolveStructuredClassification`, `deriveSuPattern`, `querySuPattern`) | `StructuredClassify.cpp` |
| `NeighborhoodAnalysis` | 645-681, 1242-1244 (`extractNeighborhoodAccessInfo`, `queryNeighborhoodAccessInfo`) | `NeighborhoodAnalysis.cpp` |

Keep in umbrella `SuLoopAccessAnalysis.cpp`: public wrappers (`analyzeSuLoopAccesses`, `query*`, `findContractionTilingCandidate`, `buildModuleSuAccessRelations` lines 1351-1483).

`.td`: new `include/carts/dialect/sde/Transforms/AnalysisPasses.td` (or DepPasses.td) - four verify/analysis passes only if lit requires; otherwise file split without pipeline insertion (lower risk).

#### 1h. `LayoutAssignment.cpp` (870 lines)

**Parent:** `lib/carts/dialect/sde/Transforms/dep/loop/LayoutAssignment.cpp`

| New pass | Lines | File |
|----------|-------|------|
| `ArrayAccessProfile` | Delegate to `buildModuleSuAccessRelations` (already in analysis) | Pass = thin re-export or skip |
| `LayoutCandidateChoose` | 45-325 (`enumerateCandidates`, `assignLayout`, PhaseB-C) | `LayoutCandidateChoose.cpp` |
| `WriterLayoutCommit` | 377-624 (`witnessedWriterOwnerPositions`, `inferReaderRequiredLayout`, PhaseD apply in `LayoutAssignmentPass::runOnOperation` 631-863) | `WriterLayoutCommit.cpp` |

`.td`: `DepPasses.td` - split `LayoutAssignment` into `sde-layout-candidate-choose` + `sde-writer-layout-commit`, deprecate monolithic `sde-layout-assignment`.

#### 1i. `DistributedLaunchConsistency.cpp` (676 lines)

**Parent:** `lib/carts/dialect/arts/Transforms/DistributedLaunchConsistency.cpp`
`.td`: `include/carts/passes/Passes.td`

| New pass | Lines | Symbols |
|----------|-------|---------|
| `WriterOwnerRoute` | 36-565 helpers + second `module.walk` (606-658) | `getWriterOwnerTarget`, `getConsistentWriterOwnerTarget`, route promotion |
| `EdtSplitForMixedDeps` | First `module.walk` (581-597) + T014 future split logic | Mixed local-only dep + distributed writer rejection |

Existing lit: `lib/carts/dialect/arts/test/db/distributed_launch_consistency_*.mlir` (3 files).

---

## 2. v4 @0b9338f07 salvage inventory

| File / change | Verdict | Notes |
|---------------|---------|-------|
| `SdeToArtsBoundaryCoarseSu.cpp` (191 lines) | ADAPT | Re-carve from `SdeToArtsBoundaryAccessLowering.cpp`; wire header `SdeToArtsBoundaryCoarseSu.h` |
| `SdeToArtsBoundaryRawAccessVerify.cpp` (116 lines) | ADAPT | Re-carve verify logic; add `createVerifyRawAccessCoveredPass` to `buildSdeToArtsPipeline` (v4 line ~1173) |
| `SdeToArtsBoundaryStandaloneCu.cpp` diff | ADAPT | Partial T030; re-carve on base, not merge |
| `SdeToArtsBoundaryDepAnalysis.cpp` diff | ADAPT | T029 partial; structural template |
| `EffectPasses.td` `BlockGrainPlan` def | ADAPT description only | Pass name/summary OK; body from @782988ad1 |
| `DistributionPlanning.cpp` v4 diff | DO NOT TOUCH / DO NOT MERGE | Removes `reconcileSameOwnerArrayGrain`, `buildBudgetReconciledBlockGrainPlan`, `commitWriterLayoutFromCoiteratedRead`; changes `requiresUnimplementedStencilWavefront` predicate |
| `RedistributionEdges.cpp` v4 diff | DO NOT MERGE | Removes A1 `EdgeClassify` enum path; weakens same-owner handling |
| `Redistribute.cpp` v4 diff | DO NOT MERGE | Deletes ~600 lines reduce-scatter target logic |
| `LayoutAssignment.cpp` v4 diff | DO NOT MERGE | 173 lines removed |
| `RankExpandMu.cpp`, `MuAccessWindow.cpp` v4 diffs | DO NOT MERGE | Coupled to v4 grain regression |
| `sde_block_grain_plan_budget_commit.mlir` | ADAPT | New lit template; extend with reconcile case |
| Deleted lit on v4 (7 files incl. `sde_seidel_*`, `sde_redistribute_rejects_same_owner_retile.mlir`, `sde_writer_layout_commit_*`) | DO NOT REPLICATE | v4 regressions; keep all 79 correctness-base lit files |

DistributionPlanning divergence - preservation rule:

```
CORRECTNESS (@782988ad1)                          v4 (@0b9338f07) - REJECT
-----------------------------------------------------------------------------
buildBudgetReconciledBlockGrainPlan :1410         inline commitBudgetReconciledLayout :1348
reconcileSameOwnerArrayGrain :2157                ABSENT
commitWriterLayoutFromCoiteratedRead :1137        ABSENT
coprime/budgetUnified in GrainInfo :2170+         incompatibleWithBudget logic MISSING
```

Re-carve procedure for BlockGrainPlan:

1. Copy `BlockGrainPlanPass` shell from v4 (cost model fallback, `EffectPasses.td`, `createBlockGrainPlanPass`, `Compile.cpp` insert).
2. Paste body from @782988ad1 `buildBudgetReconciledBlockGrainPlan` + `reconcileSameOwnerArrayGrain` verbatim.
3. Never import v4's simplified `commitBudgetReconciledLayout` or deleted reconcile.

---

## 3. Safe execution order (incremental, no big-bang)

Each step keeps the build compiling. Testing/validation (lit + medium + large) is deferred to the post-implementation validation phase per user direction; keep changes structurally sound so the deferred gates can pass.

| Step | Task | Depends on | Risk |
|------|------|------------|------|
| 0 | Extract `DistributionLayoutUtils.cpp` shared helpers | - | Low |
| 1 | T028d `DistributionFailClosed` pass + lit | Step 0 | Low - seidel diagnostic isolated |
| 2 | T028b `BlockGrainPlan` pass (782988ad1 body) + `Compile.cpp` insert + lit | Step 0 | High - stream/correlation |
| 3 | T028a `OwnerDimSelect` pass; slim `DistributionPlanning` | Step 2 | Medium - FEM co-iterated writer |
| 4 | T028c `MovementTagging` + remove distribute loop from planning | Step 3 | Medium |
| 5 | Delete empty `DistributionPlanning` or make no-op stub; fix `Compile.cpp` order | Step 4 | Low |
| 6 | T027a `EdgeClassify` analysis + pass + lit | Step 5 (grain unified before redistribute) | Medium - A1 diagnostics |
| 7 | T027b `RankExpandedEdgeProject` + thin `RedistributionEdges.cpp` | Step 6 | Medium - halo projection |
| 8 | T031c `DistributedLaunchConsistency` -> `WriterOwnerRoute` / `EdtSplitForMixedDeps` | Independent (ARTS layer) | Low-Medium |
| 9 | T031b `LayoutAssignment` split | Step 0 | Medium - layout facts |
| 10 | T031a `SuLoopAccessAnalysis` file split | Step 9 | Low if no pipeline change |
| 11 | T029/T030 boundary re-carve (CoarseSu, RawAccessVerify) from v4 template | Steps 1-7 stable | Separate boundary |

Pass order target in `buildSdePlanningPipeline` after T028:

```
layout-assignment
-> interchange -> tiling -> raise-to-sde
-> owner-dim-select
-> block-grain-plan          // includes reconcileSameOwnerArrayGrain
-> movement-tagging
-> distribution-fail-closed   // or run fail-closed BEFORE movement-tagging
-> barrier-elimination -> ...
-> rank-expand-mu -> ...
-> [edge-classify -> rank-expanded-edge-project]   // optional pipeline inserts
-> sde-redistribute -> ...
```

Dependency note: `reconcileSameOwnerArrayGrain` must run after all per-SU layout commits, before `rank-expand-mu` / `sde-redistribute` (correctness base comment @ lines 2154-2156, 2439-2441).

---

## 4. CMake / build touch list (each step)

- `lib/carts/dialect/sde/Analysis/CMakeLists.txt` - new analysis .cpp
- `lib/carts/dialect/sde/Transforms/CMakeLists.txt` - new effect/state .cpp
- `lib/carts/dialect/sde/Transforms/Passes.cpp` (or generated) - constructors
- `include/carts/dialect/sde/Transforms/Passes.h` - declarations
- `tools/compile/Compile.cpp` - pipeline order
- ARTS boundary CMake if T029 salvage

---

## 5. Risks, ambiguities, human call-outs

1. Pass vs analysis-only split: Plan table says "pass" but `collectRedistributionEdges` is analysis called from `Redistribute.cpp`. Decision: start with analysis file split + thin ModulePass wrappers for lit; only add IR-stamping if a pass needs mutable module state.
2. OwnerDimSelect vs MovementTagging boundary: Wavefront skew (`tryRealizeWavefrontSkew`, lines 655-683) spans fail-closed + layout. Recommend: keep skew realization in OwnerDimSelect; FailClosed only gates before skew attempt.
3. v4 `requiresUnimplementedStencilWavefront` changed predicate (`queryInPlaceSharedState` vs `requiresInPlaceSelfRawWavefrontFailClosed`). Always use @782988ad1 predicate.
4. T031 pass registration vs file-only split: `SuLoopAccessAnalysis` is on-demand analysis, not in production pipeline today. Recommend: file split first; add `.td`/lit only for `LayoutAssignment` and `DistributedLaunchConsistency` where pipeline hooks exist.
5. T014 `EdtSplitForMixedDeps`: Named in T031 table but is US6 functional work, not pure refactor. Do not conflate with `WriterOwnerRoute` split - keep rejection diagnostic in `EdtSplitForMixedDeps`; routing in `WriterOwnerRoute`.
6. Medium vs constitution §7: Phase 11 refactor steps gate on lit + medium 18/21 locally; full §7 2n checksum waits until sandbox/cluster unblocks.
7. Deleted v4 lit: v4 removed tests that correctness base relies on. Never delete `sde_seidel_in_place_wavefront_fail_closed.mlir`, `sde_redistribute_rejects_same_owner_retile.mlir`, `sde_writer_layout_commit_coiterated_stencil.mlir` during re-carve.
8. Post-base drift: Line ranges are against `782988ad1`. Re-validate symbols with `git show 782988ad1:<path> | grep -n` before each carve step.

---

## 6. Quick reference - correctness-base symbols to preserve

| A1 / A7 fix | Location @782988ad1 |
|-------------|---------------------|
| Same-owner fail-closed (no degenerate all_to_all) | `RedistributionEdges.cpp:663-674`, `classifyRedistributionEdge:447-460` |
| Grain GCD + coprime budget fix | `DistributionPlanning.cpp:2157-2368` (`reconcileSameOwnerArrayGrain`) |
| Budget block plan | `buildBudgetReconciledBlockGrainPlan:1410-1481` |
| Seidel fail-closed | `emitStencilWavefrontFailClosed:2131-2155` |
| Co-iterated stencil writer | `commitWriterLayoutFromCoiteratedRead:1137-1175` |
