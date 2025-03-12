///==========================================================================
// File: ConvertArtsToLLVM.cpp
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
///==========================================================================

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
// #include "polygeist/Ops.h"
/// Arts
#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
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
  void runOnOperation() override;

  void iterateOps(Operation *op);
  void iterateDataBlockOps(Operation *op);
  void handleParallel(EdtOp &op);
  void handleSingle(EdtOp &op);
  void handleSync(EdtOp &op);
  void handleEdt(EdtOp &op);
  void handleEvent(EventOp &op);
  void handleDatablock(DataBlockOp &op);

private:
  ArtsCodegen *AC = nullptr;
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void ConvertArtsToLLVMPass::handleParallel(EdtOp &op) {
  /// Set insertion point to the current op for epoch creation.
  llvm_unreachable("Parallel EDTs not supported yet");
}

void ConvertArtsToLLVMPass::handleSync(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt sync\n" << line);
  auto *ctx = op.getContext();
  Location loc = UnknownLoc::get(ctx);
  mlir::Region &region = op.getRegion();

  /// Set insertion point to the current op for epoch creation.
  EdtCodegen *newEdt = nullptr;
  {
    OpBuilder::InsertionGuard guard(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Create a done-edt for the sync Edt epoch.
    EdtCodegen syncDoneEdt(*AC);
    syncDoneEdt.setDepC(AC->createIntConstant(1, AC->Int32, loc));
    syncDoneEdt.build(loc);
    LLVM_DEBUG(DBGS() << "Sync done EDT created\n");

    /// Create the parallel epoch.
    auto parDone_slot = AC->createIntConstant(0, AC->Int32, loc);
    auto parEpoch_guid =
        AC->createEpoch(syncDoneEdt.getGuid(), parDone_slot, loc);
    LLVM_DEBUG(DBGS() << "Parallel epoch created\n");

    /// Create the parallel EDT with the single op's region.
    auto par_dependencies = op.getDependenciesVector();
    newEdt = AC->createEdt(&par_dependencies, &region, &parEpoch_guid, nullptr);
    LLVM_DEBUG(DBGS() << "Parallel EDT created\n");

    /// Insert wait on handle after parallel EDT.
    AC->setInsertionPointAfter(op);
    AC->waitOnHandle(parEpoch_guid, loc);
  }

  /// Erase the old parallel op.
  opsToRemove.insert(op);
  LLVM_DEBUG(DBGS() << "Parallel op lowered\n\n");

  /// Visit new function
  assert(newEdt && "New EDT not created");
  iterateOps(newEdt->getFunc());
}

void ConvertArtsToLLVMPass::handleSingle(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt single\n");
  llvm_unreachable("Single EDTs not supported yet");
}

void ConvertArtsToLLVMPass::handleEdt(EdtOp &op) {
  auto *ctx = op.getContext();
  Location loc = UnknownLoc::get(ctx);
  LLVM_DEBUG(DBGS() << "Lowering arts.edt\n");
  EdtCodegen *newEdt = nullptr;
  {
    OpBuilder::InsertionGuard guard(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Create the parallel EDT with the single op's region.
    auto epoch = AC->getCurrentEpochGuid(loc);
    auto dependencies = op.getDependenciesVector();
    auto &region = op.getRegion();
    newEdt = AC->createEdt(&dependencies, &region, &epoch, nullptr);
  }

  /// Erase the old edt
  opsToRemove.insert(op);

  /// Visit new function
  assert(newEdt && "New EDT not created");
  iterateOps(newEdt->getFunc());
}

void ConvertArtsToLLVMPass::handleDatablock(DataBlockOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.datablock\n");
  AC->setInsertionPoint(op);
  auto &builder = AC->getBuilder();

  /// Create a new DataBlockOp with an opaque pointer type
  auto sizes = op.getSizes();
  auto pointerType = op.isSingle()
                         ? AC->VoidPtr
                         : MemRefType::get({ShapedType::kDynamic}, AC->VoidPtr);
  auto newDbOp = builder.create<arts::DataBlockOp>(
      op->getLoc(), pointerType, op.getMode(), op.getPtr(), op.getElementType(),
      op.getElementTypeSize(), op.getIndices(), sizes, op.getInEvent(),
      op.getOutEvent());
  newDbOp->setAttrs(op->getAttrs());

  /// Helper lambda to compute the pointer based on the indices.
  auto computePtr = [&](ValueRange indices, Type elementType,
                        Location loc) -> Value {
    auto newPtr = AC->castToLLVMPtr(newDbOp, loc);
    if (indices.empty())
      return newPtr;
    // return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, newPtr);

    /// Cast indices to int32
    SmallVector<Value> newIndices;
    newIndices.reserve(indices.size());

    for (auto index : indices)
      newIndices.push_back(AC->castToInt(AC->Int64, index, loc));
    auto gepOp = builder
                     .create<LLVM::GEPOp>(loc, AC->llvmPtr, AC->llvmPtr, newPtr,
                                          newIndices)
                     .getResult();
    return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, gepOp);
  };

  /// Lambda to process load operations (for both memref::LoadOp and
  /// affine::AffineLoadOp).
  auto processLoad = [&](auto loadOp) {
    auto loc = loadOp.getLoc();
    auto newPtr = computePtr(loadOp.getIndices(), loadOp.getType(), loc);
    auto newLoad = builder
                       .create<LLVM::LoadOp>(
                           loc, loadOp.getMemRefType().getElementType(), newPtr)
                       .getResult();
    loadOp.getResult().replaceAllUsesWith(newLoad);
    loadOp.erase();
  };

  /// Lambda to process store operations (for both memref::StoreOp and
  /// affine::AffineStoreOp).
  auto processStore = [&](auto storeOp) {
    auto loc = storeOp.getLoc();
    auto newPtr = computePtr(storeOp.getIndices(),
                             storeOp.getValueToStore().getType(), loc);
    builder.create<LLVM::StoreOp>(loc, storeOp.getValueToStore(), newPtr);
    storeOp.erase();
  };

  /// Walk all uses of the old datablock to properly replace it with the new
  /// memref. Ignore uses in arts operations.
  for (auto &use : llvm::make_early_inc_range(op->getUses())) {
    /// Set insertion point to the user
    auto user = use.getOwner();
    AC->setInsertionPoint(user);

    /// memref.load or affine.load
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      processLoad(loadOp);
    } else if (auto loadOp = dyn_cast<affine::AffineLoadOp>(user)) {
      processLoad(loadOp);
    }
    /// memref.store or affine.store
    else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      processStore(storeOp);
    } else if (auto storeOp = dyn_cast<affine::AffineStoreOp>(user)) {
      processStore(storeOp);
    }
    /// Propagate datablock pointer for nested datablock ops.
    else if (auto dbOp = dyn_cast<arts::DataBlockOp>(user)) {
      dbOp.getPtr().replaceUsesWithIf(
          newDbOp.getResult(),
          [&](OpOperand &operand) { return operand.getOwner() == dbOp; });
    }
    /// EdtOp - update dependencies.
    else if (auto edtOp = dyn_cast<arts::EdtOp>(user)) {
      auto edtDeps = edtOp.getDependencies();
      for (auto dep : edtDeps) {
        if (dep != op)
          continue;
        dep.replaceUsesWithIf(newDbOp.getResult(), [&](OpOperand &operand) {
          return operand.getOwner() == edtOp;
        });
      }
    } else {
      LLVM_DEBUG(DBGS() << "Unknown use of datablock op: " << *user << "\n");
      llvm_unreachable("Unknown use of datablock op");
    }
  }

  /// Create a new datablock codegen object.
  AC->createDatablock(newDbOp, op->getLoc());

  /// Mark dbs for removal.
  // opsToRemove.insert(op);
  op.erase();
  opsToRemove.insert(newDbOp.getPtr().getDefiningOp());
  opsToRemove.insert(newDbOp);
}

void ConvertArtsToLLVMPass::handleEvent(EventOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.event\n");
  AC->getOrCreateEvent(op, op->getLoc());
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::iterateOps(Operation *operation) {
  operation->walk<mlir::WalkOrder::PreOrder>(
      [&](Operation *op) -> mlir::WalkResult {
        /// Skip operations marked for removal.
        if (opsToRemove.count(op))
          return mlir::WalkResult::skip();

        if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
          handleDatablock(dbOp);
          return mlir::WalkResult::advance();
        } else if (auto edtOp = dyn_cast<arts::EdtOp>(op)) {
          if (edtOp.isParallel())
            handleParallel(edtOp);
          else if (edtOp.isSync())
            handleSync(edtOp);
          else if (edtOp.isSingle())
            handleSingle(edtOp);
          else
            handleEdt(edtOp);
          return mlir::WalkResult::advance();
        } else if (auto allocEventOp = dyn_cast<arts::EventOp>(op)) {
          handleEvent(allocEventOp);
          return mlir::WalkResult::advance();
        }
        return mlir::WalkResult::advance();
      });
}

void ConvertArtsToLLVMPass::iterateDataBlockOps(Operation *operation) {
  operation->walk<mlir::WalkOrder::PreOrder>(
      [&](Operation *op) -> mlir::WalkResult {
        /// Skip operations marked for removal.
        if (opsToRemove.count(op))
          return mlir::WalkResult::skip();
        if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
          handleDatablock(dbOp);
          return mlir::WalkResult::advance();
        }
        return mlir::WalkResult::advance();
      });
}

void ConvertArtsToLLVMPass::runOnOperation() {
  ModuleOp module = getOperation();
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

  /// Initialize the AC object
  AC = new ArtsCodegen(module, llvmDL, mlirDL);

  LLVM_DEBUG(DBGS() << "Iterate over all the functions\n");
  for (auto func : module.getOps<func::FuncOp>())
    iterateDataBlockOps(func);
  LLVM_DEBUG({
    dbgs() << "Module after iterating Datablocks:\n";
    module.dump();
  });

  for (auto func : module.getOps<func::FuncOp>())
    iterateOps(func);
  removeOps(module, AC->getBuilder(), opsToRemove);

  /// Initialize the runtime
  AC->initializeRuntime(UnknownLoc::get(module.getContext()));

  /// Create a ConversionTarget.
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
} // namespace arts
} // namespace mlir
