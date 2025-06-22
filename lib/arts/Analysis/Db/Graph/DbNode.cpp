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

static std::string toString(DbInfo::DepType type) {
  switch (type) {
  case DbInfo::DepType::Read:
    return "Read";
  case DbInfo::DepType::Write:
    return "Write";
  case DbInfo::DepType::ReadWrite:
    return "ReadWrite";
  case DbInfo::DepType::Unknown:
    return "Unknown";
  }
  return "Unknown";
}

///===----------------------------------------------------------------------===///
/// DbAllocNode Implementation
///===----------------------------------------------------------------------===///

DbAllocNode::DbAllocNode(DbAllocOp createOp, DbAnalysis *analysis)
    : DbInfo(createOp.getOperation(), true, analysis), dbAllocOp(createOp) {}

DbDepNode *DbAllocNode::getOrCreateDepNode(DbDepOp depOp) {
  auto it = depNodeMap.find(depOp);
  if (it != depNodeMap.end())
    return it->second;

  // Create new dep node
  unsigned childId = nextChildId++;
  auto depNode =
      std::make_unique<DbDepNode>(depOp, false, this, analysis);
  DbDepNode *ptr = depNode.get();
  depNodes.push_back(std::move(depNode));
  depNodeMap[depOp] = ptr;
  return ptr;
}

DbDepNode *DbAllocNode::findDepNode(DbDepOp depOp) const {
  auto it = depNodeMap.find(depOp);
  return (it != depNodeMap.end()) ? it->second : nullptr;
}

void DbAllocNode::forEachDepNode(
    const std::function<void(DbDepNode *)> &fn) const {
  for (const auto &node : depNodes) {
    fn(node.get());
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode " << id << " (" << getHierId() << ")\n";
  os << "  Dep type: " << ::toString(depType) << "\n";
  os << "  Dep nodes: " << depNodes.size() << "\n";
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
/// DbDepNode Implementation
///===----------------------------------------------------------------------===///

DbDepNode::DbDepNode(DbDepOp depOp, bool isAllocFlag,
                           DbAllocNode *parent, DbAnalysis *analysis)
    : DbInfo(depOp.getOperation(), isAllocFlag, analysis),
      dbDepOp(depOp) {}

void DbDepNode::print(llvm::raw_ostream &os) const {
  os << "DbDepNode " << id << " (" << getHierId() << ")\n";
  os << "  Dep type: " << ::toString(depType) << "\n";
  os << "  In dep edges: " << inDepEdges.size() << "\n";
  os << "  Out dep edges: " << outDepEdges.size() << "\n";
}

void DbDepNode::addInDepEdge(DbDepEdge *edge) { inDepEdges.insert(edge); }

void DbDepNode::addOutDepEdge(DbDepEdge *edge) { outDepEdges.insert(edge); }

const DenseSet<DbDepEdge *> &DbDepNode::getInDepEdges() const {
  return inDepEdges;
}

const DenseSet<DbDepEdge *> &DbDepNode::getOutDepEdges() const {
  return outDepEdges;
}