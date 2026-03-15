# Compiler Passes

- Cleanup passes ask: "Can I simplify structure without changing meaning?" They resolve canonical form, dead code, and repeated IR noise.
- Analysis/metadata passes ask: "What facts can I extract from the current IR?" They resolve metadata, graph facts, and refined contract facts.
- Pattern passes ask: "Does this IR belong to a known reusable semantic family?" They resolve pattern contracts and family-specific rewrites.
- Controller passes ask: "Given the contract and machine, what execution/layout policy should we choose?" They resolve EDT shape, DB partitioning, ownership, and epoch structure.
- Lowering passes ask: "How do I encode the chosen policy in lower-level IR?" They resolve ARTS ops into concrete DB/EDT/epoch/runtime forms.

## 1. canonicalize-memrefs

- LowerAffine: asks "Can affine ops be normalized into the common SCF/arithmetic form we use downstream?" Resolves a lower, uniform loop/memref form.
- CSE, Inliner, ArtsInliner, PolygeistCanonicalize, CanonicalizeMemrefs, DCE, CSE: ask "Can redundant IR be removed and memref usage normalized?" Resolve a cleaner IR substrate for analysis.

## 2. collect-metadata

- replaceAffineCFG / RaiseSCFToAffine / CSE: ask "Can loop/control structure be raised into a form that exposes access and dependence facts?" Resolve analyzable affine-style structure.
- CollectMetadataPass: asks "What loop, memref, access, and dependence metadata can we attach now?" Resolves pre-pattern metadata used to seed discovery.

## 3. initial-cleanup

- LowerAffine, CSE, CanonicalizeFor: ask "Can we reduce structural noise before semantic conversion?" Resolve a simpler pre-ARTS loop form.

## 4. openmp-to-arts

- ConvertOpenMPtoArts: asks "Which OpenMP regions map to ARTS loops, EDTs, and dependence constructs?" Resolves the first ARTS-level parallel IR.
- DCE, CSE: resolve cleanup after conversion.

## 5. pattern-pipeline

This is the stage that should own semantic family reasoning.

- PatternDiscoveryPass(seed): asks "From current IR plus metadata, which ops are candidates for a known family?" Resolves initial pattern contracts with provisional family/revision.
- DepTransforms: asks "Does a known dependence family require schedule/dependence rewrites now?" Resolves dependence-shaped rewrites like Jacobi/Seidel-style families.
- LoopNormalization: asks "Can candidate loops be normalized into the canonical form expected by pattern validators?" Resolves normalized loop structure.
- StencilBoundaryPeeling: asks "Can we separate interior from boundary so stencil families become explicit?" Resolves peel structure for clean interior kernels.
- LoopReordering: asks "Is there a loop order that better matches locality/distribution/metadata intent?" Resolves improved loop order before kernel transforms.
- PatternDiscoveryPass(refine): asks "After normalization/peeling/reordering, can we strengthen or upgrade the contract?" Resolves refined pattern contracts and stronger revisions.
- KernelTransforms: asks "Does a known kernel family require kernel-form rewrites now?" Resolves elementwise pipeline, stencil tiling, matmul-reduction style rewrites.
- CSE: resolves cleanup after semantic rewrites.

## 6. edt-transforms

- EdtPass(runAnalysis=false): asks "Can EDT IR be structurally simplified/rebuilt now that patterns are fixed?" Resolves normalized EDT structure.
- EdtInvariantCodeMotion: asks "What can move out of EDT bodies safely?" Resolves EDT-invariant hoisting.
- DCE, SymbolDCE, CSE: resolve cleanup.
- EdtPtrRematerialization: asks "Can pointer materialization be delayed/recreated nearer to use?" Resolves cleaner EDT bodies and later lowering opportunities.

## 7. create-dbs

- DistributedHostLoopOutlining when enabled: asks "Which host loops must enter the regular ARTS loop/DB path for multinode/distributed correctness?" Resolves host-loop bridging into ARTS form.
- CreateDbs: asks "Which values become DBs, where are acquires/releases, and which DBs should carry lowering contracts?" Resolves explicit DB IR.
- PolygeistCanonicalize, CSE, SymbolDCE, Mem2Reg, PolygeistCanonicalize: resolve cleanup after DB materialization.

## 8. db-opt

- DbPass: asks "Can DB ops be simplified, normalized, or cleaned without changing the selected semantics?" Resolves high-level DB IR cleanup.
- PolygeistCanonicalize, CSE, Mem2Reg: resolve surrounding cleanup.

## 9. edt-opt

- EdtPass(runAnalysis=true): asks "Given the current IR, what EDT analysis facts and cleanups can we apply?" Resolves analyzed EDT normalization.
- LoopFusion: asks "Can adjacent loop/EDT structure be fused without violating contracts?" Resolves profitable fusion.
- CSE: cleanup.

## 10. concurrency

- ConcurrencyPass: asks "Where should logical task concurrency be introduced?" Resolves ARTS concurrency structure.
- ForOptPass: asks "Can arts.for structure be simplified before distribution/lowering?" Resolves cleaner loop/task form.
- Canonicalization passes resolve cleanup.

## 11. edt-distribution

- EdtDistributionPass: asks "Which distribution strategy should each task loop use?" Resolves EDT distribution kind/pattern.
- ForLowering: asks "Given the existing contract and distribution choice, how do I rewrite loop acquires and slices?" Resolves loop-to-task lowering. It should consume contracts only, not rediscover stencil/wavefront logic.

## 12. concurrency-opt

This is the main controller stage.

- EdtPass, DCE, Canonicalize, CSE: ask "Can task IR be simplified before final DB decisions?" Resolve cleaned task structure.
- EpochOpt: asks "Can epoch structure be simplified before final shaping?" Resolves smaller epoch overhead.
- DbPartitioning: asks "Given refined contracts, DB analysis, and machine facts, should each DB be coarse, fine-grained, block, or stencil, and with what block shape?" Resolves partition mode and partition geometry.
- DistributedDbOwnership when enabled: asks "Which node owns which distributed DB region?" Resolves distributed ownership policy.
- DbPass, DbOptsPass: ask "After policy selection, what DB cleanups or local rewrites are now legal?" Resolve post-partition DB cleanup.
- BlockLoopStripMining: asks "Should block loops be strip-mined to better match selected block geometry?" Resolves block-oriented loop form.
- ArtsHoisting: asks "What runtime/data operations can move out safely now?" Resolves hoisting after partitioning.
- EdtAllocaSinking, DCE, Mem2Reg: cleanup and stack-use normalization.

## 13. epochs

- CreateEpochs: asks "Where do synchronization epochs need to exist after final concurrency and DB policy are known?" Resolves explicit epoch structure.
- Canonicalization passes resolve cleanup.

## 14. pre-lowering

- EdtAllocaSinking: asks "Can allocas move closer to their final EDT use?" Resolves cleaner lowering input.
- ParallelEdtLowering: asks "How do high-level parallel EDT ops become lower-level runtime task forms?" Resolves parallel EDT lowering.
- DbLowering: asks "How do DB ops and contracts become lower-level DB runtime ops?" Resolves DB lowering while consuming contracts.
- EdtLowering: asks "How do remaining EDT ops/dependencies become lower-level runtime ops?" Resolves EDT lowering.
- LICM, DataPtrHoisting, ScalarReplacement: ask standard codegen questions about invariant movement and scalarization.
- EpochLowering: asks "How do epochs become lower-level runtime synchronization?" Resolves epoch lowering.

## 15. arts-to-llvm

- ConvertArtsToLLVM: asks "How do all remaining ARTS dialect ops map to LLVM-level IR/runtime calls?" Resolves final dialect conversion.
- DataPtrHoisting, canonicalization, Mem2Reg, ControlFlowSink: resolve post-conversion cleanup and codegen readiness.

## What each major subsystem should own

- Pattern ownership: PatternDiscovery, DepTransforms, KernelTransforms.
- Contract refinement ownership: pre-DB pattern pipeline seeds contracts; post-DB DbAnalysis refines them in the same language (`DbAnalysis.h`).
- Policy ownership: EdtDistribution, DbPartitioning, DistributedDbOwnership, CreateEpochs.
- Lowering ownership: ForLowering, DbLowering, EdtLowering, EpochLowering, ConvertArtsToLLVM.

## The clean question each layer should ask

- Pattern layer: "What family is this?"
- Analysis layer: "What facts refine that family?"
- Controller layer: "What policy follows from that family plus machine facts?"
- Lowering layer: "How do I encode that chosen policy?"
