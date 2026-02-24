///==========================================================================///
/// File: DistributedDbOwnershipPolicy.cpp
///
/// Eligibility policy for distributed DB ownership marking.
///==========================================================================///

#include "arts/Analysis/Db/DistributedDbOwnershipPolicy.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
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
  if (ValueUtils::getConstantIndex(sizes.front(), blockCount))
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
    if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(value),
                                      constant))
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

  while (!worklist.empty()) {
    Value handle = worklist.pop_back_val();
    if (!visitedValues.insert(handle).second)
      continue;

    for (Operation *user : handle.getUsers()) {
      if (isa<DbAcquireOp, DbFreeOp, EdtOp>(user))
        continue;

      if (auto castOp = dyn_cast<memref::CastOp>(user)) {
        worklist.push_back(castOp.getResult());
        continue;
      }

      if (auto dbRefOp = dyn_cast<DbRefOp>(user)) {
        for (Value result : dbRefOp->getResults())
          worklist.push_back(result);
        continue;
      }

      if (auto dbGepOp = dyn_cast<DbGepOp>(user)) {
        for (Value result : dbGepOp->getResults())
          worklist.push_back(result);
        continue;
      }

      if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
        for (Value result : unrealized->getResults())
          worklist.push_back(result);
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

static bool hasWriterInternodeEdtUse(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  return anyAcquireNodeMatches(
      alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
        if (!acquireNode || !acquireNode->isWriterAccess())
          return false;

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

        AccessPattern pattern = acquireNode->getAccessPattern();
        if (!acquireNode->isReadOnlyAccess())
          return false;

        bool hasStencilHint =
            pattern == AccessPattern::Stencil ||
            acquireOp->hasAttr(AttrNames::Operation::StencilCenterOffset);
        return hasStencilHint;
      });
}

static bool hasOnlyEdtAcquireUsers(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  return allAcquireNodesMatch(
      alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
        return static_cast<bool>(acquireNode->getEdtUser());
      });
}

} // namespace

const char *mlir::arts::toString(DistributedDbOwnershipRejectReason reason) {
  switch (reason) {
  case DistributedDbOwnershipRejectReason::None:
    return "eligible";
  case DistributedDbOwnershipRejectReason::NestedInEdt:
    return "nested_in_edt";
  case DistributedDbOwnershipRejectReason::GlobalAllocType:
    return "global_alloc";
  case DistributedDbOwnershipRejectReason::SingleBlock:
    return "single_block";
  case DistributedDbOwnershipRejectReason::UnsupportedShape:
    return "unsupported_shape";
  case DistributedDbOwnershipRejectReason::StencilReadInternodeUse:
    return "stencil_read_internode_use";
  case DistributedDbOwnershipRejectReason::ReadOnlyInternodeUse:
    return "read_only_internode_use";
  case DistributedDbOwnershipRejectReason::UnsupportedPtrUsers:
    return "unsupported_ptr_users";
  case DistributedDbOwnershipRejectReason::UnsupportedGuidUsers:
    return "unsupported_guid_users";
  case DistributedDbOwnershipRejectReason::NonEdtAcquireUse:
    return "non_edt_acquire_use";
  case DistributedDbOwnershipRejectReason::NoInternodeEdtUse:
    return "no_internode_edt_use";
  }
  return "unknown";
}

DistributedDbOwnershipEligibilityResult
mlir::arts::evaluateDistributedDbOwnershipEligibility(DbAllocOp alloc,
                                                      DbAnalysis &dbAnalysis) {
  if (!alloc)
    return {false, DistributedDbOwnershipRejectReason::UnsupportedShape};
  if (alloc->getParentOfType<EdtOp>())
    return {false, DistributedDbOwnershipRejectReason::NestedInEdt};
  if (alloc.getAllocType() == DbAllocType::global)
    return {false, DistributedDbOwnershipRejectReason::GlobalAllocType};
  if (!hasMultipleAllocationBlocks(alloc))
    return {false, DistributedDbOwnershipRejectReason::SingleBlock};
  if (!hasSupportedAllocationShape(alloc))
    return {false, DistributedDbOwnershipRejectReason::UnsupportedShape};
  if (!hasOnlyAllowedHandleUsers(alloc.getPtr()))
    return {false, DistributedDbOwnershipRejectReason::UnsupportedPtrUsers};
  if (!hasOnlyAllowedHandleUsers(alloc.getGuid()))
    return {false, DistributedDbOwnershipRejectReason::UnsupportedGuidUsers};
  if (!hasOnlyEdtAcquireUsers(alloc, dbAnalysis))
    return {false, DistributedDbOwnershipRejectReason::NonEdtAcquireUse};
  if (!isUsedByInternodeEdt(alloc, dbAnalysis))
    return {false, DistributedDbOwnershipRejectReason::NoInternodeEdtUse};
  if (hasStencilReadInternodeEdtUse(alloc, dbAnalysis))
    return {false, DistributedDbOwnershipRejectReason::StencilReadInternodeUse};
  if (!hasWriterInternodeEdtUse(alloc, dbAnalysis))
    return {false, DistributedDbOwnershipRejectReason::ReadOnlyInternodeUse};
  return {true, DistributedDbOwnershipRejectReason::None};
}
