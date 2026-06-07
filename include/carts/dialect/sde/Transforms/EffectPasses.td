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

def DistributionPlanning
    : Pass<"distribution-planning", "mlir::ModuleOp"> {
  let summary = "Derive SDE-owned distribution intent from structured SDE loops";
  let description = [{
    Uses SDE-owned structured classification and generic logical-capacity
    heuristics to wrap eligible `sde.su_iterate` operations in
    `sde.su_distribute`. The current implementation is deliberately
    narrow: it authors blocked advisories for structured independent loops and
    owner-compute advisories for stencil loops with enough work to amortize
    coordination.

    The target boundary is responsible for materializing this advisory into
    downstream distribution metadata.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createDistributionPlanningPass()";
}

def StorageGrainReconciliation
    : Pass<"storage-grain-reconciliation", "mlir::ModuleOp"> {
  let summary = "Reconcile the DB block grain across an array's su_iterate writers";
  let description = [{
    Runs after `distribution-planning` has stamped a per-writer physical plan.
    When one array is written by structurally different `sde.su_iterate`
    scheduling units that committed DIFFERENT `physicalBlockShape`s (e.g. a
    data-parallel init and a pipeline kernel), the ARTS DB materializer cannot
    share one per-block distributed DB and the array coarsens into a
    `host_whole` DB plus a bridge. This pass reconciles those writers to one DB
    grain -- the per-dim GCD of their blocks (<= every realized loop step, so no
    retile is needed) -- by rewriting only `physicalBlockShape`. The per-writer
    `logicalWorkerSlice` (CU grain) is left untouched, keeping DB grain and CU
    grain separate. Fail-closed: single-writer, already-coherent, owner-dim
    mismatched, and matmul/stencil/in-place arrays are byte-identical no-ops.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor =
      "mlir::carts::sde::createStorageGrainReconciliationPass()";
}

def BarrierElimination
    : Pass<"barrier-elimination", "mlir::ModuleOp"> {
  let summary = "Plan SDE barriers and stamp timestep stage boundaries";
  let description = [{
    Analyzes read and write sets of consecutive sde.su_iterate operations
    separated by sde.su_barrier ops. When the write-set of the predecessor
    loop is provably disjoint from the read-set of the successor loop, the
    barrier is marked for elimination. The target boundary consumes that mark
    and omits the corresponding downstream synchronization object. Required
    timestep barriers remain in SDE as explicit stage boundaries and the
    recognized timestep pair is stamped with its repeated/alternating
    repetition structure and `advance_stage` async strategy.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createBarrierEliminationPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_PASSES
