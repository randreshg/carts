# Complete Directory Map: Current -> Target

Every source file in the CARTS compiler mapped to its target location
in the new 3-dialect IREE-style architecture. 162 `.cpp` files, 115
headers/TableGen, 1 compile driver.

**Legend**: `=` stays in place, `->` moves, `NEW` created, `DEAD` removed.

**Status**: Phase 1, 2A-2B, 3B (steps 5-8) COMPLETE. All pass .cpp files
have moved to their target locations under `lib/arts/dialect/`. Only Phase 3A
(moving core IR TableGen/headers to `dialect/core/IR/`) remains.

---

## 1. Dialect IR (TableGen + Dialect.cpp)

### Current: `include/arts/` + `lib/arts/Dialect.cpp`

| Current Location | Target Location | Phase | Notes |
|---|---|---|---|
| `include/arts/Dialect.td` | `include/arts/dialect/core/IR/ArtsDialect.td` | 3 | Forwarding header at old path |
| `include/arts/Ops.td` | `include/arts/dialect/core/IR/ArtsOps.td` | 1+3 | Phase 1: remove 14 rt ops; Phase 3: move file |
| `include/arts/Attributes.td` | `include/arts/dialect/core/IR/ArtsAttrs.td` | 1+3 | Phase 1: remove DepAccessMode; Phase 3: move |
| `include/arts/Types.td` | `include/arts/dialect/core/IR/ArtsTypes.td` | 3 | Move |
| `include/arts/Dialect.h` | `include/arts/dialect/core/IR/ArtsDialect.h` | 3 | Forwarding header at old path |
| `lib/arts/Dialect.cpp` | `lib/arts/dialect/core/IR/Dialect.cpp` | 1+3 | Phase 1: remove 2 verifiers + 2 builders; Phase 3: move |
| — | `include/arts/dialect/rt/IR/RtDialect.td` | NEW/1 | `arts_rt` dialect definition |
| — | `include/arts/dialect/rt/IR/RtOps.td` | NEW/1 | 14 ops extracted from Ops.td |
| — | `include/arts/dialect/rt/IR/RtDialect.h` | NEW/1 | Public C++ header |
| — | `lib/arts/dialect/rt/IR/RtDialect.cpp` | NEW/1 | ArtsRtDialect::initialize() |
| — | `lib/arts/dialect/rt/IR/RtOps.cpp` | NEW/1 | Verifiers for CreateEpochOp, RecordDepOp |
| — | `include/arts/dialect/sde/IR/SdeDialect.td` | NEW/2 | SDE dialect definition |
| — | `include/arts/dialect/sde/IR/SdeOps.td` | NEW/2 | CU/SU/MU/yield ops |
| — | `include/arts/dialect/sde/IR/SdeTypes.td` | NEW/2 | CompletionType, DepType |
| — | `include/arts/dialect/sde/IR/SdeAttrs.td` | NEW/2 | Enums + metadata attributes |
| — | `include/arts/dialect/sde/IR/SdeEffects.h` | NEW/2 | ComputeResource, DataResource, SyncResource |
| — | `include/arts/dialect/sde/IR/SdeDialect.h` | NEW/2 | Public C++ header |
| — | `lib/arts/dialect/sde/IR/SdeDialect.cpp` | NEW/2 | SdeDialect::initialize() |
| — | `lib/arts/dialect/sde/IR/SdeOps.cpp` | NEW/2 | Verifiers + builders |

---

## 2. Codegen Infrastructure

### Current: `lib/arts/codegen/` + `include/arts/codegen/`

These files provide the `ArtsCodegen` context used by both core and rt
LLVM conversion patterns. They stay shared.

| Current Location | Target Location | Phase | Notes |
|---|---|---|---|
| `lib/arts/codegen/Codegen.cpp` | = | — | Shared by core + rt patterns |
| `include/arts/codegen/Codegen.h` | = | — | Shared |
| `include/arts/codegen/Types.h` | = | — | Shared |
| `include/arts/codegen/Kinds.def` | = | — | Shared |

---

## 3. Passes — Transforms

### Current: `lib/arts/passes/transforms/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `CollectMetadata.cpp` | `lib/arts/dialect/sde/Transforms/CollectMetadata.cpp` | 2D | SDE metadata; zero ARTS deps |
| `ConvertOpenMPToArts.cpp` | `lib/arts/dialect/core/Conversion/OmpToArts/ConvertOpenMPToArts.cpp` | 3 | Phase 2: replaced by OmpToSde; Phase 3: move remainder |
| `ConvertArtsToLLVM.cpp` | `lib/arts/dialect/core/Conversion/ArtsToLLVM/ConvertArtsToLLVM.cpp` | 3 | DONE — calls both populateCorePatterns + populateRtToLLVMPatterns |
| `ConvertArtsToLLVMPatterns.cpp` | `lib/arts/dialect/core/Conversion/ArtsToLLVM/ConvertArtsToLLVMPatterns.cpp` | 1+3 | DONE — Phase 1: extract 14 rt patterns; Phase 3: move |
| `Concurrency.cpp` | `lib/arts/dialect/core/Transforms/Concurrency.cpp` | 3 | Walks EdtOp concurrency graph |
| `CreateDbs.cpp` | `lib/arts/dialect/core/Transforms/CreateDbs.cpp` | 3 | Creates arts.db_alloc from memref analysis |
| `CreateEpochs.cpp` | `lib/arts/dialect/core/Transforms/CreateEpochs.cpp` | 3 | Creates arts.epoch regions |
| `DbLowering.cpp` | `lib/arts/dialect/core/Transforms/DbLowering.cpp` | 3 | arts.db_alloc -> arts.db_acquire (core->core, NOT rt) |
| `DepTransforms.cpp` | `lib/arts/dialect/core/Transforms/DepTransforms.cpp` | 3 | Dependency transforms; has ARTS EdtOp deps |
| `DistributedHostLoopOutlining.cpp` | `lib/arts/dialect/core/Transforms/DistributedHostLoopOutlining.cpp` | 3 | ARTS structural |
| `EdtDistribution.cpp` | `lib/arts/dialect/core/Transforms/EdtDistribution.cpp` | 3 | ARTS structural |
| `EdtLowering.cpp` | `lib/arts/dialect/rt/Conversion/ArtsToRt/EdtLowering.cpp` | 3 | arts.edt -> arts_rt.edt_create; Phase 1: add using decls |
| `EdtLoweringSupport.cpp` | `lib/arts/dialect/rt/Conversion/ArtsToRt/EdtLoweringSupport.cpp` | 3 | Support for EdtLowering; Phase 1: add using decls |
| `EdtPtrRematerialization.cpp` | `lib/arts/dialect/core/Transforms/EdtPtrRematerialization.cpp` | 3 | ARTS structural |
| `EpochLowering.cpp` | `lib/arts/dialect/rt/Conversion/ArtsToRt/EpochLowering.cpp` | 3 | arts.epoch -> arts_rt.create_epoch; Phase 1: add using decls |
| `ForLowering.cpp` | `lib/arts/dialect/core/Transforms/ForLowering.cpp` | 3 | arts.for -> EDT structure; Phase 1: add using decls |
| `ForOpt.cpp` | `lib/arts/dialect/core/Transforms/ForOpt.cpp` | 3 | ARTS structural |
| `LoweringContractCleanup.cpp` | `lib/arts/dialect/core/Transforms/LoweringContractCleanup.cpp` | 3 | Removes ARTS metadata attrs; has ARTS deps |
| `ParallelEdtLowering.cpp` | `lib/arts/dialect/core/Transforms/ParallelEdtLowering.cpp` | 3 | arts.edt(parallel) -> arts.edt workers (core->core, NOT rt) |
| `PatternDiscovery.cpp` | `lib/arts/dialect/core/Transforms/PatternDiscovery.cpp` | 3 | Pattern detection; has EdtOp/ForOp deps |
| — | `lib/arts/dialect/rt/Conversion/RtToLLVM/RtToLLVMPatterns.cpp` | NEW/1 | 14 patterns extracted from ConvertArtsToLLVMPatterns.cpp |
| — | `lib/arts/dialect/sde/Transforms/ConvertOpenMPToSde.cpp` | NEW/2 | 12 OMP->SDE patterns |
| — | `lib/arts/dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp` | NEW/2 | SDE->ARTS lowering |
| — | `lib/arts/dialect/sde/Transforms/RaiseToLinalg.cpp` | NEW/2 | scf.for+memref -> linalg.generic |
| — | `lib/arts/dialect/sde/Transforms/RaiseToTensor.cpp` | NEW/2 | linalg on memref -> linalg on tensor |
| — | `lib/arts/dialect/sde/Transforms/SdeOpt.cpp` | NEW/2 | tensor-space tile-and-fuse |

### Internal Headers: `include/arts/passes/transforms/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `ConvertArtsToLLVMInternal.h` | `include/arts/dialect/core/Conversion/ArtsToLLVM/ConvertArtsToLLVMInternal.h` | 3 | DONE — shared ArtsToLLVMPattern<T> base |
| `EdtLoweringInternal.h` | `include/arts/dialect/rt/Conversion/ArtsToRt/EdtLoweringInternal.h` | 3 | Phase 1: add using decls |

### Pass Declarations: `include/arts/passes/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `Passes.h` | = | 1+ | Add VerifyStructuralLowered decl |
| `Passes.td` | = | 1+ | Add VerifyStructuralLowered def |

---

## 4. Passes — Optimizations

### 4.1 Codegen Optimizations: `lib/arts/passes/opt/codegen/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `AliasScopeGen.cpp` | `lib/arts/dialect/core/Transforms/AliasScopeGen.cpp` | 3 | DONE — LLVM alias scopes |
| `DataPtrHoisting.cpp` | `lib/arts/dialect/rt/Transforms/DataPtrHoisting.cpp` | 3 | Hoists lowered dep/db ptr loads |
| `DataPtrHoistingSupport.cpp` | `lib/arts/dialect/rt/Transforms/DataPtrHoistingSupport.cpp` | 3 | Phase 1: add using decls |
| `GuidRangCallOpt.cpp` | `lib/arts/dialect/rt/Transforms/GuidRangCallOpt.cpp` | 3 | arts_guid_reserve -> range |
| `Hoisting.cpp` | `lib/arts/dialect/core/Transforms/Hoisting.cpp` | 3 | Hoists arts.db_acquire/ref |
| `LoopVectorizationHints.cpp` | `lib/arts/dialect/core/Transforms/LoopVectorizationHints.cpp` | 3 | DONE — LLVM vectorization metadata |
| `RuntimeCallOpt.cpp` | `lib/arts/dialect/rt/Transforms/RuntimeCallOpt.cpp` | 3 | DONE — Runtime call hoisting |
| `ScalarReplacement.cpp` | `lib/arts/dialect/core/Transforms/ScalarReplacement.cpp` | 3 | DONE — Generic memref->register |

#### Internal Headers: `include/arts/passes/opt/codegen/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DataPtrHoistingInternal.h` | `include/arts/dialect/rt/Transforms/DataPtrHoistingInternal.h` | 3 | Moves with DataPtrHoisting |

### 4.2 DB Optimizations: `lib/arts/passes/opt/db/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `BlockLoopStripMining.cpp` | `lib/arts/dialect/core/Transforms/BlockLoopStripMining.cpp` | 3 | ARTS structural |
| `BlockLoopStripMiningSupport.cpp` | `lib/arts/dialect/core/Transforms/BlockLoopStripMiningSupport.cpp` | 3 | Support file |
| `ContractValidation.cpp` | `lib/arts/verify/ContractValidation.cpp` | 3 | Verification |
| `DbDistributedOwnership.cpp` | `lib/arts/dialect/core/Transforms/DbDistributedOwnership.cpp` | 3 | ARTS structural |
| `DbModeTightening.cpp` | `lib/arts/dialect/core/Transforms/DbModeTightening.cpp` | 3 | ARTS structural |
| `DbPartitioning.cpp` | `lib/arts/dialect/core/Transforms/DbPartitioning.cpp` | 3 | ARTS structural |
| `DbPartitioningSupport.cpp` | `lib/arts/dialect/core/Transforms/DbPartitioningSupport.cpp` | 3 | Support file |
| `DbScratchElimination.cpp` | `lib/arts/dialect/core/Transforms/DbScratchElimination.cpp` | 3 | ARTS structural |
| `DbTransformsPass.cpp` | `lib/arts/dialect/core/Transforms/DbTransformsPass.cpp` | 3 | ARTS structural |

#### Internal Headers: `include/arts/passes/opt/db/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `BlockLoopStripMiningInternal.h` | `include/arts/dialect/core/Transforms/BlockLoopStripMiningInternal.h` | 3 | Moves with pass |
| `DbPartitioningInternal.h` | `include/arts/dialect/core/Transforms/DbPartitioningInternal.h` | 3 | Moves with pass |

### 4.3 EDT Optimizations: `lib/arts/passes/opt/edt/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EdtAllocaSinking.cpp` | `lib/arts/dialect/core/Transforms/EdtAllocaSinking.cpp` | 3 | ARTS structural |
| `EdtICM.cpp` | `lib/arts/dialect/core/Transforms/EdtICM.cpp` | 3 | ARTS structural |
| `EdtOrchestrationOpt.cpp` | `lib/arts/dialect/core/Transforms/EdtOrchestrationOpt.cpp` | 3 | ARTS structural |
| `EdtStructuralOpt.cpp` | `lib/arts/dialect/core/Transforms/EdtStructuralOpt.cpp` | 3 | ARTS structural |
| `EdtTransformsPass.cpp` | `lib/arts/dialect/core/Transforms/EdtTransformsPass.cpp` | 3 | ARTS structural |
| `PersistentStructuredRegion.cpp` | `lib/arts/dialect/core/Transforms/PersistentStructuredRegion.cpp` | 3 | ARTS structural |
| `StructuredKernelPlanPass.cpp` | `lib/arts/dialect/core/Transforms/StructuredKernelPlanPass.cpp` | 3 | ARTS structural |

#### Internal Headers: `include/arts/passes/opt/epoch/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EpochOptInternal.h` | `include/arts/dialect/core/Transforms/EpochOptInternal.h` | 3 | Moves with pass |

### 4.4 Epoch Optimizations: `lib/arts/passes/opt/epoch/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EpochOpt.cpp` | `lib/arts/dialect/core/Transforms/EpochOpt.cpp` | 3 | ARTS structural |
| `EpochOptCpsChain.cpp` | `lib/arts/dialect/core/Transforms/EpochOptCpsChain.cpp` | 3 | ARTS structural |
| `EpochOptScheduling.cpp` | `lib/arts/dialect/core/Transforms/EpochOptScheduling.cpp` | 3 | ARTS structural; Phase 1: add using decls |
| `EpochOptStructural.cpp` | `lib/arts/dialect/core/Transforms/EpochOptStructural.cpp` | 3 | ARTS structural |
| `EpochOptSupport.cpp` | `lib/arts/dialect/core/Transforms/EpochOptSupport.cpp` | 3 | ARTS structural |

### 4.5 General Optimizations: `lib/arts/passes/opt/general/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DeadCodeElimination.cpp` | `lib/arts/general/Transforms/DeadCodeElimination.cpp` | 3 | Generic |
| `HandleDeps.cpp` | `lib/arts/general/Transforms/HandleDeps.cpp` | 3 | Generic |
| `Inliner.cpp` | `lib/arts/general/Transforms/Inliner.cpp` | 3 | Generic |
| `RaiseMemRefDimensionality.cpp` | `lib/arts/general/Transforms/RaiseMemRefDimensionality.cpp` | 3 | Generic |

### 4.6 Kernel Optimizations: `lib/arts/passes/opt/kernel/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `KernelTransforms.cpp` | `lib/arts/patterns/Transforms/KernelTransforms.cpp` | 3 | Pattern-level kernel rewrites |

### 4.7 Loop Optimizations: `lib/arts/passes/opt/loop/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `LoopFusion.cpp` | `lib/arts/dialect/core/Transforms/LoopFusion.cpp` | 3 | Walks EdtOp + BarrierOp — ARTS structural |
| `LoopNormalization.cpp` | `lib/arts/dialect/core/Transforms/LoopNormalization.cpp` | 3 | ARTS-coupled: ForOp in match/apply, DbPatternMatchers |
| `LoopReordering.cpp` | `lib/arts/dialect/core/Transforms/LoopReordering.cpp` | 3 | ARTS-coupled: ForOp, DbPatternMatchers auto-detection |
| `StencilBoundaryPeeling.cpp` | `lib/arts/dialect/core/Transforms/StencilBoundaryPeeling.cpp` | 3 | ARTS-coupled: operates on ForOp loop bounds |

---

## 5. Passes — Verify

### Current: `lib/arts/passes/verify/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `ContractValidation.cpp` | `lib/arts/verify/ContractValidation.cpp` | 3 | Already listed in 4.2 |
| `VerifyDbCreated.cpp` | `lib/arts/verify/VerifyDbCreated.cpp` | 3 | |
| `VerifyDbLowered.cpp` | `lib/arts/verify/VerifyDbLowered.cpp` | 3 | |
| `VerifyEdtCreated.cpp` | `lib/arts/verify/VerifyEdtCreated.cpp` | 3 | |
| `VerifyEdtLowered.cpp` | `lib/arts/verify/VerifyEdtLowered.cpp` | 3 | |
| `VerifyEpochCreated.cpp` | `lib/arts/verify/VerifyEpochCreated.cpp` | 3 | |
| `VerifyEpochLowered.cpp` | `lib/arts/verify/VerifyEpochLowered.cpp` | 3 | |
| `VerifyForLowered.cpp` | `lib/arts/verify/VerifyForLowered.cpp` | 3 | |
| `VerifyLowered.cpp` | `lib/arts/verify/VerifyLowered.cpp` | 1+3 | Phase 1: check arts_rt; Phase 3: move |
| `VerifyMetadata.cpp` | `lib/arts/verify/VerifyMetadata.cpp` | 3 | |
| `VerifyMetadataIntegrity.cpp` | `lib/arts/verify/VerifyMetadataIntegrity.cpp` | 3 | |
| `VerifyPreLowered.cpp` | `lib/arts/verify/VerifyPreLowered.cpp` | 1+3 | Phase 1: check no arts_rt early; Phase 3: move |
| — | `lib/arts/verify/VerifySdeLowered.cpp` | NEW/2 | No sde.* ops survive after SdeToArts |
| — | `lib/arts/verify/VerifyStructuralLowered.cpp` | NEW/1 | No arts.edt/for/epoch survive after ArtsToRt |

---

## 6. Rewriter Infrastructure

### Current: `lib/arts/transforms/`

Rewriter libraries provide the implementation behind pass files.
They move with their owning dialect.

### 6.1 DB Rewriters: `lib/arts/transforms/db/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbBlockPlanResolver.cpp` | `lib/arts/dialect/core/Transforms/db/DbBlockPlanResolver.cpp` | 3 | |
| `DbIndexerBase.cpp` | `lib/arts/dialect/core/Transforms/db/DbIndexerBase.cpp` | 3 | |
| `DbLayoutStrategy.cpp` | `lib/arts/dialect/core/Transforms/db/DbLayoutStrategy.cpp` | 3 | Phase 1: add using decls |
| `DbPartitionPlanner.cpp` | `lib/arts/dialect/core/Transforms/db/DbPartitionPlanner.cpp` | 3 | |
| `DbRewriter.cpp` | `lib/arts/dialect/core/Transforms/db/DbRewriter.cpp` | 3 | |
| `DbTransforms.cpp` | `lib/arts/dialect/core/Transforms/db/DbTransforms.cpp` | 3 | |
| `block/DbBlockIndexer.cpp` | `lib/arts/dialect/core/Transforms/db/block/DbBlockIndexer.cpp` | 3 | |
| `block/DbBlockRewriter.cpp` | `lib/arts/dialect/core/Transforms/db/block/DbBlockRewriter.cpp` | 3 | |
| `elementwise/DbElementWiseIndexer.cpp` | `lib/arts/dialect/core/Transforms/db/elementwise/DbElementWiseIndexer.cpp` | 3 | |
| `elementwise/DbElementWiseRewriter.cpp` | `lib/arts/dialect/core/Transforms/db/elementwise/DbElementWiseRewriter.cpp` | 3 | |
| `stencil/DbStencilIndexer.cpp` | `lib/arts/dialect/core/Transforms/db/stencil/DbStencilIndexer.cpp` | 3 | |
| `stencil/DbStencilRewriter.cpp` | `lib/arts/dialect/core/Transforms/db/stencil/DbStencilRewriter.cpp` | 3 | |
| `strategy/BlockPartitionStrategy.cpp` | `lib/arts/dialect/core/Transforms/db/strategy/BlockPartitionStrategy.cpp` | 3 | |
| `strategy/CoarsePartitionStrategy.cpp` | `lib/arts/dialect/core/Transforms/db/strategy/CoarsePartitionStrategy.cpp` | 3 | |
| `strategy/FineGrainedPartitionStrategy.cpp` | `lib/arts/dialect/core/Transforms/db/strategy/FineGrainedPartitionStrategy.cpp` | 3 | |
| `strategy/PartitionStrategyFactory.cpp` | `lib/arts/dialect/core/Transforms/db/strategy/PartitionStrategyFactory.cpp` | 3 | |
| `strategy/StencilPartitionStrategy.cpp` | `lib/arts/dialect/core/Transforms/db/strategy/StencilPartitionStrategy.cpp` | 3 | |
| `transforms/GUIDRangeDetection.cpp` | DEAD | 3 | Dead code (identified in audit) |

#### DB Rewriter Headers: `include/arts/transforms/db/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbBlockPlanResolver.h` | `include/arts/dialect/core/Transforms/db/DbBlockPlanResolver.h` | 3 | |
| `DbIndexerBase.h` | `include/arts/dialect/core/Transforms/db/DbIndexerBase.h` | 3 | |
| `DbLayoutStrategy.h` | `include/arts/dialect/core/Transforms/db/DbLayoutStrategy.h` | 3 | |
| `DbPartitionPlanner.h` | `include/arts/dialect/core/Transforms/db/DbPartitionPlanner.h` | 3 | |
| `DbPartitionTypes.h` | `include/arts/dialect/core/Transforms/db/DbPartitionTypes.h` | 3 | |
| `DbPartitionWindowUtils.h` | `include/arts/dialect/core/Transforms/db/DbPartitionWindowUtils.h` | 3 | |
| `DbRewriter.h` | `include/arts/dialect/core/Transforms/db/DbRewriter.h` | 3 | |
| `DbTransforms.h` | `include/arts/dialect/core/Transforms/db/DbTransforms.h` | 3 | |
| `PartitionStrategy.h` | `include/arts/dialect/core/Transforms/db/PartitionStrategy.h` | 3 | |
| `block/DbBlockIndexer.h` | `include/arts/dialect/core/Transforms/db/block/DbBlockIndexer.h` | 3 | |
| `block/DbBlockRewriter.h` | `include/arts/dialect/core/Transforms/db/block/DbBlockRewriter.h` | 3 | |
| `elementwise/DbElementWiseIndexer.h` | `include/arts/dialect/core/Transforms/db/elementwise/DbElementWiseIndexer.h` | 3 | |
| `elementwise/DbElementWiseRewriter.h` | `include/arts/dialect/core/Transforms/db/elementwise/DbElementWiseRewriter.h` | 3 | |
| `stencil/DbStencilIndexer.h` | `include/arts/dialect/core/Transforms/db/stencil/DbStencilIndexer.h` | 3 | |
| `stencil/DbStencilRewriter.h` | `include/arts/dialect/core/Transforms/db/stencil/DbStencilRewriter.h` | 3 | |
| `transforms/GUIDRangeDetection.h` | DEAD | 3 | Dead code |

### 6.2 EDT Rewriters: `lib/arts/transforms/edt/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EdtInvariantCodeMotion.cpp` | `lib/arts/dialect/core/Transforms/edt/EdtInvariantCodeMotion.cpp` | 3 | |
| `EdtParallelSplitLowering.cpp` | `lib/arts/dialect/core/Transforms/edt/EdtParallelSplitLowering.cpp` | 3 | |
| `EdtPtrRematerialization.cpp` | `lib/arts/dialect/core/Transforms/edt/EdtPtrRematerialization.cpp` | 3 | |
| `EdtReductionLowering.cpp` | `lib/arts/dialect/core/Transforms/edt/EdtReductionLowering.cpp` | 3 | |
| `EdtRewriter.cpp` | `lib/arts/dialect/core/Transforms/edt/EdtRewriter.cpp` | 3 | |
| `EdtTaskLoopLowering.cpp` | `lib/arts/dialect/core/Transforms/edt/EdtTaskLoopLowering.cpp` | 3 | |
| `WorkDistributionUtils.cpp` | `lib/arts/dialect/core/Transforms/edt/WorkDistributionUtils.cpp` | 3 | |
| `tiling2d/Tile2DTaskLoopLowering.cpp` | `lib/arts/dialect/core/Transforms/edt/tiling2d/Tile2DTaskLoopLowering.cpp` | 3 | |

#### EDT Rewriter Headers: `include/arts/transforms/edt/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EdtICM.h` | `include/arts/dialect/core/Transforms/edt/EdtICM.h` | 3 | |
| `EdtParallelSplitLowering.h` | `include/arts/dialect/core/Transforms/edt/EdtParallelSplitLowering.h` | 3 | |
| `EdtPtrRematerialization.h` | `include/arts/dialect/core/Transforms/edt/EdtPtrRematerialization.h` | 3 | |
| `EdtReductionLowering.h` | `include/arts/dialect/core/Transforms/edt/EdtReductionLowering.h` | 3 | |
| `EdtRewriter.h` | `include/arts/dialect/core/Transforms/edt/EdtRewriter.h` | 3 | |
| `EdtTaskLoopLowering.h` | `include/arts/dialect/core/Transforms/edt/EdtTaskLoopLowering.h` | 3 | |
| `EdtTransforms.h` | `include/arts/dialect/core/Transforms/edt/EdtTransforms.h` | 3 | |
| `WorkDistributionUtils.h` | `include/arts/dialect/core/Transforms/edt/WorkDistributionUtils.h` | 3 | |

### 6.3 Dep Rewriters: `lib/arts/transforms/dep/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `JacobiAlternatingBuffersPattern.cpp` | `lib/arts/patterns/Transforms/dep/JacobiAlternatingBuffersPattern.cpp` | 3 | Pattern-specific |
| `Seidel2DWavefrontPattern.cpp` | `lib/arts/patterns/Transforms/dep/Seidel2DWavefrontPattern.cpp` | 3 | Pattern-specific |

#### Dep Rewriter Headers: `include/arts/transforms/dep/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DepTransform.h` | `include/arts/patterns/Transforms/dep/DepTransform.h` | 3 | |

### 6.4 Kernel Rewriters: `lib/arts/transforms/kernel/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `ElementwisePipelinePattern.cpp` | `lib/arts/patterns/Transforms/kernel/ElementwisePipelinePattern.cpp` | 3 | Pattern-specific |
| `MatmulReductionPattern.cpp` | `lib/arts/patterns/Transforms/kernel/MatmulReductionPattern.cpp` | 3 | Pattern-specific |
| `StencilTilingNDPattern.cpp` | `lib/arts/patterns/Transforms/kernel/StencilTilingNDPattern.cpp` | 3 | Pattern-specific |

#### Kernel Rewriter Headers: `include/arts/transforms/kernel/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `KernelTransform.h` | `include/arts/patterns/Transforms/kernel/KernelTransform.h` | 3 | |

### 6.5 Loop Rewriters: `lib/arts/transforms/loop/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `LoopNormalizer.cpp` | `lib/arts/dialect/sde/Transforms/loop/LoopNormalizer.cpp` | 2D | SDE-compatible |
| `PerfectNestLinearizationPattern.cpp` | `lib/arts/patterns/Transforms/loop/PerfectNestLinearizationPattern.cpp` | 3 | Pattern-specific |
| `SymmetricTriangularPattern.cpp` | `lib/arts/patterns/Transforms/loop/SymmetricTriangularPattern.cpp` | 3 | Pattern-specific |

#### Loop Rewriter Headers: `include/arts/transforms/loop/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `LoopNormalizer.h` | `include/arts/dialect/sde/Transforms/loop/LoopNormalizer.h` | 2D | |

### 6.6 Pattern Rewriters: `lib/arts/transforms/pattern/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `PatternTransform.cpp` | `lib/arts/patterns/Transforms/pattern/PatternTransform.cpp` | 3 | |

#### Pattern Rewriter Headers: `include/arts/transforms/pattern/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `PatternTransform.h` | `include/arts/patterns/Transforms/pattern/PatternTransform.h` | 3 | |

---

## 7. Analysis Layer

### 7.1 Top-Level Analysis: `lib/arts/analysis/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `AccessPatternAnalysis.cpp` | `lib/arts/patterns/Analysis/AccessPatternAnalysis.cpp` | 3 | Pattern-level |
| `AnalysisManager.cpp` | `lib/arts/analysis/AnalysisManager.cpp` | = | Shared framework |
| `InformationCache.cpp` | `lib/arts/analysis/InformationCache.cpp` | = | Shared framework |
| `StringAnalysis.cpp` | `lib/arts/analysis/StringAnalysis.cpp` | = | Shared |

#### Top-Level Analysis Headers: `include/arts/analysis/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `AccessPatternAnalysis.h` | `include/arts/patterns/Analysis/AccessPatternAnalysis.h` | 3 | Pattern-level |
| `Analysis.h` | = | — | Shared |
| `AnalysisDependencies.h` | = | — | Shared |
| `AnalysisManager.h` | = | — | Shared |
| `InformationCache.h` | = | — | Shared |
| `StringAnalysis.h` | = | — | Shared |

### 7.2 DB Analysis: `lib/arts/analysis/db/`

All ARTS-structural. Stay in shared `analysis/` or move to `dialect/core/Analysis/`.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbAliasAnalysis.cpp` | = | — | ARTS structural; accessed via AM |
| `DbAnalysis.cpp` | = | — | ARTS structural; accessed via AM |
| `DbDistributedEligibility.cpp` | = | — | ARTS structural; Phase 1: add using decls |
| `DbPatternMatchers.cpp` | `lib/arts/patterns/Analysis/DbPatternMatchers.cpp` | 3 | Pattern-level detection |
| `OwnershipProof.cpp` | = | — | ARTS structural |

#### DB Analysis Headers: `include/arts/analysis/db/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbAliasAnalysis.h` | = | — | ARTS structural |
| `DbAnalysis.h` | = | — | ARTS structural |
| `DbDistributedEligibility.h` | = | — | ARTS structural |
| `DbPartitionState.h` | = | — | ARTS structural |
| `DbPatternMatchers.h` | `include/arts/patterns/Analysis/DbPatternMatchers.h` | 3 | Pattern-level |
| `OwnershipProof.h` | = | — | ARTS structural |

### 7.3 EDT Analysis: `lib/arts/analysis/edt/`

All ARTS-structural. Stay in `analysis/`.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EdtAnalysis.cpp` | = | — | ARTS structural |
| `EdtDataFlowAnalysis.cpp` | = | — | ARTS structural |
| `EpochAnalysis.cpp` | = | — | ARTS structural |

#### EDT Analysis Headers: `include/arts/analysis/edt/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `EdtAnalysis.h` | = | — | ARTS structural |
| `EdtDataFlowAnalysis.h` | = | — | ARTS structural |
| `EdtInfo.h` | = | — | ARTS structural |
| `EpochAnalysis.h` | = | — | ARTS structural |

### 7.4 Graph Infrastructure: `lib/arts/analysis/graphs/`

All ARTS-structural. Stay in `analysis/`.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `db/DbAcquireNode.cpp` | = | — | |
| `db/DbAllocNode.cpp` | = | — | |
| `db/DbBlockInfoComputer.cpp` | = | — | |
| `db/DbDimAnalyzer.cpp` | = | — | |
| `db/DbGraph.cpp` | = | — | |
| `db/MemoryAccessClassifier.cpp` | = | — | |
| `db/PartitionBoundsAnalyzer.cpp` | = | — | |
| `edt/EdtEdge.cpp` | = | — | |
| `edt/EdtGraph.cpp` | = | — | |
| `edt/EdtNode.cpp` | = | — | |

#### Graph Headers: `include/arts/analysis/graphs/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `base/EdgeBase.h` | = | — | |
| `base/NodeBase.h` | = | — | |
| `db/DbAccessPattern.h` | `include/arts/patterns/Analysis/DbAccessPattern.h` | 3 | Pattern enum |
| `db/DbBlockInfoComputer.h` | = | — | |
| `db/DbDimAnalyzer.h` | = | — | |
| `db/DbGraph.h` | = | — | |
| `db/DbNode.h` | = | — | |
| `db/MemoryAccessClassifier.h` | = | — | |
| `db/PartitionBoundsAnalyzer.h` | = | — | |
| `edt/EdtEdge.h` | = | — | |
| `edt/EdtGraph.h` | = | — | |
| `edt/EdtNode.h` | = | — | |

### 7.5 Heuristics: `lib/arts/analysis/heuristics/`

All ARTS-structural (cost models for EDT/DB/Epoch decisions). Stay in `analysis/`.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbHeuristics.cpp` | = | — | |
| `DistributionHeuristics.cpp` | `lib/arts/patterns/Analysis/DistributionHeuristics.cpp` | 3 | Machine-aware strategy |
| `EdtHeuristics.cpp` | = | — | |
| `EpochHeuristics.cpp` | = | — | Phase 1: add using decls |
| `HeuristicUtils.cpp` | = | — | Shared |
| `PartitioningHeuristics.cpp` | = | — | |
| `PersistentRegionCostModel.cpp` | = | — | |
| `StructuredKernelPlanAnalysis.cpp` | = | — | |

#### Heuristic Headers: `include/arts/analysis/heuristics/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbHeuristics.h` | = | — | |
| `DistributionHeuristics.h` | `include/arts/patterns/Analysis/DistributionHeuristics.h` | 3 | |
| `EdtHeuristics.h` | = | — | |
| `EpochHeuristics.h` | = | — | |
| `HeuristicUtils.h` | = | — | |
| `PartitioningHeuristics.h` | = | — | |
| `PersistentRegionCostModel.h` | = | — | |
| `StructuredKernelPlanAnalysis.h` | = | — | |

### 7.6 Loop Analysis: `lib/arts/analysis/loop/`

Hybrid — core loop info is SDE-compatible, DB facade is ARTS-structural.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `LoopAnalysis.cpp` | = (split in Phase 2) | 2 | Extract SdeLoopAnalysis; keep DB facade |
| `LoopNode.cpp` | = (split in Phase 2) | 2 | Extract core loop info to SDE |

#### Loop Analysis Headers: `include/arts/analysis/loop/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `LoopAnalysis.h` | = (split in Phase 2) | 2 | SDE base + ARTS extension |
| `LoopNode.h` | = (split in Phase 2) | 2 | SDE base + ARTS extension |

### 7.7 Metadata Analysis: `lib/arts/analysis/metadata/`

All SDE-compatible (zero ARTS deps). Move to SDE analysis.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `LoopAnalyzer.cpp` | `lib/arts/dialect/sde/Analysis/metadata/LoopAnalyzer.cpp` | 2D | |
| `MemrefAnalyzer.cpp` | `lib/arts/dialect/sde/Analysis/metadata/MemrefAnalyzer.cpp` | 2D | |
| `MetadataAttacher.cpp` | `lib/arts/dialect/sde/Analysis/metadata/MetadataAttacher.cpp` | 2D | |
| `MetadataIO.cpp` | `lib/arts/dialect/sde/Analysis/metadata/MetadataIO.cpp` | 2D | |
| `MetadataManager.cpp` | `lib/arts/dialect/sde/Analysis/metadata/MetadataManager.cpp` | 2D | |
| `MetadataRegistry.cpp` | `lib/arts/dialect/sde/Analysis/metadata/MetadataRegistry.cpp` | 2D | |

#### Metadata Analysis Headers: `include/arts/analysis/metadata/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `AccessAnalyzer.h` | `include/arts/dialect/sde/Analysis/metadata/AccessAnalyzer.h` | 2D | |
| `DependenceAnalyzer.h` | `include/arts/dialect/sde/Analysis/metadata/DependenceAnalyzer.h` | 2D | |
| `LoopAnalyzer.h` | `include/arts/dialect/sde/Analysis/metadata/LoopAnalyzer.h` | 2D | |
| `MemrefAnalyzer.h` | `include/arts/dialect/sde/Analysis/metadata/MemrefAnalyzer.h` | 2D | |
| `MetadataAttacher.h` | `include/arts/dialect/sde/Analysis/metadata/MetadataAttacher.h` | 2D | |
| `MetadataIO.h` | `include/arts/dialect/sde/Analysis/metadata/MetadataIO.h` | 2D | |
| `MetadataManager.h` | `include/arts/dialect/sde/Analysis/metadata/MetadataManager.h` | 2D | |
| `MetadataRegistry.h` | `include/arts/dialect/sde/Analysis/metadata/MetadataRegistry.h` | 2D | |

### 7.8 Value Analysis: `lib/arts/analysis/value/`

SDE-compatible (pure MLIR value ops, except 1 DbGepOp ref).

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `ValueAnalysis.cpp` | `lib/arts/dialect/sde/Analysis/ValueAnalysis.cpp` | 2D | Phase 1: add using decl for DbGepOp |

#### Value Analysis Headers: `include/arts/analysis/value/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `ValueAnalysis.h` | `include/arts/dialect/sde/Analysis/ValueAnalysis.h` | 2D | |

---

## 8. Utilities

### Current: `lib/arts/utils/`

Shared utilities stay in `utils/`. They serve all dialect layers.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `DbUtils.cpp` | = | — | Critical cross-cutting hub; Phase 1: template instantiation for DepDbAcquireOp |
| `EdtUtils.cpp` | = | — | Shared |
| `LoopInvarianceUtils.cpp` | = | — | Shared |
| `LoopUtils.cpp` | = | — | Shared |
| `LoweringContractUtils.cpp` | = | — | Shared |
| `PassInstrumentation.cpp` | = | — | Shared |
| `PatternSemantics.cpp` | = | — | Shared |
| `RemovalUtils.cpp` | = | — | Shared |
| `Utils.cpp` | = | — | Shared |
| `abstract_machine/AbstractMachine.cpp` | = | — | Shared |
| `benchmarks/CartsBenchmarks.cpp` | = | — | Shared |
| `metadata/AccessStats.cpp` | = | — | Shared |
| `metadata/IdRegistry.cpp` | = | — | Shared |
| `metadata/LocationMetadata.cpp` | = | — | Shared |
| `metadata/LoopMetadata.cpp` | = | — | Shared |
| `metadata/MemrefMetadata.cpp` | = | — | Shared |
| `metadata/ValueMetadata.cpp` | = | — | Shared |

#### Utility Headers: `include/arts/utils/`

All stay in place — shared across all dialect layers.

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `BlockedAccessUtils.h` | = | — | |
| `DbUtils.h` | = | — | |
| `Debug.h` | = | — | |
| `EdtUtils.h` | = | — | |
| `LoopInvarianceUtils.h` | = | — | |
| `LoopUtils.h` | = | — | |
| `LoweringContractUtils.h` | = | — | |
| `OperationAttributes.h` | = | — | |
| `PartitionPredicates.h` | = | — | |
| `PassInstrumentation.h` | = | — | |
| `PatternSemantics.h` | = | — | |
| `RemovalUtils.h` | = | — | |
| `StencilAttributes.h` | = | — | |
| `Utils.h` | = | — | |
| `abstract_machine/AbstractMachine.h` | = | — | |
| `benchmarks/CartsBenchmarks.h` | = | — | |
| `metadata/AccessStats.h` | = | — | |
| `metadata/IdRegistry.h` | = | — | |
| `metadata/LocationMetadata.h` | = | — | |
| `metadata/LoopMetadata.h` | = | — | |
| `metadata/MemrefMetadata.h` | = | — | |
| `metadata/Metadata.h` | = | — | |
| `metadata/ValueMetadata.h` | = | — | |
| `testing/CartsTest.h` | = | — | |

---

## 9. Compile Driver

### Current: `tools/compile/`

| Current Location | Target | Phase | Notes |
|---|---|---|---|
| `Compile.cpp` | = | 1+2 | Phase 1: register arts_rt; Phase 2: add SDE stages |

---

## 10. Summary Statistics

### File Movement by Phase

| Phase | Files Created | Files Modified | Files Moved | Files Removed |
|---|---|---|---|---|
| Phase 1 (arts_rt) | 7 | ~18 | 0 | 0 |
| Phase 2 (SDE) | 12 | ~8 | ~11 | 0 |
| Phase 3 (reorg) | 0 | ~30 CMakeLists | ~130 | 2 (dead) |

### File Count by Target Directory

| Target Directory | .cpp | .h/.td | Total | Description |
|---|---|---|---|---|
| `dialect/sde/IR/` | 2 | 6 | 8 | NEW: SDE dialect definition |
| `dialect/sde/Transforms/` | 9 | 2 | 11 | NEW + moved: SDE transforms |
| `dialect/sde/Analysis/` | 9 | 10 | 19 | Moved: metadata, value analysis |
| `dialect/core/IR/` | 1 | 5 | 6 | Moved: core ARTS dialect |
| `dialect/core/Transforms/` | 47 | 26 | 73 | Moved: all ARTS-structural passes + rewriters |
| `dialect/core/Conversion/` | 2 | 0 | 2 | NEW + moved: OmpToArts, SdeToArts |
| `dialect/rt/IR/` | 2 | 3 | 5 | NEW: arts_rt dialect |
| `dialect/rt/Transforms/` | 4 | 1 | 5 | Moved: codegen post-lowering |
| `dialect/rt/Conversion/ArtsToRt/` | 5 | 1 | 6 | Moved: EDT/epoch/DB/parallel lowering |
| `dialect/rt/Conversion/RtToLLVM/` | 1 | 0 | 1 | NEW: 14 patterns from ConvertArtsToLLVM |
| `patterns/Transforms/` | 10 | 3 | 13 | Moved: pattern detection + kernel rewrites |
| `patterns/Analysis/` | 3 | 4 | 7 | Moved: pattern matchers, access analysis |
| `general/Transforms/` | 8 | 0 | 8 | Moved: DCE, CSE wrappers, memref raising |
| `verify/` | 13 | 0 | 13 | Moved: 11 existing + 2 new verification |
| `analysis/` (shared) | 22 | 23 | 45 | Stays: DB/EDT/epoch analysis, graphs, heuristics |
| `utils/` (shared) | 17 | 24 | 41 | Stays: all shared utilities |
| `codegen/` (shared) | 1 | 3 | 4 | Stays: ArtsCodegen context |
| `passes/transforms/` (residual) | 2 | 2 | 4 | Stays: ConvertArtsToLLVM pass driver + internal |

### Phase 1 Cross-Reference Audit (15 files needing `using` declarations)

| File | Rt Op References | Risk |
|---|---|---|
| `EpochLowering.cpp` | 96 refs to 8 rt ops | Critical |
| `EdtLowering.cpp` | 53 refs to 10 rt ops | Critical |
| `ConvertArtsToLLVMPatterns.cpp` | 51 refs to 14 rt ops | Critical |
| `ForLowering.cpp` | ~10 refs to 3 rt ops | Medium |
| `EdtLoweringSupport.cpp` | 1 ref (DepDbAcquireOp) | Low |
| `EpochOptScheduling.cpp` | 1 ref (DepDbAcquireOp) | Low |
| `DbLayoutStrategy.cpp` | uses via DbGepOp | Low |
| `ValueAnalysis.cpp` | 2 refs (DbGepOp) | Low |
| `DbDistributedEligibility.cpp` | uses via DbGepOp | Low |
| `DataPtrHoistingSupport.cpp` | uses DepDbAcquireOp | Low |
| `AliasScopeGen.cpp` | uses DbGepOp | Low |
| `EdtLoweringInternal.h` | type declarations | Low |
| `DbUtils.cpp` | template instantiation | Medium |
| `EpochHeuristics.cpp` | 1 ref | Low |
| `Compile.cpp` | register dialect | Low |

---

## 11. Target Directory Tree (Complete)

```
include/arts/
  dialect/
    sde/IR/
      SdeDialect.td                     NEW Phase 2
      SdeOps.td                         NEW Phase 2
      SdeTypes.td                       NEW Phase 2
      SdeAttrs.td                       NEW Phase 2
      SdeEffects.h                      NEW Phase 2
      SdeDialect.h                      NEW Phase 2
      CMakeLists.txt                    NEW Phase 2
    sde/Analysis/
      metadata/
        AccessAnalyzer.h                <- analysis/metadata/
        DependenceAnalyzer.h            <- analysis/metadata/
        LoopAnalyzer.h                  <- analysis/metadata/
        MemrefAnalyzer.h                <- analysis/metadata/
        MetadataAttacher.h              <- analysis/metadata/
        MetadataIO.h                    <- analysis/metadata/
        MetadataManager.h              <- analysis/metadata/
        MetadataRegistry.h             <- analysis/metadata/
      ValueAnalysis.h                   <- analysis/value/
    sde/Transforms/
      loop/
        LoopNormalizer.h                <- transforms/loop/
    core/IR/
      ArtsDialect.td                    <- Dialect.td
      ArtsOps.td                        <- Ops.td (reduced: 21 ops)
      ArtsAttrs.td                      <- Attributes.td (reduced)
      ArtsTypes.td                      <- Types.td
      ArtsDialect.h                     <- Dialect.h
      CMakeLists.txt                    NEW Phase 3
    core/Transforms/
      db/                               <- transforms/db/ (all headers)
      edt/                              <- transforms/edt/ (all headers)
      BlockLoopStripMiningInternal.h    <- passes/opt/db/
      DbPartitioningInternal.h          <- passes/opt/db/
      EpochOptInternal.h                <- passes/opt/epoch/
    core/Conversion/
      (no public headers)
    rt/IR/
      RtDialect.td                      NEW Phase 1
      RtOps.td                          NEW Phase 1
      RtDialect.h                       NEW Phase 1
      CMakeLists.txt                    NEW Phase 1
    rt/Transforms/
      DataPtrHoistingInternal.h         <- passes/opt/codegen/
    rt/Conversion/
      ArtsToRt/
        EdtLoweringInternal.h           <- passes/transforms/
      RtToLLVM/
        (no public headers)
  patterns/
    Analysis/
      AccessPatternAnalysis.h           <- analysis/
      DbPatternMatchers.h              <- analysis/db/
      DbAccessPattern.h                <- analysis/graphs/db/
      DistributionHeuristics.h          <- analysis/heuristics/
    Transforms/
      dep/DepTransform.h               <- transforms/dep/
      kernel/KernelTransform.h          <- transforms/kernel/
      pattern/PatternTransform.h        <- transforms/pattern/
  analysis/                             STAYS (shared framework)
  codegen/                              STAYS (shared ArtsCodegen)
  passes/                               STAYS (Passes.h, Passes.td, internal headers)
  utils/                                STAYS (all shared utilities)

lib/arts/
  dialect/
    sde/
      IR/
        SdeDialect.cpp                  NEW Phase 2
        SdeOps.cpp                      NEW Phase 2
        CMakeLists.txt                  NEW Phase 2
      Analysis/
        metadata/
          LoopAnalyzer.cpp              <- analysis/metadata/
          MemrefAnalyzer.cpp            <- analysis/metadata/
          MetadataAttacher.cpp          <- analysis/metadata/
          MetadataIO.cpp                <- analysis/metadata/
          MetadataManager.cpp           <- analysis/metadata/
          MetadataRegistry.cpp          <- analysis/metadata/
        ValueAnalysis.cpp               <- analysis/value/
        CMakeLists.txt                  NEW Phase 2D
      Transforms/
        CollectMetadata.cpp             <- passes/transforms/
        ConvertOpenMPToSde.cpp          NEW Phase 2B
        RaiseToLinalg.cpp               NEW Phase 2C (deferred)
        RaiseToTensor.cpp               NEW Phase 2C (deferred)
        SdeOpt.cpp                      NEW Phase 2C (deferred)
        CMakeLists.txt                  NEW Phase 2
    core/
      IR/
        Dialect.cpp                     <- Dialect.cpp (reduced)
        CMakeLists.txt                  NEW Phase 3
      Transforms/
        BlockLoopStripMining.cpp        <- passes/opt/db/
        BlockLoopStripMiningSupport.cpp <- passes/opt/db/
        Concurrency.cpp                 <- passes/transforms/
        CreateDbs.cpp                   <- passes/transforms/
        CreateEpochs.cpp                <- passes/transforms/
        DbDistributedOwnership.cpp      <- passes/opt/db/
        DbModeTightening.cpp            <- passes/opt/db/
        DbPartitioning.cpp              <- passes/opt/db/
        DbPartitioningSupport.cpp       <- passes/opt/db/
        DbScratchElimination.cpp        <- passes/opt/db/
        DbTransformsPass.cpp            <- passes/opt/db/
        DistributedHostLoopOutlining.cpp <- passes/transforms/
        EdtAllocaSinking.cpp            <- passes/opt/edt/
        EdtDistribution.cpp             <- passes/transforms/
        EdtICM.cpp                      <- passes/opt/edt/
        EdtOrchestrationOpt.cpp         <- passes/opt/edt/
        EdtPtrRematerialization.cpp     <- passes/transforms/
        EdtStructuralOpt.cpp            <- passes/opt/edt/
        EdtTransformsPass.cpp           <- passes/opt/edt/
        EpochOpt.cpp                    <- passes/opt/epoch/
        EpochOptCpsChain.cpp            <- passes/opt/epoch/
        EpochOptScheduling.cpp          <- passes/opt/epoch/
        EpochOptStructural.cpp          <- passes/opt/epoch/
        EpochOptSupport.cpp             <- passes/opt/epoch/
        ForLowering.cpp                 <- passes/transforms/
        ForOpt.cpp                      <- passes/transforms/
        Hoisting.cpp                    <- passes/opt/codegen/
        LoopFusion.cpp                  <- passes/opt/loop/
        PersistentStructuredRegion.cpp  <- passes/opt/edt/
        StructuredKernelPlanPass.cpp    <- passes/opt/edt/
        db/                             <- transforms/db/ (17 .cpp files)
        edt/                            <- transforms/edt/ (8 .cpp files)
        CMakeLists.txt                  NEW Phase 3
      Conversion/
        OmpToArts/
          ConvertOpenMPToArts.cpp       <- passes/transforms/
        SdeToArts/
          SdeToArtsPatterns.cpp         NEW Phase 2B
        CMakeLists.txt                  NEW Phase 3
    rt/
      IR/
        RtDialect.cpp                   NEW Phase 1
        RtOps.cpp                       NEW Phase 1
        CMakeLists.txt                  NEW Phase 1
      Transforms/
        DataPtrHoisting.cpp             <- passes/opt/codegen/
        DataPtrHoistingSupport.cpp      <- passes/opt/codegen/
        GuidRangCallOpt.cpp             <- passes/opt/codegen/
        RuntimeCallOpt.cpp              <- passes/opt/codegen/
        CMakeLists.txt                  NEW Phase 3
      Conversion/
        ArtsToRt/
          DbLowering.cpp               <- passes/transforms/
          EdtLowering.cpp              <- passes/transforms/
          EdtLoweringSupport.cpp        <- passes/transforms/
          EpochLowering.cpp            <- passes/transforms/
          ParallelEdtLowering.cpp       <- passes/transforms/
          CMakeLists.txt                NEW Phase 3
        RtToLLVM/
          RtToLLVMPatterns.cpp          NEW Phase 1
          CMakeLists.txt                NEW Phase 1
  patterns/
    Analysis/
      AccessPatternAnalysis.cpp         <- analysis/
      DbPatternMatchers.cpp            <- analysis/db/
      DistributionHeuristics.cpp        <- analysis/heuristics/
    Transforms/
      DepTransforms.cpp                 <- passes/transforms/
      KernelTransforms.cpp              <- passes/opt/kernel/
      PatternDiscovery.cpp              <- passes/transforms/
      StencilBoundaryPeeling.cpp        <- passes/opt/loop/
      dep/
        JacobiAlternatingBuffersPattern.cpp   <- transforms/dep/
        Seidel2DWavefrontPattern.cpp          <- transforms/dep/
      kernel/
        ElementwisePipelinePattern.cpp  <- transforms/kernel/
        MatmulReductionPattern.cpp      <- transforms/kernel/
        StencilTilingNDPattern.cpp      <- transforms/kernel/
      loop/
        PerfectNestLinearizationPattern.cpp <- transforms/loop/
        SymmetricTriangularPattern.cpp  <- transforms/loop/
      pattern/
        PatternTransform.cpp            <- transforms/pattern/
    CMakeLists.txt                      NEW Phase 3
  general/
    Transforms/
      AliasScopeGen.cpp                 <- passes/opt/codegen/
      DeadCodeElimination.cpp           <- passes/opt/general/
      HandleDeps.cpp                    <- passes/opt/general/
      Inliner.cpp                       <- passes/opt/general/
      LoopVectorizationHints.cpp        <- passes/opt/codegen/
      LoweringContractCleanup.cpp       <- passes/transforms/
      RaiseMemRefDimensionality.cpp     <- passes/opt/general/
      ScalarReplacement.cpp             <- passes/opt/codegen/
    CMakeLists.txt                      NEW Phase 3
  verify/
    ContractValidation.cpp              <- passes/opt/db/
    VerifyDbCreated.cpp                 <- passes/verify/
    VerifyDbLowered.cpp                 <- passes/verify/
    VerifyEdtCreated.cpp                <- passes/verify/
    VerifyEdtLowered.cpp                <- passes/verify/
    VerifyEpochCreated.cpp              <- passes/verify/
    VerifyEpochLowered.cpp              <- passes/verify/
    VerifyForLowered.cpp                <- passes/verify/
    VerifyLowered.cpp                   <- passes/verify/
    VerifyMetadata.cpp                  <- passes/verify/
    VerifyMetadataIntegrity.cpp         <- passes/verify/
    VerifyPreLowered.cpp                <- passes/verify/
    VerifySdeLowered.cpp                NEW Phase 2
    VerifyStructuralLowered.cpp         NEW Phase 1
    CMakeLists.txt                      NEW Phase 3
  analysis/                             STAYS (shared: db/, edt/, graphs/, heuristics/, loop/)
  codegen/                              STAYS (shared ArtsCodegen)
  passes/transforms/                    RESIDUAL (ConvertArtsToLLVM driver + patterns)
  utils/                                STAYS (all shared utilities)
```
