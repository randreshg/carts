# Pass Placement & Pipeline Investigation

**Date**: 2026-04-11 (Phase 3 — deep investigation round)
**Branch**: `architecture/sde-restructuring`
**Method**: 9-agent parallel research with direct code analysis
**Prior audits**: `architecture-gap-analysis-2026-04-11.md` (78%),
`architecture-reaudit-2026-04-11.md` (82.3%)

## Purpose

This document investigates where pass **source files** should live in the
3-dialect directory tree (`sde/`, `core/`, `rt/`) based on semantic ownership
and actual dependency analysis. This is about **directory placement** — which
dialect owns each pass — not about pipeline execution order.

The guiding principle: each pass belongs in the dialect directory that owns
its **semantic concern**, provided its code dependencies allow it. When
dependencies force a pass to stay in a higher dialect, the semantic decision
should still be owned upstream via contract attributes.

---

## Part 1: Dependency Analysis

### Current Dependency Rules

```
sde/Transforms/     → sde/IR, sde/Analysis, arts/utils, arts/Dialect.h
                       (ZERO deps on core/Analysis or core/Transforms)

core/Transforms/     → core/IR, core/Analysis, arts/utils, arts/Dialect.h
                       (can use all core infrastructure)

rt/Transforms/       → rt/IR, arts/utils, arts/codegen, arts/Dialect.h
                       (ZERO deps on core/Analysis — only codegen + utils)
```

A pass CAN live in `sde/` if it:
1. Only includes `sde/*`, `arts/utils/*`, `arts/Dialect.h`, standard MLIR
2. Does NOT include `core/Analysis/*` or `core/Transforms/*`
3. Does NOT use `arts::ForOp`, `arts::EdtOp`, `arts::EpochOp`, `arts::DbAllocOp`

A pass CAN live in `rt/` if it:
1. Only includes `rt/*`, `arts/utils/*`, `arts/codegen/*`, standard MLIR
2. Does NOT include `core/Analysis/*`

---

## Part 2: Per-Pass Directory Verdicts

### 2.1 RaiseMemRefDimensionality

| | |
|---|---|
| **Current location** | `core/Transforms/RaiseMemRefDimensionality.cpp` |
| **User position** | "doesn't belong in core" |
| **ARTS ops used** | `arts::OmpDepOp` (creates) — nothing else |
| **Non-MLIR deps** | `polygeist/Ops.h`, `arts/utils/*`, `arts/Dialect.h` |
| **Core Analysis deps** | NONE |

**Verdict: MOVE to `core/Conversion/PolygeistToArts/`**

This is a frontend ingress conversion — it normalizes Polygeist output
(`double**` → `memref<?x?xf64>`) and wraps OMP task deps in `arts.omp_dep`.
It's a conversion from Polygeist+OMP IR into normalized ARTS input IR, not a
core structural transform. The `Conversion/PolygeistToArts/` subdirectory
is the correct IREE-style home for a cross-dialect conversion pass.

It cannot move to `sde/` because it creates `arts::OmpDepOp` (core dialect),
and it runs before SDE exists.

---

### 2.2 KernelTransforms

| | |
|---|---|
| **Current location** | `core/Transforms/KernelTransforms.cpp` + `kernel/` |
| **User position** | "must be part of SDE" |
| **ARTS ops used** | `arts::ForOp` (walks, matches) |
| **Core Analysis deps** | `AnalysisManager.h`, `AnalysisDependencies.h` |
| **Sub-patterns** | ElementwisePipeline, StencilTilingND, MatmulReduction |

**Verdict: SPLIT ownership**

The semantic **decisions** belong in SDE. The structural **realizations** stay
in core. Concretely:

| Sub-pattern | Where it belongs | Why |
|-------------|-----------------|-----|
| ElementwisePipeline | **DELETE** — `SdeElementwiseFusion` replaces it | Fully redundant with SDE |
| StencilTilingND | **core/Transforms/kernel/** | Computes halo geometry from ARTS ForOp; reads SDE contracts |
| MatmulReduction | **core/Transforms/kernel/** | Structural rewrite on ARTS ForOp; SDE stamps classification |

**Cannot move to `sde/`**: Uses `arts::ForOp`, `core/Analysis/AnalysisManager`.
SDE has zero core/Analysis deps and this must be preserved.

**SDE already owns the semantic decisions**: `SdeStructuredSummaries` stamps
classification, `SdeElementwiseFusion` handles fusion, `RaiseToLinalg`
detects matmul. KernelTransforms should become a **contract consumer** that
reads SDE-stamped attributes, not a re-discoverer.

---

### 2.3 DepTransforms

| | |
|---|---|
| **Current location** | `core/Transforms/DepTransforms.cpp` + `dep/` |
| **User position** | "must be part of SDE" |
| **ARTS ops used** | `EdtOp` (match+create), `ForOp` (match+create), `EpochOp` (CREATE) |
| **Core Analysis deps** | `AnalysisManager`, `DbAnalysis`, `DistributionHeuristics`, `PartitioningHeuristics` |

**Verdict: SPLIT semantic classification → SDE, structural rewriting → core**

The source files **cannot move to `sde/`** — they literally create
`arts::EpochOp` and `arts::EdtOp`, which don't exist in the SDE dialect, and
use `DbAnalysis`/`DistributionHeuristics` from core/Analysis.

But the user's intuition IS architecturally correct: "this is a wavefront" and
"this uses alternating buffers" are **semantic classifications** that SDE should
own. The fix is not to move files, but to split concerns:

1. **Add to SDE** (`sde/Transforms/SdeStructuredSummaries.cpp`):
   - Wavefront family detection (in-place 2D stencil → `classification(wavefront)`)
   - Jacobi alternating-buffer detection (copy+stencil pattern → `classification(jacobi_alternating)`)
   - Tile geometry hints on `su_distribute`

2. **Keep in core** (`core/Transforms/dep/`):
   - `Seidel2DWavefrontPattern` reads SDE hints, does structural rewrite
   - `JacobiAlternatingBuffersPattern` reads SDE hints, does structural rewrite

3. **Simplify core passes**: From "detect + rewrite" to "read contract + rewrite"

---

### 2.4 Inliner

| | |
|---|---|
| **Current location** | `core/Transforms/Inliner.cpp` |
| **User position** | "should happen before SDE" |
| **ARTS ops used** | NONE (uses `func::CallOp`, standard MLIR inlining) |
| **Core Analysis deps** | NONE |

**Verdict: CORRECT LOCATION — stays in `core/Transforms/`**

The Inliner is a pre-SDE utility pass. It already runs at Stage 1, before SDE
exists. It doesn't belong in `sde/` because it has nothing to do with SDE
semantics — it's generic ARTS-policy-driven function inlining. It doesn't
belong in `rt/` because it runs early.

Could theoretically be in `utils/` since it has zero ARTS op deps, but it
carries ARTS-specific policy (OMP-aware inlining decisions), so `core/` is
the right home.

---

### 2.5 LoopReordering

| | |
|---|---|
| **Current location** | `core/Transforms/LoopReordering.cpp` |
| **User position** | "should happen in SDE using tensor, linalg and other analysis" |
| **ARTS ops used** | `arts::ForOp` (walks at line 61, matches) |
| **Core Analysis deps** | `AnalysisManager`, `DbPatternMatchers` (`detectMatmulInitReductionLoopNest`) |

**Verdict: STAYS in `core/Transforms/` TODAY — future SDE pass**

**Cannot move to `sde/` today**: Uses `arts::ForOp` and `DbPatternMatchers`
from core/Analysis. Both are hard dependencies.

**User's vision is the correct future state**: Loop interchange for matmul
should be a `linalg.interchange` transform on transient linalg carriers in
the SDE tensor window. This requires:
- DestinationStyleOpInterface on CU ops (Tier 1 gap)
- RaiseToLinalg to generate matmul carriers (partially exists)
- New `SdeLoopInterchange` pass operating on `linalg.generic`

When SDE handles interchange at the linalg level, the ARTS-side
`LoopReordering` becomes unnecessary. Until then, it must stay in core.

---

### 2.6 ScalarReplacement

| | |
|---|---|
| **Current location** | `core/Transforms/ScalarReplacement.cpp` |
| **User position** | "should happen in rt" |
| **ARTS ops used** | ZERO — only `scf::ForOp`, `memref`, `arith` |
| **Core Analysis deps** | NONE |
| **Non-MLIR deps** | `arts/utils/LoopInvarianceUtils.h`, `polygeist/Ops.h` |

**Verdict: MOVE to `rt/Transforms/`**

The user is right. ScalarReplacement:
- Has **zero ARTS dialect deps** — only standard MLIR ops
- Runs at Stage 16 (`pre-lowering`), after EdtLowering (which IS in ArtsToRt/)
- Peers with `DataPtrHoisting` which IS in `rt/Transforms/`
- Both are post-lowering codegen optimizations at the RT tier
- Its deps (`arts/utils/*`, `polygeist/Ops.h`) are already used by RT passes

The `rt/Transforms/` directory is for post-lowering optimizations that improve
codegen quality. ScalarReplacement (memref→register promotion for
vectorization) is exactly that. DataPtrHoisting is its peer.

---

### 2.7 StencilBoundaryPeeling

| | |
|---|---|
| **Current location** | `core/Transforms/StencilBoundaryPeeling.cpp` |
| **ARTS ops used** | Only `arts::createConstantIndex()` (a utility function) |
| **Core Analysis deps** | NONE |
| **Actual IR ops** | `scf::ForOp`, `scf::IfOp`, `memref`, `arith` |

**Verdict: Borderline — could move to `sde/Transforms/` or stay in core**

StencilBoundaryPeeling is nearly SDE-compatible:
- Zero ARTS op deps (just a utility helper `createConstantIndex`)
- Zero core/Analysis deps
- Operates on `scf::ForOp` + `scf::IfOp` (both exist inside SDE bodies)

**However**, it currently runs AFTER SDE→ARTS conversion (Stage 4, pattern
pipeline). Moving it to SDE would mean running it before conversion, which
changes the semantics — SDE bodies might not yet have their final loop form.

**Recommendation**: Stay in `core/Transforms/` for now. If SDE gains pre-
conversion loop normalization, this could move. Low priority.

---

### 2.8 LoopNormalization

| | |
|---|---|
| **Current location** | `core/Transforms/LoopNormalization.cpp` + `loop/` |
| **ARTS ops used** | `arts::ForOp` in `SymmetricTriangularPattern` (line 64) |
| **Core Analysis deps** | `AnalysisManager`, `DbPatternMatchers` |

**Verdict: STAYS in `core/Transforms/loop/`**

`SymmetricTriangularPattern` directly matches `arts::ForOp` and uses
`DbPatternMatchers`. Cannot move to `sde/` with current deps.

**However**, `PerfectNestLinearizationPattern` uses only `scf::ForOp` — this
sub-pattern could be extracted to `utils/` or `sde/` if needed.

---

## Part 3: Summary — Where Files Belong

### Moves

| Pass | From | To | Blocker |
|------|------|----|---------|
| RaiseMemRefDimensionality | `core/Transforms/` | `core/Conversion/PolygeistToArts/` | None |
| ScalarReplacement | `core/Transforms/` | `rt/Transforms/` | None |
| ElementwisePipeline | `core/Transforms/kernel/` | **DELETE** | None (SDE replaces it) |

### Split Ownership (source stays, semantic ownership moves to SDE)

| Pass | Source stays in | Semantic ownership → SDE | Action |
|------|---------------|-------------------------|--------|
| KernelTransforms | `core/Transforms/kernel/` | `SdeStructuredSummaries` stamps contracts | Make StencilTiling/Matmul read SDE contracts |
| DepTransforms | `core/Transforms/dep/` | `SdeStructuredSummaries` classifies wavefront/Jacobi | Make patterns read SDE contracts instead of re-detecting |
| LoopReordering | `core/Transforms/` | Future: `SdeLoopInterchange` via linalg | Needs DestinationStyleOpInterface first |

### Stays (correct location)

| Pass | Location | Reason |
|------|----------|--------|
| Inliner | `core/Transforms/` | Pre-SDE utility with ARTS inlining policy |
| LoopNormalization | `core/Transforms/loop/` | Uses `arts::ForOp` + `DbPatternMatchers` |
| StencilBoundaryPeeling | `core/Transforms/` | Correct post-conversion placement; low priority |

---

## Part 4: Pipeline Ordering Analysis

*(Pipeline ordering is SEPARATE from directory placement. A pass can live in
`sde/` but run after `ConvertSdeToArts`. A pass can live in `rt/` but run
before `EpochLowering`. Directory = ownership. Pipeline = execution.)*

### 4.1 Pipeline Structure

```
Stage  1: raise-memref-dimensionality  [8 passes]  Frontend normalization
Stage  2: initial-cleanup              [3 passes]  Affine lowering + canonicalization
Stage  3: openmp-to-arts               [16 passes] SDE: OMP→SDE→ARTS
Stage  4: pattern-pipeline             [6 passes]  ARTS fallback patterns
Stage  5: edt-transforms               [6 passes]  EDT structural cleanup
Stage  6: create-dbs                   [9 passes]  DB materialization
Stage  7: db-opt                       [4 passes]  DB mode tightening
Stage  8: edt-opt                      [4 passes]  EDT optimization
Stage  9: concurrency                  [4 passes]  Parallelism decisions
Stage 10: edt-distribution             [5 passes]  Distribution + ForLowering
Stage 11: post-distribution-cleanup    [6 passes]  Structural cleanup
Stage 12: db-partitioning              [3 passes]  Block partitioning
Stage 13: post-db-refinement           [6 passes]  DB/EDT refinement
Stage 14: late-concurrency-cleanup     [6 passes]  Strip mining + hoisting
Stage 15: epochs                       [5 passes]  Epoch creation + CPS
Stage 16: pre-lowering                 [14 passes] EDT/DB/Epoch lowering
Stage 17: arts-to-llvm                 [11 passes] ARTS→LLVM conversion
Stage 18: post-o3-opt                  [6 passes]  Epilogue (conditional)
Stage 19: llvm-ir-emission             [8 passes]  Final LLVM emission
```

### 4.2 Redundancy Analysis

| Pass | Count in builders | Notes |
|------|-------------------|-------|
| PolygeistCanonicalize | 14x | Expected — cleanup after each transformation |
| CSE | 11x | Expected — cleanup after each transformation |
| EdtStructuralOpt | 5x | TODO at line 812: "evaluate if second call needed" |
| DCE | 5x | Expected |
| Mem2Reg | 4x | Expected |
| DataPtrHoisting | 2x | Intentional: pre and post ARTS→LLVM |
| EdtAllocaSinking | 2x | TODO at line 877: evaluate if stage-14 instance is sufficient |
| LowerAffine | 2x | Stages 1-2; ensures affine fully lowered before SDE |

### 4.3 Affine Re-Raising — NOT RECOMMENDED

**Investigated**: Should SDE see `affine.for`/`affine.load` instead of
reconstructing `AffineExpr` from arith ops?

**Verdict: NO** — three blockers:

1. **AffineScope incompatibility**: `affine.for` requires an `AffineScope`
   ancestor (`func.func`). OMP regions lack this trait. Polygeist's
   `RaiseToAffine.cpp:66-75` explicitly rejects loops inside OMP ops.

2. **Delaying LowerAffine**: Even moving `LowerAffine` to after SDE→ARTS
   (instead of Stage 1-2) hits the same blocker — OMP regions still lack
   `AffineScope`.

3. **Linalg carriers already provide what's needed**: `linalg.generic`'s
   `indexing_maps` captures the same affine indexing information. SDE already
   gets this via `RaiseToLinalg`.

**Alternative**: Strengthen `StructuredOpAnalysis.tryGetAffineExpr()` — expand
pattern coverage (divisions, modulo), cache results.

### 4.4 Pattern-Pipeline Stage — TRANSITIONAL, SHRINKING

The `buildPatternPipeline` comment (Compile.cpp:716-722) explicitly says:

```
/// Transitional ARTS-side fallback pipeline for semantic families that are not
/// yet fully planned in SDE before DB creation.
```

**Current absorption status**:

| Pass | SDE equivalent? | Action |
|------|-----------------|--------|
| DepTransforms | Semantic classification → SDE | SdeStructuredSummaries absorbs family detection |
| LoopNormalization | No SDE equivalent | Stays (uses arts::ForOp) |
| StencilBoundaryPeeling | No SDE equivalent | Stays |
| LoopReordering | Future: linalg.interchange | Blocked by DSI |
| KernelTransforms (Ewise) | `SdeElementwiseFusion` | **DELETE — redundant** |
| KernelTransforms (Stencil) | `SdeStructuredSummaries` (partial) | Stays (halo geometry) |
| KernelTransforms (Matmul) | `RaiseToLinalg` (classification) | Stays (structural rewrite) |

### 4.5 Pipeline Ordering Correctness

Dependency graph is correctly structured. No ordering anomalies found.
Pre-lowering correctly has dual dependency on epochs + late-concurrency-cleanup.

---

## Part 5: Prioritized Actions

### Tier 0: File Moves (mechanical, no semantic change)

| # | Action | Effort | Risk |
|---|--------|--------|------|
| 1 | Move `RaiseMemRefDimensionality.cpp` → `core/Conversion/PolygeistToArts/` | 1-2h | None |
| 2 | Move `ScalarReplacement.cpp` → `rt/Transforms/` | 1-2h | None |
| 3 | Delete `ElementwisePipelinePattern.cpp` + disable in `KernelTransforms.cpp` | 1h | Low |

### Tier 1: Semantic Ownership Shifts

| # | Action | Effort | Risk |
|---|--------|--------|------|
| 4 | Add wavefront/Jacobi classification to `SdeStructuredSummaries` | 4-6h | Low |
| 5 | Make DepTransforms read SDE contracts (not re-detect) | 3-4h | Medium |
| 6 | Make KernelTransforms read SDE contracts (not re-detect) | 2-3h | Low |

### Tier 2: Future Directory Moves (blocked)

| # | Action | Blocker | Effort |
|---|--------|---------|--------|
| 7 | New `SdeLoopInterchange` pass replaces LoopReordering | DestinationStyleOpInterface | 8-10h |
| 8 | StencilBoundaryPeeling → sde/ | SDE pre-conversion normalization | 3-4h |

### Pipeline Cleanup

| # | Action | Effort |
|---|--------|--------|
| 9 | Evaluate EdtStructuralOpt 5x (TODO at line 812) | 2h |
| 10 | Evaluate EdtAllocaSinking 2x (TODO at line 877) | 1h |

---

## Appendix: Agent Summary

| # | Agent | Key Finding |
|---|-------|-------------|
| 1 | RaiseMemRefDimensionality | Frontend bridge; only creates OmpDepOp; move to Conversion/ |
| 2 | KernelTransforms | ElementwisePipeline redundant with SDE; StencilTiling/Matmul must stay core |
| 3 | DepTransforms | Creates EpochOp/EdtOp; 10+ DbAnalysis calls; semantic detection → SDE, rewriting → core |
| 4 | Inliner | Already Stage 1; OMP-aware; correct location |
| 5 | LoopReordering | Uses arts::ForOp + DbPatternMatchers; future SDE via linalg.interchange |
| 6 | ScalarReplacement | Zero ARTS deps; runs at RT tier next to DataPtrHoisting; move to rt/ |
| 7 | Affine Re-Raising | AffineScope blocks OMP; linalg carriers provide equivalent; NOT recommended |
| 8 | Pattern-Pipeline | Explicitly transitional; ElementwisePipeline retirable now |
| 9 | Pipeline Ordering | Dependency graph correct; redundancies expected (CSE/Canonicalize) |
