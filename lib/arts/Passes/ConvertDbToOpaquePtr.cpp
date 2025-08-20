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
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(preprocess_dbs);

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//

// Determine allocation type based on basePtr characteristics
static DbAllocType inferAllocType(Value basePtr) {
  if (!basePtr) {
    return DbAllocType::heap; // Default to heap for null pointers
  }

  // Check if basePtr comes from a stack-like allocation
  if (auto definingOp = basePtr.getDefiningOp()) {
    if (isa<memref::AllocaOp>(definingOp)) {
      return DbAllocType::stack;
    }
    if (isa<memref::AllocOp>(definingOp)) {
      return DbAllocType::heap;
    }
    // Check for global memrefs
    if (isa<memref::GetGlobalOp>(definingOp)) {
      return DbAllocType::global;
    }
  }

  return DbAllocType::unknown;
}

DbAllocOp createDbAlloc(OpBuilder &builder, Location loc, Value basePtr,
                        ArtsMode mode) {
  /// Create the DbAllocOp with proper signature
  auto opaqueType = MemRefType::get({-1}, builder.getI8Type()); // Opaque buffer

  /// Infer allocation type from basePtr
  auto allocType = inferAllocType(basePtr);
  auto allocTypeAttr = DbAllocTypeAttr::get(builder.getContext(), allocType);

  /// Create DbAllocOp
  SmallVector<Value> emptySize;
  auto dbAllocOp = builder.create<DbAllocOp>(
      loc, mode, emptySize, basePtr);

  ARTS_INFO("Created DbAllocOp with allocation type: "
            << stringifyDbAllocType(allocType));
  return dbAllocOp;
}

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertDbToOpaquePtrPass
    : public arts::ConvertDbToOpaquePtrBase<ConvertDbToOpaquePtrPass> {
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

  ARTS_DEBUG_HEADER(ConvertDbToOpaquePtrPass);
  ARTS_DEBUG_REGION(module.dump(););

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

  ARTS_DEBUG_FOOTER(ConvertDbToOpaquePtrPass);
  ARTS_DEBUG_REGION(module.dump(););
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
        auto oldType = dbAllocOp.getResult().getType().cast<MemRefType>();
        auto newType = MemRefType::get(oldType.getShape(), builder.getI8Type());

        auto newDbAllocOp = builder.create<DbAllocOp>(
            dbAllocOp.getLoc(), dbAllocOp.getMode(), ValueRange{}, dbAllocOp.getAddress());

        /// Copy attributes and mark as processed
        newDbAllocOp->setAttrs(dbAllocOp->getAttrs());
        newDbAllocOp->setAttr("newDb", builder.getUnitAttr());
        // Copy allocType from original DbAllocOp
        if (auto allocTypeAttr = dbAllocOp.getAllocTypeAttr()) {
          newDbAllocOp.setAllocTypeAttr(allocTypeAttr);
        }

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
    ARTS_INFO("Rewiring DbAllocOp:\n  " << dbFrom << "\n  " << dbTo);

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

  module.walk<mlir::WalkOrder::PreOrder>(
      [&](Operation *op) -> mlir::WalkResult {
        if (op->hasAttr("newDb"))
          return mlir::WalkResult::skip();

        if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
          AC->setInsertionPointAfter(acquireOp);

          auto oldType = acquireOp.getResult().getType().cast<MemRefType>();
          auto newType =
              MemRefType::get(oldType.getShape(), builder.getI8Type());

          auto newAcquireOp = builder.create<DbAcquireOp>(
              acquireOp.getLoc(), newType, acquireOp.getMode(),
              acquireOp.getSource(), acquireOp.getIndices(),
              acquireOp.getOffsets(), acquireOp.getSizes());

          newAcquireOp->setAttrs(acquireOp->getAttrs());
          newAcquireOp->setAttr("newDb", builder.getUnitAttr());

          dbAccessOps.push_back(acquireOp);
          accessesToRewire[acquireOp] = newAcquireOp;
        } else if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
          AC->setInsertionPointAfter(releaseOp);

          auto newReleaseOp = builder.create<DbReleaseOp>(
              releaseOp.getLoc(), releaseOp.getSources());

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
    ARTS_DEBUG_TYPE("Rewiring DB Access:\n  " << *accessFrom << "\n  "
                                              << *accessTo << "\n");

    /// Process all users of the old access op
    processDbOpUsers(accessFrom, accessTo, builder);

    opsToRemove.insert(accessFrom);
  }
}

void ConvertDbToOpaquePtrPass::processDbOpUsers(Operation *dbFrom,
                                                Operation *dbTo,
                                                OpBuilder &builder) {
  auto computePtr = [&](Location loc) -> Value {
    auto llvmCast = AC->castToLLVMPtr(dbTo->getResult(0), loc);
    if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo)) {
      if (auto allocType = dbAllocOp.getAllocType();
          allocType && *allocType == DbAllocType::stack) {
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
      ARTS_ERROR("Unknown use of DB op: " << *user);
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