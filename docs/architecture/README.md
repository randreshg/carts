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
  ||   Type-polymorphic: tensor + memref native            ||
  ==========================================================
       |
       |  +------------------------------------------------+
       |  | MLIR Infrastructure (composed, not reinvented)  |
       |  |                                                 |
       |  |  +----------+  +----------+  +----------+      |
       |  |  |  Linalg  |  |  Tensor  |  |  Affine  |      |
       |  |  | native:  |  | native:  |  | analysis:|      |
       |  |  | generic, |  | SSA deps |  | deps,    |      |
       |  |  | tile&fuse|  | no alias |  | parallel |      |
       |  |  +----------+  +----------+  +----------+      |
       |  |  +----------+  +----------+                     |
       |  |  | Bufferz  |  |  Vector  |                     |
       |  |  | tensor>m |  | codegen  |                     |
       |  |  +----------+  +----------+                     |
       |  +------------------------------------------------+
       |
       |  CU: sde.cu_region, sde.cu_task, sde.cu_reduce, sde.cu_atomic
       |    (ins/outs accept tensor OR memref via DestinationStyleOpInterface)
       |  SU: sde.su_iterate, sde.su_barrier, sde.su_distribute
       |    (iter_args carry tensor values for SSA-tracked state)
       |  MU: sde.mu_dep, sde.mu_access, sde.mu_reduction_decl
       |    (memref fallback only; tensor path uses tensor.extract/insert_slice)
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
- **Tensor-native ops**: SDE CU ops are type-polymorphic (tensor + memref) with `DestinationStyleOpInterface`; in tensor space, SSA def-use chains ARE the dependency graph -- no custom analysis needed
- **Compose with MLIR**: linalg/tensor/bufferization are native to SDE, not separate; affine for analysis
- **Lossless from OMP**: Preserve reduction combiners, nowait, collapse, schedule
- **Runtime-agnostic SDE**: Could target ARTS, Legion, StarPU, or GPU runtimes

## Directory Layout (Target)

Each dialect owns its own IREE-style directory with `IR/`, `Conversion/`,
`Transforms/`, and `Analysis/` subdirectories. Shared infrastructure
(`patterns/`, `general/`, `verify/`, `analysis/`, `utils/`) sits outside
the dialect directories.

```
include/arts/dialect/
  sde/IR/          SdeDialect.td, SdeOps.td, SdeTypes.td, SdeAttrs.td, SdeDialect.h
  core/IR/         ArtsDialect.td, ArtsOps.td, ArtsAttrs.td, ArtsTypes.td, ArtsDialect.h
  rt/IR/           RtDialect.td, RtOps.td, RtDialect.h

lib/arts/
  dialect/
    sde/                   "this computation reads A, writes C with neighbor deps"
      IR/                  SdeDialect.cpp, SdeOps.cpp
      Analysis/            metadata/, ValueAnalysis, SdeLoopAnalysis
      Transforms/          ConvertOpenMPToSde, RaiseToLinalg, RaiseToTensor, SdeOpt,
                           LoopNormalization, LoopReordering, CollectMetadata

    core/                  "create an EDT with 3 deps partitioned as block-stencil"
      IR/                  Dialect.cpp (reduced: 21 ops)
      Transforms/          EdtStructuralOpt, DbPartitioning, CreateDbs, Concurrency,
                           EdtDistribution, ForLowering, CreateEpochs, EpochOpt, ...
      Conversion/
        SdeToArts/         SDE -> ARTS conversion (+ linalg-to-loops)

    rt/                    "call artsEdtCreate(guid, paramv, 3)"
      IR/                  RtDialect.cpp, RtOps.cpp (14 ops)
      Conversion/
        ArtsToRt/          EdtLowering, EpochLowering, DbLowering, ParallelEdtLowering
        RtToLLVM/          RtToLLVMPatterns (14 patterns from ConvertArtsToLLVM)
      Transforms/          DataPtrHoisting, GuidRangCallOpt, RuntimeCallOpt

  patterns/                pattern detection and classification (no dialect dependency)
  general/                 standard MLIR transforms (DCE, CSE, memref raising)
  verify/                  pipeline barrier checks (11 passes)
  analysis/                shared analysis framework
  utils/                   shared utilities
```
