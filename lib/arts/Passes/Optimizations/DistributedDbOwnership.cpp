///==========================================================================///
/// File: DistributedDbOwnership.cpp
///
/// Marks eligible DbAlloc operations for distributed ownership lowering.
///
/// Example:
///   Before:
///     %db = arts.db_alloc ...      // no ownership marker
///
///   After:
///     %db = arts.db_alloc ... {distributed}
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include <cassert>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(distributed_db_ownership);

using namespace mlir;
using namespace mlir::arts;

namespace {

enum class DistributedRejectReason {
  None,
  NestedInEdt,
  GlobalAllocType,
  SingleBlock,
  UnsupportedShape,
  ReadOnlyInternodeUse,
  BroadcastReadAcquire,
  UnsupportedPtrUsers,
  UnsupportedGuidUsers,
  NonEdtAcquireUse,
  NoInternodeEdtUse,
};

struct EligibilityResult {
  bool eligible = false;
  DistributedRejectReason reason = DistributedRejectReason::None;
};

static const char *toString(DistributedRejectReason reason) {
  switch (reason) {
  case DistributedRejectReason::None:
    return "eligible";
  case DistributedRejectReason::NestedInEdt:
    return "nested_in_edt";
  case DistributedRejectReason::GlobalAllocType:
    return "global_alloc";
  case DistributedRejectReason::SingleBlock:
    return "single_block";
  case DistributedRejectReason::UnsupportedShape:
    return "unsupported_shape";
  case DistributedRejectReason::ReadOnlyInternodeUse:
    return "read_only_internode_use";
  case DistributedRejectReason::BroadcastReadAcquire:
    return "broadcast_read_acquire";
  case DistributedRejectReason::UnsupportedPtrUsers:
    return "unsupported_ptr_users";
  case DistributedRejectReason::UnsupportedGuidUsers:
    return "unsupported_guid_users";
  case DistributedRejectReason::NonEdtAcquireUse:
    return "non_edt_acquire_use";
  case DistributedRejectReason::NoInternodeEdtUse:
    return "no_internode_edt_use";
  }
  return "unknown";
}

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
  return anyAcquireNodeMatches(alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
    EdtOp edt = acquireNode->getEdtUser();
    return edt && edt.getConcurrency() == EdtConcurrency::internode;
  });
}

static bool hasWriterInternodeEdtUse(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  return anyAcquireNodeMatches(alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
    DbAcquireOp acquireOp = acquireNode->getDbAcquireOp();
    if (!acquireOp)
      return false;
    if (acquireOp.getMode() == ArtsMode::in)
      return false;

    EdtOp edt = acquireNode->getEdtUser();
    return edt && edt.getConcurrency() == EdtConcurrency::internode;
  });
}

static bool hasOnlyEdtAcquireUsers(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  return allAcquireNodesMatch(alloc, dbAnalysis, [&](DbAcquireNode *acquireNode) {
    return static_cast<bool>(acquireNode->getEdtUser());
  });
}

[[maybe_unused]] static bool hasBroadcastReadAcquireUsers(DbAllocOp alloc,
                                                          DbAnalysis &dbAnalysis) {
  func::FuncOp parentFunc = alloc->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  DbGraph &graph = dbAnalysis.getOrCreateGraph(parentFunc);
  DbAllocNode *allocNode = graph.getDbAllocNode(alloc);
  if (!allocNode)
    return false;

  auto sameOrEqualConstant = [](Value lhs, Value rhs) -> bool {
    lhs = ValueUtils::stripNumericCasts(lhs);
    rhs = ValueUtils::stripNumericCasts(rhs);
    if (lhs == rhs)
      return true;
    int64_t lhsConst = 0;
    int64_t rhsConst = 0;
    if (!ValueUtils::getConstantIndex(lhs, lhsConst) ||
        !ValueUtils::getConstantIndex(rhs, rhsConst))
      return false;
    return lhsConst == rhsConst;
  };

  auto isZeroIndex = [](Value value) -> bool {
    value = ValueUtils::stripNumericCasts(value);
    int64_t constant = 0;
    return ValueUtils::getConstantIndex(value, constant) && constant == 0;
  };

  bool hasBroadcastReadAcquire = false;
  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (hasBroadcastReadAcquire || !acquireNode)
      return;

    DbAllocNode *rootAlloc = acquireNode->getRootAlloc();
    if (!rootAlloc || rootAlloc != allocNode)
      return;

    DbAcquireOp acquireOp = acquireNode->getDbAcquireOp();
    if (!acquireOp || acquireOp.getMode() != ArtsMode::in)
      return;

    auto allocSizes = alloc.getSizes();
    auto acquireOffsets = acquireOp.getOffsets();
    auto acquireSizes = acquireOp.getSizes();
    if (allocSizes.size() != acquireOffsets.size() ||
        allocSizes.size() != acquireSizes.size())
      return;

    bool fullRangeRead = true;
    for (auto [allocSize, acquireOffset, acquireSize] :
         llvm::zip_equal(allocSizes, acquireOffsets, acquireSizes)) {
      if (!isZeroIndex(acquireOffset) ||
          !sameOrEqualConstant(acquireSize, allocSize)) {
        fullRangeRead = false;
        break;
      }
    }

    if (fullRangeRead)
      hasBroadcastReadAcquire = true;
  });

  return hasBroadcastReadAcquire;
}

static EligibilityResult
evaluateDistributedEligibility(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  if (!alloc)
    return {false, DistributedRejectReason::UnsupportedShape};
  if (alloc->getParentOfType<EdtOp>())
    return {false, DistributedRejectReason::NestedInEdt};
  if (alloc.getAllocType() == DbAllocType::global)
    return {false, DistributedRejectReason::GlobalAllocType};
  if (!hasMultipleAllocationBlocks(alloc))
    return {false, DistributedRejectReason::SingleBlock};
  if (!hasSupportedAllocationShape(alloc))
    return {false, DistributedRejectReason::UnsupportedShape};
  if (!hasOnlyAllowedHandleUsers(alloc.getPtr()))
    return {false, DistributedRejectReason::UnsupportedPtrUsers};
  if (!hasOnlyAllowedHandleUsers(alloc.getGuid()))
    return {false, DistributedRejectReason::UnsupportedGuidUsers};
  if (!hasOnlyEdtAcquireUsers(alloc, dbAnalysis))
    return {false, DistributedRejectReason::NonEdtAcquireUse};
  if (!isUsedByInternodeEdt(alloc, dbAnalysis))
    return {false, DistributedRejectReason::NoInternodeEdtUse};
  if (!hasWriterInternodeEdtUse(alloc, dbAnalysis))
    return {false, DistributedRejectReason::ReadOnlyInternodeUse};
  return {true, DistributedRejectReason::None};
}

struct DistributedDbOwnershipPass
    : public arts::DistributedDbOwnershipBase<DistributedDbOwnershipPass> {
  explicit DistributedDbOwnershipPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    auto *machine = &AM->getAbstractMachine();
    if (!machine->hasConfigFile() || !machine->hasValidNodeCount() ||
        !machine->hasValidThreads()) {
      module.emitError(
          "invalid ARTS machine configuration for distributed DB ownership");
      signalPassFailure();
      return;
    }

    auto &dbAnalysis = AM->getDbAnalysis();
    dbAnalysis.invalidate();

    unsigned totalAllocs = 0;
    unsigned markedDistributed = 0;
    module.walk([&](DbAllocOp alloc) {
      ++totalAllocs;
      EligibilityResult eligibility =
          evaluateDistributedEligibility(alloc, dbAnalysis);
      setDistributedDbAllocation(alloc.getOperation(), eligibility.eligible);
      if (eligibility.eligible)
        ++markedDistributed;
      else
        ARTS_DEBUG("Reject DbAlloc arts.id=" << getArtsId(alloc.getOperation())
                                             << " reason="
                                             << toString(eligibility.reason));
    });

    ARTS_INFO("DistributedDbOwnership marked " << markedDistributed << " / "
                                               << totalAllocs
                                               << " DbAlloc operations");
  }

private:
  ArtsAnalysisManager *AM = nullptr;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createDistributedDbOwnershipPass(ArtsAnalysisManager *AM) {
  return std::make_unique<DistributedDbOwnershipPass>(AM);
}
} // namespace arts
} // namespace mlir
