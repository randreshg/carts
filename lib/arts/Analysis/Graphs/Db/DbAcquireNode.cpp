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
// DbAcquireNode
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

  /// Capture optional offset/size hints directly from the operation.
  /// For now lets only consider the front hint for the partition offset and
  /// size
  if (!dbAcquireOp.getOffsetHints().empty())
    partitionOffset = dbAcquireOp.getOffsetHints().front();
  if (!dbAcquireOp.getSizeHints().empty())
    partitionSize = dbAcquireOp.getSizeHints().front();

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
    llvm_unreachable("Acquire ptr should be used by an EDT or another acquire");
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
///
/// The index expression is typically: chunkOffset + loopIV + constant
/// We extract the constant to determine the offset adjustment needed.
///===----------------------------------------------------------------------===///

/// Represents analyzed access bounds for an array
struct AccessBoundsInfo {
  int64_t minOffset = 0;  /// Min constant offset from chunk_base
  int64_t maxOffset = 0;  /// Max constant offset from chunk_base
  bool isStencil = false; /// True if min != max (multiple offsets detected)
  bool valid = false;     /// True if analysis succeeded
};

/// Extract the constant offset from an index expression.
/// Tries to express idx as: chunkOffset + loopIV + constant

/// Analyze all memory accesses to compute actual bounds relative to chunk base.
/// For stencil patterns like A[i-1], A[i], A[i+1], this extracts the min/max
/// offsets. For uniform access like B[i+1], this extracts the constant
/// adjustment.
static AccessBoundsInfo analyzeAccessBounds(DbAcquireNode *node,
                                            Value chunkOffset, Value loopIV);

/// Detect indirect (data-dependent) indices derived from memory loads.
static bool isIndirectIndex(Value idx, int depth = 0) {
  if (!idx || depth > 8)
    return false;

  idx = ValueUtils::stripNumericCasts(idx);
  if (ValueUtils::isValueConstant(idx) || isa<BlockArgument>(idx))
    return false;

  Operation *defOp = idx.getDefiningOp();
  if (!defOp)
    return false;

  if (isa<memref::LoadOp, LLVM::LoadOp>(defOp))
    return true;

  if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
          arith::DivUIOp, arith::RemSIOp, arith::RemUIOp, arith::IndexCastOp,
          arith::IndexCastUIOp, arith::ExtSIOp, arith::ExtUIOp,
          arith::TruncIOp, arith::MinSIOp, arith::MinUIOp, arith::MaxSIOp,
          arith::MaxUIOp, arith::SelectOp, arith::CmpIOp, arith::CmpFOp,
          affine::AffineApplyOp>(defOp)) {
    for (Value operand : defOp->getOperands()) {
      if (isIndirectIndex(operand, depth + 1))
        return true;
    }
    return false;
  }

  /// Any other op is treated as data-dependent.
  return true;
}

/// Check EDT type, memory accesses, and parallel loop metadata.
bool DbAcquireNode::hasValidEdtAndAccesses(bool useFineGrainedFallback) {
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << dbAcquireOp << ": " << reason);
    return false;
  };

  /// Basic operation validation
  if (!dbAcquireOp)
    return skip("invalid acquire operation");

  /// Allow non-EDT acquires to be treated as full-range if we can size them.
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
    bool allowFallback = useFineGrainedFallback &&
                         allocNode->hasNonAffineAccesses &&
                         *allocNode->hasNonAffineAccesses;
    if (!allowFallback)
      return skip("memref metadata is not marked as parallel-friendly");
    ARTS_DEBUG("Memref not marked parallel-friendly but fine-grained fallback "
               "enabled for non-affine access");
  }

  /// EDT and loop metadata validation
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

/// Extract chunk offset/size and validate access patterns.
bool DbAcquireNode::computePartitionBounds(bool useFineGrainedFallback) {
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << dbAcquireOp << ": " << reason);
    return false;
  };

  DbAcquireOp mutableAcquire = DbAcquireOp(dbAcquireOp.getOperation());

  /// Prefer explicit offset/size hints if they exist.
  auto offsetHints = mutableAcquire.getOffsetHints();
  auto sizeHints = mutableAcquire.getSizeHints();
  if (!offsetHints.empty())
    partitionOffset = offsetHints.front();
  if (!sizeHints.empty())
    partitionSize = sizeHints.front();

  /// Only fall back to existing offsets/sizes if they look meaningful
  /// (i.e., not the default slice of offset=0, size=1).
  auto hasMeaningfulSlice = [&](Value off, Value size) -> bool {
    int64_t offConst = 0, sizeConst = 0;
    bool isDefaultOffset =
        ValueUtils::getConstantIndex(off, offConst) && offConst == 0;
    bool isDefaultSize =
        ValueUtils::getConstantIndex(size, sizeConst) && sizeConst == 1;
    return !(isDefaultOffset && isDefaultSize);
  };

  if (!partitionOffset && !mutableAcquire.getOffsets().empty() &&
      !mutableAcquire.getSizes().empty() &&
      hasMeaningfulSlice(mutableAcquire.getOffsets().front(),
                         mutableAcquire.getSizes().front())) {
    partitionOffset = mutableAcquire.getOffsets().front();
    partitionSize = mutableAcquire.getSizes().front();
  }

  /// If this acquire is not associated with an EDT, allow full-range usage.
  if (!edtUserOp)
    return true;

  /// Validate access pattern compatibility with partition offset.
  /// For fine-grained fallback (element-wise partitioning), skip this validation
  /// since element-wise partitioning doesn't need offset-based chunking.
  if (partitionOffset && !canPartitionWithOffset(partitionOffset)) {
    if (!offsetHints.empty()) {
      ARTS_DEBUG("Partition offset not derived from access; "
                 "continuing with offset_hints");
    } else if (useFineGrainedFallback) {
      ARTS_DEBUG("Partition offset not derived from access; "
                 "using fine-grained fallback (element-wise partitioning)");
    } else {
      return skip("first index of memory access not derived from partition "
                  "offset");
    }
  }

  /// If no partition hints, nothing more to validate
  if (!partitionOffset || !partitionSize)
    return true;

  /// Validate partition hints and compute adjusted chunk info for stencil
  /// patterns (A[i-1], A[i], A[i+1]) and uniform access with offset (B[i+1]).
  originalBounds_ = std::make_pair(partitionOffset, partitionSize);
  ArtsAnalysisManager &AM = analysis->getAnalysisManager();
  LoopAnalysis &loopAnalysis = AM.getLoopAnalysis();

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);

  /// Find the first dynamic index from memory operations
  Value firstDynIdx;
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    ValueRange dbRefIndices = dbRef.getIndices();
    for (Operation *memOp : memOps) {
      ValueRange memIndices;
      if (auto load = dyn_cast<memref::LoadOp>(memOp))
        memIndices = load.getIndices();
      else if (auto store = dyn_cast<memref::StoreOp>(memOp))
        memIndices = store.getIndices();
      else
        continue;

      SmallVector<Value> fullChain;
      fullChain.append(dbRefIndices.begin(), dbRefIndices.end());
      fullChain.append(memIndices.begin(), memIndices.end());

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
      computedChunkInfo = std::make_pair(partitionOffset, partitionSize);
      return true;
    }
    if (useFineGrainedFallback) {
      ARTS_DEBUG("  No dynamic index but using fine-grained fallback");
      return true;
    }
    return skip("no dynamic index found in memory accesses");
  }

  EdtOp edt = getEdtUser();
  SmallVector<scf::ForOp, 4> forLoops;
  edt.walk([&](scf::ForOp forOp) {
    forLoops.push_back(forOp);
    return WalkResult::advance();
  });

  if (forLoops.empty()) {
    if (useFineGrainedFallback) {
      ARTS_DEBUG("  No for loops but using fine-grained fallback");
      return true;
    }
    return skip("partition hints require chunk loop for validation");
  }

  /// Find the loop whose IV the first dynamic index depends on.
  auto loopDepth = [](scf::ForOp loop) -> int {
    int depth = 0;
    for (Operation *parent = loop->getParentOp(); parent;
         parent = parent->getParentOp()) {
      if (isa<scf::ForOp>(parent))
        ++depth;
    }
    return depth;
  };

  scf::ForOp chunkLoop;
  int bestDepth = -1;
  bool foundPreferred = false;

  for (scf::ForOp forOp : forLoops) {
    LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp);
    if (!loopNode)
      continue;

    if (!loopNode->dependsOnInductionVar(firstDynIdx))
      continue;
    if (!offsetIsZero && !ValueUtils::dependsOn(firstDynIdx, partitionOffset))
      continue;

    Value loopIV = forOp.getInductionVar();
    bool offsetDependsOnIV = !offsetIsZero && partitionOffset &&
                             ValueUtils::dependsOn(partitionOffset, loopIV);
    bool isPreferred = !offsetDependsOnIV;
    int depth = loopDepth(forOp);

    if (isPreferred) {
      if (!foundPreferred || depth > bestDepth) {
        foundPreferred = true;
        bestDepth = depth;
        chunkLoop = forOp;
      }
    } else if (!foundPreferred && depth > bestDepth) {
      bestDepth = depth;
      chunkLoop = forOp;
    }
  }

  if (!chunkLoop) {
    if (useFineGrainedFallback) {
      /// For fine-grained fallback, we don't need loop-based chunk analysis
      /// since element-wise partitioning handles each element individually.
      ARTS_DEBUG("  No chunk loop found but using fine-grained fallback");
      return true;
    }
    return skip(
        offsetIsZero
            ? "no loop found where index depends on IV"
            : "no loop found where index depends on both IV and offset");
  }

  Value loopIV = chunkLoop.getInductionVar();
  AccessBoundsInfo bounds = analyzeAccessBounds(this, partitionOffset, loopIV);

  if (!bounds.valid) {
    if (useFineGrainedFallback) {
      /// For fine-grained fallback (element-wise partitioning), skip detailed
      /// bounds analysis. Element-wise partitioning doesn't need precise chunk
      /// bounds since each element is acquired individually.
      ARTS_DEBUG("  Access bounds analysis failed but using fine-grained "
                 "fallback (element-wise partitioning)");
      return true;
    }
    return skip("access bounds analysis failed");
  }

  ARTS_DEBUG("  Bounds analysis for acquire "
             << dbAcquireOp << ": minOffset=" << bounds.minOffset
             << ", maxOffset=" << bounds.maxOffset
             << ", isStencil=" << bounds.isStencil);

  /// Store stencil bounds for later use by DbRewriter
  StencilBounds sb;
  sb.minOffset = bounds.minOffset;
  sb.maxOffset = bounds.maxOffset;
  sb.isStencil = bounds.isStencil;
  sb.valid = bounds.valid;
  stencilBounds_ = sb;

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  Value adjustedOffset = partitionOffset;
  Value adjustedSize = partitionSize;

  if (bounds.minOffset < 0) {
    /// Clamp to zero: max(partitionOffset - |minOffset|, 0)
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

  computedChunkInfo = std::make_pair(adjustedOffset, adjustedSize);
  return true;
}

/// Check if this acquire can be partitioned. Validates EDT/access requirements,
/// extracts partition bounds, and recursively checks nested children.
bool DbAcquireNode::canBePartitioned(bool useFineGrainedFallback) {
  if (!edtUserOp)
    return false;

  if (!hasValidEdtAndAccesses(useFineGrainedFallback))
    return false;

  if (!computePartitionBounds(useFineGrainedFallback))
    return false;

  /// Step 3: Recursively check nested acquire children
  for (const auto &childAcq : childAcquires) {
    if (childAcq && !childAcq->canBePartitioned(useFineGrainedFallback)) {
      ARTS_DEBUG("Skipping acquire " << dbAcquireOp
                                     << ": nested acquire child failed");
      return false;
    }
  }

  ARTS_DEBUG("PASS: acquire can be partitioned: " << dbAcquireOp);
  return true;
}

AccessPattern DbAcquireNode::getAccessPattern() const {
  if (accessPattern_)
    return *accessPattern_;

  /// Stencil classification requires bounds analysis to have run.
  if (stencilBounds_ && stencilBounds_->valid && stencilBounds_->isStencil) {
    accessPattern_ = AccessPattern::Stencil;
    return *accessPattern_;
  }

  /// Explicit indices imply element-wise or irregular access.
  DbAcquireOp acqOp = const_cast<DbAcquireOp &>(dbAcquireOp);
  if (!acqOp.getIndices().empty()) {
    accessPattern_ = AccessPattern::Indexed;
    return *accessPattern_;
  }

  /// Default to uniform access for offset-based or coarse acquires.
  accessPattern_ = AccessPattern::Uniform;
  return *accessPattern_;
}

/// Analyze memory accesses to compute bounds relative to chunk base.
static AccessBoundsInfo analyzeAccessBounds(DbAcquireNode *node,
                                            Value chunkOffset, Value loopIV) {
  AccessBoundsInfo bounds;
  bounds.minOffset = std::numeric_limits<int64_t>::max();
  bounds.maxOffset = std::numeric_limits<int64_t>::min();

  if (!node || !chunkOffset || !loopIV)
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
    ValueRange dbRefIndices = dbRef.getIndices();

    for (Operation *memOp : memOps) {
      ValueRange memIndices;
      if (auto load = dyn_cast<memref::LoadOp>(memOp))
        memIndices = load.getIndices();
      else if (auto store = dyn_cast<memref::StoreOp>(memOp))
        memIndices = store.getIndices();
      else
        continue;

      /// Prefer memref indices for offset validation; fall back to db_ref
      /// indices only when no memref indices are present.
      SmallVector<Value> indexChain;
      if (!memIndices.empty())
        indexChain.append(memIndices.begin(), memIndices.end());
      else
        indexChain.append(dbRefIndices.begin(), dbRefIndices.end());

      if (indexChain.empty())
        continue;

      /// Find first dynamic index
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
          ValueUtils::extractConstantOffset(firstDynIdx, loopIV, chunkOffset);
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

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    ValueRange dbRefIndices = dbRef.getIndices();

    for (Operation *memOp : memOps) {
      ValueRange memIndices;
      if (auto load = dyn_cast<memref::LoadOp>(memOp))
        memIndices = load.getIndices();
      else if (auto store = dyn_cast<memref::StoreOp>(memOp))
        memIndices = store.getIndices();
      else
        continue;

      SmallVector<Value> fullChain;
      fullChain.append(dbRefIndices.begin(), dbRefIndices.end());
      fullChain.append(memIndices.begin(), memIndices.end());

      for (Value idx : fullChain) {
        int64_t constVal;
        if (ValueUtils::getConstantIndex(idx, constVal))
          continue;
        if (isIndirectIndex(idx))
          return true;
      }
    }
  }

  return false;
}

///===----------------------------------------------------------------------===///
// Helper methods for querying memory access
///===----------------------------------------------------------------------===///

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
    ValueRange dbRefIndices = dbRef.getIndices();

    for (Operation *memOp : memOps) {
      ValueRange memIndices;
      if (auto load = dyn_cast<memref::LoadOp>(memOp))
        memIndices = load.getIndices();
      else if (auto store = dyn_cast<memref::StoreOp>(memOp))
        memIndices = store.getIndices();
      else
        continue;

      SmallVector<Value> fullChain;
      fullChain.append(dbRefIndices.begin(), dbRefIndices.end());
      fullChain.append(memIndices.begin(), memIndices.end());

      if (fullChain.empty())
        continue;

      /// Ensure the partition offset only affects the leading dynamic
      /// dimension.
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
                       "disabling chunked partitioning");
            return false;
          }
          offsetSeen = true;
        }
      }

      /// Find first dynamic index, while rejecting indirect indices.
      Value firstDynIdx;
      for (Value idx : fullChain) {
        int64_t constVal;
        if (ValueUtils::getConstantIndex(idx, constVal))
          continue;

        if (isIndirectIndex(idx)) {
          ARTS_DEBUG("  indirect index detected; disabling chunked "
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

LogicalResult DbAcquireNode::computeChunkInfo(Value &chunkOffset,
                                              Value &chunkSize) {
  ARTS_DEBUG("computeChunkInfo for acquire: " << dbAcquireOp);

  /// Check cache first
  if (computedChunkInfo) {
    chunkOffset = computedChunkInfo->first;
    chunkSize = computedChunkInfo->second;
    ARTS_DEBUG("  using cached chunk info");
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

    chunkOffset = builder.create<arith::ConstantIndexOp>(loc, 0);
    chunkSize = allocOp.getElementSizes().front();
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
  /// Collect loops in EDT
  SmallVector<scf::ForOp> forLoops;
  scf::WhileOp chunkWhile;
  edt.walk([&](scf::ForOp forOp) {
    forLoops.push_back(forOp);
    return WalkResult::advance();
  });
  edt.walk([&](scf::WhileOp whileOp) {
    chunkWhile = whileOp;
    return WalkResult::interrupt();
  });
  if (chunkWhile)
    ARTS_DEBUG("  found scf.while at " << chunkWhile.getLoc());
  if (!forLoops.empty())
    ARTS_DEBUG("  found " << forLoops.size() << " scf.for loops in EDT");

  /// Path 1: Use offset/size hints if available
  if (partitionOffset && partitionSize)
    return computeChunkInfoFromHints(chunkOffset, chunkSize);

  /// Path 2: Try while-loop pattern
  if (chunkWhile) {
    Value offsetForCheck;
    if (succeeded(computeChunkInfoFromWhile(chunkWhile, chunkOffset, chunkSize,
                                            &offsetForCheck))) {
      Value checkOffset = offsetForCheck ? offsetForCheck : chunkOffset;
      if (!canPartitionWithOffset(checkOffset))
        return fullRangeFallback("scf.while offset not derived from access");
      return success();
    }
    ARTS_DEBUG("  computeChunkInfoFromWhile failed");
    return fullRangeFallback("scf.while chunk info");
  }

  /// Path 3: Try for-loop pattern
  /// Sort loops by nesting depth (outermost first)
  auto loopDepth = [](scf::ForOp loop) -> int {
    int depth = 0;
    for (Operation *parent = loop->getParentOp(); parent;
         parent = parent->getParentOp()) {
      if (isa<scf::ForOp>(parent))
        ++depth;
    }
    return depth;
  };
  llvm::sort(forLoops, [&](scf::ForOp a, scf::ForOp b) {
    return loopDepth(a) < loopDepth(b);
  });

  return computeChunkInfoFromForLoop(forLoops, chunkOffset, chunkSize);
}

LogicalResult DbAcquireNode::computeChunkInfoFromWhile(scf::WhileOp whileOp,
                                                       Value &chunkOffset,
                                                       Value &chunkSize,
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

  SmallVector<Value> chunkSizes;
  Value initStripped = ValueUtils::stripNumericCasts(initValue);
  for (Value bound : whileBounds) {
    Value boundStripped = ValueUtils::stripNumericCasts(bound);
    if (auto addOp = boundStripped.getDefiningOp<arith::AddIOp>()) {
      Value lhs = addOp.getLhs();
      Value rhs = addOp.getRhs();
      Value lhsStripped = ValueUtils::stripNumericCasts(lhs);
      Value rhsStripped = ValueUtils::stripNumericCasts(rhs);
      Value chunkCandidate;
      if (lhsStripped == initStripped)
        chunkCandidate = rhs;
      else if (rhsStripped == initStripped)
        chunkCandidate = lhs;

      if (chunkCandidate) {
        Value chunkDom = ValueUtils::traceValueToDominating(
            chunkCandidate, dbAcquireOp, builder, domInfo, loc);
        Value chunkIdx = ValueUtils::ensureIndexType(chunkDom, builder, loc);
        if (chunkIdx) {
          chunkSizes.push_back(chunkIdx);
          continue;
        }
      }
    }

    Value boundDom = ValueUtils::traceValueToDominating(bound, dbAcquireOp,
                                                        builder, domInfo, loc);
    Value boundIdx = ValueUtils::ensureIndexType(boundDom, builder, loc);
    if (!boundIdx)
      continue;
    chunkSizes.push_back(
        builder.create<arith::SubIOp>(loc, boundIdx, startIdx));
  }

  if (chunkSizes.empty())
    return failure();

  Value minSize = chunkSizes.front();
  for (Value candidate : llvm::drop_begin(chunkSizes))
    minSize = builder.create<arith::MinSIOp>(loc, minSize, candidate);

  Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  minSize = builder.create<arith::MaxSIOp>(loc, minSize, zero);

  AccessBoundsInfo bounds = analyzeAccessBounds(this, initValue, loopIV);
  if (!bounds.valid)
    return failure();

  if (!stencilBounds_) {
    StencilBounds sb;
    sb.minOffset = bounds.minOffset;
    sb.maxOffset = bounds.maxOffset;
    sb.isStencil = bounds.isStencil;
    sb.valid = bounds.valid;
    stencilBounds_ = sb;
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

  chunkOffset = adjustedOffset;
  chunkSize = adjustedSize;
  return success();
}

LogicalResult DbAcquireNode::computeChunkInfoFromHints(Value &chunkOffset,
                                                       Value &chunkSize) {
  if (!partitionOffset || !partitionSize)
    return failure();

  ARTS_DEBUG("  computeChunkInfoFromHints: " << partitionOffset << " / "
                                             << partitionSize);
  if (!originalBounds_)
    originalBounds_ = std::make_pair(partitionOffset, partitionSize);

  if (!canPartitionWithOffset(partitionOffset)) {
    ARTS_DEBUG("  offset hints not derived from access; failing");
    return failure();
  }

  Value firstDynIdx;
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    ValueRange dbRefIndices = dbRef.getIndices();
    for (Operation *memOp : memOps) {
      ValueRange memIndices;
      if (auto load = dyn_cast<memref::LoadOp>(memOp))
        memIndices = load.getIndices();
      else if (auto store = dyn_cast<memref::StoreOp>(memOp))
        memIndices = store.getIndices();
      else
        continue;

      SmallVector<Value> fullChain;
      fullChain.append(dbRefIndices.begin(), dbRefIndices.end());
      fullChain.append(memIndices.begin(), memIndices.end());

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
      chunkOffset = partitionOffset;
      chunkSize = partitionSize;
      return success();
    }
    return failure();
  }

  EdtOp edt = getEdtUser();
  SmallVector<scf::ForOp> forLoops;
  edt.walk([&](scf::ForOp forOp) {
    forLoops.push_back(forOp);
    return WalkResult::advance();
  });

  scf::ForOp boundsLoop;
  auto loopDepth = [](scf::ForOp loop) -> int {
    int depth = 0;
    for (Operation *parent = loop->getParentOp(); parent;
         parent = parent->getParentOp()) {
      if (isa<scf::ForOp>(parent))
        ++depth;
    }
    return depth;
  };
  int bestDepth = -1;
  bool foundPreferred = false;

  for (scf::ForOp loop : forLoops) {
    LoopNode *loopNode =
        analysis->getAnalysisManager().getLoopAnalysis().getOrCreateLoopNode(
            loop);
    if (!loopNode)
      continue;

    if (!loopNode->dependsOnInductionVar(firstDynIdx))
      continue;
    if (!offsetIsZero && !ValueUtils::dependsOn(firstDynIdx, partitionOffset))
      continue;

    Value loopIV = loop.getInductionVar();
    bool offsetDependsOnIV = !offsetIsZero && partitionOffset &&
                             ValueUtils::dependsOn(partitionOffset, loopIV);
    bool isPreferred = !offsetDependsOnIV;
    int depth = loopDepth(loop);

    if (isPreferred) {
      if (!foundPreferred || depth > bestDepth) {
        foundPreferred = true;
        bestDepth = depth;
        boundsLoop = loop;
      }
    } else if (!foundPreferred && depth > bestDepth) {
      bestDepth = depth;
      boundsLoop = loop;
    }
  }

  if (boundsLoop) {
    Value loopIV = boundsLoop.getInductionVar();
    AccessBoundsInfo bounds =
        analyzeAccessBounds(this, partitionOffset, loopIV);

    if (bounds.valid) {
      /// Store stencil bounds for DbRewriter
      if (!stencilBounds_) {
        StencilBounds sb;
        sb.minOffset = bounds.minOffset;
        sb.maxOffset = bounds.maxOffset;
        sb.isStencil = bounds.isStencil;
        sb.valid = bounds.valid;
        stencilBounds_ = sb;
      }

      OpBuilder builder(dbAcquireOp);
      Location loc = dbAcquireOp.getLoc();
      builder.setInsertionPoint(dbAcquireOp);

      if (bounds.minOffset != 0) {
        Value adjustment =
            builder.create<arith::ConstantIndexOp>(loc, bounds.minOffset);
        chunkOffset =
            builder.create<arith::AddIOp>(loc, partitionOffset, adjustment);
      } else {
        chunkOffset = partitionOffset;
      }

      int64_t sizeAdjustment = bounds.maxOffset - bounds.minOffset;
      if (sizeAdjustment != 0) {
        Value adjustment =
            builder.create<arith::ConstantIndexOp>(loc, sizeAdjustment);
        chunkSize =
            builder.create<arith::AddIOp>(loc, partitionSize, adjustment);
      } else {
        chunkSize = partitionSize;
      }

      return success();
    }
  }

  chunkOffset = partitionOffset;
  chunkSize = partitionSize;
  return success();
}

LogicalResult
DbAcquireNode::computeChunkInfoFromForLoop(ArrayRef<scf::ForOp> forLoops,
                                           Value &chunkOffset, Value &chunkSize,
                                           Value *offsetForCheck) {
  Value ptrArg = getUseInEdt();
  if (!ptrArg) {
    ARTS_DEBUG("  computeChunkInfoFromForLoop: no EDT block argument");
    return failure();
  }

  if (forLoops.empty()) {
    ARTS_DEBUG("  computeChunkInfoFromForLoop: no scf.for found");
    return failure();
  }

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  LoopAnalysis &loopAnalysis = analysis->getAnalysisManager().getLoopAnalysis();

  for (scf::ForOp loop : forLoops) {
    Value offsetCandidate;
    LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(loop);
    if (!loopNode)
      continue;
    Value loopIV = loopNode->getInductionVar();
    auto dependsOnIV = [&](Value v) -> bool {
      return loopNode->dependsOnInductionVarNormalized(v);
    };
    auto isInvariant = [&](Value v) -> bool {
      return loopNode->isValueLoopInvariant(v);
    };
    auto pickCandidateFromIndex = [&](Value idx) -> Value {
      Value stripped = ValueUtils::stripNumericCasts(idx);
      if (stripped == loopIV)
        return builder.create<arith::ConstantIndexOp>(loc, 0);

      if (auto addOp = stripped.getDefiningOp<arith::AddIOp>()) {
        Value lhs = addOp.getLhs();
        Value rhs = addOp.getRhs();
        bool lhsIV = dependsOnIV(lhs);
        bool rhsIV = dependsOnIV(rhs);
        if (lhsIV && !rhsIV && isInvariant(rhs))
          return rhs;
        if (rhsIV && !lhsIV && isInvariant(lhs))
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

    if (!stencilBounds_) {
      StencilBounds sb;
      sb.minOffset = bounds.minOffset;
      sb.maxOffset = bounds.maxOffset;
      sb.isStencil = bounds.isStencil;
      sb.valid = bounds.valid;
      stencilBounds_ = sb;
    }

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

    chunkOffset = adjustedOffset;
    chunkSize = adjustedSize;
    if (offsetForCheck)
      *offsetForCheck = offsetCandidate;
    ARTS_DEBUG("  selected loop " << loop.getLoc() << " chunkOffset="
                                  << chunkOffset << " chunkSize=" << chunkSize);

    Value checkOffset = offsetForCheck ? *offsetForCheck : chunkOffset;
    if (!canPartitionWithOffset(checkOffset))
      return failure();
    return success();
  }

  ARTS_DEBUG("  no suitable scf.for loop found for chunk info");
  return failure();
}

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << estimatedBytes;
  os << "\n";
}

///===----------------------------------------------------------------------===///
// Twin-diff overlap analysis methods
///===----------------------------------------------------------------------===///

bool DbAcquireNode::isWorkerIndexedAccess() const {
  if (!op)
    return false;

  DbAcquireOp mutableOp(op);
  auto offsets = mutableOp.getOffsetHints();
  auto sizes = mutableOp.getSizeHints();

  if (offsets.empty() || sizes.empty())
    return false;

  int64_t sz = 0;
  if (!ValueUtils::getConstantIndex(sizes[0], sz) || sz != 1)
    return false;

  /// Offset is a BlockArgument means each invocation gets different offset
  return isa<BlockArgument>(offsets[0]);
}

bool DbAcquireNode::hasDisjointPartitionWith(const DbAcquireNode *other) const {
  if (!other)
    return false;

  DbAcquireOp acqA = dbAcquireOp;
  DbAcquireOp acqB = other->getDbAcquireOp();
  if (!acqA || !acqB)
    return false;

  auto offsetsA = acqA.getOffsetHints();
  auto sizesA = acqA.getSizeHints();
  auto offsetsB = acqB.getOffsetHints();
  auto sizesB = acqB.getSizeHints();

  if (offsetsA.empty() || sizesA.empty() || offsetsB.empty() || sizesB.empty())
    return false;

  Value offA = offsetsA[0];
  Value sizeA = sizesA[0];
  Value offB = offsetsB[0];
  Value sizeB = sizesB[0];

  /// Check constant range disjointness
  int64_t offAVal = 0, sizeAVal = 0, offBVal = 0, sizeBVal = 0;
  if (ValueUtils::getConstantIndex(offA, offAVal) &&
      ValueUtils::getConstantIndex(sizeA, sizeAVal) &&
      ValueUtils::getConstantIndex(offB, offBVal) &&
      ValueUtils::getConstantIndex(sizeB, sizeBVal)) {
    int64_t endA = offAVal + sizeAVal;
    int64_t endB = offBVal + sizeBVal;
    if (endA <= offBVal || endB <= offAVal)
      return true;
  }

  /// Check worker-indexed pattern: different BlockArguments with size=1
  int64_t sizeAConst = 0, sizeBConst = 0;
  bool sizeAIsOne =
      ValueUtils::getConstantIndex(sizeA, sizeAConst) && sizeAConst == 1;
  bool sizeBIsOne =
      ValueUtils::getConstantIndex(sizeB, sizeBConst) && sizeBConst == 1;

  if (sizeAIsOne && sizeBIsOne) {
    auto blockArgA = dyn_cast<BlockArgument>(offA);
    auto blockArgB = dyn_cast<BlockArgument>(offB);
    if (blockArgA && blockArgB && blockArgA != blockArgB &&
        blockArgA.getOwner() != blockArgB.getOwner())
      return true;
  }

  return false;
}
