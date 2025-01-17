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

#include "mlir/IR/Types.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"

#include "mlir/Interfaces/DataLayoutInterfaces.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"

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

/// DataBlock
mlir::Value ArtsCodegen::getDatablockMode(llvm::StringRef mode) {
  auto Loc = UnknownLoc::get(builder.getContext());
  /// ARTS_DB_READ
  if (mode == "in")
    return createIntConstant(7, Int32, Loc);
  /// ARTS_DB_PIN
  if (mode == "out")
    return createIntConstant(9, Int32, Loc);
  return createIntConstant(9, Int32, Loc);
}

DataBlockCodegen *ArtsCodegen::getDatablock(arts::MakeDepOp edtDep) {
  /// Try to find in the map
  auto it = datablocks.find(edtDep);
  if (it != datablocks.end())
    return it->second;
  return nullptr;
}

DataBlockCodegen *ArtsCodegen::createDatablock(arts::MakeDepOp edtDep,
                                               Location loc) {
  assert(getDatablock(edtDep) == nullptr && "Datablock already exists");

  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(edtDep);

  /// If the value is a load, get the memref
  auto dbMemref = edtDep.getVal();
  if (auto loadOp = dyn_cast<memref::LoadOp>(dbMemref.getDefiningOp()))
    dbMemref = loadOp.getMemref();

  /// Get the memref type
  auto memrefType = dbMemref.getType().dyn_cast<MemRefType>();
  assert(memrefType && "Expected MemRefType");

  /// Get the size of the datablocl=k
  mlir::Value constantOne = builder.create<arith::ConstantIndexOp>(loc, 1);
  mlir::Value numElements = constantOne;
  for (int64_t i = 0, rank = memrefType.getRank(); i < rank; ++i) {
    mlir::Value dimSize =
        memrefType.isDynamicDim(i)
            ? builder.create<memref::DimOp>(loc, dbMemref, i).getResult()
            : builder
                  .create<arith::ConstantIndexOp>(loc, memrefType.getDimSize(i))
                  .getResult();
    numElements = builder.create<arith::MulIOp>(loc, numElements, dimSize);
  }
  /// Create a datablock or an array of datablocks based on the size
  if (numElements == constantOne) {
    /// Report error - Not supported
    assert(0 && "Not implemented yet");
  }

  /// Allocate an array of Datablocks
  auto dbArray = builder.create<memref::AllocaOp>(loc, ArtsDbPtr, numElements);
  auto dbSize = createIntConstant(
      mlirDL.getTypeSize(memrefType.getElementType()), Int64, loc);
  auto dbMode = getDatablockMode(edtDep.getMode());

  /// Create the datablock
  createRuntimeCall(ARTSRTL_artsDbCreateArray,
                    {dbArray, dbSize, dbMode,
                     castToInt(Int32, numElements, loc),
                     castToPtr(dbMemref, loc)},
                    loc);

  /// Create the datablock codegen object
  datablocks[edtDep] = new DataBlockCodegen(dbArray, numElements);
  return datablocks[edtDep];
}

DataBlockCodegen *ArtsCodegen::getOrCreateDatablock(arts::MakeDepOp edtDep,
                                                    Location loc) {
  /// Try to find in the map
  if (auto db = getDatablock(edtDep))
    return db;
  /// If it doesnt exist, create it
  return createDatablock(edtDep, loc);
}

/// Edt
Value ArtsCodegen::createEdtGuid(Value node, Location loc) {
  auto guidEdtType = createIntConstant(1, artsType_t, loc);
  auto reserveGuidCall =
      createRuntimeCall(ARTSRTL_artsReserveGuidRoute, {guidEdtType, node}, loc);
  return reserveGuidCall.getResult(0);
}

func::FuncOp ArtsCodegen::createEdtFunction(Location loc) {
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, EdtFn);
  edtFuncOp.setPrivate();
  module.push_back(edtFuncOp);
  return edtFuncOp;
}

func::CallOp
ArtsCodegen::createEdtCallWithGuid(func::FuncOp edtFunc, Value guid,
                                   SmallVector<mlir::Value> parametersValues,
                                   Value depC, Value node, Location loc) {
  auto paramC = createIntConstant(parametersValues.size(), Int32, loc);
  auto paramV = createParamV(parametersValues, loc);
  auto edtFuncPtr = createFunctionPtr(edtFunc, loc);
  return createRuntimeCall(ARTSRTL_artsEdtCreateWithGuid,
                           {edtFuncPtr, guid, paramC, paramV, depC}, loc);
}

Value ArtsCodegen::createEdt(func::FuncOp edtFunc,
                             SmallVector<mlir::Value> parametersValues,
                             Value depC, Value node, Location loc) {
  auto paramC = createIntConstant(parametersValues.size(), Int32, loc);
  auto paramV = createParamV(parametersValues, loc);
  auto edtFuncPtr = createFunctionPtr(edtFunc, loc);
  return createRuntimeCall(ARTSRTL_artsEdtCreate,
                           {edtFuncPtr, node, paramC, paramV, depC}, loc)
      .getResult(0);
}

Value ArtsCodegen::createEdtWithEpoch(func::FuncOp edtFunc,
                                      SmallVector<mlir::Value> parametersValues,
                                      Value depC, Value node, Value epoch,
                                      Location loc) {
  auto paramC = createIntConstant(parametersValues.size(), Int32, loc);
  auto paramV = createParamV(parametersValues, loc);
  auto edtFuncPtr = createFunctionPtr(edtFunc, loc);
  return createRuntimeCall(
             types::RuntimeFunction::ARTSRTL_artsEdtCreateWithEpoch,
             {edtFuncPtr, node, paramC, paramV, depC, epoch}, loc)
      .getResult(0);
}

Value ArtsCodegen::insertEdtEntry(func::FuncOp edtFunc,
                                  SmallVector<Value> &params,
                                  SmallVector<Value> &deps, Location loc) {
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
  auto paramSize = params.size();
  for (unsigned paramIndex = 0; paramIndex < paramSize; paramIndex++) {
    Value oldParam = params[paramIndex];
    /// Build and i64 constant for 'i'
    auto i = builder.create<arith::ConstantOp>(
        loc, Int64, builder.getIntegerAttr(Int64, paramIndex));
    /// Create the GEP to load the value in the ParamV array
    // auto paramVElemPtr = builder.create<LLVM::GEPOp>(
    //     loc, Int64, paramV, ValueRange{i}, "paramv_" +
    //     std::to_string(paramIndex));

    // Value oldParam = params[parameterIndex];
    // auto paramVName = "paramv_" + std::to_string(paramIndex);
    // Value paramVElemPtr = builder.create<LLVM::GEPOp>(
    //     loc, Int64, paramV, ValueRange{paramIndex}, paramVName);
    // builder.create<LLVM::StoreOp>(loc, param, paramVElemPtr);
    // paramIndex++;
  }
}

void ArtsCodegen::outlineRegion(func::FuncOp func, Region &region,
                                SmallVector<Value> &params,
                                SmallVector<Value> &deps) {
  /// Create a new block for the function
  OpBuilder::InsertionGuard guard(builder);
  auto *entryBlock = func.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Get reference to the block arguments
  Value paramC = entryBlock->getArgument(0);
  Value paramV = entryBlock->getArgument(1);
  Value depC = entryBlock->getArgument(2);
  Value depV = entryBlock->getArgument(3);

  /// Insert the parameters
  auto loc = func.getLoc();
  auto paramSize = params.size();
  for (unsigned paramIndex = 0; paramIndex < paramSize; paramIndex++) {
    auto i = builder.create<arith::ConstantIndexOp>(loc, paramIndex);
    auto paramVElem = builder.create<memref::LoadOp>(loc, paramV, ValueRange{i});
    /// Cast it back to the original type
    auto oldParam = params[paramIndex];
    LLVM_DEBUG(DBGS() << "Old param: " << oldParam << "\n");
    auto newParam = castToInt(oldParam.getType(), paramVElem, loc);
    oldParam.replaceUsesWithIf(newParam, [&](OpOperand &operand) {
      return region.isAncestor(operand.getOwner()->getParentRegion());
    });
  }

  // /// Insert the dependencies
  // auto depSize = deps.size();
  // for (unsigned depIndex = 0; depIndex < depSize; depIndex++) {
  //   /// Build and i64 constant for 'i'
  //   auto i = builder.create<arith::ConstantIndexOp>(loc, depIndex);
  //   /// Create the memref load to load the value in the DepV array
  //   auto depVElem = builder.create<memref::LoadOp>(loc, depV, i);
  //   /// Get the ptr from the dep
  //   auto depPtr = getPtrFromEdtDep(depVElem, loc);
  //   /// Replace the uses of the dep with the ptr
  //   deps[depIndex].replaceUsesWithIf(depPtr, [&](OpOperand &operand) {
  //     return region.isAncestor(operand.getOwner()->getParentRegion());
  //   });
  // }

  /// Outline the region
  builder.setInsertionPointToStart(entryBlock);
  for (auto &op : llvm::make_early_inc_range(region.front())) {
    builder.clone(op);
    op.erase();
  }
}

/// Epoch
Value ArtsCodegen::createEpoch(Value finishEdtGuid, Value finishEdtSlot,
                               Location loc) {
  return createRuntimeCall(ARTSRTL_artsInitializeAndStartEpoch,
                           {finishEdtGuid, finishEdtSlot}, loc)
      .getResult(0);
}

/// Utils
Value ArtsCodegen::getGuidFromDb(Value db, Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetGuidFromDataBlock, {db}, loc)
      .getResult(0);
}

Value ArtsCodegen::getPtrFromDb(Value db, Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetPtrFromDataBlock, {db}, loc)
      .getResult(0);
}

Value ArtsCodegen::getGuidFromEdtDep(Value dep, Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetGuidFromEdtDep, {dep}, loc)
      .getResult(0);
}

Value ArtsCodegen::getPtrFromEdtDep(Value dep, Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetPtrFromEdtDep, {dep}, loc)
      .getResult(0);
}

Value ArtsCodegen::getCurrentEpochGuid(Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetCurrentEpochGuid, {}, loc)
      .getResult(0);
}

Value ArtsCodegen::getCurrentEdtGuid(Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetCurrentGuid, {}, loc).getResult(0);
}

Value ArtsCodegen::getCurrentNode(Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetCurrentNode, {}, loc).getResult(0);
}

Value ArtsCodegen::getNumDeps(SmallVector<Value> &deps, Location loc) {
  /// Initialize the constant in 0
  auto numDeps = createIntConstant(0, Int32, loc);
  for (auto dep : deps) {
    /// Verify the dep has already been processed
    auto *depDb = getDatablock(cast<MakeDepOp>(dep.getDefiningOp()));
    assert(depDb && "Datablock not found");
    /// Num of elements in the datablock
    auto numElements = castToInt(Int32, depDb->getNumElements(), loc);
    /// Add the number of elements in the datablock
    numDeps =
        builder.create<arith::AddIOp>(loc, numDeps, numElements).getResult();
  }
  return numDeps;
}

/// Helpers
Value ArtsCodegen::createParamV(SmallVector<Value> &parameters, Location loc) {
  /// Create array of i64
  unsigned int paramSize = parameters.size();
  auto paramVArray = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({paramSize}, Int64));
  for (unsigned i = 0; i < paramSize; i++) {
    /// If the value is not an i64, cast it
    auto param = castToInt(Int64, parameters[i], loc);
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

Value ArtsCodegen::createFunctionPtr(func::FuncOp funcOp, Location loc) {
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

Value ArtsCodegen::createIntConstant(unsigned value, Type type, Location loc) {
  return builder.create<arith::ConstantOp>(loc, type,
                                           builder.getIntegerAttr(type, value));
}

/// Casting
Value ArtsCodegen::castParameter(mlir::Type targetType, Value value, Location loc) {
  assert(targetType.isIntOrIndexOrFloat() && "Target type should be a number");
  if (value.getType().isIntOrIndexOrFloat())
    return value;
  /// If target type is an integer
  if (targetType.isa<IntegerType>())
    return castToInt(targetType, value, loc);
  /// If target type is an index
  if (targetType.isIndex())
    return castToIndex(value, loc);
  /// If target type is a float
  if (targetType.isa<FloatType>())
    return castToFloat(targetType, value, loc);
  return value;
}

Value ArtsCodegen::castToIndex(Value value, Location loc) {
  if (value.getType().isIndex())
    return value;
  /// If it is not an index, cast it to an index.
  auto indexType = IndexType::get(value.getContext());
  return builder.create<arith::IndexCastOp>(loc, indexType, value).getResult();
}

Value ArtsCodegen::castToFloat(mlir::Type targetType, Value value, Location loc) {
  assert(targetType.isa<FloatType>() && "Target type should be a float");
  if (value.getType().isa<FloatType>())
    return value;
  /// If it is not a float, cast it to a float.
  return builder.create<arith::SIToFPOp>(loc, targetType, value).getResult();
}

Value ArtsCodegen::castToInt(mlir::Type targetType, Value value, Location loc) {
  assert(targetType.isa<IntegerType>() && "Target type should be an integer");
  /// If it is already an i64, return it.
  if (value.getType() == targetType) 
    return value;
  /// If it is not an i64, cast it to i64.
  /// Handles index types, floats, and other integer types.
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
  return value;
}

Value ArtsCodegen::castToPtr(Value value, Location loc) {
  if (value.getType().isa<LLVM::LLVMPointerType>())
    return value;
  /// If it is not a pointer, cast it to a pointer.
  /// Polygeist - memref2pointer
  auto valPtr = builder.create<polygeist::Memref2PointerOp>(
      loc, LLVM::LLVMPointerType::get(value.getType()), value);
  /// pointer2memref
  return builder.create<polygeist::Pointer2MemrefOp>(loc, VoidPtr, valPtr);
}
