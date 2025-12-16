///==========================================================================///
/// File: ArtsPasses.td
///
/// This file defines the ArtsPasses class which is used to register the passes
/// for the ARTS dialect.
///==========================================================================///
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

def CollectMetadata : Pass<"collect-metadata", "mlir::ModuleOp"> {
  let summary = "Collect metadata for dual-compilation strategy";
  let description = [{
    This pass analyzes program structures (loops, array accesses, both affine
    and non-affine) and attaches comprehensive metadata for use in dual-compilation.
    It runs on sequential (non-OpenMP) code where analysis is cleaner, then the
    metadata is exported and injected into the parallel compilation.

    Metadata collected from both affine and non-affine operations:
    - Array dimensions inferred from access patterns (e.g., i*128+j → [128,128])
    - Loop bounds (static and dynamic), trip counts, and parallelism opportunities
    - Memory access patterns and regions (affine and non-affine loads/stores)
    - Data dependencies and access frequency statistics
    - Read/write ratios and lifetime information

    This preserves high-level semantic information through OpenMP transformations.
  }];
  let dependentDialects = [
    "memref::MemRefDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "affine::AffineDialect"
  ];
  let options = [
    Option<"exportMetadata", "export-metadata", "bool", "false",
           "Export metadata to JSON file after collection">,
    Option<"metadataFile", "metadata-file", "std::string", "\".carts-metadata.json\"",
           "Path to metadata JSON file for export">
  ];
  let constructor = "mlir::arts::createCollectMetadataPass()";
}

def CanonicalizeMemrefs : Pass<"canonicalize-memrefs", "mlir::ModuleOp"> {
  let summary = "Canonicalize memref allocations and accesses";
  let description = [{
    This pass canonicalizes memref allocations and accesses for downstream passes.
    It converts nested array-of-pointer allocations to multi-dimensional memrefs and
    rewrites pointer-based accesses to direct memref indexed accesses.
  }];
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createCanonicalizeMemrefsPass()";
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
  let constructor = "mlir::arts::createConvertOpenMPtoArtsPass()";
}

def DeadCodeElimination : Pass<"arts-dce", "mlir::ModuleOp"> {
  let summary = "Remove dead loads, stores, allocas, and symbols";
  let description = [{
    Performs dead code elimination for memref operations:
    - Removes loads whose results are unused
    - Removes stores to allocas that are never loaded
    - Removes allocas with no remaining uses
    - Runs SymbolDCE for dead functions and globals

    This is particularly useful after openmp-to-arts conversion where OMP
    dependency proxy allocas become unused.
  }];
  let dependentDialects = [
    "memref::MemRefDialect"
  ];
  let constructor = "mlir::arts::createDeadCodeEliminationPass()";
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
  let constructor = "mlir::arts::createDbPass(nullptr)";
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
  let constructor = "mlir::arts::createCreateDbsPass(nullptr)";
}

def DbLowering : Pass<"db-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS DataBlock handles to opaque pointer form";
  let description = [{
    This pass converts ARTS DataBlocks from typed memrefs to the opaque pointer
    representation required by the runtime. It assumes the datablocks are already
    in canonical form.
  }];
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createDbLoweringPass()";
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
  let constructor = "mlir::arts::createEdtPass(nullptr, true)";
  let options = [
    Option<"runAnalysis", "run-analysis", "bool", /*default=*/"true",
           "Enable EDT analysis.">
  ];
}

def Concurrency : Pass<"concurrency", "mlir::ModuleOp"> {
  let summary = "Build EDT concurrency graph and apply concurrency-driven cleanups";
  let dependentDialects = [
    "func::FuncDialect",
    "arts::ArtsDialect"
  ];
  let constructor = "mlir::arts::createConcurrencyPass(nullptr)";
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

def ArtsDataPointerHoisting : Pass<"arts-data-pointer-hoisting", "mlir::ModuleOp"> {
  let summary = "Hoist data pointer loads from deps struct out of loops";
  let description = [{
    This pass hoists llvm.load operations that extract data pointers from the
    ARTS deps struct (artsEdtDep_t.ptr field) to before enclosing loops.

    The deps struct contains pointers to data arrays. Without hoisting, these
    pointer loads happen inside innermost loops (O(n^3) loads for 3-nested loops).
    With hoisting, the pointers are loaded once before the loop nest (O(1) loads).

    This is critical for performance because LLVM's LICM cannot hoist these loads
    without TBAA metadata proving they don't alias with data stores.

    Example transformation:
    ```mlir
    scf.for %i = ... {
      scf.for %j = ... {
        %ptr = llvm.load %dep_ptr_addr : !llvm.ptr -> !llvm.ptr  // Load every iteration
        %val = polygeist.load %ptr[%i, %j] : memref<?x?xf64>
      }
    }
    ```
    becomes:
    ```mlir
    %ptr = llvm.load %dep_ptr_addr : !llvm.ptr -> !llvm.ptr  // Load once
    scf.for %i = ... {
      scf.for %j = ... {
        %val = polygeist.load %ptr[%i, %j] : memref<?x?xf64>
      }
    }
    ```
  }];
  let dependentDialects = [
    "arts::ArtsDialect",
    "LLVM::LLVMDialect",
    "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::createArtsDataPointerHoistingPass()";
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

def ParallelEdtLowering : Pass<"parallel-edt-lowering", "mlir::ModuleOp"> {
  let summary = "Lower ARTS parallel EDTs to task graphs";
  let description = [{
    This pass transforms parallel EDT operations that contain loop constructs into
    epoch-wrapped task graphs. It must execute before datablock lowering to ensure
    proper dependency management.
  }];
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "scf::SCFDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createParallelEdtLoweringPass()";
}

def ForLowering : Pass<"for-lowering", "mlir::ModuleOp"> {
  let summary = "Lower arts.for inside parallel EDTs to chunked task EDTs";
  let description = [{
    Lowers arts.for operations nested within arts.edt<parallel> by creating a
    symbolic worker-id placeholder, computing chunk bounds, acquiring required
    datablocks, and generating a task EDT with a local scf.for over the chunk.
  }];
  let dependentDialects = [
    "memref::MemRefDialect",
    "LLVM::LLVMDialect",
    "func::FuncDialect",
    "arts::ArtsDialect",
    "arith::ArithDialect",
    "scf::SCFDialect",
    "polygeist::PolygeistDialect"
  ];
  let constructor = "mlir::arts::createForLoweringPass()";
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
