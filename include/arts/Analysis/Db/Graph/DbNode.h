#ifndef ARTS_ANALYSIS_DB_GRAPH_DBNODE_H
#define ARTS_ANALYSIS_DB_GRAPH_DBNODE_H

#include "arts/Analysis/Db/DbInfo.h"
#include "arts/ArtsDialect.h"
#include "mlir/Support/LLVM.h"
#include <cassert>
#include <functional>
#include <memory>

namespace mlir {
namespace arts {

class DbAnalysis;
class DbAllocNode;
class DbDepNode;
class DbDepEdge;
class DbAllocEdge;

/// DbAllocNode
class DbAllocNode : public DbInfo {
public:
  DbAllocNode(DbAllocOp createOp, mlir::arts::DbAnalysis *analysis);

  void print(raw_ostream &os) const override;

  static bool classof(const DbInfo *info) {
    return info->isAlloc();
  }

  /// Get or create an dep node for an dep operation
  DbDepNode *getOrCreateDepNode(DbDepOp depOp);
  DbDepNode *findDepNode(DbDepOp depOp) const;
  void forEachDepNode(const std::function<void(DbDepNode *)> &fn) const;

  /// Edge management
  void addInAllocEdge(DbAllocEdge *edge);
  void addOutAllocEdge(DbAllocEdge *edge);
  const DenseSet<DbAllocEdge *> &getInAllocEdges() const;
  const DenseSet<DbAllocEdge *> &getOutAllocEdges() const;

private:
  DbAllocOp dbAllocOp;
  SmallVector<std::unique_ptr<DbDepNode>> depNodes;
  DenseMap<DbDepOp, DbDepNode *> depNodeMap;
  unsigned nextChildId = 1;
  DenseSet<DbAllocEdge *> inAllocEdges;
  DenseSet<DbAllocEdge *> outAllocEdges;
};

/// DbDepNode
class DbDepNode : public DbInfo {
public:
  DbDepNode(DbDepOp depOp, bool isAllocFlag, DbAllocNode *parent,
               DbAnalysis *analysis);

  void print(llvm::raw_ostream &os) const override;

  static bool classof(const DbInfo *info) {
    return info->isDep();
  }

  /// Edge management
  void addInDepEdge(DbDepEdge *edge);
  void addOutDepEdge(DbDepEdge *edge);
  const DenseSet<DbDepEdge *> &getInDepEdges() const;
  const DenseSet<DbDepEdge *> &getOutDepEdges() const;

private:
  DbDepOp dbDepOp;
  DenseSet<DbDepEdge *> inDepEdges;
  DenseSet<DbDepEdge *> outDepEdges;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_GRAPH_DBNODE_H