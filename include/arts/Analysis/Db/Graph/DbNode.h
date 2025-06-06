#ifndef ARTS_ANALYSIS_DB_GRAPH_DBNODE_H
#define ARTS_ANALYSIS_DB_GRAPH_DBNODE_H

#include "arts/Analysis/Db/DbInfo.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include <cassert>
#include <functional>
#include <memory>
#include <string>

namespace mlir {
namespace arts {

class DbAnalysis;
class DbAllocNode;
class DbAccessNode;

/// DbAllocNode
class DbAllocNode : public DbInfo {
public:
  DbAllocNode(DbCreateOp createOp, mlir::arts::DbAnalysis *analysis);
  /// Get or create an access node for an access operation
  DbAccessNode *getOrCreateAccessNode(DbAccessOp accessOp);
  DbAccessNode *findAccessNode(DbAccessOp accessOp) const;
  void forEachAccessNode(const std::function<void(DbAccessNode *)> &fn) const;

  /// Accessors
  Value getPtr();
  SmallVector<Value> getIndices();
  SmallVector<Value> getSizes();

  /// DbInfo interface implementation
  void analyze() override;
  void collectUses() override;
  void print(llvm::raw_ostream &os) const override;

private:
  DbCreateOp dbCreateOp;
  SmallVector<std::unique_ptr<DbAccessNode>> accessNodes;
  DenseMap<DbAccessOp, DbAccessNode *> accessNodeMap;
  unsigned nextChildId = 1;
};

/// DbAccessNode
class DbAccessNode : public DbInfo {
public:
  DbAccessNode(DbAccessOp accessOp, bool isAllocFlag, DbAllocNode *parent,
               DbAnalysis *analysis)
      : DbInfo(accessOp.getOperation(), isAllocFlag, analysis) {
    // this->id = 0;
  }

  // DbInfo interface implementation
  void analyze() override;
  void collectUses() override;
  void print(llvm::raw_ostream &os) const override;

private:
  DbAccessOp dbAccessOp;
  AccessType accessType;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_GRAPH_DBNODE_H