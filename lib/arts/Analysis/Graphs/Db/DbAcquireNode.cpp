///==========================================================================///
/// DbAcquireNode.cpp - Implementation of DbAcquire node
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

/// Check if a value is probably derived from base via arithmetic operations.
/// Recursively walks through add/mul/sub operations to find if base is
/// transitively reachable. This handles complex index computations like:
///   globalIdx = lowerBound + (chunkOffset + localIter) * step
/// where we want to check if globalIdx is derived from chunkOffset.

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

static Value traceValueToDominating(Value value, Operation *insertBefore,
                                    OpBuilder &builder, DominanceInfo &domInfo,
                                    Location loc, unsigned depth = 0) {
  if (!value || depth > 16)
    return nullptr;

  if (domInfo.properlyDominates(value, insertBefore))
    return value;

  if (auto blockArg = value.dyn_cast<BlockArgument>())
    return nullptr;

  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return nullptr;

  auto trace = [&](Value operand) -> Value {
    return traceValueToDominating(operand, insertBefore, builder, domInfo, loc,
                                  depth + 1);
  };

  builder.setInsertionPoint(insertBefore);

  if (auto addOp = dyn_cast<arith::AddIOp>(defOp)) {
    Value lhs = trace(addOp.getLhs());
    Value rhs = trace(addOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::AddIOp>(loc, lhs, rhs);
  }

  if (auto subOp = dyn_cast<arith::SubIOp>(defOp)) {
    Value lhs = trace(subOp.getLhs());
    Value rhs = trace(subOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::SubIOp>(loc, lhs, rhs);
  }

  if (auto mulOp = dyn_cast<arith::MulIOp>(defOp)) {
    Value lhs = trace(mulOp.getLhs());
    Value rhs = trace(mulOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::MulIOp>(loc, lhs, rhs);
  }

  if (auto divOp = dyn_cast<arith::DivSIOp>(defOp)) {
    Value lhs = trace(divOp.getLhs());
    Value rhs = trace(divOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::DivSIOp>(loc, lhs, rhs);
  }

  if (auto divOp = dyn_cast<arith::DivUIOp>(defOp)) {
    Value lhs = trace(divOp.getLhs());
    Value rhs = trace(divOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::DivUIOp>(loc, lhs, rhs);
  }

  if (auto divOp = dyn_cast<arith::CeilDivSIOp>(defOp)) {
    Value lhs = trace(divOp.getLhs());
    Value rhs = trace(divOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::CeilDivSIOp>(loc, lhs, rhs);
  }

  if (auto divOp = dyn_cast<arith::CeilDivUIOp>(defOp)) {
    Value lhs = trace(divOp.getLhs());
    Value rhs = trace(divOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::CeilDivUIOp>(loc, lhs, rhs);
  }

  if (auto maxOp = dyn_cast<arith::MaxSIOp>(defOp)) {
    Value lhs = trace(maxOp.getLhs());
    Value rhs = trace(maxOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::MaxSIOp>(loc, lhs, rhs);
  }

  if (auto minOp = dyn_cast<arith::MinSIOp>(defOp)) {
    Value lhs = trace(minOp.getLhs());
    Value rhs = trace(minOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::MinSIOp>(loc, lhs, rhs);
  }

  if (auto minOp = dyn_cast<arith::MinUIOp>(defOp)) {
    Value lhs = trace(minOp.getLhs());
    Value rhs = trace(minOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::MinUIOp>(loc, lhs, rhs);
  }

  if (auto maxOp = dyn_cast<arith::MaxUIOp>(defOp)) {
    Value lhs = trace(maxOp.getLhs());
    Value rhs = trace(maxOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::MaxUIOp>(loc, lhs, rhs);
  }

  if (auto remOp = dyn_cast<arith::RemSIOp>(defOp)) {
    Value lhs = trace(remOp.getLhs());
    Value rhs = trace(remOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::RemSIOp>(loc, lhs, rhs);
  }

  if (auto remOp = dyn_cast<arith::RemUIOp>(defOp)) {
    Value lhs = trace(remOp.getLhs());
    Value rhs = trace(remOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::RemUIOp>(loc, lhs, rhs);
  }

  if (auto cmpOp = dyn_cast<arith::CmpIOp>(defOp)) {
    Value lhs = trace(cmpOp.getLhs());
    Value rhs = trace(cmpOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::CmpIOp>(loc, cmpOp.getPredicate(), lhs, rhs);
  }

  if (auto andOp = dyn_cast<arith::AndIOp>(defOp)) {
    Value lhs = trace(andOp.getLhs());
    Value rhs = trace(andOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::AndIOp>(loc, lhs, rhs);
  }

  if (auto orOp = dyn_cast<arith::OrIOp>(defOp)) {
    Value lhs = trace(orOp.getLhs());
    Value rhs = trace(orOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::OrIOp>(loc, lhs, rhs);
  }

  if (auto xorOp = dyn_cast<arith::XOrIOp>(defOp)) {
    Value lhs = trace(xorOp.getLhs());
    Value rhs = trace(xorOp.getRhs());
    if (lhs && rhs)
      return builder.create<arith::XOrIOp>(loc, lhs, rhs);
  }

  if (auto selectOp = dyn_cast<arith::SelectOp>(defOp)) {
    Value cond = trace(selectOp.getCondition());
    Value trueVal = trace(selectOp.getTrueValue());
    Value falseVal = trace(selectOp.getFalseValue());
    if (cond && trueVal && falseVal)
      return builder.create<arith::SelectOp>(loc, cond, trueVal, falseVal);
  }

  if (auto castOp = dyn_cast<arith::IndexCastOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    if (inner.getType() == castOp.getType())
      return inner;
    return builder.create<arith::IndexCastOp>(loc, castOp.getType(), inner);
  }

  if (auto castOp = dyn_cast<arith::IndexCastUIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (!inner)
      return nullptr;
    if (inner.getType() == castOp.getType())
      return inner;
    return builder.create<arith::IndexCastUIOp>(loc, castOp.getType(), inner);
  }

  if (auto castOp = dyn_cast<arith::ExtSIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (inner)
      return builder.create<arith::ExtSIOp>(loc, castOp.getType(), inner);
  }

  if (auto castOp = dyn_cast<arith::ExtUIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (inner)
      return builder.create<arith::ExtUIOp>(loc, castOp.getType(), inner);
  }

  if (auto castOp = dyn_cast<arith::TruncIOp>(defOp)) {
    Value inner = trace(castOp.getIn());
    if (inner)
      return builder.create<arith::TruncIOp>(loc, castOp.getType(), inner);
  }

  if (auto constOp = dyn_cast<arith::ConstantOp>(defOp))
    return builder.create<arith::ConstantOp>(loc, constOp.getType(),
                                             constOp.getValue());
  if (auto constIdxOp = dyn_cast<arith::ConstantIndexOp>(defOp))
    return builder.create<arith::ConstantIndexOp>(loc, constIdxOp.value());

  return nullptr;
}

/// Extract the constant offset from an index expression.
/// Tries to express idx as: chunkOffset + loopIV + constant

/// Analyze all memory accesses to compute actual bounds relative to chunk base.
/// For stencil patterns like A[i-1], A[i], A[i+1], this extracts the min/max
/// offsets. For uniform access like B[i+1], this extracts the constant
/// adjustment.
static AccessBoundsInfo analyzeAccessBounds(DbAcquireNode *node,
                                            Value chunkOffset, Value loopIV);

/// canBePartitioned - Check if this acquire can be partitioned.
///
/// This is called by DbAllocNode::canBePartitioned() for hierarchical
/// validation. The check includes:
/// 1. Must be used by a task EDT (not broadcast/reduce)
/// 2. Must have memory accesses (loads/stores)
/// 3. Parent allocation must be parallel-friendly
/// 4. EDT must contain parallel loop metadata
/// 5. Access pattern must be compatible with partition offset:
///    - First dynamic index of [DbRefOp indices + load/store indices]
///      must be derived from the partition offset
/// 6. Recursively validates ALL nested acquire children
///
/// Returns true only if this acquire AND all its nested children pass.
bool DbAcquireNode::canBePartitioned() {
  /// Early check: if no EDT user, this acquire can't be partitioned
  /// (happens for nested acquires where parent acquire is consumed by child)
  if (!edtUserOp)
    return false;

  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << dbAcquireOp << ": " << reason);
    return false;
  };

  /// Step 1: Basic validation
  if (!dbAcquireOp)
    return skip("invalid acquire operation");

  EdtOp edt = getEdtUser();
  if (!edt || !edt.getOperation() || edt.getType() != EdtType::task)
    return skip("not a task EDT");

  if (!hasMemoryAccesses())
    return skip("no memory accesses");

  DbAllocNode *allocNode = getRootAlloc();
  if (!allocNode)
    return skip("missing allocation node");

  /// Note: Non-uniform access (stencil patterns) is allowed if we can analyze
  /// the access bounds. The check is done later after extracting partition
  /// info. See Level 2 stencil-aware partitioning in analysis.md.

  if (!allocNode->isParallelFriendly())
    return skip("memref metadata is not marked as parallel-friendly");

  /// Step 2: EDT and loop metadata validation
  ArtsAnalysisManager &AM = analysis->getAnalysisManager();
  EdtAnalysis &edtAnalysis = AM.getEdtAnalysis();

  func::FuncOp func = edt->getParentOfType<func::FuncOp>();
  if (!func)
    return skip("unable to find parent function");

  EdtGraph &edtGraph = edtAnalysis.getOrCreateEdtGraph(func);
  EdtNode *edtNode = edtGraph.getEdtNode(edt);
  if (!edtNode)
    return skip("missing EDT node");

  if (!edtNode->hasParallelLoopMetadata())
    return skip("worker EDT does not contain parallel loop metadata");

  /// Step 3: Partition offset/size extraction and validation
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

  /// Validate access pattern compatibility with partition offset.
  /// An acquire is only eligible if its memory access first index
  /// is derived from the partition offset (for chunked parallel access).
  if (partitionOffset) {
    if (!canPartitionWithOffset(partitionOffset))
      return skip("first index of memory access not derived from partition "
                  "offset");
  }

  /// Validate partition hints and compute adjusted chunk info for stencil
  /// patterns (A[i-1], A[i], A[i+1]) and uniform access with offset (B[i+1]).
  if (partitionOffset && partitionSize) {
    originalBounds_ = std::make_pair(partitionOffset, partitionSize);
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

    if (!firstDynIdx)
      return skip("no dynamic index found in memory accesses");

    SmallVector<scf::ForOp, 4> forLoops;
    edt.walk([&](scf::ForOp forOp) {
      forLoops.push_back(forOp);
      return WalkResult::advance();
    });

    if (forLoops.empty())
      return skip("partition hints require chunk loop for validation");

    /// Find the loop whose IV the first dynamic index depends on
    scf::ForOp chunkLoop;
    for (scf::ForOp forOp : forLoops) {
      LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(forOp);
      if (!loopNode)
        continue;

      if (loopNode->dependsOnInductionVar(firstDynIdx) &&
          ValueUtils::dependsOn(firstDynIdx, partitionOffset)) {
        chunkLoop = forOp;
        break;
      }
    }

    if (!chunkLoop)
      return skip("no loop found where index depends on both IV and offset");

    Value loopIV = chunkLoop.getInductionVar();
    AccessBoundsInfo bounds =
        analyzeAccessBounds(this, partitionOffset, loopIV);

    if (!bounds.valid)
      return skip("access bounds analysis failed");

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
  }

  /// Recursively check nested acquire children
  for (const auto &childAcq : childAcquires) {
    if (childAcq && !childAcq->canBePartitioned())
      return skip("nested acquire child failed canBePartitioned()");
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

      SmallVector<Value> fullChain;
      fullChain.append(dbRefIndices.begin(), dbRefIndices.end());
      fullChain.append(memIndices.begin(), memIndices.end());

      if (fullChain.empty())
        continue;

      /// Find first dynamic index
      Value firstDynIdx;
      for (Value idx : fullChain) {
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

///===----------------------------------------------------------------------===///
// Helper methods for querying memory access counts
///===----------------------------------------------------------------------===///

bool DbAcquireNode::hasLoads() {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *op : memOps) {
      if (isa<memref::LoadOp>(op))
        return true;
    }
  }
  return false;
}

bool DbAcquireNode::hasStores() {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *op : memOps) {
      if (isa<memref::StoreOp>(op))
        return true;
    }
  }
  return false;
}

bool DbAcquireNode::hasMemoryAccesses() {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    if (!memOps.empty())
      return true;
  }
  return false;
}

size_t DbAcquireNode::countLoads() {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  size_t count = 0;
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *op : memOps) {
      if (isa<memref::LoadOp>(op))
        ++count;
    }
  }
  return count;
}

size_t DbAcquireNode::countStores() {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);
  size_t count = 0;
  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *op : memOps) {
      if (isa<memref::StoreOp>(op))
        ++count;
    }
  }
  return count;
}

bool DbAcquireNode::canPartitionWithOffset(Value offset) {
  if (!offset)
    return false;
  Value offsetStripped = ValueUtils::stripNumericCasts(offset);

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

      /// Find first dynamic index
      Value firstDynIdx;
      for (Value idx : fullChain) {
        int64_t constVal;
        if (!ValueUtils::getConstantIndex(idx, constVal)) {
          firstDynIdx = idx;
          break;
        }
      }

      if (!firstDynIdx)
        continue;

      if (!LoopNode::dependsOnLoopInitNormalized(firstDynIdx, offsetStripped))
        return false;
    }
  }

  return true;
}

LogicalResult DbAcquireNode::computeChunkInfo(Value &chunkOffset,
                                              Value &chunkSize) {
  ARTS_DEBUG("computeChunkInfo for acquire: " << dbAcquireOp);
  if (computedChunkInfo) {
    chunkOffset = computedChunkInfo->first;
    chunkSize = computedChunkInfo->second;
    ARTS_DEBUG("  using cached chunk info");
    return success();
  }

  EdtOp edt = getEdtUser();
  if (!edt) {
    ARTS_DEBUG("  no EDT user found");
    return failure();
  }

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

  if (partitionOffset && partitionSize) {
    ARTS_DEBUG("  using offset/size hints: " << partitionOffset << " / "
                                             << partitionSize);
    if (!originalBounds_)
      originalBounds_ = std::make_pair(partitionOffset, partitionSize);
    if (!canPartitionWithOffset(partitionOffset))
      return failure();

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

    scf::ForOp boundsLoop;
    if (firstDynIdx) {
      for (scf::ForOp loop : forLoops) {
        Value loopIV = loop.getInductionVar();
        if (ValueUtils::dependsOn(firstDynIdx, loopIV) &&
            ValueUtils::dependsOn(firstDynIdx, partitionOffset)) {
          boundsLoop = loop;
          break;
        }
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

  if (chunkWhile) {
    Value offsetForCheck;
    if (succeeded(computeChunkInfoFromWhile(chunkWhile, chunkOffset, chunkSize,
                                            &offsetForCheck))) {
      Value checkOffset = offsetForCheck ? offsetForCheck : chunkOffset;
      if (!canPartitionWithOffset(checkOffset))
        return failure();
      return success();
    }
    ARTS_DEBUG("  computeChunkInfoFromWhile failed");
    return failure();
  }

  Value ptrArg = getUseInEdt();
  if (!ptrArg) {
    ARTS_DEBUG("  no EDT block argument for acquire");
    return failure();
  }

  if (forLoops.empty()) {
    ARTS_DEBUG("  no scf.for found in EDT");
    return failure();
  }

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

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  builder.setInsertionPoint(dbAcquireOp);

  LoopAnalysis &loopAnalysis = analysis->getAnalysisManager().getLoopAnalysis();

  Value offsetForCheck;
  auto tryComputeFromFor = [&](scf::ForOp loop) -> LogicalResult {
    Value offsetCandidate;
    LoopNode *loopNode = loopAnalysis.getOrCreateLoopNode(loop);
    if (!loopNode)
      return failure();
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
      return failure();
    }

    AccessBoundsInfo bounds =
        analyzeAccessBounds(this, offsetCandidate, loopIV);
    if (!bounds.valid) {
      ARTS_DEBUG("  bounds analysis failed for loop at " << loop.getLoc());
      return failure();
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
      return failure();
    DominanceInfo domInfo(func);

    Value offsetDom = traceValueToDominating(offsetCandidate, dbAcquireOp,
                                             builder, domInfo, loc);
    Value sizeDom = traceValueToDominating(loop.getUpperBound(), dbAcquireOp,
                                           builder, domInfo, loc);
    if (!offsetDom || !sizeDom) {
      ARTS_DEBUG("  offset/size does not dominate for loop at "
                 << loop.getLoc());
      return failure();
    }

    Value adjustedOffset = ValueUtils::ensureIndexType(offsetDom, builder, loc);
    Value adjustedSize = ValueUtils::ensureIndexType(sizeDom, builder, loc);
    if (!adjustedOffset || !adjustedSize)
      return failure();

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
    offsetForCheck = offsetCandidate;
    ARTS_DEBUG("  selected loop " << loop.getLoc() << " chunkOffset="
                                  << chunkOffset << " chunkSize=" << chunkSize);
    return success();
  };

  for (scf::ForOp loop : forLoops) {
    if (succeeded(tryComputeFromFor(loop))) {
      Value checkOffset = offsetForCheck ? offsetForCheck : chunkOffset;
      if (!canPartitionWithOffset(checkOffset))
        return failure();
      return success();
    }
  }

  ARTS_DEBUG("  no suitable scf.for loop found for chunk info");
  return failure();
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

  Value initDom =
      traceValueToDominating(initValue, dbAcquireOp, builder, domInfo, loc);
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
        Value chunkDom = traceValueToDominating(chunkCandidate, dbAcquireOp,
                                                builder, domInfo, loc);
        Value chunkIdx = ValueUtils::ensureIndexType(chunkDom, builder, loc);
        if (chunkIdx) {
          chunkSizes.push_back(chunkIdx);
          continue;
        }
      }
    }

    Value boundDom =
        traceValueToDominating(bound, dbAcquireOp, builder, domInfo, loc);
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

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << estimatedBytes;
  os << "\n";
}

/// Compute a maximal prefix of invariant indices that are uniform across the
/// EDT. These can be used to coarsen the acquire by pinning leading dimensions.
SmallVector<Value, 4> DbAcquireNode::computeInvariantIndices() {
  SmallVector<Value, 4> result;
  const size_t rank = dbAcquireOp.getSizes().size();

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);

  SmallVector<Value, 4> candidate(rank, Value());
  SmallVector<bool, 4> invalid(rank, false);

  EdtOp edtUser = getEdtUser();
  if (!edtUser)
    return result;
  auto &edtRegion = edtUser.getBody();

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    if (!edtRegion.isAncestor(dbRef->getParentRegion()))
      continue;

    ValueRange idxs = dbRef.getIndices();
    auto minRank = std::min<size_t>(rank, idxs.size());

    for (size_t d = 0; d < minRank; ++d) {
      if (invalid[d])
        continue;
      Value idxV = idxs[d];
      if (!candidate[d])
        candidate[d] = idxV;
      else if (candidate[d] != idxV)
        invalid[d] = true;
    }
  }

  for (size_t d = 0; d < rank; ++d) {
    if (!candidate[d] || invalid[d])
      break;
    if (!EdtUtils::isInvariantInEdt(edtRegion, candidate[d]))
      break;
    result.push_back(candidate[d]);
  }

  return result;
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
