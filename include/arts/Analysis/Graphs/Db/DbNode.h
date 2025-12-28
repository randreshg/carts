///==========================================================================///
/// Db/DbNode.h - Db-specific nodes derived from NodeBase
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H

#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace mlir {
namespace arts {

/// Forward declarations
class DbAnalysis;
class DbAcquireNode;
class ArtsMetadataManager;

////===----------------------------------------------------------------------===////
/// DbAllocNode
/// Represents a `arts.db.alloc` operation in the DB graph.
////===----------------------------------------------------------------------===////
class DbAllocNode : public NodeBase, public MemrefMetadata {
public:
  DbAllocNode(DbAllocOp op, DbAnalysis *analysis);

  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return op; }

  void addInEdge(EdgeBase *edge) override { inEdges.insert(edge); }
  void addOutEdge(EdgeBase *edge) override { outEdges.insert(edge); }
  const DenseSet<EdgeBase *> &getInEdges() const override { return inEdges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return outEdges; }

  /// Graph structure management
  DbAcquireNode *getOrCreateAcquireNode(DbAcquireOp op);
  void forEachChildNode(const std::function<void(NodeBase *)> &fn) const;
  size_t getAcquireNodesSize() const { return acquireNodes.size(); }

  /// Operation accessors
  DbAllocOp getDbAllocOp() const { return dbAllocOp; }
  DbFreeOp getDbFreeOp() const { return dbFreeOp; }
  bool isStringDatablock() const { return isStringBacked; }
  NodeKind getKind() const override { return NodeKind::DbAlloc; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAlloc;
  }

  /// Returns true if the memref is accessed in parallel loops and is not a
  /// string datablock
  bool isParallelFriendly() const;

  /// Check if this allocation can be partitioned for parallel execution.
  /// Validates allocation metadata and recursively checks all acquires.
  bool canBePartitioned();

  /// Check if this allocation is already fine-grained
  /// (all elementSizes are constant 1, meaning each datablock holds one
  /// element)
  bool isFineGrained() const;

  /// Check if at most one acquire writes to this allocation.
  /// If true, twin-diff is not needed (no write conflicts possible).
  bool hasSingleWriter() const;

  /// Check if all acquires use worker-indexed pattern (array[workerId] size=1).
  /// If true, all accesses are inherently disjoint.
  bool allAcquiresWorkerIndexed() const;

  /// Analyze if all acquires can be proven non-overlapping.
  /// Uses multiple detection methods: alias analysis, partition hints,
  /// worker-indexed patterns, and single-writer detection.
  bool canProveNonOverlapping() const;

  /// Collect acquire access patterns and summarize them at allocation level.
  /// Returns AcquirePatternSummary (defined in DbAccessPattern.h)
  AcquirePatternSummary summarizeAcquirePatterns() const;

  /// Returns true if any acquire is stencil-like.
  bool hasStencilAccess() const;

  /// Returns true if allocation mixes different access patterns.
  bool hasMixedAccessPatterns() const;

  /// Analysis metadata
  uint64_t inCount = 0, outCount = 0;
  uint64_t beginIndex = 0, endIndex = 0;
  bool isLongLived = false;
  uint64_t maxLoopDepth = 0;
  uint64_t criticalSpan = 0, criticalPath = 0;
  unsigned long long totalAccessBytes = 0;
  uint64_t numAcquires = 0;
  bool isStringBacked = false;

private:
  DbAllocOp dbAllocOp;
  DbFreeOp dbFreeOp;
  Operation *op;
  DbAnalysis *analysis;
  std::string hierId;
  unsigned nextChildId = 1;

  /// Graph structure: children acquire nodes
  SmallVector<std::unique_ptr<DbAcquireNode>, 4> acquireNodes;
  DenseMap<DbAcquireOp, DbAcquireNode *> acquireMap;
};

////===----------------------------------------------------------------------===////
/// DbAcquireNode
/// Represents a `arts.db.acquire` operation in the DB graph.
////===----------------------------------------------------------------------===////
class DbAcquireNode : public NodeBase {
public:
  /// AccessPattern enum is now defined in DbAccessPattern.h

  DbAcquireNode(DbAcquireOp op, NodeBase *parent, DbAllocNode *rootAlloc,
                DbAnalysis *analysis, std::string initialHierId = "");

  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return op; }

  void addInEdge(EdgeBase *edge) override { inEdges.insert(edge); }
  void addOutEdge(EdgeBase *edge) override { outEdges.insert(edge); }
  const DenseSet<EdgeBase *> &getInEdges() const override { return inEdges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return outEdges; }

  DbAllocNode *getParent() const { return rootAlloc; }
  NodeBase *getDirectParent() const { return parent; }
  DbAllocNode *getRootAlloc() const { return rootAlloc; }
  DbAnalysis *getAnalysis() const { return analysis; }

  NodeKind getKind() const override { return NodeKind::DbAcquire; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAcquire;
  }

  EdtOp getEdtUser() const { return EdtOp(edtUserOp); }
  Value getUseInEdt() const { return useInEdt; }

  /// Collect access operations mapping DbRefOp to its memory operations.
  /// All data accesses go through DbRefOp, this preserves that relationship.
  /// This is the primary API for accessing memory operations.
  void collectAccessOperations(
      DenseMap<DbRefOp, SetVector<Operation *>> &dbRefToMemOps);

  /// Helper methods for querying memory access counts.
  /// These use collectAccessOperations internally.
  bool hasLoads();
  bool hasStores();
  bool hasMemoryAccesses();
  size_t countLoads();
  size_t countStores();

  DbAcquireOp getDbAcquireOp() const { return dbAcquireOp; }
  DbReleaseOp getDbReleaseOp() const { return dbReleaseOp; }

  SmallVector<Value, 4> computeInvariantIndices();

  DbAcquireNode *getOrCreateAcquireNode(DbAcquireOp op);
  void forEachChildNode(const std::function<void(NodeBase *)> &fn) const;

  /// Check if this acquire can be partitioned for parallel execution.
  /// Validates EDT type, memory accesses, access patterns, and nested children.
  bool canBePartitioned();

  /// Check if array accesses use indices derived from the given offset.
  /// Returns true if partitioning is safe (first index = offset + delta).
  bool canPartitionWithOffset(Value offset);

  LogicalResult computeChunkInfo(Value &chunkOffset, Value &chunkSize);
  LogicalResult computeChunkInfoFromWhile(scf::WhileOp whileOp,
                                          Value &chunkOffset, Value &chunkSize,
                                          Value *offsetForCheck = nullptr);

  /// StencilBounds struct is now defined in DbAccessPattern.h

  /// Get stencil bounds for this acquire (computed during canBePartitioned)
  const std::optional<StencilBounds> &getStencilBounds() const {
    return stencilBounds_;
  }

  /// Classify this acquire's access pattern using current analysis state.
  AccessPattern getAccessPattern() const;

  void setPartitionInfo(Value offset, Value size) {
    partitionOffset = offset;
    partitionSize = size;
  }

  std::pair<Value, Value> getPartitionInfo() const {
    return {partitionOffset, partitionSize};
  }

  const std::optional<std::pair<Value, Value>> &getOriginalBounds() const {
    return originalBounds_;
  }

  /// Check if this acquire uses worker-indexed pattern: offsets[%workerId] with
  /// sizes[1]. This pattern is inherently disjoint across workers.
  bool isWorkerIndexedAccess() const;

  /// Check if this acquire has disjoint partition info with another acquire.
  /// Uses offset_hints/size_hints from ForLowering to determine disjointness.
  bool hasDisjointPartitionWith(const DbAcquireNode *other) const;

private:
  DbAcquireOp dbAcquireOp;
  DbReleaseOp dbReleaseOp{nullptr};
  Operation *op = nullptr;
  NodeBase *parent = nullptr;
  DbAllocNode *rootAlloc = nullptr;
  DbAnalysis *analysis = nullptr;
  std::string hierId;
  Operation *edtUserOp = nullptr;
  Value useInEdt;
  Value partitionOffset;
  Value partitionSize;

  /// Cached adjusted chunk info
  std::optional<std::pair<Value, Value>> computedChunkInfo;
  std::optional<std::pair<Value, Value>> originalBounds_;
  std::optional<StencilBounds> stencilBounds_;
  mutable std::optional<AccessPattern> accessPattern_;

public:
  uint64_t inCount = 0, outCount = 0;
  uint64_t beginIndex = 0, endIndex = 0;
  unsigned long long estimatedBytes = 0;

  /// Nested acquire storage
  unsigned nextChildId = 1;
  SmallVector<std::unique_ptr<DbAcquireNode>, 4> childAcquires;
  DenseMap<DbAcquireOp, DbAcquireNode *> childMap;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
