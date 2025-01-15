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

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/TypeUtilities.h"

#include "arts/Utils/ArtsTypes.h"

#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"

#include "mlir/Interfaces/DataLayoutInterfaces.h"

#include "llvm/Support/Debug.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>

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
                         llvm::DataLayout &llvmDL, mlir::DataLayout &mlirDL)
    : module(module), builder(builder), llvmDL(llvmDL), mlirDL(mlirDL) {
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
  VarName = FunctionType::get(context, {__VA_ARGS__}, {ReturnType});           \
  VarName##Ptr = MemRefType::get(                                              \
      {ShapedType::kDynamic},                                                  \
      LLVM::LLVMFunctionType::get(LLVM::LLVMVoidType::get(context),            \
                                  VarName.getInputs(), false));
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

DataBlockCodegen *ArtsCodegen::getOrCreateDatablock(mlir::Value memref) {
  /// Try to find in the map
  auto it = datablocks.find(memref);
  if (it != datablocks.end()) {
    return it->second;
  }
  /// If it doesnt exist, create it
  auto datablock = new DataBlockCodegen(memref);
  datablocks[memref] = datablock;
  return datablock;
}

mlir::Value ArtsCodegen::createDatablock(arts::MakeDepOp edtDep, Location loc) {
  // /// Get the data block size
  // auto dataSize = edtDep.getDataSize();
  // /// Get the data block alignment
  // auto dataAlignment = edtDep.getDataAlignment();
  // /// Get the data block type
  // auto dataType = edtDep.getDataType();
  // /// Get the data block pointer
  // auto dataPtr = edtDep.getDataPtr();
  /// Get dependency information
  auto mode = edtDep.getMode();
  auto memref = edtDep.getVal();

  LLVM_DEBUG(dbgs() << "Creating datablock for mode: " << mode
                    << ", memref: " << memref << "\n");

  auto db = getOrCreateDatablock(memref);

  /// Memref info
  /// Size
  auto memrefType = memref.getType().cast<MemRefType>();
  // int64_t offset = 0;
  // SmallVector<int64_t, 5> strides;
  // if (failed(getStridesAndOffset(memrefType, strides, offset))) {
  //   LLVM_DEBUG(dbgs() << "Failed to get strides and offset\n");
  // }
  // else {
  //   LLVM_DEBUG(dbgs() << "Memref offset: " << offset << "\n");
  //   for(auto stride : strides) {
  //     LLVM_DEBUG(dbgs() << "Memref stride: " << stride << "\n");
  //   }
  // }
  LLVM_DEBUG(dbgs() << "  - Memref type: " << memrefType << "\n");
  auto memrefElementType = memrefType.getElementType();
  LLVM_DEBUG(dbgs() << "    - Memref element type: " << memrefElementType
                    << "\n");
  if (memrefType.hasStaticShape()) {
    auto memrefSize = memrefType.getShape();
    auto elementType = memrefType.getElementType();
    auto memrefElementSize = mlirDL.getTypeSize(elementType);
    for (auto dim : memrefSize) {
      if (dim == ShapedType::kDynamic) {
        LLVM_DEBUG(dbgs() << "    - Memref size: " << dim << "\n");
      } else {
        LLVM_DEBUG(dbgs() << "    - Memref size: " << dim << " * "
                          << memrefElementSize << "\n");
      }
    }
  } else {
    LLVM_DEBUG(dbgs() << "  - Memref size: dynamic\n");
    /// Check if the memref defining op is an alloc, if so, get the size
    if (auto allocOp = dyn_cast<memref::AllocOp>(memref.getDefiningOp())) {
      auto memrefSize = allocOp.getDynamicSizes();
      auto elementType = memrefType.getElementType();
      auto memrefElementSize = mlirDL.getTypeSize(elementType);
      LLVM_DEBUG(dbgs() << "    - Memref size: ");
      for (auto size : memrefSize) {
        LLVM_DEBUG(dbgs() << size << " * " << memrefElementSize << " ");
      }
      LLVM_DEBUG(dbgs() << "\n");
      LLVM_DEBUG(dbgs() << "    - Memref type: " << elementType << "\n");
    }
  }

  return NULL;
}

func::FuncOp ArtsCodegen::createEdtFunction(Location loc) {
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, EdtFunction);
  edtFuncOp.setPrivate();
  module.push_back(edtFuncOp);
  return edtFuncOp;
}

func::CallOp ArtsCodegen::createEdtCallWithGuid(
    func::FuncOp edtFunc, SmallVector<mlir::Value> parametersValues,
    unsigned numDeps, unsigned route, Location loc) {
  auto edtGuid = createEdtGuid(route, loc);
  auto depC = createIntConstant(numDeps, Int32, loc);
  auto paramV = createParamV(parametersValues, loc);
  auto edtFuncPtr = createFunctionPointer(edtFunc, loc);
  return createRuntimeCall(ARTSRTL_artsEdtCreateWithGuid,
                           {edtFuncPtr, edtGuid, depC, paramV, depC}, loc);
}

func::CallOp ArtsCodegen::createEdtCallWithGuid(arts::EdtOp edtOp,
                                                func::FuncOp edtFunc,
                                                unsigned route, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(edtOp);
  return createEdtCallWithGuid(edtFunc, edtOp.getParametersValues(),
                               edtOp.getNumDependencies(), route, loc);
}

func::CallOp ArtsCodegen::createEdtCallWithGuid(arts::ParallelOp parOp,
                                                func::FuncOp edtFunc,
                                                unsigned route, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(parOp);
  /// Create an empty parallel done function
  auto parallelDoneEdtFunc = createEdtFunction(loc);
  auto parallelDoneEdtGuid =
      createEdtCallWithGuid(parallelDoneEdtFunc, {}, 1, route, loc)
          .getResult(0);
  /// Create an epoch to sync the children edts
  auto parallelEpoch = createRuntimeCall(
      ARTSRTL_artsInitializeAndStartEpoch,
      {parallelDoneEdtGuid, createIntConstant(0, Int32, loc)}, loc);
  /// Create edt with epoch
  auto parallelEdtFunc = createEdtFunction(loc);
  // auto parallelEdtCall =
  //     createEdtCall(parOp.getParametersValues(), parOp.getNumDependencies());
  // return createEdtCall(parOp.getParametersValues(),
  // parOp.getNumDependencies());
  return NULL;
}

func::CallOp ArtsCodegen::createEdtCallWithEpoch(arts::EdtOp edtOp,
                                                 func::FuncOp edtFunc,
                                                 unsigned route, Value epoch,
                                                 Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(edtOp);
  auto routeC = createIntConstant(route, Int32, loc);
  auto depC = createIntConstant(edtOp.getNumDependencies(), Int32, loc);
  auto edtParameters = edtOp.getParametersValues();
  auto paramV = createParamV(edtParameters, loc);
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
  // mlir::MemRefType::get({ShapedType::kDynamic}, Int64);
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

Value ArtsCodegen::createFunctionPointer(func::FuncOp funcOp, Location loc) {
  auto FT = funcOp.getFunctionType();
  auto LFT = LLVM::LLVMFunctionType::get(
      LLVM::LLVMVoidType::get(funcOp.getContext()), FT.getInputs(), false);
  auto getFuncOp = builder.create<polygeist::GetFuncOp>(
      loc, LLVM::LLVMPointerType::get(LFT), funcOp.getName());
  return builder
      .create<polygeist::Pointer2MemrefOp>(
          loc, MemRefType::get({ShapedType::kDynamic}, LFT), getFuncOp)
      .getResult();
}

Value ArtsCodegen::castToInt64(Value value, Location loc) {
  /// If it is not an i64, cast it to i64.
  /// Handles index types, floats, and other integer types.
  auto targetType = builder.getI64Type();
  if (value.getType() != targetType) {
    if (value.getType().isIndex()) {
      // Handle index type conversion to i64
      value = builder.create<arith::IndexCastOp>(loc, targetType, value)
                  .getResult();
    } else if (value.getType().isa<mlir::FloatType>()) {
      // Handle float type conversion to i64
      value =
          builder.create<arith::FPToSIOp>(loc, targetType, value).getResult();
    } else if (auto intType = value.getType().dyn_cast<mlir::IntegerType>()) {
      // Handle other integer types (e.g., i8, i16, i32)
      if (intType.isSignless()) {
        value =
            builder.create<arith::ExtSIOp>(loc, targetType, value).getResult();
      } else {
        value =
            builder.create<arith::ExtUIOp>(loc, targetType, value).getResult();
      }
    } else {
      // Unsupported type, emit an error or assert
      llvm::errs() << "Unsupported type for casting to i64: " << value.getType()
                   << "\n";
      assert(false && "Unsupported type");
    }
  }
  return value;
}

Value ArtsCodegen::createParamV(SmallVector<Value> &parameters, Location loc) {
  /// Create array of i64
  unsigned int paramSize = parameters.size();
  auto paramVArray = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({paramSize}, Int64));
  for (unsigned i = 0; i < paramSize; i++) {
    /// If the value is not an i64, cast it
    auto param = castToInt64(parameters[i], loc);
    /// Affine store
    auto paramIndex = builder.create<arith::ConstantIndexOp>(loc, i);
    builder.create<affine::AffineStoreOp>(loc, param, paramVArray,
                                          ValueRange{paramIndex});
  }
  /// memref cast to memref<-1xi64>
  auto paramV = builder.create<memref::CastOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, Int64), paramVArray);
  return paramV.getResult();
}

Value ArtsCodegen::createIntConstant(unsigned value, Type type, Location loc) {
  return builder.create<arith::ConstantOp>(loc, type,
                                           builder.getIntegerAttr(type, value));
}

Value ArtsCodegen::createEdtGuid(uint64_t node, Location loc) {
  auto guidEdtType = createIntConstant(1, artsType_t, loc);
  auto guidEdtNode = createIntConstant(node, Int32, loc);
  auto reserveGuidCall = createRuntimeCall(ARTSRTL_artsReserveGuidRoute,
                                           {guidEdtType, guidEdtNode}, loc);
  return reserveGuidCall.getResult(0);
}