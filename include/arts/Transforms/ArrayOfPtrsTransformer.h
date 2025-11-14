///==========================================================================///
/// File: ArrayOfPtrsTransformer.h
///
/// This file defines the ArrayOfPtrsTransformer class which eliminates
/// array-of-pointers patterns by replacing wrapper allocas with direct usage.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_ARRAYOFPTRSTRANSFORMER_H
#define ARTS_TRANSFORMS_ARRAYOFPTRSTRANSFORMER_H

#include "arts/Utils/OpRemovalManager.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// ArrayOfPtrsTransformer - Eliminates array-of-pointers wrapper patterns
///
/// Handles rank-0 allocas that store memref allocations:
///   Example: memref<memref<?xi32>> where the alloca wraps an allocation
///   Transforms cascaded accesses to direct memref usage
///
/// Pattern detection:
/// - Finds rank-0 allocas with memref element types
/// - Identifies direct stores (not in loops) of AllocOp results
/// - Ensures there are load operations for cascaded access
///
/// Transformation:
/// - Replaces loads from the alloca with direct allocation usage
/// - Eliminates the wrapper alloca and its store operation
/// - All accesses become direct without indirection
///
/// Note: This transformer only handles memref.load/store operations.
/// Affine operations are lowered before this pass runs.
///===----------------------------------------------------------------------===///
class ArrayOfPtrsTransformer {
public:
  /// Configuration options for transformation
  struct Config {
    /// Print detailed transformation info
    bool verbose;

    Config() : verbose(false) {}
  };

  /// Statistics about the transformation
  struct Statistics {
    unsigned patternsDetected;
    unsigned patternsTransformed;
    unsigned accessesRewritten;

    Statistics()
        : patternsDetected(0), patternsTransformed(0), accessesRewritten(0) {}

    void print(llvm::raw_ostream &os) const;
  };

  /// Construct a transformer with the specified context and configuration
  ArrayOfPtrsTransformer(MLIRContext *context, Config config = Config());

  /// Main entry point: transform all array-of-pointers patterns in the module
  /// Returns success if all patterns were successfully transformed
  LogicalResult transform(ModuleOp module, OpBuilder &builder,
                          OpRemovalManager &removalMgr);

  /// Get statistics about the last transformation
  Statistics getStatistics() const { return stats_; }

private:
  ///===--------------------------------------------------------------------===///
  /// Pattern Structure
  ///===--------------------------------------------------------------------===///

  struct Pattern {
    Value alloca;
    Value storedAlloc;
    Operation *storeOp;

    Pattern() = default;
    Pattern(Pattern &&) = default;
    Pattern &operator=(Pattern &&) = default;
    Pattern(const Pattern &) = delete;
    Pattern &operator=(const Pattern &) = delete;
  };

  ///===--------------------------------------------------------------------===///
  /// Internal Methods
  ///===--------------------------------------------------------------------===///

  /// Detect an array-of-pointers pattern starting from an alloca
  std::optional<Pattern> detectPattern(Value alloca);

  /// Transform all accesses to use the stored allocation directly
  LogicalResult transformAccesses(Pattern &pattern);

  /// Clean up the wrapper alloca and store operation
  void cleanupPattern(Pattern &pattern, OpRemovalManager &removalMgr);

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

#endif // ARTS_TRANSFORMS_ARRAYOFPTRSTRANSFORMER_H
