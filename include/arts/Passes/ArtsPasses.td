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

def Datablock : Pass<"datablock", "mlir::ModuleOp"> {
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
  let constructor = "mlir::arts::createDatablockPass()";
}

def CreateDatablocks : Pass<"create-datablocks", "mlir::ModuleOp"> {
  let summary = "Identify Datablocks in the program";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateDatablocksPass()";
}

def Edt : Pass<"edt", "mlir::ModuleOp"> {
  let summary = "Using the EDT Analysis, perform changes to the EDT operations and optimize them if the user wants to";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEdtPass()";
}

def CreateEpochs : Pass<"create-epochs", "mlir::ModuleOp"> {
  let summary = "Analyzes the program and creates ARTS epochs";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateEpochsPass()";
}

def CreateEvents : Pass<"create-events", "mlir::ModuleOp"> {
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

def ConvertArtsToLLVM : Pass<"convert-arts-to-llvm", "mlir::ModuleOp"> {
  let summary = "Convert ARTS Dialect to LLVM Dialect";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertArtsToLLVMPass()";
}
#endif // CARTS_PASSES
