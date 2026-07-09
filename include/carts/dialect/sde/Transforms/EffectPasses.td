///==========================================================================///
/// Effect passes — scheduling decisions (schedule, chunk, reduction,
///                  distribution, barrier elimination)
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_EFFECT_PASSES

include "mlir/Pass/PassBase.td"

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

def MovementTagging
    : Pass<"sde-movement-tagging", "mlir::ModuleOp"> {
  let summary = "Tag distributable SDE loops with sde.su_distribute";
  let description = [{
    Selects the SDE distribution kind (blocked vs owner_compute) for eligible
    structured `sde.su_iterate` operations from SDE-owned classification and
    generic logical-capacity heuristics, and wraps the chosen loops in
    `sde.su_distribute`. Fail-closed gating for unimplemented in-place stencil
    wavefronts is owned by sde-distribution-fail-closed, which runs earlier; this
    pass only tags work that is legally distributable. It names no collectives,
    DBs, EDTs, owner maps, routes, or runtime policy.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createMovementTaggingPass()";
}

def OwnerDimSelect
    : Pass<"sde-owner-dim-select", "mlir::ModuleOp"> {
  let summary = "Select SDE owner dims and commit per-SU physical layout";
  let description = [{
    Per-SU owner-dimension selection and physical layout commit for the SDE
    distribution chain. Realizes the in-place wavefront/skew transform where
    legal, commits loop-step replicated layout, and runs the ordered per-pattern
    physical committers (budget-reconciled grain, co-iterated writer inherit,
    stencil owner_tile, matmul, uniform elementwise, reduction task shape, and
    in-place serial slice). It commits only layouts the realized SDE loop steps
    already satisfy; unsupported shapes stay unmodified for later SDE verifiers.
    It selects no movement families, distribution wrappers, DB grain, routes, or
    runtime policy.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createOwnerDimSelectPass()";
}

def BlockGrainPlan
    : Pass<"sde-block-grain-plan", "mlir::ModuleOp"> {
  let summary = "Unify SDE block grain for budgeted same-owner layouts";
  let description = [{
    Owns the SDE block-grain decisions for data-parallel distributed arrays.
    The budget-reconciled physical grain committer is consumed by the per-SU
    owner-dim commit pass; this pass itself runs the A1 same-owner grain
    unification, committing one node-agnostic block grain (the per-dim GCD /
    budget grain) on every block_parallel fact of an array so producer and
    consumer SUs share a home and no same-owner re-tile is left for
    RedistributionEdges to reject. It does not choose owner dimensions, movement
    families, distribution wrappers, DB grain, routes, or runtime policy.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::carts::sde::createBlockGrainPlanPass()";
}

def SdeExposeLegalBlocks
    : Pass<"sde-expose-legal-blocks", "mlir::ModuleOp"> {
  let summary = "Expose legal SDE owner blocks from memory legality facts";
  let description = [{
    Consumes committed SDE array layout facts plus SdeMemoryLegality target
    capacity facts and materializes a node-agnostic budgetBlockShape. This is a
    narrow Phase B slice of SdeExposeLegalBlocks: it never looks at worker/node
    count, never chooses movement families, and only refines owner-dim block
    extents when the owning SDE layout fact and static root type are available.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createSdeExposeLegalBlocksPass()";
}

def SdeMovementSynthesis
    : Pass<"sde-movement-synthesis", "mlir::ModuleOp"> {
  let summary = "Synthesize first-class SDE movement ops from typed layout facts";
  let description = [{
    Consumes SDE substrate analyses plus committed array layout/root facts and
    materializes concrete SU movement structure. This vertical slice handles
    grounded owner-preserving stencil halos. Unsupported all-to-all and
    reduce-scatter candidates fail closed unless a later slice proves and
    realizes the backing reduction/contraction structure.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor = "mlir::carts::sde::createSdeMovementSynthesisPass()";
}

def DistributionFailClosed
    : Pass<"sde-distribution-fail-closed", "mlir::ModuleOp"> {
  let summary = "Fail closed for unimplemented in-place stencil wavefront cases";
  let description = [{
    A7 fail-closed gate for the SDE distribution chain. Refuses multi-worker
    lowering of in-place self-read neighborhood stencils (Gauss-Seidel family)
    whose loop-carried neighbor offsets would require an SDE wavefront/skew
    transform that is not implemented. Distributing such a loop in place would
    violate Gauss-Seidel ordering, and preserving the order is serial, so the
    pass emits a citing diagnostic and signals failure rather than silently
    dropping legal parallelism or miscompiling.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect"];
  let constructor = "mlir::carts::sde::createDistributionFailClosedPass()";
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
    rank-expanded MUs and in-CU memory ops are in place, each surviving
    `sde.su_barrier` separates phases whose query-derived block-grid windows
    (`[blockLo, blockHi)` per owner dim) reveal whether the barrier orders a
    real dependency. Reading the shared `classifyBarrierSync` analysis, this
    pass erases every barrier proven REDUNDANT.

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
