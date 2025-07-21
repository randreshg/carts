//===----------------------------------------------------------------------===//
// Db/DbNode.cpp - Implementation of Db nodes
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Graphs/Db/DbNode.h"

using namespace mlir::arts;

DbAllocNode::DbAllocNode(DbAllocOp op, DbAnalysis *analysis)
    : dbAllocOp(op), op(op.getOperation()), analysis(analysis) {}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode (" << getHierId() << ")\n";
}

DbAcquireNode *DbAllocNode::getOrCreateAcquireNode(DbAcquireOp op) {
  auto it = acquireMap.find(op);
  if (it != acquireMap.end()) return it->second;
  auto newNode = std::make_unique<DbAcquireNode>(op, this, analysis);
  std::string childId = (getHierId() + "." + std::to_string(nextChildId++)).str();
  newNode->setHierId(childId);
  DbAcquireNode *ptr = newNode.get();
  acquireNodes.push_back(std::move(newNode));
  acquireMap[op] = ptr;
  return ptr;
}

DbReleaseNode *DbAllocNode::getOrCreateReleaseNode(DbReleaseOp op) {
  auto it = releaseMap.find(op);
  if (it != releaseMap.end()) return it->second;
  auto newNode = std::make_unique<DbReleaseNode>(op, this, analysis);
  std::string childId = (getHierId() + "." + std::to_string(nextChildId++)).str();
  newNode->setHierId(childId);
  DbReleaseNode *ptr = newNode.get();
  releaseNodes.push_back(std::move(newNode));
  releaseMap[op] = ptr;
  return ptr;
}

void DbAllocNode::forEachChildNode(const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : acquireNodes) fn(n.get());
  for (const auto &n : releaseNodes) fn(n.get());
}

DbAcquireNode::DbAcquireNode(DbAcquireOp op, DbAllocNode *parent, DbAnalysis *analysis)
    : dbAcquireOp(op), op(op.getOperation()), parent(parent), analysis(analysis) {}

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")\n";
}

DbReleaseNode::DbReleaseNode(DbReleaseOp op, DbAllocNode *parent, DbAnalysis *analysis)
    : dbReleaseOp(op), op(op.getOperation()), parent(parent), analysis(analysis) {}

void DbReleaseNode::print(llvm::raw_ostream &os) const {
  os << "DbReleaseNode (" << getHierId() << ")\n";
}