# Pipeline Redesign with Tensor Integration

## Pipeline Overview

```
Stages 1-2:   Normalization    [NormalizeMemrefs, CollectMetadata]
Stage 3:      Cleanup           [LowerAffine, CSE]
Stage 3.5:    OMP->SDE          [ConvertOpenMPToSde]
Stage 3.6:    RaiseToLinalg     [scf.for+memref -> linalg.generic]
Stage 3.7:    RaiseToTensor     [linalg on memref -> linalg on tensor]
Stage 3.8:    SDE analysis+opt  [tensor-space analysis, tile-and-fuse]
Stage 3.9:    Bufferize         [one-shot bufferize: tensor -> memref]
Stage 4:      SDE->ARTS         [ConvertSdeToArts + linalg-to-loops]
Stage 5:      pattern-pipeline  [PatternDiscovery, KernelTransforms]
Stages 6-16:  arts.* optimization
Stage 17:     arts.* -> arts_rt.*  [pre-lowering]
Stage 18:     arts_rt.* -> llvm.*  [final codegen]
```

## The Tensor Window (Stages 3.7-3.9)

This is where SDE gets maximum leverage from MLIR infrastructure.

### Problem

CARTS currently works entirely in memref space. Memrefs are mutable pointers.
To determine data dependencies, the compiler must:
- Run expensive alias analysis
- Track all possible load/store interferences
- Conservatively assume dependencies when analysis fails

### Solution: Raise to Tensor Space

In tensor IR:
- Every value is **immutable** -- no aliasing, no side effects
- SSA def-use chains ARE the complete dependency graph
- `tensor.extract_slice` = reading a subregion = DB acquire(READ)
- `tensor.insert_slice` = writing a subregion = DB acquire(WRITE)
- One-shot bufferization decides in-place vs copy = DB mode optimization

### The Raise/Analyze/Lower Cycle

```
memref-based IR (from Polygeist)
  |
  v RaiseToLinalg: scf.for+memref -> linalg.generic on memref
  |   WHERE patterns are recognized (best-effort)
  |
  v RaiseToTensor: linalg.generic on memref -> linalg.generic on tensor
  |   Unrecognized memref ops stay as memref (graceful fallback)
  |
  v SDE Analysis (in tensor space):
  |   - SSA dataflow = dependency graph (free, exact)
  |   - tensor.extract_slice = DB acquire region
  |   - No alias analysis needed for tensor ops
  |   - Linalg tile-and-fuse in tensor space
  |   - Pattern classification from iterator_types
  |
  v Bufferize: one-shot bufferization -> linalg.generic on memref
  |   Optimal in-place decisions (replaces manual DB alias analysis)
  |   MLIR upstream pass -- battle-tested
  |
  v linalg-to-loops: linalg on memref -> scf.for (upstream MLIR)
  |
  v SDE->ARTS conversion (scf.for based, as today)
```

**Key**: Each raising pass is **best-effort**. Code that can't be raised to
linalg stays as scf.for+memref. Code that can't be raised to tensor stays
as linalg on memref.

### What Tensor Space Replaces

| CARTS Current | Tensor Space Equivalent |
|---|---|
| `DependenceAnalyzer` (wraps affine) | SSA def-use chains on tensor values |
| `DbAliasAnalysis` (custom) | No aliasing by construction |
| `AccessAnalyzer` (custom) | `tensor.extract_slice` offsets/sizes |
| `DbPartitionState` (custom) | Bufferization in-place analysis |
| CreateDbs memref walk | `tensor.extract_slice` -> `arts.db_acquire` |

### Example: Jacobi Stencil in Tensor Space

```mlir
// After RaiseToTensor:
%A_tensor = bufferization.to_tensor %A : memref<?xf64>

%result = linalg.generic {
  indexing_maps = [
    affine_map<(i) -> (i - 1)>,
    affine_map<(i) -> (i + 1)>,
    affine_map<(i) -> (i)>
  ],
  iterator_types = ["parallel"]
} ins(%A_tensor, %A_tensor : tensor<?xf64>, tensor<?xf64>)
  outs(%B_tensor : tensor<?xf64>) {
^bb0(%a_left: f64, %a_right: f64, %b: f64):
  %sum = arith.addf %a_left, %a_right : f64
  %val = arith.mulf %half, %sum : f64
  linalg.yield %val : f64
} -> tensor<?xf64>

// Dependencies are VISIBLE in SSA: %result depends on %A_tensor
// Access pattern is VISIBLE in indexing_maps: stencil +/- 1 offset
// No alias analysis needed -- tensor values are immutable

// After one-shot bufferization -> back to memref for ARTS
bufferization.materialize_in_destination %result in %B : ...
```

## Three-Layered Analysis Strategy

```
+------------------------------------------------------------+
|  Layer 1: Linalg + Affine (structured, precise)            |
|  ~60% of benchmarks                                        |
|  scf.for+memref -> linalg.generic -> tensor (SSA deps)     |
+------------------------------------------------------------+
|  Layer 2: SCF + Metadata (conservative, annotated)         |
|  ~25% of benchmarks                                        |
|  scf.for/while -> LoopMetadata (conservative deps)         |
+------------------------------------------------------------+
|  Layer 3: Indirect/Sparse (pattern-based, full-range)      |
|  ~15% of benchmarks                                        |
|  A[B[i]] -> classify as indirect, full-range acquire       |
+------------------------------------------------------------+
```

## Verification Barriers

11 verification passes form 6 barrier points:

```
[Stage 2]  VerifyMetadata + VerifyMetadataIntegrity
              -> SDE metadata is complete and consistent

[Stage 4]  VerifyEdtCreated
              -> All OMP/SDE constructs converted to arts.edt/for/epoch

[Stage 7]  VerifyDbCreated
              -> All data dependencies materialized as DB ops

[Stage 11] VerifyForLowered
              -> All arts.for lowered to EDT + epoch structure

[Stage 16] VerifyEpochCreated
              -> All epoch synchronization points created

[Stage 17] VerifyEdtLowered + VerifyPreLowered
              -> All arts.edt lowered to arts_rt ops; no SDE ops survive

[Stage 18] VerifyDbLowered + VerifyLowered
              -> All DB ops converted; no arts/arts_rt ops survive
```

New barriers for 3-dialect architecture:
- **VerifySdeLowered** (after SDE->Arts): no `sde.*` ops survive
- **VerifyPreLowered**: also check no `arts_rt.*` ops exist prematurely
- **VerifyLowered**: also check no `arts_rt.*` ops survive

## Stage-by-Stage Detail

### Stages 1-3: Pre-Dialect (general/ + sde/)

```
Stage 1 (raise-memref):
  general/:  LowerAffine, CSE, ArtsInliner, PolygeistCanonicalize,
             RaiseMemRefDimensionality, HandleDeps, DCE

Stage 2 (collect-metadata):
  sde/:      CollectMetadata
  general/:  replaceAffineCFG, RaiseSCFToAffine, CSE
  verify/:   VerifyMetadata, VerifyMetadataIntegrity

Stage 3 (initial-cleanup):
  general/:  LowerAffine, CSE, PolygeistCanonicalizeFor
```

### Stage 3.5-4: SDE Pipeline (NEW)

```
Stage 3.5 (omp-to-sde):
  sde/:      ConvertOpenMPToSde (12 patterns)
  general/:  DCE, CSE

Stage 3.6 (raise-to-linalg):
  sde/:      RaiseToLinalg (scf.for+memref -> linalg.generic)

Stage 3.7 (raise-to-tensor):
  sde/:      RaiseToTensor (linalg memref -> linalg tensor)

Stage 3.8 (sde-analysis-opt):
  sde/:      SdeOpt (tensor-space tile-and-fuse, dep analysis)

Stage 3.9 (bufferize):
  upstream:  one-shot-bufferize (tensor -> memref)
  upstream:  linalg-to-loops (linalg -> scf.for)

Stage 4 (sde-to-arts):
  core/:     ConvertSdeToArts
  general/:  DCE, CSE
  verify/:   VerifyEdtCreated, VerifySdeLowered
```

### Stages 5-18: Unchanged from Current Pipeline

See [pass-placement.md](pass-placement.md) for complete per-stage pass list.
