///==========================================================================///
/// File: NestedAllocTransformer.h
///
/// This file defines the NestedAllocTransformer class which transforms nested
/// array-of-arrays allocations into canonical multi-dimensional memrefs.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_NESTEDALLOCTRANSFORMER_H
#define ARTS_TRANSFORMS_NESTEDALLOCTRANSFORMER_H

#include "arts/Utils/OpRemovalManager.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// NestedAllocTransformer - Transforms nested allocations to canonical form
///
/// Handles arrays-of-arrays patterns where an outer allocation contains
/// pointers to inner allocations:
///   Example: A[N][M] where A is memref<?xmemref<?xf64>>
///   Transforms to: memref<?x?xf64>
///
/// The transformer:
/// - Detects nested allocation patterns by analyzing initialization loops
/// - Creates canonical N-dimensional allocations
/// - Rewrites all cascaded accesses to direct N-dimensional indexing
/// - Cleans up legacy allocations and initialization loops
/// - Handles wrapper allocas (rank-0 allocas storing outer arrays)
///===----------------------------------------------------------------------===///
class NestedAllocTransformer {
public:
  /// Configuration options for transformation
  struct Config {
    /// Ensure index semantics are preserved
    bool preserveSemantics;
    /// Print detailed transformation info
    bool verbose;

    Config() : preserveSemantics(true), verbose(false) {}
  };

  /// Statistics about the transformation
  struct Statistics {
    unsigned patternsDetected;
    unsigned patternsTransformed;
    unsigned accessesRewritten;
    unsigned allocationsRemoved;

    Statistics()
        : patternsDetected(0), patternsTransformed(0), accessesRewritten(0),
          allocationsRemoved(0) {}

    void print(llvm::raw_ostream &os) const;
  };

  /// Construct a transformer with the specified context and configuration
  NestedAllocTransformer(MLIRContext *context, Config config = Config());

  /// Main entry point: transform all nested allocations in the module
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
    Value outerAlloc;
    Value dimSize;
    Value innerDimSize;
    Value innerAllocValue;
    Type elementType;
    scf::ForOp initLoop;
    std::unique_ptr<Pattern> inner;
    Value wrapperAlloca; // Optional: rank-0 alloca that stores outerAlloc

    Pattern() = default;
    Pattern(Pattern &&) = default;
    Pattern &operator=(Pattern &&) = default;
    Pattern(const Pattern &) = delete;
    Pattern &operator=(const Pattern &) = delete;

    int getDepth() const { return inner ? 1 + inner->getDepth() : 2; }

    SmallVector<Value> getAllSizes() const;
    SmallVector<scf::ForOp> getAllInitLoops() const;
    SmallVector<Value> getAllNestedAllocs() const;
    Type getFinalElementType() const { return elementType; }
  };

  ///===--------------------------------------------------------------------===///
  /// Internal Methods
  ///===--------------------------------------------------------------------===///

  /// Detect a nested allocation pattern starting from an allocation
  std::optional<Pattern> detectPattern(Value alloc);

  /// Transform a pattern to canonical form
  Value transformToCanonical(Pattern &pattern, Location loc,
                             OpBuilder &builder);

  /// Transform all accesses to use the canonical allocation
  LogicalResult transformAccesses(Pattern &pattern, Value ndAlloc);

  /// Clean up legacy allocations and operations
  void cleanupPattern(Pattern &pattern, OpRemovalManager &removalMgr);

  /// Helper: Get dynamic sizes from an allocation operation
  SmallVector<Value> getAllocDynamicSizes(Operation *allocOp);

  /// Helper: Create a canonical N-dimensional allocation
  Value createCanonicalAllocation(OpBuilder &builder, Location loc,
                                  Type elementType, ArrayRef<Value> dimSizes);

  /// Helper: Transfer metadata from old to new allocation
  void transferMetadata(Operation *oldAlloc, Operation *newAlloc);

  /// Helper: Create deallocation operations for new allocation
  void duplicateDeallocations(Value oldAlloc, Value newAlloc,
                              OpBuilder &builder);

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

#endif // ARTS_TRANSFORMS_NESTEDALLOCTRANSFORMER_H
