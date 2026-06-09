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
  let constructor = "mlir::carts::arts::createDbModeTighteningPass()";
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
  let constructor = "mlir::carts::arts::createDbOwnerMapRealizationPass()";
}

def DbCommitDistributedDeps
    : Pass<"db-commit-distributed-deps", "mlir::ModuleOp"> {
  let summary = "Commit DB-mode and halo facts on every distributed acquire";
  let description = [{
    Commits the runtime DB mode (ro/ew/rw) on every `arts.db_acquire` whose
    source DB is distributed. For distributed stencil halo acquires, ARTS
    commits explicit `element_offsets`/`element_sizes` windows for ARTS-RT to
    lower mechanically; `halo_slice` remains stencil-reach metadata for
    verification and diagnostics. It only fills committed facts; acquires
    already carrying a mode verdict keep it.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createDbCommitDistributedDepsPass()";
}

def DbConsolidateStencilHalos
    : Pass<"db-consolidate-stencil-halos", "mlir::ModuleOp"> {
  let summary = "Consolidate DB stencil halo windows";
  let description = [{
    Unifies stencil halo bounds from graph analysis, raw acquire attrs, and
    lowering contracts into one contract window, then removes redundant raw
    attrs from the acquire.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createDbConsolidateStencilHalosPass()";
}

def DbShortenLifetimes : Pass<"db-shorten-lifetimes", "mlir::ModuleOp"> {
  let summary = "Remove cleanup-only DB acquire chains";
  let description = [{
    Removes DB acquire chains whose guid and pointer uses feed only cleanup
    plumbing, after checking the reachable DB-use graph.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createDbShortenLifetimesPass()";
}

def DbDeadRootElimination
    : Pass<"db-dead-root-elimination", "mlir::ModuleOp"> {
  let summary = "Remove dead DB root chains";
  let description = [{
    Removes root DB allocation chains whose guid and pointer uses are reduced
    to forwarding and cleanup plumbing only.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createDbDeadRootEliminationPass()";
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

def VerifyArtsCdag : Pass<"verify-arts-cdag", "mlir::ModuleOp"> {
  let summary = "Verify the ARTS boundary before ARTS-RT lowering";
  let description = [{
    Fail-closed verification at the ARTS/ARTS-RT boundary:
    - no EDT mixes incompatible DB dep grains;
    - every distributed `arts.db_acquire` carries a committed runtime DB mode;
    - distributed DB owner-map / placement facts are consistent;
    - no internode EDT depends on a coarse aggregate user DB;
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
  let constructor = "mlir::carts::arts::createCreateDbsPass()";
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

def EdtInlineNoDepTasks
    : Pass<"edt-inline-no-dep-tasks", "mlir::ModuleOp"> {
  let summary = "Inline top-level dep-free EDT bodies";
  let description = [{
    Splices top-level task or sync EDT bodies that have no deps into the
    parent block. It does not reason about barriers, DB frontiers, epochs, or
    memory dependence; those ordering decisions belong to SDE/CODIR structure and
    ARTS epoch realization.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createEdtInlineNoDepTasksPass()";
}

def EdtDeadDepElimination
    : Pass<"edt-dead-dep-elimination", "mlir::ModuleOp"> {
  let summary = "Remove cleanup-only EDT dep slots";
  let description = [{
    Removes dep slots whose EDT block arguments are unused or only feed
    cleanup/control-token plumbing. It does not reason about placement, DB grain,
    barriers, or epochs.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createEdtDeadDepEliminationPass()";
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

def EpochAmortizeRepeatedLoop
    : Pass<"arts-epoch-amortize-repeated-loop", "mlir::ModuleOp"> {
  let summary = "Amortize committed repeated-timestep epoch loops";
  let description = [{
    Hoists a repeated epoch out of a timestep loop and wraps its EDT bodies in
    repeat loops. This is a narrow ARTS realization of the committed SDE
    full_timestep repetition plan; it does not re-derive cross-timestep
    legality or perform general epoch optimization.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect"];
  let constructor =
      "mlir::carts::arts::createEpochAmortizeRepeatedLoopPass()";
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

///===----------------------------------------------------------------------===///
/// Verification Passes
///===----------------------------------------------------------------------===///

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
