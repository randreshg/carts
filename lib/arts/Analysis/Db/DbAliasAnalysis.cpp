//===----------------------------------------------------------------------===//
// Db/DbAliasAnalysis.cpp - Implementation of DbAliasAnalysis
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_alias_analysis);

using namespace mlir;
using namespace mlir::arts;

namespace {
std::pair<Value, Value> makeOrderedPair(Value a, Value b) {
  // Use pointer comparison for ordering
  return a.getAsOpaquePointer() < b.getAsOpaquePointer() ? std::make_pair(a, b)
                                                         : std::make_pair(b, a);
}
} // namespace

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis) : analysis(analysis) {
  assert(analysis && "Analysis cannot be null");
}

bool DbAliasAnalysis::mayAlias(const NodeBase &a, const NodeBase &b,
                               const std::string &indent) {
  Value ptrA = getUnderlyingValue(a);
  Value ptrB = getUnderlyingValue(b);

  ARTS_INFO("Analyzing alias between node " << a.getHierId() << " and node "
                                            << b.getHierId());

  // Same value aliases with itself
  if (ptrA == ptrB) {
    ARTS_INFO("  -> Same pointer, must alias");
    return true;
  }

  // Check cache
  auto key = makeOrderedPair(ptrA, ptrB);
  auto it = aliasCache.find(key);
  if (it != aliasCache.end()) {
    ARTS_INFO("  -> Cache hit: " << (it->second ? "may alias" : "no alias"));
    return it->second;
  }

  // Initial implementation: simple structural checks with slice comparison.
  bool result = true;

  auto sliceOf = [&](const NodeBase &n) -> std::tuple<Value, SmallVector<int64_t>, SmallVector<int64_t>> {
    SmallVector<int64_t> offs, lens;
    Value root;
    if (auto *acq = dyn_cast<DbAcquireNode>(&n)) {
      auto op = cast<DbAcquireOp>(acq->getOp());
      root = op.getSource();
      auto offsets = op.getOffsets();
      auto sizes = op.getSizes();
      offs.reserve(offsets.size());
      lens.reserve(sizes.size());
      offs.reserve(offsets.size());
      for (Value v : offsets)
        offs.push_back(v.getDefiningOp<mlir::arith::ConstantIndexOp>() ?
                       v.getDefiningOp<mlir::arith::ConstantIndexOp>().value() : INT64_MIN);
      lens.reserve(sizes.size());
      for (Value v : sizes)
        lens.push_back(v.getDefiningOp<mlir::arith::ConstantIndexOp>() ?
                       v.getDefiningOp<mlir::arith::ConstantIndexOp>().value() : INT64_MIN);
    } else if (auto *rel = dyn_cast<DbReleaseNode>(&n)) {
      auto op = cast<DbReleaseOp>(rel->getOp());
      if (!op.getSources().empty()) root = op.getSources()[0];
      // Unknown slice for release; be conservative
      offs.assign(1, INT64_MIN);
      lens.assign(1, INT64_MIN);
    } else if (auto *alloc = dyn_cast<DbAllocNode>(&n)) {
      root = alloc->getDbAllocOp().getPtr();
      offs.assign(1, 0);
      lens.assign(1, INT64_MIN);
    }
    return {root, offs, lens};
  };

  auto [ra, oa, la] = sliceOf(a);
  auto [rb, ob, lb] = sliceOf(b);
  if (ra && rb && ra == rb) {
    // If both slices are constant and disjoint in all known dims → no alias.
    bool anyUnknown = false;
    if (oa.size() == ob.size() && la.size() == lb.size() && oa.size() == la.size()) {
      bool disjoint = false;
      for (size_t i = 0; i < oa.size(); ++i) {
        if (oa[i] == INT64_MIN || ob[i] == INT64_MIN || la[i] == INT64_MIN || lb[i] == INT64_MIN) {
          anyUnknown = true;
          continue;
        }
        int64_t a0 = oa[i];
        int64_t a1 = oa[i] + la[i];
        int64_t b0 = ob[i];
        int64_t b1 = ob[i] + lb[i];
        bool overlap = !(a1 <= b0 || b1 <= a0);
        if (!overlap) { disjoint = true; break; }
      }
      if (!anyUnknown && disjoint) {
        result = false;
      }
    }
  }

  // If still unknown, use DbAnalysis overlap estimator for acquires
  if (result) {
    const auto *acqA = dyn_cast<DbAcquireNode>(&a);
    const auto *acqB = dyn_cast<DbAcquireNode>(&b);
    if (acqA && acqB) {
      auto k = analysis->estimateOverlap(cast<DbAcquireOp>(acqA->getOp()),
                                         cast<DbAcquireOp>(acqB->getOp()));
      if (k == DbAnalysis::OverlapKind::Disjoint) result = false;
    }
  }

  // if (isa<DbAllocNode>(&a) && isa<DbAllocNode>(&b)) {
  //   // Different allocations do not alias; same op aliases.
  //   result = a.getOp() == b.getOp();
  // } else if ((isa<DbAcquireNode>(&a) || isa<DbReleaseNode>(&a)) &&
  //            (isa<DbAcquireNode>(&b) || isa<DbReleaseNode>(&b))) {
  //   auto parentA = isa<DbAcquireNode>(&a)
  //                      ? dyn_cast<DbAcquireNode>(&a)->getParent()
  //                      : dyn_cast<DbReleaseNode>(&a)->getParent();
  //   auto parentB = isa<DbAcquireNode>(&b)
  //                      ? dyn_cast<DbAcquireNode>(&b)->getParent()
  //                      : dyn_cast<DbReleaseNode>(&b)->getParent();
  //   if (parentA && parentB)
  //     result = parentA->getOp() == parentB->getOp();
  //   else
  //     result = true; // conservative
  // } else {
  //   // Mixed alloc and access: alias if access refers to the alloc
  //   const NodeBase &alloc = isa<DbAllocNode>(&a) ? a : b;
  //   const NodeBase &access = isa<DbAllocNode>(&a) ? b : a;
  //   auto accessParent = isa<DbAcquireNode>(&access)
  //                           ? dyn_cast<DbAcquireNode>(&access)->getParent()
  //                           : dyn_cast<DbReleaseNode>(&access)->getParent();
  //   if (accessParent)
  //     result = accessParent->getOp() == alloc.getOp();
  //   else
  //     result = true; // conservative
  // }

  aliasCache[key] = result;
  ARTS_INFO("  -> Final result: " << (result ? "may alias" : "no alias"));
  return result;
}

Value DbAliasAnalysis::getUnderlyingValue(const NodeBase &node) {
  Operation *op = node.getOp();
  if (isa<DbAllocOp>(op)) {
    return op->getResult(0);
  } else if (isa<DbAcquireOp>(op)) {
    return cast<DbAcquireOp>(op).getSource();
  } else if (isa<DbReleaseOp>(op)) {
    return cast<DbReleaseOp>(op).getSources()[0];
  }
  llvm_unreachable("Invalid DB node type");
}