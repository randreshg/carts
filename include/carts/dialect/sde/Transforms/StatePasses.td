///==========================================================================///
/// State passes — memref-state cleanup before SDE planning.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES

include "mlir/Pass/PassBase.td"

def PromoteTargetAttrs
    : Pass<"sde-promote-target-attrs", "mlir::ModuleOp"> {
  let summary = "Promote polygeist target-cpu/features to CARTS target attrs";
  let description = [{
    cgeist stamps `polygeist.target-cpu` and `polygeist.target-features` on
    the top-level module. Downstream conversion passes (notably the LLVM
    lowerings inside `llvm-ir-emission`) drop foreign module attrs, so any
    CARTS pass that reads them in the LLVM-facing stages observes a default
    x86-64 target and emits SSE-width vector hints on AVX/AVX-512 hosts.

    This pass copies the polygeist attrs into target-neutral
    `carts.target-cpu` / `carts.target-features` on the same module before
    any other CARTS pass runs. The CARTS attrs become the single source of
    truth consumers must read. The pass is a no-op when the polygeist attrs
    are absent and idempotent when the CARTS attrs are already present.
  }];
}

def ScalarForwarding : Pass<"scalar-forwarding", "mlir::ModuleOp"> {
  let summary = "Forward constant-initialized scalar memrefs across region boundaries";
  let description = [{
    Detects rank-0 memref.alloca ops initialized with a constant value and
    replaces loads inside nested regions (omp.parallel, cu_region, etc.) with
    the constant when all stores inside that region write the same value.

    This eliminates Polygeist's continuation-boolean pattern where a memref<i1>
    is used as a structured return guard, turning always-true scf.if conditions
    into foldable constants.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect"];
  let constructor = "mlir::carts::sde::createScalarForwardingPass()";
}

def SdeDeadStateCleanupPass
    : Pass<"sde-dead-state-cleanup", "mlir::ModuleOp"> {
  let summary = "Remove dead frontend helper IR before SDE planning";
  let description = [{
    Removes dead dialect-neutral helper IR exposed by SDE input
    normalization: unused memref loads, dead stack stores/allocas, no-op
    load/store self writes, unused pure leaf operations, and private symbols.

    This pass intentionally does not inspect target storage, task, placement,
    or lowering objects.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::func::FuncDialect"];
}

def MemoryUnitMaterialization
    : Pass<"sde-memory-unit-materialization", "mlir::ModuleOp"> {
  let summary = "Materialize SDE shared memrefs as memory units";
  let description = [{
    Rewrites shared memref allocations used by SDE scheduling units into
    `sde.mu_alloc` storage before the dialect boundary. This pass is
    intentionally SDE-local: it creates target-neutral MU storage declarations
    and leaves conversion to concrete target objects to boundary lowering.

    Physical layout and owner-slice facts stay on the scheduling unit and are
    consumed at boundary conversion to form acquire windows for supported
    elementwise/matmul owner slices. Stencil, reduction, halo, and other
    complex blocked plans remain on the raw bridge until their SDE-local view
    rewrite exists. The pass does not rediscover target-object layouts or
    perform concrete index rewriting.
  }];
  let dependentDialects = ["::mlir::carts::sde::CartsSdeDialect",
                           "::mlir::memref::MemRefDialect"];
  let constructor =
      "mlir::carts::sde::createMemoryUnitMaterializationPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_STATE_PASSES
