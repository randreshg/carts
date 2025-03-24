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
  void runOnOperation() override;

  void iterateOps(Operation *op);
  void preprocessDataBlockOps(Operation *op);
  void handleParallel(EdtOp &op);
  void handleSingle(EdtOp &op);
  void handleSync(EdtOp &op);
  void handleEdt(EdtOp &op);
  void handleEvent(EventOp &op);
  void handleDatablock(DataBlockOp &op);

private:
  ModuleOp module;
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
  LLVM_DEBUG(DBGS() << "Lowering arts.datablock\n  " << op << "\n");
  AC->setInsertionPointAfter(op);

  /// Create a new datablock codegen object.
  AC->createDatablock(op, op->getLoc());

  /// Mark ops for removal.
  opsToRemove.insert(op.getPtr().getDefiningOp());
  opsToRemove.insert(op);
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

        /// Handle ARTS operations.
        if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
          // handleDatablock(dbOp);
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

void ConvertArtsToLLVMPass::preprocessDataBlockOps(Operation *operation) {
  auto &builder = AC->getBuilder();
  /// Iterate over all DataBlockOps and create new DataBlockOps with opaque
  /// pointers.
  DenseMap<DataBlockOp, DataBlockOp> dbsToRewire;
  operation->walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DataBlockOp dbOp) -> mlir::WalkResult {
        /// Skip already processed (new) datablock ops.
        if (dbOp->hasAttr("newDbPtr"))
          return mlir::WalkResult::skip();

        // LLVM_DEBUG(dbgs() << "Processing datablock op:\n  " << dbOp << "\n");
        AC->setInsertionPointAfter(dbOp);
        /// Create a new DataBlockOp with an opaque pointer type
        auto sizes = dbOp.getSizes();
        auto pointerType = dbOp.isSingle() ? AC->VoidPtr : [&]() -> Type {
          /// Try to use the op result type.
          if (auto memrefType =
                  dbOp.getResult().getType().dyn_cast<MemRefType>()) {
            /// Use the static shape from the op result type for the outer
            /// memref.
            return MemRefType::get(memrefType.getShape(), AC->VoidPtr);
          } else {
            /// Fallback: use a dynamic shape pointer.
            return MemRefType::get({ShapedType::kDynamic}, AC->VoidPtr);
          }
        }();

        auto newDbOp = builder.create<arts::DataBlockOp>(
            dbOp->getLoc(), pointerType, dbOp.getMode(), dbOp.getPtr(),
            dbOp.getElementType(), dbOp.getElementTypeSize(), dbOp.getIndices(),
            sizes, dbOp.getInEvent(), dbOp.getOutEvent());
        newDbOp->setAttrs(dbOp->getAttrs());
        /// Mark the new datablock so that it is not processed again.
        newDbOp->setAttr("newDbPtr", builder.getUnitAttr());

        /// Insert to rewire map
        dbsToRewire[dbOp] = newDbOp;

        return mlir::WalkResult::advance();
      });

  /// Return if no datablocks to rewire
  if (dbsToRewire.empty())
    return;

  /// Rewire the datablocks
  // LLVM_DEBUG(dbgs() << "Dbs to be rewired size: " << dbsToRewire.size()
  //                   << "\n");
  SmallVector<DataBlockOp, 8> dbsToHandle;
  for (auto &rewire : dbsToRewire) {
    auto dbFrom = rewire.first;
    auto dbTo = rewire.second;
    LLVM_DEBUG(dbgs() << "Rewiring datablock:\n  " << dbFrom << "\n  " << dbTo
                      << "\n");

    /// Get the new datablock pointer type
    auto computePtr = [&](ValueRange indices, Location loc) -> Value {
      if (indices.empty())
        return AC->castToLLVMPtr(dbTo, loc);

      /// Create a SubIndexOp for newPtr
      Value subIndex = dbTo;
      for (auto index : indices) {
        auto mt = subIndex.getType().cast<MemRefType>();
        auto shape = std::vector<int64_t>(mt.getShape());
        shape.erase(shape.begin());
        auto mt0 = mlir::MemRefType::get(shape, mt.getElementType(),
                                         MemRefLayoutAttrInterface(),
                                         mt.getMemorySpace());
        subIndex =
            builder.create<polygeist::SubIndexOp>(loc, mt0, subIndex, index);
      }

      Value subIndexPtr = AC->castToLLVMPtr(subIndex, loc);
      return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, subIndexPtr);
    };

    /// Walk all uses of the old datablock to properly replace it with the new
    /// memref. Ignore uses in arts operations.
    for (auto &use : llvm::make_early_inc_range(dbFrom->getUses())) {
      /// Set insertion point to the user
      auto user = use.getOwner();
      AC->setInsertionPoint(user);

      /// memref.load
      if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
        auto loc = loadOp.getLoc();
        auto newPtr = computePtr(loadOp.getIndices(), loc);
        auto newLoad =
            builder
                .create<LLVM::LoadOp>(
                    loc, loadOp.getMemRefType().getElementType(), newPtr)
                .getResult();
        loadOp.getResult().replaceAllUsesWith(newLoad);
        loadOp.erase();
      }
      /// memref.store
      else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        auto loc = storeOp.getLoc();
        auto newPtr = computePtr(storeOp.getIndices(), loc);
        builder.create<LLVM::StoreOp>(loc, storeOp.getValueToStore(), newPtr);
        storeOp.erase();
      }
      /// Propagate datablock pointer for nested datablock ops.
      else if (auto dbUseOp = dyn_cast<arts::DataBlockOp>(user)) {
        dbUseOp.getPtr().replaceUsesWithIf(
            dbTo.getResult(),
            [&](OpOperand &operand) { return operand.getOwner() == dbUseOp; });
      }
      /// EdtOp - update dependencies.
      else if (auto edtOp = dyn_cast<arts::EdtOp>(user)) {
        auto edtDeps = edtOp.getDependencies();
        for (auto dep : edtDeps) {
          if (dep != dbFrom)
            continue;
          dep.replaceUsesWithIf(dbTo.getResult(), [&](OpOperand &operand) {
            return operand.getOwner() == edtOp;
          });
        }
      } else {
        LLVM_DEBUG(DBGS() << "Unknown use of datablock op: " << *user << "\n");
        llvm_unreachable("Unknown use of datablock op");
      }
    }

    /// Erase the old datablock op
    opsToRemove.insert(dbFrom);

    /// Add the new datablock to the list of datablocks to handle
    dbsToHandle.push_back(dbTo);
  }
  removeOps(module, builder, opsToRemove);

  /// Handle the new datablocks
  LLVM_DEBUG(dbgs() << line << "Handling new datablocks\n");
  for (auto db : dbsToHandle)
    handleDatablock(db);
}

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

  /// Initialize the AC object
  AC = new ArtsCodegen(module, llvmDL, mlirDL);

  LLVM_DEBUG(DBGS() << "Preprocess Datablocks\n");
  for (auto func : module.getOps<func::FuncOp>())
    preprocessDataBlockOps(func);
  LLVM_DEBUG({
    DBGS() << "Module after iterating Datablocks:\n";
    module.dump();
    dbgs() << line;
  });

  LLVM_DEBUG(DBGS() << "Iterating ops\n");
  for (auto func : module.getOps<func::FuncOp>())
    iterateOps(func);
  removeOps(module, AC->getBuilder(), opsToRemove);

  /// Initialize the runtime
  AC->initializeRuntime(UnknownLoc::get(module.getContext()));

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
