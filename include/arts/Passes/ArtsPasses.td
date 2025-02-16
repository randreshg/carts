#ifndef CARTS_PASSES
#define CARTS_PASSES

include "mlir/Pass/PassBase.td"
include "mlir/Rewrite/PassUtil.td"

def ConvertOpenMPToArts : Pass<"convert-openmp-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP functions to ARTS";
  let dependentDialects = [
    "memref::MemRefDialect",
    "omp::OpenMPDialect",
    "LLVM::LLVMDialect",
    "arts::ArtsDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertOpenMPtoARTSPass()";
}

def ConvertArtsToFuncs : Pass<"convert-arts-to-funcs", "mlir::ModuleOp"> {
  let summary = "Convert ARTS functions to Funcs";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertArtsToFuncsPass()";
}

def CreateArtsEvents : Pass<"create-events", "mlir::ModuleOp"> {
  let summary = "Analyzes the program and creates ARTS events";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateEventsPass()";
}

def DataBlock : Pass<"datablocks", "mlir::ModuleOp"> {
  let summary = "Using the Datablock Analysis, perform changes to the DataBlock operations and optimize them if the user wants to";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createDataBlockPass()";
}
#endif // CARTS_PASSES
