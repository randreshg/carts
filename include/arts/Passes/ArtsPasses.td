#ifndef CARTS_PASSES
#define CARTS_PASSES

include "mlir/Pass/PassBase.td"
include "mlir/Rewrite/PassUtil.td"

def ArtsInliner : Pass<"arts-inliner", "mlir::ModuleOp"> {
  let summary = "Inline functions in the program";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createArtsInlinerPass()";
}


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

def Db : Pass<"db", "mlir::ModuleOp"> {
  let summary = "Analyze and optimize the datablocks in the program";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createDbPass()";
}

def CreateDbs : Pass<"create-dbs", "mlir::ModuleOp"> {
  let summary = "Identify Dbs in the program";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateDbsPass()";
  let options = [
    Option<"identifyDbs", "identify-dbs", "bool", /*default=*/"true",
           "Automatically identify the datablocks.">,
  ];
}

def NormalizeDbs : Pass<"normalize-dbs", "mlir::ModuleOp"> {
  let summary = "Normalize ARTS DataBlock handles for runtime compatibility";
  let description = [{
    This pass converts ARTS DataBlocks from typed memrefs to opaque pointer types
    suitable for the ARTS runtime. The preprocessing logic differs based on the
    original allocation type (stack vs dynamic).
  }];
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createNormalizeDbsPass()";
}

def Edt : Pass<"edt", "mlir::ModuleOp"> {
  let summary = "Optimize EDTs";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEdtPass()";
}

def EdtConcurrency : Pass<"edt-concurrency", "mlir::ModuleOp"> {
  let summary = "Build EDT concurrency graph and apply concurrency-driven cleanups";
  let dependentDialects = [
    "func::FuncDialect",
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createEdtConcurrencyPass()";
}

def CreateEpochs : Pass<"create-epochs", "mlir::ModuleOp"> {
  let summary = "Analyzes the program and creates ARTS epochs";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCreateEpochsPass()";
}

def ConvertArtsToLLVM : Pass<"convert-arts-to-llvm", "mlir::ModuleOp"> {
  let summary = "Convert ARTS Dialect to LLVM Dialect";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createConvertArtsToLLVMPass()";
  let options = [
    Option<"debug", "debug", "bool", /*default=*/"false",
           "Enable debug mode.">,
  ];
}

def EdtInvariantCodeMotion : Pass<"edt-invariant-code-motion", "mlir::ModuleOp"> {
  let summary = "Hoist invariant operations from arts edt regions";
  let dependentDialects = [
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createEdtInvariantCodeMotionPass()";
}

def EdtPtrRematerialization : Pass<"edt-ptr-rematerialization", "mlir::ModuleOp"> {
  let summary = "Rematerialize pointer dependencies in EDTs.";
  let description = [{
    This pass rematerializes pointer dependencies in event-driven tasks (EDTs) 
    to optimize memory usage and data movement.
  }];
  let constructor = "mlir::arts::createEdtPtrRematerializationPass()";
}

def EpochLowering : Pass<"epoch-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS epochs and propagate epoch GUIDs to EDTs";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEpochLoweringPass()";
}

def EdtLowering : Pass<"edt-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS EDTs to LLVM";
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createEdtLoweringPass()";
}

#endif // CARTS_PASSES
