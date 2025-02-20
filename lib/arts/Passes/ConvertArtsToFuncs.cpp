//===- ConvertArtsToFuncs.cpp - Convert ARTS Dialect to Func Dialect ------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

// #include "mlir/Conversion/ARTSToFuncs/ARTSToFuncs.h"

// #include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

/// Dialects
#include "mlir/Conversion/IndexToLLVM/IndexToLLVM.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
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
/// Conversion
#include "mlir/Conversion/AffineToStandard/AffineToStandard.h"
#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include <algorithm>
#include <optional>

/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "convert-arts-to-funcs"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
namespace {
struct ConvertArtsToFuncsPass
    : public arts::ConvertArtsToFuncsBase<ConvertArtsToFuncsPass> {
  void runOnOperation() override;

  void iterateDbs(ModuleOp module);
  void iterateOps(Operation *op);
  void handleParallel(EdtOp &op);
  void handleSingle(EdtOp &op);
  void handleEdt(EdtOp &op);
  void handleEvent(EventOp &op);
  void handleDatablock(DataBlockOp &op);

private:
  ArtsCodegen *AC = nullptr;
  SetVector<Operation *> opsToRemove;
};
} // end namespace

void ConvertArtsToFuncsPass::handleParallel(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt parallel\n" << line);
  auto *ctx = op.getContext();
  Location loc = UnknownLoc::get(ctx);
  mlir::Region &region = op.getRegion();

  /// Set insertion point to the current op for epoch creation.
  EdtCodegen *newEdt = nullptr;
  {
    OpBuilder::InsertionGuard guard(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Create a done-edt for the parallel epoch.
    EdtCodegen parDoneEdt(*AC);
    parDoneEdt.setDepC(AC->createIntConstant(1, AC->Int32, loc));
    parDoneEdt.build(loc);

    /// Create the parallel epoch.
    auto parDone_slot = AC->createIntConstant(0, AC->Int32, loc);
    auto parEpoch_guid =
        AC->createEpoch(parDoneEdt.getGuid(), parDone_slot, loc);

    /// Create the parallel EDT with the single op's region.
    auto par_deps = op.getDeps();
    newEdt = AC->createEdt(&par_deps, &region, &parEpoch_guid, nullptr, true);
  }

  /// Erase the old parallel op.
  opsToRemove.insert(op);
  LLVM_DEBUG(DBGS() << "Parallel op lowered\n\n");

  /// Visit new function
  assert(newEdt && "New EDT not created");
  iterateOps(newEdt->getFunc());
}

void ConvertArtsToFuncsPass::handleSingle(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt single\n");
  llvm_unreachable("Single EDTs not supported yet");
}

void ConvertArtsToFuncsPass::handleEdt(EdtOp &op) {
  auto *ctx = op.getContext();
  Location loc = UnknownLoc::get(ctx);
  LLVM_DEBUG(DBGS() << "Lowering arts.edt\n");
  EdtCodegen *newEdt = nullptr;
  {
    OpBuilder::InsertionGuard guard(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Create the parallel EDT with the single op's region.
    auto epoch = AC->getCurrentEpochGuid(loc);
    auto deps = op.getDeps();
    auto &region = op.getRegion();
    newEdt = AC->createEdt(&deps, &region, &epoch, nullptr, true);
  }

  /// Erase the old edt
  opsToRemove.insert(op);

  /// Visit new function
  assert(newEdt && "New EDT not created");
  iterateOps(newEdt->getFunc());
}

void ConvertArtsToFuncsPass::handleDatablock(DataBlockOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.datablock\n");
  AC->setInsertionPoint(op);
  auto &builder = AC->getBuilder();

  /// Create a new DataBlockOp with an opaque pointer type
  auto sizes = op.getSizes();
  bool isSingleElement = sizes.size() == 1 && [&] {
    if (auto constOp = sizes[0].getDefiningOp<arith::ConstantIndexOp>())
      return constOp.value() == 1;
    return false;
  }();
  auto pointerType = isSingleElement
                         ? AC->VoidPtr
                         : MemRefType::get({ShapedType::kDynamic}, AC->VoidPtr);
  auto newDbOp = builder.create<arts::DataBlockOp>(
      op->getLoc(), pointerType, op.getMode(), op.getPtr(), op.getElementType(),
      op.getElementTypeSize(), op.getIndices(), sizes);
  newDbOp->setAttrs(op->getAttrs());

  /// Helper lambda to check if the indices represent a single constant zero.
  auto isSingleZeroOffset = [&](ValueRange indices) -> bool {
    if (indices.size() != 1)
      return false;
    if (auto constOp = indices.front().getDefiningOp<arith::ConstantIndexOp>())
      return constOp.value() == 0;
    return false;
  };

  /// Helper lambda to compute the pointer based on the indices.
  auto MT = newDbOp.getType().cast<MemRefType>();
  auto computePtr = [&](ValueRange indices, Type elementType,
                        Location loc) -> Value {
    auto newPtr = AC->castToLLVMPtr(newDbOp, MT, loc);
    if (isSingleZeroOffset(indices))
      return newPtr;

    /// Cast indices to int32
    SmallVector<Value> newIndices;
    newIndices.reserve(indices.size());
    for (auto index : indices)
      newIndices.push_back(AC->castToInt(AC->Int64, index, loc));
    return builder
        .create<LLVM::GEPOp>(loc, AC->llvmPtr, elementType, newPtr, newIndices)
        .getResult();
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
    /// EdtOp - fix dependencies.
    else if (auto edtOp = dyn_cast<arts::EdtOp>(user)) {
      for (auto dep : edtOp.getDependencies()) {
        if (dep == op)
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
  opsToRemove.insert(op);
  opsToRemove.insert(newDbOp.getPtr().getDefiningOp());
  opsToRemove.insert(newDbOp);
}

void ConvertArtsToFuncsPass::handleEvent(EventOp &op) {
  LLVM_DEBUG(DBGS() << "Skipping arts.event: " << op << "\n");
}

void ConvertArtsToFuncsPass::iterateOps(Operation *operation) {
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
          else if (edtOp.isSingle())
            handleSingle(edtOp);
          else
            handleEdt(edtOp);
          return mlir::WalkResult::advance();
        } else if (auto eventOp = dyn_cast<arts::EventOp>(op)) {
          handleEvent(eventOp);
          return mlir::WalkResult::advance();
        }
        return mlir::WalkResult::advance();
      });
}

// void ConvertArtsToFuncsPass::iterateDbs(ModuleOp module) {
//   module->walk<mlir::WalkOrder::PreOrder>(
//       [&](Operation *op) -> mlir::WalkResult {
//         /// Skip operations marked for removal.
//         if (opsToRemove.count(op))
//           return mlir::WalkResult::skip();

//         if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
//           handleDatablock(dbOp);
//           return mlir::WalkResult::advance();
//         }
//         else if (auto edtOp = dyn_cast<arts::EdtOp>(op)) {
//           if (edtOp.isParallel())
//             handleParallel(edtOp);
//           else if (edtOp.isSingle())
//             handleSingle(edtOp);
//           else
//             handleEdt(edtOp);
//           return mlir::WalkResult::advance();
//         } else if (auto eventOp = dyn_cast<arts::EventOp>(op)) {
//           handleEvent(eventOp);
//           return mlir::WalkResult::advance();
//         }
//         return mlir::WalkResult::advance();
//       });
// }

void ConvertArtsToFuncsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << "=== ConvertArtsToFuncsPass START ===\n");

  ModuleOp module = getOperation();

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

  /// Handle arts operations
  for (auto func : module.getOps<func::FuncOp>())
    iterateOps(func);
  removeOps(module, AC->getBuilder(), opsToRemove);

  /// Create a ConversionTarget.
  // auto *ctx = &getContext();
  // ConversionTarget target(*ctx);
  // target.addIllegalDialect<memref::MemRefDialect>();
  // target.addLegalDialect<LLVM::LLVMDialect, arith::ArithDialect,
  //                        func::FuncDialect, affine::AffineDialect,
  //                        scf::SCFDialect>();

  // // Add the memref-to-LLVM conversion pass (or add its patterns manually)
  // LowerToLLVMOptions options(module.getContext());
  // options.useOpaquePointers = false;

  // // Populate type conversions.
  // LLVMTypeConverter converter(module.getContext(), options);
  // RewritePatternSet patterns(ctx);
  // populateFinalizeMemRefToLLVMConversionPatterns(converter, patterns);
  // arith::populateArithToLLVMConversionPatterns(converter, patterns);
  // index::populateIndexToLLVMConversionPatterns(converter, patterns);
  // populateAffineToStdConversionPatterns(patterns);

  // // Then run the conversion:
  // if (failed(applyPartialConversion(module, target, std::move(patterns))))
  // {
  //   module.emitError("Conversion from memref to LLVM failed");
  //   return signalPassFailure();
  // }

  // /// Create a ConversionTarget.
  // ConversionTarget target(*ctx);
  // /// Mark arts.edt as illegal so that our conversion pattern is applied.
  // target.addIllegalOp<arts::EdtOp>();
  // target.addLegalOp<arts::AllocaOp, arts::DataBlockOp, arts::YieldOp,
  //                   arts::BarrierOp>();
  // /// Mark other ops as legal.
  // target.addLegalDialect<arith::ArithDialect, cf::ControlFlowDialect,
  //                        func::FuncDialect, memref::MemRefDialect,
  //                        affine::AffineDialect,
  //                        polygeist::PolygeistDialect, scf::SCFDialect,
  //                        LLVM::LLVMDialect>();
  // target.addLegalOp<ModuleOp>();

  LLVM_DEBUG(dbgs() << line << "ConvertArtsToFuncsPass FINISHED \n" << line);
  module.dump();
  delete AC;
}
//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createConvertArtsToFuncsPass() {
  return std::make_unique<ConvertArtsToFuncsPass>();
}
} // namespace arts
} // namespace mlir
