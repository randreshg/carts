//===- ArtsCodegen.cpp - Builder for LLVM-IR for ARTS directives ----===//
//===----------------------------------------------------------------------===//
/// This file implements the ArtsCodegen class, which is used as a
/// convenient way to create LLVM instructions for ARTS directives.
///
//===----------------------------------------------------------------------===//

#include "arts/Codegen/ArtsCodegen.h"

// #include "arts/utils/ARTSCache.h"
// #include "arts/utils/ARTSUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

#include "arts/Utils/ArtsTypes.h"

#include "llvm/Support/Debug.h"

#include <cassert>
// #include <cstdint>
// #include <string>
// #include <sys/types.h>

// DEBUG
#define DEBUG_TYPE "arts-codegen"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace arts;
using namespace mlir::func;
using namespace mlir::LLVM;

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
  MLIRContext *Ctx = module.getContext();
#define ARTS_TYPE(VarName, InitValue) VarName = InitValue;
#define ARTS_ARRAY_TYPE(VarName, ElemTy, ArraySize)                            \
  VarName##ArrayTy = LLVM::LLVMArrayType::get(ElemTy, ArraySize);              \
  VarName##PtrTy = LLVM::LLVMPointerType::get(ElemTy);
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  VarName = LLVM::LLVMFunctionType::get(ReturnType, {__VA_ARGS__}, IsVarArg);  \
  VarName##Ptr = LLVM::LLVMPointerType::get(VarName);
#define ARTS_STRUCT_TYPE(VarName, StructName, Packed, ...)                     \
  VarName = LLVM::LLVMStructType::getLiteral(builder.getContext(),             \
                                             {__VA_ARGS__}, Packed);           \
  VarName##Ptr = LLVM::LLVMPointerType::get(VarName);
#include "arts/Codegen/ARTSKinds.def"
}

func::FuncOp
ArtsCodegen::getOrCreateRuntimeFunction(types::RuntimeFunction FnID) {
  LLVM::LLVMFunctionType fnType = nullptr;
  func::FuncOp funcOp;

  /// Try to find the declaration in the module first.
  switch (FnID) {
#define ARTS_RTL(Enum, Str, IsVarArg, ReturnType, ...)                         \
  case Enum:                                                                   \
    fnType = LLVM::LLVMFunctionType::get(ReturnType, {__VA_ARGS__}, IsVarArg); \
    funcOp = module.lookupSymbol<func::FuncOp>(Str);                           \
    break;
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

Value ArtsCodegen::createRuntimeCall(RuntimeFunction FnID,
                                     ArrayRef<mlir::Value> args, Location loc) {
  // Get or create the runtime function
  func::FuncOp func = getOrCreateRuntimeFunction(FnID);
  assert(func && "Runtime function should exist");

  // Create the call operation
  CallOp call = builder.create<CallOp>(loc, func, args);
  LLVM_DEBUG(DBGS() << "Created call to runtime function: " << func->getName()
                    << "\n");
  return call.getResult(0);
}