///==========================================================================///
/// File: DbDistributedEligibility.cpp
///
/// Eligibility analysis for distributed DB ownership marking.
///==========================================================================///

#include "arts/analysis/db/DbDistributedEligibility.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/graphs/db/DbGraph.h"
#include "arts/analysis/graphs/db/DbNode.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool hasMultipleAllocationBlocks(DbAllocOp alloc) {
  auto sizes = alloc.getSizes();
  if (sizes.empty())
    return false;
  if (sizes.size() > 1)
    return true;

  int64_t blockCount = 0;
  if (ValueAnalysis::getConstantIndex(sizes.front(), blockCount))
    return blockCount > 1;
  return true;
}

static bool hasSupportedAllocationShape(DbAllocOp alloc) {
  /// Distributed ownership currently targets matrix/tensor-style allocations.
  /// Keep vectors/scalars local to avoid over-marking small or temporary DBs.
  if (alloc.getElementSizes().size() < 2)
    return false;

  auto isPositiveOrDynamic = [](Value value) -> bool {
    int64_t constant = 0;
    if (!ValueAnalysis::getConstantIndex(
            ValueAnalysis::stripNumericCasts(value), constant))
      return true;
    return constant > 0;
  };

  for (Value size : alloc.getSizes())
    if (!isPositiveOrDynamic(size))
      return false;
  for (Value size : alloc.getElementSizes())
    if (!isPositiveOrDynamic(size))
      return false;
  return true;
}

static bool hasOnlyAllowedHandleUsers(Value rootHandle) {
  if (!rootHandle)
    return false;

  SmallVector<Value, 8> worklist{rootHandle};
  llvm::DenseSet<Value> visitedValues;

  auto enqueue = [&](Value value) {
    if (value)
      worklist.push_back(value);
  };

  auto isMemoryUserInsideEdt = [](Operation *user) {
    return user && static_cast<bool>(user->getParentOfType<EdtOp>());
  };

  while (!worklist.empty()) {
    Value value = worklist.pop_back_val();
    if (!visitedValues.insert(value).second)
      continue;

    for (Operation *user : value.getUsers()) {
      if (isa<DbAcquireOp, DbFreeOp, DbReleaseOp, EdtOp>(user))
        continue;

      if (auto castOp = dyn_cast<memref::CastOp>(user)) {
        if (castOp.getSource() == value)
          enqueue(castOp.getResult());
        continue;
      }

      if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
        if (subview.getSource() == value)
          enqueue(subview.getResult());
        continue;
      }

      if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(user)) {
        if (reinterpret.getSource() == value)
          enqueue(reinterpret.getResult());
        continue;
      }

      if (auto view = dyn_cast<memref::ViewOp>(user)) {
        if (view.getSource() == value)
          enqueue(view.getResult());
        continue;
      }

      if (auto dbRefOp = dyn_cast<DbRefOp>(user)) {
        if (dbRefOp.getSource() == value)
          enqueue(dbRefOp.getResult());
        continue;
      }

      if (auto dbGepOp = dyn_cast<DbGepOp>(user)) {
        if (dbGepOp.getBasePtr() == value)
          enqueue(dbGepOp.getResult());
        continue;
      }

      if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
        if (llvm::is_contained(unrealized.getInputs(), value))
          for (Value result : unrealized->getResults())
            enqueue(result);
        continue;
      }

      if (auto access = DbUtils::getMemoryAccessInfo(user)) {
        if (access->memref == value && isMemoryUserInsideEdt(user))
          continue;
        return false;
      }

      if (isa<memref::DimOp, memref::DeallocOp>(user))
        continue;

      if (isa<arith::ConstantOp>(user))
        continue;

      if (isMemoryEffectFree(user)) {
        for (Value result : user->getResults())
          enqueue(result);
        continue;
      }

      return false;
    }
  }

  return true;
}

template <typename PredicateT>
static bool anyAcquireNodeMatches(DbAllocOp alloc, DbAnalysis &dbAnalysis,
                                  PredicateT &&predicate) {
  func::FuncOp parentFunc = alloc->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  DbGraph &graph = dbAnalysis.getOrCreateGraph(parentFunc);
  DbAllocNode *allocNode = graph.getDbAllocNode(alloc);
  if (!allocNode)
    return false;

  bool matched = false;
  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (matched || !acquireNode)
      return;
    if (acquireNode->getRootAlloc() != allocNode)
      return;
    matched = predicate(acquireNode);
  });

  return matched;
}

template <typename PredicateT>
static bool allAcquireNodesMatch(DbAllocOp alloc, DbAnalysis &dbAnalysis,
                                 PredicateT &&predicate) {
  func::FuncOp parentFunc = alloc->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  DbGraph &graph = dbAnalysis.getOrCreateGraph(parentFunc);
  DbAllocNode *allocNode = graph.getDbAllocNode(alloc);
  if (!allocNode)
    return false;

  bool allMatched = true;
  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (!allMatched || !acquireNode)
      return;
    if (acquireNode->getRootAlloc() != allocNode)
      return;
    allMatched = predicate(acquireNode);
  });

  return allMatched;
}

static bool isUsedByInternodeEdt(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  return anyAcquireNodeMatches(
      alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
        EdtOp edt = acquireNode->getEdtUser();
        return edt && edt.getConcurrency() == EdtConcurrency::internode;
      });
}

static bool hasStencilReadInternodeEdtUse(DbAllocOp alloc,
                                          DbAnalysis &dbAnalysis) {
  return anyAcquireNodeMatches(
      alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
        DbAcquireOp acquireOp = acquireNode->getDbAcquireOp();
        if (!acquireOp)
          return false;

        EdtOp edt = acquireNode->getEdtUser();
        if (!edt || edt.getConcurrency() != EdtConcurrency::internode)
          return false;

        if (!acquireNode->isReadOnlyAccess())
          return false;

        auto depPattern = getEffectiveDepPattern(acquireOp.getOperation())
                              .value_or(ArtsDependencePattern::unknown);
        if (isStencilFamilyDepPattern(depPattern) ||
            getStencilCenterOffset(acquireOp.getOperation()))
          return true;

        if (const DbAcquirePartitionFacts *facts =
                dbAnalysis.getAcquirePartitionFacts(acquireOp))
          return facts->accessPattern == AccessPattern::Stencil;

        return acquireNode->getAccessPattern() == AccessPattern::Stencil;
      });
}

static bool hasOnlyEdtAcquireUsers(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  return allAcquireNodesMatch(
      alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
        return static_cast<bool>(acquireNode->getEdtUser());
      });
}

} // namespace

const char *mlir::arts::toString(DistributedDbEligibilityRejectReason reason) {
  switch (reason) {
  case DistributedDbEligibilityRejectReason::None:
    return "eligible";
  case DistributedDbEligibilityRejectReason::NestedInEdt:
    return "nested_in_edt";
  case DistributedDbEligibilityRejectReason::GlobalAllocType:
    return "global_alloc";
  case DistributedDbEligibilityRejectReason::SingleBlock:
    return "single_block";
  case DistributedDbEligibilityRejectReason::UnsupportedShape:
    return "unsupported_shape";
  case DistributedDbEligibilityRejectReason::StencilReadInternodeUse:
    return "stencil_read_internode_use";
  case DistributedDbEligibilityRejectReason::UnsupportedPtrUsers:
    return "unsupported_ptr_users";
  case DistributedDbEligibilityRejectReason::UnsupportedGuidUsers:
    return "unsupported_guid_users";
  case DistributedDbEligibilityRejectReason::NonEdtAcquireUse:
    return "non_edt_acquire_use";
  case DistributedDbEligibilityRejectReason::NoInternodeEdtUse:
    return "no_internode_edt_use";
  }
  return "unknown";
}

DistributedDbEligibilityResult
mlir::arts::evaluateDistributedDbEligibility(DbAllocOp alloc,
                                             DbAnalysis &dbAnalysis) {
  if (!alloc)
    return {false, DistributedDbEligibilityRejectReason::UnsupportedShape};
  if (alloc->getParentOfType<EdtOp>())
    return {false, DistributedDbEligibilityRejectReason::NestedInEdt};
  if (alloc.getAllocType() == DbAllocType::global)
    return {false, DistributedDbEligibilityRejectReason::GlobalAllocType};
  if (!hasMultipleAllocationBlocks(alloc))
    return {false, DistributedDbEligibilityRejectReason::SingleBlock};
  if (!hasSupportedAllocationShape(alloc))
    return {false, DistributedDbEligibilityRejectReason::UnsupportedShape};
  if (!hasOnlyAllowedHandleUsers(alloc.getPtr()))
    return {false, DistributedDbEligibilityRejectReason::UnsupportedPtrUsers};
  if (!hasOnlyAllowedHandleUsers(alloc.getGuid()))
    return {false, DistributedDbEligibilityRejectReason::UnsupportedGuidUsers};
  if (!hasOnlyEdtAcquireUsers(alloc, dbAnalysis))
    return {false, DistributedDbEligibilityRejectReason::NonEdtAcquireUse};
  if (!isUsedByInternodeEdt(alloc, dbAnalysis))
    return {false, DistributedDbEligibilityRejectReason::NoInternodeEdtUse};
  if (hasStencilReadInternodeEdtUse(alloc, dbAnalysis))
    return {false,
            DistributedDbEligibilityRejectReason::StencilReadInternodeUse};
  return {true, DistributedDbEligibilityRejectReason::None};
}
