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

#include "llvm/ADT/SmallVector.h"
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

// ---------------------------- Datablocks ---------------------------- ///
DataBlockCodegen::DataBlockCodegen(ArtsCodegen &AC)
    : AC(AC), builder(AC.builder) {}

DataBlockCodegen::DataBlockCodegen(ArtsCodegen &AC, arts::MakeDepOp edtDep,
                                   Location loc)
    : AC(AC), builder(AC.builder) {
  create(edtDep, loc);
}

void DataBlockCodegen::create(arts::MakeDepOp edtDep, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  AC.setInsertionPoint(edtDep);

  bool isArray = false;

  /// If the value is a load, it corresponds to a memref of an array
  auto dbMemref = edtDep.getVal();
  if (auto loadOp = dyn_cast<memref::LoadOp>(dbMemref.getDefiningOp())) {
    dbMemref = loadOp.getMemref();
  } else {
    isArray = true;
  }

  /// Get the memref type
  auto memrefType = dbMemref.getType().dyn_cast<MemRefType>();
  assert(memrefType && "Expected MemRefType");

  /// Get the size of the datablock
  /// We assume in this point that the memref is an array
  Value numElements = builder.create<arith::ConstantIndexOp>(loc, 1);
  for (int64_t i = 0, rank = memrefType.getRank(); i < rank; ++i) {
    Value dimSize =
        memrefType.isDynamicDim(i)
            ? builder.create<memref::DimOp>(loc, dbMemref, i)
            : builder
                  .create<arith::ConstantIndexOp>(loc, memrefType.getDimSize(i))
                  .getResult();
    numElements = builder.create<arith::MulIOp>(loc, numElements, dimSize);
  }

  /// Create a datablock or an array of datablocks based on the size
  if (numElements == builder.create<arith::ConstantIndexOp>(loc, 1)) {
    /// Report error - Not supported
    /// TODO: Implement this case - This is for pointers
    assert(0 && "Not implemented yet");
  }

  /// Allocate an array of Datablocks
  auto dbArray =
      builder.create<memref::AllocaOp>(loc, AC.ArtsDbPtr, numElements);
  auto dbSize = AC.createIntConstant(
      AC.mlirDL.getTypeSize(memrefType.getElementType()), AC.Int64, loc);
  auto dbMode = getMode(edtDep.getMode());

  /// Create the datablock
  AC.createRuntimeCall(ARTSRTL_artsDbCreateArray,
                       {dbArray, dbSize, dbMode,
                        AC.castToInt(AC.Int32, numElements, loc),
                        AC.castToPtr(dbMemref, loc)},
                       loc);

  /// Set the values
  db = dbArray;
  this->numElements = numElements;
  this->isArray = isArray;
}

Value DataBlockCodegen::getMode(llvm::StringRef mode) {
  auto Loc = UnknownLoc::get(builder.getContext());
  unsigned enumValue = 9;
  /// ARTS_DB_READ
  if (mode == "in")
    enumValue = 7;
  /// ARTS_DB_PIN
  else if (mode == "out")
    enumValue = 9;
  return AC.createIntConstant(enumValue, AC.Int32, Loc);
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
  datablocks[edtDep] = new DataBlockCodegen(*this, edtDep, loc);
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

// ---------------------------- EDTs ---------------------------- ///
unsigned EdtCodegen::edtCounter = 0;

EdtCodegen::EdtCodegen(ArtsCodegen &AC) : AC(AC), builder(AC.builder) {}

Value EdtCodegen::createGuid(Value node, Location loc) {
  auto guidEdtType = AC.createIntConstant(1, AC.artsType_t, loc);
  auto reserveGuidCall =
      AC.createRuntimeCall(ARTSRTL_artsReserveGuidRoute, {guidEdtType, node}, loc);
  return reserveGuidCall.getResult(0);
}

func::FuncOp EdtCodegen::createFn(Location loc) {
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, AC.EdtFn);
  edtFuncOp.setPrivate();
  AC.module.push_back(edtFuncOp);
  return edtFuncOp;
}

func::CallOp EdtCodegen::createCallWithGuid(func::FuncOp edtFunc, Value guid,
                                            SmallVector<Value> params,
                                            Value depC, Value node,
                                            Location loc) {
  auto paramC = AC.createIntConstant(params.size(), AC.Int32, loc);
  auto paramV = createParamV(params, loc);
  auto edtFuncPtr = AC.createFnPtr(edtFunc, loc);
  return AC.createRuntimeCall(ARTSRTL_artsEdtCreateWithGuid,
                              {edtFuncPtr, guid, paramC, paramV, depC}, loc);
}

Value EdtCodegen::create(func::FuncOp edtFunc, SmallVector<Value> params,
                         Value depC, Value node, Location loc) {
  auto paramC = AC.createIntConstant(params.size(), AC.Int32, loc);
  auto paramV = createParamV(params, loc);
  auto edtFuncPtr = AC.createFnPtr(edtFunc, loc);
  return AC
      .createRuntimeCall(ARTSRTL_artsEdtCreate,
                         {edtFuncPtr, node, paramC, paramV, depC}, loc)
      .getResult(0);
}

Value EdtCodegen::createWithEpoch(func::FuncOp edtFunc,
                                  SmallVector<Value> params, Value depC,
                                  Value node, Value epoch, Location loc) {
  auto paramC = AC.createIntConstant(params.size(), AC.Int32, loc);
  auto paramV = createParamV(params, loc);
  auto edtFuncPtr = AC.createFnPtr(edtFunc, loc);
  return AC
      .createRuntimeCall(types::RuntimeFunction::ARTSRTL_artsEdtCreateWithEpoch,
                         {edtFuncPtr, node, paramC, paramV, depC, epoch}, loc)
      .getResult(0);
}

void EdtCodegen::insertFnEntry(func::FuncOp func, Region &region,
                               SmallVector<Value> &params,
                               SmallVector<Value> &consts,
                               SmallVector<Value> &deps,
                               DenseMap<Value, Value> &rewireMap) {
  OpBuilder::InsertionGuard guard(builder);

  auto *entryBlock = func.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Get reference to the block arguments
  Value paramC = entryBlock->getArgument(0);
  Value paramV = entryBlock->getArgument(1);
  Value depC = entryBlock->getArgument(2);
  Value depV = entryBlock->getArgument(3);

  auto loc = func.getLoc();
  /// Clone constants
  for (auto oldConst : consts) {
    auto newConst = builder.clone(*oldConst.getDefiningOp())->getResult(0);
    rewireMap[oldConst] = newConst;
  }

  /// Insert the parameters
  auto paramSize = params.size();
  for (unsigned paramIndex = 0; paramIndex < paramSize; paramIndex++) {
    auto i = builder.create<arith::ConstantIndexOp>(loc, paramIndex);
    auto paramVElem =
        builder.create<memref::LoadOp>(loc, paramV, ValueRange{i});
    /// Cast it back to the original type
    auto oldParam = params[paramIndex];
    auto newParam = AC.castParameter(oldParam.getType(), paramVElem, loc);
    rewireMap[oldParam] = newParam;
  }

  /// Insert the dependencies
  auto depSize = deps.size();
  auto initialIndex = AC.createIntConstant(0, AC.Int32, loc);
  for (unsigned depIndex = 0; depIndex < depSize; depIndex++) {
    /// Get DataBlock
    auto edtDep = cast<MakeDepOp>(deps[depIndex].getDefiningOp());
    auto &db = *AC.getDatablock(edtDep);
    /// If the datblock is an array,
    /// Get dependency ptr
    auto i = builder.create<arith::ConstantIndexOp>(loc, depIndex);
    auto depVElem = builder.create<memref::LoadOp>(loc, depV, ValueRange({i}));
    auto depPtr = AC.getPtrFromEdtDep(depVElem, loc);
    /// Cast it back to the original type
    auto oldDep = edtDep.getVal();
    if (auto loadOp = dyn_cast<memref::LoadOp>(oldDep.getDefiningOp()))
      oldDep = loadOp.getMemref();
    /// Cast it back to the original type
    auto newDep = AC.castDependency(oldDep.getType(), depPtr, loc);
    rewireMap[oldDep] = depPtr;
  }

  /// Rewire the values
  for (auto &rewire : rewireMap) {
    rewire.first.replaceUsesWithIf(rewire.second, [&](OpOperand &operand) {
      return region.isAncestor(operand.getOwner()->getParentRegion());
    });
  }
}

Value EdtCodegen::createParamV(SmallVector<Value> &params, Location loc) {
  /// Create array of i64
  unsigned int paramSize = params.size();
  auto paramVArray = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({paramSize}, AC.Int64));
  for (unsigned i = 0; i < paramSize; i++) {
    /// If the value is not an i64, cast it
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    /// Affine store
    auto paramIndex = builder.create<arith::ConstantIndexOp>(loc, i);
    builder.create<affine::AffineStoreOp>(loc, param, paramVArray,
                                          ValueRange{paramIndex});
  }
  /// memref cast to memref<-1xi64>
  auto paramV = builder.create<memref::CastOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64), paramVArray);
  return paramV.getResult();
}

// ---------------------------- ARTS Codegen ---------------------------- ///
ArtsCodegen::ArtsCodegen(ModuleOp &module, OpBuilder &builder,
                         llvm::DataLayout &llvmDL, mlir::DataLayout &mlirDL)
    : module(module), builder(builder), llvmDL(llvmDL), mlirDL(mlirDL) {}

ArtsCodegen::~ArtsCodegen() {}
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

void ArtsCodegen::initializeTypes() {
  MLIRContext *context = module.getContext();
#define ARTS_TYPE(VarName, InitValue) VarName = InitValue;
#define ARTS_FUNCTION_TYPE(VarName, ReturnType, ...)                           \
  VarName = FunctionType::get(                                                 \
      context, {__VA_ARGS__},                                                  \
      ((ReturnType == Void) ? ArrayRef<Type>{} : ArrayRef<Type>{ReturnType})); \
  VarName##Ptr = MemRefType::get(                                              \
      {ShapedType::kDynamic},                                                  \
      LLVM::LLVMFunctionType::get(LLVM::LLVMVoidType::get(context),            \
                                  VarName.getInputs(), false));
#define ARTS_STRUCT_TYPE(VarName, StructName, Packed, ...)                     \
  VarName = LLVM::LLVMStructType::getLiteral(context, {__VA_ARGS__}, Packed);  \
  VarName##Ptr = MemRefType::get({ShapedType::kDynamic}, VarName);
#include "arts/Codegen/ARTSKinds.def"
}

func::CallOp ArtsCodegen::createRuntimeCall(RuntimeFunction FnID,
                                            ArrayRef<Value> args,
                                            Location loc) {
  /// Get or create the runtime function
  func::FuncOp func = getOrCreateRuntimeFunction(FnID);
  assert(func && "Runtime function should exist");
  /// Create the call operation
  func::CallOp call = builder.create<func::CallOp>(loc, func, args);
  return call;
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
Value ArtsCodegen::createFnPtr(func::FuncOp funcOp, Location loc) {
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
Value ArtsCodegen::castParameter(mlir::Type targetType, Value source,
                                 Location loc) {
  assert(targetType.isIntOrIndexOrFloat() && "Target type should be a number");
  if (source.getType().isIntOrIndexOrFloat())
    return source;
  /// If target type is an integer
  if (targetType.isa<IntegerType>())
    return castToInt(targetType, source, loc);
  /// If target type is an index
  if (targetType.isIndex())
    return castToIndex(source, loc);
  /// If target type is a float
  if (targetType.isa<FloatType>())
    return castToFloat(targetType, source, loc);
  return source;
}

Value ArtsCodegen::castDependency(mlir::Type targetType, Value source,
                                  Location loc) {
  /// Ensure the source is a memref
  auto sourceType = source.getType().dyn_cast<mlir::MemRefType>();
  assert(sourceType && "Source must be a memref type");

  /// Ensure the target type is also a memref
  auto targetMemRefType = targetType.dyn_cast<mlir::MemRefType>();
  assert(targetMemRefType && "Target type must be a memref type");

  // Check compatibility of the memref cast
  if (!mlir::memref::CastOp::areCastCompatible(sourceType, targetMemRefType)) {
    llvm::errs() << "Cannot cast memref of type " << sourceType << " to type "
                 << targetMemRefType << "\n";
    return nullptr;
  }

  /// Create the memref.cast operation
  return builder.create<memref::CastOp>(loc, targetMemRefType, source);
}

Value ArtsCodegen::castToIndex(Value source, Location loc) {
  if (source.getType().isIndex())
    return source;
  /// If it is not an index, cast it to an index.
  auto indexType = IndexType::get(source.getContext());
  return builder.create<arith::IndexCastOp>(loc, indexType, source).getResult();
}

Value ArtsCodegen::castToFloat(mlir::Type targetType, Value source,
                               Location loc) {
  assert(targetType.isa<FloatType>() && "Target type should be a float");
  if (source.getType().isa<FloatType>())
    return source;
  /// If it is not a float, cast it to a float.
  return builder.create<arith::SIToFPOp>(loc, targetType, source).getResult();
}

Value ArtsCodegen::castToInt(mlir::Type targetType, Value source,
                             Location loc) {
  assert(targetType.isa<IntegerType>() && "Target type should be an integer");
  /// If it is already an i64, return it.
  if (source.getType() == targetType)
    return source;
  /// If it is not an i64, cast it to i64.
  /// Handles index types, floats, and other integer types.
  if (source.getType().isIndex()) {
    // Handle index type conversion to i64
    source =
        builder.create<arith::IndexCastOp>(loc, targetType, source).getResult();
  } else if (source.getType().isa<mlir::FloatType>()) {
    // Handle float type conversion to i64
    source =
        builder.create<arith::FPToSIOp>(loc, targetType, source).getResult();
  } else if (auto intType = source.getType().dyn_cast<mlir::IntegerType>()) {
    // Handle other integer types (e.g., i8, i16, i32)
    if (intType.isSignless()) {
      source =
          builder.create<arith::ExtSIOp>(loc, targetType, source).getResult();
    } else {
      source =
          builder.create<arith::ExtUIOp>(loc, targetType, source).getResult();
    }
  } else {
    // Unsupported type, emit an error or assert
    llvm::errs() << "Unsupported type for casting to i64: " << source.getType()
                 << "\n";
    assert(false && "Unsupported type");
  }
  return source;
}

Value ArtsCodegen::castToPtr(Value source, Location loc) {
  if (source.getType().isa<LLVM::LLVMPointerType>())
    return source;
  /// If it is not a pointer, cast it to a pointer.
  /// Polygeist - memref2pointer
  auto valPtr = builder.create<polygeist::Memref2PointerOp>(
      loc, LLVM::LLVMPointerType::get(source.getType()), source);
  /// pointer2memref
  return builder.create<polygeist::Pointer2MemrefOp>(loc, VoidPtr, valPtr);
}