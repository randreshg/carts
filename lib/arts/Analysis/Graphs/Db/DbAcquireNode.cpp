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

//===----------------------------------------------------------------------===//
// DbAcquireNode
//===----------------------------------------------------------------------===//
DbAcquireNode::DbAcquireNode(DbAcquireOp op, DbAllocNode *parent,
                             DbAnalysis *analysis)
    : dbAcquireOp(op), op(op.getOperation()), parent(parent),
      analysis(analysis) {
  info.sizes.assign(op.getSizes().begin(), op.getSizes().end());
  info.offsets.assign(op.getOffsets().begin(), op.getOffsets().end());
  info.indices.assign(op.getIndices().begin(), op.getIndices().end());
  info.constSizes.reserve(info.sizes.size());
  info.constOffsets.reserve(info.offsets.size());
  bool hasUnknown = false;
  for (Value v : info.offsets) {
    if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
      info.constOffsets.push_back(c.value());
    else {
      info.constOffsets.push_back(INT64_MIN);
      hasUnknown = true;
    }
  }
  unsigned long long totalElems = 1;
  for (Value v : info.sizes) {
    if (auto c = v.getDefiningOp<arith::ConstantIndexOp>()) {
      info.constSizes.push_back(c.value());
      totalElems *= (unsigned long long)std::max<int64_t>(c.value(), 1);
    } else {
      info.constSizes.push_back(INT64_MIN);
      hasUnknown = true;
    }
  }
  if (!hasUnknown) {
    /// Use element type from parent DbAlloc to compute bytes
    unsigned long long elemBytes =
        arts::getElementTypeByteSize(parent->getDbAllocOp().getElementType());
    if (elemBytes > 0) {
      if (totalElems >
          std::numeric_limits<unsigned long long>::max() / elemBytes)
        info.estimatedBytes = std::numeric_limits<unsigned long long>::max();
      else
        info.estimatedBytes = totalElems * elemBytes;
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
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << info.estimatedBytes;
  os << "\n";
}

namespace {
static void collectAccesses(Value root, SmallVectorImpl<Operation *> &ops) {
  SmallVector<Value, 8> worklist{root};
  DenseSet<Value> visited;
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

SmallVector<DbAcquireNode::OffsetRange, 4>
DbAcquireNode::computeAccessedOffsetRanges() {
  SmallVector<OffsetRange, 4> result;
  const size_t rank = info.sizes.size();
  result.resize(rank);

  auto edt = op->getParentOfType<arts::EdtOp>();
  if (!edt)
    return result;

  SmallVector<Operation *, 16> accesses;
  collectAccesses(dbAcquireOp.getPtr(), accesses);

  SmallVector<Value, 4> minIdx(rank, Value());
  SmallVector<Value, 4> maxIdx(rank, Value());
  SmallVector<bool, 4> minIsConst(rank, false), maxIsConst(rank, false);
  SmallVector<int64_t, 4> minConst(rank, std::numeric_limits<int64_t>::max());
  SmallVector<int64_t, 4> maxConst(rank, std::numeric_limits<int64_t>::min());
  assert(analysis && "DbAnalysis must be available");
  auto *loopAnalysis = analysis->getLoopAnalysis();

  for (Operation *acc : accesses) {
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

    if (!edt.getBody().isAncestor(acc->getParentRegion()))
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

  return result;
}

SmallVector<Value, 4> DbAcquireNode::computeInvariantIndices() {
  SmallVector<Value, 4> result;
  const size_t rank = info.sizes.size();

  auto edt = op->getParentOfType<arts::EdtOp>();
  if (!edt)
    return result;

  SmallVector<Operation *, 16> accesses;
  collectAccesses(dbAcquireOp.getPtr(), accesses);

  SmallVector<Value, 4> candidate(rank, Value());
  SmallVector<bool, 4> invalid(rank, false);

  for (Operation *acc : accesses) {
    ValueRange idxs;
    if (auto ld = dyn_cast<memref::LoadOp>(acc))
      idxs = ld.getIndices();
    else if (auto st = dyn_cast<memref::StoreOp>(acc))
      idxs = st.getIndices();
    else
      continue;

    if (!edt.getBody().isAncestor(acc->getParentRegion()))
      continue;

    for (size_t d = 0; d < std::min<size_t>(rank, idxs.size()); ++d) {
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
    if (!arts::isInvariantInEdt(edt.getBody(), candidate[d]))
      break;
    result.push_back(candidate[d]);
  }

  return result;
}
