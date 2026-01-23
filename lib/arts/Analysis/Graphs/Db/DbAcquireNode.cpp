///==========================================================================///
/// File: DbAcquireNode.cpp
///
/// Implementation of DbAcquire node
///==========================================================================///

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>

using namespace mlir;
using namespace mlir::arts;
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_acquire_node);

///===----------------------------------------------------------------------===///
/// DbAcquireNode
///===----------------------------------------------------------------------===///
DbAcquireNode::DbAcquireNode(DbAcquireOp op, NodeBase *parent,
                             DbAllocNode *rootAlloc, DbAnalysis *analysis,
                             std::string initialHierId)
    : dbAcquireOp(op), dbReleaseOp(nullptr), op(op.getOperation()),
      parent(parent), rootAlloc(rootAlloc), analysis(analysis) {
  if (!initialHierId.empty())
    hierId = std::move(initialHierId);

  /// Verify operation is valid
  Operation *opPtr = op.getOperation();
  if (!opPtr) {
    ARTS_ERROR("Cannot create DbAcquireNode: operation pointer is null");
    return;
  }

  /// Verify rootAlloc is valid
  if (!rootAlloc) {
    ARTS_ERROR("Cannot create DbAcquireNode: rootAlloc is null");
    return;
  }

  /// Estimate the footprint of this acquire from constant slice information.
  bool hasUnknown = false;
  unsigned long long totalElems = 1;
  for (Value v : dbAcquireOp.getSizes()) {
    int64_t cst = 0;
    if (!ValueUtils::getConstantIndex(v, cst)) {
      hasUnknown = true;
      break;
    }

    if (cst <= 0)
      continue;
    if (totalElems > std::numeric_limits<unsigned long long>::max() /
                         (unsigned long long)cst) {
      estimatedBytes = std::numeric_limits<unsigned long long>::max();
      hasUnknown = true;
      break;
    }
    totalElems *= (unsigned long long)cst;
  }

  if (!hasUnknown) {
    DbAllocOp rootAllocOp = rootAlloc->getDbAllocOp();
    if (Type elementType = rootAllocOp.getElementType()) {
      unsigned long long elemBytes = arts::getElementTypeByteSize(elementType);
      if (elemBytes != 0) {
        if (totalElems >
            std::numeric_limits<unsigned long long>::max() / elemBytes) {
          estimatedBytes = std::numeric_limits<unsigned long long>::max();
        } else {
          estimatedBytes = totalElems * elemBytes;
        }
      }
    }
  }

  /// Identify the EDT that consumes this acquire and the block argument used
  Value ptrResult = dbAcquireOp.getPtr();
  if (!ptrResult) {
    ARTS_ERROR("DbAcquireOp ptr result is null");
    return;
  }

  /// The ptr result should be passed directly to an EDT or another acquire
  auto users = ptrResult.getUsers();
  if (users.empty()) {
    ARTS_ERROR("DbAcquireOp ptr has no users");
    return;
  }

  Operation *singleUser = *users.begin();
  if (!singleUser) {
    ARTS_ERROR("DbAcquireOp ptr user is null");
    return;
  }

  /// Capture partition info from the operation.
  if (!dbAcquireOp.getIndices().empty())
    partitionOffset = dbAcquireOp.getIndices().front();
  else if (!dbAcquireOp.getOffsets().empty())
    partitionOffset = dbAcquireOp.getOffsets().front();
  if (!dbAcquireOp.getSizes().empty())
    partitionSize = dbAcquireOp.getSizes().front();

  if (isa<DbAcquireOp>(singleUser)) {
    /// Nested acquire; create child node lazily via getOrCreateAcquireNode
    /// The rest of this constructor computes info for this node; nested
    /// children will compute their own. For nested acquires, the EDT user
    /// is found when processing the child acquire node.
    return;
  }

  /// Use utility function to get EDT and block argument
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(dbAcquireOp);
  edtUserOp = edt.getOperation();
  useInEdt = blockArg;

  if (!edtUserOp || !useInEdt) {
    ARTS_ERROR("DbAcquireOp ptr result is not used by an EDT or another "
               "acquire.\n"
               "  Acquire op: "
               << dbAcquireOp
               << "\n"
                  "  Single user: "
               << *singleUser
               << "\n"
                  "  User type: "
               << singleUser->getName());
    ARTS_UNREACHABLE("Acquire ptr should be used by an EDT or another acquire");
  }

  /// Find the corresponding DbReleaseOp for this acquire
  for (Operation *user : useInEdt.getUsers()) {
    auto releaseOp = dyn_cast<DbReleaseOp>(user);
    if (!releaseOp)
      continue;
    if (releaseOp.getSource() == useInEdt) {
      dbReleaseOp = releaseOp;
      break;
    }
  }

  /// Get the in/out mode based on the memory accesses
  bool hasLoadAccesses = hasLoads();
  bool hasStoreAccesses = hasStores();
  if (hasLoadAccesses && !hasStoreAccesses) {
    inCount = 1;
    outCount = 0;
  } else if (hasStoreAccesses && !hasLoadAccesses) {
    inCount = 0;
    outCount = 1;
  } else {
    inCount = 1;
    outCount = 1;
  }
}

DbAcquireNode *DbAcquireNode::getOrCreateAcquireNode(DbAcquireOp op) {
  auto it = childMap.find(op);
  if (it != childMap.end())
    return it->second;
  std::string childId = getHierId().str() + "." + std::to_string(nextChildId++);
  auto newNode =
      std::make_unique<DbAcquireNode>(op, this, rootAlloc, analysis, childId);
  DbAcquireNode *ptr = newNode.get();
  childAcquires.push_back(std::move(newNode));
  childMap[op] = ptr;
  return ptr;
}

void DbAcquireNode::forEachChildNode(
    const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : childAcquires)
    fn(n.get());
}

///===----------------------------------------------------------------------===///
/// Stencil-Aware Access Bounds Analysis
///
/// For stencil patterns like A[i-1], A[i], A[i+1], we need to analyze the
/// actual memory access bounds rather than using iteration hints directly.
/// The index expression is typically: blockOffset + loopIV + constant
/// We extract the constant to determine the offset adjustment needed.
///===----------------------------------------------------------------------===///

struct AccessBoundsInfo {
  int64_t minOffset = 0;
  int64_t maxOffset = 0;
  bool isStencil = false;
  bool valid = false;
};

/// Analyze all memory accesses to compute actual bounds relative to block base.
/// For stencil patterns like A[i-1], A[i], A[i+1], this extracts the min/max
/// offsets. For uniform access like B[i+1], this extracts the constant
/// adjustment.
static AccessBoundsInfo analyzeAccessBounds(DbAcquireNode *node,
                                            Value blockOffset, Value loopIV);

/// Find the best loop node for bounds analysis.
/// Prefers loops whose IV the first dynamic index depends on, selecting deeper
/// loops when possible. Returns nullptr if no suitable loop is found.
static LoopNode *findBestLoopNode(ArrayRef<LoopNode *> loopNodes,
                                  Value firstDynIdx, Value partitionOffset,
                                  bool offsetIsZero) {
  LoopNode *bestNode = nullptr;
  int bestDepth = -1;
  bool foundPreferred = false;

  for (LoopNode *loopNode : loopNodes) {
    if (!loopNode->dependsOnInductionVar(firstDynIdx))
      continue;
    if (!offsetIsZero && !ValueUtils::dependsOn(firstDynIdx, partitionOffset))
      continue;

    Value loopIV = loopNode->getInductionVar();
    bool offsetDependsOnIV = !offsetIsZero && partitionOffset &&
                             ValueUtils::dependsOn(partitionOffset, loopIV);
    bool isPreferred = !offsetDependsOnIV;
    int depth = loopNode->getNestingDepth();

    if (isPreferred) {
      if (!foundPreferred || depth > bestDepth) {
        foundPreferred = true;
        bestDepth = depth;
        bestNode = loopNode;
      }
    } else if (!foundPreferred && depth > bestDepth) {
      bestDepth = depth;
      bestNode = loopNode;
    }
  }

  return bestNode;
}

/// Detect indirect (data-dependent) indices derived from memory loads.
static bool isIndirectIndex(Value idx, Value partitionOffset, int depth = 0) {
  if (!idx || depth > 8)
    return false;

  idx = ValueUtils::stripNumericCasts(idx);
  if (ValueUtils::isValueConstant(idx))
    return false;
  if (partitionOffset) {
    Value offsetStripped = ValueUtils::stripNumericCasts(partitionOffset);
    if (idx == offsetStripped)
      return false;
  }

  if (auto blockArg = idx.dyn_cast<BlockArgument>()) {
    if (partitionOffset) {
      Value offsetStripped = ValueUtils::stripNumericCasts(partitionOffset);
      if (offsetStripped == blockArg ||
          ValueUtils::dependsOn(offsetStripped, blockArg))
        return false;
    }

    Operation *parentOp = blockArg.getOwner()->getParentOp();
    if (auto forOp = dyn_cast<affine::AffineForOp>(parentOp)) {
      if (blockArg == forOp.getInductionVar())
        return false;
    }
    if (auto forOp = dyn_cast<scf::ForOp>(parentOp)) {
      if (blockArg == forOp.getInductionVar())
        return false;
    }
    if (auto parOp = dyn_cast<scf::ParallelOp>(parentOp)) {
      for (Value iv : parOp.getInductionVars()) {
        if (blockArg == iv)
          return false;
      }
    }
    return true;
  }

  Operation *defOp = idx.getDefiningOp();
  if (!defOp)
    return false;

  if (auto load = dyn_cast<memref::LoadOp>(defOp)) {
    bool isDbLoad =
        DatablockUtils::getUnderlyingDb(load.getMemref()) != nullptr;
    bool hasDynamicIndex = llvm::any_of(load.getIndices(), [](Value idx) {
      int64_t constVal = 0;
      return !ValueUtils::getConstantIndex(idx, constVal);
    });
    return isDbLoad || hasDynamicIndex;
  }
  if (auto load = dyn_cast<LLVM::LoadOp>(defOp))
    return DatablockUtils::getUnderlyingDb(load.getAddr()) != nullptr;

  if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
          arith::DivUIOp, arith::RemSIOp, arith::RemUIOp, arith::IndexCastOp,
          arith::IndexCastUIOp, arith::ExtSIOp, arith::ExtUIOp, arith::TruncIOp,
          arith::MinSIOp, arith::MinUIOp, arith::MaxSIOp, arith::MaxUIOp,
          arith::SelectOp, arith::CmpIOp, arith::CmpFOp, affine::AffineApplyOp>(
          defOp)) {
    for (Value operand : defOp->getOperands()) {
      if (isIndirectIndex(operand, partitionOffset, depth + 1))
        return true;
    }
    return false;
  }

  return true;
}

bool DbAcquireNode::hasValidEdtAndAccesses() {
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << dbAcquireOp << ": " << reason);
    return false;
  };

  if (!dbAcquireOp)
    return skip("invalid acquire operation");

  if (!edtUserOp) {
    if (!hasMemoryAccesses()) {
      ARTS_DEBUG("No EDT user and no memory accesses - full-range acquire");
      return true;
    }
    DbAllocNode *allocNode = getRootAlloc();
    if (allocNode) {
      DbAllocOp allocOp = allocNode->getDbAllocOp();
      if (allocOp && !allocOp.getElementSizes().empty()) {
        ARTS_DEBUG("No EDT user - full-range acquire with alloc sizes");
        return true;
      }
    }
    return skip("no EDT user and missing allocation sizes");
  }

  EdtOp edt = getEdtUser();
  if (!edt || !edt.getOperation())
    return skip("missing EDT");

  bool hasMemAccesses = hasMemoryAccesses();
  bool isTaskEdt = (edt.getType() == EdtType::task);
  bool isPassThrough = !isTaskEdt && !hasMemAccesses;

  if (!isTaskEdt && !isPassThrough)
    return skip("not a task EDT");

  if (isTaskEdt && !hasMemAccesses)
    return skip("no memory accesses");

  DbAllocNode *allocNode = getRootAlloc();
  if (!allocNode)
    return skip("missing allocation node");

  if (!allocNode->isParallelFriendly()) {
    /// Allow non-parallel-friendly accesses - heuristics will decide
    /// partitioning strategy
    ARTS_DEBUG("Memref not marked parallel-friendly - allowing for heuristic "
               "evaluation");
  }

  ArtsAnalysisManager &AM = analysis->getAnalysisManager();
  EdtAnalysis &edtAnalysis = AM.getEdtAnalysis();

  func::FuncOp func = edt->getParentOfType<func::FuncOp>();
  if (!func)
    return skip("unable to find parent function");

  EdtGraph &edtGraph = edtAnalysis.getOrCreateEdtGraph(func);
  EdtNode *edtNode = edtGraph.getEdtNode(edt);
  if (!edtNode)
    return skip("missing EDT node");

  if (isTaskEdt && !edtNode->hasParallelLoopMetadata()) {
    ARTS_DEBUG("Skipping parallel loop metadata check: treating as full-range");
    partitionOffset = Value();
    partitionSize = Value();
  }

  return true;
}

bool DbAcquireNode::computePartitionBounds() {
  DbAcquireOp mutableAcquire = DbAcquireOp(dbAcquireOp.getOperation());

  if (!mutableAcquire.getIndices().empty())
    partitionOffset = mutableAcquire.getIndices().front();
  else if (!mutableAcquire.getOffsets().empty())
    partitionOffset = mutableAcquire.getOffsets().front();
  if (!mutableAcquire.getSizes().empty())
    partitionSize = mutableAcquire.getSizes().front();

  if (!edtUserOp)
    return true;

  bool hasPartitionHint = !mutableAcquire.getIndices().empty() ||
                          !mutableAcquire.getOffsets().empty();
  if (partitionOffset && !canPartitionWithOffset(partitionOffset)) {
    if (hasPartitionHint) {
      ARTS_DEBUG("Partition offset not derived from access; "
                 "continuing with partition hint");
    } else {
      ARTS_DEBUG("Partition offset not derived from access; "
                 "allowing for heuristic evaluation");
    }
  }

  if (!partitionOffset || !partitionSize)
    return true;

  originalBounds = std::make_pair(partitionOffset, partitionSize);
  ArtsAnalysisManager &AM = analysis->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);

  Value firstDynIdx;
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      for (Value idx : fullChain) {
        int64_t constVal;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          firstDynIdx = idx;
          break;
        }
      }
      if (firstDynIdx)
        break;
    }
    if (firstDynIdx)
      break;
  }

  bool offsetIsZero = ValueUtils::isZeroConstant(
      ValueUtils::stripNumericCasts(partitionOffset));
  if (!firstDynIdx) {
    if (offsetIsZero) {
      ARTS_DEBUG("  No dynamic index - allowing zero-offset partition hints");
      computedBlockInfo = std::make_pair(partitionOffset, partitionSize);
      return true;
    }
    ARTS_DEBUG("  No dynamic index - allowing for heuristic evaluation");
    return true;
  }

  EdtOp edt = getEdtUser();
  SmallVector<LoopNode *, 4> loopNodes;
  loopAnalysis.collectForLoopsInOperation(edt, loopNodes);

  if (loopNodes.empty()) {
    ARTS_DEBUG("  No for loops - allowing for heuristic evaluation");
    return true;
  }

  LoopNode *blockLoopNode =
      findBestLoopNode(loopNodes, firstDynIdx, partitionOffset, offsetIsZero);

  if (!blockLoopNode) {
    ARTS_DEBUG("  No block loop found - allowing for heuristic evaluation");
    return true;
  }

  Value loopIV = blockLoopNode->getInductionVar();
  AccessBoundsInfo bounds = analyzeAccessBounds(this, partitionOffset, loopIV);

  if (!bounds.valid) {
    ARTS_DEBUG(
        "  Access bounds analysis failed - allowing for heuristic evaluation");
    return true;
  }

  ARTS_DEBUG("  Bounds analysis for acquire "
             << dbAcquireOp << ": minOffset=" << bounds.minOffset
             << ", maxOffset=" << bounds.maxOffset
             << ", isStencil=" << bounds.isStencil);

  stencilBounds = StencilBounds::create(bounds.minOffset, bounds.maxOffset,
                                        bounds.isStencil, bounds.valid);

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  Value adjustedOffset = partitionOffset;
  Value adjustedSize = partitionSize;

  if (bounds.minOffset < 0) {
    Value absAdj =
        builder.create<arith::ConstantIndexOp>(loc, -bounds.minOffset);
    Value cond = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                               partitionOffset, absAdj);
    Value sub = builder.create<arith::SubIOp>(loc, partitionOffset, absAdj);
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    adjustedOffset = builder.create<arith::SelectOp>(loc, cond, sub, zero);
  } else if (bounds.minOffset > 0) {
    Value adj = builder.create<arith::ConstantIndexOp>(loc, bounds.minOffset);
    adjustedOffset = builder.create<arith::AddIOp>(loc, partitionOffset, adj);
  }

  int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
  if (sizeAdjustment != 0) {
    Value adjustment =
        builder.create<arith::ConstantIndexOp>(loc, sizeAdjustment);
    adjustedSize =
        builder.create<arith::AddIOp>(loc, partitionSize, adjustment);
  }

  computedBlockInfo = std::make_pair(adjustedOffset, adjustedSize);
  return true;
}

bool DbAcquireNode::canBePartitioned() {
  if (!edtUserOp)
    return false;

  if (!hasValidEdtAndAccesses())
    return false;

  if (!computePartitionBounds())
    return false;

  for (const auto &childAcq : childAcquires) {
    if (childAcq && !childAcq->canBePartitioned()) {
      ARTS_DEBUG("Skipping acquire " << dbAcquireOp
                                     << ": nested acquire child failed");
      return false;
    }
  }

  ARTS_DEBUG("PASS: acquire can be partitioned: " << dbAcquireOp);
  return true;
}

AccessPattern DbAcquireNode::getAccessPattern() const {
  if (accessPattern)
    return *accessPattern;

  if (stencilBounds && stencilBounds->valid && stencilBounds->isStencil) {
    accessPattern = AccessPattern::Stencil;
    return *accessPattern;
  }

  DbAcquireOp acqOp = const_cast<DbAcquireOp &>(dbAcquireOp);
  if (acqOp.hasMultiplePartitionEntries()) {
    int64_t minOffset = 0, maxOffset = 0;
    if (DatablockUtils::hasMultiEntryStencilPattern(acqOp, minOffset,
                                                    maxOffset)) {
      accessPattern = AccessPattern::Stencil;
      return *accessPattern;
    }
  }
  if (!acqOp.getPartitionIndices().empty()) {
    accessPattern = AccessPattern::Indexed;
    return *accessPattern;
  }

  accessPattern = AccessPattern::Uniform;
  return *accessPattern;
}

static AccessBoundsInfo analyzeAccessBounds(DbAcquireNode *node,
                                            Value blockOffset, Value loopIV) {
  AccessBoundsInfo bounds;
  bounds.minOffset = std::numeric_limits<int64_t>::max();
  bounds.maxOffset = std::numeric_limits<int64_t>::min();

  if (!node || !blockOffset || !loopIV)
    return bounds;

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  node->collectAccessOperations(dbRefToMemOps);

  if (dbRefToMemOps.empty()) {
    bounds.minOffset = 0;
    bounds.maxOffset = 0;
    bounds.valid = true;
    return bounds;
  }

  bool foundAny = false;

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> indexChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      if (indexChain.empty())
        continue;

      Value firstDynIdx;
      for (Value idx : indexChain) {
        int64_t constVal;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          firstDynIdx = idx;
          break;
        }
      }

      if (!firstDynIdx)
        continue;

      auto constOffset =
          ValueUtils::extractConstantOffset(firstDynIdx, loopIV, blockOffset);
      if (constOffset) {
        foundAny = true;
        bounds.minOffset = std::min(bounds.minOffset, *constOffset);
        bounds.maxOffset = std::max(bounds.maxOffset, *constOffset);
      } else {
        return bounds;
      }
    }
  }

  if (foundAny) {
    bounds.isStencil = (bounds.minOffset != bounds.maxOffset);
    bounds.valid = true;
  }

  return bounds;
}

void DbAcquireNode::collectAccessOperations(
    DenseMap<DbRefOp, SetVector<Operation *>> &dbRefToMemOps) {
  if (!edtUserOp || !useInEdt)
    return;
  EdtOp edtUser = getEdtUser();

  SmallVector<Value, 16> worklist;
  SetVector<Value> visited;
  worklist.push_back(useInEdt);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!visited.insert(current))
      continue;

    for (Operation *user : current.getUsers()) {
      if (!edtUser.getBody().isAncestor(user->getParentRegion()))
        continue;

      if (auto dbRef = dyn_cast<DbRefOp>(user)) {
        dbRefToMemOps.try_emplace(dbRef);

        Value refResult = dbRef.getResult();
        worklist.push_back(refResult);

        for (Operation *memUser : refResult.getUsers()) {
          if (!edtUser.getBody().isAncestor(memUser->getParentRegion()))
            continue;
          if (isa<memref::LoadOp, memref::StoreOp>(memUser))
            dbRefToMemOps[dbRef].insert(memUser);
        }
      }
    }
  }
}

bool DbAcquireNode::hasIndirectAccess() const {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  const_cast<DbAcquireNode *>(this)->collectAccessOperations(dbRefToMemOps);
  Value offset = partitionOffset;

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      for (Value idx : fullChain) {
        int64_t constVal;
        if (ValueUtils::getConstantIndex(idx, constVal))
          continue;
        if (isIndirectIndex(idx, offset))
          return true;
      }
    }
  }

  return false;
}

bool DbAcquireNode::hasDirectAccess() const {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  const_cast<DbAcquireNode *>(this)->collectAccessOperations(dbRefToMemOps);
  Value offset = partitionOffset;

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      if (fullChain.empty())
        return true;

      bool hasIndirect = false;
      for (Value idx : fullChain) {
        int64_t constVal;
        if (ValueUtils::getConstantIndex(idx, constVal))
          continue;
        if (isIndirectIndex(idx, offset)) {
          hasIndirect = true;
          break;
        }
      }

      if (!hasIndirect)
        return true;
    }
  }

  return false;
}

/// Helper methods for querying memory access
void DbAcquireNode::forEachMemoryAccess(
    llvm::function_ref<void(Operation *, bool)> callback) {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *op : memOps) {
      bool isStore = isa<memref::StoreOp>(op);
      callback(op, isStore);
    }
  }
}

bool DbAcquireNode::hasLoads() {
  bool found = false;
  forEachMemoryAccess([&](Operation *, bool isStore) {
    if (!isStore)
      found = true;
  });
  return found;
}

bool DbAcquireNode::hasStores() {
  bool found = false;
  forEachMemoryAccess([&](Operation *, bool isStore) {
    if (isStore)
      found = true;
  });
  return found;
}

bool DbAcquireNode::hasMemoryAccesses() {
  bool found = false;
  forEachMemoryAccess([&](Operation *, bool) { found = true; });
  return found;
}

size_t DbAcquireNode::countLoads() {
  size_t count = 0;
  forEachMemoryAccess([&](Operation *, bool isStore) {
    if (!isStore)
      ++count;
  });
  return count;
}

size_t DbAcquireNode::countStores() {
  size_t count = 0;
  forEachMemoryAccess([&](Operation *, bool isStore) {
    if (isStore)
      ++count;
  });
  return count;
}

bool DbAcquireNode::canPartitionWithOffset(Value offset) {
  if (!offset)
    return false;
  Value offsetStripped = ValueUtils::stripNumericCasts(offset);
  if (ValueUtils::isZeroConstant(offsetStripped))
    return true;

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);

  if (dbRefToMemOps.empty())
    return true;

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      if (fullChain.empty())
        continue;

      bool offsetSeen = false;
      int64_t firstDynPos = -1;
      for (unsigned i = 0; i < fullChain.size(); ++i) {
        int64_t constVal = 0;
        bool isConst = ValueUtils::getConstantIndex(fullChain[i], constVal);
        if (!isConst && firstDynPos < 0)
          firstDynPos = static_cast<int64_t>(i);

        if (ValueUtils::dependsOn(fullChain[i], offsetStripped)) {
          if (firstDynPos >= 0 && i != static_cast<unsigned>(firstDynPos)) {
            ARTS_DEBUG("  partition offset maps to non-leading dim; "
                       "disabling blocked partitioning");
            return false;
          }
          offsetSeen = true;
        }
      }

      Value firstDynIdx;
      for (Value idx : fullChain) {
        int64_t constVal;
        if (ValueUtils::getConstantIndex(idx, constVal))
          continue;

        if (isIndirectIndex(idx, offsetStripped)) {
          ARTS_DEBUG("  indirect index detected; disabling blocked "
                     "partitioning");
          return false;
        }

        if (!firstDynIdx)
          firstDynIdx = idx;
      }

      if (!firstDynIdx)
        continue;

      if (!offsetSeen ||
          !LoopNode::dependsOnLoopInitNormalized(firstDynIdx, offsetStripped))
        return false;
    }
  }

  return true;
}

LogicalResult DbAcquireNode::computeBlockInfo(Value &blockOffset,
                                              Value &blockSize) {
  ARTS_DEBUG("computeBlockInfo for acquire: " << dbAcquireOp);

  if (computedBlockInfo) {
    blockOffset = computedBlockInfo->first;
    blockSize = computedBlockInfo->second;
    ARTS_DEBUG("  using cached block info");
    return success();
  }

  auto fullRangeFallback = [&](StringRef reason) -> LogicalResult {
    DbAllocNode *allocNode = getRootAlloc();
    if (!allocNode) {
      ARTS_DEBUG("  no allocation node for full-range fallback");
      return failure();
    }
    DbAllocOp allocOp = allocNode->getDbAllocOp();
    if (!allocOp || allocOp.getElementSizes().empty()) {
      ARTS_DEBUG("  missing element sizes for full-range fallback");
      return failure();
    }

    OpBuilder builder(dbAcquireOp);
    Location loc = dbAcquireOp.getLoc();
    builder.setInsertionPoint(dbAcquireOp);

    blockOffset = builder.create<arith::ConstantIndexOp>(loc, 0);
    blockSize = allocOp.getElementSizes().front();
    ARTS_DEBUG("  full-range fallback (" << reason << ")");
    return success();
  };

  EdtOp edt = getEdtUser();
  if (!edt) {
    ARTS_DEBUG("  no EDT user found");
    return fullRangeFallback("no EDT user");
  }

  if (!hasMemoryAccesses() && !partitionOffset && !partitionSize)
    return fullRangeFallback("pass-through acquire");

  ArtsAnalysisManager &AM = analysis->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();
  SmallVector<LoopNode *> forLoopNodes;
  SmallVector<LoopNode *> whileLoopNodes;
  loopAnalysis.collectLoopsInOperation<scf::ForOp>(edt, forLoopNodes);
  loopAnalysis.collectLoopsInOperation<scf::WhileOp>(edt, whileLoopNodes);

  scf::WhileOp blockWhile;
  if (!whileLoopNodes.empty()) {
    blockWhile = cast<scf::WhileOp>(whileLoopNodes[0]->getLoopOp());
  }
  if (blockWhile)
    ARTS_DEBUG("  found scf.while at " << blockWhile.getLoc());
  if (!forLoopNodes.empty())
    ARTS_DEBUG("  found " << forLoopNodes.size() << " scf.for loops in EDT");

  if (partitionOffset && partitionSize) {
    if (succeeded(computeBlockInfoFromHints(blockOffset, blockSize))) {
      /// Prefer loop-derived size when it depends on the block offset or is
      /// dynamic, while keeping the hint-derived offset.
      if (!forLoopNodes.empty()) {
        Value loopOffset, loopSize;
        if (succeeded(computeBlockInfoFromForLoop(forLoopNodes, loopOffset,
                                                  loopSize))) {
          bool useLoopSize = false;
          int64_t hintConst = 0;
          int64_t loopConst = 0;
          bool hintIsConst = ValueUtils::getConstantIndex(blockSize, hintConst);
          bool loopIsConst = ValueUtils::getConstantIndex(loopSize, loopConst);
          bool offsetRelated = false;
          if (loopOffset && blockOffset) {
            Value loopOffStripped = ValueUtils::stripNumericCasts(loopOffset);
            Value blockOffStripped = ValueUtils::stripNumericCasts(blockOffset);
            if (loopOffStripped == blockOffStripped)
              offsetRelated = true;
            if (!offsetRelated &&
                (ValueUtils::dependsOn(loopOffset, blockOffset) ||
                 ValueUtils::dependsOn(blockOffset, loopOffset)))
              offsetRelated = true;
            if (!offsetRelated) {
              int64_t loopOffConst = 0;
              int64_t blockOffConst = 0;
              if (ValueUtils::getConstantIndex(loopOffStripped, loopOffConst) &&
                  ValueUtils::getConstantIndex(blockOffStripped,
                                               blockOffConst) &&
                  loopOffConst == blockOffConst)
                offsetRelated = true;
            }
          }

          bool loopSizeDependsOnOffset =
              blockOffset && ValueUtils::dependsOn(loopSize, blockOffset);

          if (loopSizeDependsOnOffset)
            useLoopSize = true;
          if (offsetRelated && !loopIsConst)
            useLoopSize = true;
          if (offsetRelated && hintIsConst && loopIsConst &&
              hintConst != loopConst)
            useLoopSize = true;

          if (useLoopSize) {
            blockSize = loopSize;
            ARTS_DEBUG("  overriding blockSize from loops: " << blockSize);
          }
        }
        /// If loop analysis couldn't connect offsets, still allow a loop upper
        /// bound that depends on the partition offset (common in chunked loops
        /// where EDT body only sees local indices).
        if (partitionOffset &&
            !ValueUtils::dependsOn(blockSize, partitionOffset)) {
          func::FuncOp func = dbAcquireOp->getParentOfType<func::FuncOp>();
          if (func) {
            OpBuilder builder(dbAcquireOp);
            Location loc = dbAcquireOp.getLoc();
            DominanceInfo domInfo(func);
            for (LoopNode *loopNode : forLoopNodes) {
              auto loop = cast<scf::ForOp>(loopNode->getLoopOp());
              Value ubDom = ValueUtils::traceValueToDominating(
                  loop.getUpperBound(), dbAcquireOp, builder, domInfo, loc);
              if (!ubDom)
                continue;
              if (ValueUtils::dependsOn(ubDom, partitionOffset)) {
                blockSize = ubDom;
                ARTS_DEBUG("  overriding blockSize from loop upper bound: "
                           << blockSize);
                break;
              }
            }
          }
        }
      }
      return success();
    }
  }

  if (blockWhile) {
    Value offsetForCheck;
    if (succeeded(computeBlockInfoFromWhile(blockWhile, blockOffset, blockSize,
                                            &offsetForCheck))) {
      Value checkOffset = offsetForCheck ? offsetForCheck : blockOffset;
      if (!canPartitionWithOffset(checkOffset))
        return fullRangeFallback("scf.while offset not derived from access");
      return success();
    }
    ARTS_DEBUG("  computeBlockInfoFromWhile failed");
    return fullRangeFallback("scf.while block info");
  }

  llvm::sort(forLoopNodes, [](LoopNode *a, LoopNode *b) {
    return a->getNestingDepth() < b->getNestingDepth();
  });

  return computeBlockInfoFromForLoop(forLoopNodes, blockOffset, blockSize);
}

LogicalResult DbAcquireNode::computeBlockInfoFromWhile(scf::WhileOp whileOp,
                                                       Value &blockOffset,
                                                       Value &blockSize,
                                                       Value *offsetForCheck) {
  if (!whileOp || !dbAcquireOp)
    return failure();
  Block &before = whileOp.getBefore().front();
  auto condition = dyn_cast<scf::ConditionOp>(before.getTerminator());
  if (!condition)
    return failure();

  LoopAnalysis &loopAnalysis = analysis->getAnalysisManager().getLoopAnalysis();
  LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(whileOp);
  if (!loopNode)
    return failure();

  Value loopIV = loopNode->getInductionVar();
  auto ivArg = loopIV.dyn_cast<BlockArgument>();
  if (!ivArg)
    return failure();
  if (ivArg.getOwner() != &before)
    return failure();

  unsigned ivIndex = ivArg.getArgNumber();
  if (ivIndex >= whileOp.getInits().size())
    return failure();
  Value initValue = whileOp.getInits()[ivIndex];
  if (offsetForCheck)
    *offsetForCheck = initValue;

  SmallVector<Value> whileBounds;
  arts::collectWhileBounds(condition.getCondition(), loopIV, whileBounds);
  if (whileBounds.empty())
    return failure();

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);
  func::FuncOp func = dbAcquireOp->getParentOfType<func::FuncOp>();
  if (!func)
    return failure();
  DominanceInfo domInfo(func);

  Value initDom = ValueUtils::traceValueToDominating(initValue, dbAcquireOp,
                                                     builder, domInfo, loc);
  if (!initDom)
    return failure();

  Value startIdx = ValueUtils::ensureIndexType(initDom, builder, loc);
  if (!startIdx)
    return failure();

  SmallVector<Value> blockSizes;
  Value initStripped = ValueUtils::stripNumericCasts(initValue);
  for (Value bound : whileBounds) {
    Value boundStripped = ValueUtils::stripNumericCasts(bound);
    if (auto addOp = boundStripped.getDefiningOp<arith::AddIOp>()) {
      Value lhs = addOp.getLhs();
      Value rhs = addOp.getRhs();
      Value lhsStripped = ValueUtils::stripNumericCasts(lhs);
      Value rhsStripped = ValueUtils::stripNumericCasts(rhs);
      Value sizeCandidate;
      if (lhsStripped == initStripped)
        sizeCandidate = rhs;
      else if (rhsStripped == initStripped)
        sizeCandidate = lhs;

      if (sizeCandidate) {
        Value sizeDom = ValueUtils::traceValueToDominating(
            sizeCandidate, dbAcquireOp, builder, domInfo, loc);
        Value sizeIdx = ValueUtils::ensureIndexType(sizeDom, builder, loc);
        if (sizeIdx) {
          blockSizes.push_back(sizeIdx);
          continue;
        }
      }
    }

    Value boundDom = ValueUtils::traceValueToDominating(bound, dbAcquireOp,
                                                        builder, domInfo, loc);
    Value boundIdx = ValueUtils::ensureIndexType(boundDom, builder, loc);
    if (!boundIdx)
      continue;
    blockSizes.push_back(
        builder.create<arith::SubIOp>(loc, boundIdx, startIdx));
  }

  if (blockSizes.empty())
    return failure();

  Value minSize = blockSizes.front();
  for (Value candidate : llvm::drop_begin(blockSizes))
    minSize = builder.create<arith::MinSIOp>(loc, minSize, candidate);

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  minSize = builder.create<arith::MaxSIOp>(loc, minSize, zero);

  AccessBoundsInfo bounds = analyzeAccessBounds(this, initValue, loopIV);
  if (!bounds.valid)
    return failure();

  if (!stencilBounds) {
    stencilBounds = StencilBounds::create(bounds.minOffset, bounds.maxOffset,
                                          bounds.isStencil, bounds.valid);
  }

  Value adjustedOffset = startIdx;
  Value adjustedSize = minSize;

  if (bounds.minOffset < 0) {
    Value absAdj =
        builder.create<arith::ConstantIndexOp>(loc, -bounds.minOffset);
    Value cond = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                               startIdx, absAdj);
    Value sub = builder.create<arith::SubIOp>(loc, startIdx, absAdj);
    Value zeroIdx = builder.create<arith::ConstantIndexOp>(loc, 0);
    adjustedOffset = builder.create<arith::SelectOp>(loc, cond, sub, zeroIdx);
  } else if (bounds.minOffset > 0) {
    Value adj = builder.create<arith::ConstantIndexOp>(loc, bounds.minOffset);
    adjustedOffset = builder.create<arith::AddIOp>(loc, startIdx, adj);
  }

  int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
  if (sizeAdjustment != 0) {
    Value adjustment =
        builder.create<arith::ConstantIndexOp>(loc, sizeAdjustment);
    adjustedSize = builder.create<arith::AddIOp>(loc, minSize, adjustment);
  }

  blockOffset = adjustedOffset;
  blockSize = adjustedSize;
  return success();
}

LogicalResult DbAcquireNode::computeBlockInfoFromHints(Value &blockOffset,
                                                       Value &blockSize) {
  if (!partitionOffset || !partitionSize)
    return failure();

  ARTS_DEBUG("  computeBlockInfoFromHints: " << partitionOffset << " / "
                                             << partitionSize);
  if (!originalBounds)
    originalBounds = std::make_pair(partitionOffset, partitionSize);

  if (!canPartitionWithOffset(partitionOffset)) {
    ARTS_DEBUG("  offset hints not derived from access; failing");
    return failure();
  }

  Value firstDynIdx;
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      for (Value idx : fullChain) {
        int64_t constVal;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          firstDynIdx = idx;
          break;
        }
      }
      if (firstDynIdx)
        break;
    }
    if (firstDynIdx)
      break;
  }

  bool offsetIsZero = ValueUtils::isZeroConstant(
      ValueUtils::stripNumericCasts(partitionOffset));
  if (!firstDynIdx) {
    if (offsetIsZero) {
      blockOffset = partitionOffset;
      blockSize = partitionSize;
      return success();
    }
    return failure();
  }

  EdtOp edt = getEdtUser();
  ArtsAnalysisManager &AM = analysis->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();
  SmallVector<LoopNode *> forLoopNodes;
  loopAnalysis.collectForLoopsInOperation(edt, forLoopNodes);

  LoopNode *boundsLoopNode = findBestLoopNode(forLoopNodes, firstDynIdx,
                                              partitionOffset, offsetIsZero);

  if (boundsLoopNode) {
    Value loopIV = boundsLoopNode->getInductionVar();
    AccessBoundsInfo bounds =
        analyzeAccessBounds(this, partitionOffset, loopIV);

    if (bounds.valid) {
      if (!stencilBounds)
        stencilBounds = StencilBounds::create(
            bounds.minOffset, bounds.maxOffset, bounds.isStencil, bounds.valid);

      OpBuilder builder(dbAcquireOp);
      Location loc = dbAcquireOp.getLoc();
      builder.setInsertionPoint(dbAcquireOp);

      if (bounds.minOffset != 0) {
        Value adjustment =
            builder.create<arith::ConstantIndexOp>(loc, bounds.minOffset);
        blockOffset =
            builder.create<arith::AddIOp>(loc, partitionOffset, adjustment);
      } else {
        blockOffset = partitionOffset;
      }

      int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
      if (sizeAdjustment != 0) {
        Value adjustment =
            builder.create<arith::ConstantIndexOp>(loc, sizeAdjustment);
        blockSize =
            builder.create<arith::AddIOp>(loc, partitionSize, adjustment);
      } else {
        blockSize = partitionSize;
      }

      return success();
    }
  }

  blockOffset = partitionOffset;
  blockSize = partitionSize;
  return success();
}

LogicalResult
DbAcquireNode::computeBlockInfoFromForLoop(ArrayRef<LoopNode *> loopNodes,
                                           Value &blockOffset, Value &blockSize,
                                           Value *offsetForCheck) {
  Value ptrArg = getUseInEdt();
  if (!ptrArg) {
    ARTS_DEBUG("  computeBlockInfoFromForLoop: no EDT block argument");
    return failure();
  }

  if (loopNodes.empty()) {
    ARTS_DEBUG("  computeBlockInfoFromForLoop: no scf.for found");
    return failure();
  }

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  for (LoopNode *loopNode : loopNodes) {
    Value offsetCandidate;
    auto loop = cast<scf::ForOp>(loopNode->getLoopOp());
    Value loopIV = loopNode->getInductionVar();
    auto pickCandidateFromIndex = [&](Value idx) -> Value {
      Value stripped = ValueUtils::stripNumericCasts(idx);
      if (stripped == loopIV)
        return builder.create<arith::ConstantIndexOp>(loc, 0);

      if (auto addOp = stripped.getDefiningOp<arith::AddIOp>()) {
        Value lhs = addOp.getLhs();
        Value rhs = addOp.getRhs();
        bool lhsIV = loopNode->dependsOnInductionVarNormalized(lhs);
        bool rhsIV = loopNode->dependsOnInductionVarNormalized(rhs);
        if (lhsIV && !rhsIV && loopNode->isValueLoopInvariant(rhs))
          return rhs;
        if (rhsIV && !lhsIV && loopNode->isValueLoopInvariant(lhs))
          return lhs;
      }
      return Value();
    };

    for (Operation *user : ptrArg.getUsers()) {
      auto refOp = dyn_cast<DbRefOp>(user);
      if (!refOp || !loop->isAncestor(refOp))
        continue;

      Value refVal = refOp.getResult();
      for (Operation *memUser : refVal.getUsers()) {
        ValueRange memIndices;
        if (auto loadOp = dyn_cast<memref::LoadOp>(memUser))
          memIndices = loadOp.getIndices();
        else if (auto storeOp = dyn_cast<memref::StoreOp>(memUser))
          memIndices = storeOp.getIndices();
        else
          continue;

        if (!loop->isAncestor(memUser))
          continue;

        if (memIndices.empty())
          continue;
        Value idx = memIndices.front();
        offsetCandidate = pickCandidateFromIndex(idx);

        if (offsetCandidate)
          break;
      }

      /// If memref indices don't expose the partitioned dimension (common in
      /// row-partitioned 2D arrays), fall back to DbRef indices.
      if (!offsetCandidate) {
        for (Value refIdx : refOp.getIndices()) {
          offsetCandidate = pickCandidateFromIndex(refIdx);
          if (offsetCandidate)
            break;
        }
      }
      if (offsetCandidate)
        break;
    }

    if (!offsetCandidate) {
      ARTS_DEBUG("  no offset candidate found in loop at " << loop.getLoc());
      continue;
    }

    AccessBoundsInfo bounds =
        analyzeAccessBounds(this, offsetCandidate, loopIV);
    if (!bounds.valid) {
      ARTS_DEBUG("  bounds analysis failed for loop at " << loop.getLoc());
      continue;
    }

    if (!stencilBounds)
      stencilBounds = StencilBounds::create(bounds.minOffset, bounds.maxOffset,
                                            bounds.isStencil, bounds.valid);

    func::FuncOp func = dbAcquireOp->getParentOfType<func::FuncOp>();
    if (!func)
      continue;
    DominanceInfo domInfo(func);

    Value offsetDom = ValueUtils::traceValueToDominating(
        offsetCandidate, dbAcquireOp, builder, domInfo, loc);
    Value sizeDom = ValueUtils::traceValueToDominating(
        loop.getUpperBound(), dbAcquireOp, builder, domInfo, loc);
    if (!offsetDom || !sizeDom) {
      ARTS_DEBUG("  offset/size does not dominate for loop at "
                 << loop.getLoc());
      continue;
    }

    Value adjustedOffset = ValueUtils::ensureIndexType(offsetDom, builder, loc);
    Value adjustedSize = ValueUtils::ensureIndexType(sizeDom, builder, loc);
    if (!adjustedOffset || !adjustedSize)
      continue;

    if (bounds.minOffset < 0) {
      Value absAdj =
          builder.create<arith::ConstantIndexOp>(loc, -bounds.minOffset);
      Value cond = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                                 adjustedOffset, absAdj);
      Value sub = builder.create<arith::SubIOp>(loc, adjustedOffset, absAdj);
      Value zeroIdx = builder.create<arith::ConstantIndexOp>(loc, 0);
      adjustedOffset = builder.create<arith::SelectOp>(loc, cond, sub, zeroIdx);
    } else if (bounds.minOffset > 0) {
      Value adj = builder.create<arith::ConstantIndexOp>(loc, bounds.minOffset);
      adjustedOffset = builder.create<arith::AddIOp>(loc, adjustedOffset, adj);
    }

    int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
    if (sizeAdjustment != 0) {
      Value adjustment =
          builder.create<arith::ConstantIndexOp>(loc, sizeAdjustment);
      adjustedSize =
          builder.create<arith::AddIOp>(loc, adjustedSize, adjustment);
    }

    blockOffset = adjustedOffset;
    blockSize = adjustedSize;
    if (offsetForCheck)
      *offsetForCheck = offsetCandidate;
    ARTS_DEBUG("  selected loop " << loop.getLoc() << " blockOffset="
                                  << blockOffset << " blockSize=" << blockSize);

    Value checkOffset = offsetForCheck ? *offsetForCheck : blockOffset;
    if (!canPartitionWithOffset(checkOffset))
      return failure();
    return success();
  }

  ARTS_DEBUG("  no suitable scf.for loop found for block info");
  return failure();
}

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << estimatedBytes;
  os << "\n";
}

bool DbAcquireNode::isWorkerIndexedAccess() const {
  if (!op)
    return false;

  DbAcquireOp mutableOp(op);

  Value partitionIdx =
      !mutableOp.getIndices().empty()
          ? mutableOp.getIndices().front()
          : (!mutableOp.getOffsets().empty() ? mutableOp.getOffsets().front()
                                             : nullptr);
  Value partitionSz =
      !mutableOp.getSizes().empty() ? mutableOp.getSizes().front() : nullptr;

  if (!partitionIdx || !partitionSz)
    return false;

  int64_t sz = 0;
  if (!ValueUtils::getConstantIndex(partitionSz, sz) || sz != 1)
    return false;

  return isa<BlockArgument>(partitionIdx);
}

/// Twin-diff overlap analysis methods
bool DbAcquireNode::hasDisjointPartitionWith(const DbAcquireNode *other) const {
  if (!other)
    return false;

  DbAcquireOp acqA = dbAcquireOp;
  DbAcquireOp acqB = other->getDbAcquireOp();
  if (!acqA || !acqB)
    return false;

  auto getPartitionIdx = [](DbAcquireOp acq) -> Value {
    if (!acq.getIndices().empty())
      return acq.getIndices().front();
    if (!acq.getOffsets().empty())
      return acq.getOffsets().front();
    return nullptr;
  };
  auto getPartitionSize = [](DbAcquireOp acq) -> Value {
    if (!acq.getSizes().empty())
      return acq.getSizes().front();
    return nullptr;
  };

  Value partIdxA = getPartitionIdx(acqA);
  Value partSizeA = getPartitionSize(acqA);
  Value partIdxB = getPartitionIdx(acqB);
  Value partSizeB = getPartitionSize(acqB);

  if (!partIdxA || !partSizeA || !partIdxB || !partSizeB)
    return false;

  int64_t idxAVal = 0, sizeAVal = 0, idxBVal = 0, sizeBVal = 0;
  if (ValueUtils::getConstantIndex(partIdxA, idxAVal) &&
      ValueUtils::getConstantIndex(partSizeA, sizeAVal) &&
      ValueUtils::getConstantIndex(partIdxB, idxBVal) &&
      ValueUtils::getConstantIndex(partSizeB, sizeBVal)) {
    if (sizeAVal == sizeBVal && idxAVal != idxBVal)
      return true;
    int64_t startA = idxAVal;
    int64_t endA = startA + sizeAVal;
    int64_t startB = idxBVal;
    int64_t endB = startB + sizeBVal;
    if (endA <= startB || endB <= startA)
      return true;
  }

  int64_t sizeAConst = 0, sizeBConst = 0;
  bool sizeAIsOne =
      ValueUtils::getConstantIndex(partSizeA, sizeAConst) && sizeAConst == 1;
  bool sizeBIsOne =
      ValueUtils::getConstantIndex(partSizeB, sizeBConst) && sizeBConst == 1;

  if (sizeAIsOne && sizeBIsOne) {
    auto blockArgA = dyn_cast<BlockArgument>(partIdxA);
    auto blockArgB = dyn_cast<BlockArgument>(partIdxB);
    if (blockArgA && blockArgB && blockArgA != blockArgB &&
        blockArgA.getOwner() != blockArgB.getOwner())
      return true;
  }

  return false;
}

bool DbAcquireNode::accessIndexDependsOn(Value idx) {
  if (!idx)
    return false;

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DatablockUtils::collectFullIndexChain(dbRef, memOp);
      for (Value chainIdx : fullChain) {
        if (ValueUtils::dependsOn(chainIdx, idx))
          return true;
      }
    }
  }

  return false;
}

bool DbAcquireNode::validateElementWisePartitioning() {
  auto partitionIndices = dbAcquireOp.getPartitionIndices();
  if (partitionIndices.empty())
    return true;

  /// Check 1: Do partition indices come from a loop with step > 1?
  /// If so, this is a block-wise pattern (e.g., for k_block = 0 to N step 16)
  for (Value idx : partitionIndices) {
    Operation *parent = dbAcquireOp->getParentOp();
    while (parent) {
      if (auto forOp = dyn_cast<scf::ForOp>(parent)) {
        Value loopIV = forOp.getInductionVar();
        if (ValueUtils::dependsOn(idx, loopIV)) {
          if (auto stepConst =
                  forOp.getStep().getDefiningOp<arith::ConstantIndexOp>()) {
            if (stepConst.value() > 1) {
              ARTS_DEBUG("  Block-wise pattern detected: loop step = "
                         << stepConst.value() << " for index " << idx);
              return false;
            }
          }
        }
      }
      parent = parent->getParentOp();
    }
  }

  /// Check 2: Do the partition indices appear in EDT body accesses?
  for (Value idx : partitionIndices) {
    if (!accessIndexDependsOn(idx)) {
      ARTS_DEBUG("  Partition index " << idx << " not found in access chain");
      return false;
    }
  }

  return true;
}
