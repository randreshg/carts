///==========================================================================
/// File: PreprocessDbs.cpp
///
/// This pass preprocesses ARTS DataBlocks to convert them from typed memrefs
/// to opaque pointer types suitable for the ARTS runtime. The preprocessing
/// logic differs based on the original allocation type (stack vs dynamic).
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
#define line "-----------------------------------------\n"
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
  auto dbAllocOp =
      createDbAllocOp(builder, loc, types::toString(dep), basePtr);

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
  void preprocessDbsInFunction(func::FuncOp func);
  void processDbAlloc(DbAllocOp dbAllocOp);
  bool processDbDepUser(DbDepOp dbDepOp, DbAllocOp newDbAllocOp,
                           OpBuilder &builder, Location loc);
  Value computePtr(DbAllocOp dbAllocOp, Value basePtr, OpBuilder &builder,
                   Location loc);

private:
  ArtsCodegen *AC = nullptr;
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void PreprocessDbsPass::runOnOperation() {
  ModuleOp module = getOperation();

  LLVM_DEBUG({
    dbgs() << line << "PreprocessDbsPass START\n" << line;
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

  /// Process each function
  for (auto func : module.getOps<func::FuncOp>())
    preprocessDbsInFunction(func);

  /// Remove old operations
  removeOps(module, AC->getBuilder(), opsToRemove);

  LLVM_DEBUG({
    dbgs() << line << "PreprocessDbsPass FINISHED\n" << line;
    module.dump();
  });

  delete AC;
}

void PreprocessDbsPass::preprocessDbsInFunction(func::FuncOp func) {
  LLVM_DEBUG(DBGS() << "Preprocessing DBs in function: " << func.getName()
                    << "\n");

  SmallVector<DbAllocOp, 4> dbAllocs;
  func.walk([&](DbAllocOp op) { dbAllocs.push_back(op); });

  for (auto dbAllocOp : dbAllocs)
    processDbAlloc(dbAllocOp);
}

void PreprocessDbsPass::processDbAlloc(DbAllocOp dbAllocOp) {
  auto &builder = AC->getBuilder();
  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(dbAllocOp);

  auto loc = dbAllocOp.getLoc();
  auto oldResult = dbAllocOp.getResult();

  LLVM_DEBUG(DBGS() << "Processing DbAllocOp\n");
  /// Create new DbAllocOp with opaque pointer type
  auto newType = MemRefType::get({ShapedType::kDynamic}, AC->VoidPtr);
  auto newDbAllocOp = builder.create<DbAllocOp>(
      loc, newType, dbAllocOp.getMode(), dbAllocOp.getAddress(),
      dbAllocOp.getSizes(), nullptr);
  setDbAllocType(newDbAllocOp, getDbAllocType(dbAllocOp), builder);

  /// Process all users of the old DbAllocOp
  SmallVector<std::pair<OpOperand *, Operation *>> usesToProcess;
  for (auto &use : oldResult.getUses())
    usesToProcess.push_back({&use, use.getOwner()});

  for (auto [useOperand, owner] : usesToProcess) {
    builder.setInsertionPoint(owner);

    if (auto dbDepOp = dyn_cast<DbDepOp>(owner)) {
      if (!processDbDepUser(dbDepOp, newDbAllocOp, builder, loc)) {
        dbDepOp.emitError("Failed to process DbDepOp user");
        continue;
      }
    } else if (auto edtOp = dyn_cast<EdtOp>(owner)) {
      useOperand->set(newDbAllocOp.getResult());
    } else {
      owner->emitError("Unsupported user of DbAllocOp: ") << owner->getName();
      continue;
    }
  }

  opsToRemove.insert(dbAllocOp);
}

bool PreprocessDbsPass::processDbDepUser(DbDepOp dbDepOp,
                                            DbAllocOp newDbAllocOp,
                                            OpBuilder &builder, Location loc) {
  /// Create new DbDepOp that references the preprocessed DbAllocOp
  auto newDbDepOp = builder.create<DbDepOp>(
      dbDepOp.getLoc(), dbDepOp.getResult().getType(),
      dbDepOp.getModeAttr(), newDbAllocOp.getResult(),
      dbDepOp.getIndices(), dbDepOp.getOffsets(), dbDepOp.getSizes());

  auto oldDbDepResult = dbDepOp.getResult();
  auto newDbDepResult = newDbDepOp.getResult();

  /// Process all users of the old DbDepOp
  SmallVector<std::pair<OpOperand *, Operation *>> depUsesToProcess;
  for (auto &use : oldDbDepResult.getUses())
    depUsesToProcess.push_back({&use, use.getOwner()});

  for (auto [useOperand, userOp] : depUsesToProcess) {
    builder.setInsertionPoint(userOp);

    if (auto loadOp = dyn_cast<memref::LoadOp>(userOp)) {
      auto ptr = computePtr(newDbAllocOp, newDbDepResult, builder, loc);
      if (!ptr)
        continue;

      auto newLoad = builder.create<LLVM::LoadOp>(
          loadOp.getLoc(), loadOp.getResult().getType(), ptr);
      loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(loadOp);

    } else if (auto storeOp = dyn_cast<memref::StoreOp>(userOp)) {
      auto ptr = computePtr(newDbAllocOp, newDbDepResult, builder, loc);
      if (!ptr)
        continue;

      builder.create<LLVM::StoreOp>(storeOp.getLoc(), storeOp.getValueToStore(),
                                    ptr);
      opsToRemove.insert(storeOp);

    } else if (auto edtOp = dyn_cast<EdtOp>(userOp)) {
      /// EDT dependency - replace with new DbDepOp result
      useOperand->set(newDbDepResult);

    } else {
      userOp->emitError("Unsupported user of DbDepOp: ")
          << userOp->getName();
      return false;
    }
  }

  opsToRemove.insert(dbDepOp.getOperation());
  return true;
}

Value PreprocessDbsPass::computePtr(DbAllocOp dbAllocOp, Value basePtr,
                                    OpBuilder &builder, Location loc) {
  /// Stack/parameter allocations need pointer loading
  if (arts::isStackAlloc(dbAllocOp)) {
    LLVM_DEBUG(DBGS() << "Computing pointer for stack/parameter allocation\n");
    auto llvmCast = AC->castToLLVMPtr(basePtr, loc);
    return builder.create<LLVM::LoadOp>(loc, AC->llvmPtr, llvmCast);
  }
  /// Dynamic allocations already have pointer indirection
  else if (arts::isDynamicAlloc(dbAllocOp)) {
    LLVM_DEBUG(DBGS() << "Computing pointer for heap/dynamic allocation\n");
    return AC->castToLLVMPtr(basePtr, loc);
  }
  /// Global allocations need address-of operation
  else if (arts::isGlobalAlloc(dbAllocOp)) {
    LLVM_DEBUG(DBGS() << "Computing pointer for global allocation\n");
    return AC->castToLLVMPtr(basePtr, loc);
  } else {
    llvm_unreachable("Unknown allocation type");
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