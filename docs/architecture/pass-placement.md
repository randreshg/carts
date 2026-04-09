# Pass Placement Map (69 passes across 20 stages)

## Directory Classification

| Directory | Ownership | Description |
|---|---|---|
| `sde/Transforms/` | SDE dialect | Metadata, normalization, OMP->SDE, raise, tensor |
| `patterns/Transforms/` | shared | Pattern detection, classification, kernel rewrites |
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
patterns/  -> "this is a stencil; peel boundaries; reorder for cache"
core/      -> "create an EDT with 3 deps partitioned as block-stencil"
rt/        -> "call artsEdtCreate(guid, paramv, 3)"
```

Pattern classification is NEITHER runtime-specific NOR dialect-specific.
Pattern passes reference ZERO ARTS structural ops.

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

### Stage 5: Pattern Pipeline (patterns/ + sde/)

```
  patterns/: PatternDiscovery(seed), DepTransforms, StencilBoundaryPeeling,
             PatternDiscovery(refine), KernelTransforms
  sde/:      LoopNormalization, LoopReordering
  general/:  CSE
```

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
  rt/Conversion/ArtsToRt/:  ParallelEdtLowering, DbLowering, EdtLowering, EpochLowering
                            (arts.edt/epoch → arts_rt.edt_create/create_epoch)
  rt/Transforms/:           DataPtrHoisting (hoists lowered dep/db ptr loads)
  core/:                    EdtAllocaSinking, EpochOptScheduling
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

## Summary Statistics

| Classification | Count | Key Passes |
|---|---|---|
| sde/Transforms/ | 7 | CollectMetadata, ConvertOpenMPToSde, RaiseToLinalg, RaiseToTensor, LoopNormalization, LoopReordering, SdeOpt |
| patterns/Transforms/ | 4 | PatternDiscovery, KernelTransforms, StencilBoundaryPeeling, DepTransforms |
| core/Transforms/ | 23 | EdtStructuralOpt, DbPartitioning, Concurrency, CreateEpochs, LoopFusion |
| core/Conversion/SdeToArts/ | 2 | ConvertSdeToArts, CreateDbs |
| rt/Conversion/ArtsToRt/ | 4 | EdtLowering, EpochLowering, DbLowering, ParallelEdtLowering |
| rt/Conversion/RtToLLVM/ | 1 | ConvertArtsToLLVM (14 rt patterns + core DB/barrier patterns) |
| rt/Transforms/ | 4 | DataPtrHoisting, GuidRangCallOpt, RuntimeCallOpt |
| general/Transforms/ | 16 | RaiseMemRef, DCE, ScalarReplacement, AliasScopeGen, LoopVecHints |
| verify/ | 11+ | VerifyMetadata, VerifyEdtCreated, VerifySdeLowered, VerifyLowered |

## Hidden ARTS Dependencies in SDE/Pattern Passes

Three passes have hidden ARTS deps that must be resolved:

### 1. PatternDiscovery.cpp -- stamps contracts on EdtOp

**Fix**: PatternDiscovery stamps contracts on ForOp only. A downstream
core/ pass propagates to EDT ops after SDE->ARTS conversion.

### 2. LoopReordering.cpp -- calls DbPatternMatchers

**Fix**: Metadata-based reordering path is SDE-compatible. Extract matmul
auto-detection into a separate pattern-pipeline pass.

### 3. LoopFusion.cpp -- walks EdtOp + BarrierOp

**Classification**: This is `core/Transforms/`, not patterns/.
