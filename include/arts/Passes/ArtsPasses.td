#ifndef CARTS_PASSES
#define CARTS_PASSES

include "mlir/Pass/PassBase.td"
include "mlir/Rewrite/PassUtil.td"

def ConvertOpenMPToARTS : Pass<"convert-openmp-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP functions to ARTS";
  let dependentDialects = [
    "memref::MemRefDialect",
    "omp::OpenMPDialect",
    "LLVM::LLVMDialect",
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createConvertOpenMPtoARTSPass()";
}

def ConvertARTSToFuncs : Pass<"convert-arts-to-funcs", "mlir::ModuleOp"> {
  let summary = "Convert ARTS functions to Funcs";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createConvertARTSToFuncsPass()";
}
#endif // CARTS_PASSES
