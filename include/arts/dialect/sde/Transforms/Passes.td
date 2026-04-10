///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the SDE (Structured Decomposition Environment) dialect.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES

include "mlir/Pass/PassBase.td"

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
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "affine::AffineDialect"
  ];
  let options = [
    Option<"exportMetadata", "export-metadata", "bool", "false",
           "Export metadata to JSON file after collection">,
    Option<"metadataFile", "metadata-file", "std::string",
           "\".carts-metadata.json\"", "Path to metadata JSON file for export">
  ];
  let constructor = "mlir::arts::createCollectMetadataPass()";
}

def RaiseToLinalg : Pass<"raise-to-linalg", "mlir::ModuleOp"> {
  let summary = "Compute linalg-style structural classification for loop nests";
  let description = [{
    Analyzes loop nests inside arts.for operations and computes linalg-style
    structural information: indexing maps, iterator types, and pattern
    classification. Stamps the results as attributes on the arts.for op.

    Recognized patterns:
    - elementwise: all parallel iterators, identity/permutation indexing maps
    - matmul: triple-nested with multiply-accumulate and reduction dimension
    - stencil: parallel iterators with constant-offset indexing maps
    - reduction: has reduction iterators with accumulation pattern

    The stamped attributes enable PatternDiscovery to leverage structural
    classification instead of manual access-pattern heuristics.
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "func::FuncDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::sde::createRaiseToLinalgPass()";
}

def ConvertOpenMPToSde : Pass<"convert-openmp-to-sde", "mlir::ModuleOp"> {
  let summary = "Convert OpenMP operations to SDE operations";
  let description = [{
    Converts OpenMP ops into SDE ops, preserving information that the
    direct OMP-to-ARTS conversion loses:
    - Reduction combiner kind + identity
    - Nowait semantics
    - Schedule + chunk size
    - Task completion tokens
  }];
  let dependentDialects = [
    "memref::MemRefDialect", "omp::OpenMPDialect",
    "arts::sde::ArtsSdeDialect", "arts::ArtsDialect",
    "arith::ArithDialect", "scf::SCFDialect"
  ];
  let constructor = "mlir::arts::sde::createConvertOpenMPToSdePass()";
}

def ConvertSdeToArts : Pass<"convert-sde-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert SDE operations to ARTS operations";
  let description = [{
    Converts SDE dialect ops into ARTS dialect ops, producing the same IR
    that ConvertOpenMPToArts currently generates.
  }];
  let dependentDialects = [
    "arts::sde::ArtsSdeDialect", "arts::ArtsDialect",
    "memref::MemRefDialect", "arith::ArithDialect"
  ];
  let constructor = "mlir::arts::sde::createConvertSdeToArtsPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES
