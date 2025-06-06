///==========================================================================
/// File: DbAliasAnalysis.cpp
///
/// Simplified implementation of datablock alias analysis.
/// Removes over-engineering while preserving essential functionality.
///==========================================================================

#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbInfo.h"
#include "arts/Analysis/Db/Graph/DbNode.h"
#include "arts/ArtsDialect.h"
#include "mlir/Analysis/AliasAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-alias-analysis"
#define DBGS() (llvm::dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// DbAliasAnalysis Implementation
///===----------------------------------------------------------------------===///

DbAliasAnalysis::DbAliasAnalysis(DbAnalysis *analysis) : analysis(analysis) {
  assert(analysis && "Analysis cannot be null");
}

bool DbAliasAnalysis::mayAlias(DbInfo &A, DbInfo &B) {
  auto APtr = A.isAlloc() ? A.getResult() : A.getPtr();
  auto BPtr = B.isAlloc() ? B.getResult() : B.getPtr();

  /// Same value alias with itself
  if (APtr == BPtr)
    return true;

  /// Check cache
  auto key = makeOrderedPair(APtr, BPtr);
  auto it = aliasCache.find(key);
  if (it != aliasCache.end()) {
    return it->second;
  }

  /// Check if they are from the same allocation
  if (!areFromSameAllocation(APtr, BPtr)) {
    aliasCache[key] = false;
    return false;
  }

  bool result = false;

  // Same allocation, check memory regions
  // MemoryRegion regionA = getMemoryRegion(A);
  // MemoryRegion regionB = getMemoryRegion(B);
  // result = regionA.mayAlias(regionB);

  // Cache the result
  aliasCache[key] = result;
  return result;
}

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

bool DbAliasAnalysis::areFromSameAllocation(Value A, Value B) {
  Operation *rootA = findRootAllocation(A);
  Operation *rootB = findRootAllocation(B);

  return rootA && rootB && rootA == rootB;
}

Operation *DbAliasAnalysis::findRootAllocation(Value val) {
  Value current = val;
  Operation *lastAlloc = nullptr;

  while (current) {
    if (auto defineOp = current.getDefiningOp()) {
      if (isa<memref::AllocOp, memref::AllocaOp>(defineOp)) {
        lastAlloc = defineOp;
        break;
      } else if (auto subviewOp = dyn_cast<memref::SubViewOp>(defineOp)) {
        current = subviewOp.getSource();
        continue;
      } else if (auto dbCreateOp = dyn_cast<arts::DbCreateOp>(defineOp)) {
        lastAlloc = defineOp;
        break;
      }
    }
    break;
  }

  return lastAlloc;
}

/*
bool DbDataFlowAnalysis::ptrMayAlias(DatablockNode &A, DatablockNode &B) {
  if (A.aliases.contains(B.id))
    return true;

  for (auto &eA : A.effects) {
    for (auto &eB : B.effects) {
      if (mayAlias(eA, eB)) {
        // LLVM_DEBUG(dbgs() << "    - Datablocks may alias\n");
        A.aliases.insert(B.id);
        B.aliases.insert(A.id);
        return true;
      }
    }
  }
  return false;
}

bool DatablockAnalysis::ptrMayAlias(DatablockNode &A, Value val) {
  for (auto &eA : A.effects) {
    if (mayAlias(eA, val))
      return true;
  }
  return false;
}

*/