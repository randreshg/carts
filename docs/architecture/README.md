# CARTS Sub-Dialect Architecture

Progressive lowering from runtime-agnostic intent to ARTS runtime calls,
following IREE's proven multi-dialect pattern.

```
  C/C++ + OpenMP
  +----------+
  | Polygeist | -- cgeist --> scf + memref + arith + omp
  +----------+
       |
  =====|=====================================================
  ||   v   SDE DIALECT (sde.*)  Stages 1-5                ||
  ||   CU: what runs  SU: how/when  MU: data access       ||
  ==========================================================
       |
       |  +------------------------------------------------+
       |  | MLIR Infrastructure (composed, not reinvented)  |
       |  |                                                 |
       |  |  +----------+  +----------+  +----------+      |
       |  |  |  Affine  |  |  Linalg  |  |  Tensor  |      |
       |  |  | analysis |  | struct   |  | analysis |      |
       |  |  +----------+  +----------+  +----------+      |
       |  |  +----------+  +----------+                     |
       |  |  |  Vector  |  | Bufferz  |                     |
       |  |  | codegen  |  | tensor>m |                     |
       |  |  +----------+  +----------+                     |
       |  +------------------------------------------------+
       |
       |  CU: sde.cu_region, sde.cu_task, sde.cu_reduce, sde.cu_atomic
       |  SU: sde.su_iterate, sde.su_barrier, sde.su_distribute
       |  MU: sde.mu_dep, sde.mu_access, sde.mu_reduction_decl
       |
  -----+--- SdeToArts Conversion ----------------------------
       |
  =====|=====================================================
  ||   v   ARTS CORE (arts.*)   Stages 6-16               ||
  ||   "Create EDT with 3 deps, partition as block-stencil"||
  ==========================================================
       |
       |  core/Transforms: EdtStructuralOpt, DbPartitioning,
       |    CreateDbs, EdtDistribution, CreateEpochs, ...
       |
       |  Arts Ops: arts.edt, arts.for, arts.epoch,
       |    arts.db_alloc, arts.db_acquire, arts.barrier
       |
  -----+--- ArtsToRt Lowering -------------------------------
       |
  =====|=====================================================
  ||   v   ARTS RT (arts_rt.*)  Stages 17-18              ||
  ||   "call artsEdtCreate(guid, paramv, 3)"              ||
  ==========================================================
       |
       |  Rt Ops: arts_rt.edt_create, arts_rt.rec_dep,
       |    arts_rt.create_epoch, arts_rt.dep_db_acquire
       |
  -----+--- ConvertArtsToLLVM -------------------------------
       |
  =====|=====================================================
  ||   v   LLVM  (llvm.*)                                 ||
  ||   ARTS C Runtime API + LLVM IR                       ||
  ==========================================================
```

## Document Index

| Document | Contents |
|---|---|
| [sde-dialect.md](sde-dialect.md) | SDE dialect specification: CU/SU/MU ops, types, attributes, effect system, OMP-to-SDE conversion |
| [arts-rt-dialect.md](arts-rt-dialect.md) | Phase 1: Extract 14 runtime ops into `arts_rt` dialect |
| [pipeline-redesign.md](pipeline-redesign.md) | Pipeline stages with tensor raise/analyze/bufferize cycle |
| [pass-placement.md](pass-placement.md) | All 69 passes classified into sde/patterns/core/rt/general/verify |
| [mlir-infrastructure.md](mlir-infrastructure.md) | MLIR dialect integration: linalg, tensor, bufferization, affine, vector |
| [external-techniques.md](external-techniques.md) | Techniques adopted from IREE, Halide, Legion, PaRSEC, Chapel, etc. |
| [implementation-order.md](implementation-order.md) | Phased execution plan with parallelization strategy |
| [cost-model.md](cost-model.md) | Cost model, 21 hardcoded thresholds, runtime awareness gaps, convergence framework |
| [op-classification.md](op-classification.md) | Complete op/attribute classification table, IREE patterns, operation lifecycle |

## Why Three Dialects?

The monolithic `arts` dialect (1260-line Ops.td, 32+ ops) mixes three
fundamentally different concerns:

1. **Semantic intent** -- what computation to perform, what data it touches
2. **Structural representation** -- EDT/DB/Epoch runtime abstractions
3. **Runtime API calls** -- 1:1 mapping to ARTS C functions

Each concern has different lifetime, stability, and optimization potential.
Splitting into `sde.*` -> `arts.*` -> `arts_rt.*` with progressive lowering
enables:

- **SDE**: Runtime-agnostic analysis using MLIR infrastructure (tensor, linalg, affine)
- **ARTS core**: Structural optimization on EDT/DB/Epoch without SDE noise
- **ARTS RT**: Clean codegen boundary with verifiable 1:1 API mapping

## Design Principles

- **CU/SU/MU separation**: Compute Units (what), Scheduling Units (how/when), Memory Units (data)
- **STATE, DEPENDENCY, EFFECT are first-class** in every CU op
- **Tensor-first analysis**: Raise to tensor space for alias-free SSA dependency analysis
- **Compose with MLIR**: Use linalg/tensor/affine/bufferization, don't reinvent
- **Lossless from OMP**: Preserve reduction combiners, nowait, collapse, schedule
- **Runtime-agnostic SDE**: Could target ARTS, Legion, StarPU, or GPU runtimes

## Directory Layout (Target)

```
lib/arts/
  dialect/
    sde/       "this computation reads A, writes C with neighbor deps"
    core/      "create an EDT with 3 deps partitioned as block-stencil"
    rt/        "call artsEdtCreate(guid, paramv, 3)"
  patterns/    pattern detection and classification (no dialect dependency)
  general/     standard MLIR transforms (DCE, CSE, memref raising)
  verify/      pipeline barrier checks
  analysis/    shared analysis framework
  utils/       shared utilities
```
