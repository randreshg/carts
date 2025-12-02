///==========================================================================///
/// DbAllocNode.cpp - Implementation of DbAlloc node
///==========================================================================///

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
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
  assert(op && "DbAllocOp is required to create DbAllocNode");
  assert(op.getOperation() && "Operation is required to create DbAllocNode");

  /// Verify operation is valid
  Operation *opPtr = op.getOperation();
  if (!opPtr) {
    ARTS_ERROR("Cannot create DbAllocNode: operation pointer is null");
    return;
  }
  analysis->getAnalysisManager()
      .getMetadataManager()
      .getIdRegistry()
      .getOrCreate(opPtr);

  /// Import metadata from operation attributes, falling back to manager
  bool hasMetadata = importFromOp();
  if (!hasMetadata) {
    ArtsMetadataManager &metadataManager =
        analysis->getAnalysisManager().getMetadataManager();
    if (metadataManager.ensureMemrefMetadata(op.getOperation()))
      importFromOp();
  }

  if (Value address = dbAllocOp.getAddress()) {
    auto &stringAnalysis = analysis->getAnalysisManager().getStringAnalysis();
    isStringBacked = stringAnalysis.isStringMemRef(address);
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

bool DbAllocNode::isParallelFriendly() const {
  if (isStringDatablock())
    return false;

  if (!accessedInParallelLoop || !accessedInParallelLoop.value_or(false))
    return false;

  return true;
}

bool DbAllocNode::shouldSliceAlloc() const {
  bool canSlice = false;
  if (accessedInParallelLoop && *accessedInParallelLoop) {
    if (!hasLoopCarriedDeps || !*hasLoopCarriedDeps)
      canSlice = true;
  }

  if (hasUniformAccess && *hasUniformAccess) {
    if (dominantAccessPattern) {
      auto pattern = *dominantAccessPattern;
      if (pattern == AccessPatternType::Sequential ||
          pattern == AccessPatternType::Strided)
        canSlice = true;
    }
  }

  return canSlice;
}

bool DbAllocNode::isFineGrained() const {
  if (!dbAllocOp)
    return false;

  DbAllocOp allocOp = const_cast<DbAllocOp &>(dbAllocOp);
  ValueRange elementSizes = allocOp.getElementSizes();
  if (elementSizes.empty())
    return false;

  /// Fine-grained: ALL elementSizes are constant 1
  /// This means each datablock holds exactly one element
  return llvm::all_of(elementSizes, [](Value v) {
    int64_t cst;
    return arts::getConstantIndex(v, cst) && cst == 1;
  });
}

///===----------------------------------------------------------------------===///
// Twin-diff overlap analysis methods
///===----------------------------------------------------------------------===///

bool DbAllocNode::hasSingleWriter() const {
  int writeCount = 0;

  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      continue;

    ArtsMode mode = acqOp.getMode();
    if (mode == ArtsMode::out || mode == ArtsMode::inout)
      writeCount++;

    /// Early exit - more than one writer
    if (writeCount > 1)
      return false;
  }

  if (writeCount <= 1) {
    ARTS_DEBUG("Single writer detected (" << writeCount
                                          << " writers) → no overlap possible");
    return true;
  }

  return false;
}

bool DbAllocNode::allAcquiresWorkerIndexed() const {
  if (acquireNodes.empty())
    return true;

  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    if (!acqNode->isWorkerIndexedAccess())
      return false;
  }

  ARTS_DEBUG("All acquires use worker-indexed pattern -> disjoint");
  return true;
}

bool DbAllocNode::canProveNonOverlapping() const {
  /// No acquires = trivially non-overlapping
  if (acquireNodes.empty())
    return true;

  /// Single acquire = non-overlapping with itself
  if (acquireNodes.size() == 1)
    return true;

  ARTS_DEBUG("Analyzing overlap for " << acquireNodes.size() << " acquires");

  /// Method 1: Single writer = no overlap possible
  /// If only one EDT writes, there's no possibility of write conflicts
  if (hasSingleWriter())
    return true;

  /// Method 2: All worker-indexed = disjoint
  /// If all acquires use the pattern array[workerId] with size=1,
  /// they access disjoint slots by construction
  if (allAcquiresWorkerIndexed())
    return true;

  /// Get alias analysis from DbAnalysis
  DbAliasAnalysis *aliasAnalysis = analysis->getAliasAnalysis();

  /// Check all acquire pairs using enhanced analysis
  for (size_t i = 0; i < acquireNodes.size(); ++i) {
    for (size_t j = i + 1; j < acquireNodes.size(); ++j) {
      const DbAcquireNode *acqA = acquireNodes[i].get();
      const DbAcquireNode *acqB = acquireNodes[j].get();

      if (!acqA || !acqB)
        continue;

      /// Method 3: Use DbAliasAnalysis overlap estimation
      auto overlap = aliasAnalysis->estimateOverlap(
          const_cast<DbAcquireNode *>(acqA), const_cast<DbAcquireNode *>(acqB));
      if (overlap == DbAliasAnalysis::OverlapKind::Disjoint) {
        /// Proven disjoint by alias analysis
        ARTS_DEBUG("Pair (" << i << "," << j << "): Disjoint (aliasAnalysis)");
        continue;
      }

      /// Method 4: Check offset_hints/size_hints from ForLowering
      if (acqA->hasDisjointPartitionWith(acqB)) {
        /// Proven disjoint by loop partitioning
        ARTS_DEBUG("Pair (" << i << "," << j
                            << "): Disjoint (partition hints)");
        continue;
      }

      /// Could not prove disjoint - potential overlap
      ARTS_DEBUG("Pair (" << i << "," << j << "): POTENTIAL OVERLAP");
      return false;
    }
  }

  ARTS_DEBUG("All acquire pairs proven disjoint");
  return true; /// All pairs proven disjoint
}
