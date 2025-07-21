//===----------------------------------------------------------------------===//
// Db/ConvertDbToOpaquePtr.cpp - Pass for preprocessing DBs to opaque pointers
//===----------------------------------------------------------------------===//

// ConvertDbToOpaquePtrPass Explanation
//
// The ConvertDbToOpaquePtrPass is a crucial preprocessing step in the ARTS MLIR
// pipeline. It transforms data block (DB) operations from typed memrefs (e.g.,
// memref<f32>) to opaque pointers (void*) suitable for the ARTS runtime system.
// ARTS, being an asynchronous many-task runtime, manages data blocks as untyped
// buffers to enable flexible, runtime-managed memory and dependencies. This
// pass ensures compatibility by:
// - Converting DB allocations (db_alloc) to create opaque pointers.
// - Adjusting acquires (db_acquire) and releases (db_release) to operate on
// these pointers.
// - Lowering loads/stores on DBs to LLVM IR calls, handling casts for typed
// accesses.
//
// This pass is important because it bridges high-level MLIR representations
// with ARTS' low-level API (e.g., artsDbCreate, artsDbAcquire, artsDbRelease
// from arts.h), preventing type mismatches during codegen. It runs before
// ConvertToLLVMPass to ensure correct lowering, avoiding runtime errors and
// enabling optimizations like buffer reuse. Without it, typed DBs would not map
// correctly to ARTS' void*-based DBs, breaking execution.

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
                        StringRef mode) {
  /// Create the DbAllocOp with opaque type
  auto opaqueType = MemRefType::get({-1}, builder.getI8Type()); // Opaque buffer
  auto dbAllocOp = builder.create<DbAllocOp>(loc, mode, basePtr);

  /// Set allocation type (stack/dynamic) based on basePtr
  auto allocType = getAllocType(basePtr);
  setDbAllocType(dbAllocOp, allocType, builder);

  LLVM_DEBUG(DBGS() << "Created DbAllocOp with allocation type: "
                    << toString(allocType) << "\n");
  return dbAllocOp;
}

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertDbToOpaquePtrPass : public arts::ConvertDbToOpaquePtrBase<ConvertDbToOpaquePtrPass> {
  void runOnOperation() override;

private:
  void preprocessDbAllocOps(ModuleOp module);
  void preprocessDbAccessOps(ModuleOp module);
  void processDbOpUsers(Operation *dbFrom, Operation *dbTo, OpBuilder &builder);

private:
  ArtsCodegen *AC = nullptr;
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void ConvertDbToOpaquePtrPass::runOnOperation() {
  ModuleOp module = getOperation();

  LLVM_DEBUG({
    dbgs() << LINE << "ConvertDbToOpaquePtrPass START\n" << LINE;
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

  /// Process DbAlloc and access operations (acquire/release)
  preprocessDbAllocOps(module);
  preprocessDbAccessOps(module);

  /// Remove old operations
  removeOps(module, AC->getBuilder(), opsToRemove);

  LLVM_DEBUG({
    dbgs() << LINE << "ConvertDbToOpaquePtrPass FINISHED\n" << LINE;
    module.dump();
  });

  /// Clean up ArtsCodegen
  delete AC;
  AC = nullptr;
}

void ConvertDbToOpaquePtrPass::preprocessDbAllocOps(ModuleOp module) {
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
        auto oldType = dbAllocOp.getResult(0).getType().cast<MemRefType>();
        auto newType = MemRefType::get(oldType.getShape(), builder.getI8Type());

        auto newDbAllocOp = builder.create<DbAllocOp>(
            dbAllocOp.getLoc(), dbAllocOp.getMode(), dbAllocOp.getAddress());

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

void ConvertDbToOpaquePtrPass::preprocessDbAccessOps(ModuleOp module) {
  auto &builder = AC->getBuilder();

  /// Collect all DbAcquireOp and DbReleaseOp, create new with opaque pointers
  SmallVector<Operation *, 8> dbAccessOps;
  DenseMap<Operation *, Operation *> accessesToRewire;

  module.walk<mlir::WalkOrder::PreOrder>([&](Operation *op)
                                             -> mlir::WalkResult {
    if (op->hasAttr("newDb"))
      return mlir::WalkResult::skip();

    if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
      AC->setInsertionPointAfter(acquireOp);

      auto oldType = acquireOp.getResult().getType().cast<MemRefType>();
      auto newType = MemRefType::get(oldType.getShape(), builder.getI8Type());

      auto newAcquireOp = builder.create<DbAcquireOp>(
          acquireOp.getLoc(), newType, acquireOp.getSource(),
          acquireOp.getIndices());

      newAcquireOp->setAttrs(acquireOp->getAttrs());
      newAcquireOp->setAttr("newDb", builder.getUnitAttr());

      dbAccessOps.push_back(acquireOp);
      accessesToRewire[acquireOp] = newAcquireOp;
    } else if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
      AC->setInsertionPointAfter(releaseOp);

      auto newReleaseOp = builder.create<DbReleaseOp>(
          releaseOp.getLoc(), releaseOp.getSource(), releaseOp.getIndices());

      newReleaseOp->setAttrs(releaseOp->getAttrs());
      newReleaseOp->setAttr("newDb", builder.getUnitAttr());

      dbAccessOps.push_back(releaseOp);
      accessesToRewire[releaseOp] = newReleaseOp;
    }
    return mlir::WalkResult::advance();
  });

  /// Return if no accesses to rewire
  if (accessesToRewire.empty())
    return;

  /// Rewire the accesses
  for (auto accessFrom : dbAccessOps) {
    auto accessTo = accessesToRewire[accessFrom];
    LLVM_DEBUG(DBGS() << "Rewiring DB Access:\n  " << *accessFrom << "\n  "
                      << *accessTo << "\n");

    /// Process all users of the old access op
    processDbOpUsers(accessFrom, accessTo, builder);

    opsToRemove.insert(accessFrom);
  }
}

void ConvertDbToOpaquePtrPass::processDbOpUsers(Operation *dbFrom, Operation *dbTo,
                                         OpBuilder &builder) {
  auto computePtr = [&](Location loc) -> Value {
    auto llvmCast = AC->castToLLVMPtr(dbTo->getResult(0), loc);
    if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo)) {
      if (isStackAlloc(dbAllocOp)) {
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
    } else if (auto acquireOp = dyn_cast<DbAcquireOp>(user)) {
      use.set(dbTo->getResult(0));
    } else if (auto releaseOp = dyn_cast<DbReleaseOp>(user)) {
      use.set(dbTo->getResult(0));
    } else if (auto edtOp = dyn_cast<EdtOp>(user)) {
      use.set(dbTo->getResult(0));
    } else {
      LLVM_DEBUG(DBGS() << "Unknown use of DB op: " << *user << "\n");
      user->emitError("Unsupported user of DB op");
    }
  }
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertDbToOpaquePtrPass() {
  return std::make_unique<ConvertDbToOpaquePtrPass>();
}
} // namespace arts
} // namespace mlir