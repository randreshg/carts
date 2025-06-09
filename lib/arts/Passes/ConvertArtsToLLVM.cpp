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

  ConvertArtsToLLVMPass(bool debug = false) : debug(debug) {}

  void runOnOperation() override;

  void iterateOps();
  void preprocessDbControlOps(Operation *op);
  void handleEdt(EdtOp &op);
  // void handleEvent(EventOp &op);
  void handleEpoch(EpochOp &op);
  void handleDbCreate(DbCreateOp &op);
  void handleDb(DbControlOp &op);
  void handleGetTotalWorkers(GetTotalWorkersOp &op);
  void handleGetTotalNodes(GetTotalNodesOp &op);
  void handleGetCurrentWorker(GetCurrentWorkerOp &op);
  void handleGetCurrentNode(GetCurrentNodeOp &op);

private:
  Value computeDbSize(DbCreateOp &op, Location loc);

private:
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  DenseMap<EdtOp, Value> edtToEpoch;
  SetVector<Operation *> opsToRemove;
  bool debug = false;
};
} // end namespace

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

void ConvertArtsToLLVMPass::handleDbCreate(DbCreateOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.db_create\n");
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  
  Location loc = op->getLoc();
  
  // Convert mode string to ARTS mode enum
  StringRef mode = op.getMode();
  Value modeVal = AC->getDbMode(mode, loc);
  
  // Get current node for GUID creation
  Value currentNode = AC->getCurrentNode(loc);
  
  // Create GUID using ARTS runtime
  Value guid = AC->createRuntimeCall(ARTSRTL_artsReserveGuidRoute, {modeVal, currentNode}, loc)->getResult(0);
  
  // Compute db size
  Value size = computeDbSize(op, loc);
  
  // Create the db
  Value ptr;
  if (op.getAddress()) {
    // Use address if provided
    Value address = AC->castToVoidPtr(op.getAddress(), loc);
    ptr = AC->createRuntimeCall(ARTSRTL_artsDbCreateWithGuidAndData, {guid, address, size}, loc)->getResult(0);
  } else {
    // Create empty db
    ptr = AC->createRuntimeCall(ARTSRTL_artsDbCreateWithGuid, {guid, size}, loc)->getResult(0);
  }
  
  // Cast pointer back to the expected type
  Value typedPtr = AC->castToVoidPtr(ptr, loc);
  
  // Replace the results
  op.getPtr().replaceAllUsesWith(typedPtr);
  
  // Mark for removal
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleDb(DbControlOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.db\n  " << op << "\n");
  AC->setInsertionPointAfter(op);

  /// Create a new db codegen object.
  AC->createDb(op, op->getLoc());

  /// Mark ops for removal.
  opsToRemove.insert(op.getPtr().getDefiningOp());
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetTotalWorkers(GetTotalWorkersOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getTotalWorkers\n");
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  auto totalWorkers = AC->getTotalWorkers(op.getLoc());
  op.replaceAllUsesWith(totalWorkers);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetTotalNodes(GetTotalNodesOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getTotalNodes\n");
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  auto totalNodes = AC->getTotalNodes(op.getLoc());
  op.replaceAllUsesWith(totalNodes);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetCurrentWorker(GetCurrentWorkerOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getCurrentWorker\n");
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(op);
  auto currentWorker = AC->getCurrentWorker(op.getLoc());
  op.replaceAllUsesWith(currentWorker);
  opsToRemove.insert(op);
}

void ConvertArtsToLLVMPass::handleGetCurrentNode(GetCurrentNodeOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.getCurrentNode\n");
  auto currentNode = AC->getCurrentNode(op.getLoc());
  op.replaceAllUsesWith(currentNode);
  opsToRemove.insert(op);
}



Value ConvertArtsToLLVMPass::computeDbSize(DbCreateOp &op, Location loc) {
  // Compute the total size based on the sizes operands
  auto sizes = op.getSizes();
  if (sizes.empty()) {
    // If no sizes provided, use the size of the address memref
    if (op.getAddress()) {
      auto memrefType = op.getAddress().getType().cast<MemRefType>();
      // For memref types, compute size as product of dimensions times element size
      auto elementType = memrefType.getElementType();
      
      // Get element size in bytes
      Value elementSize = AC->getBuilder().create<polygeist::TypeSizeOp>(
          loc, AC->getBuilder().getIndexType(), elementType);
      Value elementSizeInt64 = AC->castToInt(AC->Int64, elementSize, loc);
      
      // Multiply by total number of elements
      Value totalElements = AC->createIntConstant(1, AC->Int64, loc);
      for (int64_t dimSize : memrefType.getShape()) {
        if (dimSize != ShapedType::kDynamic) {
          Value dim = AC->createIntConstant(dimSize, AC->Int64, loc);
          totalElements = AC->getBuilder().create<arith::MulIOp>(loc, totalElements, dim);
        }
      }
      
      return AC->getBuilder().create<arith::MulIOp>(loc, totalElements, elementSizeInt64);
    }
    // Default size if nothing else
    return AC->createIntConstant(1, AC->Int64, loc);
  }
  
  // Multiply all sizes together
  Value totalSize = AC->createIntConstant(1, AC->Int64, loc);
  for (Value size : sizes) {
    Value sizeAsInt64 = AC->castToInt(AC->Int64, size, loc);
    totalSize = AC->getBuilder().create<arith::MulIOp>(loc, totalSize, sizeAsInt64);
  }
  
  return totalSize;
}

void ConvertArtsToLLVMPass::iterateOps() {
  /// Iterate over the FuncOps/DbControlOps in the module
  for (auto func : module.getOps<func::FuncOp>())
    preprocessDbControlOps(func);
  LLVM_DEBUG({
    dbgs() << "Module after preprocessing DataBlocks:\n";
    module.dump();
    dbgs() << line;
  });

  /// Lower all get-* ops in one pass
  module->walk([&](arts::GetTotalWorkersOp op) { handleGetTotalWorkers(op); });
  module->walk([&](arts::GetTotalNodesOp op) { handleGetTotalNodes(op); });
  module->walk(
      [&](arts::GetCurrentWorkerOp op) { handleGetCurrentWorker(op); });
  module->walk([&](arts::GetCurrentNodeOp op) { handleGetCurrentNode(op); });

  //  Iterate over the EpochsOps and EventOps in the module
  module.walk([&](arts::EpochOp epoch) { handleEpoch(epoch); });
  LLVM_DEBUG({
    DBGS() << "Module after iterating Epochs and Events:\n";
    module.dump();
    dbgs() << line;
  });

  /// Iterate over the DbCreateOps in the module
  module.walk([&](arts::DbCreateOp op) { handleDbCreate(op); });

  /// Iterate over the EdtOps in the module
  module.walk<mlir::WalkOrder::PreOrder>([&](arts::EdtOp edtOp) {
    if (opsToRemove.count(edtOp))
      return mlir::WalkResult::skip();
    if (edtOp.isTask())
      handleEdt(edtOp);
    else {
      llvm_unreachable(
          "Sync, Parallel, and single, EDTs should have been lowered. "
          "Did you run the 'create-epochs' pass?");
    }
    return mlir::WalkResult::advance();
  });
  removeOps(module, AC->getBuilder(), opsToRemove);

  LLVM_DEBUG({
    DBGS() << "Module after iterating edts:\n";
    module.dump();
    dbgs() << line;
  });
}

void ConvertArtsToLLVMPass::preprocessDbControlOps(Operation *operation) {
  auto &builder = AC->getBuilder();
  /// Iterate over all DbControlOps and create new DbControlOps with opaque
  /// pointers.
  SmallVector<DbControlOp, 8> dbOps;
  DenseMap<DbControlOp, DbControlOp> dbsToRewire;
  operation->walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DbControlOp dbOp) -> mlir::WalkResult {
        /// Skip already processed new db ops.
        if (dbOp->hasAttr("newDb"))
          return mlir::WalkResult::skip();

        AC->setInsertionPointAfter(dbOp);
        auto sizes = dbOp.getSizes();

        auto getOpaqueElementType = [&]() -> Type {
          /// Coarse-grained datablocks are always void pointers.
          if (dbOp.hasSingleSize())
            return MemRefType::get({ShapedType::kDynamic}, AC->VoidPtr);

          /// Fine-grained datablocks copy the shape of the original memref.
          auto elementType = dbOp.getElementType();
          if (auto memrefType = elementType.dyn_cast<MemRefType>())
            return MemRefType::get(memrefType.getShape(), AC->VoidPtr);

          /// If the element type is not a memref, it is a pointer type.
          return AC->VoidPtr;
        };

        auto getPointerType = [&](Type inputType, Type elementType) -> Type {
          if (auto memrefType = inputType.dyn_cast<MemRefType>())
            return MemRefType::get(memrefType.getShape(), elementType);
          return MemRefType::get({ShapedType::kDynamic}, elementType);
        };

        auto elementType = getOpaqueElementType();
        auto elementPtrType =
            dbOp.hasSingleSize()
                ? elementType
                : getPointerType(dbOp.getResult().getType(), elementType);

        auto newDbOp = builder.create<arts::DbControlOp>(
            dbOp->getLoc(), elementPtrType, dbOp.getMode(), dbOp.getPtr(),
            dbOp.getElementType(), dbOp.getElementTypeSize(), dbOp.getIndices(),
            dbOp.getOffsets(), sizes);
        newDbOp->setAttrs(dbOp->getAttrs());
        newDbOp->setAttr("newDb", builder.getUnitAttr());

        dbOps.push_back(dbOp);
        dbsToRewire[dbOp] = newDbOp;
        return mlir::WalkResult::advance();
      });

  /// Return if no datablocks to rewire
  if (dbsToRewire.empty())
    return;

  /// Rewire the datablocks
  SmallVector<DbControlOp, 8> dbsToHandle;
  for (auto dbFrom : dbOps) {
    auto dbTo = dbsToRewire[dbFrom];
    LLVM_DEBUG(dbgs() << "Rewiring db:\n  " << dbFrom << "\n  " << dbTo
                      << "\n");

    /// Get the new db pointer type
    auto computePtr = [&](ValueRange indices, Location loc) -> Value {
      if (indices.empty()) {
        auto llvmCast = AC->castToLLVMPtr(dbTo, loc);
        if (dbTo.hasSingleSize())
          return llvmCast;

        return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmCast);
      }

      /// Create a SubIndexOp for newPtr
      Value subIndex = dbTo;
      for (auto index : indices) {
        auto mt = subIndex.getType().cast<MemRefType>();
        auto shape = SmallVector<int64_t>(mt.getShape());
        shape.erase(shape.begin());
        auto mt0 = mlir::MemRefType::get(shape, mt.getElementType(),
                                         MemRefLayoutAttrInterface(),
                                         mt.getMemorySpace());
        subIndex =
            builder.create<polygeist::SubIndexOp>(loc, mt0, subIndex, index);
      }

      Value subIndexPtr = AC->castToLLVMPtr(subIndex, loc);
      if (dbTo.hasSingleSize())
        return subIndexPtr;
      return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, subIndexPtr);
    };

    /// If not, load the appropriate pointer type and replace the old op with
    /// the new one.
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
      /// Propagate db pointer for nested db ops.
      else if (auto dbUseOp = dyn_cast<arts::DbControlOp>(user)) {
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
      }
      /// Memref2PointerOp - update the pointer type.
      else if (auto memref2PtrOp =
                   dyn_cast<polygeist::Memref2PointerOp>(user)) {
        auto loc = memref2PtrOp.getLoc();
        auto newMemref2PtrOp = builder.create<polygeist::Memref2PointerOp>(
            loc, memref2PtrOp.getResult().getType(), dbTo.getResult());
        memref2PtrOp.getResult().replaceAllUsesWith(
            newMemref2PtrOp.getResult());
        memref2PtrOp.erase();
      } else {
        LLVM_DEBUG(DBGS() << "Unknown use of db op: " << *user << "\n");
        llvm_unreachable("Unknown use of db op");
      }
    }

    /// Erase the old db op
    dbFrom->erase();

    /// Add the new db to the list of datablocks to handle
    dbsToHandle.push_back(dbTo);
  }
  removeOps(module, builder, opsToRemove);

  /// Handle the new datablocks
  LLVM_DEBUG(dbgs() << line << "Handling new datablocks\n");
  for (auto db : dbsToHandle)
    handleDb(db);
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
  AC = new ArtsCodegen(module, llvmDL, mlirDL, debug);
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

std::unique_ptr<Pass> createConvertArtsToLLVMPass(bool debug) {
  return std::make_unique<ConvertArtsToLLVMPass>(debug);
}

} // namespace arts
} // namespace mlir
