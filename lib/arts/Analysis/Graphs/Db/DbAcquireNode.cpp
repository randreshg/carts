///==========================================================================///
/// DbAcquireNode.cpp - Implementation of DbAcquire node
///==========================================================================///

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
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
    if (totalElems >
        std::numeric_limits<unsigned long long>::max() /
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
  if (auto nestedAcquire = dyn_cast<DbAcquireOp>(singleUser)) {
    /// Nested acquire; create child node lazily via getOrCreateAcquireNode
    /// The rest of this constructor computes info for this node; nested
    /// children will compute their own.
  } else {
    /// Use utility function to get EDT and block argument
    auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(dbAcquireOp);
    edtUser = edt;
    useInEdt = blockArg;
    assert(edtUser && useInEdt &&
           "Acquire ptr should be used by an EDT or another acquire");
  }

  /// Find the corresponding DbReleaseOp for this acquire
  assert(useInEdt && "Acquire ptr should be used by an EDT or another acquire");
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
  if (useInEdt)
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
  const size_t rank = sizes.size();

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
