///==========================================================================///
/// Effect passes — scheduling decisions (schedule, chunk, reduction,
///                  distribution, barrier elimination)
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_PASSES

include "mlir/Pass/PassBase.td"

def ScheduleRefinement
    : Pass<"schedule-refinement", "mlir::ModuleOp"> {
  let summary = "Refine abstract SDE schedule hints into concrete schedules";
  let description = [{
    Uses the SDE-owned cost model to refine eligible `sde.su_iterate`
    schedule hints before later SDE optimizations run. The current
    implementation is deliberately narrow: it only handles one-dimensional
    loops with constant trip counts, only rewrites `auto` / `runtime`
    schedules, and chooses the lowest-overhead concrete policy among
    `static`, `dynamic`, and `guided`.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createScheduleRefinementPass()";
}

def ChunkOpt : Pass<"chunk-opt", "mlir::ModuleOp"> {
  let summary = "Fill missing SDE chunk sizes from logical capacity";
  let description = [{
    Uses SDE-owned logical capacity to synthesize chunk sizes for eligible
    sde.su_iterate operations when the source program left them unspecified.
    Symbolic trip-count cases use `sde.resource_query <logical_workers>` so
    the boundary layer, not SDE, binds the expression to a concrete target
    mechanism. The current implementation handles one-dimensional
    dynamic/guided loops and preserves explicit source chunk sizes.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::carts::sde::createChunkOptPass()";
}

def ReductionStrategy : Pass<"reduction-strategy", "mlir::ModuleOp"> {
  let summary = "Select an SDE reduction strategy from the active cost model";
  let description = [{
    Uses the SDE-owned cost model to annotate eligible reduction-bearing
    sde.su_iterate operations with a reduction strategy recommendation.
    The current implementation is deliberately narrow: it only handles
    single-reduction loops with preserved reduction kinds, chooses between
    `atomic` and `tree`, and leaves lowering behavior unchanged.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createReductionStrategyPass()";
}

def SdeAtomicReductionRealization
    : Pass<"sde-atomic-reduction-realization", "mlir::ModuleOp"> {
  let summary = "Realize SDE atomic reductions as explicit CU atomics";
  let description = [{
    Consumes `reduction_strategy(<atomic>)` on eligible `sde.su_iterate`
    operations by rewriting the leaf-CU load/add/store update on the reduction
    accumulator into an explicit `sde.cu_atomic`. After the source effect is
    represented directly, the pass removes the consumed reduction operands and
    attrs so the SDE-to-ARTS boundary only sees executable CU work plus
    scheduling facts.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor =
      "mlir::carts::sde::createSdeAtomicReductionRealizationPass()";
}

def DistributionPlanning
    : Pass<"distribution-planning", "mlir::ModuleOp"> {
  let summary = "Commit SDE distribution facts on structured SDE loops";
  let description = [{
    Uses SDE-owned structured classification and generic logical-capacity
    heuristics to wrap eligible `sde.su_iterate` operations in
    `sde.su_distribute`, commits physical owner/block facts that are already
    realized by SDE loop shape, and fails closed for unimplemented wavefront
    stencil cases.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createDistributionPlanningPass()";
}

def BarrierElimination
    : Pass<"barrier-elimination", "mlir::ModuleOp"> {
  let summary = "Remove redundant SDE barriers and commit timestep stages";
  let description = [{
    Analyzes read and write sets of consecutive sde.su_iterate operations
    separated by sde.su_barrier ops. When the write-set of the predecessor
    loop is provably disjoint from the read/write set of the successor loop,
    the pass erases the barrier in SDE. Required timestep barriers remain in
    SDE as explicit stage boundaries and the recognized timestep pair receives
    its repeated/alternating repetition structure and `advance_stage` async
    strategy.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createBarrierEliminationPass()";
}

def MuAccessWindowSyncOpt
    : Pass<"sde-mu-access-window-sync-opt", "mlir::ModuleOp"> {
  let summary = "Remove SU barriers proven redundant by MU access windows";
  let description = [{
    The transform counterpart of `verify-sde-mu-access-window-sync`. After
    `raise-to-mu-access-window` has committed per-CU `sde.mu_access_window` facts,
    each surviving `sde.su_barrier` separates a before phase and an after phase
    whose block-grid windows (`[blockLo, blockHi)` per owner dim) reveal whether
    the barrier orders a real dependency. Reading the shared `classifyBarrierSync`
    analysis, this pass erases every barrier proven REDUNDANT — the ordered CUs
    touch disjoint blocks or only read shared blocks, and both phases are fully
    described by windows, so removing the global ordering point lets SU
    structure expose independent CU bodies for later ARTS dataflow.

    It is conservative and fails closed by construction: it removes a barrier
    only on a REDUNDANT verdict. JUSTIFIED barriers (a real aligned RAW/WAW/WAR
    ordering) are preserved as the irreducible global ordering ARTS refines into
    explicit dependency edges from the same windows; MISALIGNED barriers (a
    consumer reaching blocks the producer never wrote) are preserved for the
    later SDE redistribution transform that owns the movement; and out-of-scope,
    malformed, rank-mismatched, or otherwise unprovable barriers are left
    untouched for `verify-sde-mu-access-window-sync` to gate. The pass introduces
    no token, slice, dependency-graph, distribution, or stage-boundary metadata —
    it only deletes a proven-unnecessary ordering point.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createMuAccessWindowSyncOptPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_PASSES
