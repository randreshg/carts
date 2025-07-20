///==========================================================================
/// File: ArtsCodegen.cpp
///==========================================================================

#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Codegen/DbCodegen.h"
#include "arts/Codegen/EdtCodegen.h"

/// Other dialects
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Support/LLVM.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"
/// Other
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"
#include "mlir/Transforms/DialectConversion.h"
/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsTypes.h"
#include "arts/Utils/ArtsUtils.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cstdint>
#include <functional>

/// DEBUG
#define DEBUG_TYPE "arts-codegen"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
#define METADATA "-----------------------------------------\n[artsCodegen] "

using namespace mlir;
using namespace mlir::func;
using namespace mlir::LLVM;
using namespace mlir::arts;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::cf;

/// ARTS Codegen
ArtsCodegen::ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
                         mlir::DataLayout &mlirDL, bool debug)
    : module(module), builder(OpBuilder(module->getContext())), llvmDL(llvmDL),
      mlirDL(mlirDL), debug(debug) {
  initializeTypes();
  collectGlobalLLVMStrings();
}

ArtsCodegen::~ArtsCodegen() {
  for (auto &db : dbAllocs)
    delete db.second;
  for (auto &db : dbDeps)
    delete db.second;
  for (auto &edt : edts)
    delete edt.second;
}

func::FuncOp ArtsCodegen::getOrCreateRuntimeFunction(RuntimeFunction FnID) {
  auto cacheIt = runtimeFunctionCache.find(FnID);
  if (cacheIt != runtimeFunctionCache.end())
    return cacheIt->second;

  OpBuilder::InsertionGuard IG(builder);
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
    /// Create a new declaration if we need one.
    switch (FnID) {
#define ARTS_RTL(Enum, Str, ...)                                               \
  case Enum:                                                                   \
    funcOp = builder.create<func::FuncOp>(getUnknownLoc(), Str, fnType);       \
    break;
#include "arts/Codegen/ARTSKinds.def"
    }
  }

  /// Set the function as private and the llvm.linkage attribute to external
  funcOp.setPrivate();
  funcOp->setAttr(
      "llvm.linkage",
      LLVM::LinkageAttr::get(builder.getContext(), LLVM::Linkage::External));

  assert(funcOp && "Failed to create ARTS runtime function");

  /// Cache the function for future use
  runtimeFunctionCache[FnID] = funcOp;
  return funcOp;
}

void ArtsCodegen::initializeTypes() {
  MLIRContext *context = module.getContext();
  llvmPtr = LLVM::LLVMPointerType::get(context);
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
  func::FuncOp func = getOrCreateRuntimeFunction(FnID);
  assert(func && "Runtime function should exist");
  return builder.create<func::CallOp>(loc, func, args);
}

/// DataBlock
DbAllocCodegen *ArtsCodegen::getDbAlloc(Value op) {
  if (!op)
    return nullptr;
  auto it = dbAllocs.find(op);
  return (it != dbAllocs.end()) ? it->second : nullptr;
}

DbAllocCodegen *ArtsCodegen::getDbAlloc(DbAllocOp dbOp) {
  if (!dbOp)
    return nullptr;
  auto it = dbAllocs.find(dbOp.getResult());
  return (it != dbAllocs.end()) ? it->second : nullptr;
}

DbDepCodegen *ArtsCodegen::getDbDep(Value op) {
  if (!op)
    return nullptr;
  auto it = dbDeps.find(op);
  return (it != dbDeps.end()) ? it->second : nullptr;
}

DbDepCodegen *ArtsCodegen::getDbDep(DbDepOp dbOp) {
  if (!dbOp)
    return nullptr;
  auto it = dbDeps.find(dbOp.getResult());
  if (it != dbDeps.end())
    return it->second;
  return nullptr;
}

DbAllocCodegen *ArtsCodegen::createDbAlloc(DbAllocOp dbOp, Location loc) {
  assert(!getDbAlloc(dbOp) && "DbAlloc already exists");
  dbAllocs[dbOp.getResult()] = new DbAllocCodegen(*this, dbOp, loc);
  return dbAllocs[dbOp.getResult()];
}

DbDepCodegen *ArtsCodegen::createDbDep(DbDepOp dbOp, Operation *parentOp) {
  assert(!getDbDep(dbOp) && "DbDep already exists");
  dbDeps[dbOp.getResult()] = new DbDepCodegen(*this, dbOp, parentOp);
  return dbDeps[dbOp.getResult()];
}

DbAllocCodegen *ArtsCodegen::getOrCreateDbAlloc(DbAllocOp dbOp, Location loc) {
  if (auto existing = getDbAlloc(dbOp))
    return existing;
  return createDbAlloc(dbOp, loc);
}

DbDepCodegen *ArtsCodegen::getOrCreateDbDep(DbDepOp dbOp, Operation *parentOp) {
  if (auto existing = getDbDep(dbOp))
    return existing;
  return createDbDep(dbOp, parentOp);
}

void ArtsCodegen::addDbDep(Value dbGuid, Value edtGuid, Value edtSlot,
                           Location loc) {
  auto edtSlotInt = castToInt(Int32, edtSlot, loc);
  createRuntimeCall(ARTSRTL_artsDbAddDependence, {dbGuid, edtGuid, edtSlotInt},
                    loc);
}

void ArtsCodegen::incrementDbLatchCount(Value dbGuid, Location loc) {
  createRuntimeCall(ARTSRTL_artsDbIncrementLatch, {dbGuid}, loc);
}

void ArtsCodegen::decrementDbLatchCount(Value dbGuid, Location loc) {
  createRuntimeCall(ARTSRTL_artsDbDecrementLatch, {dbGuid}, loc);
}

/// EDT
EdtCodegen *ArtsCodegen::getEdt(Region *region) {
  auto it = edts.find(region);
  if (it != edts.end())
    return it->second;
  return nullptr;
}

EdtCodegen *ArtsCodegen::createEdt(SmallVector<Value> *opDeps, Region *region,
                                   Value *epoch, Location *loc, bool build) {
  if (!region || getEdt(region)) {
    LLVM_DEBUG(dbgs() << "Region already has an EDT\n");
    assert(false && "Edt already exists");
  }
  edts[region] = new EdtCodegen(*this, opDeps, region, epoch, loc, build);
  return edts[region];
}

/// Epoch
Value ArtsCodegen::createEpoch(Value finishEdtGuid, Value finishEdtSlot,
                               Location loc) {
  auto finishEdtSlotInt = castToInt(Int32, finishEdtSlot, loc);
  return createRuntimeCall(ARTSRTL_artsInitializeAndStartEpoch,
                           {finishEdtGuid, finishEdtSlotInt}, loc)
      .getResult(0);
}

/// Utils
Value ArtsCodegen::getGuidFromEdtDep(Value dep, Location loc) {
  const auto zeroInt = createIntConstant(0, Int32, loc);
  auto guidValue = builder.create<LLVM::GEPOp>(loc, llvmPtr, ArtsEdtDep, dep,
                                               ValueRange{zeroInt, zeroInt});
  return builder.create<LLVM::LoadOp>(loc, ArtsGuid, guidValue.getResult());
}

Value ArtsCodegen::getPtrFromEdtDep(Value dep, Location loc) {
  const auto zeroInt = createIntConstant(0, Int32, loc);
  const auto twoInt = createIntConstant(2, Int32, loc);
  auto gepOp = builder.create<LLVM::GEPOp>(loc, llvmPtr, ArtsEdtDep, dep,
                                           ValueRange{zeroInt, twoInt});
  return builder.create<LLVM::LoadOp>(loc, VoidPtr, gepOp.getResult());
}

Value ArtsCodegen::getCurrentEpochGuid(Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetCurrentEpochGuid, {}, loc)
      .getResult(0);
}

Value ArtsCodegen::getCurrentEdtGuid(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentGuid);
  return builder.create<func::CallOp>(loc, func, ArrayRef<Value>{})
      .getResult(0);
}

Value ArtsCodegen::getTotalWorkers(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetTotalWorkers);
  return builder.create<func::CallOp>(loc, func, ArrayRef<Value>{})
      .getResult(0);
}

Value ArtsCodegen::getTotalNodes(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetTotalNodes);
  return builder.create<func::CallOp>(loc, func, ArrayRef<Value>{})
      .getResult(0);
}

Value ArtsCodegen::getCurrentWorker(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentWorker);
  return builder.create<func::CallOp>(loc, func, ArrayRef<Value>{})
      .getResult(0);
}

Value ArtsCodegen::getCurrentNode(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentNode);
  return builder.create<func::CallOp>(loc, func, ArrayRef<Value>{})
      .getResult(0);
}

func::CallOp ArtsCodegen::signalEdt(Value edtGuid, Value edtSlot, Value dbGuid,
                                    Location loc) {
  auto edtSlotInt = castToInt(Int32, edtSlot, loc);
  return createRuntimeCall(ARTSRTL_artsSignalEdt, {edtGuid, edtSlotInt, dbGuid},
                           loc);
}

void ArtsCodegen::waitOnHandle(Value epochGuid, Location loc) {
  createRuntimeCall(ARTSRTL_artsWaitOnHandle, {epochGuid}, loc);
}

func::FuncOp ArtsCodegen::insertInitPerWorker(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);
  auto newFn =
      builder.create<func::FuncOp>(loc, "initPerWorker", InitPerWorkerFn);
  newFn.setPublic();
  module.push_back(newFn);
  auto *entryBlock = newFn.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);
  builder.create<func::ReturnOp>(loc);
  return newFn;
}

func::FuncOp ArtsCodegen::insertInitPerNode(Location loc,
                                            func::FuncOp callback) {
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);

  /// Create the function using the InitPerNodeFn type.
  auto newFn = builder.create<func::FuncOp>(loc, "initPerNode", InitPerNodeFn);
  newFn.setPublic();
  module.push_back(newFn);

  /// Create the entry block.
  Block *entryBlock = newFn.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Retrieve the first argument and compare arg with 1.
  auto arg = entryBlock->getArgument(0);
  auto cmp = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                           castToIndex(arg, loc),
                                           createIndexConstant(1, loc));

  /// Create separate then and merge blocks in the function body.
  auto thenBlock = new Block();
  auto mergeBlock = new Block();
  mergeBlock->addArgument(newFn.getArgument(1).getType(), loc);
  mergeBlock->addArgument(newFn.getArgument(2).getType(), loc);
  newFn.getBody().push_back(thenBlock);
  newFn.getBody().push_back(mergeBlock);

  /// In the entry block, do a conditional branch: if cmp true, jump to
  /// thenBlock; otherwise, continue in mergeBlock.
  builder.create<cf::CondBranchOp>(
      loc, cmp, thenBlock, ValueRange{}, mergeBlock,
      ValueRange{newFn.getArgument(1), newFn.getArgument(2)});

  /// thenBlock - If nodeId >= 1, return
  builder.setInsertionPointToStart(thenBlock);
  builder.create<func::ReturnOp>(loc);

  /// mergeBlock - Otherwise, call the callback function and return
  builder.setInsertionPointToStart(mergeBlock);
  if (callback) {
    /// If the callback function receives arguments, pass them to the call,
    /// otherwise, pass an empty ValueRange.
    auto callArgs = ValueRange{};
    if (callback.getNumArguments() > 0)
      callArgs = {newFn.getArgument(1), newFn.getArgument(2)};
    builder.create<func::CallOp>(loc, callback, callArgs);
  }
  createRuntimeCall(ARTSRTL_artsShutdown, {}, loc);
  builder.create<func::ReturnOp>(loc);
  return newFn;
}

func::FuncOp ArtsCodegen::insertArtsMainFn(Location loc,
                                           func::FuncOp callback) {
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);

  /// Create the function using the "MainEdt" type.
  auto newFn = builder.create<func::FuncOp>(loc, "artsMain", ArtsMainFn);
  newFn.setPublic();

  /// Create the entry block.
  auto *entryBlock = newFn.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Insert call to 'artsRT' function
  auto callArgs = ValueRange{};
  if (callback.getNumArguments() > 0)
    callArgs = {newFn.getArgument(0), newFn.getArgument(1)};
  builder.create<func::CallOp>(loc, callback, callArgs);

  /// Return
  createRuntimeCall(ARTSRTL_artsShutdown, {}, loc);
  builder.create<func::ReturnOp>(loc);
  return newFn;
}

func::FuncOp ArtsCodegen::insertMainFn(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);

  /// Create the function using the "MainFn" type.
  auto newFn = builder.create<func::FuncOp>(loc, "main", MainFn);
  newFn.setPublic();

  /// Create the entry block.
  auto *entryBlock = newFn.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Insert call to 'artsRT' function
  createRuntimeCall(ARTSRTL_artsRT,
                    {newFn.getArgument(0), newFn.getArgument(1)}, loc);

  /// Return 0
  builder.create<func::ReturnOp>(loc, createIntConstant(0, Int32, loc));
  return newFn;
}

void ArtsCodegen::initRT(Location loc) {
  auto mainFn = module.lookupSymbol<func::FuncOp>("main");
  if (!mainFn)
    return;

  /// Rename main function to "mainBody"
  mainFn.setName("mainBody");

  /// Insert init functions (artsMain and main)
  insertArtsMainFn(loc, mainFn);
  insertMainFn(loc);
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

Value ArtsCodegen::createIndexConstant(int value, Location loc) {
  return builder.create<arith::ConstantIndexOp>(loc, value);
}

Value ArtsCodegen::createIntConstant(int value, Type type, Location loc) {
  assert(type.isa<IntegerType>() && "Expected integer type");
  auto v = builder.create<arith::ConstantOp>(
      loc, type, builder.getIntegerAttr(type, value));
  return v;
}

Value ArtsCodegen::createPtr(Value source, Location loc) {
  if (source.getType().isa<LLVM::LLVMPointerType>())
    return source;
  /// If it is not a pointer, cast it to a pointer
  auto srcTy = source.getType().cast<MemRefType>();
  auto valPtr = builder.create<polygeist::Memref2PointerOp>(
      loc, LLVM::LLVMPointerType::get(srcTy), source);
  auto valTy = mlir::MemRefType::get(srcTy.getShape(), VoidPtr,
                                     srcTy.getLayout(), srcTy.getMemorySpace());
  return builder.create<polygeist::Pointer2MemrefOp>(loc, valTy, valPtr);
}

/// Casting
Value ArtsCodegen::castParameter(mlir::Type targetType, Value source,
                                 Location loc) {
  assert(targetType.isIntOrIndexOrFloat() && "Target type should be a number");
  /// If target type is an integer
  if (targetType.isa<IntegerType>())
    return castToInt(targetType, source, loc);
  /// If target type is an index
  if (targetType.isa<IndexType>())
    return castToIndex(source, loc);
  /// If target type is a float
  if (targetType.isa<FloatType>())
    return castToFloat(targetType, source, loc);
  return source;
}

Value ArtsCodegen::castPtr(mlir::Type targetType, Value source, Location loc) {
  auto srcType = source.getType().dyn_cast<MemRefType>();
  auto dstType = targetType.dyn_cast<MemRefType>();
  assert((srcType && dstType) && "Expected memref type");

  /// Cast if the types are compatible
  if (memref::CastOp::areCastCompatible(srcType, dstType))
    return builder.create<memref::CastOp>(loc, dstType, source);

  /// If the types are not compatible, cast to LLVM pointer and then to memref
  return builder.create<polygeist::Pointer2MemrefOp>(loc, targetType,
                                                     createPtr(source, loc));
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

Value ArtsCodegen::castToInt(Type targetType, Value source, Location loc) {
  assert(targetType.isa<IntegerType>() &&
         "Target type should be an integer (e.g. i64, i32, etc.)");

  /// If types match exactly, no cast is needed.
  if (source.getType() == targetType)
    return source;

  /// If source is index => use IndexCastOp to target integer type
  if (source.getType().isIndex())
    return builder.create<arith::IndexCastOp>(loc, targetType, source);

  /// If source is float => use FPToSIOp
  if (source.getType().isa<FloatType>())
    return builder.create<arith::FPToSIOp>(loc, targetType, source);

  /// If source is integer => handle extension or truncation
  if (auto srcIntType = source.getType().dyn_cast<IntegerType>()) {
    auto dstIntType = targetType.cast<IntegerType>();
    unsigned srcWidth = srcIntType.getWidth();
    unsigned dstWidth = dstIntType.getWidth();

    if (srcWidth == dstWidth)
      return source;
    else if (srcWidth < dstWidth)
      return builder.create<arith::ExtSIOp>(loc, targetType, source);
    return builder.create<arith::TruncIOp>(loc, targetType, source);
  }

  /// If none of the above matched => unsupported type
  llvm::errs() << "Unsupported type for casting to integer: " << source << "\n";
  assert(false && "Unsupported type in castToInt");
  return nullptr;
}

Value ArtsCodegen::castToVoidPtr(Value source, Location loc) {
  auto valPtr = source;
  if (!valPtr.getType().isa<LLVM::LLVMPointerType>()) {
    valPtr = castToLLVMPtr(source, loc);
  }
  return builder.create<polygeist::Pointer2MemrefOp>(loc, VoidPtr, valPtr);
}

Value ArtsCodegen::castToLLVMPtr(Value source, Location loc) {
  if (source.getType().isa<LLVM::LLVMPointerType>())
    return source;
  MemRefType MT = source.getType().cast<MemRefType>();
  return builder.create<polygeist::Memref2PointerOp>(
      loc,
      LLVM::LLVMPointerType::get(builder.getContext(),
                                 MT.getMemorySpaceAsInt()),
      source);
}

void ArtsCodegen::collectGlobalLLVMStrings() {
  for (auto &op : module.getOps()) {
    if (auto global = dyn_cast<LLVM::GlobalOp>(op)) {
      llvmStringGlobals[global.getName().str()] = global;
    }
  }
}

Value ArtsCodegen::getOrCreateGlobalLLVMString(Location loc, StringRef value) {
  /// Check if we already have this string cached. If so, return the cached
  /// global
  if (llvmStringGlobals.find(value.str()) == llvmStringGlobals.end()) {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(module.getBody());
    auto type = LLVM::LLVMArrayType::get(builder.getI8Type(), value.size() + 1);

    /// Create an unique name for the global
    std::string globalName = "str_" + std::to_string(llvmStringGlobals.size());
    llvmStringGlobals[value.str()] = builder.create<LLVM::GlobalOp>(
        loc, type, true, LLVM::Linkage::Internal, globalName,
        builder.getStringAttr(value.str() + '\0'));
  }

  LLVM::GlobalOp global = llvmStringGlobals[value.str()];
  return builder.create<LLVM::AddressOfOp>(loc, global);
}

void ArtsCodegen::createPrintfCall(Location loc, llvm::StringRef format,
                                   ValueRange args) {
  if (!debug)
    return;

  /// Create the printf function declaration if it doesn't exist
  auto printfFn = module.lookupSymbol<LLVM::LLVMFuncOp>("printf");
  if (!printfFn) {
    auto i32Type = builder.getI32Type();
    auto i8PtrType = LLVM::LLVMPointerType::get(builder.getContext());
    auto printfType = LLVM::LLVMFunctionType::get(i32Type, {i8PtrType}, true);

    printfFn = builder.create<LLVM::LLVMFuncOp>(loc, "printf", printfType);
    printfFn.setLinkage(LLVM::Linkage::External);
  }
  assert(printfFn && "printf function not found");

  /// Get or create the format string global and cast it to a generic pointer
  auto formatStrPtr = getOrCreateGlobalLLVMString(loc, format);
  auto castedFormatPtr =
      builder.create<LLVM::BitcastOp>(loc, llvmPtr, formatStrPtr);

  /// Create the printf call with all arguments
  SmallVector<Value> callArgs;
  callArgs.push_back(castedFormatPtr);
  callArgs.append(args.begin(), args.end());

  /// Create the call operation with a symbol reference
  builder.create<LLVM::CallOp>(
      loc, TypeRange{builder.getI32Type()},
      SymbolRefAttr::get(builder.getContext(), printfFn.getName()), callArgs);
}

/// Insertion point management
void ArtsCodegen::setInsertionPoint(Operation *op) {
  builder.setInsertionPoint(op);
}

void ArtsCodegen::setInsertionPointAfter(Operation *op) {
  builder.setInsertionPointAfter(op);
}

void ArtsCodegen::setInsertionPoint(ModuleOp &module) {
  builder.setInsertionPointToStart(module.getBody());
}

void ArtsCodegen::addReplacement(Value original, Value newValue,
                                 Region *region) {
  replacementMap[region ? region : &emptyRegion][original] = newValue;
}

void ArtsCodegen::applyReplacements(Region *region) {
  assert(region && "Region must be provided");
  replaceInRegion(*region, replacementMap[region]);
}

void ArtsCodegen::applyReplacements() {
  LLVM_DEBUG(DBGS() << "Applying " << replacementMap.size()
                    << " delayed replacements\n");

  for (auto &region : replacementMap) {
    if (region.first == &emptyRegion)
      replaceUses(region.second);
    else
      replaceInRegion(*region.first, region.second);
  }
  replacementMap.clear();
}