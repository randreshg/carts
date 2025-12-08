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
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseMap.h"
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
    if (!arts::getConstantIndex(v, cst)) {
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
  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(dbAcquireOp);
  edtUser = edt;
  useInEdt = blockArg;

  if (!edtUser || !useInEdt) {
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
  collectAccesses(useInEdt);
  if (loads.size() > 0) {
    inCount = 1;
    outCount = 0;
  } else if (stores.size() > 0) {
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

bool DbAcquireNode::isEligibleForSlicing() {
  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("Skipping acquire " << dbAcquireOp << ": " << reason);
    return false;
  };

  if (!dbAcquireOp)
    return skip("invalid acquire operation");

  EdtOp edt = getEdtUser();
  if (!edt || edt.getType() != EdtType::task)
    return skip("not a task EDT");

  if (loads.empty() && stores.empty())
    return skip("no memory accesses");

  DbAllocNode *allocNode = getRootAlloc();
  if (!allocNode)
    return skip("missing allocation node");

  if (allocNode->hasUniformAccess && !*allocNode->hasUniformAccess)
    return skip("non-uniform access");

  if (!allocNode->isParallelFriendly())
    return skip("memref metadata is not marked as parallel-friendly");

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
        arts::getConstantIndex(off, offConst) && offConst == 0;
    bool isDefaultSize =
        arts::getConstantIndex(size, sizeConst) && sizeConst == 1;
    return !(isDefaultOffset && isDefaultSize);
  };

  if (!partitionOffset && !mutableAcquire.getOffsets().empty() &&
      !mutableAcquire.getSizes().empty() &&
      hasMeaningfulSlice(mutableAcquire.getOffsets().front(),
                         mutableAcquire.getSizes().front())) {
    partitionOffset = mutableAcquire.getOffsets().front();
    partitionSize = mutableAcquire.getSizes().front();
  }

  return true;
}

LogicalResult DbAcquireNode::computeChunkInfo(Value &chunkOffset,
                                              Value &chunkSize) {
  EdtOp edt = getEdtUser();
  if (!edt)
    return failure();

  if (partitionOffset && partitionSize) {
    chunkOffset = partitionOffset;
    chunkSize = partitionSize;
    return success();
  }

  scf::ForOp chunkLoop;
  scf::WhileOp chunkWhile;
  edt.walk([&](scf::ForOp forOp) {
    chunkLoop = forOp;
    return WalkResult::interrupt();
  });
  edt.walk([&](scf::WhileOp whileOp) {
    chunkWhile = whileOp;
    return WalkResult::interrupt();
  });

  if (chunkWhile) {
    if (succeeded(
            computeChunkInfoFromWhile(chunkWhile, chunkOffset, chunkSize)))
      return success();
  }

  if (!chunkLoop)
    return failure();

  chunkSize = chunkLoop.getUpperBound();

  Value ptrArg = getUseInEdt();
  if (!ptrArg)
    return failure();

  Value offsetCandidate;
  for (Operation *user : ptrArg.getUsers()) {
    auto refOp = dyn_cast<DbRefOp>(user);
    if (!refOp || !chunkLoop->isAncestor(refOp))
      continue;

    Value refVal = refOp.getResult();
    for (Operation *memUser : refVal.getUsers()) {
      auto storeOp = dyn_cast<memref::StoreOp>(memUser);
      if (!storeOp || !chunkLoop->isAncestor(storeOp))
        continue;

      if (storeOp.getIndices().empty())
        continue;
      Value idx = storeOp.getIndices().front();
      auto addOp = idx.getDefiningOp<arith::AddIOp>();
      if (!addOp)
        continue;

      Value iv = chunkLoop.getInductionVar();
      if (addOp.getOperand(0) == iv)
        offsetCandidate = addOp.getOperand(1);
      else if (addOp.getOperand(1) == iv)
        offsetCandidate = addOp.getOperand(0);

      if (offsetCandidate)
        break;
    }
    if (offsetCandidate)
      break;
  }

  if (!offsetCandidate)
    return failure();

  chunkOffset = offsetCandidate;
  return success();
}

LogicalResult DbAcquireNode::computeChunkInfoFromWhile(scf::WhileOp whileOp,
                                                       Value &chunkOffset,
                                                       Value &chunkSize) {
  if (!whileOp || !dbAcquireOp)
    return failure();
  if (whileOp.getInits().size() != 1)
    return failure();

  Value initValue = whileOp.getInits().front();
  Block &before = whileOp.getBefore().front();
  auto condition = dyn_cast<scf::ConditionOp>(before.getTerminator());
  if (!condition || before.getNumArguments() != 1)
    return failure();

  SmallVector<Value> bounds;
  arts::collectWhileBounds(condition.getCondition(), before.getArgument(0),
                           bounds);
  if (bounds.empty())
    return failure();

  OpBuilder builder(dbAcquireOp);
  Location loc = dbAcquireOp.getLoc();
  Value startIdx = arts::ensureIndexType(initValue, builder, loc);
  if (!startIdx)
    return failure();

  SmallVector<Value> chunkSizes;
  for (Value bound : bounds) {
    Value boundIdx = arts::ensureIndexType(bound, builder, loc);
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

  chunkOffset = startIdx;
  chunkSize = minSize;
  return success();
}

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << estimatedBytes;
  os << "\n";
}

/// Compute a maximal prefix of invariant indices (one per leading dimension
/// of the acquired memref) that are uniform across the EDT. These indices
/// can be used to coarsen the acquire by pinning leading dimensions.
/// The returned Values are SSA values already present in the IR; this API
/// does not materialize new constants or perform replacements.
SmallVector<Value, 4> DbAcquireNode::computeInvariantIndices() {
  ARTS_DEBUG("Starting invariant indices computation for " << dbAcquireOp);

  SmallVector<Value, 4> result;
  const size_t rank = dbAcquireOp.getSizes().size();

  SmallVector<Operation *> memoryAccesses;
  getMemoryAccesses(memoryAccesses, true, true);
  ARTS_DEBUG("Found " << memoryAccesses.size() << " memory accesses");

  SmallVector<Value, 4> candidate(rank, Value());
  SmallVector<bool, 4> invalid(rank, false);

  auto &edtRegion = edtUser.getBody();
  for (Operation *acc : memoryAccesses) {
    ValueRange idxs;
    if (auto ld = dyn_cast<memref::LoadOp>(acc))
      idxs = ld.getIndices();
    else if (auto dbLd = dyn_cast<DbRefOp>(acc))
      idxs = dbLd.getIndices();
    else if (auto st = dyn_cast<memref::StoreOp>(acc))
      idxs = st.getIndices();
    else
      continue;

    if (!edtRegion.isAncestor(acc->getParentRegion()))
      continue;

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
    if (!arts::isInvariantInEdt(edtRegion, candidate[d]))
      break;
    result.push_back(candidate[d]);
  }

  ARTS_DEBUG_REGION({
    ARTS_DEBUG(" - Invariant indices:");
    for (size_t d = 0; d < result.size(); ++d) {
      ARTS_DEBUG("   - Dimension " << d << ": " << result[d]);
    }
  });
  return result;
}

///===----------------------------------------------------------------------===///
// Twin-diff overlap analysis methods
///===----------------------------------------------------------------------===///

bool DbAcquireNode::isWorkerIndexedAccess() const {
  if (!op)
    return false;

  /// Need mutable reference to call non-const getters (op is Operation*)
  DbAcquireOp mutableOp(op);
  auto offsets = mutableOp.getOffsetHints();
  auto sizes = mutableOp.getSizeHints();

  if (offsets.empty() || sizes.empty())
    return false;

  /// Check size is constant 1 (unit access per worker)
  int64_t sz = 0;
  if (!arts::getConstantIndex(sizes[0], sz) || sz != 1)
    return false;

  /// Check offset is a BlockArgument (varying per EDT/iteration)
  Value offset = offsets[0];
  if (isa<BlockArgument>(offset)) {
    /// This is indexed by loop induction var or EDT parameter
    /// Each invocation gets different offset -> disjoint by construction
    ARTS_DEBUG(
        "Worker-indexed pattern detected: offset is BlockArg with size=1");
    return true;
  }

  return false;
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

  /// Need at least one dimension of offset/size hints
  if (offsetsA.empty() || sizesA.empty() || offsetsB.empty() || sizesB.empty())
    return false;

  Value offA = offsetsA[0];
  Value sizeA = sizesA[0];
  Value offB = offsetsB[0];
  Value sizeB = sizesB[0];

  /// Try constant range disjointness first
  int64_t offAVal = 0, sizeAVal = 0, offBVal = 0, sizeBVal = 0;
  if (arts::getConstantIndex(offA, offAVal) &&
      arts::getConstantIndex(sizeA, sizeAVal) &&
      arts::getConstantIndex(offB, offBVal) &&
      arts::getConstantIndex(sizeB, sizeBVal)) {
    int64_t endA = offAVal + sizeAVal;
    int64_t endB = offBVal + sizeBVal;
    if (endA <= offBVal || endB <= offAVal) {
      ARTS_DEBUG("Partition hints prove disjoint: constant ranges ["
                 << offAVal << "," << endA << ") vs [" << offBVal << "," << endB
                 << ")");
      return true;
    }
  }

  /// Check worker-indexed pattern: different BlockArguments with size=1
  int64_t sizeAConst = 0, sizeBConst = 0;
  bool sizeAIsOne =
      arts::getConstantIndex(sizeA, sizeAConst) && sizeAConst == 1;
  bool sizeBIsOne =
      arts::getConstantIndex(sizeB, sizeBConst) && sizeBConst == 1;

  if (sizeAIsOne && sizeBIsOne) {
    auto blockArgA = dyn_cast<BlockArgument>(offA);
    auto blockArgB = dyn_cast<BlockArgument>(offB);
    if (blockArgA && blockArgB && blockArgA != blockArgB) {
      if (blockArgA.getOwner() != blockArgB.getOwner()) {
        ARTS_DEBUG(
            "Partition hints prove disjoint: different BlockArgs with size=1");
        return true;
      }
    }
  }

  return false;
}

/// Collect all memory accesses (loads/stores) that transitively use a value.
void DbAcquireNode::collectAccesses(Value db) {
  SmallVector<Value, 8> worklist{db};
  DenseSet<Value> visited;
  visited.reserve(16);

  while (!worklist.empty()) {
    Value v = worklist.pop_back_val();
    if (!visited.insert(v).second)
      continue;

    for (Operation *user : v.getUsers()) {
      /// Follow through casts
      if (auto castOp = dyn_cast<memref::CastOp>(user)) {
        worklist.push_back(castOp.getResult());
        continue;
      }

      /// Track memref and LLVM stores
      if (isa<memref::StoreOp, LLVM::StoreOp>(user)) {
        stores.push_back(user);
        continue;
      }

      /// Track memref loads
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        loads.push_back(user);
        Type resultTy = loadOp.getResult().getType();
        if (isa<MemRefType>(resultTy))
          worklist.push_back(loadOp.getResult());
        continue;
      }

      if (auto dbRefOp = dyn_cast<DbRefOp>(user)) {
        references.push_back(user);
        Type resultTy = dbRefOp.getResult().getType();
        if (isa<MemRefType>(resultTy))
          worklist.push_back(dbRefOp.getResult());
        continue;
      }

      /// Track LLVM loads
      if (auto loadOp = dyn_cast<LLVM::LoadOp>(user)) {
        loads.push_back(user);
        continue;
      }
    }
  }
}
