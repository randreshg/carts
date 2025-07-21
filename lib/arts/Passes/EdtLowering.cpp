//===----------------------------------------------------------------------===//
// Edt/EdtLoweringPass.cpp - Pass to lower EDTs with outlining and packing
//===----------------------------------------------------------------------===//

// EdtLoweringPass Explanation
//
// This pass lowers EdtOps by outlining regions to functions and packing params/deps:
// - Outlines region to func with ARTS signature.
// - Inserts edt_param_pack before EdtOp for param_memref.
// - Inserts edt_param_unpack in function for params (multiple results).
// - Inserts edt_dep_unpack in function for deps (loads guids, calls artsGetFromDb).
// - Replaces EdtOp with edt_outline_fn (takes param_memref; returns guid; attr: outlined_func).
// - Adds record_in_dep and increment_out_latch after edt_outline_fn using guid.
// - Deps not packed separately; record/increment use individual deps post-creation.

/// Dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "edt-lowering"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

namespace {

// EdtLoweringPass: Lowers EDTs by outlining and packing params/deps.
struct EdtLoweringPass : public arts::EdtLoweringBase<EdtLoweringPass> {
  void runOnOperation() override;

private:
  bool lowerEdt(EdtOp edtOp);
  func::FuncOp outlineFunction(EdtOp edtOp, const SmallVector<Value> ¶ms, const SmallVector<Value> &consts, const SmallVector<Value> &deps);
  void insertParamUnpack(func::FuncOp edtFunc, const SmallVector<Value> ¶ms);
  void insertDepUnpack(func::FuncOp edtFunc, const SmallVector<Value> &deps);
  Value packParams(EdtOp edtOp, const SmallVector<Value> ¶ms);
  void insertRecordAndIncrement(EdtOp edtOp, Value edtGuid, const SmallVector<Value> &deps);
};

} // namespace

void EdtLoweringPass::runOnOperation() {
  ModuleOp module = getOperation();
  bool changed = false;

  module.walk([&](EdtOp edtOp) {
    if (lowerEdt(edtOp)) changed = true;
  });

  if (!changed) {
    markAllAnalysesPreserved();
  }
}

bool EdtLoweringPass::lowerEdt(EdtOp edtOp) {
  Location loc = edtOp.getLoc();

  // Collect free vars and classify (params are non-const free vars)
  SmallVector<Value> freeVars = getFreeVars(edtOp.getRegion());
  SmallVector<Value> params, consts;
  classifyFreeVars(freeVars, params, consts);

  // Deps from EdtOp operands
  SmallVector<Value> deps = edtOp.getDependencies();

  // Outline function
  func::FuncOp edtFunc = outlineFunction(edtOp, params, consts, deps);

  // Pack params before EdtOp
  Value paramMem = packParams(edtOp, params);

  // Create edt_outline_fn (returns guid for record/increment)
  OpBuilder builder(edtOp);
  auto edtOutlineOp = builder.create<EdtOutlineFnOp>(loc, builder.getI64Type(), paramMem);
  edtOutlineOp->setAttr("outlined_func", builder.getStringAttr(edtFunc.getName()));
  Value edtGuid = edtOutlineOp.getGuid();

  // Insert record/increment after
  insertRecordAndIncrement(edtOp, edtGuid, deps);

  // Erase original EdtOp
  edtOp.erase();

  return true;
}

func::FuncOp EdtLoweringPass::outlineFunction(EdtOp edtOp, const SmallVector<Value> ¶ms, const SmallVector<Value> &consts, const SmallVector<Value> &deps) {
  Location loc = edtOp.getLoc();
  ModuleOp module = edtOp->getParentOfType<ModuleOp>();
  OpBuilder builder(module.getBodyRegion());

  // ARTS signature
  auto ptrTy = LLVM::LLVMPointerType::get(builder.getContext());
  auto i32Ty = builder.getI32Type();
  auto i64Ty = builder.getI64Type();
  FunctionType edtFuncType = FunctionType::get(
      builder.getContext(), 
      {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy}, 
      {});

  static unsigned edtId = 0;
  std::string funcName = "edt_" + edtOp.getName().str() + "_" + std::to_string(++edtId);
  auto edtFunc = builder.create<func::FuncOp>(loc, funcName, edtFuncType);
  edtFunc.setPrivate();

  auto *entryBlock = edtFunc.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  // Clone constants
  DenseMap<Value, Value> remap;
  for (Value c : consts) {
    remap[c] = builder.clone(*c.getDefiningOp())->getResult(0);
  }

  // Insert unpacks (add to remap)
  insertParamUnpack(edtFunc, params, remap);
  insertDepUnpack(edtFunc, deps, remap);

  // Clone region with remap
  edtOp.getRegion().cloneInto(&edtFunc.getBody(), remap);

  // Replace yield with return
  auto &oldBlock = edtOp.getRegion().front();
  if (auto yield = dyn_cast<YieldOp>(oldBlock.getTerminator())) {
    builder.setInsertionPointToEnd(&edtFunc.getBody().front());
    builder.create<func::ReturnOp>(loc);
    yield.erase();
  }

  // Clear old region
  edtOp.getRegion().getBlocks().clear();

  return edtFunc;
}

void EdtLoweringPass::insertParamUnpack(func::FuncOp edtFunc, const SmallVector<Value> ¶ms, DenseMap<Value, Value> &remap) {
  Location loc = edtFunc.getLoc();
  Block *entry = &edtFunc.getBody().front();
  OpBuilder builder(entry, entry->begin());

  Value paramv = entry->getArgument(2);  // paramv ptr
  SmallVector<Type> paramTypes;
  for (Value p : params) paramTypes.push_back(p.getType());

  auto unpackOp = builder.create<EdtParamUnpackOp>(loc, paramTypes, paramv);

  for (size_t i = 0; i < params.size(); ++i) {
    remap[params[i]] = unpackOp.getResult(i);
  }
}

void EdtLoweringPass::insertDepUnpack(func::FuncOp edtFunc, const SmallVector<Value> &deps, DenseMap<Value, Value> &remap) {
  Location loc = edtFunc.getLoc();
  Block *entry = &edtFunc.getBody().front();
  OpBuilder builder(entry, entry->begin());

  Value depv = entry->getArgument(4);  // depv ptr
  SmallVector<Type> depTypes;
  for (Value d : deps) depTypes.push_back(d.getType());

  auto unpackOp = builder.create<EdtDepUnpackOp>(loc, depTypes, depv);

  for (size_t i = 0; i < deps.size(); ++i) {
    remap[deps[i]] = unpackOp.getResult(i);
  }
}

Value EdtLoweringPass::packParams(EdtOp edtOp, const SmallVector<Value> ¶ms) {
  Location loc = edtOp.getLoc();
  OpBuilder builder(edtOp);

  auto packOp = builder.create<EdtParamPackOp>(loc, params);
  return packOp.getMemref();
}

void EdtLoweringPass::insertRecordAndIncrement(EdtOp edtOp, Value edtGuid, const SmallVector<Value> &deps) {
  Location loc = edtOp.getLoc();
  OpBuilder builder(edtOp.getNextNode());

  int slot = 0;
  for (Value dep : deps) {
    // Assume analysis to get mode (in/out)
    StringRef mode = getDepMode(dep);  // Placeholder helper
    if (mode == "in" || mode == "inout") {
      builder.create<RecordInDepOp>(loc, edtGuid, dep, builder.getI32Attr(slot));
    }
    if (mode == "out" || mode == "inout") {
      builder.create<IncrementOutLatchOp>(loc, dep);
    }
    slot++;
  }
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtLoweringPass() {
  return std::make_unique<EdtLoweringPass>();
}
} // namespace arts
} // namespace mlir