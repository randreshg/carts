//===- ArtsCodegen.cpp - Builder for LLVM-IR for ARTS directives ----===//
//===----------------------------------------------------------------------===//
/// This file implements the ArtsCodegen class, which is used as a
/// convenient way to create LLVM instructions for ARTS directives.
///
//===----------------------------------------------------------------------===//

#include "arts/Codegen/ArtsCodegen.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

#include "arts/Utils/ArtsTypes.h"

#include "llvm/Support/Debug.h"

#include <cassert>

// DEBUG
#define DEBUG_TYPE "arts-codegen"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::LLVM;
using namespace mlir::arts;
using namespace mlir::arith;

ArtsCodegen::ArtsCodegen(ModuleOp &module, OpBuilder &builder)
    : module(module), builder(builder), dataLayout(mlir::DataLayout(module)) {
  initialize();
}

ArtsCodegen::~ArtsCodegen() {}

void ArtsCodegen::initialize() {
  LLVM_DEBUG(DBGS() << "Initializing ArtsCodegen\n");
  initializeTypes();
  LLVM_DEBUG(DBGS() << "ArtsCodegen initialized\n");
}

void ArtsCodegen::finalize() {
  LLVM_DEBUG(DBGS() << "Finalizing ArtsCodegen\n");
  LLVM_DEBUG(DBGS() << "ArtsCodegen finalized\n");
}

void ArtsCodegen::initializeTypes() {
  MLIRContext *ctx = module.getContext();
#define ARTS_TYPE(VarName, InitValue) VarName = InitValue;
#define ARTS_ARRAY_TYPE(VarName, ElemTy, ArraySize)                            \
  VarName##ArrayTy = LLVM::LLVMArrayType::get(ElemTy, ArraySize);              \
  VarName##PtrTy = LLVM::LLVMPointerType::get(ctx);
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  VarName = FunctionType::get(ctx, {__VA_ARGS__}, {ReturnType});
#define ARTS_STRUCT_TYPE(VarName, StructName, Packed, ...)                     \
  VarName = LLVM::LLVMStructType::getLiteral(ctx, {__VA_ARGS__}, Packed);      \
  VarName##Ptr = LLVM::LLVMPointerType::get(ctx);
#include "arts/Codegen/ARTSKinds.def"
}

func::FuncOp
ArtsCodegen::getOrCreateRuntimeFunction(types::RuntimeFunction FnID) {
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(module.getBody());

  FunctionType fnType = nullptr;
  func::FuncOp funcOp;
  /// Try to find the declaration in the module first.
  switch (FnID) {
#define ARTS_RTL(Enum, Str, IsVarArg, ReturnType, ...)                         \
  case Enum: {                                                                 \
    SmallVector<Type, 4> argumentTypes{__VA_ARGS__};                           \
    fnType = builder.getFunctionType(argumentTypes,                            \
                                     ReturnType.isa<mlir::NoneType>()          \
                                         ? ArrayRef<Type>{}                    \
                                         : ArrayRef<Type>{ReturnType});        \
    funcOp = module.lookupSymbol<func::FuncOp>(Str);                           \
    break;                                                                     \
  }
#include "arts/Codegen/ARTSKinds.def"
  }

  if (!funcOp) {
    // Create a new declaration if we need one.
    switch (FnID) {
#define ARTS_RTL(Enum, Str, ...)                                               \
  case Enum:                                                                   \
    funcOp =                                                                   \
        builder.create<func::FuncOp>(builder.getUnknownLoc(), Str, fnType);    \
    break;
#include "arts/Codegen/ARTSKinds.def"
    }
  }

  assert(funcOp && "Failed to create ARTS runtime function");
  return funcOp;
}

func::CallOp ArtsCodegen::createRuntimeCall(RuntimeFunction FnID,
                                            ArrayRef<mlir::Value> args,
                                            Location loc) {
  /// Get or create the runtime function
  func::FuncOp func = getOrCreateRuntimeFunction(FnID);
  assert(func && "Runtime function should exist");

  /// Create the call operation
  func::CallOp call = builder.create<func::CallOp>(loc, func, args);
  return call;
}

func::FuncOp ArtsCodegen::createEdtFunction(Location loc) {
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, EdtFunction);
  module.push_back(edtFuncOp);
  return edtFuncOp;
}

func::CallOp ArtsCodegen::insertEdtCall(arts::EdtOp edtOp, func::FuncOp edtFunc,
                                        Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(edtOp);
  auto edtGuid = createEdtGuid(0, loc);
  auto edtParameters = edtOp.getParametersValues();
  auto edtDepNum = edtOp.getNumDependencies();
  LLVM_DEBUG(dbgs() << "EDT GUID: " << edtGuid << "\n");
  auto edtCall =
      createEdtCall(edtFunc, edtGuid, edtParameters, edtDepNum, edtOp.getLoc());
  return edtCall;
}

// artsGuid_t artsEdtCreateWithGuid(artsEdt_t funcPtr, artsGuid_t guid,
// uint32_t paramc, uint64_t * paramv, uint32_t depc);
func::CallOp ArtsCodegen::createEdtCall(func::FuncOp edtFunc,
                                        mlir::Value edtGuid,
                                        SmallVector<Value> &edtParameters,
                                        unsigned edtDepNum, Location loc) {
  auto edtFuncPtr = builder.create<func::ConstantOp>(
      loc, EdtFunction, SymbolRefAttr::get(edtFunc));

  LLVM_DEBUG(DBGS() << "EDT function pointer: " << edtFuncPtr << "\n");
  /// uint32_t paramc
  auto paramc =
      builder
          .create<arith::ConstantOp>(
              loc, Int32, builder.getIntegerAttr(Int32, edtParameters.size()))
          .getResult();
  LLVM_DEBUG(DBGS() << "EDT parameters count: " << paramc << "\n");
  /// uint64_t * paramv
  auto paramv =
      builder.create<LLVM::AllocaOp>(loc, Int64Ptr, paramc).getResult();
  LLVM_DEBUG(DBGS() << "EDT parameters: " << paramv << "\n");
  /// uint32_t depc
  auto depc = builder
                  .create<arith::ConstantOp>(
                      loc, Int32, builder.getIntegerAttr(Int32, edtDepNum))
                  .getResult();
  LLVM_DEBUG(DBGS() << "EDT dependencies count: " << depc << "\n");
  /// Store the parameters
  SmallVector<Value, 5> args = {edtFuncPtr.getResult(), edtGuid, paramc, paramv,
                                depc};
  return createRuntimeCall(ARTSRTL_artsEdtCreateWithGuid, args, loc);
}

Value ArtsCodegen::createEdtGuid(uint64_t edtNode, Location loc) {
  auto guidEdtType = builder.create<arith::ConstantOp>(
      loc, Int32, builder.getIntegerAttr(Int32, 1));
  auto guidEdtNode = builder.create<arith::ConstantOp>(
      loc, Int32, builder.getIntegerAttr(Int32, edtNode));
  auto reserveGuidCall = createRuntimeCall(ARTSRTL_artsReserveGuidRoute,
                                           {guidEdtType, guidEdtNode}, loc);
  return reserveGuidCall.getResult(0);
}
