//===----------------------------------------------------------------------===//
// Db/DbAliasAnalysis.cpp - Implementation of DbAliasAnalysis
//===----------------------------------------------------------------------===//

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "llvm/Support/Debug.h"

#undef DEBUG_TYPE
#define DEBUG_TYPE "db-alias-analysis"
#define dbgs() llvm::dbgs()
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
#define DBGS_INDENT(indent) (dbgs() << indent << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

namespace {
std::pair<Value, Value> makeOrderedPair(Value a, Value b) {
  // Use pointer comparison for ordering
  return a.getAsOpaquePointer() < b.getAsOpaquePointer() ? std::make_pair(a, b)
                                                         : std::make_pair(b, a);
}
} // namespace

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis)
    : analysis(analysis) {
  assert(analysis && "Analysis cannot be null");
}

bool DbAliasAnalysis::mayAlias(const NodeBase &a, const NodeBase &b,
                               const std::string &indent) {
  Value ptrA = getUnderlyingValue(a);
  Value ptrB = getUnderlyingValue(b);

  LLVM_DEBUG(DBGS_INDENT(indent)
             << "Analyzing alias between node " << a.getHierId() << " and node "
             << b.getHierId() << "\n");

  // Same value aliases with itself
  if (ptrA == ptrB) {
    LLVM_DEBUG(dbgs() << indent << "  -> Same pointer, must alias\n");
    return true;
  }

  // Check cache
  auto key = makeOrderedPair(ptrA, ptrB);
  auto it = aliasCache.find(key);
  if (it != aliasCache.end()) {
    LLVM_DEBUG(dbgs() << indent << "  -> Cache hit: "
                      << (it->second ? "may alias" : "no alias") << "\n");
    return it->second;
  }

  // Initial implementation: simple structural checks, otherwise conservative.
  bool result = true;

  if (isa<DbAllocNode>(&a) && isa<DbAllocNode>(&b)) {
    // Different allocations do not alias; same op aliases.
    result = a.getOp() == b.getOp();
  } else if ((isa<DbAcquireNode>(&a) || isa<DbReleaseNode>(&a)) &&
             (isa<DbAcquireNode>(&b) || isa<DbReleaseNode>(&b))) {
    auto parentA = isa<DbAcquireNode>(&a)
                       ? dyn_cast<DbAcquireNode>(&a)->getParent()
                       : dyn_cast<DbReleaseNode>(&a)->getParent();
    auto parentB = isa<DbAcquireNode>(&b)
                       ? dyn_cast<DbAcquireNode>(&b)->getParent()
                       : dyn_cast<DbReleaseNode>(&b)->getParent();
    if (parentA && parentB)
      result = parentA->getOp() == parentB->getOp();
    else
      result = true; // conservative
  } else {
    // Mixed alloc and access: alias if access refers to the alloc
    const NodeBase &alloc = isa<DbAllocNode>(&a) ? a : b;
    const NodeBase &access = isa<DbAllocNode>(&a) ? b : a;
    auto accessParent = isa<DbAcquireNode>(&access)
                            ? dyn_cast<DbAcquireNode>(&access)->getParent()
                            : dyn_cast<DbReleaseNode>(&access)->getParent();
    if (accessParent)
      result = accessParent->getOp() == alloc.getOp();
    else
      result = true; // conservative
  }

  aliasCache[key] = result;
  LLVM_DEBUG(dbgs() << indent << "  -> Final result: "
                    << (result ? "may alias" : "no alias") << "\n");
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