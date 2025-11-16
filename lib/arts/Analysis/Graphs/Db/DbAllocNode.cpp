///==========================================================================///
/// DbAllocNode.cpp - Implementation of DbAlloc node
///==========================================================================///

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinTypes.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_alloc_node);

///===----------------------------------------------------------------------===///
// DbAllocNode
///===----------------------------------------------------------------------===///
DbAllocNode::DbAllocNode(DbAllocOp op, DbAnalysis *analysis)
    : MemrefMetadata(op.getOperation()), dbAllocOp(op), dbFreeOp(nullptr),
      op(op.getOperation()), analysis(analysis) {
  /// Verify operation is valid
  Operation *opPtr = op.getOperation();
  if (!opPtr) {
    ARTS_ERROR("Cannot create DbAllocNode: operation pointer is null");
    return;
  }

  /// Import metadata from operation attributes, falling back to manager
  bool hasMetadata = importFromOp();
  if (!hasMetadata && analysis) {
    ArtsMetadataManager &metadataManager =
        analysis->getAnalysisManager().getMetadataManager();
    if (metadataManager.ensureMemrefMetadata(op.getOperation()))
      importFromOp();
  }

  if (analysis) {
    if (Value address = dbAllocOp.getAddress()) {
      auto &stringAnalysis = analysis->getAnalysisManager().getStringAnalysis();
      isStringBacked = stringAnalysis.isStringMemRef(address);
    }
  }

  /// Find the corresponding DbFreeOp for this allocation
  Value dbVal = dbAllocOp.getPtr();
  for (Operation *user : dbVal.getUsers()) {
    if (auto freeOp = dyn_cast<DbFreeOp>(user)) {
      dbFreeOp = freeOp;
      break;
    }
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode (" << getHierId() << ")";
  os << "\n";
}

void DbAllocNode::forEachChildNode(
    const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : acquireNodes)
    fn(n.get());
}

DbAcquireNode *DbAllocNode::getOrCreateAcquireNode(DbAcquireOp op) {
  auto it = acquireMap.find(op);
  if (it != acquireMap.end())
    return it->second;
  std::string childId =
      (getHierId() + "." + std::to_string(nextChildId++)).str();
  auto newNode =
      std::make_unique<DbAcquireNode>(op, this, this, analysis, childId);
  DbAcquireNode *ptr = newNode.get();
  acquireNodes.push_back(std::move(newNode));
  acquireMap[op] = ptr;
  return ptr;
}
