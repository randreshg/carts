///==========================================================================
/// Db/DbNode.h - Db-specific nodes derived from NodeBase
///==========================================================================

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H

#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/ArtsDialect.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

/// Forward declarations
class DbAnalysis;
class DbAcquireNode;
class DbReleaseNode;

//===----------------------------------------------------------------------===//
// DbAllocNode
// It represents a `arts.db.alloc` operations
//===----------------------------------------------------------------------===//
class DbAllocNode : public NodeBase {
public:
  /// Construct an allocation node and pre-compute cheap, immutable facts
  /// (e.g., static byte size when shape is fully static).
  DbAllocNode(DbAllocOp op, DbAnalysis *analysis);

  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return op; }

  void addInEdge(EdgeBase *edge) override { inEdges.insert(edge); }
  void addOutEdge(EdgeBase *edge) override { outEdges.insert(edge); }
  const DenseSet<EdgeBase *> &getInEdges() const override { return inEdges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return outEdges; }

  DbAcquireNode *getOrCreateAcquireNode(DbAcquireOp op);
  DbReleaseNode *getOrCreateReleaseNode(DbReleaseOp op);
  void forEachChildNode(const std::function<void(NodeBase *)> &fn) const;
  size_t getAcquireNodesSize() const { return acquireNodes.size(); }
  size_t getReleaseNodesSize() const { return releaseNodes.size(); }
  DbAllocOp getDbAllocOp() const { return dbAllocOp; }
  DbAllocInfo &getInfo() { return info; }
  const DbAllocInfo &getInfo() const { return info; }

  NodeKind getKind() const override { return NodeKind::DbAlloc; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAlloc;
  }

private:
  DbAllocOp dbAllocOp;
  Operation *op;
  DbAnalysis *analysis;
  std::string hierId;
  unsigned nextChildId = 1;

  SmallVector<std::unique_ptr<DbAcquireNode>, 4> acquireNodes;
  SmallVector<std::unique_ptr<DbReleaseNode>, 4> releaseNodes;
  DenseMap<DbAcquireOp, DbAcquireNode *> acquireMap;
  DenseMap<DbReleaseOp, DbReleaseNode *> releaseMap;
  DbAllocInfo info;
};

//===----------------------------------------------------------------------===//
// DbAcquireNode
// It represents a `arts.db.acquire` operation.
//===----------------------------------------------------------------------===//
class DbAcquireNode : public NodeBase {
public:
  /// Construct an acquire node and pre-compute value/constant slice
  /// properties and per-acquire in/out mode counts.
  DbAcquireNode(DbAcquireOp op, DbAllocNode *parent, DbAnalysis *analysis);

  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return op; }

  void addInEdge(EdgeBase *edge) override { inEdges.insert(edge); }
  void addOutEdge(EdgeBase *edge) override { outEdges.insert(edge); }
  const DenseSet<EdgeBase *> &getInEdges() const override { return inEdges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return outEdges; }

  DbAllocNode *getParent() const { return parent; }
  DbAcquireInfo &getInfo() { return info; }
  const DbAcquireInfo &getInfo() const { return info; }

  NodeKind getKind() const override { return NodeKind::DbAcquire; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAcquire;
  }

  /// Access range for a single dimension. The effective accessed offset is
  /// computed as base + idx, where base is the acquire's offset for the
  /// dimension and idx is an index value used in loads/stores within the EDT.
  ///
  /// For conservative, non-mutating analysis, we return existing SSA Values
  /// for the base, the best-known lower index (minIndex) and upper index
  /// (maxIndex). When unknown, the respective Value is null. If the upper
  /// bound originates from scf.for, maxIndex corresponds to the loop upper
  /// bound (exclusive).
  struct OffsetRange {
    Value base;     ///< acquire offset[d]
    Value minIndex; ///< lower index bound Value (e.g., loop lower bound)
    Value maxIndex; ///< upper index bound Value (exclusive for scf.for)
  };

  /// Compute per-dimension accessed offset ranges for this acquire by scanning
  /// memref.load/store users inside the enclosing EDT. Returns, for each
  /// dimension, the acquire base offset and the best-known lower/upper index
  /// Values that were used (e.g., loop bounds or constant indices). No IR is
  /// created; if a bound cannot be proven, the corresponding Value is null.
  SmallVector<OffsetRange, 4> computeAccessedOffsetRanges();

  /// Compute a maximal prefix of invariant indices (one per leading dimension
  /// of the acquired memref) that are uniform across the EDT. These indices
  /// can be used to coarsen the acquire by pinning leading dimensions.
  /// The returned Values are SSA values already present in the IR; this API
  /// does not materialize new constants or perform replacements.
  SmallVector<Value, 4> computeInvariantIndices();

  EdtOp getEdtUser() const { return edtUser; }
  Value getUseInEdt() const { return useInEdt; }
  const SmallVector<Operation *, 16> &getMemoryAccesses() const {
    return memoryAccesses;
  }
  DbAcquireOp getDbAcquireOp() const { return dbAcquireOp; }

private:
  DbAcquireOp dbAcquireOp;
  Operation *op;
  DbAllocNode *parent;
  DbAnalysis *analysis;
  std::string hierId;
  DbAcquireInfo info;
  EdtOp edtUser;
  Value useInEdt;
  SmallVector<Operation *, 16> memoryAccesses;
};

//===----------------------------------------------------------------------===//
// DbReleaseNode
// It represents a `arts.db.release` operation.
//===----------------------------------------------------------------------===//

class DbReleaseNode : public NodeBase {
public:
  DbReleaseNode(DbReleaseOp op, DbAllocNode *parent, DbAnalysis *analysis);

  StringRef getHierId() const override { return hierId; }
  void setHierId(std::string id) { hierId = id; }
  void print(llvm::raw_ostream &os) const override;
  Operation *getOp() const override { return op; }

  void addInEdge(EdgeBase *edge) override { inEdges.insert(edge); }
  void addOutEdge(EdgeBase *edge) override { outEdges.insert(edge); }
  const DenseSet<EdgeBase *> &getInEdges() const override { return inEdges; }
  const DenseSet<EdgeBase *> &getOutEdges() const override { return outEdges; }

  DbAllocNode *getParent() const { return parent; }
  DbReleaseInfo &getInfo() { return info; }
  const DbReleaseInfo &getInfo() const { return info; }

  NodeKind getKind() const override { return NodeKind::DbRelease; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbRelease;
  }

private:
  DbReleaseOp dbReleaseOp;
  Operation *op;
  DbAllocNode *parent;
  DbAnalysis *analysis;
  std::string hierId;
  DbReleaseInfo info;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
