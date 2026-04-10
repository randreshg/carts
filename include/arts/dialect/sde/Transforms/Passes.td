///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the SDE (Structured Decomposition Environment) dialect.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES

include "mlir/Pass/PassBase.td"

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
    "::mlir::memref::MemRefDialect", "::mlir::func::FuncDialect",
    "::mlir::arts::ArtsDialect", "::mlir::arith::ArithDialect",
    "::mlir::scf::SCFDialect"
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
    "::mlir::memref::MemRefDialect", "::mlir::omp::OpenMPDialect",
    "::mlir::arts::sde::ArtsSdeDialect", "::mlir::arts::ArtsDialect",
    "::mlir::arith::ArithDialect", "::mlir::scf::SCFDialect"
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
    "::mlir::arts::sde::ArtsSdeDialect", "::mlir::arts::ArtsDialect",
    "::mlir::memref::MemRefDialect", "::mlir::arith::ArithDialect"
  ];
  let constructor = "mlir::arts::sde::createConvertSdeToArtsPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES
