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
#include "arts/Codegen/DbCodegen.h"
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
#include <memory>
#include <optional>

/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-arts-to-llvm"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Constants and Configuration
//===----------------------------------------------------------------------===//
namespace {
/// Configuration constants
constexpr int DEFAULT_EDT_SLOT = 0;
constexpr int DEFAULT_DEP_COUNT = 1;

/// Helper type aliases
using ValueMap = DenseMap<Operation *, Value>;
} // namespace

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertArtsToLLVMPass
    : public arts::ConvertArtsToLLVMBase<ConvertArtsToLLVMPass> {

  explicit ConvertArtsToLLVMPass(bool debug = false) : debugMode(debug) {}

  ~ConvertArtsToLLVMPass() { delete AC; }

  void runOnOperation() override;

private:
  LogicalResult initializeCodegen();
  LogicalResult processOperations(func::FuncOp func);
  LogicalResult processEdtOperations(func::FuncOp func);
  void finalizeConversion();

  /// Operation ha
  template <typename OpType> LogicalResult handleRuntimeInfoOp(OpType op);
  LogicalResult handleEdt(EdtOp op);
  LogicalResult handleEpoch(EpochOp op);
  LogicalResult handleBarrier(BarrierOp op);
  LogicalResult handleAlloc(AllocOp op);
  LogicalResult handleDbAlloc(DbAllocOp op);
  LogicalResult handleDbDep(DbDepOp op);

  /// Utility methods
  bool hasArtsOperations() const;
  void cleanupOperations();
  void reportStatistics() const;

  /// Error handling
  LogicalResult emitOpError(Operation *op, const Twine &message) const;

  /// Helper for walk results
  WalkResult handleWalkResult(LogicalResult result) const {
    return succeeded(result) ? WalkResult::advance() : WalkResult::interrupt();
  }

  /// Member variables
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  ValueMap edtToEpoch;
  SetVector<Operation *> opsToRemove;
  bool debugMode = false;

  /// Statistics tracking (using a struct for better organization)
  struct ConversionStatistics {
    unsigned edtOps = 0;
    unsigned epochOps = 0;
    unsigned barrierOps = 0;
    unsigned allocOps = 0;
    unsigned dbAllocOps = 0;
    unsigned dbDepOps = 0;
    unsigned runtimeInfoOps = 0;

    void reset() { *this = ConversionStatistics{}; }
  } statistics;
};
} // namespace

//===----------------------------------------------------------------------===//
// Core Pass Implementation
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::runOnOperation() {
  module = getOperation();

  LLVM_DEBUG({
    dbgs() << LINE << "ConvertArtsToLLVMPass START\n" << LINE;
    dbgs() << "Input module:\n";
    module.dump();
  });

  /// Early exit if no ARTS operations
  if (!hasArtsOperations()) {
    LLVM_DEBUG(DBGS() << "No ARTS operations found, skipping conversion\n");
    return;
  }

  /// Initialize codegen infrastructure
  if (failed(initializeCodegen()))
    return signalPassFailure();

  /// Process operations
  auto mainFunc = module.lookupSymbol<func::FuncOp>("main");
  if (failed(processOperations(mainFunc)))
    return signalPassFailure();

  LLVM_DEBUG({
    dbgs() << LINE << "Module after processing operations (before finalization):\n";
    module.dump();
  });

  /// Finalize conversion and cleanup
  finalizeConversion();

  LLVM_DEBUG({
    dbgs() << LINE << "ConvertArtsToLLVMPass COMPLETED\n" << LINE;
    dbgs() << "Final module:\n";
    module.dump();
    dbgs() << LINE << LINE;
  });
}

LogicalResult ConvertArtsToLLVMPass::initializeCodegen() {
  /// Validate data layout
  auto llvmDLAttr = module->getAttrOfType<StringAttr>("llvm.data_layout");
  if (!llvmDLAttr)
    return emitOpError(module, "Module missing required LLVM data layout");

  llvm::DataLayout llvmDL(llvmDLAttr.getValue().str());
  mlir::DataLayout mlirDL(module);
  AC = new ArtsCodegen(module, llvmDL, mlirDL, debugMode);

  LLVM_DEBUG(DBGS() << "ArtsCodegen initialized successfully\n");
  return success();
}

bool ConvertArtsToLLVMPass::hasArtsOperations() const {
  /// Quick check to see if we have any ARTS operations to convert
  bool hasOps = false;
  module->walk([&](Operation *op) {
    if (isArtsOp(op)) {
      hasOps = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return hasOps;
}

LogicalResult ConvertArtsToLLVMPass::processOperations(func::FuncOp func) {
  LLVM_DEBUG(DBGS() << "Starting operation processing\n");

  /// Single-pass operation processing with ordered handling
  LLVM_DEBUG(dbgs() << LINE << "1st pass - Runtime info operations\n");
  auto result = func->walk([&](Operation *op) -> WalkResult {
    if (opsToRemove.contains(op))
      return WalkResult::advance();

    /// Handle runtime information operations first (no dependencies)
    if (auto runtimeOp = dyn_cast<arts::GetTotalWorkersOp>(op))
      return handleWalkResult(handleRuntimeInfoOp(runtimeOp));

    if (auto runtimeOp = dyn_cast<arts::GetTotalNodesOp>(op))
      return handleWalkResult(handleRuntimeInfoOp(runtimeOp));

    if (auto runtimeOp = dyn_cast<arts::GetCurrentWorkerOp>(op))
      return handleWalkResult(handleRuntimeInfoOp(runtimeOp));

    if (auto runtimeOp = dyn_cast<arts::GetCurrentNodeOp>(op))
      return handleWalkResult(handleRuntimeInfoOp(runtimeOp));

    /// Handle allocation operations
    if (auto allocOp = dyn_cast<arts::AllocOp>(op))
      return handleWalkResult(handleAlloc(allocOp));

    /// Handle DB alloc operations
    if (auto dbAllocOp = dyn_cast<arts::DbAllocOp>(op))
      return handleWalkResult(handleDbAlloc(dbAllocOp));

    /// Handle DbDep operations - they are managed by DbDepCodegen but need to be removed
    if (auto dbDepOp = dyn_cast<arts::DbDepOp>(op))
      return handleWalkResult(handleDbDep(dbDepOp));

    return WalkResult::advance();
  });

  if (result.wasInterrupted())
    return failure();

  /// Second pass for sync operations
  LLVM_DEBUG(dbgs() << LINE << "2nd pass - Sync operations\n");
  result = func->walk<WalkOrder::PreOrder>([&](Operation *op) -> WalkResult {
    if (opsToRemove.contains(op))
      return WalkResult::skip();

    /// Handle epochs first
    if (auto epochOp = dyn_cast<arts::EpochOp>(op))
      return handleWalkResult(handleEpoch(epochOp));

    /// Handle barriers
    if (auto barrierOp = dyn_cast<arts::BarrierOp>(op))
      return handleWalkResult(handleBarrier(barrierOp));

    return WalkResult::advance();
  });

  if (result.wasInterrupted())
    return failure();

  /// Third pass for EDTs
  LLVM_DEBUG(dbgs() << LINE << "3rd pass - EDT operations\n");
  result = handleWalkResult(processEdtOperations(func));

  LLVM_DEBUG(dbgs() << LINE << "Operation processing completed successfully\n");
  return success();
}

LogicalResult ConvertArtsToLLVMPass::processEdtOperations(func::FuncOp func) {
  auto result =
      func->walk<WalkOrder::PreOrder>([&](Operation *op) -> WalkResult {
        if (opsToRemove.contains(op))
          return WalkResult::skip();

        /// Handle EDTs
        if (auto edtOp = dyn_cast<arts::EdtOp>(op))
          return handleWalkResult(handleEdt(edtOp));

        return WalkResult::advance();
      });

  if (result.wasInterrupted())
    return failure();

  return success();
}

void ConvertArtsToLLVMPass::finalizeConversion() {
  /// Apply all replacements before cleanup
  AC->applyReplacements();

  /// Clean up operations marked for removal
  cleanupOperations();

  /// Initialize runtime and report statistics
  AC->initRT(AC->getUnknownLoc());
  reportStatistics();
}

//===----------------------------------------------------------------------===//
// Operation Handlers
//===----------------------------------------------------------------------===//

template <typename OpType>
LogicalResult ConvertArtsToLLVMPass::handleRuntimeInfoOp(OpType op) {
  LLVM_DEBUG(DBGS() << "Processing runtime info operation: " << op << "\n");
  statistics.runtimeInfoOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  Value result;
  if constexpr (std::is_same_v<OpType, arts::GetTotalWorkersOp>)
    result = AC->getTotalWorkers(op.getLoc());
  else if constexpr (std::is_same_v<OpType, arts::GetTotalNodesOp>)
    result = AC->getTotalNodes(op.getLoc());
  else if constexpr (std::is_same_v<OpType, arts::GetCurrentWorkerOp>)
    result = AC->getCurrentWorker(op.getLoc());
  else if constexpr (std::is_same_v<OpType, arts::GetCurrentNodeOp>)
    result = AC->getCurrentNode(op.getLoc());
  else
    return emitOpError(op, "Unsupported runtime info operation");

  AC->addReplacement(op.getResult(), result);
  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleEdt(EdtOp op) {
  LLVM_DEBUG(DBGS() << "Processing EDT: " << op << "\n");
  statistics.edtOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  /// Create the EDT
  auto dependencies = op.getDependenciesVector();
  auto &region = op.getRegion();
  auto loc = op.getLoc();

  Value currentEpoch = edtToEpoch.lookup(op);
  if (!currentEpoch)
    currentEpoch = AC->getCurrentEpochGuid(AC->getUnknownLoc());

  auto *newEdt = AC->createEdt(&dependencies, &region, &currentEpoch, &loc);
  if (!newEdt)
    return emitOpError(op, "Failed to create EDT");

  opsToRemove.insert(op);

  /// Handle child EDTs
  return processEdtOperations(newEdt->getFunc());
}

LogicalResult ConvertArtsToLLVMPass::handleEpoch(EpochOp op) {
  LLVM_DEBUG(DBGS() << "Processing Epoch: " << op << "\n");
  statistics.epochOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  /// Create a done-edt for the epoch
  EdtCodegen epochDoneEdt(*AC);
  epochDoneEdt.setDepC(
      AC->createIntConstant(DEFAULT_DEP_COUNT, AC->Int32, op.getLoc()));
  epochDoneEdt.build(op.getLoc());

  auto guid = epochDoneEdt.getGuid();
  if (!guid)
    return emitOpError(op, "Failed to create epoch done EDT");

  /// Create the epoch
  auto epochDoneSlot =
      AC->createIntConstant(DEFAULT_EDT_SLOT, AC->Int32, op.getLoc());
  auto currentEpoch = AC->createEpoch(guid, epochDoneSlot, op.getLoc());

  /// Move all operations out of the epoch region to preserve them
  auto &epochRegion = op.getRegion();
  auto &epochBlock = epochRegion.front();
  SmallVector<Operation *, 8> opsToMove;
  for (auto &innerOp : epochBlock.without_terminator())
    opsToMove.push_back(&innerOp);

  /// Move operations before the epoch operation and map child EDTs
  SmallVector<arts::EdtOp> childEdts;
  for (auto *opToMove : opsToMove) {
    opToMove->moveBefore(op);

    if (auto edtOp = dyn_cast<arts::EdtOp>(opToMove)) {
      childEdts.push_back(edtOp);
      edtToEpoch[edtOp] = currentEpoch;
    }
  }

  LLVM_DEBUG(DBGS() << "Mapped " << childEdts.size()
                    << " child EDTs to epoch\n");

  /// Set insertion point after epoch creation and add wait
  AC->setInsertionPointAfter(currentEpoch.getDefiningOp());
  AC->waitOnHandle(currentEpoch, op.getLoc());

  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleBarrier(BarrierOp op) {
  LLVM_DEBUG(DBGS() << "Processing Barrier: " << op << "\n");
  statistics.barrierOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  AC->createRuntimeCall(types::ARTSRTL_artsYield, {}, op.getLoc());

  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleAlloc(AllocOp op) {
  LLVM_DEBUG(DBGS() << "Processing Alloc: " << op << "\n");
  statistics.allocOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  auto resultType = op.getResult().getType().cast<MemRefType>();
  auto elementType = resultType.getElementType();

  /// Calculate total allocation size
  auto elementSize = AC->getMLIRDataLayout().getTypeSizeInBits(elementType) / 8;
  auto elementSizeValue =
      AC->createIntConstant(elementSize, AC->Int64, op.getLoc());
  Value totalSize = elementSizeValue;

  /// Handle dynamic sizes
  auto dynamicSizes = op.getDynamicSizes();
  if (!dynamicSizes.empty()) {
    for (auto size : dynamicSizes) {
      auto sizeValue = AC->castToInt(AC->Int64, size, op.getLoc());
      totalSize = AC->getBuilder().create<arith::MulIOp>(op.getLoc(), totalSize,
                                                         sizeValue);
    }
  } else {
    /// Handle static shape
    auto shape = resultType.getShape();
    for (auto dim : shape) {
      if (dim != ShapedType::kDynamic) {
        auto dimValue = AC->createIntConstant(dim, AC->Int64, op.getLoc());
        totalSize = AC->getBuilder().create<arith::MulIOp>(op.getLoc(),
                                                           totalSize, dimValue);
      }
    }
  }

  /// Call ARTS runtime allocation
  auto artsAllocCall = AC->createRuntimeCall(types::ARTSRTL_artsMalloc,
                                             {totalSize}, op.getLoc());
  auto allocPtr = artsAllocCall.getResult(0);
  auto castPtr = AC->castPtr(resultType, allocPtr, op.getLoc());

  AC->addReplacement(op.getResult(), castPtr);
  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleDbAlloc(DbAllocOp op) {
  LLVM_DEBUG(DBGS() << "Processing top-level DbAlloc: " << op << "\n");
  statistics.dbAllocOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  auto *dbCodegen = AC->getOrCreateDbAlloc(op, op.getLoc());
  if (!dbCodegen)
    return emitOpError(op, "Failed to create DbAllocCodegen");

  AC->addReplacement(op.getResult(), dbCodegen->getPtr());
  opsToRemove.insert(op);
  LLVM_DEBUG(DBGS() << "DbAlloc processing completed successfully\n");

  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleDbDep(DbDepOp op) {
  LLVM_DEBUG(DBGS() << "Processing DbDep: " << op << "\n");
  statistics.dbDepOps++;

  /// DbDep operations are processed through DbDepCodegen objects
  /// when their source DbAlloc operations are handled. Mark them for removal
  /// since their replacements will be set up by the DbDepCodegen infrastructure.
  opsToRemove.insert(op);
  LLVM_DEBUG(DBGS() << "DbDep marked for removal: " << op << "\n");

  return success();
}

//===----------------------------------------------------------------------===//
// Utility Methods
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::cleanupOperations() {
  LLVM_DEBUG(DBGS() << "Cleaning up " << opsToRemove.size() << " operations\n");

  for (Operation *op : opsToRemove) {
    if (op && op->getParentOp())
      op->erase();
  }
  opsToRemove.clear();
}

void ConvertArtsToLLVMPass::reportStatistics() const {
  LLVM_DEBUG({
    dbgs() << "\n" << LINE << "ARTS to LLVM Conversion Statistics\n" << LINE;
    dbgs() << "EDT operations: " << statistics.edtOps << "\n";
    dbgs() << "Epoch operations: " << statistics.epochOps << "\n";
    dbgs() << "Barrier operations: " << statistics.barrierOps << "\n";
    dbgs() << "Alloc operations: " << statistics.allocOps << "\n";
    dbgs() << "DbAlloc operations: " << statistics.dbAllocOps << "\n";
    dbgs() << "DbDep operations: " << statistics.dbDepOps << "\n";
    dbgs() << "Runtime Info operations: " << statistics.runtimeInfoOps << "\n";
    dbgs() << LINE;
  });
}

LogicalResult ConvertArtsToLLVMPass::emitOpError(Operation *op,
                                                 const Twine &message) const {
  return op->emitError() << message;
}

//===----------------------------------------------------------------------===//
// Pass Functions
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
