///==========================================================================///
/// File: DistributedDbOwnership.cpp
///
/// Marks eligible DbAlloc operations for distributed ownership lowering.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
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

static bool isUsedByInternodeEdt(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  func::FuncOp parentFunc = alloc->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  DbGraph &graph = dbAnalysis.getOrCreateGraph(parentFunc);
  DbAllocNode *allocNode = graph.getDbAllocNode(alloc);
  if (!allocNode)
    return false;

  bool hasInternodeUse = false;
  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (hasInternodeUse || !acquireNode)
      return;

    DbAllocNode *rootAlloc = acquireNode->getRootAlloc();
    if (!rootAlloc || rootAlloc != allocNode)
      return;

    EdtOp edt = acquireNode->getEdtUser();
    if (!edt)
      return;
    if (edt.getConcurrency() == EdtConcurrency::internode)
      hasInternodeUse = true;
  });

  return hasInternodeUse;
}

static bool hasOnlyEdtAcquireUsers(DbAllocOp alloc, DbAnalysis &dbAnalysis) {
  func::FuncOp parentFunc = alloc->getParentOfType<func::FuncOp>();
  if (!parentFunc)
    return false;

  DbGraph &graph = dbAnalysis.getOrCreateGraph(parentFunc);
  DbAllocNode *allocNode = graph.getDbAllocNode(alloc);
  if (!allocNode)
    return false;

  bool allAcquireUsersAreEdts = true;
  graph.forEachAcquireNode([&](DbAcquireNode *acquireNode) {
    if (!allAcquireUsersAreEdts || !acquireNode)
      return;

    DbAllocNode *rootAlloc = acquireNode->getRootAlloc();
    if (!rootAlloc || rootAlloc != allocNode)
      return;

    if (!acquireNode->getEdtUser())
      allAcquireUsersAreEdts = false;
  });

  return allAcquireUsersAreEdts;
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
