///==========================================================================///
/// Db/DbNode.h - Db-specific nodes derived from NodeBase
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H

#include "arts/Analysis/Graphs/Base/NodeBase.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <memory>
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
  NodeKind getKind() const override { return NodeKind::DbAlloc; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAlloc;
  }

  /// Analysis metadata
  uint64_t inCount = 0;
  uint64_t outCount = 0;
  uint64_t beginIndex = 0;
  uint64_t endIndex = 0;
  bool isLongLived = false;
  uint64_t maxLoopDepth = 0;
  uint64_t criticalSpan = 0;
  uint64_t criticalPath = 0;
  unsigned long long totalAccessBytes = 0;
  uint64_t numReleases = 0;
  bool maybeEscaping = false;
  uint64_t numAcquires = 0;
  SmallVector<DbAllocOp, 8> reuseMatches;

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

  NodeKind getKind() const override { return NodeKind::DbAcquire; }
  static bool classof(const NodeBase *N) {
    return N->getKind() == NodeKind::DbAcquire;
  }

  EdtOp getEdtUser() const { return edtUser; }
  Value getUseInEdt() const { return useInEdt; }
  void getMemoryAccesses(SmallVector<Operation *> &memAccesses,
                         bool load = true, bool store = true) {
    memAccesses.clear();
    memAccesses.reserve((load ? loads.size() : 0) +
                        (store ? stores.size() : 0));
    if (load)
      memAccesses.append(loads.begin(), loads.end());
    if (store)
      memAccesses.append(stores.begin(), stores.end());
  }
  SmallVector<Operation *, 16> &getLoads() { return loads; }
  SmallVector<Operation *, 16> &getStores() { return stores; }
  SmallVector<Operation *, 16> &getReferences() { return references; }
  DbAcquireOp getDbAcquireOp() const { return dbAcquireOp; }
  DbReleaseOp getDbReleaseOp() const { return dbReleaseOp; }

  SmallVector<Value, 4> computeInvariantIndices();

  DbAcquireNode *getOrCreateAcquireNode(DbAcquireOp op);
  void forEachChildNode(const std::function<void(NodeBase *)> &fn) const;

private:
  DbAcquireOp dbAcquireOp;
  DbReleaseOp dbReleaseOp;
  Operation *op;
  NodeBase *parent;
  DbAllocNode *rootAlloc;
  DbAnalysis *analysis;
  std::string hierId;
  EdtOp edtUser;
  Value useInEdt;
  /// Memory accesses (loads/stores) inside the EDT
  SmallVector<Operation *, 16> loads, stores, references;

public:
  uint64_t inCount = 0;
  uint64_t outCount = 0;
  uint64_t beginIndex = 0;
  uint64_t endIndex = 0;
  unsigned long long estimatedBytes = 0;

  /// Helper functions
  void collectAccesses(Value db);

  /// Nested acquire storage
  unsigned nextChildId = 1;
  SmallVector<std::unique_ptr<DbAcquireNode>, 4> childAcquires;
  DenseMap<DbAcquireOp, DbAcquireNode *> childMap;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBNODE_H
