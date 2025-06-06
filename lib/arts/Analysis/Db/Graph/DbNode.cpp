///==========================================================================
/// File: DbNode.cpp
///
/// Simplified implementation of db graph nodes.
///==========================================================================

#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-node"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

/// Initialize the static ID counter from DbInfo
unsigned DbInfo::nextGlobalId = 0;

///===----------------------------------------------------------------------===///
/// AccessType Implementation
///===----------------------------------------------------------------------===///

std::string toString(AccessType type) {
  switch (type) {
  case AccessType::Unknown:
    return "Unknown";
  case AccessType::Read:
    return "Read";
  case AccessType::Write:
    return "Write";
  case AccessType::ReadWrite:
    return "ReadWrite";
  }
  return "Unknown";
}

///===----------------------------------------------------------------------===///
/// DbAllocNode Implementation
///===----------------------------------------------------------------------===///

DbAllocNode::DbAllocNode(DbCreateOp createOp, DbAnalysis *analysis)
    : DbInfo(createOp.getOperation(), true, analysis), dbCreateOp(createOp) {
}

DbAccessNode *DbAllocNode::getOrCreateAccessNode(DbAccessOp accessOp) {
  if (auto *existingNode = findAccessNode(accessOp))
    return existingNode;

  /// Create a new access node
  auto newNode =
      std::make_unique<DbAccessNode>(accessOp, false, this, analysis);
  DbAccessNode *nodePtr = newNode.get();

  /// Set hierarchical ID for the new access node using the parent's ID and its
  /// own child counter (e.g., "A1.1", "A1.2").
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

Value DbAllocNode::getPtr() { return dbCreateOp.getPtr(); }

SmallVector<Value> DbAllocNode::getIndices() {
  SmallVector<Value> result;
  auto operands = dbCreateOp.getOperands();
  if (operands.size() > 1) {
    for (auto val : operands.drop_front(1)) {
      result.push_back(val);
    }
  }
  return result;
}

void DbAccessNode::analyze() {
  LLVM_DEBUG(dbgs() << "Analyzing access node " << getHierId() << "\n");
  collectUses();
}

void DbAccessNode::collectUses() {
  accessType = AccessType::Unknown;

  if (!op)
    return;

  for (auto *user : op->getResult(0).getUsers()) {
    if (isa<memref::LoadOp>(user)) {
      accessType = (accessType == AccessType::Write) ? AccessType::ReadWrite
                                                     : AccessType::Read;
    } else if (isa<memref::StoreOp>(user)) {
      accessType = (accessType == AccessType::Read) ? AccessType::ReadWrite
                                                    : AccessType::Write;
    }
  }

  if (accessType == AccessType::Unknown) {
    accessType = AccessType::Read;
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode " << id << " (" << getHierId() << ")\n";
}

void DbAccessNode::print(llvm::raw_ostream &os) const {
  os << "DbAccessNode " << id << " (" << getHierId() << ")\n";
  os << "  Access type: " << toString(accessType) << "\n";
}