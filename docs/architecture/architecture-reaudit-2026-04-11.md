# CARTS Architecture Re-Audit

**Date**: 2026-04-11 (second pass, same day)
**Branch**: `architecture/sde-restructuring` (102 commits ahead of main, clean working tree)
**Method**: 10-agent parallel re-audit of all architecture docs against current codebase
**Prior audit**: `architecture-gap-analysis-2026-04-11.md` scored 78%; this re-audit reflects
changes committed between the two passes

## Overall Score: ~82% (up from 78%)

The branch has made material progress since the first audit snapshot. All 3
previously missing SDE ops now exist, typed semantic surfaces are live,
LoopLikeOpInterface is implemented, 3 new SDE passes move semantic planning
into the SDE tier, and reduction strategy is a fully working end-to-end pipeline.

| Dimension            | First Audit | Re-Audit | Delta | Notes |
|----------------------|-------------|----------|-------|-------|
| Structural layout    | 100%        | 100%     | --    | Unchanged |
| RT dialect           | 100%        | 100%     | --    | Unchanged |
| SDE pipeline         | 90%         | 95%      | +5    | 3 new passes (StructuredSummaries, ElementwiseFusion, DistributionPlanning) |
| SDE op design        | 45%         | 85%      | +40   | All ops exist, types defined, LoopLikeOpInterface live |
| Tensor integration   | 60%         | 75%      | +15   | Broader linalg/tensor coverage, multi-output tiling |
| Cost model           | 30%         | 50%      | +20   | 7 active consumers (6 SDE + ET-5), core still hardcoded |
| Directory moves      | 85%         | 95%      | +10   | GUIDRangeDetection alive (false positive), patterns/verify in core/ |
| Documentation        | 70%         | 90%      | +20   | 6 of 7 docs FRESH after refresh commits |
| Test architecture    | 85%         | 88%      | +3    | 261 tests, 3 verify tests, 27 cross-dialect |
| External techniques  | 60%         | 65%      | +5    | No new adoptions; existing ones better documented |
| Metadata removal     | 100%        | 100%     | --    | Clean; SkipLoopMetadataRecovery is actually active |

---

## What Changed Since the First Audit

### SDE Dialect: From Shell to Functional Semantic Layer

**All 3 previously missing ops now implemented:**

| Op | SdeOps.td Lines | Status |
|----|-----------------|--------|
| `sde.su_distribute` | 285-303 | Advisory distribution wrapper; used by SdeDistributionPlanning |
| `sde.mu_access` | 348-367 | Access region annotation; lowering erases it |
| `sde.mu_reduction_decl` | 369-394 | Reduction declaration with custom verifier (SdeOps.cpp:42-73) |

**Types defined in new SdeTypes.td (lines 17-32):**

| Type | Mnemonic | Consumers |
|------|----------|-----------|
| `CompletionType` | `!arts_sde.completion` | `SdeSuBarrierOp` operands |
| `DepType` | `!arts_sde.dep` | `SdeCuTaskOp` deps, `SdeMuDepOp` result, ConvertOpenMPToSde |

**LoopLikeOpInterface implemented on `SdeSuIterateOp`** (SdeOps.cpp:16-40):
- `getLoopRegions()`, `getLoopInductionVars()`, `getLoopLowerBounds()`,
  `getLoopUpperBounds()`, `getLoopSteps()` -- all 5 methods present

**Total SDE surface: 11 ops, 2 types, 8 enum attributes, 1 interface**

### SDE Pipeline: 12 Passes, 3 New

The openmp-to-arts stage now runs 16 passes (12 SDE + cleanup/verification):

```
 1. ConvertOpenMPToSde           OMP -> SDE boundary
 2. SdeScopeSelection            local/distributed scope from cost model
 3. SdeScheduleRefinement        auto/runtime -> static/dynamic/guided
 4. SdeChunkOptimization         cost-model chunk sizes
 5. SdeReductionStrategy         atomic/tree/local_accumulate from cost model
 6. RaiseToLinalg                linalg.generic carrier materialization
 7. RaiseToTensor                memref -> tensor conversion for carriers
 8. SdeTensorOptimization        cost-model-driven tiling/strip-mining
 9. SdeStructuredSummaries  NEW  classification + neighborhood stamps
10. SdeElementwiseFusion    NEW  fuse consecutive elementwise su_iterate
11. SdeDistributionPlanning NEW  wrap eligible ops in su_distribute
12. ConvertSdeToArts             SDE -> ARTS boundary (consume carriers, stamp contracts)
13. VerifySdeLowered             verification barrier
14. DeadCodeElimination          cleanup
15. CSE                          cleanup
16. VerifyEdtCreated             final verification
```

**SdeStructuredSummaries** (91 LOC): Stamps `structuredClassification`
(elementwise/stencil/matmul/reduction) and stencil neighborhood attributes
(min/max offsets, owner dims, spatial dims, write footprint).

**SdeElementwiseFusion** (255 LOC): Identifies consecutive sibling
elementwise `su_iterate` ops with disjoint writes and same iteration space,
fuses them, reclassifies as `elementwise_pipeline`.

**SdeDistributionPlanning** (162 LOC): Wraps eligible `su_iterate` in
`su_distribute` with cost-model-driven advisory hints. Elementwise + local
scope -> `blocked`; stencil + distributed scope -> `owner_compute`.

### Reduction Strategy: Full End-to-End Pipeline

The reduction strategy now flows from SDE selection through core lowering
to LLVM codegen with 3 distinct code paths:

```
SDE: SdeReductionStrategy (cost-model-driven selection)
  -> sde::SdeReductionStrategyAttr on sde.su_iterate
  |
SDE->ARTS: ConvertSdeToArts (preserves strategy)
  -> "arts.reduction_strategy" string attr on arts.for + parent arts.edt
  |
ARTS Core: ForLowering / EdtReductionLowering (consumes strategy)
  -> resolveReductionStrategy() reads attr
  -> switch(strategy):
       local_accumulate: scf.for with iter_args (sequential fold)
       atomic:           arts.atomic_add (int only; float falls back to linear)
       tree:             scf.for step 2 + scf.if (binary reduction tree)
  |
LLVM: ConvertArtsToLLVMPatterns
  -> arts.atomic_add -> llvm.atomicrmw add
```

**23 tests** cover the full pipeline across sde/, core/, and rt/ directories.

### Cost Model: 7 Active Consumers

| Consumer | File | What It Uses |
|----------|------|-------------|
| SdeReductionStrategy | sde/Transforms/ | `getAtomicUpdateCost()`, `getReductionCost(workers)` |
| SdeScheduleRefinement | sde/Transforms/ | `getSchedulingOverhead(kind, tripCount)` |
| SdeChunkOptimization | sde/Transforms/ | `getWorkerCount()`, `getMinIterationsPerWorker()` |
| SdeTensorOptimization | sde/Transforms/ | `getWorkerCount()`, `getMinIterationsPerWorker()` |
| SdeDistributionPlanning | sde/Transforms/ | `getMinIterationsPerWorker()`, `getNodeCount()` |
| SdeScopeSelection | sde/Transforms/ | `isDistributed()` |
| EdtTransformsPass (ET-5) | core/Transforms/ | `getAtomicUpdateCost()`, `getTaskSyncCost()` |

**Still hardcoded** (no cost model): PartitioningHeuristics (20 H1 rules),
DbHeuristics, EpochHeuristics, most of DistributionHeuristics.

### Documentation: 6 of 7 Checked Are Now FRESH

| Document | Status | Notes |
|----------|--------|-------|
| sde-dialect.md | FRESH | Correctly describes implemented vs planned |
| pipeline-redesign.md | FRESH | No collect-metadata, accurate openmp-to-arts stage |
| linalg-in-sde.md | FRESH | Correctly describes real carriers, not "diagnostic-only" |
| pass-placement.md | FRESH | All 3 new SDE passes included |
| external-techniques.md | FRESH | Legion/Chapel correctly marked "Planned" |
| cost-model.md | PARTIALLY STALE | Sections 1-2 accurate; sections 3-4 describe unimplemented convergence framework; section 5 references deleted MetadataManager |
| docs/sde.md | FRESH | Most comprehensive and up-to-date doc in the project |

### First-Audit Corrections

Several findings from the first audit were **false positives** that this
re-audit corrects:

| First Audit Claim | Re-Audit Finding |
|-------------------|------------------|
| GUIDRangeDetection is dead code | **ALIVE** -- called by DbTransformsPass.cpp:34 |
| SkipLoopMetadataRecovery is dead | **ACTIVE** -- Seidel2DWavefrontPattern needs it for tiled loops |
| MetadataAttrNames survivors are unused | **USED** -- IdRegistry.cpp:151-152 loads serialized metadata sections |
| patterns/, verify/, general/ never created | **By design** -- Phase 3 merged them into core/Transforms/ subdirs |
| RaiseToLinalg is "diagnostic-only" | **Fully functional** -- creates real linalg.generic ops (docs were stale, now refreshed) |
| Transient IR pattern undocumented | **Documented** in linalg-in-sde.md with full lifecycle |

---

## Remaining Gaps (Ranked by Impact)

### Tier 1: Design Gaps

#### 1.1 DestinationStyleOpInterface Missing on CU Ops

The documented tensor-native `ins`/`outs` operand schema is not implemented.
CU ops have no data operands -- tensor carriers are materialized in loop
bodies via RaiseToLinalg, not through op-level composition.

- **Impact**: Cannot compose with linalg tile-and-fuse infrastructure
- **Files**: `include/arts/dialect/sde/IR/SdeOps.td` (cu_region, cu_task, cu_reduce)
- **Effort**: 8-10h
- **Dependency**: None

#### 1.2 RegionBranchOpInterface Missing

`cu_region` and `cu_task` lack proper control-flow semantics for MLIR analysis.

- **Impact**: Edge case; not blocking current functionality
- **Effort**: 2-3h

#### 1.3 Cost Model Disconnected from Core Heuristics

20 hardcoded thresholds in PartitioningHeuristics. 8 in DistributionHeuristics
(beyond the stencil-strip path that does use cost model). No convergence
framework. No HeuristicOutcome tracking.

- **Impact**: Decisions are non-portable and can't self-correct
- **Files**: PartitioningHeuristics.cpp, DistributionHeuristics.cpp, DbHeuristics.h
- **Effort**: 12-16h
- **Note**: cost-model.md sections 3-4 still describe the unimplemented vision

### Tier 2: Ownership Boundary Debt

#### 2.1 Wavefront Planning Still in ARTS Core

SDE now owns classification, elementwise fusion, and advisory distribution.
But wavefront family selection, tile geometry, and frontier planning are
still synthesized in ARTS core:

| What | Where Now | Should Be |
|------|-----------|-----------|
| Wavefront family detection | StructuredKernelPlanAnalysis + EdtDistribution | SdeStructuredSummaries or new SdeWavefrontAnalysis |
| Wavefront tile geometry | Seidel2DWavefrontPattern (1200+ LOC) + DistributionHeuristics | SdeDistributionPlanning with tile geometry hints |
| Stencil ownership semantics | EdtDistribution re-analyzes after SDE | Trust SDE's scope decision |

**Files to enhance** (SDE):
- `sde/Transforms/SdeStructuredSummaries.cpp` -- add wavefront family detection
- `sde/Transforms/SdeDistributionPlanning.cpp` -- expand with tile geometry

**Files to simplify** (ARTS):
- `core/Transforms/dep/Seidel2DWavefrontPattern.cpp` -- read SDE hints instead of computing geometry
- `core/Analysis/heuristics/DistributionHeuristics.cpp` -- retire `chooseWavefront2DTilingPlan()`
- `core/Transforms/EdtDistribution.cpp` -- remove plan-driven fallback when SDE provides hints

#### 2.2 StructuredKernelPlanAnalysis Duplicates SdeStructuredSummaries

Both perform loop structure -> pattern family classification. One in ARTS
core, one in SDE. The SDE version should be authoritative; the ARTS version
should only be a fallback for ops that bypass SDE.

### Tier 3: Cleanup

#### 3.1 EdtPtrRematerialization Duplicate

Two files compile to separate .o files:
- `core/Transforms/EdtPtrRematerialization.cpp` (73 LOC, wrapper)
- `core/Transforms/edt/EdtPtrRematerialization.cpp` (241 LOC, implementation)

**Fix**: Merge pass registration into edt/ version, delete wrapper.

#### 3.2 cost-model.md Partially Stale

Sections 3-4 describe unimplemented convergence framework and unified cost
function. Section 5 references deleted MetadataManager. Should label
sections 3-4 as "Proposed Future Design" and remove MetadataManager reference.

#### 3.3 Test Coverage Gaps

| Gap | Count | Notes |
|-----|-------|-------|
| Verification pass tests | 3 of 11 | Up from 2; VerifySdeLowered and VerifyForLowered added |
| Wavefront pattern tests | 2 | Seidel-specific tests deleted during restructuring, not restored |
| CPS chain transport | 0 isolated | Only covered in full benchmark runs |
| Stencil halo indexing | Sparse | Known jacobi-halo bug still open |

#### 3.4 Output/ Directories

739K of committed lit artifacts in `tests/contracts/core/Output/` and
`tests/contracts/core/partitioning/safety/Output/`.

#### 3.5 Runtime-Agnostic SDE Claim

SDE namespace is `arts_sde`, only targets ARTS, schedule kinds map to ARTS
ForOp. No abstraction layer for Legion/StarPU/GPU backends. The claim in
docs is aspirational. External-techniques.md now correctly marks
Legion/Chapel as "Planned."

---

## Quantitative Summary

### File Counts

| Dialect | .cpp Files | Passes | Ops |
|---------|-----------|--------|-----|
| SDE | 15 | 12 | 11 |
| Core | 129 | ~47 | 18 |
| RT | 8 | 4 | 14 |

### Test Counts

| Directory | Tests | Coverage |
|-----------|-------|----------|
| sde/ | 79 | Stages 1-5 (OMP->SDE->ARTS) |
| core/ | 105 | Stages 6-16 (EDT/DB/Epoch) |
| rt/ | 45 | Stages 17-18 (lowering/LLVM) |
| verify/ | 3 | Verification barriers |
| cli/ | 9 | CLI options |
| examples/ | 32 | Integration (non-lit) |
| **Total** | **261** (+33 from first audit's 228) | |

### Branch Scope

- 782 files changed, +55,750 / -18,978 lines vs main
- 102 commits
- Clean working tree (no uncommitted changes)

---

## Prioritized Action Items (Updated)

### P0: Remaining Doc Alignment (2h)

| Action | File |
|--------|------|
| Label sections 3-4 as "Proposed Future Design" | cost-model.md |
| Remove MetadataManager reference from section 5 | cost-model.md |

### P1: SDE Interface Completion (10-13h)

| Action | Effort | Impact |
|--------|--------|--------|
| Add DestinationStyleOpInterface + ins/outs to CU ops | 8-10h | Enables linalg tile-and-fuse |
| Add RegionBranchOpInterface to cu_region, cu_task | 2-3h | Proper CF semantics |

### P2: Wavefront Ownership Migration (16-20h)

| Action | Effort | Impact |
|--------|--------|--------|
| Add wavefront family detection to SdeStructuredSummaries | 4-6h | SDE owns semantic planning |
| Add tile geometry hints to SdeDistributionPlanning | 6-8h | SDE prescribes, ARTS realizes |
| Simplify Seidel2DWavefrontPattern to consume SDE hints | 4-6h | Remove ARTS-side semantic policy |
| Retire StructuredKernelPlanAnalysis overlap | 2h | Single source of truth |

### P3: Cost Model in Core Heuristics (12-16h)

| Action | Effort | Impact |
|--------|--------|--------|
| Hook ARTSCostModel into PartitioningHeuristics | 6-8h | Replace 20 hardcoded rules |
| Hook ARTSCostModel into DistributionHeuristics | 4-6h | Cost-driven distribution |
| Add HeuristicOutcome tracking | 4h | Enable calibration |

### P4: Cleanup (3-4h)

| Action | Effort |
|--------|--------|
| Merge EdtPtrRematerialization duplicate | 30min |
| Restore wavefront pattern test coverage (2 tests) | 1h |
| Add CPS chain isolated contract tests | 2h |
| Remove committed Output/ directories | 15min |

---

## Appendix: Agent Summary

| # | Agent | Key Finding |
|---|-------|-------------|
| 1 | SDE Ops & Types | All 11 ops, 2 types, LoopLikeOpInterface -- DSI/RegionBranch still missing |
| 2 | SDE Passes | 12 passes, 3 new (StructuredSummaries, ElementwiseFusion, DistributionPlanning) |
| 3 | Cost Model | 7 consumers (6 SDE + ET-5); Partitioning/Db/Epoch still hardcoded |
| 4 | Reduction Strategy | Full e2e pipeline: SDE->ARTS->core->LLVM, 3 strategies, 23 tests |
| 5 | SDE Ownership | SDE owns classification+fusion+advisory dist; wavefront still in ARTS |
| 6 | Docs Freshness | 6/7 FRESH; cost-model.md partially stale (sections 3-4) |
| 7 | Test Coverage | 261 tests (+33), 3 verify, 27 cross-dialect; wavefront/CPS gaps |
| 8 | Directory Layout | 95% complete; GUIDRangeDetection alive (false positive); EdtPtrRemat dup |
| 9 | Transient IR | Fully implemented and documented in linalg-in-sde.md |
| 10 | Overall Score | 82.3% weighted across 11 dimensions |
