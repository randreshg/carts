//===- ArtsCodegen.cpp - Builder for LLVM-IR for ARTS directives ----===//
//===----------------------------------------------------------------------===//
/// This file implements the ArtsCodegen class, which is used as a
/// convenient way to create LLVM instructions for ARTS directives.
///
//===----------------------------------------------------------------------===//

#include "arts/Codegen/ArtsCodegen.h"
#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

#include "arts/Utils/ArtsTypes.h"

#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"

#include "mlir/Interfaces/DataLayoutInterfaces.h"

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
using namespace mlir::polygeist;

ArtsCodegen::ArtsCodegen(ModuleOp &module, OpBuilder &builder,
                         llvm::DataLayout &dataLayout)
    : module(module), builder(builder), dataLayout(dataLayout) {
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
  MLIRContext *context = module.getContext();
#define ARTS_TYPE(VarName, InitValue) VarName = InitValue;
#define ARTS_FUNCTION_TYPE(VarName, ReturnType, ...)                           \
  VarName = FunctionType::get(context, {__VA_ARGS__}, {ReturnType});
#define ARTS_STRUCT_TYPE(VarName, StructName, Packed, ...)                     \
  VarName = LLVM::LLVMStructType::getLiteral(context, {__VA_ARGS__}, Packed);  \
  VarName##Ptr = MemRefType::get({ShapedType::kDynamic}, VarName);
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
#define ARTS_RTL(Enum, Str, ReturnType, ...)                                   \
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
  /// Set the function as private
  funcOp.setPrivate();
  /// Set the llvm.linkage attribute to external
  funcOp->setAttr(
      "llvm.linkage",
      LLVM::LinkageAttr::get(builder.getContext(), LLVM::Linkage::External));

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

func::CallOp ArtsCodegen::createEdtCallWithGuid(arts::EdtOp edtOp,
                                                func::FuncOp edtFunc,
                                                unsigned route, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(edtOp);
  auto edtGuid = createEdtGuid(route, loc);
  auto depC = createIntConstant(edtOp.getNumDependencies(), Int32, loc);
  auto paramC = createIntConstant(edtOp.getNumParameters(), Int32, loc);
  auto edtParameters = edtOp.getParametersValues();
  auto paramV = createParamV(edtParameters, paramC, loc);
  auto edtFuncPtr = createFunctionPointer(edtFunc, loc);
  /// Create edt with guid
  SmallVector<Value, 5> args = {edtFuncPtr, edtGuid, depC, paramV, depC};
  return createRuntimeCall(ARTSRTL_artsEdtCreateWithGuid, args, loc);
}

func::CallOp ArtsCodegen::createEdtCallWithGuid(arts::ParallelOp parOp,
                                                func::FuncOp edtFunc,
                                                unsigned route, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(parOp);
  auto edtGuid = createEdtGuid(route, loc);
  auto depC = createIntConstant(parOp.getNumDependencies(), Int32, loc);
  auto paramC = createIntConstant(parOp.getNumParameters(), Int32, loc);
  auto edtParameters = parOp.getParametersValues();
  auto paramV = createParamV(edtParameters, paramC, loc);
  auto edtFuncPtr = createFunctionPointer(edtFunc, loc);
  /// Create edt with guid
  SmallVector<Value, 5> args = {edtFuncPtr, edtGuid, depC, paramV, depC};
  return createRuntimeCall(ARTSRTL_artsEdtCreateWithGuid, args, loc);
}

func::CallOp ArtsCodegen::createEdtCallWithEpoch(arts::EdtOp edtOp,
                                                 func::FuncOp edtFunc,
                                                 unsigned route, Value epoch,
                                                 Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(edtOp);
  auto routeC = createIntConstant(route, Int32, loc);
  auto depC = createIntConstant(edtOp.getNumDependencies(), Int32, loc);
  auto paramC = createIntConstant(edtOp.getNumParameters(), Int32, loc);
  auto edtParameters = edtOp.getParametersValues();
  auto paramV = createParamV(edtParameters, paramC, loc);
  auto edtFuncPtr = createFunctionPointer(edtFunc, loc);
  /// Create edt with epoch
  return createRuntimeCall(ARTSRTL_artsEdtCreateWithEpoch,
                           {edtFuncPtr, routeC, depC, paramV, depC, epoch},
                           loc);
}

Value ArtsCodegen::insertEdtEntry(func::FuncOp edtFunc,
                                  SmallVector<Value> &opParameters,
                                  SmallVector<Value> &opDependencies,
                                  Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  // setInsertionPoint(edtFunc.getBody());
  mlir::MemRefType::get({-1}, Int64);
  auto &entryBlock = edtFunc.front();
  /// Get reference to the block arguments
  Value paramC = entryBlock.getArgument(0);
  Value paramV = entryBlock.getArgument(1);
  Value depC = entryBlock.getArgument(2);
  Value depV = entryBlock.getArgument(3);
  /// Insert the parameters
  auto paramSize = opParameters.size();
  for (unsigned paramIndex = 0; paramIndex < paramSize; paramIndex++) {
    Value oldParam = opParameters[paramIndex];
    /// Build and i64 constant for 'i'
    auto i = builder.create<arith::ConstantOp>(
        loc, Int64, builder.getIntegerAttr(Int64, paramIndex));
    /// Create the GEP to load the value in the ParamV array
    // auto paramVElemPtr = builder.create<LLVM::GEPOp>(
    //     loc, Int64, paramV, ValueRange{i}, "paramv_" +
    //     std::to_string(paramIndex));

    // Value oldParam = opParameters[parameterIndex];
    // auto paramVName = "paramv_" + std::to_string(paramIndex);
    // Value paramVElemPtr = builder.create<LLVM::GEPOp>(
    //     loc, Int64, paramV, ValueRange{paramIndex}, paramVName);
    // builder.create<LLVM::StoreOp>(loc, param, paramVElemPtr);
    // paramIndex++;
  }
}

/// Helpers
Value ArtsCodegen::createFunctionPointer(func::FuncOp funcOp, Location loc) {
  // ValueCategory
  auto FT = funcOp.getFunctionType();
  mlir::Type RT = LLVM::LLVMVoidType::get(funcOp.getContext());
  LLVM::LLVMFunctionType LFT =
      LLVM::LLVMFunctionType::get(RT, FT.getInputs(), false);
  auto getFuncOp = builder.create<polygeist::GetFuncOp>(
      loc, LLVM::LLVMPointerType::get(LFT), funcOp.getName());
  auto funcMemRef = builder.create<polygeist::Pointer2MemrefOp>(
      loc, MemRefType::get({-1}, FT), getFuncOp.getResult());
  return funcMemRef.getResult();
}

Value ArtsCodegen::createParamV(SmallVector<Value> &parameters, Value paramC,
                                Location loc) {
  unsigned int paramSize = parameters.size();
  for (unsigned i = 0; i < paramSize; i++) {
    /// If the value is not an i64, cast it
    auto param = parameters[i];
    if (param.getType() != Int64) {
      auto castOp = builder.create<arith::ExtUIOp>(loc, Int64, param);
      param = castOp.getResult();
    }
    /// Affine store
    auto i64Const = builder.create<arith::ConstantOp>(
        loc, Int64, builder.getIntegerAttr(Int64, i));
    builder.create<affine::AffineStoreOp>(loc, param, paramC,
                                          ValueRange{i64Const});
  }
  /// Create array of i64
  auto paramSizeValue = createIntConstant(paramSize, Int64, loc);
  auto paramVArray =
      builder
          .create<memref::AllocaOp>(loc, MemRefType::get({paramSize}, Int64),
                                    paramSizeValue)
          .getResult();
  /// memref cast to memref<-1xi64>
  auto paramV = builder.create<memref::CastOp>(
      loc, MemRefType::get({-1}, Int64), paramVArray);
  return paramV.getResult();
}

Value ArtsCodegen::createIntConstant(unsigned value, Type type, Location loc) {
  return builder.create<arith::ConstantOp>(loc, type,
                                           builder.getIntegerAttr(type, value));
}

Value ArtsCodegen::createEdtGuid(uint64_t node, Location loc) {
  auto guidEdtType = createIntConstant(1, Int32, loc);
  auto guidEdtNode = createIntConstant(node, Int64, loc);
  auto reserveGuidCall = createRuntimeCall(ARTSRTL_artsReserveGuidRoute,
                                           {guidEdtType, guidEdtNode}, loc);
  return reserveGuidCall.getResult(0);
}