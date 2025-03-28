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

  void iterateOps();
  void preprocessDataBlockOps(Operation *op);
  void handleParallel(EdtOp &op);
  void handleEdt(EdtOp &op);
  void handleEvent(EventOp &op);
  void handleEpoch(EpochOp &op);
  void handleDatablock(DataBlockOp &op);

private:
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  DenseMap<EdtOp, Value> edtToEpoch;
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void ConvertArtsToLLVMPass::handleParallel(EdtOp &op) {
  /// Set insertion point to the current op for epoch creation.
  llvm_unreachable("Parallel EDTs not supported yet");
}

void ConvertArtsToLLVMPass::handleEdt(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt\n");
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

void ConvertArtsToLLVMPass::handleEpoch(EpochOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.epoch: " << op << "\n");
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

  /// Move epoch region operations out of the epoch op and insert them after the
  /// current epoch.
  Operation *currentEpochOp = currentEpoch.getDefiningOp();

  /// Assume the epoch region has a single block.
  Block *epochBlock = &op.getRegion().front();

  Operation *currentOp = currentEpochOp;
  for (Operation &childOp : llvm::make_early_inc_range(*epochBlock)) {
    if (isa<arts::YieldOp>(childOp))
      continue;
    /// If the operation is an edt, add it to the map
    if (auto edtOp = dyn_cast<arts::EdtOp>(&childOp)) {
      edtToEpoch[edtOp] = currentEpoch;
    }
    /// Move the operation after the current epoch op.
    childOp.moveAfter(currentOp);
    currentOp = &childOp;
  }
  AC->setInsertionPointAfter(currentOp);

  /// Insert wait on handle after epoch.
  AC->waitOnHandle(currentEpoch, loc);

  /// Remove the epoch op.
  op->erase();
}

void ConvertArtsToLLVMPass::iterateOps() {
  /// Iterate over the FuncOps/DataBlockOps in the module
  for (auto func : module.getOps<func::FuncOp>())
    preprocessDataBlockOps(func);
  LLVM_DEBUG({
    dbgs() << "Module after preprocessing DataBlocks:\n";
    module.dump();
    dbgs() << line;
  });

  /// Iterate over the EpochsOps and EventOps in the module
  module.walk([&](arts::EpochOp epoch) { handleEpoch(epoch); });
  module.walk([&](arts::EventOp event) { handleEvent(event); });
  LLVM_DEBUG({
    DBGS() << "Module after iterating DataBlocks and Events:\n";
    module.dump();
    dbgs() << line;
  });

  /// Iterate over the EdtOps in the module
  module.walk<mlir::WalkOrder::PreOrder>([&](arts::EdtOp edtOp) {
    if (opsToRemove.count(edtOp))
      return mlir::WalkResult::skip();
    if (edtOp.isTask())
      handleEdt(edtOp);
    else if (edtOp.isParallel())
      handleParallel(edtOp);
    else {
      llvm_unreachable("Sync, single, and other EDTs should have been lowered. "
                       "Did you run the 'create-epochs' pass?");
    }
    return mlir::WalkResult::advance();
  });

  LLVM_DEBUG({
    DBGS() << "Module after iterating edts:\n";
    module.dump();
    dbgs() << line;
  });

  removeOps(module, AC->getBuilder(), opsToRemove);
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

        /// Create a new DataBlockOp with an opaque pointer type
        AC->setInsertionPointAfter(dbOp);
        auto sizes = dbOp.getSizes();
        auto pointerType = dbOp.isSingle() ? AC->VoidPtr : [&]() -> Type {
          /// Try to use the static shape from the op result type for the outer
          /// memref.
          if (auto memrefType =
                  dbOp.getResult().getType().dyn_cast<MemRefType>()) {
            return MemRefType::get(memrefType.getShape(), AC->VoidPtr);
          }
          /// Fallback: use a dynamic shape pointer.
          return MemRefType::get({ShapedType::kDynamic}, AC->VoidPtr);
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

  /// Initialize the AC object, iterate ops and initialize the runtime.
  AC = new ArtsCodegen(module, llvmDL, mlirDL);
  iterateOps();
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
