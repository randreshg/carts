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
  DbAllocInfo &getInfoRef() { return info; }
  const DbAllocInfo &getInfoRef() const { return info; }

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
  DbAcquireInfo &getInfoRef() { return info; }
  const DbAcquireInfo &getInfoRef() const { return info; }

  NodeKind getKind() const override { return NodeKind::DbAcquire; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAcquire;
  }

private:
  DbAcquireOp dbAcquireOp;
  Operation *op;
  DbAllocNode *parent;
  DbAnalysis *analysis;
  std::string hierId;
  DbAcquireInfo info;
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
  DbReleaseInfo &getInfoRef() { return info; }
  const DbReleaseInfo &getInfoRef() const { return info; }

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
