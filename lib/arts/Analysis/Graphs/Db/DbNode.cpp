///==========================================================================
/// DbNode.cpp - Implementation of Db nodes
///==========================================================================

#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinTypes.h"
#include <algorithm>
#include <limits>

using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// DbAllocNode
//===----------------------------------------------------------------------===//
DbAllocNode::DbAllocNode(DbAllocOp op, DbAnalysis *analysis)
    : dbAllocOp(op), op(op.getOperation()), analysis(analysis) {
  /// TODO: Estimation of bytes is not correct - we should use the dballoc
  /// payload size Pre-compute cheap, immutable alloc facts once.
  info.isAlloc = true;
  info.estimatedBytes = 0;
  info.staticBytes = 0;
  if (auto memTy = dyn_cast<MemRefType>(op.getPtr().getType())) {
    if (memTy.hasStaticShape()) {
      uint64_t bytes = 0;
      if (auto intTy = dyn_cast<IntegerType>(memTy.getElementType()))
        bytes = (uint64_t)(intTy.getWidth() / 8u);
      else if (auto fTy = dyn_cast<FloatType>(memTy.getElementType()))
        bytes = (uint64_t)(fTy.getWidth() / 8u);
      else
        bytes = 0;
      for (int64_t d : memTy.getShape()) {
        uint64_t mul = (uint64_t)std::max<int64_t>(d, 1);
        if (bytes > std::numeric_limits<uint64_t>::max() / mul) {
          bytes = std::numeric_limits<uint64_t>::max();
          break;
        }
        bytes *= mul;
      }
      info.estimatedBytes = bytes;
    }
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode (" << getHierId() << ")\n";
}

void DbAllocNode::forEachChildNode(
    const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : acquireNodes)
    fn(n.get());
  for (const auto &n : releaseNodes)
    fn(n.get());
}

//===----------------------------------------------------------------------===//
// DbAcquireNode
//===----------------------------------------------------------------------===//
DbAcquireNode *DbAllocNode::getOrCreateAcquireNode(DbAcquireOp op) {
  auto it = acquireMap.find(op);
  if (it != acquireMap.end())
    return it->second;
  auto newNode = std::make_unique<DbAcquireNode>(op, this, analysis);
  std::string childId =
      (getHierId() + "." + std::to_string(nextChildId++)).str();
  newNode->setHierId(childId);
  DbAcquireNode *ptr = newNode.get();
  acquireNodes.push_back(std::move(newNode));
  acquireMap[op] = ptr;
  return ptr;
}

DbAcquireNode::DbAcquireNode(DbAcquireOp op, DbAllocNode *parent,
                             DbAnalysis *analysis)
    : dbAcquireOp(op), op(op.getOperation()), parent(parent),
      analysis(analysis) {
  info.sizes.assign(op.getSizes().begin(), op.getSizes().end());
  info.offsets.assign(op.getOffsets().begin(), op.getOffsets().end());
  info.pinned.assign(op.getIndices().begin(), op.getIndices().end());
  info.constSizes.reserve(info.sizes.size());
  info.constOffsets.reserve(info.offsets.size());
  bool hasUnknown = false;
  for (Value v : info.offsets) {
    if (auto c = v.getDefiningOp<mlir::arith::ConstantIndexOp>())
      info.constOffsets.push_back(c.value());
    else {
      info.constOffsets.push_back(INT64_MIN);
      hasUnknown = true;
    }
  }
  uint64_t totalElems = 1;
  for (Value v : info.sizes) {
    if (auto c = v.getDefiningOp<mlir::arith::ConstantIndexOp>()) {
      info.constSizes.push_back(c.value());
      totalElems *= (uint64_t)std::max<int64_t>(c.value(), 1);
    } else {
      info.constSizes.push_back(INT64_MIN);
      hasUnknown = true;
    }
  }
  if (!hasUnknown) {
    /// TODO: This is not correct - we should use the dballoc payload size
    /// Try element type size from result memref
    if (auto memTy = dyn_cast<MemRefType>(op.getPtr().getType())) {
      if (auto intTy = dyn_cast<IntegerType>(memTy.getElementType()))
        info.estimatedBytes = (intTy.getWidth() / 8u) * totalElems;
      else if (auto fTy = dyn_cast<FloatType>(memTy.getElementType()))
        info.estimatedBytes = (fTy.getWidth() / 8u) * totalElems;
    }
  }
  /// Count in/out modes
  auto mode = const_cast<DbAcquireOp &>(op).getMode();
  if (mode == ArtsMode::in) {
    info.inCount = 1;
    info.outCount = 0;
  } else if (mode == ArtsMode::out) {
    info.inCount = 0;
    info.outCount = 1;
  } else {
    info.inCount = 1;
    info.outCount = 1;
  }
}

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")\n";
}

//===----------------------------------------------------------------------===//
// DbReleaseNode
//===----------------------------------------------------------------------===//
DbReleaseNode *DbAllocNode::getOrCreateReleaseNode(DbReleaseOp op) {
  auto it = releaseMap.find(op);
  if (it != releaseMap.end())
    return it->second;
  auto newNode = std::make_unique<DbReleaseNode>(op, this, analysis);
  std::string childId =
      (getHierId() + "." + std::to_string(nextChildId++)).str();
  newNode->setHierId(childId);
  DbReleaseNode *ptr = newNode.get();
  releaseNodes.push_back(std::move(newNode));
  releaseMap[op] = ptr;
  return ptr;
}

DbReleaseNode::DbReleaseNode(DbReleaseOp op, DbAllocNode *parent,
                             DbAnalysis *analysis)
    : dbReleaseOp(op), op(op.getOperation()), parent(parent),
      analysis(analysis) {
  info.release = op;
}

void DbReleaseNode::print(llvm::raw_ostream &os) const {
  os << "DbReleaseNode (" << getHierId() << ")\n";
}