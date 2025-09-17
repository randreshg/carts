///==========================================================================
/// DbAllocNode.cpp - Implementation of DbAlloc node
///==========================================================================

#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinTypes.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// DbAllocNode
//===----------------------------------------------------------------------===//
DbAllocNode::DbAllocNode(DbAllocOp op, DbAnalysis *analysis)
    : dbAllocOp(op), op(op.getOperation()), analysis(analysis) {
  info.isAlloc = true;
  info.estimatedBytes = 0;
  info.staticBytes = 0;
  /// Prefer computing bytes from DbAllocOp payload sizes and element type
  /// when payload sizes are explicitly provided; otherwise, fall back.
  unsigned long long elemBytes =
      arts::getElementTypeByteSize(op.getElementType());
  bool usedPayload = false;
  if (!op.getPayloadSizes().empty()) {
    bool allConst = true;
    unsigned long long payloadElems = 1;
    for (Value v : op.getPayloadSizes()) {
      if (auto c = v.getDefiningOp<arith::ConstantIndexOp>()) {
        unsigned long long mul =
            (unsigned long long)std::max<int64_t>(c.value(), 1);
        if (payloadElems >
            std::numeric_limits<unsigned long long>::max() / mul) {
          payloadElems = std::numeric_limits<unsigned long long>::max();
          /// Still treat as const but saturated
          allConst = true;
          break;
        }
        payloadElems *= mul;
      } else {
        allConst = false;
        break;
      }
    }
    if (elemBytes > 0 && allConst) {
      unsigned long long bytes = 0;
      if (payloadElems > 0) {
        if (payloadElems >
            std::numeric_limits<unsigned long long>::max() / elemBytes)
          bytes = std::numeric_limits<unsigned long long>::max();
        else
          bytes = payloadElems * elemBytes;
      }
      info.staticBytes = bytes;
      info.estimatedBytes = bytes;
      usedPayload = true;
    }
  }

  if (!usedPayload) {
    /// Fallback: if pointer memref has fully static shape, approximate
    if (auto memTy = dyn_cast<MemRefType>(op.getPtr().getType())) {
      if (memTy.hasStaticShape()) {
        unsigned long long bytes = elemBytes;
        /// If element type from DbAlloc wasn't known, try ptr element type
        if (bytes == 0) {
          bytes = getElementTypeByteSize(memTy.getElementType());
        }
        for (int64_t d : memTy.getShape()) {
          unsigned long long mul = (unsigned long long)std::max<int64_t>(d, 1);
          if (bytes > std::numeric_limits<unsigned long long>::max() / mul) {
            bytes = std::numeric_limits<unsigned long long>::max();
            break;
          }
          bytes *= mul;
        }
        info.estimatedBytes = bytes;
        info.staticBytes = bytes;
      }
    }
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode (" << getHierId() << ")";
  os << " staticBytes=" << info.staticBytes;
  os << "\n";
}

void DbAllocNode::forEachChildNode(
    const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : acquireNodes)
    fn(n.get());
  for (const auto &n : releaseNodes)
    fn(n.get());
}

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
