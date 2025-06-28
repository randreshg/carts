///==========================================================================
/// File: PreprocessDbs.cpp
///
/// This pass preprocesses ARTS DataBlocks to convert them from typed memrefs
/// to opaque pointer types suitable for the ARTS runtime. The preprocessing
/// logic differs based on the original allocation type (stack vs dynamic).
/// This passs should run before the ConvertToLLVM pass, as it lowers the
/// load and store operations to LLVM IR.
///==========================================================================

/// Dialects
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
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Ops.h"

/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "preprocess-dbs"
#define LINE "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//
DbAllocOp createDbAlloc(OpBuilder &builder, Location loc, Value basePtr,
                        types::DbDepType dep) {
  /// Create the DbAllocOp using the existing function to ensure sizes are
  /// filled out
  auto dbAllocOp = createDbAllocOp(builder, loc, types::toString(dep), basePtr);

  /// Set the allocation type attribute using the helper function
  auto allocType = arts::getAllocType(basePtr);
  setDbAllocType(dbAllocOp, allocType, builder);

  LLVM_DEBUG(DBGS() << "Created DbAllocOp with allocation type: "
                    << types::toString(allocType) << "\n");
  return dbAllocOp;
}

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct PreprocessDbsPass : public arts::PreprocessDbsBase<PreprocessDbsPass> {
  void runOnOperation() override;

private:
  void preprocessDbAllocOps(ModuleOp module);
  void preprocessDbDepOps(ModuleOp module);
  void processDbOpUsers(Operation *dbFrom, Operation *dbTo, OpBuilder &builder);

private:
  ArtsCodegen *AC = nullptr;
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void PreprocessDbsPass::runOnOperation() {
  ModuleOp module = getOperation();

  LLVM_DEBUG({
    dbgs() << LINE << "PreprocessDbsPass START\n" << LINE;
    module.dump();
  });

  /// Initialize ArtsCodegen for helper functions
  auto llvmDLAttr = module->getAttrOfType<StringAttr>("llvm.data_layout");
  if (!llvmDLAttr) {
    module.emitError("Module does not have a data layout");
    return signalPassFailure();
  }
  llvm::DataLayout llvmDL(llvmDLAttr.getValue().str());
  mlir::DataLayout mlirDL(module);
  AC = new ArtsCodegen(module, llvmDL, mlirDL, false);

  /// Process DbAlloc and DbDep operations
  preprocessDbAllocOps(module);
  preprocessDbDepOps(module);

  /// Remove old operations
  removeOps(module, AC->getBuilder(), opsToRemove);

  LLVM_DEBUG({
    dbgs() << LINE << "PreprocessDbsPass FINISHED\n" << LINE;
    module.dump();
  });

  /// Clean up ArtsCodegen
  delete AC;
  AC = nullptr;
}

void PreprocessDbsPass::preprocessDbAllocOps(ModuleOp module) {
  auto &builder = AC->getBuilder();

  /// Collect all DbAllocOps and create new ones with opaque pointers
  SmallVector<DbAllocOp, 8> dbAllocOps;
  DenseMap<DbAllocOp, DbAllocOp> dbsToRewire;

  module.walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DbAllocOp dbAllocOp) -> mlir::WalkResult {
        /// Skip already processed new db ops
        if (dbAllocOp->hasAttr("newDb"))
          return mlir::WalkResult::skip();

        AC->setInsertionPointAfter(dbAllocOp);

        /// Create new DbAllocOp with opaque pointer type
        auto oldType = dbAllocOp.getResult().getType().cast<MemRefType>();
        auto newType = MemRefType::get(oldType.getShape(), AC->VoidPtr);

        auto newDbAllocOp = builder.create<DbAllocOp>(
            dbAllocOp.getLoc(), newType, dbAllocOp.getMode(),
            dbAllocOp.getAddress(), dbAllocOp.getSizes(), nullptr);

        /// Copy attributes and mark as processed
        newDbAllocOp->setAttrs(dbAllocOp->getAttrs());
        newDbAllocOp->setAttr("newDb", builder.getUnitAttr());
        setDbAllocType(newDbAllocOp, getDbAllocType(dbAllocOp), builder);

        dbAllocOps.push_back(dbAllocOp);
        dbsToRewire[dbAllocOp] = newDbAllocOp;
        return mlir::WalkResult::advance();
      });

  /// Return if no datablocks to rewire
  if (dbsToRewire.empty())
    return;

  /// Rewire the DbAllocOps
  for (auto dbFrom : dbAllocOps) {
    auto dbTo = dbsToRewire[dbFrom];
    LLVM_DEBUG(DBGS() << "Rewiring DbAllocOp:\n  " << dbFrom << "\n  " << dbTo
                      << "\n");

    /// Process all users of the old DbAllocOp
    processDbOpUsers(dbFrom.getOperation(), dbTo.getOperation(), builder);
    opsToRemove.insert(dbFrom);
  }
}

void PreprocessDbsPass::preprocessDbDepOps(ModuleOp module) {
  auto &builder = AC->getBuilder();

  /// Collect all DbDepOps and create new ones with opaque pointers
  SmallVector<DbDepOp, 8> dbDepOps;
  DenseMap<DbDepOp, DbDepOp> dbsToRewire;

  module.walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DbDepOp dbDepOp) -> mlir::WalkResult {
        /// Skip already processed new db ops
        if (dbDepOp->hasAttr("newDb"))
          return mlir::WalkResult::skip();

        AC->setInsertionPointAfter(dbDepOp);

        /// Create new DbDepOp with opaque pointer type
        auto oldType = dbDepOp.getResult().getType().cast<MemRefType>();
        auto newType = MemRefType::get(oldType.getShape(), AC->VoidPtr);

        auto newDbDepOp = builder.create<DbDepOp>(
            dbDepOp.getLoc(), newType, dbDepOp.getModeAttr(),
            dbDepOp.getOperand(0), dbDepOp.getIndices(), dbDepOp.getOffsets(),
            dbDepOp.getSizes());

        /// Copy attributes and mark as processed
        newDbDepOp->setAttrs(dbDepOp->getAttrs());
        newDbDepOp->setAttr("newDb", builder.getUnitAttr());

        dbDepOps.push_back(dbDepOp);
        dbsToRewire[dbDepOp] = newDbDepOp;
        return mlir::WalkResult::advance();
      });

  /// Return if no datablocks to rewire
  if (dbsToRewire.empty())
    return;

  /// Rewire the DbDepOps
  for (auto dbFrom : dbDepOps) {
    auto dbTo = dbsToRewire[dbFrom];
    LLVM_DEBUG(DBGS() << "Rewiring DbDepOp:\n  " << dbFrom << "\n  " << dbTo
                      << "\n");

    /// Process all users of the old DbDepOp
    processDbOpUsers(dbFrom.getOperation(), dbTo.getOperation(), builder);

    /// Erase the old DbDepOp
    opsToRemove.insert(dbFrom);
  }
}

void PreprocessDbsPass::processDbOpUsers(Operation *dbFrom, Operation *dbTo,
                                         OpBuilder &builder) {
  auto computePtr = [&](Location loc) -> Value {
    auto llvmCast = AC->castToLLVMPtr(dbTo->getResult(0), loc);
    if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo)) {
      if (arts::isStackAlloc(dbAllocOp)) {
        return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmCast);
      }
    }
    return llvmCast;
  };

  /// Process all users of the old DbOp
  for (auto &use : llvm::make_early_inc_range(dbFrom->getUses())) {
    auto user = use.getOwner();
    AC->setInsertionPoint(user);

    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      auto loc = loadOp.getLoc();
      auto newPtr = computePtr(loc);
      auto newLoad = builder.create<LLVM::LoadOp>(
          loc, loadOp.getResult().getType(), newPtr);
      loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(loadOp);
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      auto loc = storeOp.getLoc();
      auto newPtr = computePtr(loc);
      builder.create<LLVM::StoreOp>(loc, storeOp.getValueToStore(), newPtr);
      opsToRemove.insert(storeOp);
    } else if (auto dbDepOp = dyn_cast<DbDepOp>(user)) {
      /// DbDep dependency - replace with new DbOp result
      use.set(dbTo->getResult(0));
    } else if (auto edtOp = dyn_cast<EdtOp>(user)) {
      /// EDT dependency - replace with new DbOp result
      use.set(dbTo->getResult(0));
    } else {
      LLVM_DEBUG(DBGS() << "Unknown use of DbOp: " << *user << "\n");
      user->emitError("Unsupported user of DbOp: ") << user->getName();
    }
  }
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createPreprocessDbsPass() {
  return std::make_unique<PreprocessDbsPass>();
}
} // namespace arts
} // namespace mlir