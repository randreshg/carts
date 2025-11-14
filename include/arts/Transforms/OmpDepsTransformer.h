///==========================================================================///
/// File: OmpDepsTransformer.h
///
/// This file defines the OmpDepsTransformer class which canonicalizes OpenMP
/// task dependencies to arts.omp_dep operations.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_OMPDEPSTRANSFORMER_H
#define ARTS_TRANSFORMS_OMPDEPSTRANSFORMER_H

#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// OmpDepsTransformer - Canonicalizes OpenMP task dependencies
///
/// Handles canonicalization of OpenMP task dependencies:
/// - Scalar dependencies (direct values)
/// - Array element dependencies (via load operations)
/// - Chunk dependencies (via subview operations)
/// - Token container patterns (alloca/store/load)
///
/// Transforms dependencies to arts.omp_dep operations that explicitly encode:
/// - Source memref
/// - Access indices
/// - Chunk sizes
/// - Dependency mode (in/out/inout)
///
/// This enables accurate dependency tracking for parallel tasks.
///===----------------------------------------------------------------------===///
class OmpDepsTransformer {
public:
  /// Configuration options for transformation
  struct Config {
    /// Print detailed transformation info
    bool verbose;

    Config() : verbose(false) {}
  };

  /// Statistics about the transformation
  struct Statistics {
    unsigned tasksProcessed;
    unsigned depsCanonical;

    Statistics() : tasksProcessed(0), depsCanonical(0) {}

    void print(llvm::raw_ostream &os) const;
  };

  /// Construct a transformer with the specified context and configuration
  OmpDepsTransformer(MLIRContext *context, Config config = Config());

  /// Main entry point: canonicalize all OpenMP task dependencies in the module
  /// Returns success if all dependencies were successfully canonicalized
  LogicalResult transform(ModuleOp module, OpBuilder &builder);

  /// Get statistics about the last transformation
  Statistics getStatistics() const { return stats_; }

private:
  ///===--------------------------------------------------------------------===///
  /// Internal Structures
  ///===--------------------------------------------------------------------===///

  struct DepInfo {
    Value source;
    SmallVector<Value> indices;
    SmallVector<Value> sizes;
  };

  ///===--------------------------------------------------------------------===///
  /// Internal Methods
  ///===--------------------------------------------------------------------===///

  /// Process all tasks in the module
  void canonicalizeDependencies();

  /// Canonicalize dependencies for a single task operation
  void canonicalizeTask(omp::TaskOp task);

  /// Extract dependency information from a dependency variable
  std::optional<DepInfo> extractDepInfo(Value depVar, OpBuilder &builder,
                                        Location loc);

  /// Helper: Extract memref and indices from a load operation
  std::optional<std::pair<Value, SmallVector<Value>>>
  extractLoadInfo(Operation *op);

  /// Helper: Check if memref is token container and return stored value
  std::optional<Value> followTokenContainer(Value memref);

  /// Helper: Get dependency mode from task for a specific variable
  std::optional<omp::ClauseTaskDepend> getDepModeFromTask(omp::TaskOp task,
                                                          Value depVar);

  /// Helper: Materialize an OpFoldResult as a Value
  Value materializeOpFoldResult(OpFoldResult ofr, OpBuilder &builder,
                                Location loc);

  ///===--------------------------------------------------------------------===///
  /// Member Variables
  ///===--------------------------------------------------------------------===///

  MLIRContext *ctx_;
  Config config_;
  Statistics stats_;
  ModuleOp module_;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_OMPDEPSTRANSFORMER_H
