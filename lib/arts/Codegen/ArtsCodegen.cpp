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
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsTypes.h"
/// Debug
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
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

void DataBlockCodegen::create(arts::DataBlockOp dbOp, Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  AC.setInsertionPoint(dbOp);

  bool isLoad = false;
  if (dbOp->hasAttr("isLoad"))
    isLoad = true;

  /// Datablock info
  memref = dbOp.getBase();
  auto dbMode = getMode(dbOp.getMode());

  /// Debug
  LLVM_DEBUG(dbgs() << " - Datablock: " << dbOp << "\n");

  /// Lambda function to get the size of the element type
  auto getTypeSize = [&]() {
    /// If it is a DatablockOp, get the size from the datablock
    if (auto dbBaseOp = dyn_cast<arts::DataBlockOp>(memref.getDefiningOp())) {
      auto dbBase = AC.getOrCreateDatablock(dbBaseOp, dbBaseOp.getLoc());
      assert(dbBase && "Datablock not found");
      return dbBase->size;
    }
    /// If it is a memref, get the size from the memref type
    auto baseMemTy = memref.getType().cast<MemRefType>();
    auto baseTy = baseMemTy.getElementType();
    auto dbTySize =
        AC.createIntConstant(AC.mlirDL.getTypeSize(baseTy), AC.Int32, loc);
    return dbTySize;
  };

  // LLVM_DEBUG(DBGS() << "Datablock type: " << baseTy << "\n");
  Value oneIdx = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto dbPtr = AC.castToLLVMPtr(memref, loc);
  numElements = oneIdx;
  auto dbTySize = getTypeSize();
  if (isLoad) {
    size = dbTySize;
    guid = AC.createRuntimeCall(ARTSRTL_artsDbCreatePtr,
                                {dbPtr, dbTySize, dbMode}, loc)
               ->getResult(0);
    return;
  }

  /// If we hit this point, we are haven an array of datablocks
  isArray = true;

  /// We'll multiply the sizes to get numElements.
  auto sizes = dbOp.getSizes();
  for (Value sz : sizes)
    numElements = builder.create<arith::MulIOp>(loc, numElements, sz);

  /// Create an array of Datablocks
  guid = builder.create<memref::AllocaOp>(loc, AC.ArtsDbPtr, numElements);
  auto numElementsInt = AC.castToInt(AC.Int32, numElements, loc);
  size = builder.create<arith::MulIOp>(loc, numElementsInt, dbTySize);
  AC.createRuntimeCall(ARTSRTL_artsDbCreateArray,
                       {guid, dbTySize, dbMode, numElementsInt, dbPtr}, loc);
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

// ---------------------------- EDTs ---------------------------- ///
unsigned EdtCodegen::edtCounter = 0;

EdtCodegen::EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps,
                       SmallVector<Value> *opParams,
                       SmallVector<Value> *opConsts, Region *region,
                       Value *epoch, Location *loc, bool build,
                       ConversionPatternRewriter *rewriter)
    : AC(AC), builder(AC.builder), region(region), epoch(epoch) {
  OpBuilder::InsertionGuard guard(builder);
  auto curLoc = loc ? *loc : UnknownLoc::get(builder.getContext());

  func = createFn(curLoc);
  node = AC.getCurrentNode(curLoc);
  processDepsAndParams(opDeps, opParams, curLoc);
  if (opConsts)
    consts = *opConsts;

  if (build && rewriter)
    this->build(curLoc, *rewriter);
}

void EdtCodegen::build(Location loc, ConversionPatternRewriter &rewriter) {
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

  /// Replace the arts.yield with a return
  auto yieldOp = funcEntryBlock.getTerminator();
  AC.setInsertionPoint(yieldOp);
  yieldOp->replaceAllUsesWith(builder.create<func::ReturnOp>(loc));

  // rewriter.replaceOpWithNewOp<func::ReturnOp>(yieldOp, ValueRange{});

  // auto regionParent = region->getParentOp();
  // rewriter.eraseOp(regionParent);
}

void EdtCodegen::processDepsAndParams(SmallVector<Value> *opDeps,
                                      SmallVector<Value> *opParams,
                                      Location loc) {
  /// Initialize the parameters
  if (opParams)
    params.append(opParams->begin(), opParams->end());

  if (opDeps) {
    deps = *opDeps;
    /// Creates a DataBlock for each dependency
    SmallVector<DataBlockCodegen *> dbs;
    for (auto dep : *opDeps) {
      auto makeDepOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp());
      assert(makeDepOp && "Dependency is not a datablock op");
      dbs.push_back(AC.getDatablock(makeDepOp));
    }

    /// Compute depC and update parameters
    depC = AC.createIndexConstant(0, loc);
    for (auto db : dbs) {
      /// If the db is an array, add the number of elements as a parameter
      if (db->getIsArray())
        dbSizeMap[db] = insertParam(db->getNumElements());
      /// Add the number of elements in the datablock
      depC = builder.create<arith::AddIOp>(loc, depC, db->getNumElements())
                 .getResult();
    }
    AC.castToInt(AC.Int32, depC, loc);
  }

  /// Insert the parameters
  unsigned int paramSize = params.size();
  paramC = AC.createIntConstant(paramSize, AC.Int32, loc);
  auto paramVArray = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({paramSize}, AC.Int64));
  for (unsigned i = 0; i < paramSize; i++) {
    /// If the value is not an i64, cast it
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    /// Affine store
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
  for (auto &dep : deps) {
    /// Get DataBlock
    auto oldDep = cast<DataBlockOp>(dep.getDefiningOp());
    auto *db = AC.getDatablock(oldDep);
    /// Create an array of dbs
    if (db->getIsArray()) {
      auto numElements = getNewParam(dbSizeMap[db]);
      auto elementTy =
          db->getMemref().getType().cast<MemRefType>().getElementType();
      LLVM_DEBUG(dbgs() << " - Array of Datablocks: " << db->getMemref()
                        << "\n");
      LLVM_DEBUG(dbgs() << " - Type: " << elementTy << "\n");

      auto [dbPtrArray, dbGuidArray] = AC.CreatePtrAndGuidArrayFromDeps(
          numElements, elementTy, fnDepV, index, loc);
      /// Cast the dbPtrArray to the original type
      // dbPtrArray = AC.castArrayDependency(db, numElements, dbPtrArray, loc);
      rewireMap[oldDep] = dbPtrArray;
      /// Increment the index
      index =
          builder.create<arith::AddIOp>(loc, index, numElements).getResult();
      continue;
    }

    /// artsDbCreateArrayFromDeps Get dependency ptr
    auto depVElem =
        builder.create<memref::LoadOp>(loc, fnDepV, ValueRange({index}));
    auto depPtr = AC.getPtrFromEdtDep(depVElem, loc);
    /// Increment the index
    index = builder.create<arith::AddIOp>(loc, index, oneIdx).getResult();
    /// Cast it back to the original type
    // AC.castDependency(dep, depPtr, loc);
    rewireMap[oldDep] = depPtr;
  }

  utils::replaceInRegion(*region, rewireMap);
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
ArtsCodegen::ArtsCodegen(ModuleOp &module, OpBuilder &builder,
                         llvm::DataLayout &llvmDL, mlir::DataLayout &mlirDL)
    : module(module), builder(builder), llvmDL(llvmDL), mlirDL(mlirDL) {
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

/// DataBlock
DataBlockCodegen *ArtsCodegen::getDatablock(arts::DataBlockOp dbOp) {
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

EdtCodegen *ArtsCodegen::createEdt(SmallVector<Value> *opDeps,
                                   SmallVector<Value> *opParams,
                                   SmallVector<Value> *opConsts, Region *region,
                                   Value *epoch, Location *loc, bool build,
                                   ConversionPatternRewriter *rewriter) {
  assert(region && !getEdt(region) && "Edt already exists");
  edts[region] = new EdtCodegen(*this, opDeps, opParams, opConsts, region,
                                epoch, loc, build, rewriter);
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
    auto *depDb = getDatablock(cast<DataBlockOp>(dep.getDefiningOp()));
    assert(depDb && "Datablock not found");
    /// Add the number of elements in the datablock
    auto numElements = castToInt(Int32, depDb->getNumElements(), loc);
    numDeps =
        builder.create<arith::AddIOp>(loc, numDeps, numElements).getResult();
  }
  return numDeps;
}

Value ArtsCodegen::createArrayFromDeps(Value numElements, Value deps,
                                       Value initialSlot, Location loc) {
  /// Allocate an array of Datablocks
  auto dbArray = builder.create<memref::AllocaOp>(loc, ArtsDbPtr, numElements);
  /// Set info based on the deps
  createRuntimeCall(ARTSRTL_artsDbCreateArrayFromDeps,
                    {dbArray, numElements, deps, initialSlot}, loc);
  return dbArray;
}

pair<Value, Value> ArtsCodegen::CreatePtrAndGuidArrayFromDeps(Value numElements,
                                                              Type elemTy,
                                                              Value deps,
                                                              Value initialSlot,
                                                              Location loc) {
  /// Allocate an array of pointers and guids
  MemRefType ptrTy = MemRefType::get({ShapedType::kDynamic}, elemTy);
  auto arrayTy = MemRefType::get({ShapedType::kDynamic}, ptrTy);
  auto ptrArray = builder.create<memref::AllocaOp>(loc, arrayTy, numElements);
  auto guidArray = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, ArtsGuid), numElements);
  auto llvmPtr = castToLLVMPtr(ptrArray, loc);

  /// Set info based on the deps
  createRuntimeCall(ARTSRTL_artsDbCreatePtrAndGuidArrayFromDeps,
                    {guidArray, llvmPtr, numElements, deps, initialSlot}, loc);

  return {ptrArray, guidArray};
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

  if (!memref::CastOp::areCastCompatible(srcType, dstType)) {
    llvm::errs() << "Incompatible cast from " << srcType << " to " << dstType
                 << "\n";
    assert(false && "Incompatible cast");
  }
  return builder.create<memref::CastOp>(loc, dstType, source);
}

Value ArtsCodegen::castDependency(DataBlockCodegen *dbDep, Value source,
                                  Location loc) {
  assert(dbDep->getIsArray() && "Expected a single datablock");
  return castPointer(dbDep->getMemref().getType(), source, loc);
}

Value ArtsCodegen::castArrayDependency(DataBlockCodegen *dbDep, Value dbSize,
                                       Value source, Location loc) {
  assert(dbDep->getIsArray() && "Expected array datablock");
  auto dbMemref = dbDep->getMemref();
  auto targetType = dbMemref.getType();

  /// Get source and dest types
  auto srcType = source.getType().dyn_cast<MemRefType>();
  auto dstType = targetType.dyn_cast<MemRefType>();
  assert((srcType && dstType) && "Expected memref type");
  LLVM_DEBUG(dbgs() << "Cast array dependency from " << srcType << " to "
                    << dstType << "\n");

  /// New buffer
  auto newBuffer = builder.create<memref::AllocOp>(loc, dstType, dbSize);
  auto elementTy = dstType.getElementType();
  auto elementMemrefTy = MemRefType::get({ShapedType::kDynamic}, elementTy);

  LLVM_DEBUG(dbgs() << "New buffer: " << newBuffer << "\n");
  /// Create a for loop that goes through each element of the array
  /// and cast it to the new type
  auto c0 = builder.create<arith::ConstantIndexOp>(loc, 0);
  auto c1 = builder.create<arith::ConstantIndexOp>(loc, 1);
  auto loop = builder.create<scf::ForOp>(loc, c0, dbSize, c1);
  {
    builder.setInsertionPointToStart(loop.getBody());
    auto iv = loop.getInductionVar();
    auto srcPtr = builder.create<memref::LoadOp>(loc, source, iv);
    LLVM_DEBUG(dbgs() << "SrcPtr: " << srcPtr << "\n");
    Value castPtr = castPointer(elementMemrefTy, srcPtr, loc);
    LLVM_DEBUG(dbgs() << "CastPtr: " << castPtr << "\n");
    // Value loadPtr = builder.create<memref::LoadOp>(loc, castPtr, c0);
    // LLVM_DEBUG(dbgs() << "LoadPtr: " << loadPtr << "\n");

    // builder.create<memref::StoreOp>(loc, loadPtr, newBuffer, iv);
  }
  builder.setInsertionPointAfter(loop);
  return newBuffer;
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

Value ArtsCodegen::castToLLVMPtr(Value source, Location loc) {
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