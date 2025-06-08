///==========================================================================
/// File: DbNode.cpp
///
/// Simplified implementation of db graph nodes.
///==========================================================================

#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Db/Graph/DbEdge.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-graph"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// DbInfo Implementation
///===----------------------------------------------------------------------===///

static std::string toString(DbInfo::AccessType type) {
  switch (type) {
  case DbInfo::AccessType::Read:
    return "Read";
  case DbInfo::AccessType::Write:
    return "Write";
  case DbInfo::AccessType::ReadWrite:
    return "ReadWrite";
  case DbInfo::AccessType::Unknown:
    return "Unknown";
  }
  return "Unknown";
}

///===----------------------------------------------------------------------===///
/// DbAllocNode Implementation
///===----------------------------------------------------------------------===///

DbAllocNode::DbAllocNode(DbCreateOp createOp, DbAnalysis *analysis)
    : DbInfo(createOp.getOperation(), true, analysis), dbCreateOp(createOp) {}

DbAccessNode *DbAllocNode::getOrCreateAccessNode(DbAccessOp accessOp) {
  if (auto *existingNode = findAccessNode(accessOp))
    return existingNode;

  /// Create a new access node
  auto newNode =
      std::make_unique<DbAccessNode>(accessOp, false, this, analysis);
  DbAccessNode *nodePtr = newNode.get();

  nodePtr->setHierId(getHierId() + "." + std::to_string(nextChildId++));

  accessNodes.push_back(std::move(newNode));
  accessNodeMap[accessOp] = nodePtr;
  return nodePtr;
}

DbAccessNode *DbAllocNode::findAccessNode(DbAccessOp accessOp) const {
  auto it = accessNodeMap.find(accessOp);
  return it != accessNodeMap.end() ? it->second : nullptr;
}

void DbAllocNode::forEachAccessNode(
    const std::function<void(DbAccessNode *)> &fn) const {
  for (const auto &node : accessNodes) {
    fn(node.get());
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode " << id << " (" << getHierId() << ")\n";
  os << "  Access type: " << ::toString(accessType) << "\n";
  os << "  Access nodes: " << accessNodes.size() << "\n";
  os << "  In alloc edges: " << inAllocEdges.size() << "\n";
  os << "  Out alloc edges: " << outAllocEdges.size() << "\n";
}

void DbAllocNode::addInAllocEdge(DbAllocEdge *edge) {
  inAllocEdges.insert(edge);
}

void DbAllocNode::addOutAllocEdge(DbAllocEdge *edge) {
  outAllocEdges.insert(edge);
}

const DenseSet<DbAllocEdge *> &DbAllocNode::getInAllocEdges() const {
  return inAllocEdges;
}

const DenseSet<DbAllocEdge *> &DbAllocNode::getOutAllocEdges() const {
  return outAllocEdges;
}

///===----------------------------------------------------------------------===///
/// DbAccessNode Implementation
///===----------------------------------------------------------------------===///

DbAccessNode::DbAccessNode(DbAccessOp accessOp, bool isAllocFlag,
                           DbAllocNode *parent, DbAnalysis *analysis)
    : DbInfo(accessOp.getOperation(), isAllocFlag, analysis),
      dbAccessOp(accessOp) {}

void DbAccessNode::print(llvm::raw_ostream &os) const {
  os << "DbAccessNode " << id << " (" << getHierId() << ")\n";
  os << "  Access type: " << ::toString(accessType) << "\n";
  os << "  In dep edges: " << inDepEdges.size() << "\n";
  os << "  Out dep edges: " << outDepEdges.size() << "\n";
}

void DbAccessNode::addInDepEdge(DbDepEdge *edge) { inDepEdges.insert(edge); }

void DbAccessNode::addOutDepEdge(DbDepEdge *edge) { outDepEdges.insert(edge); }

const DenseSet<DbDepEdge *> &DbAccessNode::getInDepEdges() const {
  return inDepEdges;
}

const DenseSet<DbDepEdge *> &DbAccessNode::getOutDepEdges() const {
  return outDepEdges;
}