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

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(convert_db_to_opaque_ptr);

using namespace mlir;
using namespace arts;

// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertDbToOpaquePtrPass
    : public arts::ConvertDbToOpaquePtrBase<ConvertDbToOpaquePtrPass> {
  ~ConvertDbToOpaquePtrPass() { delete AC; }

  void runOnOperation() override;

private:
  /// Determine allocation type based on basePtr characteristics
  DbAllocType inferAllocType(Value basePtr);

  /// Create a DbAllocOp using the pass' ArtsCodegen (uses current node route)
  DbAllocOp createDbAlloc(Location loc, Value basePtr, ArtsMode mode);

  void preprocessDbAllocOps();
  void preprocessDbAccessOps();
  void processDbOpUsers(Operation *dbFrom, Operation *dbTo);

private:
  SetVector<Operation *> opsToRemove;
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
};
} // end namespace

DbAllocType ConvertDbToOpaquePtrPass::inferAllocType(Value basePtr) {
  /// Default to heap for null pointers
  if (!basePtr)
    return DbAllocType::heap;

  if (Operation *definingOp = basePtr.getDefiningOp()) {
    if (isa<memref::AllocaOp>(definingOp))
      return DbAllocType::stack;
    if (isa<memref::AllocOp>(definingOp))
      return DbAllocType::heap;
    if (isa<memref::GetGlobalOp>(definingOp))
      return DbAllocType::global;
  }
  return DbAllocType::unknown;
}

DbAllocOp ConvertDbToOpaquePtrPass::createDbAlloc(Location loc, Value basePtr,
                                                  ArtsMode mode) {
  /// Route must always be provided; default to 0
  Value route = AC->createIntConstant(0, AC->Int32, loc);
  /// Infer allocation type or default to heap
  DbAllocType allocType = inferAllocType(basePtr);
  /// Default DB mode to pin
  DbMode dbMode = DbMode::pin;
  auto dbAllocOp = AC->create<DbAllocOp>(loc, mode, ValueRange{}, route,
                                         allocType, dbMode, basePtr);

  ARTS_INFO("Created DbAllocOp with allocation type: "
            << stringifyDbAllocType(inferAllocType(basePtr)));
  return dbAllocOp;
}

void ConvertDbToOpaquePtrPass::runOnOperation() {
  module = getOperation();

  ARTS_DEBUG_HEADER(ConvertDbToOpaquePtr);
  ARTS_DEBUG_REGION(module->dump(););

  /// Initialize ArtsCodegen for helper functions
  AC = new ArtsCodegen(module, false);

  /// Process DbAlloc and access operations (acquire/release)
  preprocessDbAllocOps();
  preprocessDbAccessOps();

  /// Remove old operations
  removeOps(module, AC->getBuilder(), opsToRemove);

  ARTS_DEBUG_FOOTER(ConvertDbToOpaquePtrPass);
  ARTS_DEBUG_REGION(module.dump(););
}

void ConvertDbToOpaquePtrPass::preprocessDbAllocOps() {
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
        // auto oldType = dbAllocOp.getResult().getType().cast<MemRefType>();
        // auto newType = MemRefType::get(oldType.getShape(),
        // builder.getI8Type());

        auto route = AC->createIntConstant(0, AC->Int32, dbAllocOp.getLoc());

        /// Get allocType from original or default to heap
        DbAllocType allocType = DbAllocType::heap;
        if (auto allocTypeAttr =
                dbAllocOp->getAttrOfType<DbAllocTypeAttr>("allocType"))
          allocType = allocTypeAttr.getValue();

        /// Preserve the original DB mode if it exists, otherwise default to pin
        DbMode dbMode = DbMode::pin;
        if (auto dbModeAttr = dbAllocOp->getAttrOfType<DbModeAttr>("dbMode"))
          dbMode = dbModeAttr.getValue();

        auto newDbAllocOp = AC->create<DbAllocOp>(
            dbAllocOp.getLoc(), dbAllocOp.getMode(), ValueRange{}, route,
            allocType, dbMode, dbAllocOp.getAddress());

        /// Copy other attributes and mark as processed
        newDbAllocOp->setAttrs(dbAllocOp->getAttrs());
        newDbAllocOp->setAttr("newDb", AC->getBuilder().getUnitAttr());

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
    processDbOpUsers(dbFrom.getOperation(), dbTo.getOperation());
    opsToRemove.insert(dbFrom);
  }
}

void ConvertDbToOpaquePtrPass::preprocessDbAccessOps() {
  /// Collect all DbAcquireOp and DbReleaseOp, create new with opaque pointers
  SmallVector<Operation *, 8> dbAccessOps;
  DenseMap<Operation *, Operation *> accessesToRewire;

  module.walk<mlir::WalkOrder::PreOrder>(
      [&](Operation *op) -> mlir::WalkResult {
        if (op->hasAttr("newDb"))
          return mlir::WalkResult::skip();

        if (auto acquireOp = dyn_cast<DbAcquireOp>(op)) {
          AC->setInsertionPointAfter(acquireOp);

          auto oldType = acquireOp.getPtr().getType().cast<MemRefType>();
          auto newType = MemRefType::get(oldType.getShape(), AC->Int8);

          auto newAcquireOp = AC->create<DbAcquireOp>(
              acquireOp.getLoc(), newType, acquireOp.getMode(),
              acquireOp.getSource(), acquireOp.getIndices(),
              acquireOp.getOffsets(), acquireOp.getSizes());

          newAcquireOp->setAttrs(acquireOp->getAttrs());
          newAcquireOp->setAttr("newDb", AC->getBuilder().getUnitAttr());

          dbAccessOps.push_back(acquireOp);
          accessesToRewire[acquireOp] = newAcquireOp;
        } else if (auto releaseOp = dyn_cast<DbReleaseOp>(op)) {
          AC->setInsertionPointAfter(releaseOp);

          auto newReleaseOp = AC->create<DbReleaseOp>(releaseOp.getLoc(),
                                                      releaseOp.getSources());

          newReleaseOp->setAttrs(releaseOp->getAttrs());
          newReleaseOp->setAttr("newDb", AC->getBuilder().getUnitAttr());

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
    processDbOpUsers(accessFrom, accessTo);

    opsToRemove.insert(accessFrom);
  }
}

void ConvertDbToOpaquePtrPass::processDbOpUsers(Operation *dbFrom,
                                                Operation *dbTo) {
  auto computePtr = [&](Location loc) -> Value {
    Value ptrValue;
    if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo)) {
      ptrValue = dbAllocOp.getPtr();
    } else if (auto dbAcquireOp = dyn_cast<DbAcquireOp>(dbTo)) {
      ptrValue = dbAcquireOp.getPtr();
    } else {
      ptrValue = dbTo->getResult(1); // fallback for other types
    }
    auto llvmCast = AC->castToLLVMPtr(ptrValue, loc);
    if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo)) {
      if (auto attr = dbAllocOp->getAttrOfType<DbAllocTypeAttr>("allocType")) {
        if (attr.getValue() == DbAllocType::stack)
          return AC->create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmCast);
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
      auto newLoad =
          AC->create<LLVM::LoadOp>(loc, loadOp.getResult().getType(), newPtr);
      loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(loadOp);
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      auto loc = storeOp.getLoc();
      auto newPtr = computePtr(loc);
      AC->create<LLVM::StoreOp>(loc, storeOp.getValueToStore(), newPtr);
      opsToRemove.insert(storeOp);
    } else if (auto acquireOp = dyn_cast<DbAcquireOp>(user)) {
      if (auto dbAcquireOp = dyn_cast<DbAcquireOp>(dbTo))
        use.set(dbAcquireOp.getPtr());
    } else if (auto releaseOp = dyn_cast<DbReleaseOp>(user)) {
      if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo))
        use.set(dbAllocOp.getPtr());
      else if (auto dbAcquireOp = dyn_cast<DbAcquireOp>(dbTo))
        use.set(dbAcquireOp.getPtr());
    } else if (auto edtOp = dyn_cast<EdtOp>(user)) {
      if (auto dbAllocOp = dyn_cast<DbAllocOp>(dbTo))
        use.set(dbAllocOp.getPtr());
      else if (auto dbAcquireOp = dyn_cast<DbAcquireOp>(dbTo))
        use.set(dbAcquireOp.getPtr());
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