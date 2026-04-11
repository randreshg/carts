# CARTS Architecture Gap Analysis

**Date**: 2026-04-11
**Branch**: `architecture/sde-restructuring`
**Method**: 10-agent parallel audit of all 15 docs in `docs/architecture/` against actual codebase

## Overall Score: ~78% Realized at Audit Time

The 3-dialect split (SDE -> ARTS core -> ARTS RT) is **structurally complete** --
directories, ops, conversion passes, and tests all exist. But the *semantic
vision* documented in the architecture -- tensor-native ops, MLIR composition,
cost-driven heuristics, runtime-agnostic SDE -- has significant gaps.

## Status Refresh After the Audit

This audit was directionally correct, but several branch-local gaps were closed
after the 2026-04-11 snapshot. Read this file as a **gap ledger with a stale
top section**, not as the exact current branch status.

### Closed Since the Audit

- `RaiseToLinalg` is no longer just a structural-classification pass for the
  supported subset. It now includes a narrow reduction-carrier slice in
  addition to the earlier elementwise and matmul carriers.
- `RaiseToTensor` broadened its empty-init proofs:
  - cast-alias read/modify/write outputs no longer degrade to `tensor.empty`
  - disjoint-write reduction-shaped carriers can now use `tensor.empty` when
    the output value is provably not read
- `SdeTensorOptimization` broadened beyond the original one-dimensional subset:
  - narrow matmul outer-dimension tiling is live
  - 2-D disjoint-write elementwise tiling is now covered
- `ConvertSdeToArts` now preserves fallback SDE stencil contracts when the loop
  reaches the boundary without a surviving transient carrier.
- The narrow SDE semantic-surface gaps are no longer completely open:
  - `!arts_sde.dep` and `!arts_sde.completion` now exist as real typedefs
  - `arts_sde.mu_dep`, `arts_sde.cu_task`, and `arts_sde.su_barrier` use the
    typed dep/completion surface instead of generic placeholders
  - `arts_sde.mu_access`, `arts_sde.mu_reduction_decl`, and
    `arts_sde.su_distribute` now exist as narrow semantic carriers
  - `arts_sde.su_iterate` now implements `LoopLikeOpInterface`
- `arts.reduction_strategy` is no longer annotation-only:
  - `ConvertSdeToArts` forwards it
  - core lowering now materially consumes it
  - `local_accumulate`, `atomic`, and `tree` produce distinct result-combine
    paths
  - odd-worker tree coverage is now present
- One core hardcoded-threshold cluster was closed:
  - `EdtTransformsPass::selectReductionStrategies()` now compares
    cost-model-derived atomic-vs-tree costs instead of using a fixed worker
    cutoff
- `docs/sde.md` has already been rewritten to match the current branch state,
  so any documentation score that implicitly counts it as stale is now too low.

### Still Real After the Audit

- The remaining SDE semantic-design gaps are narrower than at audit time:
  destination-style tensor-native operands, `RegionBranchOpInterface`, and
  broader backend-neutral semantics are still missing, but the basic typed
  dep/completion surface and the previously missing SDE ops are now present.
- Wavefront and distribution planning ownership is still inconsistent with the
  refreshed SDE boundary. `Seidel2DWavefrontPattern`,
  `DistributionHeuristics::chooseWavefront2DTilingPlan`,
  `StructuredKernelPlan*`, and `EdtDistribution` still synthesize semantic
  policy on ARTS IR. Treat this as remaining boundary debt: SDE should own
  wavefront-family selection, tile geometry, and contract formation; ARTS
  should only realize and optimize the resulting structure.
- The broader cost-model integration gaps are still real: the SDE-facing cost
  model exists and is used by SDE passes, and one core ET-5 threshold cluster
  now uses cost-model-derived atomic-vs-tree selection, but the larger core
  heuristic files still rely heavily on hardcoded thresholds.
- The directory-reorganization and cleanup work is still largely open.
- The architecture docs under `docs/architecture/` were still stale at audit
  time and several remain stale until updated individually.

| Dimension            | Completion | Notes                                     |
|----------------------|------------|-------------------------------------------|
| Structural layout    | 100%       | All 3 dialects, IREE-style dirs           |
| RT dialect           | 100%       | 14 ops, ArtsToRt, RtToLLVM, VerifyLowered |
| SDE pipeline         | 90%        | OMP->SDE->ARTS working, 9 passes          |
| SDE op design        | 70%        | Typed dep/completion, LoopLike, and the previously missing narrow ops now exist; destination-style and region-branch semantics still absent |
| Tensor integration   | 60%        | Audit-time snapshot; branch has since added broader tensor/linalg coverage |
| Cost model           | 40%        | SDE passes use it and ET-5 now consumes it for atomic-vs-tree selection, but broader core heuristics still hardcode many thresholds |
| Directory moves      | 85%        | patterns/, verify/, general/ never created |
| Documentation        | 70%        | Audit-time snapshot; `docs/sde.md` has since been refreshed |
| Test architecture    | 85%        | Audit-time snapshot; targeted downstream reduction coverage has since improved |
| External techniques  | 60%        | IREE/PaRSEC adopted; Legion/Chapel at 0%  |
| Metadata removal     | 100%       | Clean, 2 trivial dead-code remnants       |

---

## Tier 1: Critical Gaps

### 1.1 SDE Dialect Is Still Narrower Than the Original Documented Design

The SDE dialect is no longer missing its basic typed semantic surface, but it
is still narrower than the original tensor-native design sketched in the
architecture docs.

**Missing ops (documented in sde-dialect.md, not in SdeOps.td):**

| Op                     | Purpose                                        | Status          |
|------------------------|-------------------------------------------------|-----------------|
| `sde.su_distribute`    | Advisory distribution hint for work mapping     | Implemented as a narrow advisory wrapper; current lowering inlines it away |
| `sde.mu_access`        | In-body access region annotation (memref fallback) | Implemented as an annotation op; current lowering erases it |
| `sde.mu_reduction_decl`| Module-level reduction symbol with identity + combiner | Implemented as a narrow declaration op; current lowering erases it |

**Missing interfaces:**

| Interface                      | Documented On          | Status              | Impact                              |
|--------------------------------|------------------------|----------------------|--------------------------------------|
| `DestinationStyleOpInterface`  | cu_region, cu_task, cu_reduce | Zero occurrences | Cannot compose with linalg tile-and-fuse |
| `LoopLikeOpInterface`          | su_iterate             | Implemented          | Generic loop utilities can now recognize `arts_sde.su_iterate` |
| `RegionBranchOpInterface`      | cu_region, cu_task     | Not implemented      | No proper CF semantics               |
| `MemoryEffectsOpInterface`     | All CU/SU/MU           | Only RecursiveMemoryEffects | Partial                          |

**Missing types:**

| Type             | Mnemonic         | Documented Purpose                      | Actual          |
|------------------|------------------|-----------------------------------------|-----------------|
| `CompletionType` | `sde.completion` | Opaque token signaling CU completion    | Implemented as `!arts_sde.completion` |
| `DepType`        | `sde.dep`        | Typed dependency handle with mode+region | Implemented as `!arts_sde.dep` |

**Missing operand schema (ins/outs):**

All CU ops are documented with `DestinationStyleOpInterface` and `ins`/`outs`
operands (like `linalg.generic`). Actual signatures:

- `sde.cu_region`: Zero data operands -- just attributes (`kind`, `concurrency_scope`, `nowait`)
- `sde.cu_task`: Only typed dependency operands (`Variadic<DepType>:$deps`) -- no data operands
- `sde.cu_reduce`: Raw `AnyType` arguments, not `ins`/`outs` form
- `sde.su_iterate`: Only bounds/steps, no data operands

Tensor carriers are materialized by `linalg.generic` INSIDE the loop body
(via RaiseToLinalg), not through SDE op operands.

**Runtime-agnostic claim vs reality:**

The doc states SDE "could target any task-based runtime (ARTS, Legion, StarPU,
GPU)." In practice:
- Namespace is `::mlir::arts::sde`, assembly name `arts_sde`
- Only conversion target is ARTS (SdeToArtsPatterns.cpp)
- Schedule kinds map directly to ARTS ForOp
- No abstraction layer for alternative runtime targets

### 1.2 Cost Model Still Has Major Gaps In Core Heuristics

`ARTSCostModel` (include/arts/utils/costs/ARTSCostModel.h) has calibrated
cycle counts:

```
Task creation:  1800 cycles (local), 2500 (distributed)
Task sync:      3000 cycles (local), 5000 (distributed)
Halo exchange:  0.01/byte (local), 0.5/byte (distributed)
Atomic updates: 100 cycles (local), 500 (distributed)
```

At audit time this was effectively true for the core heuristic files. Since
then, the branch has also grown SDE-side consumers (`SdeScopeSelection`,
`SdeScheduleRefinement`, `SdeChunkOptimization`, `SdeReductionStrategy`,
`SdeTensorOptimization`), and `EdtTransformsPass::selectReductionStrategies()`
now compares cost-model-derived atomic-vs-tree costs instead of using a fixed
worker cutoff. The broader **core** heuristic files still rely heavily on
hardcoded if-else cascades.

**20 hardcoded thresholds found across 4 files:**

| Threshold                      | Value        | File                          |
|--------------------------------|--------------|-------------------------------|
| Tiny stencil                   | <= 8         | PartitioningHeuristics.cpp:29 |
| `kMaxElements`                 | 65,536       | PartitioningHeuristics.cpp:206 |
| `kMaxBlocks`                   | 64           | PartitioningHeuristics.cpp:207 |
| `kPreferredMinCellsPerTask`    | 32,768       | DistributionHeuristics.cpp:153 |
| `kAbsoluteMinCellsPerTask`     | 8,192        | DistributionHeuristics.cpp:173 |
| `kPreferredMaxTilesPerWorker`  | 10           | DistributionHeuristics.cpp:197 |
| `kMinOwnedOuterItersFloor`     | 8            | DistributionHeuristics.cpp:228 |
| `kAmortizationPressureStride`  | 8            | DistributionHeuristics.cpp:229 |
| `kOwnedSpanStridePenalty`      | 2            | DistributionHeuristics.cpp:230 |
| `kMaxAmortizationMultiplier`   | 4            | DistributionHeuristics.cpp:231 |
| `kStencilIterWorkEstimateCap`  | 2M (1<<21)   | DistributionHeuristics.cpp:387 |
| `minItersPerWorker` (base)     | 8            | DistributionHeuristics.cpp:391 |
| Persistent region overhead     | > 0.3        | PersistentRegionCostModel.cpp:54 |
| Persistent region benefit      | > 0.2        | PersistentRegionCostModel.cpp:55 |
| Persistent region stability    | > 0.4        | PersistentRegionCostModel.cpp:56 |
| `kSmallTaskThreshold`          | 64           | EdtTransformsPass.cpp:98      |
| `kLoopDepthMultiplier`         | 8            | EdtTransformsPass.cpp:102     |
| `kAtomicWorkerThreshold`       | 16           | EdtTransformsPass.cpp:107     |
| `kMaxOuterDBs`                 | 1,024        | DbHeuristics.h:64            |
| `kMaxDepsPerEDT`               | 8            | DbHeuristics.h:65            |

**Convergence framework**: documented as 3-phase domain-local loops with K=2
iterations. **Not implemented** -- 18 rigid pipeline stages, no convergence
detection. This is the root cause of the "passes that undo each other" pattern
(8 confirmed cases in cost-model.md).

**HeuristicOutcome tracking**: Not implemented. Decisions are fire-and-forget.

**Machine calibration**: Not implemented. Only conservative defaults.

### 1.3 Documentation Is Stale in Critical Areas

| Document               | Issue                                                  | Severity |
|------------------------|--------------------------------------------------------|----------|
| `pipeline-redesign.md` | Still documents Stage 2 (collect-metadata) -- removed  | High     |
| `pipeline-redesign.md` | Shows 5 SDE substages (3.5-3.9) -- code has 1 stage/13 passes | High |
| `pipeline-redesign.md` | Claims one-shot-bufferize at Stage 3.9 -- not implemented | High  |
| `linalg-in-sde.md`     | Says RaiseToLinalg is "diagnostic-only" -- branch has real linalg/tensor carriers and transforms | High |
| `external-techniques.md` | Claims Legion regions, Chapel DSI adopted -- 0% done | Medium   |
| `sde-dialect.md`       | Documents ins/outs, DSI, types -- none in SdeOps.td    | High     |
| `pass-placement.md`    | Missing 5 new SDE passes                               | Medium   |
| `cost-model.md`        | Describes convergence framework -- not implemented      | Medium   |
| `op-classification.md` | Missing new SDE op classifications                     | Low      |

Additional stale areas after the audit snapshot:

- This file itself now understates the current branch state in tensor
  integration, reduction-strategy lowering, and SDE boundary cleanup.
- This file also understates the current architectural priority: the major
  remaining ownership gap is not just missing APIs, but ARTS-side semantic
  planning that should migrate into SDE tensor/linalg analyses.

---

## Tier 2: Major Gaps

### 2.1 Directory Reorganization Phase 3 Incomplete

Three documented target directories were **never created**:

**`lib/arts/patterns/` (10+ files stuck in core):**
- `JacobiAlternatingBuffersPattern.cpp` -- in `core/Transforms/dep/`
- `Seidel2DWavefrontPattern.cpp` -- in `core/Transforms/dep/`
- `ElementwisePipelinePattern.cpp` -- in `core/Transforms/kernel/`
- `MatmulReductionPattern.cpp` -- in `core/Transforms/kernel/`
- `StencilTilingNDPattern.cpp` -- in `core/Transforms/kernel/`
- `PerfectNestLinearizationPattern.cpp` -- in `core/Transforms/loop/`
- `SymmetricTriangularPattern.cpp` -- in `core/Transforms/loop/`
- `PatternTransform.cpp` -- in `core/Transforms/pattern/`
- `AccessPatternAnalysis.cpp` -- in `core/Analysis/`
- `DbPatternMatchers.cpp` -- in `core/Analysis/db/`

**`lib/arts/verify/` (9 files scattered):**
- `ContractValidation.cpp` -- in `core/Transforms/`
- `VerifyDbCreated.cpp` -- in `core/Transforms/`
- `VerifyDbLowered.cpp` -- in `core/Transforms/`
- `VerifyEdtCreated.cpp` -- in `core/Transforms/`
- `VerifyEdtLowered.cpp` -- in `core/Transforms/`
- `VerifyEpochCreated.cpp` -- in `core/Transforms/`
- `VerifyEpochLowered.cpp` -- in `core/Transforms/`
- `VerifyForLowered.cpp` -- in `core/Transforms/`
- `VerifyPreLowered.cpp` -- in `core/Transforms/`

**`lib/arts/general/` (4 files stuck in core):**
- `DeadCodeElimination.cpp` -- in `core/Transforms/`
- `HandleDeps.cpp` -- in `core/Transforms/`
- `Inliner.cpp` -- in `core/Transforms/`
- `RaiseMemRefDimensionality.cpp` -- in `core/Transforms/`

**Dead code not cleaned up:**
- `GUIDRangeDetection.cpp` (.cpp + .h) marked DEAD in audit but still exists

**EdtPtrRematerialization duplicate:**
- `core/Transforms/edt/EdtPtrRematerialization.cpp` (241 LOC, implementation)
- `core/Transforms/EdtPtrRematerialization.cpp` (74 LOC, wrapper)
- Potential ODR violation if both compiled by CMake globbing

### 2.2 External Techniques: 40% Claim-vs-Reality Gap

| Technique                | Documented Claim                        | Implemented | Completeness |
|--------------------------|-----------------------------------------|-------------|--------------|
| IREE multi-dialect       | 3-dialect progressive lowering          | Yes         | 100%         |
| PaRSEC parameterized     | `sde.su_iterate` symbolic tasks         | Yes         | 100%         |
| Open Earth stencil       | Three-layer composition                 | Partial     | 70%          |
| Halide algo-schedule     | CU/SU separation                        | Conceptual  | 40%          |
| Flang deferred buffering | Stage 3.9 bufferize                     | Partial     | 50%          |
| Legion logical regions   | `SdeMemAccessAttr` encodes partitions   | No          | 0%           |
| Chapel domain maps       | Extensible distribution interface       | No          | 0%           |
| Lift/RISE functional     | Functional stencil combinators          | No          | 0%           |
| TPP micro-kernels        | BRGEMM fused kernel ops                 | No          | 0%           |
| Devito temporal blocking | `#sde.temporal_reuse` metadata          | No          | 0%           |
| Charm++ SDAG             | Declarative when-conditions             | No          | 0%           |

**Undocumented techniques actually used:**
- MLIR one-shot bufferization (partial, at Stage 3.9 concept)
- Affine analysis for dependency computation
- Polygeist C-to-MLIR frontend (critical infrastructure, not credited)

### 2.3 Pipeline Structure Mismatch

**Documentation claims 18+2 stages. Code has 17+2 stages (19 total).**

Key differences:

| Issue | Documentation | Code |
|-------|--------------|------|
| Stage 2 (collect-metadata) | Exists with 3 passes | **Removed entirely** |
| SDE->ARTS conversion | 5 substages (3.5-3.9) | **1 stage** ("openmp-to-arts") with 13 passes |
| Bufferize stage (3.9) | Explicit one-shot-bufferize | **Not implemented** as discrete stage |
| SDE passes | 5 listed | **9 actual** (missing: ScopeSelection, ScheduleRefinement, ChunkOpt, ReductionStrategy, TensorOpt) |
| Pass invocations | 69 unique | **142 total** invocations (duplicates by design) |

**"openmp-to-arts" stage actual pass list (Compile.cpp:685-697):**
```
ConvertOpenMPToSde
SdeScopeSelection
SdeScheduleRefinement
SdeChunkOptimization
SdeReductionStrategy
RaiseToLinalg
RaiseToTensor
SdeTensorOptimization
ConvertSdeToArts
VerifySdeLowered
DeadCodeElimination
CSE
VerifyEdtCreated
```

### 2.4 Test Architecture Gaps

**Overall**: 228 tests (exceeds 168 documented), well-organized per-dialect.

| Gap | Details | Impact |
|-----|---------|--------|
| Verification tests | Only **2 of 11** verify passes have dedicated tests | Regressions slip through |
| Cross-dialect tests | **Zero** tests validating SDE->ARTS->RT->LLVM contract chain | End-to-end preservation untested |
| Output/ bloat | 739K committed lit artifacts in `tests/contracts/core/Output/` | Repo size |
| Partitioning tests | Only 11 tests for complex DB partitioning stage | Edge cases uncovered |
| CLI tests | 9 tests, no error handling coverage | Minimal |

**Test distribution:**

| Directory | Count | Coverage |
|-----------|-------|----------|
| `tests/contracts/sde/` | 72 | Stages 1-5 |
| `tests/contracts/core/` | 76 | Stages 6-16 |
| `tests/contracts/rt/` | 45 | Stages 17-18 |
| `tests/contracts/verify/` | 2 | Verification passes |
| `tests/contracts/cli/` | 9 | CLI flags |
| `tests/examples/` | 32 | Integration (non-lit) |

---

## Tier 3: Minor Gaps

### 3.1 Implementation Ahead of Docs

The code has evolved past some documentation. These are "good problems" where
docs undersell reality:

- **RaiseToLinalg**: Creates real `linalg.generic` ops with indexing maps and
  iterator types (docs say "diagnostic-only, stamps attributes")
- **RaiseToTensor**: Fully functional with alias detection, smart init strategy
  selection (docs say "deferred")
- **SdeToArtsPatterns**: Extracts contracts from linalg structure at lines
  520-596 (docs don't describe this)
- **Transient IR pattern**: Linalg/tensor carriers are created inside SDE
  bodies for analysis, then erased before ARTS sees them. This is a key
  architectural decision that is **completely undocumented**:

```
SEMANTIC ANALYSIS WINDOW (Transient, SDE-internal):
  RaiseToLinalg (memref) -> RaiseToTensor (tensor) -> SDE optimizations
                         |  contract stamping  |
                     SdeToArtsPatterns (erase carriers)
                         |
STRUCTURAL LOWERING (Permanent, ARTS-visible):
  memref-based arts.for + stamped contracts -> downstream stages
```

This is NOT the IREE model (where each dialect's ops persist across stages).
It is closer to a **transient analysis sandwich**.

### 3.2 ARTS RT Dialect: Complete

All 14 ops verified in RtOps.td with proper verifiers and builders:

1. `CreateEpochOp`, 2. `WaitOnEpochOp`, 3. `EdtCreateOp`,
4. `EdtParamPackOp`, 5. `EdtParamUnpackOp`, 6. `RecordDepOp`,
7. `IncrementDepOp`, 8. `DepGepOp`, 9. `DepDbAcquireOp`,
10. `DbGepOp`, 11. `StatePackOp`, 12. `StateUnpackOp`,
13. `DepBindOp`, 14. `DepForwardOp`

- ArtsToRt conversion complete (EdtLowering creates 10 RT ops, EpochLowering creates 3)
- RtToLLVMPatterns complete (14 patterns registered, old duplicates removed)
- VerifyLowered checks both `arts.*` and `arts_rt.*` survival
- Only gaps: 2 aspirational filenames in doc (`DbLowering.cpp`, `ParallelEdtLowering.cpp`)

### 3.3 Metadata Removal: Clean

Removal is comprehensive and accurate:

- Zero instances of CollectMetadata, MetadataManager, MemrefMetadata,
  LoopMetadata in lib/arts/ or include/arts/
- Pipeline stage `collect-metadata` removed from Compile.cpp
- 161/161 tests pass post-removal

**2 dead-code remnants (non-critical):**
1. `SkipLoopMetadataRecovery` attribute set in Seidel2DWavefrontPattern.cpp:393
   but never read anywhere
2. `AttrNames::LoopMetadata::LocationKey` and
   `AttrNames::MemrefMetadata::AllocationId` constants defined in
   MetadataAttrNames.h but never used

**Documented survivors confirmed active:**
- `MetadataEnums.h`: 4 enums used by DbAllocNode, DbAcquireNode, DbAliasAnalysis
- `LocationMetadata.h`: Used by 7 files for source location tracking
- `IdRegistry.cpp`: Active utility for deterministic IR annotation

---

## Prioritized Action Items

### P0: Documentation Alignment (4-6h)

| Action | File | Change |
|--------|------|--------|
| Remove Stage 2 (collect-metadata) | pipeline-redesign.md, pass-placement.md | Delete section |
| Update SDE substages to match code | pipeline-redesign.md | 5 substages -> 1 stage/13 passes |
| Fix RaiseToLinalg description | linalg-in-sde.md | "diagnostic-only" -> "fully functional" |
| Document transient IR pattern | linalg-in-sde.md or new doc | Explain linalg->analyze->erase lifecycle |
| Add 5 new SDE passes | pass-placement.md | ScopeSelection, ScheduleRefinement, etc. |
| Mark unimplemented techniques | external-techniques.md | Add status column |
| Update SDE op spec to match reality | sde-dialect.md | Separate "implemented" vs "planned" |

### P1: SDE Semantic Design (18-24h)

| Action | Effort | Impact |
|--------|--------|--------|
| Add `DestinationStyleOpInterface` + `ins`/`outs` to CU ops | 8-10h | Enables linalg tile-and-fuse composition |
| Define `SdeTypes.td` (CompletionType, DepType) | 2-3h | Type safety for dep tokens |
| Implement `sde.mu_reduction_decl` | 6-8h | Preserves custom reduction combiners |

### P2: Cost Model Integration (14-18h)

| Action | Effort | Impact |
|--------|--------|--------|
| Hook ARTSCostModel into PartitioningHeuristics | 6-8h | Replace 20 hardcoded rules |
| Hook ARTSCostModel into DistributionHeuristics | 4-6h | Cost-driven distribution |
| Add HeuristicOutcome tracking | 4h | Enable cost model calibration |

### P3: Structural Completion (12-16h)

| Action | Effort | Impact |
|--------|--------|--------|
| Implement `sde.mu_access` | 3-4h | In-body region annotation |
| Implement `sde.su_distribute` | 3-4h | Advisory distribution hints |
| Create `patterns/`, `verify/`, `general/` dirs + move files | 4-6h | Complete Phase 3 |
| Add `LoopLikeOpInterface` to su_iterate | 3-4h | MLIR loop transform compat |

### P4: Test and Cleanup (6-8h)

| Action | Effort | Impact |
|--------|--------|--------|
| Add cross-dialect integration tests (SDE->ARTS->RT->LLVM) | 4-6h | End-to-end contract validation |
| Add verification pass tests (9 missing) | 2-3h | Catch verification regressions |
| Delete GUIDRangeDetection dead code | 15min | Cleanup |
| Delete SkipLoopMetadataRecovery + unused MetadataAttrNames | 15min | Cleanup |
| Remove committed Output/ directories | 15min | -739K repo |
| Resolve EdtPtrRematerialization duplicate | 30min | Fix potential ODR violation |

---

## Appendix A: Per-Agent Findings Summary

| Agent | Focus | Key Finding |
|-------|-------|-------------|
| 1. SDE Dialect | CU/SU/MU ops | 3 ops missing, 0 interfaces, 0 types, no ins/outs |
| 2. ARTS RT | 14 RT ops | Complete -- all ops, conversions, verification working |
| 3. Pipeline | 18 stages | Stage 2 removed, 5 new passes, bufferize not staged |
| 4. Directory Map | File locations | patterns/, verify/, general/ never created; 23+ files misplaced |
| 5. Cost Model | Thresholds | 20 thresholds hardcoded, cost model unused by heuristics |
| 6. MLIR Infra | linalg/tensor | Implementation MORE advanced than docs; transient IR pattern |
| 7. Tests | Coverage | 228 tests, but 0 cross-dialect, only 2 verification |
| 8. External Tech | Adoption | IREE/PaRSEC real; Legion/Chapel/Lift/TPP/Devito at 0% |
| 9. Impl Order | Progress | 92% overall; structural 100%, tensor 40%, docs 70% |
| 10. Metadata | Removal | Clean removal; 2 trivial dead-code remnants |

## Appendix B: Key File Paths

**SDE Dialect:**
- `include/arts/dialect/sde/IR/SdeOps.td` -- 8 ops defined (3 missing)
- `include/arts/dialect/sde/IR/SdeDialect.td` -- dialect scaffold
- `lib/arts/dialect/sde/Transforms/` -- 9 pass implementations

**ARTS RT Dialect:**
- `include/arts/dialect/rt/IR/RtOps.td` -- 14 ops defined
- `lib/arts/dialect/rt/Conversion/RtToLLVM/RtToLLVMPatterns.cpp` -- 1824 lines, 14 patterns
- `lib/arts/dialect/core/Conversion/ArtsToRt/` -- EdtLowering + EpochLowering

**Pipeline:**
- `tools/compile/Compile.cpp` -- stages 203-223, pass arrays 279-410, builders 649-950

**Cost Model:**
- `include/arts/utils/costs/ARTSCostModel.h` -- calibrated cycle costs
- `include/arts/utils/costs/SDECostModel.h` -- runtime-agnostic interface
- `lib/arts/dialect/core/Analysis/heuristics/PartitioningHeuristics.cpp` -- 20 H1 rules
- `lib/arts/dialect/core/Analysis/heuristics/DistributionHeuristics.cpp` -- distribution thresholds

**Transient IR (linalg/tensor):**
- `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp` -- 565 lines, creates linalg.generic
- `lib/arts/dialect/sde/Transforms/RaiseToTensor.cpp` -- 209 lines, memref->tensor
- `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp` -- contract stamping + carrier erasure

**Dead Code:**
- `lib/arts/dialect/core/Transforms/db/transforms/GUIDRangeDetection.cpp` -- marked dead, still exists
- `lib/arts/dialect/core/Transforms/dep/Seidel2DWavefrontPattern.cpp:393` -- SkipLoopMetadataRecovery unused
