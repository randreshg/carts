///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the SDE -> CODIR -> ARTS conversion boundaries.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_PASSES
#define CARTS_DIALECT_CODIR_CONVERSION_PASSES

include "mlir/Pass/PassBase.td"

def ConvertSdeToCodir : Pass<"convert-sde-to-codir", "mlir::ModuleOp"> {
  let summary = "Materialize SDE planning intent as CODIR codelet IR";
  let description = [{
    Boundary pass for the SDE-to-CODIR split. It consumes SDE codelet plans and
    materializes isolated CODIR codelets with explicit dependencies, scalar
    parameters, access modes, and token-local memref views.
  }];
  let dependentDialects = [
    "::mlir::carts::sde::CartsSdeDialect",
    "::mlir::carts::codir::CartsCodirDialect",
    "::mlir::memref::MemRefDialect",
    "::mlir::arith::ArithDialect"
  ];
  let constructor = "mlir::carts::codir::createConvertSdeToCodirPass()";
}

def ConvertCodirToArts : Pass<"convert-codir-to-arts", "mlir::ModuleOp"> {
  let summary = "Materialize CODIR codelets as abstract ARTS objects";
  let description = [{
    Boundary pass for the CODIR-to-ARTS split. It lowers CODIR codelet deps,
    params, and bodies to abstract ARTS DB acquire and EDT objects.
  }];
  let dependentDialects = [
    "::mlir::carts::codir::CartsCodirDialect",
    "::mlir::carts::arts::ArtsDialect",
    "::mlir::memref::MemRefDialect",
    "::mlir::arith::ArithDialect"
  ];
  let constructor = "mlir::carts::codir::createConvertCodirToArtsPass()";
}

def MaterializeSdeBoundaryToArts
    : Pass<"materialize-sde-boundary-to-arts", "mlir::ModuleOp"> {
  let summary = "Materialize residual SDE storage/control boundary artifacts";
  let description = [{
    Explicit boundary pass for SDE artifacts that intentionally survive
    SDE-to-CODIR codelet isolation, such as SDE memory-unit allocations,
    memory-unit data handles, resource queries, and consumed completion tokens.
    This pass is kept separate from CODIR-to-ARTS so CODIR-to-ARTS consumes
    CODIR only and fails closed on any residual SDE operation.
  }];
  let dependentDialects = [
    "::mlir::carts::sde::CartsSdeDialect",
    "::mlir::carts::arts::ArtsDialect",
    "::mlir::memref::MemRefDialect",
    "::mlir::arith::ArithDialect"
  ];
  let constructor = "mlir::carts::codir::createMaterializeSdeBoundaryToArtsPass()";
}

#endif // CARTS_DIALECT_CODIR_CONVERSION_PASSES
