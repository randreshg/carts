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
class DbAccessNode;
class DbDepEdge;
class DbAllocEdge;

/// DbAllocNode
class DbAllocNode : public DbInfo {
public:
  DbAllocNode(DbCreateOp createOp, mlir::arts::DbAnalysis *analysis);

  void print(raw_ostream &os) const override;

  static bool classof(const DbInfo *info) {
    return info->isAlloc();
  }

  /// Get or create an access node for an access operation
  DbAccessNode *getOrCreateAccessNode(DbAccessOp accessOp);
  DbAccessNode *findAccessNode(DbAccessOp accessOp) const;
  void forEachAccessNode(const std::function<void(DbAccessNode *)> &fn) const;

  /// Edge management
  void addInAllocEdge(DbAllocEdge *edge);
  void addOutAllocEdge(DbAllocEdge *edge);
  const DenseSet<DbAllocEdge *> &getInAllocEdges() const;
  const DenseSet<DbAllocEdge *> &getOutAllocEdges() const;

private:
  DbCreateOp dbCreateOp;
  SmallVector<std::unique_ptr<DbAccessNode>> accessNodes;
  DenseMap<DbAccessOp, DbAccessNode *> accessNodeMap;
  unsigned nextChildId = 1;
  DenseSet<DbAllocEdge *> inAllocEdges;
  DenseSet<DbAllocEdge *> outAllocEdges;
};

/// DbAccessNode
class DbAccessNode : public DbInfo {
public:
  DbAccessNode(DbAccessOp accessOp, bool isAllocFlag, DbAllocNode *parent,
               DbAnalysis *analysis);

  void print(llvm::raw_ostream &os) const override;

  static bool classof(const DbInfo *info) {
    return info->isAccess();
  }

  /// Edge management
  void addInDepEdge(DbDepEdge *edge);
  void addOutDepEdge(DbDepEdge *edge);
  const DenseSet<DbDepEdge *> &getInDepEdges() const;
  const DenseSet<DbDepEdge *> &getOutDepEdges() const;

private:
  DbAccessOp dbAccessOp;
  DenseSet<DbDepEdge *> inDepEdges;
  DenseSet<DbDepEdge *> outDepEdges;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_GRAPH_DBNODE_H