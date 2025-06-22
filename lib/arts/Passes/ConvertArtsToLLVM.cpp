///==========================================================================
// File: ConvertArtsToLLVM.cpp
//
// This file implements a pass to convert ARTS dialect operations into
// LLVM dialect operations. Since preprocessing is handled by a separate
// pass, this pass focuses purely on ARTS-specific conversion logic.
///==========================================================================

/// Dialects
#include "arts/Codegen/EdtCodegen.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Codegen/ArtsIR.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Others
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypeInterfaces.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"

#include "mlir/Transforms/DialectConversion.h"
#include "polygeist/Ops.h"
#include <algorithm>
#include <optional>

/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-arts-to-llvm"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertArtsToLLVMPass
    : public arts::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  ConvertArtsToLLVMPass(bool debug = false) : debug(debug) {}

  void runOnOperation() override;

  /// ARTS Operation Handlers
  void handleEdt(EdtOp &op);
  void handleEpoch(EpochOp &op);
  void handleEvent(EventOp &op);
  void handleBarrier(BarrierOp &op);
  void handleAlloc(AllocOp &op);
  void handleUndef(UndefOp &op);

  /// Runtime Information Operations
  void handleGetTotalWorkers(GetTotalWorkersOp &op);
  void handleGetTotalNodes(GetTotalNodesOp &op);
  void handleGetCurrentWorker(GetCurrentWorkerOp &op);
  void handleGetCurrentNode(GetCurrentNodeOp &op);

  /// Helper Methods
  void iterateOps();
  void cleanupUnusedOperations();
  void reportConversionStats();

private:
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  DenseMap<EdtOp, Value> edtToEpoch;
  SetVector<Operation *> opsToRemove;
  bool debug = false;

  /// Statistics and Tracking
  struct ConversionStats {
    unsigned edtOps = 0;
    unsigned epochOps = 0;
    unsigned eventOps = 0;
    unsigned barrierOps = 0;
    unsigned allocOps = 0;
    unsigned undefOps = 0;
    unsigned getTotalWorkersOps = 0;
    unsigned getTotalNodesOps = 0;
    unsigned getCurrentWorkerOps = 0;
    unsigned getCurrentNodeOps = 0;
  } stats;
};
} // end namespace

//===----------------------------------------------------------------------===//
// ARTS Operation Handlers
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::handleEdt(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt\n");
  stats.edtOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  /// Create unknown location
  Location loc = UnknownLoc::get(op.getContext());

  /// Create the parallel EDT with the single op's region.
  auto dependencies = op.getDependenciesVector();
  auto &region = op.getRegion();

  Value currentEpoch;
  if (auto it = edtToEpoch.find(op); it != edtToEpoch.end())
    currentEpoch = it->second;
  else
    currentEpoch = AC->getCurrentEpochGuid(loc);

  auto *newEdt = AC->createEdt(&dependencies, &region, &currentEpoch, &loc);

  /// Visit new function
  assert(newEdt && "New EDT not created");
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleEpoch(EpochOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.epoch: " << op << "\n");
  stats.epochOps++;

  Location loc = UnknownLoc::get(op.getContext());
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  /// Create a done-edt for the epoch.
  EdtCodegen epochDoneEdt(*AC);
  epochDoneEdt.setDepC(AC->createIntConstant(1, AC->Int32, loc));
  epochDoneEdt.build(loc);

  /// Create the epoch.
  auto epochDoneSlot = AC->createIntConstant(0, AC->Int32, loc);
  auto currentEpoch =
      AC->createEpoch(epochDoneEdt.getGuid(), epochDoneSlot, loc);

  /// Find all the EDTs that are inside the Epoch region, which ParentOfType
  /// Epoch is the current epoch and don't have a parent EdtOp.
  op.walk([&](arts::EdtOp childOp) {
    auto parentEdt = childOp->getParentOfType<EdtOp>();
    if (parentEdt || childOp->getParentOfType<EpochOp>() != op)
      return;
    edtToEpoch[childOp] = currentEpoch;
  });

  /// Move epoch region operations out of the epoch op and insert them after the
  /// current epoch.
  Operation *currentEpochOp = currentEpoch.getDefiningOp();
  Block *epochBlock = &op.getRegion().front();
  Operation *currentOp = currentEpochOp;
  for (Operation &childOp : llvm::make_early_inc_range(*epochBlock)) {
    if (isa<arts::YieldOp>(childOp))
      continue;
    childOp.moveAfter(currentOp);
    currentOp = &childOp;
  }
  AC->setInsertionPointAfter(currentOp);

  /// Insert wait on handle after epoch.
  AC->waitOnHandle(currentEpoch, loc);

  /// Remove the epoch op.
  op->erase();
}

void ConvertArtsToLLVMPass::handleEvent(EventOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.event\n");
  stats.eventOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  Location loc = op.getLoc();
  auto sizes = op.getSizes();

  /// Create runtime call to create persistent event(s)
  Value eventGuid;
  if (sizes.empty()) {
    /// Single event - create with latch count 1
    auto route = AC->createIntConstant(0, AC->Int32, loc); // Default route
    auto latchCount = AC->createIntConstant(1, AC->Int32, loc);
    auto dataGuid =
        AC->createIntConstant(0, AC->Int64, loc); // No data initially
    eventGuid = AC->createRuntimeCall(types::ARTSRTL_artsPersistentEventCreate,
                                      {route, latchCount, dataGuid}, loc)
                    .getResult(0);
  } else {
    /// Array of events - compute total size and create with appropriate latch
    /// count
    Value totalSize = AC->createIntConstant(1, AC->Int64, loc);
    for (Value size : sizes) {
      Value sizeAsInt64 = AC->castToInt(AC->Int64, size, loc);
      totalSize =
          AC->getBuilder().create<arith::MulIOp>(loc, totalSize, sizeAsInt64);
    }

    auto route = AC->createIntConstant(0, AC->Int32, loc); // Default route
    auto latchCount = AC->castToInt(AC->Int32, totalSize, loc);
    auto dataGuid =
        AC->createIntConstant(0, AC->Int64, loc); // No data initially
    eventGuid = AC->createRuntimeCall(types::ARTSRTL_artsPersistentEventCreate,
                                      {route, latchCount, dataGuid}, loc)
                    .getResult(0);
  }

  /// Replace the event operation with the GUID
  op.replaceAllUsesWith(eventGuid);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleBarrier(BarrierOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.barrier\n");
  stats.barrierOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  Location loc = op.getLoc();
  /// For barrier synchronization, we can use artsYield() to yield control and
  /// allow other EDTs to make progress
  AC->createRuntimeCall(types::ARTSRTL_artsYield, {}, loc);

  /// Remove the barrier operation
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleAlloc(AllocOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.alloc\n");
  stats.allocOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  Location loc = op.getLoc();
  auto dynamicSizes = op.getDynamicSizes();
  auto resultType = op.getResult().getType().cast<MemRefType>();

  /// Convert to memref.alloc operation
  auto memrefAlloc =
      AC->getBuilder().create<memref::AllocOp>(loc, resultType, dynamicSizes);

  /// Replace the arts.alloc with memref.alloc
  op.replaceAllUsesWith(memrefAlloc.getResult());
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleUndef(UndefOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.undef\n");
  stats.undefOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  Location loc = op.getLoc();
  auto resultType = op.getResult().getType();

  /// Convert to LLVM undef if compatible, otherwise use a default value
  Value undefValue;
  if (LLVM::isCompatibleType(resultType)) {
    undefValue = AC->getBuilder().create<LLVM::UndefOp>(loc, resultType);
  } else if (auto intType = resultType.dyn_cast<IntegerType>()) {
    undefValue = AC->createIntConstant(0, intType, loc);
  } else if (auto floatType = resultType.dyn_cast<FloatType>()) {
    undefValue = AC->getBuilder().create<arith::ConstantFloatOp>(
        loc, APFloat::getZero(floatType.getFloatSemantics()), floatType);
  } else {
    op.emitError("Unsupported type for arts.undef conversion");
    return;
  }

  /// Replace the undef operation
  op.replaceAllUsesWith(undefValue);
  opsToRemove.insert(op);
}

//===----------------------------------------------------------------------===//
// Runtime Information Operations
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::handleGetTotalWorkers(GetTotalWorkersOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getTotalWorkers\n");
  stats.getTotalWorkersOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  auto totalWorkers = AC->getTotalWorkers(op.getLoc());
  op.replaceAllUsesWith(totalWorkers);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetTotalNodes(GetTotalNodesOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getTotalNodes\n");
  stats.getTotalNodesOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  auto totalNodes = AC->getTotalNodes(op.getLoc());
  op.replaceAllUsesWith(totalNodes);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetCurrentWorker(GetCurrentWorkerOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getCurrentWorker\n");
  stats.getCurrentWorkerOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  auto currentWorker = AC->getCurrentWorker(op.getLoc());
  op.replaceAllUsesWith(currentWorker);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetCurrentNode(GetCurrentNodeOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getCurrentNode\n");
  stats.getCurrentNodeOps++;

  auto currentNode = AC->getCurrentNode(op.getLoc());
  op.replaceAllUsesWith(currentNode);
  opsToRemove.insert(op);
}

//===----------------------------------------------------------------------===//
// Helper Methods
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::cleanupUnusedOperations() {
  LLVM_DEBUG(DBGS() << "Cleaning up unused operations\n");

  /// Remove all collected operations
  for (Operation *op : opsToRemove) {
    if (op && op->getParentOp()) {
      op->erase();
    }
  }
  opsToRemove.clear();
}

void ConvertArtsToLLVMPass::reportConversionStats() {
  LLVM_DEBUG({
    dbgs() << "\n" << line << "ARTS to LLVM Conversion Statistics\n" << line;
    dbgs() << "EDT operations: " << stats.edtOps << "\n";
    dbgs() << "Epoch operations: " << stats.epochOps << "\n";
    dbgs() << "Event operations: " << stats.eventOps << "\n";
    dbgs() << "Barrier operations: " << stats.barrierOps << "\n";
    dbgs() << "Alloc operations: " << stats.allocOps << "\n";
    dbgs() << "Undef operations: " << stats.undefOps << "\n";
    dbgs() << "GetTotalWorkers operations: " << stats.getTotalWorkersOps
           << "\n";
    dbgs() << "GetTotalNodes operations: " << stats.getTotalNodesOps << "\n";
    dbgs() << "GetCurrentWorker operations: " << stats.getCurrentWorkerOps
           << "\n";
    dbgs() << "GetCurrentNode operations: " << stats.getCurrentNodeOps << "\n";
    dbgs() << line;
  });
}

void ConvertArtsToLLVMPass::iterateOps() {
  LLVM_DEBUG(DBGS() << "Starting ARTS operation iteration\n");

  /// Process runtime information operations first
  module->walk([&](arts::GetTotalWorkersOp op) { handleGetTotalWorkers(op); });
  module->walk([&](arts::GetTotalNodesOp op) { handleGetTotalNodes(op); });
  module->walk(
      [&](arts::GetCurrentWorkerOp op) { handleGetCurrentWorker(op); });
  module->walk([&](arts::GetCurrentNodeOp op) { handleGetCurrentNode(op); });

  /// Process control flow operations
  module.walk([&](arts::EpochOp epoch) { handleEpoch(epoch); });
  LLVM_DEBUG({
    DBGS() << "Module after processing epochs:\n";
    module.dump();
    dbgs() << line;
  });

  /// Process EDT operations
  module.walk<mlir::WalkOrder::PreOrder>([&](arts::EdtOp edtOp) {
    if (opsToRemove.count(edtOp))
      return mlir::WalkResult::skip();
    if (edtOp.isTask())
      handleEdt(edtOp);
    else {
      llvm_unreachable(
          "Sync, Parallel, and single EDTs should have been lowered. "
          "Did you run the 'create-epochs' pass?");
    }
    return mlir::WalkResult::advance();
  });

  /// Process other ARTS operations
  // module.walk([&](arts::EventOp op) { handleEvent(op); });
  module.walk([&](arts::BarrierOp op) { handleBarrier(op); });
  module.walk([&](arts::AllocOp op) { handleAlloc(op); });
  // module.walk([&](arts::UndefOp op) { handleUndef(op); });

  /// Clean up removed operations
  cleanupUnusedOperations();

  LLVM_DEBUG({
    DBGS() << "Module after processing all ARTS operations:\n";
    module.dump();
    dbgs() << line;
  });
}

//===----------------------------------------------------------------------===//
// Main Pass Implementation
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::runOnOperation() {
  module = getOperation();
  LLVM_DEBUG({
    dbgs() << line << "ConvertArtsToLLVMPass START\n" << line;
    module.dump();
  });

  /// Data Layouts
  auto llvmDLAttr = module->getAttrOfType<StringAttr>("llvm.data_layout");
  if (!llvmDLAttr) {
    module.emitError("Module does not have a data layout");
    return signalPassFailure();
  }
  llvm::DataLayout llvmDL(llvmDLAttr.getValue().str());
  mlir::DataLayout mlirDL(module);

  /// Initialize the AC object, iterate ops and initialize the runtime.
  AC = new ArtsCodegen(module, llvmDL, mlirDL, debug);
  iterateOps();
  AC->initRT(UnknownLoc::get(module.getContext()));

  /// Report conversion statistics
  reportConversionStats();

  LLVM_DEBUG({
    dbgs() << line << "ConvertArtsToLLVMPass FINISHED \n" << line;
    module.dump();
  });
  delete AC;
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertArtsToLLVMPass() {
  return std::make_unique<ConvertArtsToLLVMPass>();
}

std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug) {
  return std::make_unique<ConvertArtsToLLVMPass>(debug);
}

} // namespace arts
} // namespace mlir
