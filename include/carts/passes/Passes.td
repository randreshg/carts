///==========================================================================///
/// File: Passes.td
///
/// This file defines pass declarations for the ARTS dialect.
///==========================================================================///
#ifndef CARTS_PASSES
#define CARTS_PASSES

include "mlir/Pass/PassBase.td"
include "mlir/Rewrite/PassUtil.td"

def DeadCodeElimination : Pass<"arts-dce", "mlir::ModuleOp"> {
  let summary = "Remove dead ARTS objects and shared helper IR";
  let description = [{
    Performs dead code elimination for ARTS objects and shared helper IR:
    - Removes raw-dead ARTS DB, EDT, acquire, release, and undef operations
    - Runs shared dialect-neutral cleanup for dead memref helper IR
    - Removes dead private symbols

    SDE input cleanup uses the SDE-owned `sde-dead-state-cleanup` pass instead
    of invoking ARTS object cleanup before ARTS objects exist.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createDCEPass()";
}

def DbModeTightening : Pass<"db-mode-tightening", "mlir::ModuleOp"> {
  let summary = "Tighten DB access modes and infer storage types";
  let description =
      [{Tightens the access mode (in / out / inout)
            of DB allocations and acquires based on the actual load /
        store operations performed on them, and infers storage types.}];
  let dependentDialects =
      ["::mlir::memref::MemRefDialect", "::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let options = [
    Option<"forceInout", "force-inout", "bool", "false",
           "Force all stores to use inout mode (disables mode tightening to out)">
  ];
  let constructor = "mlir::carts::arts::createDbModeTighteningPass(nullptr)";
}

def DbScratchElimination : Pass<"db-scratch-elimination", "mlir::ModuleOp"> {
  let summary = "Eliminate task-private single-element scratch DBs";
  let description = [{
    Replaces task-private single-element scratch DBs with local allocas.
  }];
  let dependentDialects =
      ["::mlir::memref::MemRefDialect", "::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createDbScratchEliminationPass()";
}

def DbOwnerMapRealization
    : Pass<"db-owner-map-realization", "mlir::ModuleOp"> {
  let summary = "Realize ARTS owner-map and memory-placement facts on "
                "distribution-eligible DB allocations";
  let description = [{
    Selects host-level DB allocations whose committed SDE/CODIR owner/block plan
    can be realized across nodes, marks them `distributed`, and projects the plan
    into ARTS owner-map and `db_memory_placement` facts. It does not choose a
    distribution family or recompute block shape.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect", "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::arts::createDbOwnerMapRealizationPass(nullptr)";
}

def DbCommitDistributedDeps
    : Pass<"db-commit-distributed-deps", "mlir::ModuleOp"> {
  let summary = "Commit DB-mode and halo facts on every distributed acquire";
  let description = [{
    Commits the runtime DB mode (ro/ew/rw) on every `arts.db_acquire` whose
    source DB is distributed, and the per-slot `halo_slice` window on
    distributed stencil acquires, so ARTS-RT lowering consumes committed facts
    with no fallback inference. It only fills committed facts; acquires already
    carrying a mode verdict keep it.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createDbCommitDistributedDepsPass()";
}

def DbTransforms : Pass<"db-transforms", "mlir::ModuleOp"> {
  let summary = "Post-DB transforms (contracts, halo, cleanup)";
  let description = [{
    Hosts DB-level transformations that run after DB partitioning and
    distributed ownership decisions are known. Current transforms:
    - Contract persistence: persist refined AcquireContractSummary back to IR.
    - Stencil halo consolidation.
    - DB lifetime shortening.
    - Dead DB elimination.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createDbTransformsPass(nullptr)";
}

def PartialReductionSplitMaterialization
    : Pass<"partial-reduction-split-materialization", "mlir::ModuleOp"> {
  let summary = "Materialize partial-reduction split plans into ARTS objects";
  let description = [{
    Consumes `partialReductionSplitRequired` on supported ARTS partial
    reduction EDTs by creating reduction-tile partial DBs, split worker EDTs,
    and combine EDTs before ARTS-RT lowering.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect", "::mlir::memref::MemRefDialect",
       "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createPartialReductionSplitMaterializationPass()";
}

def MatmulContractionMaterialization
    : Pass<"matmul-contraction-materialization", "mlir::ModuleOp"> {
  let summary = "Split chained-matmul G contraction into per-tile producers";
  let description = [{
    Rewrites a block matmul consumer marked with contraction split facts that
    reads its contraction operand from a coarse `replicatedRead` copy into
    per-(owner-block, k-tile) partial-product producer EDTs reading block-native
    from the `perBlockReplicated` all-gather replica, plus a per-block summing
    settle that reduces the partials into the settled block with `arith.addf`.
    Each producer and the settle acquire every DB outside the EDT as a block-arg
    dependency, so no EDT body GEPs an outer DB alloc. The trigger is the generic
    ARTS plan shape (`partialReductionDims`, block dependencies, and a matching
    per-block replica), not benchmark names or source paths.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect", "::mlir::memref::MemRefDialect",
       "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createMatmulContractionMaterializationPass()";
}

def DistributedLaunchConsistency
    : Pass<"distributed-launch-consistency", "mlir::ModuleOp"> {
  let summary = "Keep distributed EDT placement consistent with DB ownership";
  let description = [{
    Reconciles ARTS EDT placement with distributed DB ownership decisions.
    Internode EDTs that depend on DBs rejected for distributed ownership are
    localized to intranode execution, except for small read-only coarse DBs
    that are intentionally allowed to travel as cheap dependencies.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::arith::ArithDialect",
       "::mlir::memref::MemRefDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createDistributedLaunchConsistencyPass()";
}

def VerifyDistributedDbPlacement
    : Pass<"verify-distributed-db-placement", "mlir::ModuleOp"> {
  let summary = "Verify distributed DB owner-map placement contracts";
  let description = [{
    Rejects distributed DB allocations that reach the ARTS/ARTS-RT boundary
    without an explicit owner-map policy, owner-map version, owner dimensions,
    block shape, and non-rejected distributed ownership status.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createVerifyDistributedDbPlacementPass()";
}

def VerifyArtsCdag : Pass<"verify-arts-cdag", "mlir::ModuleOp"> {
  let summary = "Verify the ARTS canonical-owner DAG after epoch creation";
  let description = [{
    Fail-closed verification of the ARTS memory model after CreateEpochs:
    - every distributed `arts.db_acquire` carries a committed runtime DB mode;
    - distributed DB owner-map / placement facts are consistent;
    - SDE-partitioned MUs are preserved (not coarsened or localized);
    - single-writer-multiple-reader holds per distributed DB block grain;
    - the EDT happens-before graph is acyclic.
    Missing facts are rejected rather than repaired.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createVerifyArtsCdagPass()";
}

def CreateDbs : Pass<"create-dbs", "mlir::ModuleOp"> {
  let summary = "Identify Dbs in the program";
  let dependentDialects = [
    "::mlir::memref::MemRefDialect", "::mlir::LLVM::LLVMDialect", "::mlir::func::FuncDialect",
    "::mlir::carts::arts::ArtsDialect", "::mlir::arith::ArithDialect", "::mlir::polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::carts::arts::createCreateDbsPass(nullptr)";
}

def EdtStructuralOpt : Pass<"edt-structural-opt", "mlir::ModuleOp"> {
  let summary = "Optimize EDTs";
  let dependentDialects = [
    "::mlir::memref::MemRefDialect", "::mlir::LLVM::LLVMDialect", "::mlir::func::FuncDialect",
    "::mlir::carts::arts::ArtsDialect", "::mlir::arith::ArithDialect", "::mlir::polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::carts::arts::createEdtStructuralOptPass(nullptr, true)";
  let options = [Option<"runAnalysis", "run-analysis", "bool",
                        /*default=*/"true", "Enable EDT analysis.">];
}

def RealizeEdtDistributionPlan
    : Pass<"realize-edt-distribution-plan", "mlir::ModuleOp"> {
  let summary = "Author the EDT distribution plan from committed facts";
  let description = [{
    Reads the committed dep pattern and access-window facts forwarded onto each
    `arts.edt` and authors the distribution realization plan: the distribution
    family, the distribution version, and the block-halo capability marker. It
    consumes already-committed facts to author ARTS structure and does not
    classify movement or select a distribution family from source patterns.
  }];
  let dependentDialects = ["::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createRealizeEdtDistributionPlanPass()";
}

def EdtAllocaSinking : Pass<"edt-alloca-sinking", "mlir::ModuleOp"> {
  let summary = "Clone external stack allocas into EDT regions";
  let dependentDialects =
      ["::mlir::memref::MemRefDialect", "::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createEdtAllocaSinkingPass()";
}

def EdtTransforms : Pass<"edt-transforms", "mlir::ModuleOp"> {
  let summary = "Post-DB EDT transforms (granularity, affinity, dependency cleanup)";
  let description = [{
    Hosts EDT-level transformations that run after DB partitioning and
    mode decisions are known. Current transforms:
    - Task granularity diagnostics.
    - Dead dependency elimination.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createEdtTransformsPass(nullptr)";
}

def CreateEpochs : Pass<"create-epochs", "mlir::ModuleOp"> {
  let summary = "Analyzes the program and creates ARTS epochs";
  let dependentDialects = [
    "::mlir::memref::MemRefDialect", "::mlir::LLVM::LLVMDialect", "::mlir::func::FuncDialect",
    "::mlir::carts::arts::ArtsDialect", "::mlir::arith::ArithDialect", "::mlir::polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::carts::arts::createCreateEpochsPass()";
}

def EdtPtrRematerialization
    : Pass<"edt-ptr-rematerialization", "mlir::ModuleOp"> {
  let summary = "Rematerialize pointer dependencies in EDTs.";
  let description =
      [{This pass rematerializes pointer dependencies in event -
        driven tasks(EDTs) to optimize memory usage and data movement.}];
  let constructor = "mlir::carts::arts::createEdtPtrRematerializationPass()";
}

def EpochOpt : Pass<"arts-epoch-opt", "mlir::ModuleOp"> {
  let summary = "Realize the committed repeated-timestep epoch shape";
  let description = [{
    Repetition amortization - amortize epoch create/wait overhead for a
    repeated epoch loop by hoisting it out of the timestep loop and wrapping
    the EDT bodies with repeat loops. This realizes the committed SDE
    full_timestep repetition plan; it does not re-derive cross-timestep
    legality.
  }];
  let options = [
    Option<"enableAmortization", "amortization", "bool", "false",
           "Enable repetition amortization for repeated epoch loops">
  ];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect"];
  let constructor = "mlir::carts::arts::createEpochOptPass()";
}

def EpochTailContinuation
    : Pass<"arts-epoch-tail-continuation", "mlir::ModuleOp"> {
  let summary = "Outline final post-epoch host tails into continuation EDTs";
  let description = [{
    Authors a real ARTS continuation EDT for timer/checksum/cleanup tails that
    follow a final epoch in main. The continuation carries DB payloads as real
    read-only dependencies and scalar state as EDT params, then owns shutdown
    with arts.shutdown. ARTS-RT consumes this structure mechanically.
  }];
  let dependentDialects = [
    "::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect",
    "::mlir::scf::SCFDialect", "::mlir::arith::ArithDialect",
    "::mlir::memref::MemRefDialect", "::mlir::LLVM::LLVMDialect"
  ];
  let constructor = "mlir::carts::arts::createEpochTailContinuationPass()";
}

def Hoisting : Pass<"arts-hoisting", "mlir::ModuleOp"> {
  let summary = "Hoist loop-invariant ARTS operations";
  let description = [{
    Unified hoisting for ARTS operations:
    - Hoists read-only db_acquire out of worker loops in epochs when operands
      do not depend on the worker induction variable
    - Hoists db_ref out of inner loops in EDTs when source and indices are
      loop-invariant
  }];
  let options = [
    Option<"enableAcquireHoisting", "acquire", "bool", "true",
           "Hoist db_acquire in epochs">,
    Option<"enableRefHoisting", "ref", "bool", "true", "Hoist db_ref in EDTs">
  ];
  let dependentDialects = [
    "::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect", "::mlir::scf::SCFDialect",
    "::mlir::memref::MemRefDialect"
  ];
  let constructor = "mlir::carts::arts::createHoistingPass()";
}

def BlockLoopStripMining : Pass<"block-stripmine", "mlir::func::FuncOp"> {
  let summary = "Strip-mine block-partitioned loops to remove div/rem overhead";
  let description = [{Matches block-partitioned loop indexing patterns and
                      rewrites them into block-local loops that reuse db_ref.
                      The 1-D path handles innermost div/rem loops, while the
                      neighborhood-aware path can also rewrite
                      enclosing stencil loops that carry block-neighbor
                      indexing state.}];
  let dependentDialects = [
    "::mlir::func::FuncDialect", "::mlir::scf::SCFDialect", "::mlir::arith::ArithDialect",
    "::mlir::carts::arts::ArtsDialect"
  ];
  let constructor = "mlir::carts::arts::createBlockLoopStripMiningPass()";
}

def ContractValidation : Pass<"contract-validation", "mlir::ModuleOp"> {
  let summary = "Validate LoweringContractOp integrity across the module";
  let description = [{
    Lightweight verification pass that walks all arts.lowering_contract
    operations and checks structural invariants:
    - Target value exists and is a valid memref
    - supported_block_halo requires a stencil-family dep_pattern
    - min_offsets/max_offsets must have matching rank
    - block_shape rank > 0 if present
    - owner_dims values are non-negative

    Also warns about DbAcquireOps carrying stencil attributes with no
    corresponding LoweringContractOp on the source pointer (orphans).

    Collects per-pattern statistics and optionally fails on errors.
  }];
  let dependentDialects =
      ["::mlir::memref::MemRefDialect", "::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let options = [
    Option<"failOnError", "fail-on-error", "bool", "false",
           "Fail compilation if any contract validation errors are found">,
    Option<"warnOnProofGaps", "warn-on-proof-gaps", "bool", "false",
           "Emit advisory warnings for ownership proof gaps">
  ];
  let constructor = "mlir::carts::arts::createContractValidationPass()";
}

///===----------------------------------------------------------------------===///
/// Verification Passes
///===----------------------------------------------------------------------===///

def VerifyEdtCreated : Pass<"verify-edt-created", "mlir::ModuleOp"> {
  let summary = "Verify EDT operations exist after OpenMP conversion";
  let dependentDialects = ["::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createVerifyEdtCreatedPass()";
}

def VerifyEpochCreated : Pass<"verify-epoch-created", "mlir::ModuleOp"> {
  let summary = "Verify epoch operations exist after epoch creation";
  let dependentDialects = ["::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createVerifyEpochCreatedPass()";
}

def VerifyArtsObjectsOnly : Pass<"verify-arts-objects-only", "mlir::ModuleOp"> {
  let summary = "Verify the CODIR-to-ARTS boundary contains only ARTS objects";
  let description = [{
    Rejects IR that violates the target dialect layering contract: no residual
    SDE or OpenMP operations may remain after CODIR-to-ARTS materialization.
  }];
  let dependentDialects = [
    "::mlir::carts::arts::ArtsDialect",
    "::mlir::carts::sde::CartsSdeDialect"
  ];
  let constructor = "mlir::carts::arts::createVerifyArtsObjectsOnlyPass()";
}

#endif // CARTS_PASSES
