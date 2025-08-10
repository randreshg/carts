///==========================================================================
// File: ConvertArtsToLLVM.cpp
//
// This file implements a pass to convert ARTS dialect operations into
// LLVM dialect operations. Since preprocessing is handled by a separate
// pass, this pass focuses purely on ARTS-specific conversion logic.
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Codegen/DbCodegen.h"
#include "arts/Codegen/EdtCodegen.h"
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

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(convert_arts_to_llvm);

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

  // DISABLED: Complex codegen functionality
  // ~ConvertArtsToLLVMPass() { delete AC; }

  void runOnOperation() override;

private:
  LogicalResult initializeCodegen();
  LogicalResult processOperations(func::FuncOp func);
  LogicalResult processEdtOperations(func::FuncOp func);
  void finalizeConversion();

  /// Operation handlers
  template <typename OpType> LogicalResult handleRuntimeInfoOp(OpType op);
  LogicalResult handleEdt(EdtOp op);
  LogicalResult handleEdtParamPack(EdtParamPackOp op);
  LogicalResult handleEdtOutline(EdtOutlineFnOp op);
  LogicalResult handleRecordInDep(RecordInDepOp op);
  LogicalResult handleIncrementOutLatch(IncrementOutLatchOp op);
  LogicalResult handleEpoch(EpochOp op);
  LogicalResult handleBarrier(BarrierOp op);
  LogicalResult handleAlloc(AllocOp op);
  LogicalResult handleDbAlloc(DbAllocOp op);
  // LogicalResult handleDbDep(DbDepOp op);

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
  DenseMap<Operation *, unsigned> edtSlotCounter;
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

  ARTS_DEBUG_HEADER(ConvertArtsToLLVMPass);
  ARTS_DEBUG(module.dump());

  /// Early exit if no ARTS operations
  if (!hasArtsOperations()) {
    ARTS_INFO("No ARTS operations found, skipping conversion");
    return;
  }

  /// Initialize codegen infrastructure
  if (failed(initializeCodegen())) {
    ARTS_ERROR("Failed to initialize ArtsCodegen");
    return signalPassFailure();
  }

  /// Process operations
  auto mainFunc = module.lookupSymbol<func::FuncOp>("main");
  if (failed(processOperations(mainFunc))) {
    ARTS_ERROR("Processing operations failed");
    return signalPassFailure();
  }

  ARTS_INFO("Module after processing operations (before finalization):");
  ARTS_DEBUG(module.dump());

  /// Finalize conversion and cleanup
  finalizeConversion();

  ARTS_DEBUG_FOOTER(ConvertArtsToLLVMPass);
  ARTS_DEBUG(module.dump());
}

LogicalResult ConvertArtsToLLVMPass::initializeCodegen() {
  /// Validate data layout
  auto llvmDLAttr = module->getAttrOfType<StringAttr>("llvm.data_layout");
  if (!llvmDLAttr)
    return emitOpError(module, "Module missing required LLVM data layout");

  llvm::DataLayout llvmDL(llvmDLAttr.getValue().str());
  mlir::DataLayout mlirDL(module);
  AC = new ArtsCodegen(module, llvmDL, mlirDL, debugMode);

  ARTS_DEBUG_TYPE("ArtsCodegen initialized successfully");
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
  ARTS_DEBUG_TYPE("Starting operation processing");

  /// Single-pass operation processing with ordered handling
  ARTS_INFO("1st pass - Runtime info operations");
  auto result = func->walk([&](Operation *op) -> WalkResult {
    if (auto opx = dyn_cast<arts::EdtParamPackOp>(op))
      return handleWalkResult(handleEdtParamPack(opx));

    if (auto opx = dyn_cast<arts::EdtOutlineFnOp>(op))
      return handleWalkResult(handleEdtOutline(opx));

    if (auto opx = dyn_cast<arts::RecordInDepOp>(op))
      return handleWalkResult(handleRecordInDep(opx));

    if (auto opx = dyn_cast<arts::IncrementOutLatchOp>(op))
      return handleWalkResult(handleIncrementOutLatch(opx));
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

    /// Handle DbDep operations - they are managed by DbDepCodegen but need to
    /// be removed
    /*
    if (auto dbDepOp = dyn_cast<arts::DbDepOp>(op))
      return handleWalkResult(handleDbDep(dbDepOp));
    */

    return WalkResult::advance();
  });

  if (result.wasInterrupted())
    return failure();

  /// Second pass for sync operations
  ARTS_INFO("2nd pass - Sync operations");
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
  ARTS_INFO("3rd pass - EDT operations");
  result = handleWalkResult(processEdtOperations(func));

  ARTS_INFO("Operation processing completed successfully");
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
  ARTS_DEBUG_TYPE("Processing runtime info operation: " << op);
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
  ARTS_DEBUG_TYPE("Processing EDT: " << op);
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

LogicalResult ConvertArtsToLLVMPass::handleEdtParamPack(EdtParamPackOp op) {
  ARTS_DEBUG_TYPE("Processing EdtParamPack: " << op);
  // No codegen required here; we only need paramc later. If needed, attach an
  // i32 constant count using the operands size.
  // For now, keep the value; replacement happens when lowering the outline
  // call.
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleEdtOutline(EdtOutlineFnOp op) {
  ARTS_DEBUG_TYPE("Processing EdtOutlineFn: " << op);
  statistics.edtOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  // Resolve outlined function
  auto funcNameAttr = op->getAttrOfType<StringAttr>("outlined_func");
  if (!funcNameAttr)
    return emitOpError(op, "Missing outlined_func attribute");
  auto outlined = module.lookupSymbol<func::FuncOp>(funcNameAttr.getValue());
  if (!outlined)
    return emitOpError(op, "Outlined function not found");

  // Build arguments for artsEdtCreate
  auto funcPtr = AC->createFnPtr(outlined, op.getLoc());
  auto route = AC->getCurrentNode(op.getLoc());

  // Compute paramc and depc conservatively to zero for now; paramv/depv are
  // packed before this op by EdtLowering; we use their memrefs as pointers.
  // Param pack is op.getParamv(); dep pack is op.getDepv();
  auto paramv = op.getParamMemref();
  (void)op.getDepMemref();

  auto zeroI32 = AC->createIntConstant(0, AC->Int32, op.getLoc());
  // If memref has dynamic length we skip computing exact count here; future
  // enhancement can derive sizes from memref type or attach attributes.
  auto paramc = zeroI32;
  auto depc = zeroI32;

  auto call = AC->createRuntimeCall(types::ARTSRTL_artsEdtCreate,
                                    {funcPtr, route, paramc, paramv, depc},
                                    op.getLoc());
  AC->addReplacement(op.getGuid(), call.getResult(0));
  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleRecordInDep(RecordInDepOp op) {
  ARTS_DEBUG_TYPE("Processing RecordInDep: " << op);
  auto edt = op.getEdtGuid().getDefiningOp();
  unsigned &slot = edtSlotCounter[edt];
  auto slotVal = AC->createIntConstant(slot, AC->Int32, op.getLoc());
  slot++;
  AC->addDbDep(op.getDep(), op.getEdtGuid(), slotVal, op.getLoc());
  opsToRemove.insert(op);
  return success();
}

LogicalResult
ConvertArtsToLLVMPass::handleIncrementOutLatch(IncrementOutLatchOp op) {
  ARTS_DEBUG_TYPE("Processing IncrementOutLatch: " << op);
  AC->incrementDbLatchCount(op.getDep(), op.getLoc());
  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleEpoch(EpochOp op) {
  ARTS_DEBUG_TYPE("Processing Epoch: " << op);
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

  ARTS_DEBUG_TYPE("Mapped " << childEdts.size() << " child EDTs to epoch");

  /// Set insertion point after epoch creation and add wait
  AC->setInsertionPointAfter(currentEpoch.getDefiningOp());
  AC->waitOnHandle(currentEpoch, op.getLoc());

  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleBarrier(BarrierOp op) {
  ARTS_DEBUG_TYPE("Processing Barrier: " << op);
  statistics.barrierOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  AC->createRuntimeCall(types::ARTSRTL_artsYield, {}, op.getLoc());

  opsToRemove.insert(op);
  return success();
}

LogicalResult ConvertArtsToLLVMPass::handleAlloc(AllocOp op) {
  ARTS_DEBUG_TYPE("Processing Alloc: " << op);
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
  ARTS_DEBUG_TYPE("Processing top-level DbAlloc: " << op);
  statistics.dbAllocOps++;

  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);

  auto *dbCodegen = AC->getOrCreateDbAlloc(op, op.getLoc());
  if (!dbCodegen)
    return emitOpError(op, "Failed to create DbAllocCodegen");

  AC->addReplacement(op.getResult(), dbCodegen->getPtr());
  opsToRemove.insert(op);
  ARTS_DEBUG_TYPE("DbAlloc processing completed successfully");

  return success();
}

/*
LogicalResult ConvertArtsToLLVMPass::handleDbDep(DbDepOp op) {
  LLVM_DEBUG(DBGS() << "Processing DbDep: " << op << "\n");
  statistics.dbDepOps++;

  /// DbDep operations are processed through DbDepCodegen objects
  /// when their source DbAlloc operations are handled. Mark them for removal
  /// since their replacements will be set up by the DbDepCodegen
infrastructure. opsToRemove.insert(op); LLVM_DEBUG(DBGS() << "DbDep marked for
removal: " << op << "\n");

  return success();
}
*/

//===----------------------------------------------------------------------===//
// Utility Methods
//===----------------------------------------------------------------------===//

void ConvertArtsToLLVMPass::cleanupOperations() {
  ARTS_DEBUG_TYPE("Cleaning up " << opsToRemove.size() << " operations");

  for (Operation *op : opsToRemove) {
    if (op && op->getParentOp())
      op->erase();
  }
  opsToRemove.clear();
}

void ConvertArtsToLLVMPass::reportStatistics() const {
  ARTS_DEBUG_REGION(
      ARTS_DBGS() << "\n"
                  << LINE << "ARTS to LLVM Conversion Statistics\n"
                  << LINE;
      ARTS_DBGS() << "EDT operations: " << statistics.edtOps << "\n";
      ARTS_DBGS() << "Epoch operations: " << statistics.epochOps << "\n";
      ARTS_DBGS() << "Barrier operations: " << statistics.barrierOps << "\n";
      ARTS_DBGS() << "Alloc operations: " << statistics.allocOps << "\n";
      ARTS_DBGS() << "DbAlloc operations: " << statistics.dbAllocOps << "\n";
      ARTS_DBGS() << "DbDep operations: " << statistics.dbDepOps << "\n";
      ARTS_DBGS() << "Runtime Info operations: " << statistics.runtimeInfoOps
                  << "\n";
      ARTS_DBGS() << LINE;);
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
