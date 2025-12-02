///==========================================================================///
/// File: DbRewriter.h
///
/// This file defines the DbRewriter class which automatically rewrites all uses
/// of a datablock allocation when its structure changes. The rewriter analyzes
/// structural differences between old and new allocations and transforms all
/// uses accordingly.
///
/// FUNDAMENTAL PRINCIPLE - Outer vs Inner Dimensions:
/// DbAllocOp structure:
///   db_alloc sizes[outer_dims] elementSizes[inner_dims]
///            ↓                 ↓
///            db_ref indices    memref.load/store indices
///
/// - db_ref operates on OUTER dimensions (sizes) - selects which datablock
/// - memref.load/store operates on INNER dimensions (elementSizes) - accesses
///   within the selected datablock
///
/// Example:
///   db_alloc sizes=[N] elementSizes=[M]
///   - N datablocks, each containing M elements
///   - db_ref[i] selects datablock i, load[j] accesses element j within it
///
/// When promoting elementSizes to sizes (coarse->fine transformation):
///   OLD: sizes=[1], elementSizes=[N]  -> db_ref[0] + load[i]
///   NEW: sizes=[N], elementSizes=[1]  -> db_ref[i] + load[0]
///
/// For coarse->fine: old inner indices (data) -> new outer, new inner = 0
/// For fine->coarse: old outer indices (data) -> new inner, new outer = 0
///
/// The rewriteDbUserOperation() helper applies these transformations when
/// rewriting load/store operations after allocation structure changes.
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
/// IndexMapping - Describes how indices map between old and new structures
///
/// Instead of categorizing transformations (Chunking, Promotion, etc.), this
/// structure directly describes how to build new indices from old ones.
///===----------------------------------------------------------------------===///
struct IndexMapping {
  /// Source of each index in the mapping
  enum class Source {
    OuterIndex,  /// From old db_ref indices
    InnerIndex,  /// From old load/store indices
    ConstantZero /// Synthesized constant 0
  };

  /// A single index mapping entry
  struct Entry {
    Source source;
    unsigned sourcePosition; /// Position in source array (outer or inner)

    Entry(Source s, unsigned pos) : source(s), sourcePosition(pos) {}
  };

  SmallVector<Entry> newOuterIndices; /// How to build new db_ref indices
  SmallVector<Entry> newInnerIndices; /// How to build new load/store indices

  /// Quick checks for transformation type
  bool isIdentity() const;
  bool isCoarseToFine() const;
  bool isFineToCoarse() const;
};

///===----------------------------------------------------------------------===///
/// DbRewriter - Automated use transformation for datablock restructuring
///
/// DbRewriter automatically rewrites all uses of a datablock allocation when
/// its structure changes. It handles:
/// - Load/Store operations
/// - DbRef operations (both direct and through EDTs)
/// - DbAcquire operations
/// - Block arguments and indirect uses
///
/// The rewriter computes an IndexMapping by comparing old and new allocation
/// structures, then applies that mapping uniformly to all uses.
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
  /// Index Mapping (core of the redesign)
  ///===--------------------------------------------------------------------===///

  /// Compute the index mapping between old and new allocation structures.
  /// Detects coarse-to-fine, fine-to-coarse, or identity transformations.
  IndexMapping computeIndexMapping(DbAllocOp oldAlloc, DbAllocOp newAlloc);

  /// Apply the index mapping to transform indices.
  /// Takes old outer indices (from db_ref) and old inner indices (from
  /// load/store), returns new outer indices and new inner indices.
  std::pair<SmallVector<Value>, SmallVector<Value>>
  transformIndices(ArrayRef<Value> oldOuterIndices,
                   ArrayRef<Value> oldInnerIndices, OpBuilder &builder,
                   Location loc);

  ///===--------------------------------------------------------------------===///
  /// Use collection
  ///===--------------------------------------------------------------------===///

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

  UseSet collectUsesRecursive(Value source, SmallPtrSet<Value, 8> &visited);
  UseSet collectUsesInEdt(Value source, EdtOp edt);

  ///===--------------------------------------------------------------------===///
  /// Transformation (simplified - no more diff parameter)
  ///===--------------------------------------------------------------------===///
  LogicalResult transformLoad(memref::LoadOp load);
  LogicalResult transformStore(memref::StoreOp store);
  LogicalResult transformDbRef(DbRefOp dbRef);
  LogicalResult transformDbAcquire(DbAcquireOp acquire);

  ///===--------------------------------------------------------------------===///
  /// Cleanup and verification
  ///===--------------------------------------------------------------------===///
  void cleanup();
  LogicalResult verify();

  ///===--------------------------------------------------------------------===///
  /// Member variables
  ///===--------------------------------------------------------------------===///
  Config config_;
  OpBuilder builder_;
  Statistics stats_;

  DbAllocOp oldAlloc_;
  DbAllocOp newAlloc_;
  IndexMapping mapping_; /// Computed once in rewriteAllUses, used everywhere

  SetVector<Operation *> opsToRemove_;
  DenseMap<Value, Value> valueMapping_;
};

/// Shared helper used by CreateDbs/DbPass to rewrite loads/stores after
/// Db slicing. Applies the COARSE-TO-FINE transformation:
///
/// Parameters:
/// - op: The operation to rewrite (LoadOp, StoreOp, or DbRefOp)
/// - elementMemRefType: The element type for the new db_ref result
/// - basePtr: The base pointer (acquired datablock) to use
/// - dbOp: The source db operation (for tracking)
/// - initialIndex: Number of dimensions consumed by acquire indices.
///                 When > 0, these dimensions were selected by the acquire
///                 and should be removed from the transformed indices.
/// - chunkOffsets: Offset values to subtract for local indexing within chunk.
///                 When non-empty, inner indices become (index - offset).
/// - builder: OpBuilder for creating new operations
/// - opsToRemove: Set to collect operations that should be erased
///
/// Transformation applied to LoadOp/StoreOp:
///   OLD: load[inner_indices]
///   NEW: db_ref[inner_indices - chunkOffsets] + load[zeros]
///
/// When initialIndex > 0 (acquire selected specific elements):
///   The first 'initialIndex' dimensions are stripped from inner_indices
///   before moving them to outer. If none remain, outer = [0].
bool rewriteDbUserOperation(Operation *op, Type elementMemRefType,
                            Value basePtr, Operation *dbOp,
                            uint64_t initialIndex, ArrayRef<Value> chunkOffsets,
                            OpBuilder &builder,
                            llvm::SetVector<Operation *> &opsToRemove);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DBREWRITER_H
