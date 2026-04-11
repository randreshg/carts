///==========================================================================///
/// File: Passes.td
///
/// Pass declarations for the SDE (Structured Decomposition Environment)
/// dialect.
///==========================================================================///
#ifndef ARTS_DIALECT_SDE_TRANSFORMS_PASSES
#define ARTS_DIALECT_SDE_TRANSFORMS_PASSES

include "mlir/Pass/PassBase.td"

def RaiseToLinalg : Pass<"raise-to-linalg", "mlir::ModuleOp"> {
  let summary = "Raise supported sde.su_iterate bodies to transient linalg.generic carriers";
  let description = [{
    Analyzes loop nests inside sde.su_iterate operations, stamps structural
    classification on the source loop, and materializes transient
    linalg.generic carriers for the currently supported subset of loop bodies.
    Unsupported bodies remain in loop form for downstream passes to handle
    via independent IR analysis.

    Recognized patterns:
    - elementwise: all parallel iterators, identity/permutation indexing maps
    - matmul: triple-nested with multiply-accumulate and reduction dimension
    - stencil: parallel iterators with constant-offset indexing maps
    - reduction: has reduction iterators with accumulation pattern

    Today, only the elementwise/matmul-style subset is materialized as a
    linalg.generic carrier. Stencil/reduction bodies currently stay on the
    stamped-attribute fallback path.

    ConvertSdeToArts consumes the transient linalg.generic ops, stamps pattern
    contracts on the created arts.for, and erases the carriers so downstream
    passes continue on loop/memref IR.
  }];
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::func::FuncDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::linalg::LinalgDialect",
                           "::mlir::scf::SCFDialect"];
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
  let dependentDialects = ["::mlir::memref::MemRefDialect",
                           "::mlir::omp::OpenMPDialect",
                           "::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arts::ArtsDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::scf::SCFDialect"];
  let constructor = "mlir::arts::sde::createConvertOpenMPToSdePass()";
}

def ConvertSdeToArts : Pass<"convert-sde-to-arts", "mlir::ModuleOp"> {
  let summary = "Convert SDE operations to ARTS operations";
  let description = [{
    Converts SDE dialect ops into ARTS dialect ops, producing the same IR
    that ConvertOpenMPToArts currently generates. When RaiseToLinalg
    materializes transient linalg.generic carriers, this pass derives the
    matching ARTS contract from them, copies semantic contract attributes, and
    erases the carriers so downstream passes continue on loop/memref IR. When
    no linalg carrier is present, it falls back to RaiseToLinalg's stamped
    classification attribute.
  }];
  let dependentDialects = ["::mlir::arts::sde::ArtsSdeDialect",
                           "::mlir::arts::ArtsDialect",
                           "::mlir::memref::MemRefDialect",
                           "::mlir::arith::ArithDialect",
                           "::mlir::linalg::LinalgDialect"];
  let constructor = "mlir::arts::sde::createConvertSdeToArtsPass()";
}

#endif // ARTS_DIALECT_SDE_TRANSFORMS_PASSES
