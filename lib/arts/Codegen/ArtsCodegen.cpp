//===- ArtsCodegen.cpp - Builder for LLVM-IR for ARTS directives ----===//
//===----------------------------------------------------------------------===//
/// This file implements the ArtsCodegen class, which is used as a
/// convenient way to create LLVM instructions for ARTS directives.
///
//===----------------------------------------------------------------------===//

/// Other dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"
/// Other
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/TypeUtilities.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"
#include "mlir/Transforms/DialectConversion.h"
/// Arts
#include "arts/Analysis/EdtAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsTypes.h"
#include "arts/Utils/ArtsUtils.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cstdint>
#include <utility>

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

DataBlockCodegen::DataBlockCodegen(ArtsCodegen &AC, arts::DataBlockOp dbOp,
                                   Location loc)
    : AC(AC), builder(AC.builder) {
  create(dbOp, loc);
}

void DataBlockCodegen::create(arts::DataBlockOp depOp, Location loc) {
  /// Datablock info
  dbOp = depOp;
  memref = dbOp.getBase();
  baseIsDb = dbOp.isBaseDb();
  elementType = dbOp.getElementType();
  elementTypeSize = dbOp.getElementTypeSize();

  /// Lambda function to replace the number of elements
  auto replaceNumElements = [&](Value numElems) {
    for (auto &use : dbOp->getUses()) {
      if (auto dbSizeOp = dyn_cast<arts::DataBlockSizeOp>(use.getOwner())) {
        dbSizeOp.getResult().replaceAllUsesWith(numElems);
        dbSizeOp.erase();
      }
    }
  };

  /// Set insertion point to the DatablockOp
  OpBuilder::InsertionGuard guard(builder);
  AC.setInsertionPoint(dbOp);

  /// Initialize numElements to 1
  numElements = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto currentNode = AC.getCurrentNode(loc);

  bool isSingleSize = false;
  /// If there is a single size, we value equal to '1', it is a single element
  if (dbOp.getSizes().size() == 1) {
    auto size = dbOp.getSizes().front();
    if (auto cstOp = size.getDefiningOp<arith::ConstantIndexOp>()) {
      if (cstOp.value() == 1)
        isSingleSize = true;
    }
  }

  /// Handle the case of a single datablock
  if (dbOp.isLoad() || isSingleSize) {
    replaceNumElements(numElements);

    /// If the base is a DB, load the value
    if (baseIsDb) {
      DataBlockCodegen *parentDb =
          AC.getDatablock(dyn_cast<arts::DataBlockOp>(memref.getDefiningOp()));
      assert(parentDb && "Parent datablock not found");
      guid = builder
                 .create<memref::LoadOp>(loc, parentDb->getGuid(),
                                         dbOp.getOffsets())
                 ->getResult(0);
      ptr = builder
                .create<memref::LoadOp>(loc, parentDb->getPtr(),
                                        dbOp.getOffsets())
                ->getResult(0);
      return;
    }

    /// If not, create a new datablock
    auto modeVal = getMode(dbOp.getMode());
    guid = createGuid(currentNode, modeVal, loc);
    ptr =
        AC.createRuntimeCall(
              ARTSRTL_artsDbCreateWithGuidAndData,
              {AC.castToPtr(memref, loc), elementTypeSize, modeVal, guid}, loc)
            ->getResult(0);

    return;
  }

  /// If we hit this point, we are handling an array of datablocks
  dbIsArray = true;

  /// Multiply the sizes to get numElements.
  auto sizes = dbOp.getSizes();
  for (Value sz : sizes)
    numElements = builder.create<arith::MulIOp>(loc, numElements, sz);
  replaceNumElements(numElements);

  /// Create an array of guids and pointers based on the sizes of the dbOp
  auto modeVal = getMode(dbOp.getMode());
  auto guidType = MemRefType::get(
      std::vector<int64_t>(sizes.size(), ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, sizes);
  LLVM_DEBUG(dbgs() << "Created array of guids: " << guid << "\n");
  auto ptrType = MemRefType::get(
      std::vector<int64_t>(sizes.size(), ShapedType::kDynamic), AC.VoidPtr);
  ptr = builder.create<memref::AllocaOp>(loc, ptrType, sizes);
  LLVM_DEBUG(dbgs() << "Created array of datablocks\n");
  /// Recursively create nested loops for each dimension
  std::function<void(unsigned, SmallVector<Value, 4> &)> createNestedLoops =
      [&](unsigned dim, SmallVector<Value, 4> &indices) {
        if (dim == sizes.size()) {
          auto guidVal = createGuid(currentNode, modeVal, loc);
          auto tySize = AC.castToInt(AC.Int64, elementTypeSize, loc);
          auto ptrVal =
              AC.createRuntimeCall(ARTSRTL_artsDbCreateWithGuid,
                                   {guidVal, tySize}, loc)
                  ->getResult(0);
          builder.create<memref::StoreOp>(loc, guidVal, guid, indices);
          builder.create<memref::StoreOp>(loc, ptrVal, ptr, indices);
          return;
        }
        /// Create loop for current dimension
        auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
        auto upper = sizes[dim];
        auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
        auto loopOp = builder.create<scf::ForOp>(loc, lower, upper, step);
        /// Set insertion point inside the loop body
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        /// Append the induction variable for the current dimension
        indices.push_back(loopOp.getInductionVar());
        /// Recurse to create the next level of loop
        createNestedLoops(dim + 1, indices);
        /// Remove the current induction variable and reset insertion point
        indices.pop_back();
        builder.setInsertionPointAfter(loopOp);
      };

  SmallVector<Value, 4> indices;
  createNestedLoops(0, indices);
}

Value DataBlockCodegen::getMode(llvm::StringRef mode) {
  auto Loc = UnknownLoc::get(builder.getContext());
  auto enumValue = 9;
  /// ARTS_DB_READ
  if (mode == "in")
    enumValue = 7;
  /// ARTS_DB_PIN
  else if (mode == "out")
    enumValue = 9;
  return AC.createIntConstant(enumValue, AC.Int32, Loc);
}

Value DataBlockCodegen::createGuid(Value node, Value mode, Location loc) {
  auto reserveGuidCall =
      AC.createRuntimeCall(ARTSRTL_artsReserveGuidRoute, {mode, node}, loc);
  return reserveGuidCall.getResult(0);
}

// ---------------------------- EDTs ---------------------------- ///
unsigned EdtCodegen::edtCounter = 0;

EdtCodegen::EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps,
                       Region *region, Value *epoch, Location *loc, bool build)
    : AC(AC), builder(AC.builder), region(region), epoch(epoch) {
  OpBuilder::InsertionGuard guard(builder);
  auto curLoc = loc ? *loc : UnknownLoc::get(builder.getContext());

  func = createFn(curLoc);
  node = AC.getCurrentNode(curLoc);

  /// Get opParams and opConsts
  if (region) {
    ConversionPatternRewriter rewriter(builder.getContext());
    EdtEnvManager envManager(rewriter, *region);
    envManager.naiveCollection(true);
    envManager.print();
    params.append(envManager.getParameters().begin(),
                  envManager.getParameters().end());
    consts.append(envManager.getConstants().begin(),
                  envManager.getConstants().end());
  }
  processDepsAndParams(opDeps, curLoc);

  if (build)
    this->build(curLoc);
}

void EdtCodegen::build(Location loc) {
  /// If not epoch is provided, create an EDT without it
  auto funcPtr = AC.createFnPtr(func, loc);
  if (!epoch) {
    guid = AC.createRuntimeCall(ARTSRTL_artsEdtCreate,
                                {funcPtr, node, paramC, paramV, depC}, loc)
               .getResult(0);
  } else {
    guid = AC.createRuntimeCall(
                 types::RuntimeFunction::ARTSRTL_artsEdtCreateWithEpoch,
                 {funcPtr, node, paramC, paramV, depC, *epoch}, loc)
               .getResult(0);
  }

  /// If no region is provided, we are done
  if (!region)
    return;

  /// Create the rewriter
  ConversionPatternRewriter rewriter(builder.getContext());

  /// Create the entry block
  createEntry(loc);
  Region &funcRegion = func.getBody();
  Block &funcEntryBlock = funcRegion.front();
  Block &entryBlock = region->front();
  rewriter.inlineRegionBefore(*region, funcRegion, funcRegion.end());

  /// Move all ops from srcBlock to the end of destBlock
  while (!entryBlock.empty()) {
    Operation &op = entryBlock.front();
    op.moveBefore(&funcEntryBlock, funcEntryBlock.end());
  }
  entryBlock.erase();

  /// Iterate over all blocks of the region and replace the arts.yield with a
  /// return
  for (auto &block : funcRegion) {
    if (auto yieldOp = dyn_cast<arts::YieldOp>(block.getTerminator())) {
      AC.setInsertionPoint(yieldOp);
      builder.create<func::ReturnOp>(loc);
      yieldOp->erase();
    }
  }
}

void EdtCodegen::processDepsAndParams(SmallVector<Value> *opDeps,
                                      Location loc) {
  if (opDeps) {
    deps = *opDeps;
    /// Creates a DataBlock for each dependency
    SmallVector<DataBlockCodegen *> dbs;
    for (auto dep : *opDeps) {
      auto dbOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp());
      assert(dbOp && "Dependency is not a datablock op");
      auto dbCodegen = AC.getDatablock(dbOp);
      assert(dbCodegen && "Datablock not found");
      dbs.push_back(dbCodegen);
    }

    /// Compute depC and update parameters
    depC = AC.createIndexConstant(0, loc);
    for (auto db : dbs) {
      /// If the db is an array, add the number of elements as a parameter
      if (db->isArray())
        dbSizeMap[db] = insertParam(db->getNumElements());
      /// Add the number of elements in the datablock
      depC = builder.create<arith::AddIOp>(loc, depC, db->getNumElements())
                 .getResult();
    }
    depC = AC.castToInt(AC.Int32, depC, loc);
  }

  /// Insert the parameters
  unsigned int paramSize = params.size();
  paramC = AC.createIntConstant(paramSize, AC.Int32, loc);
  auto paramVArray = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({paramSize}, AC.Int64));
  for (unsigned i = 0; i < paramSize; i++) {
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    auto paramIndex = AC.createIndexConstant(i, loc);
    builder.create<affine::AffineStoreOp>(loc, param, paramVArray,
                                          ValueRange{paramIndex});
  }

  /// memref cast to memref<-1xi64>
  paramV = builder.create<memref::CastOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64), paramVArray);
}

void EdtCodegen::createEntry(Location loc) {
  OpBuilder::InsertionGuard guard(builder);

  auto *entryBlock = func.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  auto rewireMap = DenseMap<Value, Value>();

  /// Get reference to the block arguments
  Value fnParamV = entryBlock->getArgument(1);
  Value fnDepV = entryBlock->getArgument(3);

  /// Clone constants
  for (auto oldConst : consts) {
    rewireMap[oldConst] =
        builder.clone(*oldConst.getDefiningOp())->getResult(0);
  }

  /// Insert the parameters
  auto paramSize = params.size();
  for (unsigned paramIndex = 0; paramIndex < paramSize; paramIndex++) {
    auto i = builder.create<arith::ConstantIndexOp>(loc, paramIndex);
    auto paramVElem =
        builder.create<memref::LoadOp>(loc, fnParamV, ValueRange{i});
    /// Cast it back to the original type
    auto oldParam = params[paramIndex];
    auto newParam = AC.castParameter(oldParam.getType(), paramVElem, loc);
    rewireMap[oldParam] = newParam;
  }

  /// Create a lambda function that given an index, return the new parameter
  /// from the rewiremap
  auto getNewParam = [&](unsigned index) {
    auto oldParam = params[index];
    return rewireMap[oldParam];
  };

  /// Insert the dependencies
  Value index = builder.create<arith::ConstantIndexOp>(loc, 0);
  Value oneIdx = builder.create<arith::ConstantIndexOp>(loc, 1);
  SetVector<Value> ignoreSet;
  for (auto &dep : deps) {
    /// Dont replace uses in datablock ops
    ignoreSet.insert(dep);

    /// Get DataBlock
    auto oldDep = cast<DataBlockOp>(dep.getDefiningOp());
    auto *db = AC.getDatablock(oldDep);
    assert(db && "Datablock not found");

    /// If the db is an array, create an array of Datablocks from the deps
    if (db->isArray()) {
      auto numElements = getNewParam(dbSizeMap[db]);
      auto elementTy = db->getElementType();
      auto [dbPtrArray, dbGuidArray] = AC.CreatePtrAndGuidArrayFromDeps(
          numElements, elementTy, fnDepV, index, loc);
      db->setEntryInfo(dbGuidArray, dbPtrArray);
      rewireMap[oldDep] = dbPtrArray;
      /// Increment the index
      index =
          builder.create<arith::AddIOp>(loc, index, numElements).getResult();
      continue;
    }

    /// If the db is a single db, get the dependency ptr
    auto depVElem =
        builder.create<memref::LoadOp>(loc, fnDepV, ValueRange({index}));
    auto entryGuid = AC.getGuidFromEdtDep(depVElem, loc);
    auto entryMemref = AC.getPtrFromEdtDep(depVElem, loc);
    db->setEntryInfo(entryGuid, entryMemref);
    /// Increment the index
    index = builder.create<arith::AddIOp>(loc, index, oneIdx).getResult();
    rewireMap[oldDep] = entryMemref;
  }

  replaceInRegion(*region, rewireMap, ignoreSet);
}

Value EdtCodegen::createGuid(Value node, Location loc) {
  auto guidEdtType = AC.createIntConstant(1, AC.ArtsType, loc);
  auto reserveGuidCall = AC.createRuntimeCall(ARTSRTL_artsReserveGuidRoute,
                                              {guidEdtType, node}, loc);
  return reserveGuidCall.getResult(0);
}

func::FuncOp EdtCodegen::createFn(Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  AC.setInsertionPoint(AC.module);
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, AC.EdtFn);
  edtFuncOp.setPrivate();
  AC.module.push_back(edtFuncOp);
  return edtFuncOp;
}

// ---------------------------- ARTS Codegen ---------------------------- ///
ArtsCodegen::ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
                         mlir::DataLayout &mlirDL)
    : module(module), builder(OpBuilder(module->getContext())), llvmDL(llvmDL),
      mlirDL(mlirDL) {
  initializeTypes();
}

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
  llvmPtr = LLVM::LLVMPointerType::get(context);
}

func::CallOp ArtsCodegen::createRuntimeCall(RuntimeFunction FnID,
                                            ArrayRef<Value> args,
                                            Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(FnID);
  assert(func && "Runtime function should exist");
  return builder.create<func::CallOp>(loc, func, args);
}

/// DataBlock
DataBlockCodegen *ArtsCodegen::getDatablock(arts::DataBlockOp dbOp) {
  if (!dbOp)
    return nullptr;
  auto it = datablocks.find(dbOp);
  return (it != datablocks.end()) ? it->second : nullptr;
}

DataBlockCodegen *ArtsCodegen::createDatablock(arts::DataBlockOp dbOp,
                                               Location loc) {
  auto &db = datablocks[dbOp];
  if (!db)
    db = new DataBlockCodegen(*this, dbOp, loc);
  return db;
}

DataBlockCodegen *ArtsCodegen::getOrCreateDatablock(arts::DataBlockOp dbOp,
                                                    Location loc) {
  if (auto db = getDatablock(dbOp))
    return db;
  return createDatablock(dbOp, loc);
}

/// EDT
EdtCodegen *ArtsCodegen::getEdt(Region *region) {
  /// Try to find in the map
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
  return createRuntimeCall(ARTSRTL_artsInitializeAndStartEpoch,
                           {finishEdtGuid, finishEdtSlot}, loc)
      .getResult(0);
}

/// Utils
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
  /// Add 'pure' attribute to the call
  auto callOp = createRuntimeCall(ARTSRTL_artsGetCurrentNode, {}, loc);
  // callOp->setAttr("llvm.sideeffect", builder.getUnitAttr());
  return callOp.getResult(0);
}

Value ArtsCodegen::getNumDeps(SmallVector<Value> &deps, Location loc) {
  auto numDeps = createIntConstant(0, Int32, loc);
  for (auto dep : deps) {
    /// Verify the dep has already been processed
    auto *depDb = getDatablock(cast<DataBlockOp>(dep.getDefiningOp()));
    assert(depDb && "Datablock not found");
    /// Add the number of elements in the datablock
    auto numElements = castToInt(Int32, depDb->getNumElements(), loc);
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
  /// If it is not a pointer, cast it to a pointer.
  /// Polygeist - memref2pointer
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

Value ArtsCodegen::castPointer(mlir::Type targetType, Value source,
                               Location loc) {
  auto srcType = source.getType().dyn_cast<MemRefType>();
  auto dstType = targetType.dyn_cast<MemRefType>();
  assert((srcType && dstType) && "Expected memref type");

  /// Cast if the types are compatible
  if (memref::CastOp::areCastCompatible(srcType, dstType))
    return builder.create<memref::CastOp>(loc, dstType, source);

  /// Otherwise, cast with memref2pointer and pointer2memref
  auto valPtr = builder.create<polygeist::Memref2PointerOp>(
      loc, LLVM::LLVMPointerType::get(srcType), source);
  return builder.create<polygeist::Pointer2MemrefOp>(loc, targetType, valPtr);
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

/// Cast a value to an integer type.
Value ArtsCodegen::castToInt(Type targetType, Value source, Location loc) {
  assert(targetType.isa<IntegerType>() &&
         "Target type should be an integer (e.g. i64, i32, etc.)");

  /// If types match exactly, no cast is needed.
  if (source.getType() == targetType)
    return source;

  /// If source is index => use IndexCastOp to target integer type
  if (source.getType().isIndex()) {
    return builder.create<arith::IndexCastOp>(loc, targetType, source);
  }

  /// If source is float => use FPToSIOp (assuming signed)
  if (source.getType().isa<FloatType>()) {
    return builder.create<arith::FPToSIOp>(loc, targetType, source);
  }

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
  llvm::errs() << "Unsupported type for casting to integer: "
               << source.getType() << "\n";
  assert(false && "Unsupported type in castToInt");
  return nullptr; /// unreachable
}

Value ArtsCodegen::castToPtr(Value source, Location loc) {
  auto valPtr = source;
  if (!valPtr.getType().isa<LLVM::LLVMPointerType>()) {
    auto memrefTy = source.getType().cast<MemRefType>();
    valPtr = castToLLVMPtr(source, memrefTy, loc);
  }
  /// polygeist - pointer2memref
  return builder.create<polygeist::Pointer2MemrefOp>(loc, VoidPtr, valPtr);
}

Value ArtsCodegen::castToLLVMPtr(Value source, MemRefType MT, Location loc) {
  if (source.getType().isa<LLVM::LLVMPointerType>())
    return source;
  /// polygeist - memref2pointer
  return builder.create<polygeist::Memref2PointerOp>(
      loc,
      LLVM::LLVMPointerType::get(builder.getContext(),
                                 MT.getMemorySpaceAsInt()),
      source);
}