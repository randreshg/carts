///==========================================================================
/// DbAcquireNode.cpp - Implementation of DbAcquire node
///==========================================================================

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/LoopAnalysis.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinTypes.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_acquire_node);

namespace {
static void collectAccesses(Value blockArg, SmallVectorImpl<Operation *> &ops) {
  SmallVector<Value, 8> worklist{blockArg};
  DenseSet<Value> visited;
  visited.reserve(16);

  while (!worklist.empty()) {
    Value v = worklist.pop_back_val();
    if (!visited.insert(v).second)
      continue;

    for (Operation *user : v.getUsers()) {
      if (auto castOp = dyn_cast<memref::CastOp>(user)) {
        worklist.push_back(castOp.getResult());
        continue;
      }
      if (isa<memref::LoadOp, memref::StoreOp>(user)) {
        ops.push_back(user);
        continue;
      }
    }
  }
}
} // namespace

//===----------------------------------------------------------------------===//
// DbAcquireNode
//===----------------------------------------------------------------------===//
DbAcquireNode::DbAcquireNode(DbAcquireOp op, DbAllocNode *parent,
                             DbAnalysis *analysis)
    : dbAcquireOp(op), op(op.getOperation()), parent(parent),
      analysis(analysis) {
  ARTS_DEBUG(" - Creating DbAcquireNode for operation: " << op);

  const size_t numSizes = op.getSizes().size();
  const size_t numOffsets = op.getOffsets().size();
  const size_t numIndices = op.getIndices().size();

  info.sizes.reserve(numSizes);
  info.offsets.reserve(numOffsets);
  info.indices.reserve(numIndices);
  info.constSizes.reserve(numSizes);
  info.constOffsets.reserve(numOffsets);

  /// Use efficient assignment with move semantics where possible
  info.sizes.assign(op.getSizes().begin(), op.getSizes().end());
  info.offsets.assign(op.getOffsets().begin(), op.getOffsets().end());
  info.indices.assign(op.getIndices().begin(), op.getIndices().end());

  bool hasUnknown = false;

  /// Process offsets
  for (Value v : info.offsets) {
    if (auto c = v.getDefiningOp<arith::ConstantIndexOp>()) {
      info.constOffsets.push_back(c.value());
    } else {
      info.constOffsets.push_back(INT64_MIN);
      hasUnknown = true;
    }
  }

  /// Process sizes
  unsigned long long totalElems = 1;
  for (Value v : info.sizes) {
    if (auto c = v.getDefiningOp<arith::ConstantIndexOp>()) {
      int64_t sizeVal = c.value();
      info.constSizes.push_back(sizeVal);
      if (totalElems <= std::numeric_limits<unsigned long long>::max() /
                            std::max<int64_t>(sizeVal, 1)) {
        totalElems *= (unsigned long long)std::max<int64_t>(sizeVal, 1);
      } else {
        totalElems = std::numeric_limits<unsigned long long>::max();
        break;
      }
    } else {
      info.constSizes.push_back(INT64_MIN);
      hasUnknown = true;
    }
  }

  /// Compute estimated bytes
  if (!hasUnknown) {
    unsigned long long elemBytes =
        arts::getElementTypeByteSize(parent->getDbAllocOp().getElementType());
    if (elemBytes > 0) {
      if (totalElems >
          std::numeric_limits<unsigned long long>::max() / elemBytes) {
        info.estimatedBytes = std::numeric_limits<unsigned long long>::max();
        ARTS_WARN("Estimated bytes overflow, using max value");
      } else {
        info.estimatedBytes = totalElems * elemBytes;
      }
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

  /// Set the EDT user and use in EDT
  Value ptrResult = dbAcquireOp.getPtr();
  assert(ptrResult.hasOneUse() && "Acquire ptr should be used exactly once");

  /// Get the EDT that uses this acquire (the user of the ptr result)
  edtUser = dyn_cast<EdtOp>(*ptrResult.getUsers().begin());
  assert(edtUser && "Acquire ptr should be used by an EDT");

  /// Find the corresponding block argument in the EDT
  auto [depsBegin, depsLen] =
      edtUser.getODSOperandIndexAndLength(/*dependencies*/ 1);

  /// Find the operand index and get block argument
  unsigned operandIdx = 0;
  for (auto &use : ptrResult.getUses()) {
    if (use.getOwner() == edtUser.getOperation()) {
      operandIdx = use.getOperandNumber();
      break;
    }
  }
  unsigned depIndex = operandIdx - depsBegin;
  assert(depIndex < depsLen && "Dependency index should be within range");
  useInEdt = edtUser.getBody().getArgument(depIndex);

  /// Collect memory accesses for this acquire
  collectAccesses(useInEdt, memoryAccesses);
}

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << info.estimatedBytes;
  os << "\n";
}

SmallVector<DbAcquireNode::OffsetRange, 4>
DbAcquireNode::computeAccessedOffsetRanges() {
  ARTS_DEBUG("Starting offset range computation");

  SmallVector<OffsetRange, 4> result;
  const size_t rank = info.sizes.size();
  result.resize(rank);

  SmallVector<Value, 4> minIdx(rank, Value());
  SmallVector<Value, 4> maxIdx(rank, Value());
  SmallVector<bool, 4> minIsConst(rank, false), maxIsConst(rank, false);
  SmallVector<int64_t, 4> minConst(rank, std::numeric_limits<int64_t>::max());
  SmallVector<int64_t, 4> maxConst(rank, std::numeric_limits<int64_t>::min());
  auto *loopAnalysis = analysis->getLoopAnalysis();
  assert(loopAnalysis && "LoopAnalysis must be available");

  for (Operation *acc : memoryAccesses) {
    Value mem;
    ValueRange idxs;
    if (auto ld = dyn_cast<memref::LoadOp>(acc)) {
      mem = ld.getMemRef();
      idxs = ld.getIndices();
    } else if (auto st = dyn_cast<memref::StoreOp>(acc)) {
      mem = st.getMemRef();
      idxs = st.getIndices();
    } else {
      continue;
    }

    if (!edtUser.getBody().isAncestor(acc->getParentRegion()))
      continue;

    auto memTy = dyn_cast<MemRefType>(mem.getType());
    if (!memTy || (size_t)memTy.getRank() != rank)
      continue;

    for (size_t d = 0; d < rank; ++d) {
      Value iv = idxs[d];
      if (int64_t cst; arts::getConstantIndex(iv, cst)) {
        if (!minIdx[d]) {
          minIdx[d] = iv;
          minIsConst[d] = true;
          minConst[d] = cst;
        } else if (minIsConst[d])
          minConst[d] = std::min(minConst[d], cst);
        if (!maxIdx[d]) {
          maxIdx[d] = iv;
          maxIsConst[d] = true;
          maxConst[d] = cst;
        } else if (maxIsConst[d])
          maxConst[d] = std::max(maxConst[d], cst);
        continue;
      }

      Value lbV, ubV;
      int64_t lbC = 0, ubm1C = 0;
      bool lbCst = false, ubCst = false;

      // Use LoopAnalysis to find loops affecting this index expression.
      SmallVector<Operation *, 4> affecting;
      loopAnalysis->collectAffectingLoops(iv, affecting);
      if (affecting.size() == 1) {
        if (auto forOp = dyn_cast<scf::ForOp>(affecting.front())) {
          lbV = forOp.getLowerBound();
          Value ubVtmp = forOp.getUpperBound();
          Value stepV = forOp.getStep();
          lbCst = arts::getConstantIndex(lbV, lbC);
          int64_t ubC = 0, stC = 0;
          bool ubCstTmp = arts::getConstantIndex(ubVtmp, ubC);
          bool stCst = arts::getConstantIndex(stepV, stC);
          if (ubCstTmp && stCst) {
            ubCst = true;
            ubm1C = ubC - stC;
          }
          ubV = ubVtmp; // exclusive bound
        }
      }
      if (lbV || ubV || lbCst || ubCst) {
        if (lbCst) {
          if (!minIdx[d]) {
            minIdx[d] = lbV;
            minIsConst[d] = true;
            minConst[d] = lbC;
          } else if (minIsConst[d])
            minConst[d] = std::min(minConst[d], lbC);
        } else {
          minIdx[d] = lbV;
          minIsConst[d] = false;
        }
        if (ubCst) {
          if (!maxIdx[d]) {
            maxIdx[d] = ubV;
            maxIsConst[d] = true;
            maxConst[d] = ubm1C;
          } else if (maxIsConst[d])
            maxConst[d] = std::max(maxConst[d], ubm1C);
        } else {
          maxIdx[d] = ubV; // exclusive bound
          maxIsConst[d] = false;
        }
        continue;
      }
      // unknown -> leave as nulls
    }
  }

  OpBuilder b(op);
  for (size_t d = 0; d < rank; ++d) {
    result[d].base = (d < info.offsets.size()) ? info.offsets[d] : Value();
    if (minIsConst[d])
      result[d].minIndex =
          b.create<arith::ConstantIndexOp>(op->getLoc(), minConst[d]);
    else
      result[d].minIndex = minIdx[d];
    if (maxIsConst[d])
      result[d].maxIndex =
          b.create<arith::ConstantIndexOp>(op->getLoc(), maxConst[d]);
    else
      result[d].maxIndex = maxIdx[d];
  }

  ARTS_DEBUG_REGION({
    ARTS_DEBUG(" - Offset range computation completed");
    for (size_t d = 0; d < rank; ++d) {
      ARTS_DEBUG(" - Dimension " << d << ": " << result[d].base << ", "
                                 << result[d].minIndex << ", "
                                 << result[d].maxIndex);
    }
  });
  return result;
}

SmallVector<Value, 4> DbAcquireNode::computeInvariantIndices() {
  ARTS_DEBUG("Starting invariant indices computation for " << dbAcquireOp);

  SmallVector<Value, 4> result;
  const size_t rank = info.sizes.size();

  ARTS_DEBUG("Found " << memoryAccesses.size() << " memory accesses");

  SmallVector<Value, 4> candidate(rank, Value());
  SmallVector<bool, 4> invalid(rank, false);

  for (Operation *acc : memoryAccesses) {
    ValueRange idxs;
    if (auto ld = dyn_cast<memref::LoadOp>(acc))
      idxs = ld.getIndices();
    else if (auto st = dyn_cast<memref::StoreOp>(acc))
      idxs = st.getIndices();
    else
      continue;

    if (!edtUser.getBody().isAncestor(acc->getParentRegion()))
      continue;

    auto minRank = std::min<size_t>(rank, idxs.size());
    for (size_t d = 0; d < minRank; ++d) {
      if (invalid[d])
        continue;
      Value idxV = idxs[d];
      if (!candidate[d]) {
        candidate[d] = idxV;
      } else if (candidate[d] != idxV) {
        invalid[d] = true;
      }
    }
  }

  for (size_t d = 0; d < rank; ++d) {
    if (!candidate[d] || invalid[d])
      break;
    if (!arts::isInvariantInEdt(edtUser.getBody(), candidate[d]))
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
