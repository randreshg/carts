#ifndef CARTS_PASSES
#define CARTS_PASSES

include "mlir/Pass/PassBase.td"
include "mlir/Rewrite/PassUtil.td"


def ConvertOpenMPToARTS : Pass<"convert-openmp-to-ARTS", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP functions to ARTS";
  let dependentDialects = [
    "memref::MemRefDialect",
    "omp::OpenMPDialect",
    "LLVM::LLVMDialect",
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createConvertOpenMPtoARTSPass()";
}

#endif // CARTS_PASSES
