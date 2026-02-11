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

/// Distributed ownership requires positive extents, but they can be symbolic
/// (e.g., node-dependent block sizing computed by distribution heuristics).
static bool hasSupportedAllocationShape(DbAllocOp alloc) {
  auto isPositiveOrDynamic = [](Value value) {
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

static bool isEligibleDistributedAlloc(DbAllocOp alloc,
                                       DbAnalysis &dbAnalysis) {
  if (!alloc)
    return false;
  if (alloc->getParentOfType<EdtOp>())
    return false;
  if (alloc.getAllocType() == DbAllocType::global)
    return false;
  if (!hasMultipleAllocationBlocks(alloc))
    return false;
  if (!hasSupportedAllocationShape(alloc))
    return false;
  if (!hasOnlyAllowedHandleUsers(alloc.getPtr()))
    return false;
  if (!hasOnlyAllowedHandleUsers(alloc.getGuid()))
    return false;
  if (!isUsedByInternodeEdt(alloc, dbAnalysis))
    return false;
  return true;
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
      bool eligible = isEligibleDistributedAlloc(alloc, dbAnalysis);
      setDistributedDbAllocation(alloc.getOperation(), eligible);
      if (eligible)
        ++markedDistributed;
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
