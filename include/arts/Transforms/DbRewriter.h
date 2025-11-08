///==========================================================================///
/// File: DbRewriter.h
///
/// This file defines the DbRewriter class which automatically rewrites all uses
/// of a datablock allocation when its structure changes. The rewriter analyzes
/// structural differences between old and new allocations and transforms all
/// uses accordingly.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DBREWRITER_H
#define ARTS_TRANSFORMS_DBREWRITER_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// DbRewriter - Automated use transformation for datablock restructuring
///
/// DbRewriter automatically rewrites all uses of a datablock allocation when
/// its structure changes. It handles:
/// - Load/Store operations
/// - DbRef operations (both direct and through EDTs)
/// - DbAcquire operations
/// - Block arguments and indirect uses
/// The rewriter infers the transformation strategy by analyzing the structural
/// differences between the old and new allocations.
///===----------------------------------------------------------------------===///
class DbRewriter {
public:
  /// Configuration options for rewriting
  struct Config {
    /// Ensure index semantics are preserved
    bool preserveSemantics;
    /// Clean up old operations
    bool removeDeadOps;
    /// Remove dependent dead operations
    bool recursiveRemoval;
    /// Print detailed transformation info
    bool verbose;

    Config()
        : preserveSemantics(true), removeDeadOps(true), recursiveRemoval(true),
          verbose(false) {}
  };

  /// Statistics about the transformation
  struct Statistics {
    unsigned loadsTransformed;
    unsigned storesTransformed;
    unsigned dbRefsUpdated;
    unsigned acquiresUpdated;
    unsigned operationsRemoved;

    Statistics()
        : loadsTransformed(0), storesTransformed(0), dbRefsUpdated(0),
          acquiresUpdated(0), operationsRemoved(0) {}

    void print(llvm::raw_ostream &os) const;
  };

  /// Construct a DbRewriter with the specified context and configuration
  DbRewriter(MLIRContext *context, Config config = Config());

  /// Main entry point: rewrite all uses of oldAlloc to use newAlloc
  /// Automatically infers transformation strategy from structural differences
  /// Returns success if all uses were successfully transformed
  LogicalResult rewriteAllUses(DbAllocOp oldAlloc, DbAllocOp newAlloc);

  /// Get statistics about the last transformation
  Statistics getStatistics() const { return stats_; }

  /// Get operations that will be removed (for external cleanup)
  const SetVector<Operation *> &getOpsToRemove() const { return opsToRemove_; }

private:
  ///===--------------------------------------------------------------------===///
  /// Internal data structures
  ///===--------------------------------------------------------------------===///

  /// Structural differences between allocations
  struct StructuralDiff {
    enum ChangeType {
      NoChange,   /// Identical structure - no transformation needed
      Promotion,  /// Dimensions moved from sizes to elementSizes
      Chunking,   /// Array divided into chunks (e.g., 1→8x8 grid)
      Refinement, /// Sizes changed but rank preserved
      Reshape     /// Complete restructuring
    } changeType = NoChange;

    /// Transformation parameters
    size_t promotedDims = 0;          /// Number of promoted dimensions
    SmallVector<Value> chunkSizes;    /// Chunk sizes for chunking
    size_t oldOuterRank = 0;          /// Original outer rank
    size_t newOuterRank = 0;          /// New outer rank
    bool needsIndexTransform = false; /// Whether index transformation is needed
  };

  /// Collection of uses by type
  struct UseSet {
    SmallVector<memref::LoadOp> loads;
    SmallVector<memref::StoreOp> stores;
    SmallVector<DbRefOp> dbRefs;
    SmallVector<DbAcquireOp> acquires;
    /// Casts, subviews, etc.
    SmallVector<Operation *> other;

    bool empty() const {
      return loads.empty() && stores.empty() && dbRefs.empty() &&
             acquires.empty() && other.empty();
    }

    size_t size() const {
      return loads.size() + stores.size() + dbRefs.size() + acquires.size() +
             other.size();
    }
  };

  ///===--------------------------------------------------------------------===///
  /// Analysis
  ///===--------------------------------------------------------------------===///

  /// Analyze structural differences between allocations
  StructuralDiff analyzeChanges(DbAllocOp oldAlloc, DbAllocOp newAlloc);

  /// Configure index transformer based on structural differences
  void configureIndexTransformer(const StructuralDiff &diff);

  ///===--------------------------------------------------------------------===///
  /// Use collection
  ///===--------------------------------------------------------------------===///
  UseSet collectUsesRecursive(Value source, SmallPtrSet<Value, 8> &visited);
  UseSet collectUsesInEdt(Value source, EdtOp edt);

  ///===--------------------------------------------------------------------===///
  /// Transformation
  ///===--------------------------------------------------------------------===///
  LogicalResult transformLoad(memref::LoadOp load, const StructuralDiff &diff);
  LogicalResult transformStore(memref::StoreOp store,
                               const StructuralDiff &diff);
  LogicalResult transformDbRef(DbRefOp dbRef, const StructuralDiff &diff);
  LogicalResult transformDbAcquire(DbAcquireOp acquire,
                                   const StructuralDiff &diff);
  LogicalResult transformBlockArgument(BlockArgument arg, Value newSource,
                                       const StructuralDiff &diff);

  /// Replace uses of old DbAllocOp with new promoted DbAllocOp
  void updateDbAllocAfterPromotion(DbAllocOp oldAlloc, DbAllocOp newAlloc,
                                   size_t pushCount,
                                   SetVector<Operation *> &opsToRemove);
  void updateDbRefOpsAfterPromotion(Value sourcePtr, Type elementMemRefType,
                                    SetVector<Operation *> &opsToRemove,
                                    Value newSourcePtr);
  void updateDbAcquireAfterPromotion(DbAcquireOp acqOp, DbAllocOp newAlloc,
                                     size_t pushCount, OpBuilder &b,
                                     Location loc,
                                     SetVector<Operation *> &opsToRemove);
  ///===--------------------------------------------------------------------===///
  /// Cleanup and verification
  ///===--------------------------------------------------------------------===///
  void cleanup();
  LogicalResult verify();

  ///===--------------------------------------------------------------------===///
  /// Member variables
  ///===--------------------------------------------------------------------===///
  MLIRContext *context_;
  Config config_;
  OpBuilder builder_;
  Statistics stats_;

  DbAllocOp oldAlloc_;
  DbAllocOp newAlloc_;
  StructuralDiff diff_;

  SetVector<Operation *> opsToRemove_;
  DenseMap<Value, Value> valueMapping_;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DBREWRITER_H
