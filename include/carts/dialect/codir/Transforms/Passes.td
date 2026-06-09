///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the CODIR (Codelet IR) dialect.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_TRANSFORMS_PASSES
#define CARTS_DIALECT_CODIR_TRANSFORMS_PASSES

include "mlir/Pass/PassBase.td"

def VerifyCodir : Pass<"verify-codir", "mlir::ModuleOp"> {
  let summary = "Verify CODIR codelet boundary invariants";
  let description = [{
    Verification barrier for CODIR. It keeps memory dependencies and scalar
    parameters in separate operand groups, requires body block arguments to
    mirror deps then params, and rejects non-scalar yielded values until CODIR
    grows an explicit memory-result ABI. The active MLIR verifier checks
    operation traits such as codelet isolation before and after the pass.
  }];
  let dependentDialects = ["::mlir::carts::codir::CartsCodirDialect"];
  let constructor = "mlir::carts::codir::createVerifyCodirPass()";
}

def CodirCodeletDCE : Pass<"codir-codelet-dce", "mlir::ModuleOp"> {
  let summary = "Remove dead operations inside isolated CODIR codelet bodies";
  let description = [{
    CODIR-local dead-code elimination. It removes unused side-effect-free
    operations inside isolated codir.codelet bodies after SDE-to-CODIR
    token-local rewrites and before CODIR verification / ARTS materialization.
  }];
  let dependentDialects = [
    "::mlir::carts::codir::CartsCodirDialect",
    "::mlir::arith::ArithDialect",
    "::mlir::memref::MemRefDialect"
  ];
  let constructor = "mlir::carts::codir::createCodirCodeletDCEPass()";
}

def ReductionDepMapping : Pass<"reduction-dep-mapping", "mlir::ModuleOp"> {
  let summary = "Map CODIR partial-reduction deps to result dimensions";
  let description = [{
    CODIR-owned reduction dependency mapping. It consumes SDE-authored
    partial-reduction structure, records how each dependency's physical
    dimensions map back to the reduction result dimensions, and records when
    owner-slice parallelism underfills the runtime worker count and needs an
    additional reduction-dimension split plus combine tree.
  }];
  let dependentDialects = [
    "::mlir::carts::codir::CartsCodirDialect",
    "::mlir::arith::ArithDialect",
    "::mlir::memref::MemRefDialect"
  ];
  let constructor = "mlir::carts::codir::createReductionDepMappingPass()";
}

def ReductionAtomicMaterialization
    : Pass<"reduction-atomic-materialization", "mlir::ModuleOp"> {
  let summary = "Materialize explicit atomic reductions in CODIR codelets";
  let description = [{
    Rewrites a CODIR codelet carrying an explicit
    `reduction_strategy<atomic>` from shared load/add/store form into
    `codir.atomic_add`. It does not infer reduction strategy; SDE must already
    have selected atomic execution and SDE-to-CODIR must have carried that fact.
  }];
  let dependentDialects = [
    "::mlir::carts::codir::CartsCodirDialect",
    "::mlir::arith::ArithDialect",
    "::mlir::memref::MemRefDialect"
  ];
  let constructor =
      "mlir::carts::codir::createReductionAtomicMaterializationPass()";
}

def DepStorageAssignment : Pass<"dep-storage-assignment", "mlir::ModuleOp"> {
  let summary = "Assign CODIR dependency storage views before ARTS lowering";
  let description = [{
    CODIR-owned dependency storage assignment. It turns high-level dependency
    storage requests into explicit per-dependency facts that ARTS
    materialization can consume directly: owner-local compute blocks, phase
    redistribution bridges, replicated reads, or host-whole local fallback.
    This keeps bridge selection out of the CODIR-to-ARTS conversion.
  }];
  let dependentDialects = [
    "::mlir::carts::codir::CartsCodirDialect",
    "::mlir::arith::ArithDialect",
    "::mlir::memref::MemRefDialect",
    "::mlir::scf::SCFDialect"
  ];
  let constructor = "mlir::carts::codir::createDepStorageAssignmentPass()";
}

#endif // CARTS_DIALECT_CODIR_TRANSFORMS_PASSES
