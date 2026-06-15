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

def DbDistributedOwnershipRealization
    : Pass<"db-distributed-ownership-realization", "mlir::ModuleOp"> {
  let summary = "Realize ARTS distributed ownership on distribution-eligible DB "
                "allocations";
  let description = [{
    Selects host-level DB allocations whose committed SDE owner/block facts can
    answer ARTS owner-route queries and marks them `distributed`. It does not
    choose a distribution family, write route attrs, or change block shape.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect", "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::arts::createDbDistributedOwnershipRealizationPass()";
}

def DbDistributedRuntimeInit
    : Pass<"db-distributed-runtime-init", "mlir::ModuleOp"> {
  let summary = "Build distributed DB runtime init callbacks from ARTS routes";
  let description = [{
    Consumes ARTS distributed DB ownership facts after DB handle lowering and
    emits explicit init callbacks containing ARTS-RT GUID reservation and
    owner-local DB creation ops. ARTS-RT-to-LLVM only lowers these runtime ops.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect",
       "::mlir::carts::arts_rt::ArtsRtDialect", "::mlir::memref::MemRefDialect",
       "::mlir::LLVM::LLVMDialect", "::mlir::arith::ArithDialect",
       "::mlir::scf::SCFDialect", "::mlir::polygeist::PolygeistDialect"];
  let constructor = "mlir::carts::arts::createDbDistributedRuntimeInitPass()";
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
    lowering facts into one explicit window, then removes redundant raw attrs
    from the acquire.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createDbConsolidateStencilHalosPass()";
}

def DbStorageBridgeCopyPlacement
    : Pass<"db-storage-bridge-copy-placement", "mlir::ModuleOp"> {
  let summary = "Place single-node storage-bridge copy-outs at host reads";
  let description = [{
    Moves already-realized single-node block-to-host storage-bridge copy EDTs
    from producer boundaries to the first coarse host observation when no
    coarse host access is crossed. Later block writes make earlier copy-outs
    stale, so they are removed while preserving realized DB grain.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::memref::MemRefDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createDbStorageBridgeCopyPlacementPass()";
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

def PartialReductionSplit
    : Pass<"partial-reduction-split", "mlir::ModuleOp"> {
  let summary = "Split partial-reduction facts into ARTS objects";
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
      "mlir::carts::arts::createPartialReductionSplitPass()";
}

def BlockContractionSplit
    : Pass<"block-contraction-split", "mlir::ModuleOp"> {
  let summary = "Split owner-block contractions into per-tile producers";
  let description = [{
    Rewrites an owner-block add-contraction consumer marked with reduction facts
    that reads one contraction operand from a full-grid `replicatedRead` view
    into per-(owner-block, replica-tile) producer EDTs reading block-native from
    the `perBlockReplicated` all-gather replica, plus a per-block summing settle
    that reduces the partials into the settled block with `arith.addf`. Each
    producer and the settle acquire every DB outside the EDT as a block-arg
    dependency, so no EDT body GEPs an outer DB alloc. The trigger is the
    ARTS fact shape (`partialReductionDims`, `partialReductionDepResultDimMaps`,
    `reduction_strategy = local_accumulate`, block dependencies, a single
    replicated-read contraction operand, and a matching per-block all-gather
    replica), not benchmark names or source paths.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect", "::mlir::memref::MemRefDialect",
       "::mlir::carts::arts::ArtsDialect"];
  let constructor =
      "mlir::carts::arts::createBlockContractionSplitPass()";
}

def EdtSplitForMixedDeps
    : Pass<"edt-split-for-mixed-deps", "mlir::ModuleOp"> {
  let summary = "Reject/localize EDTs mixing local-only and distributed-writer deps";
  let description = [{
    Reconciles ARTS EDT placement with distributed DB ownership for the
    mixed-dependency case. An EDT that mixes a local-only distributed
    dependency with a distributed writer is rejected (it would require an
    explicit ordering dependency before the codelet can be split); internode
    EDTs that only carry rejected local-only distributed deps are localized to
    intranode execution.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::arith::ArithDialect",
       "::mlir::memref::MemRefDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createEdtSplitForMixedDepsPass()";
}

def WriterOwnerRoute : Pass<"writer-owner-route", "mlir::ModuleOp"> {
  let summary = "Route distributed-writer EDTs to their DB owner";
  let description = [{
    Derives the consistent owner route for an EDT that writes distributed DBs
    from the committed DB block grid and promotes/routes the codelet to that
    owner. EDTs whose owner route cannot be derived, that span multiple owners,
    or that disagree across writers are rejected (SDE-to-ARTS must split or
    sequence them before distributed launch).
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::arith::ArithDialect",
       "::mlir::memref::MemRefDialect", "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createWriterOwnerRoutePass()";
}

def SdeStorageToArtsDb : Pass<"sde-storage-to-arts-db", "mlir::ModuleOp"> {
  let summary = "Realize SDE MU storage as ARTS DB-backed memrefs";
  let description = [{
    Consumes committed SDE MU allocation/data facts and creates the corresponding
    ARTS DB-backed memrefs. It also lowers committed SDE access windows into
    short-lived ARTS DB access-window operations so later passes can create
    acquires from explicit windows. It fails closed when SDE
    movement facts are not representable as direct ARTS storage facts.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect", "::mlir::memref::MemRefDialect",
       "::mlir::carts::sde::CartsSdeDialect",
       "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createSdeStorageToArtsDbPass()";
}

def SdeAccessesToArtsDeps
    : Pass<"sde-accesses-to-arts-deps", "mlir::ModuleOp"> {
  let summary = "Realize SDE scheduling/access facts as ARTS dependencies";
  let description = [{
    Consumes SDE scheduling facts plus ARTS DB access-window operations and
    creates ARTS `db_acquire` and `edt` objects. The pass preserves committed
    SDE owner/block/window facts and rejects missing dependency structure.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect", "::mlir::memref::MemRefDialect",
       "::mlir::carts::sde::CartsSdeDialect",
       "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createSdeAccessesToArtsDepsPass()";
}

def FinalizeSdeToArts : Pass<"finalize-sde-to-arts", "mlir::ModuleOp"> {
  let summary = "Finalize direct SDE-to-ARTS control cleanup";
  let description = [{
    Lowers remaining SDE resource/control wrappers after DB and dependency
    realization, then rejects any residual SDE or boundary access-window
    operations. It is the final pass in the direct SDE-to-ARTS boundary.
  }];
  let dependentDialects =
      ["::mlir::func::FuncDialect", "::mlir::scf::SCFDialect",
       "::mlir::arith::ArithDialect", "::mlir::memref::MemRefDialect",
       "::mlir::carts::sde::CartsSdeDialect",
       "::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createFinalizeSdeToArtsPass()";
}

def VerifyArtsCdag : Pass<"verify-arts-cdag", "mlir::ModuleOp"> {
  let summary = "Verify the ARTS boundary before ARTS-RT lowering";
  let description = [{
    Fail-closed verification at the ARTS/ARTS-RT boundary:
    - no EDT mixes incompatible DB dep grains;
    - every distributed `arts.db_acquire` carries a committed runtime DB mode;
    - distributed DB owner routes are derivable and placement is consistent;
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

def RealizeEdtDistribution
    : Pass<"realize-edt-distribution", "mlir::ModuleOp"> {
  let summary = "Realize EDT distribution facts";
  let description = [{
    Reads the committed dep pattern and access-window facts forwarded onto each
    `arts.edt` and writes the distribution family, version, and block-halo
    capability marker. It consumes already-committed facts and does not classify
    movement or select a distribution family from source patterns.
  }];
  let dependentDialects = ["::mlir::carts::arts::ArtsDialect"];
  let constructor = "mlir::carts::arts::createRealizeEdtDistributionPass()";
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
    memory dependence; those ordering decisions belong to SDE structure and
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
    full_timestep repetition facts; it does not re-derive cross-timestep
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
  let summary = "Verify the SDE-to-ARTS boundary contains only ARTS objects";
  let description = [{
    Rejects IR that violates the target dialect layering invariant: no residual
    SDE, ARTS, or OpenMP operations may remain after SDE-to-ARTS conversion.
  }];
  let dependentDialects = [
    "::mlir::carts::arts::ArtsDialect",
    "::mlir::carts::sde::CartsSdeDialect"
  ];
  let constructor = "mlir::carts::arts::createVerifyArtsObjectsOnlyPass()";
}

#endif // CARTS_PASSES
