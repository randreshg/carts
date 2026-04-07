# CARTS Pass Infrastructure Audit

**Date**: 2026-04-07
**Scope**: `lib/arts/passes/` (60 files, 35,429 LOC, 56 passes)

## Executive Summary

The CARTS pass infrastructure has strong foundations — perfect header/implementation
consistency (56/56), correct pipeline wiring, and well-organized domain directories.
However, organic growth has introduced: 2 misplaced files, 1 dual-pass file,
10 god files over 1000 LOC (52% of all pass code), and minor verification
duplication.

---

## 1. Current Directory Structure

```
lib/arts/passes/
├── transforms/           21 files  — lowering, conversion, verification
│   ├── CollectMetadata.cpp          (307 LOC)   stage 2
│   ├── ConvertOpenMPToArts.cpp      (867 LOC)   stage 4
│   ├── PatternDiscovery.cpp         (519 LOC)   stage 5
│   ├── LoopReordering.cpp           (442 LOC)   stage 5  [MISPLACED]
│   ├── EdtPtrRematerialization.cpp  (74 LOC)    stage 6
│   ├── CreateDbs.cpp                (1688 LOC)  stage 7  [GOD FILE]
│   ├── DistributedHostLoopOutlining.cpp (465 LOC) stage 7
│   ├── Concurrency.cpp              (302 LOC)   stage 10
│   ├── ForOpt.cpp                   (148 LOC)   stage 10
│   ├── EdtDistribution.cpp          (176 LOC)   stage 11
│   ├── ForLowering.cpp              (1632 LOC)  stage 11 [GOD FILE]
│   ├── CreateEpochs.cpp             (220 LOC)   stage 16
│   ├── ParallelEdtLowering.cpp      (317 LOC)   stage 17
│   ├── DbLowering.cpp               (552 LOC)   stage 17
│   ├── EdtLowering.cpp              (2107 LOC)  stage 17 [GOD FILE]
│   ├── EpochLowering.cpp            (2016 LOC)  stage 17 [GOD FILE]
│   ├── ConvertArtsToLLVM.cpp        (2822 LOC)  stage 18 [GOD FILE]
│   ├── LoweringContractCleanup.cpp  (44 LOC)    stage 18
│   └── Verify*.cpp                  (11 files, 34-193 LOC each)
│
├── opt/
│   ├── general/          4 files
│   │   ├── RaiseMemRefDimensionality.cpp (2108 LOC)  [GOD FILE]
│   │   ├── DeadCodeElimination.cpp       (493 LOC)
│   │   ├── Inliner.cpp                   (420 LOC)
│   │   └── HandleDeps.cpp                (238 LOC)
│   │
│   ├── db/               8 files
│   │   ├── DbPartitioning.cpp         (3018 LOC)  [GOD FILE - LARGEST]
│   │   ├── BlockLoopStripMining.cpp   (1559 LOC)  [GOD FILE]
│   │   ├── DbModeTightening.cpp       (734 LOC)
│   │   ├── DbTransformsPass.cpp       (467 LOC)
│   │   ├── DbScratchElimination.cpp   (363 LOC)
│   │   ├── ContractValidation.cpp     (344 LOC)   [DUPLICATE CHECKS]
│   │   ├── DbDistributedOwnership.cpp (102 LOC)
│   │   └── (7 files total)
│   │
│   ├── edt/              5 files
│   │   ├── EdtStructuralOpt.cpp       (984 LOC)   [2 PASSES IN 1 FILE]
│   │   ├── EdtTransformsPass.cpp      (967 LOC)
│   │   ├── PersistentStructuredRegion.cpp (191 LOC)
│   │   ├── StructuredKernelPlanPass.cpp   (138 LOC)
│   │   └── EdtICM.cpp                    (82 LOC)
│   │
│   ├── epoch/            5 files + 1 header (IN-PROGRESS SPLIT)
│   │   ├── EpochOpt.cpp              (315 LOC)   driver
│   │   ├── EpochOptCpsChain.cpp      (1166 LOC)  [GOD FILE]
│   │   ├── EpochOptStructural.cpp    (413 LOC)
│   │   ├── EpochOptScheduling.cpp    (329 LOC)
│   │   ├── EpochOptSupport.cpp       (204 LOC)
│   │   └── EpochOptInternal.h        (100 LOC)   private contract
│   │
│   ├── codegen/          7 files
│   │   ├── DataPtrHoisting.cpp        (1776 LOC)  [GOD FILE]
│   │   ├── LoopVectorizationHints.cpp (867 LOC)
│   │   ├── Hoisting.cpp              (540 LOC)
│   │   ├── AliasScopeGen.cpp         (485 LOC)
│   │   ├── ScalarReplacement.cpp     (352 LOC)
│   │   ├── GuidRangCallOpt.cpp       (221 LOC)
│   │   └── RuntimeCallOpt.cpp        (149 LOC)
│   │
│   ├── loop/             3 files
│   │   ├── StencilBoundaryPeeling.cpp (364 LOC)
│   │   ├── LoopFusion.cpp            (248 LOC)
│   │   └── LoopNormalization.cpp      (112 LOC)
│   │
│   ├── kernel/           1 file
│   │   └── KernelTransforms.cpp       (103 LOC)
│   │
│   └── dep/              1 file
│       └── DepTransforms.cpp          (74 LOC)    [MISPLACED]
```

---

## 2. Issues Found

### 2.1 Misplaced Files

#### A. `DepTransforms.cpp` — `opt/dep/` should be `transforms/`

- **What**: Pass wrapper for Seidel2D wavefront and Jacobi alternating buffers
- **Why misplaced**: It's a semantic schedule rewrite (transform), not an optimization.
  Runs in stage 5 (pattern-pipeline). The actual pattern implementations already
  live correctly in `lib/arts/transforms/dep/`.
- **Root cause**: PascalCase-to-lowercase directory rename preserved wrong location.

#### B. `LoopReordering.cpp` — `transforms/` should be `opt/loop/`

- **What**: Loop interchange (j-k to k-j) for stride-1 memory access
- **Why misplaced**: Loop interchange is an optimization. Other loop passes
  (LoopFusion, LoopNormalization, StencilBoundaryPeeling) already live in `opt/loop/`.

### 2.2 Multi-Pass Files

#### `EdtStructuralOpt.cpp` — 2 passes in 1 file (984 LOC)

| Pass | LOC | Purpose |
|------|-----|---------|
| EdtStructuralOptPass | ~750 | Complex EDT structural optimization (11 methods, 7 stats) |
| EdtAllocaSinkingPass | ~7 | Simple alloca sinking into EDT regions |

EdtAllocaSinkingPass is called independently in 2 pipeline stages
(late-concurrency-cleanup, pre-lowering) but is buried inside a 984-line
file about something else. Should be its own file.

### 2.3 God Files (>1000 LOC)

| Rank | File | LOC | Core Problem |
|------|------|-----|-------------|
| 1 | DbPartitioning.cpp | 3,018 | 19 static helpers (~800 LOC) doing analysis mixed with pass |
| 2 | ConvertArtsToLLVM.cpp | 2,822 | 27 conversion patterns in one mega-file |
| 3 | RaiseMemRefDimensionality.cpp | 2,108 | 2-phase monolith (analysis + transform) |
| 4 | EdtLowering.cpp | 2,107 | 6 distinct concerns (outlining, packing, deps) |
| 5 | EpochLowering.cpp | 2,016 | Standard + continuation epoch paths mixed |
| 6 | DataPtrHoisting.cpp | 1,776 | 1400 LOC helpers; 25 LOC pass orchestration |
| 7 | CreateDbs.cpp | 1,688 | 5 distinct phases conflated |
| 8 | ForLowering.cpp | 1,632 | Loop analysis + splitting + distribution |
| 9 | BlockLoopStripMining.cpp | 1,559 | Block analysis + rewriting conflated |
| 10 | EpochOptCpsChain.cpp | 1,166 | Specialized CPS; acceptable if cohesive |

MLIR convention: 200-500 LOC per pass. Our top 10 average 1,990 LOC (4-10x over).

### 2.4 Verification Duplication

`ContractValidation.cpp` (345 LOC) duplicates 4 checks already in
`LoweringContractOp::verify()` (Dialect.cpp):

- Target is memref (lines 72-80)
- supported_block_halo requires stencil pattern (lines 83-93)
- min_offsets/max_offsets rank consistency (lines 97-103)
- owner_dims non-negative (lines 115-123)

### 2.5 Navigation Confusion

Pipeline stage `edt-transforms` maps to `opt/edt/` files, not `transforms/`.
No `transforms/EdtTransforms.cpp` or `transforms/EpochTransforms.cpp` exist
(by design, but confusing for newcomers).

---

## 3. EpochAnalysis Assessment

EpochAnalysis is **well-centralized**:

- **Heuristics**: All 6 decision methods delegate to `EpochHeuristics.cpp` (600+ LOC)
- **Facade**: `EpochAnalysis.cpp` is a thin pass-facing API
- **Registration**: Properly lazy-initialized in AnalysisManager
- **Attributes**: All 13+ CPS attributes defined as constants in `OperationAttributes.h`
- **No duplicated logic**: Passes use `AM->getEpochAnalysis()` correctly
- **No epoch graph**: Unlike DbGraph/EdtGraph, epochs use heuristic-based
  decisions on immediate context (appropriate for scoping constructs)

Minor consolidation opportunity: expose `bodyIsEpochOnly()` as a public utility
(currently private to heuristics).

---

## 4. What's Well-Organized (Don't Touch)

- `opt/edt/` — 5 files, each with clear single responsibility
- `opt/db/` — 8 files, clean domain separation
- `opt/codegen/` — 7 files, focused code-gen helpers
- `opt/epoch/` — freshly split from monolith (model to follow)
- Header/implementation consistency: 56/56 passes match across Passes.td,
  Passes.h, and implementations
- Pipeline integration: all 60 passes properly wired in Compile.cpp
- Existing library extractions (good examples):
  - `lib/arts/transforms/db/stencil/DbStencilIndexer.cpp` (1209 LOC)
  - `lib/arts/transforms/edt/EdtRewriter.cpp` (1110 LOC)
  - `lib/arts/transforms/db/DbBlockPlanResolver.cpp` (788 LOC)

---

## 5. Reorganization Plan

### Tier 1: Quick Wins (3-4 hours, low risk)

| Action | Files | Effort |
|--------|-------|--------|
| Move `DepTransforms.cpp` opt/dep/ to transforms/ | 1 file + CMakeLists | 30 min |
| Move `LoopReordering.cpp` transforms/ to opt/loop/ | 1 file + CMakeLists | 30 min |
| Extract `EdtAllocaSinkingPass` to own file | 1 new file | 1 hr |
| Remove duplicate checks from ContractValidation | 1 file edit | 1 hr |
| Commit in-progress EpochOpt split | git add + commit | 15 min |

### Tier 2: God File Splits (follow EpochOpt pattern)

| God File | Split Strategy | Target |
|----------|---------------|--------|
| ConvertArtsToLLVM (2822) | Extract patterns to codegen/patterns/ | 5-6 files |
| DataPtrHoisting (1776) | Extract helpers to utils/LoopInvarianceUtils | 1 file |
| DbPartitioning (3018) | Extract analysis to transforms/db/DbPartitionAnalyzer | 1 file |
| EdtLowering (2107) | Extract param packing to codegen/EdtParamPacking | 1-2 files |
| EpochLowering (2016) | Split standard vs continuation (follow EpochOpt) | 2-3 files |
| CreateDbs (1688) | Extract phases to transforms/db/ | 2-3 files |

### Tier 3: Structural Clarity

- Add README.md per opt/ subdirectory documenting passes and pipeline stages
- Document `edt-transforms` stage to `opt/edt/` mapping explicitly

---

## 6. Pipeline-to-File Mapping

| Stage | Name | Primary Files | Location |
|-------|------|--------------|----------|
| 1 | raise-memref-dimensionality | RaiseMemRefDimensionality | opt/general/ |
| 2 | collect-metadata | CollectMetadata, VerifyMetadata* | transforms/ |
| 3 | initial-cleanup | (MLIR standard passes) | — |
| 4 | openmp-to-arts | ConvertOpenMPToArts | transforms/ |
| 5 | pattern-pipeline | PatternDiscovery, DepTransforms, LoopNorm, StencilPeel, LoopReorder, Kernel | mixed |
| 6 | edt-transforms | EdtStructuralOpt, EdtICM, EdtPtrRemat | opt/edt/ + transforms/ |
| 7 | create-dbs | CreateDbs, DistributedHostLoopOutlining | transforms/ |
| 8 | db-opt | DbModeTightening | opt/db/ |
| 9 | edt-opt | EdtStructuralOpt, LoopFusion | opt/edt/ + opt/loop/ |
| 10 | concurrency | Concurrency, ForOpt | transforms/ |
| 11 | edt-distribution | StructuredKernelPlan, EdtDistribution, ForLowering | opt/edt/ + transforms/ |
| 12 | post-distribution-cleanup | EdtStructuralOpt, EpochOpt | opt/edt/ + opt/epoch/ |
| 13 | db-partitioning | DbPartitioning, DbDistOwnership, DbTransforms | opt/db/ |
| 14 | post-db-refinement | DbModeTightening, EdtTransforms, ContractValidation, DbScratchElim | opt/db/ + opt/edt/ |
| 15 | late-concurrency-cleanup | BlockLoopStripMining, Hoisting, EdtAllocaSinking | opt/db/ + opt/codegen/ |
| 16 | epochs | CreateEpochs, PersistentStructuredRegion, EpochOpt | transforms/ + opt/ |
| 17 | pre-lowering | ParallelEdtLowering, DbLowering, EdtLowering, EpochLowering | transforms/ |
| 18 | arts-to-llvm | ConvertArtsToLLVM, LoweringContractCleanup, codegen opts | transforms/ + opt/codegen/ |

---

## 7. Scorecard

| Category | Issues | Severity |
|----------|--------|----------|
| Misplaced files | 2 | Medium |
| Multi-pass files | 1 | Medium |
| God files (>1000 LOC) | 10 (19,892 LOC) | High |
| Duplicate verification | 1 | Low |
| Navigation confusion | 2 stage/dir mismatches | Low |
| Declaration/impl consistency | 0 (perfect 56/56) | None |
| Pipeline integration | 0 (all 60 wired) | None |
| EpochAnalysis centralization | Well-organized | None |
