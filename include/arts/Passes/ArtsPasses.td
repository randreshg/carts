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
  let summary = "Using the Datablock Analysis, perform changes to the DataBlock"
                "operations and optimize them if the user wants to";
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
           "Analyzes the module and tries to automatically identify the "
           "datablocks.">,
  ];
}

def ConvertDbToOpaquePtr : Pass<"convert-db-to-opaque-ptr", "mlir::ModuleOp"> {
  let summary = "Preprocess ARTS DataBlocks for runtime compatibility";
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
  let constructor = "mlir::arts::createConvertDbToOpaquePtrPass()";
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

def EdtPointerRematerialization : Pass<"edt-pointer-rematerialization", "mlir::ModuleOp"> {
  let summary = "Rematerialize pointer values in arts edt regions";
  let dependentDialects = [
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createEdtPointerRematerializationPass()";
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
