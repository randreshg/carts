# Pass Placement Map (69 passes across 20 stages)

## Directory Classification

| Directory | Ownership | Description |
|---|---|---|
| `sde/Transforms/` | SDE dialect | Metadata, normalization, OMP->SDE, raise, tensor |
| `patterns/Analysis/` | shared | Pattern analysis only (DbPatternMatchers, AccessPatternAnalysis); all pattern *passes* are in core/ due to ARTS deps |
| `core/Transforms/` | arts dialect | EDT/DB/Epoch structural optimization |
| `core/Conversion/OmpToArts/` | arts dialect | SDE -> arts conversion |
| `rt/Conversion/ArtsToRt/` | arts_rt dialect | arts -> arts_rt lowering |
| `rt/Conversion/RtToLLVM/` | arts_rt dialect | arts_rt -> LLVM codegen |
| `rt/Transforms/` | arts_rt dialect | Post-lowering optimization |
| `general/Transforms/` | shared | Standard MLIR + codegen (DCE, CSE, scalar replacement) |
| `verify/` | shared | Verification barrier passes |

## 4-Layer Architecture

```
sde/       -> "this computation reads A, writes C with neighbor deps"
core/      -> "create an EDT with 3 deps partitioned as block-stencil"
             (includes pattern passes: PatternDiscovery, KernelTransforms, etc.)
rt/        -> "call artsEdtCreate(guid, paramv, 3)"
```

Note: Pattern passes were originally planned for a separate `patterns/` layer
but investigation found ALL 4 have ARTS structural op dependencies (ForOp,
EdtOp). They stay in `core/Transforms/`. Pattern *analysis* (read-only,
DbPatternMatchers, AccessPatternAnalysis) stays in `analysis/`.

## Stage-by-Stage Pass Placement

### Stages 1-3: Pre-Dialect (general/ + sde/)

```
Stage 1 (raise-memref):
  general/:  LowerAffine, CSE, ArtsInliner, PolygeistCanonicalize,
             RaiseMemRefDimensionality, HandleDeps, DCE

Stage 2 (collect-metadata):
  sde/:      CollectMetadata (creates SDE metadata annotations)
  general/:  replaceAffineCFG, RaiseSCFToAffine, CSE
  verify/:   VerifyMetadata, VerifyMetadataIntegrity

Stage 3 (initial-cleanup):
  general/:  LowerAffine, CSE, PolygeistCanonicalizeFor
```

### Stages 3.5-4: SDE Pipeline (NEW)

```
Stage 3.5 (omp-to-sde):
  sde/:      ConvertOpenMPToSde
  general/:  DCE, CSE

Stage 3.6 (raise-to-linalg):
  sde/:      RaiseToLinalg

Stage 3.7 (raise-to-tensor):
  sde/:      RaiseToTensor

Stage 3.8 (sde-analysis):
  sde/:      SdeOpt (tensor-space tile-and-fuse)

Stage 3.9 (bufferize):
  upstream:  one-shot-bufferize, linalg-to-loops

Stage 4 (sde-to-arts):
  core/:     ConvertSdeToArts
  general/:  DCE, CSE
  verify/:   VerifyEdtCreated, VerifySdeLowered
```

### Stage 5: Pattern Pipeline (core/)

```
  core/:     PatternDiscovery(seed), DepTransforms, StencilBoundaryPeeling,
             PatternDiscovery(refine), KernelTransforms,
             LoopNormalization, LoopReordering
  general/:  CSE
```

Note: LoopNormalization and LoopReordering were originally planned for `sde/`
but investigation found deep ARTS dependencies (ForOp in match/apply,
DbPatternMatchers in auto-detection). They operate on ARTS IR AFTER
SDE→ARTS conversion and belong in `core/Transforms/`.

### Stages 6-9: Core Arts Optimization

```
Stage 6 (edt-transforms):
  core/:     EdtStructuralOpt, EdtICM, EdtPtrRematerialization
  general/:  DCE, SymbolDCE, CSE

Stage 7 (create-dbs):
  core/:     CreateDbs, DistributedHostLoopOutlining
  general/:  CSE, PolygeistCanonicalize, SymbolDCE, Mem2Reg
  verify/:   VerifyDbCreated

Stage 8 (db-opt):
  core/:     DbModeTightening
  general/:  PolygeistCanonicalize, CSE, Mem2Reg

Stage 9 (edt-opt):
  core/:     EdtStructuralOpt(analysis=true), LoopFusion
  general/:  PolygeistCanonicalize, CSE
```

### Stages 10-12: Distribution & Cleanup

```
Stage 10 (concurrency):
  core/:     Concurrency, ForOpt
  general/:  PolygeistCanonicalize

Stage 11 (edt-distribution):
  core/:     StructuredKernelPlan, EdtDistribution, EdtOrchestrationOpt, ForLowering
  verify/:   VerifyForLowered

Stage 12 (post-distribution-cleanup):
  core/:     EdtStructuralOpt, EpochOpt
  general/:  DCE, PolygeistCanonicalize, CSE
```

### Stages 13-16: DB Partitioning & Epochs

```
Stage 13 (db-partitioning):
  core/:     DbPartitioning, DbDistributedOwnership, DbTransforms

Stage 14 (post-db-refinement):
  core/:     DbModeTightening, EdtTransforms, DbTransforms, DbScratchElimination
  verify/:   ContractValidation
  general/:  PolygeistCanonicalize, CSE

Stage 15 (late-concurrency-cleanup):
  core/:     BlockLoopStripMining, EdtAllocaSinking, Hoisting
  general/:  PolygeistCanonicalize, CSE, DCE, Mem2Reg

Stage 16 (epochs):
  core/:     CreateEpochs, PersistentStructuredRegion, EpochOptScheduling
  verify/:   VerifyEpochCreated
  general/:  PolygeistCanonicalize
```

### Stages 17-18: Lowering (rt/ dialect)

```
Stage 17 (pre-lowering):
  core/:                    ParallelEdtLowering (core->core: splits parallel EDTs),
                            DbLowering (core->core: db_alloc -> db_acquire/release),
                            EdtAllocaSinking, EpochOptScheduling
  rt/Conversion/ArtsToRt/:  EdtLowering, EpochLowering
                            (arts.edt/epoch → arts_rt.edt_create/create_epoch)
  rt/Transforms/:           DataPtrHoisting (hoists lowered dep/db ptr loads)
  general/:                 PolygeistCanonicalize, CSE, LICM, ScalarReplacement
  verify/:                  VerifyEdtLowered, VerifyPreLowered

Stage 18 (arts-to-llvm):
  rt/Conversion/RtToLLVM/:  ConvertArtsToLLVM
                            (arts_rt.* → llvm.call @arts*, arts.db_* → llvm.call @artsDb*)
  rt/Transforms/:           GuidRangCallOpt, RuntimeCallOpt, DataPtrHoisting (2nd)
  general/:                 LoweringContractCleanup, PolygeistCanonicalize, CSE, Mem2Reg,
                            ControlFlowSink, AliasScopeGen, LoopVectorizationHints
  verify/:                  VerifyDbLowered, VerifyLowered
```

## Summary Statistics (Revised per Agent Investigation)

| Classification | Count | Key Passes |
|---|---|---|
| sde/Transforms/ | 5 | CollectMetadata, ConvertOpenMPToSde, RaiseToLinalg, RaiseToTensor, SdeOpt |
| patterns/Analysis/ | 2 | DbPatternMatchers, AccessPatternAnalysis (pure analysis, no op mutation) |
| core/Transforms/ | 34 | EdtStructuralOpt, DbPartitioning, PatternDiscovery, KernelTransforms, StencilBoundaryPeeling, DepTransforms, LoopNormalization, LoopReordering, DbLowering, ParallelEdtLowering, Hoisting, AliasScopeGen, HandleDeps, ArtsInliner, LoweringContractCleanup, + 19 others |
| core/Conversion/SdeToArts/ | 2 | ConvertSdeToArts, CreateDbs |
| rt/Conversion/ArtsToRt/ | 2 | EdtLowering, EpochLowering (the only passes that produce arts_rt ops) |
| rt/Conversion/RtToLLVM/ | 1 | ConvertArtsToLLVM (14 rt patterns + core DB/barrier patterns) |
| rt/Transforms/ | 4 | DataPtrHoisting, GuidRangCallOpt, RuntimeCallOpt |
| general/Transforms/ | 3 | RaiseMemRef, DCE, ScalarReplacement (truly ARTS-agnostic) |
| verify/ | 11+ | VerifyMetadata, VerifyEdtCreated, VerifySdeLowered, VerifyLowered |

**Key revision**: The original plan had 23 core, 4 patterns, 16 general.
Investigation revealed most "patterns" and "general" passes have ARTS structural
op dependencies, moving them to core (32 core, 0 patterns, 3 general). Pattern
*analysis* (read-only) stays in `patterns/Analysis/`.

## Hidden ARTS Dependencies (Agent Investigation Findings)

Deep investigation by 10 parallel agents revealed more hidden dependencies
than initially planned. These corrections are critical for Phase 2/3 scoping.

### Pattern Passes: ALL Have ARTS Dependencies

The original plan claimed pattern passes reference "ZERO ARTS structural ops."
**This is wrong.** All 4 pattern passes have direct ARTS op references:

1. **PatternDiscovery.cpp** -- stamps contracts on `EdtOp`, checks `ForOp`,
   `YieldOp`. Fix: stamp on ForOp only; propagate to EDT in core/ pass.

2. **KernelTransforms.cpp** -- walks `EdtOp` bodies, checks `ForOp` iterator
   types, modifies `ForOp` attributes. Cannot move to `patterns/` without
   refactoring to use interfaces instead of concrete op types.

3. **StencilBoundaryPeeling.cpp** -- operates on `ForOp` loop bounds and
   accesses. Tightly coupled to ARTS loop representation.

4. **DepTransforms.cpp** -- walks `EdtOp` dependency structures. Cannot be
   separated from ARTS structural ops.

**Revised classification**: All 4 pattern passes stay in `core/Transforms/`
until SDE provides an interface-based alternative. The `patterns/` directory
should contain only pattern *analysis* (DbPatternMatchers, AccessPatternAnalysis)
which are pure analysis with no op mutation.

### General Passes: 5 of 8 Have ARTS Dependencies

Originally planned as "general/" but actually ARTS-dependent:

| Pass | ARTS Dependency | Correct Placement |
|---|---|---|
| `Hoisting.cpp` | Hoists `arts.db_acquire`, `arts.db_ref` | `core/Transforms/` |
| `AliasScopeGen.cpp` | Generates scopes for `arts.db_acquire` results | `core/Transforms/` |
| `HandleDeps.cpp` | Walks `arts.omp_dep` ops | `core/Transforms/` |
| `ArtsInliner.cpp` | Policy based on `EdtOp` boundaries | `core/Transforms/` |
| `LoweringContractCleanup.cpp` | Removes ARTS metadata attributes | `core/Transforms/` |

Truly generic passes (correct for `general/`):
- `RaiseMemRefDimensionality.cpp` -- pure memref transforms
- `DeadCodeElimination.cpp` -- standard DCE
- `ScalarReplacement.cpp` -- pure memref->register

### Lowering Passes: DbLowering and ParallelEdtLowering Misclassified

**DbLowering.cpp** does NOT produce arts_rt ops. It lowers `arts.db_alloc` →
`arts.db_acquire`/`arts.db_release` (core→core lowering). It belongs in
`core/Transforms/`, not `rt/Conversion/ArtsToRt/`.

**ParallelEdtLowering.cpp** lowers `arts.edt(parallel)` by splitting into
worker EDTs, but the output is still `arts.edt` + `arts.for` ops (core→core).
It belongs in `core/Transforms/`.

Only **EdtLowering.cpp** and **EpochLowering.cpp** actually produce arts_rt
ops and belong in `rt/Conversion/ArtsToRt/`.

### LoopReordering.cpp -- calls DbPatternMatchers

**Fix**: Metadata-based reordering path is SDE-compatible. Extract matmul
auto-detection into a separate pattern-pipeline pass.

### LoopFusion.cpp -- walks EdtOp + BarrierOp

**Classification**: This is `core/Transforms/`, not patterns/.
