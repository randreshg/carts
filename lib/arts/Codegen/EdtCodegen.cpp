///==========================================================================
/// File: EdtCodegen.cpp
///==========================================================================

#include "arts/Codegen/EdtCodegen.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Codegen/ArtsIR.h"
#include "arts/Codegen/DbCodegen.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/Transforms/DialectConversion.h"
#include "polygeist/Dialect.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "edt-codegen"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
#define METADATA "-----------------------------------------\n[artsCodegen] "

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::func;
using namespace mlir::polygeist;

unsigned EdtCodegen::edtCounter = 0;

EdtCodegen::EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps,
                       Region *region, Value *epoch, Location *loc,
                       bool buildEdt)
    : AC(AC), builder(AC.getBuilder()), region(region), epoch(epoch) {
  OpBuilder::InsertionGuard IG(builder);
  auto curLoc = loc ? *loc : AC.getUnknownLoc();

  func = createFn(curLoc);
  node = AC.getCurrentNode(curLoc);

  /// Get opParams and opConsts if region is provided
  if (region) {
    ConversionPatternRewriter rewriter(builder.getContext());
    EdtEnvManager envManager(rewriter, *region);
    envManager.naiveCollection(true);
    params.append(envManager.getParameters().begin(),
                  envManager.getParameters().end());
    consts.append(envManager.getConstants().begin(),
                  envManager.getConstants().end());
  }
  deps = opDeps ? *opDeps : SmallVector<Value>();

  /// Process the EDT: Computes depc, paramc, loads params in paramv
  process(curLoc);

  /// Inserts call to the ARTS API to create the EDT, outline the region into
  /// the function and record/satisfy events
  if (buildEdt)
    build(curLoc);
}

void EdtCodegen::build(Location loc) {
  assert(!built && "EDT already built");
  built = true;

  /// If not epoch is provided, create an EDT without it
  auto funcPtr = AC.createFnPtr(func, loc);
  AC.createPrintfCall(loc, METADATA "Creating EDT\n", {});
  if (!epoch) {
    guid = AC.createRuntimeCall(types::ARTSRTL_artsEdtCreate,
                                {funcPtr, node, paramC, paramV, depC}, loc)
               .getResult(0);
  } else {
    guid =
        AC.createRuntimeCall(types::ARTSRTL_artsEdtCreateWithEpoch,
                             {funcPtr, node, paramC, paramV, depC, *epoch}, loc)
            .getResult(0);
  }

  /// If no region is provided, we are done
  if (!region)
    return;

  /// Create the EDT function entry, then outline the region and finalize
  /// by processing events
  createEntry(loc);
  outlineRegion(loc);
  satisfyDependencies(loc);

  /// Clear rewireMap
  rewireMap.clear();
}

void EdtCodegen::process(Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  depC = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, IndexType::get(builder.getContext())));
  builder.create<memref::StoreOp>(loc, AC.createIndexConstant(0, loc), depC);

  analyzeDependencies(loc);
  setupParameters(loc);
}

void EdtCodegen::analyzeDependencies(Location loc) {
  if (deps.empty())
    return;

  for (const auto &dep : deps) {
    auto dbDepOp = dep.getDefiningOp<arts::DbDepOp>();
    assert(dbDepOp && "Dependency is not a DbDepOp");

    auto *dbDep = AC.getDbDep(dbDepOp);
    assert(dbDep && "DbDep not found");

    /// Set the EDT slot for the DB
    dbDep->setEdtSlot(builder.create<memref::LoadOp>(loc, depC).getResult());

    auto sizes = dbDepOp.getSizes();
    auto offsets = dbDepOp.getOffsets();
    auto rank = sizes.size();

    Value dbNumElements;
    if (rank <= 1) {
      /// For single-dimensional DBs, use size 1
      dbNumElements = AC.createIndexConstant(1, loc);
      insertSizeParameter(dbDep, dbNumElements, 0);
    } else {
      /// For multi-dimensional DBs, process each dimension and compute total
      /// elements
      Value product = AC.createIndexConstant(1, loc);
      for (uint64_t rankItr = 0; rankItr < rank; ++rankItr) {
        insertSizeParameter(dbDep, sizes[rankItr], rankItr);
        insertOffsetParameter(dbDep, offsets[rankItr], rankItr);
        product = builder.create<arith::MulIOp>(loc, product, sizes[rankItr])
                      .getResult();
      }
      dbNumElements = product;
    }

    /// Categorize dependencies for later processing
    if (dbDep->isInMode()) {
      /// Input DBs: accumulate dependency count
      auto currDep = builder.create<memref::LoadOp>(loc, depC);
      auto newDep = builder.create<arith::AddIOp>(loc, currDep, dbNumElements)
                        .getResult();
      builder.create<memref::StoreOp>(loc, newDep, depC);
      depsToRecord.push_back(dbDep);
    }

    if (dbDep->isOutMode()) {
      /// Output DBs: will need satisfaction
      depsToSatisfy.push_back(dbDep);
    }
  }
}

void EdtCodegen::setupParameters(Location loc) {
  auto indexMemRefType =
      MemRefType::get({}, IndexType::get(builder.getContext()));

  /// Cast dependency count to int32
  auto loadedDepC = builder.create<memref::LoadOp>(loc, depC);
  depC = AC.castToInt(AC.Int32, loadedDepC, loc);

  /// Setup parameter count and array
  unsigned staticParamCount = params.size();
  paramC = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(
      loc, AC.createIndexConstant(staticParamCount, loc), paramC);

  auto loadedParamC = builder.create<memref::LoadOp>(loc, paramC);
  paramC = AC.castToInt(AC.Int32, loadedParamC, loc);

  /// Allocate parameter array
  paramV = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64),
      ValueRange{loadedParamC});

  /// Load static parameters into the array
  for (unsigned i = 0; i < staticParamCount; ++i) {
    auto idx = AC.createIndexConstant(i, loc);
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    builder.create<memref::StoreOp>(loc, param, paramV, ValueRange{idx});
  }
}

void EdtCodegen::satisfyDependencies(Location loc) {
  recordInDeps(loc);
  incrementOutLatchCounts(loc);
  replaceEdtDepUses(loc);
}

func::FuncOp EdtCodegen::createFn(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(AC.getModule());
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, AC.EdtFn);
  edtFuncOp.setPrivate();
  auto *entryBlock = edtFuncOp.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);
  builder.create<func::ReturnOp>(loc);
  return edtFuncOp;
}

Value EdtCodegen::createGuid(Value node, Location loc) {
  auto guidEdtType = AC.createIntConstant(1, AC.ArtsType, loc);
  auto reserveGuidCall = AC.createRuntimeCall(
      types::ARTSRTL_artsReserveGuidRoute, {guidEdtType, node}, loc);
  return reserveGuidCall.getResult(0);
}

void EdtCodegen::createEntry(Location loc) {
  OpBuilder::InsertionGuard IG(builder);

  /// Setup function entry
  auto *entryBlock = &func.getBody().front();
  entryBlock->getTerminator()->erase();
  builder.setInsertionPointToStart(entryBlock);
  fnParamV = entryBlock->getArgument(1);
  fnDepV = entryBlock->getArgument(3);
  AC.createPrintfCall(loc, METADATA "Executing EDT with guid %u\n",
                      AC.getCurrentEdtGuid(loc));

  /// Clone constants
  for (auto &oldConst : consts) {
    rewireMap[oldConst] =
        builder.clone(*oldConst.getDefiningOp())->getResult(0);
  }

  /// Load parameters from function arguments
  for (unsigned i = 0, e = params.size(); i < e; ++i) {
    auto idx = AC.createIndexConstant(i, loc);
    auto paramElem =
        builder.create<memref::LoadOp>(loc, fnParamV, ValueRange{idx});
    rewireMap[params[i]] =
        AC.castParameter(params[i].getType(), paramElem, loc);
  }

  /// Process dependencies by computing the EDT context values
  auto zeroConst = AC.createIndexConstant(0, loc);
  auto indexAlloc = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, builder.getIndexType()));
  builder.create<memref::StoreOp>(loc, zeroConst, indexAlloc);

  const auto depStructSize =
      AC.castToIndex(builder.create<polygeist::TypeSizeOp>(
                         loc, builder.getIndexType(), AC.ArtsEdtDep),
                     loc);
  auto fnDepVPtr = AC.castToLLVMPtr(fnDepV, loc);

  for (auto &dep : deps) {
    auto *db = AC.getDbDep(dep);
    assert(db && "Db not found");
    if (!db->isInMode())
      continue;

    if (db->hasSingleSize()) {
      handleSingleDep(db, dep, indexAlloc.getResult(), depStructSize, fnDepVPtr,
                      loc);
    } else {
      handleMultiDep(db, dep, indexAlloc.getResult(), depStructSize, fnDepVPtr,
                     loc);
    }
    ///
    db->init(loc);
  }
}

void EdtCodegen::handleSingleDep(DbDepCodegen *db, Value dep, Value indexAlloc,
                                 const Value &depStructSize,
                                 const Value &fnDepVPtr, Location loc) {
  auto oneConst = AC.createIndexConstant(1, loc);
  auto curIndex = builder.create<memref::LoadOp>(loc, indexAlloc);

  /// Calculate memory offset and get dependency entry
  auto curGep = AC.castToInt(
      AC.Int32, builder.create<arith::MulIOp>(loc, curIndex, depStructSize),
      loc);
  auto depVElem = builder
                      .create<LLVM::GEPOp>(loc, AC.llvmPtr, AC.PtrSize,
                                           fnDepVPtr, ValueRange{curGep})
                      .getResult();
  auto entryGuid = AC.getGuidFromEdtDep(depVElem, loc);
  auto entryPtr = AC.getPtrFromEdtDep(depVElem, loc);

  /// Set EDT context attributes and rewire
  db->setGuidInEdt(entryGuid);
  db->setPtrInEdt(entryPtr);
  rewireMap[db->getOp()] = entryPtr;

  /// Increment the index for next dependency
  auto newIndex =
      builder.create<arith::AddIOp>(loc, curIndex, oneConst).getResult();
  builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
}

void EdtCodegen::handleMultiDep(DbDepCodegen *db, Value dep, Value indexAlloc,
                                const Value &depStructSize,
                                const Value &fnDepVPtr, Location loc) {
  /// Prepare dimension information
  const auto &dbSizes = db->getSizes();
  const auto &dbOffsets = db->getOffsets();
  const auto dbRank = dbSizes.size();
  auto &entrySizes = db->getSizesInEdt();
  auto &entryOffsets = db->getOffsetsInEdt();

  entrySizes.reserve(dbRank);
  entryOffsets.reserve(dbRank);

  /// Build actual sizes and offsets from parameters or constants
  for (unsigned i = 0; i < dbRank; ++i) {
    /// Sizes
    if (db->getSizeIndexInEdt().count(i))
      entrySizes.push_back(rewireMap[params[db->getSizeIndexInEdt()[i]]]);
    else if (auto cstOp = dbSizes[i].getDefiningOp<arith::ConstantIndexOp>())
      entrySizes.push_back(builder.clone(*cstOp)->getResult(0));
    else
      llvm_unreachable("Db size is not a constant");

    /// Offsets
    if (db->getOffsetIndexInEdt().count(i))
      entryOffsets.push_back(rewireMap[params[db->getOffsetIndexInEdt()[i]]]);
    else if (auto cstOp = dbOffsets[i].getDefiningOp<arith::ConstantIndexOp>())
      entryOffsets.push_back(builder.clone(*cstOp)->getResult(0));
    else
      llvm_unreachable("Db offset is not a constant");
  }

  /// Get current index and compute total elements
  auto curIndex = builder.create<memref::LoadOp>(loc, indexAlloc);
  Value totalElements = AC.createIndexConstant(1, loc);
  for (auto size : entrySizes)
    totalElements = builder.create<arith::MulIOp>(loc, totalElements, size);

  /// Advance index for next dependency
  auto nextIndex = builder.create<arith::AddIOp>(loc, curIndex, totalElements);
  builder.create<memref::StoreOp>(loc, nextIndex, indexAlloc);

  /// Create memref views for multi-dimensional access
  auto flatI8Type =
      MemRefType::get({ShapedType::kDynamic}, AC.getBuilder().getI8Type());
  auto fnDepVPtr_i8 = AC.castToLLVMPtr(fnDepV, loc);
  auto flatBuffer = builder.create<polygeist::Pointer2MemrefOp>(loc, flatI8Type,
                                                                fnDepVPtr_i8);

  /// Compute byte offset for this db section
  auto offsetInBytes =
      builder.create<arith::MulIOp>(loc, curIndex, depStructSize);

  /// Create target memref types with original dimensions
  auto guidViewType = MemRefType::get(
      SmallVector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
      AC.ArtsGuid);
  auto ptrViewType = MemRefType::get(
      SmallVector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
      AC.VoidPtr);

  /// Create N-dimensional views
  auto entryGuid = builder.create<memref::ViewOp>(loc, guidViewType, flatBuffer,
                                                  offsetInBytes, entrySizes);

  auto ptrFieldOffset = AC.createIndexConstant(16, loc);
  auto ptrByteOffset =
      builder.create<arith::AddIOp>(loc, offsetInBytes, ptrFieldOffset);
  auto entryPtr = builder.create<memref::ViewOp>(loc, ptrViewType, flatBuffer,
                                                 ptrByteOffset, entrySizes);

  db->setGuidInEdt(entryGuid);
  db->setPtrInEdt(entryPtr);
  rewireMap[db->getOp()] = entryPtr;
}

void EdtCodegen::outlineRegion(Location loc) {
  Block &funcEntryBlock = func.getBody().front();
  Block &entryBlock = region->front();

  // Apply rewireMap to the original region
  replaceInRegion(*region, rewireMap, false);

  // Move all operations from entryBlock to funcEntryBlock, except the
  // terminator
  auto &entryOps = entryBlock.getOperations();
  size_t numOpsToMove = entryOps.size() > 0 ? entryOps.size() - 1 : 0;

  if (numOpsToMove > 0) {
    funcEntryBlock.getOperations().splice(funcEntryBlock.end(), entryOps,
                                          entryOps.begin(),
                                          std::prev(entryOps.end()));
  }

  // Replace the arts.yield with return
  returnOp = nullptr;
  if (!entryBlock.empty()) {
    if (auto yieldOp = dyn_cast<arts::YieldOp>(entryBlock.back())) {
      builder.setInsertionPointToEnd(&funcEntryBlock);
      returnOp = builder.create<func::ReturnOp>(loc);
    }
  }
}

void EdtCodegen::insertValueAsParameter(
    Value value, uint64_t index,
    const std::function<void(unsigned)> &setIndex) {
  /// Skip constant values - they don't need to be parameterized
  if (isValueConstant(value))
    return;

  /// Check if value is already cached to avoid duplicate parameters
  auto cacheIt = parameterCache.find(value);
  if (cacheIt != parameterCache.end()) {
    setIndex(cacheIt->second);
    return;
  }

  /// Insert new parameter and cache the result
  auto paramPair = insertParam(value);
  parameterCache[value] = paramPair.second;
  setIndex(paramPair.second);
}

void EdtCodegen::insertSizeParameter(DbDepCodegen *dbDep, Value size,
                                     uint64_t sizeIdx) {
  insertValueAsParameter(size, sizeIdx, [&](unsigned paramIdx) {
    dbDep->getSizeIndexInEdt()[sizeIdx] = paramIdx;
  });
}

void EdtCodegen::insertOffsetParameter(DbDepCodegen *dbDep, Value offset,
                                       uint64_t offsetIdx) {
  insertValueAsParameter(offset, offsetIdx, [&](unsigned paramIdx) {
    dbDep->getOffsetIndexInEdt()[offsetIdx] = paramIdx;
  });
}

//===----------------------------------------------------------------------===//
// Dependency Processing
//===----------------------------------------------------------------------===//

void EdtCodegen::recordInDeps(Location loc) {
  /// Dependencies are recorded outside the EDT context
  if (depsToRecord.empty())
    return;

  builder.setInsertionPointAfter(guid.getDefiningOp());

  LLVM_DEBUG(dbgs() << "- Recording in-mode dependencies\n");
  for (auto *dbCG : depsToRecord) {
    if (dbCG->hasSingleSize())
      recordSingleInDep(dbCG, loc);
    else
      recordMultiInDep(dbCG, loc);
  }
}

void EdtCodegen::incrementOutLatchCounts(Location loc) {
  /// Latch counts are incremented outside the EDT context
  if (depsToSatisfy.empty())
    return;

  LLVM_DEBUG(
      dbgs() << "- Incrementing latch counts for out-mode dependencies\n");
  for (auto *dbCG : depsToSatisfy) {
    if (dbCG->hasSingleSize())
      incrementSingleOutDep(dbCG, loc);
    else
      incrementMultiOutDep(dbCG, loc);
  }
}

void EdtCodegen::replaceEdtDepUses(Location loc) {
  /// Replace all remaining uses of Edt deps with the corresponding Db pointers
  LLVM_DEBUG(dbgs() << "- Replacing EDT dependency uses\n");

  for (auto &dep : deps) {
    auto *dbDep = AC.getDbDep(dep);
    assert(dbDep && "DbDep not found");
    dbDep->getOp().replaceAllUsesWith(dbDep->getPtr());
  }
}

//===----------------------------------------------------------------------===//
// Datablock Processing
//===----------------------------------------------------------------------===//

void EdtCodegen::recordSingleInDep(DbDepCodegen *dbCG, Location loc) {
  auto dbGuid = dbCG->getGuid();
  assert(dbGuid && "GUID not available for DbDep");
  auto dbIndices = dbCG->getIndices();
  if (!dbIndices.empty())
    dbGuid = builder.create<memref::LoadOp>(loc, dbGuid, dbIndices);
  AC.addDbDep(dbGuid, guid, dbCG->getEdtSlot(), loc);
}

void EdtCodegen::recordMultiInDep(DbDepCodegen *dbCG, Location loc) {
  auto dbGuid = dbCG->getGuid();
  assert(dbGuid && "GUID not available for DbDep");

  auto indexMemRefType =
      MemRefType::get({}, IndexType::get(builder.getContext()));
  auto inSlotAlloc = builder.create<memref::AllocOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, dbCG->getEdtSlot(), inSlotAlloc);
  addDepsForMultiDb(dbGuid, guid, inSlotAlloc.getResult(), dbCG->getSizes(),
                    dbCG->getOffsets(), dbCG->getIndices(), loc);
}

void EdtCodegen::incrementSingleOutDep(DbDepCodegen *dbCG, Location loc) {
  auto dbGuid = dbCG->getGuid();
  assert(dbGuid && "GUID not available for DbDep");

  auto dbIndices = dbCG->getIndices();
  if (!dbIndices.empty())
    dbGuid = builder.create<memref::LoadOp>(loc, dbGuid, dbIndices);
  AC.incrementDbLatchCount(dbGuid, loc);
}

void EdtCodegen::incrementMultiOutDep(DbDepCodegen *dbCG, Location loc) {
  auto dbGuid = dbCG->getGuid();
  assert(dbGuid && "GUID not available for DbDep");

  auto indexMemRefType =
      MemRefType::get({}, IndexType::get(builder.getContext()));
  auto outSlotAlloc = builder.create<memref::AllocOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, dbCG->getEdtSlot(), outSlotAlloc);
  incrementLatchCountsForMultiDb(dbGuid, outSlotAlloc.getResult(),
                                 dbCG->getSizes(), dbCG->getOffsets(),
                                 dbCG->getIndices(), loc);
}

void EdtCodegen::addDepsForMultiDb(Value dbGuid, Value guid, Value inSlotAlloc,
                                   const SmallVector<Value> &dbSizes,
                                   const SmallVector<Value> &dbOffsets,
                                   const SmallVector<Value> &dbIndices,
                                   Location loc) {
  const unsigned dbRank = dbSizes.size();

  /// Pre-compute loop bounds and cache step constants
  SmallVector<std::pair<Value, Value>> precomputedBounds;
  precomputedBounds.reserve(dbRank);
  for (unsigned i = 0; i < dbRank; ++i) {
    auto lowerBound = dbOffsets[i];
    auto upperBound =
        builder.create<arith::AddIOp>(loc, lowerBound, dbSizes[i]);
    precomputedBounds.push_back({lowerBound, upperBound});
  }
  auto stepConstant = AC.createIndexConstant(1, loc);
  auto incrementConstant = AC.createIndexConstant(1, loc);

  std::function<void(unsigned, SmallVector<Value, 4> &)>
      addDependenciesRecursive = [&](unsigned dim,
                                     SmallVector<Value, 4> &indices) {
        if (dim == dbRank) {
          auto currentSlot = builder.create<memref::LoadOp>(loc, inSlotAlloc);
          SmallVector<Value> loadedDbIndices(dbIndices);
          loadedDbIndices.append(indices.begin(), indices.end());
          auto loadedDbGuid =
              builder.create<memref::LoadOp>(loc, dbGuid, loadedDbIndices);
          AC.addDbDep(loadedDbGuid, guid, currentSlot, loc);
          auto nextSlot = builder.create<arith::AddIOp>(loc, currentSlot,
                                                        incrementConstant);
          builder.create<memref::StoreOp>(loc, nextSlot, inSlotAlloc);
          return;
        }
        /// Use pre-computed bounds
        auto [lowerBound, upperBound] = precomputedBounds[dim];
        auto loopOp = builder.create<scf::ForOp>(loc, lowerBound, upperBound,
                                                 stepConstant);
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        indices.push_back(loopOp.getInductionVar());
        addDependenciesRecursive(dim + 1, indices);
        indices.pop_back();
        builder.setInsertionPointAfter(loopOp);
      };

  SmallVector<Value, 4> initIndices;
  addDependenciesRecursive(0, initIndices);
}

void EdtCodegen::incrementLatchCountsForMultiDb(
    Value dbGuid, Value outSlotAlloc, const SmallVector<Value> &dbSizes,
    const SmallVector<Value> &dbOffsets, const SmallVector<Value> &dbIndices,
    Location loc) {
  const unsigned dbRank = dbSizes.size();

  std::function<void(unsigned, Value, SmallVector<Value, 4> &)>
      incrementLatchRecursive = [&](unsigned dim, Value curSlot,
                                    SmallVector<Value, 4> &indices) {
        if (dim == dbRank) {
          SmallVector<Value> loadedDbIndices(dbIndices);
          loadedDbIndices.append(indices.begin(), indices.end());
          auto loadedDbGuid =
              builder.create<memref::LoadOp>(loc, dbGuid, loadedDbIndices);
          AC.incrementDbLatchCount(loadedDbGuid, loc);
          auto loadedSlot = builder.create<memref::LoadOp>(loc, curSlot);
          auto nextSlot =
              builder
                  .create<arith::AddIOp>(loc, loadedSlot,
                                         AC.createIndexConstant(1, loc))
                  .getResult();
          builder.create<memref::StoreOp>(loc, nextSlot, curSlot);
          return;
        } else {
          auto lowerBound = dbOffsets[dim];
          auto upperBound =
              builder.create<arith::AddIOp>(loc, lowerBound, dbSizes[dim]);
          auto step = AC.createIndexConstant(1, loc);
          auto loopOp = builder.create<scf::ForOp>(loc, lowerBound, upperBound,
                                                   step, curSlot);
          auto &loopBlock = loopOp.getRegion().front();
          builder.setInsertionPointToStart(&loopBlock);
          indices.push_back(loopBlock.getArgument(0));
          Value nextSlot = loopBlock.getArgument(1);
          incrementLatchRecursive(dim + 1, nextSlot, indices);
          indices.pop_back();
          builder.create<scf::YieldOp>(loc, nextSlot);
          builder.setInsertionPointAfter(loopOp);
        }
      };

  SmallVector<Value, 4> indices;
  incrementLatchRecursive(0, outSlotAlloc, indices);
}

func::FuncOp EdtCodegen::getFunc() { return func; }
Value EdtCodegen::getGuid() { return guid; }
Value EdtCodegen::getNode() { return node; }
SmallVector<Value> &EdtCodegen::getParams() { return params; }

void EdtCodegen::setFunc(func::FuncOp f) { func = f; }
void EdtCodegen::setGuid(Value g) { guid = g; }
void EdtCodegen::setNode(Value n) { node = n; }
void EdtCodegen::setParams(SmallVector<Value> p) { params = p; }
void EdtCodegen::setDeps(SmallVector<Value> d) { deps = d; }
void EdtCodegen::setDepC(Value d) { depC = d; }

std::pair<bool, int> EdtCodegen::insertParam(Value param) {
  for (unsigned i = 0, e = params.size(); i < e; ++i) {
    if (params[i] == param)
      return {false, i};
  }
  params.push_back(param);
  return {true, params.size() - 1};
}
