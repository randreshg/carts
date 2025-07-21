//===----------------------------------------------------------------------===//
// Db/DbNode.h - Db-specific nodes derived from NodeBase
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H

#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/ArtsDialect.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

// Forward declarations
class DbAnalysis;
class DbAcquireNode;
class DbReleaseNode;

class DbAllocNode : public NodeBase {
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

  DbAcquireNode *getOrCreateAcquireNode(DbAcquireOp op);
  DbReleaseNode *getOrCreateReleaseNode(DbReleaseOp op);
  void forEachChildNode(const std::function<void(NodeBase *)> &fn) const;
  size_t getAcquireNodesSize() const { return acquireNodes.size(); }
  size_t getReleaseNodesSize() const { return releaseNodes.size(); }
  DbAllocOp getDbAllocOp() const { return dbAllocOp; }

private:
  DbAllocOp dbAllocOp;
  Operation *op;
  DbAnalysis *analysis;
  std::string hierId;
  unsigned nextChildId = 1;

  std::vector<std::unique_ptr<DbAcquireNode>> acquireNodes;
  std::vector<std::unique_ptr<DbReleaseNode>> releaseNodes;
  DenseMap<DbAcquireOp, DbAcquireNode *> acquireMap;
  DenseMap<DbReleaseOp, DbReleaseNode *> releaseMap;
};

class DbAcquireNode : public NodeBase {
public:
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

private:
  DbAcquireOp dbAcquireOp;
  Operation *op;
  DbAllocNode *parent;
  DbAnalysis *analysis;
  std::string hierId;
};

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

private:
  DbReleaseOp dbReleaseOp;
  Operation *op;
  DbAllocNode *parent;
  DbAnalysis *analysis;
  std::string hierId;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H