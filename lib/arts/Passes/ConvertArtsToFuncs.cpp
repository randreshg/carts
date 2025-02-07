//===- ConvertArtsToFuncs.cpp - Convert ARTS Dialect to Func Dialect ------===//
//
// This file implements a pass to convert ARTS dialect operations into
// `func` dialect operations.
//
//===----------------------------------------------------------------------===//

// #include "mlir/Conversion/ARTSToFuncs/ARTSToFuncs.h"

// #include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

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

#define DEBUG_TYPE "convert-arts-to-funcs"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

namespace {

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//
struct ConvertArtsToFuncsPass
    : public arts::ConvertArtsToFuncsBase<ConvertArtsToFuncsPass> {
  void runOnOperation() override;

  /// Arts operations
  void iterateOps(Operation *op);
  void handleParallel(EdtOp &op);
  void handleSingle(EdtOp &op);
  void handleEdt(EdtOp &op);
  void handleEvent(EventOp &op);
  void removeOps();

  /// Datablock operations
  void iterateDbs(Operation *op);
  void handleDatablock(DataBlockOp &op);

  ArtsCodegen *AC = nullptr;
  SetVector<Operation *> opsToErase;
};
} // end namespace

void ConvertArtsToFuncsPass::handleParallel(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt parallel\n\n");
  auto *ctx = op.getContext();
  Location loc = UnknownLoc::get(ctx);

  /// Analyze the parallel region to locate the unique single-edt op.
  arts::EdtOp singleOp = nullptr;
  unsigned numOps = 0;
  mlir::Region &region = op.getRegion();

  /// Iterate directly over the immediate operations in the region.
  for (auto &block : region) {
    for (auto &opInst : block) {
      numOps++;
      if (auto edt = dyn_cast<arts::EdtOp>(&opInst)) {
        if (singleOp && edt.isSingle())
          llvm_unreachable("Multiple single ops in parallel op not supported");
        singleOp = edt;
      } else if (!isa<arts::BarrierOp>(&opInst) &&
                 !isa<arts::YieldOp>(&opInst)) {
        llvm_unreachable("Unknown op in parallel op - not supported");
      }
    }
  }
  assert((singleOp && (numOps == 4)) && "Invalid parallel op region structure");
  auto &singleRegion = singleOp->getRegion(0);

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
    newEdt =
        AC->createEdt(&par_deps, &singleRegion, &parEpoch_guid, nullptr, true);
  }

  /// Erase the old parallel op.
  opsToErase.insert(singleOp);
  opsToErase.insert(op);
  LLVM_DEBUG(DBGS() << "Parallel op lowered\n\n");

  /// Visit new function
  assert(newEdt && "New EDT not created");
  LLVM_DEBUG(DBGS() << "New function: " << newEdt->getFunc() << "\n");
  iterateOps(newEdt->getFunc());
}

void ConvertArtsToFuncsPass::handleSingle(EdtOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.edt: " << op << "\n");
  llvm_unreachable("Single EDTs not supported yet");
}

void ConvertArtsToFuncsPass::handleEdt(EdtOp &op) {
  auto *ctx = op.getContext();
  Location loc = UnknownLoc::get(ctx);
  LLVM_DEBUG(DBGS() << "Lowering arts.edt: " << op << "\n");
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
  opsToErase.insert(op);

  /// Visit new function
  assert(newEdt && "New EDT not created");
  LLVM_DEBUG(DBGS() << "New function: " << newEdt->getFunc() << "\n");
  iterateOps(newEdt->getFunc());
}

void ConvertArtsToFuncsPass::handleEvent(EventOp &op) {
  LLVM_DEBUG(DBGS() << "Skipping arts.event: " << op << "\n");
}

// void ConvertArtsToFuncsPass::iterateOps(Operation *operation) {
//   for (auto &region : operation->getRegions()) {
//     for (auto &block : region) {
//       for (auto &op : block) {
//         /// Ignore operations that are marked for removal
//         if (opsToErase.count(&op))
//           continue;

//         /// Handle operations
//         if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
//           opsToErase.insert(&op);
//         } else if (auto edtOp = dyn_cast<arts::EdtOp>(op)) {
//           if (edtOp.isParallel())
//             handleParallel(edtOp);
//           else if (edtOp.isSingle())
//             handleSingle(edtOp);
//           else
//             handleEdt(edtOp);
//         } else if (auto eventOp = dyn_cast<arts::EventOp>(op)) {
//           handleEvent(eventOp);
//         } else {
//           iterateOps(&op);
//         }
//       }
//     }
//   }
// }

void ConvertArtsToFuncsPass::iterateOps(Operation *operation) {
  operation->walk<mlir::WalkOrder::PreOrder>(
      [&](Operation *op) -> mlir::WalkResult {
        /// Skip operations marked for removal.
        if (opsToErase.count(op))
          return mlir::WalkResult::skip();

        if (auto dbOp = dyn_cast<arts::DataBlockOp>(op)) {
          /// Don't remove it here, as it will be removed in the next pass.
          /// Also, it is necessary to lower the datablock operation.
          opsToErase.insert(dbOp);
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

void ConvertArtsToFuncsPass::iterateDbs(Operation *operation) {
  operation->walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DataBlockOp dbOp) { handleDatablock(dbOp); });
}

void ConvertArtsToFuncsPass::handleDatablock(DataBlockOp &op) {
  LLVM_DEBUG(DBGS() << "Lowering arts.datablock: " << op << "\n");
  /// Preprocess the data block operation by converting the memref to a
  /// pointer
  Location loc = op.getLoc();
  auto &builder = AC->getBuilder();
  OpBuilder::InsertionGuard guard(builder);
  AC->setInsertionPoint(op);

  /// Convert all dimensions to dynamic
  auto memrefTy = op.getType().cast<MemRefType>();
  SmallVector<int64_t, 4> dynamicShape(memrefTy.getRank(),
                                       ShapedType::kDynamic);
  auto dynMemrefTy = MemRefType::get(dynamicShape, memrefTy.getElementType());
  auto pointerMemrefTy = MemRefType::get({ShapedType::kDynamic}, dynMemrefTy);

  /// Create a new DataBlockOp with that pointer-based type
  auto newDbOp = builder.create<arts::DataBlockOp>(
      loc, pointerMemrefTy, op.getMode(), op.getBase(), op.getOffsets(),
      op.getSizes(), op.getStrides());
  newDbOp->setAttrs(op->getAttrs());

  /// Walk all uses of the old datablock to properly replace it with the new
  /// pointer
  for (auto &use : llvm::make_early_inc_range(op->getUses())) {
    /// Set insertion point to the user
    auto user = use.getOwner();
    AC->setInsertionPoint(user);
    /// LoadOp - load the pointer using the old load indices and then the
    /// value at index 0
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      auto ptrVal = builder.create<memref::LoadOp>(
          loc, newDbOp.getResult(), ValueRange(loadOp.getIndices()));
      auto c0 = builder.create<arith::ConstantIndexOp>(loc, 0);
      auto finalVal =
          builder.create<memref::LoadOp>(loc, ptrVal, ValueRange{c0});
      loadOp.getResult().replaceAllUsesWith(finalVal.getResult());
      loadOp.erase();
    }
    /// memref.store - load the pointer using the old store indices and then
    /// store the value at index 0
    else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      auto ptrVal = builder.create<memref::LoadOp>(
          loc, newDbOp.getResult(), ValueRange(storeOp.getIndices()));
      auto c0 = builder.create<arith::ConstantIndexOp>(loc, 0);
      builder.create<memref::StoreOp>(loc, storeOp.getValueToStore(), ptrVal,
                                      ValueRange{c0});
      storeOp.erase();
    }
    /// DataBlockOp referencing op - replace base
    else if (auto dbOp = dyn_cast<arts::DataBlockOp>(user)) {
      dbOp.getBase().replaceUsesWithIf(
          newDbOp.getResult(),
          [&](OpOperand &operand) { return operand.getOwner() == dbOp; });
    }
    /// EdtOp - fix dependencies
    else if (auto edtOp = dyn_cast<arts::EdtOp>(user)) {
      for (auto dep : edtOp.getDependencies()) {
        if (dep == op) {
          dep.replaceUsesWithIf(newDbOp.getResult(), [&](OpOperand &operand) {
            return operand.getOwner() == edtOp;
          });
        }
      }
    }
    /// DbSizeOp - replace the uses with the new computed size
    else if (auto dbSizeOp = dyn_cast<arts::DataBlockSizeOp>(user)) {
      dbSizeOp.getDatablock().replaceUsesWithIf(
          newDbOp.getResult(),
          [&](OpOperand &operand) { return operand.getOwner() == dbSizeOp; });
    }
    /// fallback
    else {
      LLVM_DEBUG(DBGS() << "Unknown use of datablock op: " << *user << "\n");
      llvm_unreachable("Unknown use of datablock op");
    }
  }

  /// Create a new DataBlockCodegen object
  AC->getOrCreateDatablock(newDbOp, loc);

  /// Erase old op
  opsToErase.insert(op);
}

void ConvertArtsToFuncsPass::removeOps() {
  for (auto op : opsToErase) {
    if (!op)
      continue;
    replaceWithUndef(op, AC->getBuilder());
    recursivelyRemoveUses(op);
  }
  opsToErase.clear();
}

void ConvertArtsToFuncsPass::runOnOperation() {
  LLVM_DEBUG(dbgs() << "--- ConvertArtsToFuncsPass START ---\n");

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
  /// Handle datablock operations
  for (auto func : module.getOps<func::FuncOp>())
    iterateDbs(func);
  removeOps();
  /// Handle arts operations
  for (auto func : module.getOps<func::FuncOp>())
    iterateOps(func);
  removeOps();
  removeUndefOps(module);

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

  LLVM_DEBUG(dbgs() << "=== ConvertArtsToFuncsPass COMPLETE ===\n");
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
