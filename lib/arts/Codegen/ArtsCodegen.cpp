///==========================================================================
/// File: ArtsCodegen.cpp
///==========================================================================

#include "arts/Codegen/ArtsCodegen.h"

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
#include "arts/Analysis/Edt/EdtAnalysis.h"
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

// DEBUG
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

/// ---------------------------- Memory Pool Optimization ---------------------------- ///
/// Cache frequently used memref types to avoid repeated type creation
class MemRefTypeCache {
private:
  DenseMap<std::pair<ArrayRef<int64_t>, Type>, MemRefType> cache;
  
public:
  MemRefType getOrCreate(ArrayRef<int64_t> shape, Type elementType, MLIRContext *ctx) {
    auto key = std::make_pair(shape, elementType);
    auto it = cache.find(key);
    if (it != cache.end()) {
      return it->second;
    }
    auto memRefType = MemRefType::get(shape, elementType);
    cache[key] = memRefType;
    return memRefType;
  }
};

/// Global cache instance
static MemRefTypeCache memRefCache;

// ---------------------------- Dbs ---------------------------- ///
DataBlockCodegen::DataBlockCodegen(ArtsCodegen &AC)
    : AC(AC), builder(AC.builder) {}

DataBlockCodegen::DataBlockCodegen(ArtsCodegen &AC, arts::DbControlOp dbOp,
                                   Location loc)
    : AC(AC), builder(AC.builder) {
  create(dbOp, loc);
}

void DataBlockCodegen::create(arts::DbControlOp depOp, Location loc) {
  /// Db info
  dbOp = depOp;

  /// If the base is a DB, it will be handled when inserting the EDT Entry
  if (hasPtrDb())
    return;

  /// Set insertion point to the DbControlOp
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);

  auto currentNode = AC.getCurrentNode(loc);
  auto elementTypeSize = dbOp.getElementTypeSize();
  const auto tySize = AC.castToInt(AC.Int64, elementTypeSize, loc);

  /// Handle the case of a single db
  if (hasSingleSize()) {
    AC.createPrintfCall(loc, METADATA "Creating single DB\n", {});
    auto modeVal = getMode(dbOp.getMode());
    guid = createGuid(currentNode, modeVal, loc);
    ptr =
        AC.createRuntimeCall(ARTSRTL_artsDbCreateWithGuid, {guid, tySize}, loc)
            ->getResult(0);
    return;
  }

  /// Create an array of guids and pointers based on the sizes of the dbOp
  AC.createPrintfCall(loc, METADATA "Creating array of DBs\n", {});
  auto dbSizes = dbOp.getSizes();
  const auto dbDim = dbSizes.size();
  auto modeVal = getMode(dbOp.getMode());
  auto guidType = MemRefType::get(
      SmallVector<int64_t>(dbDim, ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, dbSizes);
  auto ptrType = dbOp.getResult().getType().cast<MemRefType>();
  if (ptrType.hasStaticShape()) {
    ptr = builder.create<memref::AllocaOp>(loc, ptrType);
  } else {
    ptr = builder.create<memref::AllocaOp>(loc, ptrType, dbSizes);
  }

  /// OPTIMIZED: Flatten loops and batch GUID creation for better performance
  std::function<void(unsigned, SmallVector<Value, 4> &)> createDbs =
      [&](unsigned dim, SmallVector<Value, 4> &indices) {
        if (dim == dbDim) {
          auto guidVal = createGuid(currentNode, modeVal, loc);
          auto ptrVal = AC.createRuntimeCall(ARTSRTL_artsDbCreateWithGuid,
                                             {guidVal, tySize}, loc)
                            ->getResult(0);
          builder.create<memref::StoreOp>(loc, guidVal, guid, indices);
          builder.create<memref::StoreOp>(loc, ptrVal, ptr, indices);
          return;
        }
        /// OPTIMIZATION: Cache loop bounds to avoid repeated constant creation
        static DenseMap<Value, std::pair<Value, Value>> loopBoundsCache;
        auto cacheKey = dbSizes[dim];
        auto cacheIt = loopBoundsCache.find(cacheKey);
        Value lowerBound, upperBound, step;
        
        if (cacheIt != loopBoundsCache.end()) {
          lowerBound = cacheIt->second.first;
          upperBound = cacheIt->second.second;
        } else {
          lowerBound = AC.createIndexConstant(0, loc);
          upperBound = dbSizes[dim];
          loopBoundsCache[cacheKey] = {lowerBound, upperBound};
        }
        step = AC.createIndexConstant(1, loc);
        
        auto loopOp = builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        indices.push_back(loopOp.getInductionVar());
        createDbs(dim + 1, indices);
        indices.pop_back();
        builder.setInsertionPointAfter(loopOp);
      };

  SmallVector<Value, 4> indices;
  createDbs(0, indices);
}

Value DataBlockCodegen::getMode(llvm::StringRef mode) {
  auto Loc = UnknownLoc::get(builder.getContext());
  /// ARTS_DB_PIN
  auto enumValue = 10;
  /// ARTS_DB_READ
  if (mode == "in")
    enumValue = 8;

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
                       Region *region, Value *epoch, Location *loc,
                       bool buildEdt)
    : AC(AC), builder(AC.builder), region(region), epoch(epoch) {
  OpBuilder::InsertionGuard IG(builder);
  auto curLoc = loc ? *loc : UnknownLoc::get(builder.getContext());

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

  /// Process the EDT: Computes depc, paramc, loas params in paramv
  process(curLoc);

  /// Inserts call to the ARTS API to create the EDT, outline the region
  /// into the function and record/satisfy events
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
    guid = AC.createRuntimeCall(ARTSRTL_artsEdtCreate,
                                {funcPtr, node, paramC, paramV, depC}, loc)
               .getResult(0);
  } else {
    guid =
        AC.createRuntimeCall(ARTSRTL_artsEdtCreateWithEpoch,
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
  processDependencies(loc);

  /// Clear rewireMap
  rewireMap.clear();
}

void EdtCodegen::process(Location loc) {
  OpBuilder::InsertionGuard guard(builder);
  auto context = builder.getContext();
  auto indexType = IndexType::get(context);
  auto indexMemRefType = MemRefType::get({}, indexType);

  /// Allocate and initialize counters.
  auto zeroConst = AC.createIndexConstant(0, loc);

  /// Allocate dependency count (depC) and initialize to zero.
  depC = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, zeroConst, depC);

  /// OPTIMIZED: Cache parameter insertions to avoid duplicates
  DenseMap<Value, unsigned> parameterCache;
  
  auto insertSizeAsParameter = [&](DataBlockCodegen *db, Value size,
                                   uint64_t sizeIdx) {
    if (isValueConstant(size))
      return;
    
    // Check cache first to avoid duplicate parameters
    auto cacheIt = parameterCache.find(size);
    if (cacheIt != parameterCache.end()) {
      entryDbs[db].sizeIndex[sizeIdx] = cacheIt->second;
      return;
    }
    
    auto paramPair = insertParam(size);
    parameterCache[size] = paramPair.second;
    entryDbs[db].sizeIndex[sizeIdx] = paramPair.second;
  };

  auto insertOffsetAsParameter = [&](DataBlockCodegen *db, Value offset,
                                     uint64_t offsetIdx) {
    if (isValueConstant(offset))
      return;
      
    // Check cache first to avoid duplicate parameters
    auto cacheIt = parameterCache.find(offset);
    if (cacheIt != parameterCache.end()) {
      entryDbs[db].offsetIndex[offsetIdx] = cacheIt->second;
      return;
    }
    
    auto paramPair = insertParam(offset);
    parameterCache[offset] = paramPair.second;
    entryDbs[db].offsetIndex[offsetIdx] = paramPair.second;
  };

  /// Process each dependency.
  if (!deps.empty()) {
    for (const auto &dep : deps) {
      // Handle both DbControlOp results and memref.subview results
      auto db = AC.getDb(dep);
      if (!db) {
        // For subview results, handle dependency differently
        processSubviewDependency(dep, loc);
        continue;
      }
      assert(db && "Db not found");

      /// Set the EDT slot for the DB - We do so, by computing the number of
      /// elements of each DB and storing it in the depC. The EDTSlot will be
      /// used in processDependencies when recording the dependencies.
      db->setEdtSlot(builder.create<memref::LoadOp>(loc, depC).getResult());
      Value dbNumElements = nullptr;

      if (db->hasSingleSize()) {
        /// For single-dimensional DBs, use the size directly.
        dbNumElements = AC.createIndexConstant(1, loc);
        insertSizeAsParameter(db, dbNumElements, 0);
      } else {
        /// For multi-dimensional DBs, compute the product of sizes.
        Value product = AC.createIndexConstant(1, loc);
        const auto sizes = db->getSizes();
        const auto offsets = db->getOffsets();
        auto rank = sizes.size();
        for (uint64_t rankItr = 0; rankItr < rank; ++rankItr) {
          insertSizeAsParameter(db, sizes[rankItr], rankItr);
          insertOffsetAsParameter(db, offsets[rankItr], rankItr);
          product = builder.create<arith::MulIOp>(loc, product, sizes[rankItr])
                        .getResult();
        }
        dbNumElements = product;
      }

      /// Input DBs are recorded, accumulate dependency count.
      if (db->isInMode()) {
        auto currDep = builder.create<memref::LoadOp>(loc, depC);
        auto newDep = builder.create<arith::AddIOp>(loc, currDep, dbNumElements)
                          .getResult();
        builder.create<memref::StoreOp>(loc, newDep, depC);
        depsToRecord.push_back(db);
      }

      /// Output DBs are satisfied
      if (db->isOutMode())
        depsToSatisfy.push_back(db);
    }
  }

  /// Cast the dependency count (depC) to int32.
  auto loadedDepC = builder.create<memref::LoadOp>(loc, depC);
  depC = AC.castToInt(AC.Int32, loadedDepC, loc);

  /// Process parameters: start with the static parameter count.
  unsigned staticParamCount = params.size();
  paramC = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(
      loc, AC.createIndexConstant(staticParamCount, loc), paramC);

  /// Cast the updated parameter count to int32.
  auto loadedParamC = builder.create<memref::LoadOp>(loc, paramC);
  paramC = AC.castToInt(AC.Int32, loadedParamC, loc);

  /// Allocate the parameter array (paramV) of type Int64.
  paramV = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64),
      ValueRange{loadedParamC});

  /// Load the static parameters into paramV.
  for (unsigned i = 0; i < staticParamCount; ++i) {
    auto paramIndex = AC.createIndexConstant(i, loc);
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    builder.create<memref::StoreOp>(loc, param, paramV, ValueRange{paramIndex});
  }
}

void EdtCodegen::processSubviewDependency(Value subview, Location loc) {
  // For subview dependencies, we need to trace back to the source DbCreateOp
  auto subviewOp = subview.getDefiningOp<memref::SubViewOp>();
  if (!subviewOp) return;
  
  // Trace back through the subview chain to find the original DbCreateOp
  Value source = subviewOp.getSource();
  while (auto parentSubview = source.getDefiningOp<memref::SubViewOp>()) {
    source = parentSubview.getSource();
  }
  
  // Check if the source is a DbCreateOp
  auto dbCreateOp = source.getDefiningOp<arts::DbCreateOp>();
  if (!dbCreateOp) return;
  
  // Get the GUID from the DbCreateOp
  Value guid = dbCreateOp.getGuid();
  
  // Determine if this is an input or output dependency based on the mode
  StringRef mode = dbCreateOp.getMode();
  bool isInput = (mode == "in" || mode == "inout");
  bool isOutput = (mode == "out" || mode == "inout");
  
  // Set EDT slot
  Value edtSlot = builder.create<memref::LoadOp>(loc, depC).getResult();
  
  // For input dependencies, increment dependency count and add to recording list
  if (isInput) {
    Value one = AC.createIndexConstant(1, loc);
    auto currDep = builder.create<memref::LoadOp>(loc, depC);
    auto newDep = builder.create<arith::AddIOp>(loc, currDep, one).getResult();
    builder.create<memref::StoreOp>(loc, newDep, depC);
    
    // Create a simple db info struct for this subview
    auto *dbCG = new DataBlockCodegen(AC);
    dbCG->setGuid(guid);
    dbCG->setEdtSlot(edtSlot);
    
    depsToRecord.push_back(dbCG);
  }
  
  // For output dependencies, add to satisfaction list
  if (isOutput) {
    // Create a simple db info struct for this subview
    auto *dbCG = new DataBlockCodegen(AC);
    dbCG->setGuid(guid);
    dbCG->setEdtSlot(edtSlot);
    
    depsToSatisfy.push_back(dbCG);
  }
}

void EdtCodegen::processDependencies(Location loc) {
  const auto indexType = IndexType::get(builder.getContext());
  const auto indexMemRefType = MemRefType::get({}, indexType);

  /// ---------------------------------------------------------------------
  /// Record In-Mode Dependencies
  /// ---------------------------------------------------------------------
  if (!depsToRecord.empty()) {
    LLVM_DEBUG(dbgs() << "- Recording in-mode dependencies\n");
    builder.setInsertionPointAfter(guid.getDefiningOp());
    for (auto *dbCG : depsToRecord) {
      /// Retrieve the db GUID and location.
      auto dbGuid = dbCG->getGuid();
      auto dbLoc = dbCG->getOp().getLoc();

      /// For single-dimension datablocks, add the dependency directly.
      if (dbCG->hasSingleSize()) {
        auto dbIndices = dbCG->getIndices();
        if (!dbIndices.empty())
          dbGuid = builder.create<memref::LoadOp>(dbLoc, dbGuid, dbIndices);
        AC.addDbDependency(dbGuid, guid, dbCG->getEdtSlot(), dbLoc);
        continue;
      }

      /// For multidimensional datablocks, add dependencies for each element.
      const auto dbSizes = dbCG->getSizes();
      const auto dbOffsets = dbCG->getOffsets();
      const auto dbIndices = dbCG->getIndices();
      const unsigned dbRank = dbSizes.size();
      auto inSlotAlloc =
          builder.create<memref::AllocOp>(dbLoc, indexMemRefType);
      builder.create<memref::StoreOp>(dbLoc, dbCG->getEdtSlot(), inSlotAlloc);

      /// OPTIMIZED: Pre-compute loop bounds and cache step constants
      SmallVector<std::pair<Value, Value>> precomputedBounds;
      precomputedBounds.reserve(dbRank);
      for (unsigned i = 0; i < dbRank; ++i) {
        auto lowerBound = dbOffsets[i];
        auto upperBound = builder.create<arith::AddIOp>(dbLoc, lowerBound, dbSizes[i]);
        precomputedBounds.push_back({lowerBound, upperBound});
      }
      auto stepConstant = AC.createIndexConstant(1, dbLoc);
      auto incrementConstant = AC.createIndexConstant(1, dbLoc);

      std::function<void(unsigned, SmallVector<Value, 4> &)>
          addDependenciesRecursive = [&](unsigned dim,
                                         SmallVector<Value, 4> &indices) {
            if (dim == dbRank) {
              auto currentSlot = builder.create<memref::LoadOp>(
                  dbLoc, inSlotAlloc.getResult());
              SmallVector<Value> loadedDbIndices(dbIndices);
              loadedDbIndices.append(indices.begin(), indices.end());
              auto loadedDbGuid = builder.create<memref::LoadOp>(
                  dbLoc, dbGuid, loadedDbIndices);
              AC.addDbDependency(loadedDbGuid, guid, currentSlot, dbLoc);
              /// Use pre-computed increment constant
              auto nextSlot = builder.create<arith::AddIOp>(
                  dbLoc, currentSlot, incrementConstant).getResult();
              builder.create<memref::StoreOp>(dbLoc, nextSlot, inSlotAlloc);
              return;
            }
            /// Use pre-computed bounds
            auto [lowerBound, upperBound] = precomputedBounds[dim];
            auto loopOp = builder.create<scf::ForOp>(dbLoc, lowerBound, upperBound, stepConstant);
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
  }

  /// ---------------------------------------------------------------------
  /// Increment Latch Counts for Out-Mode Dependencies
  /// ---------------------------------------------------------------------
  if (!depsToSatisfy.empty()) {
    LLVM_DEBUG(
        dbgs() << "- Incrementing latch counts for out-mode dependencies\n");
    for (auto *dbCG : depsToSatisfy) {
      auto dbGuid = dbCG->getGuid();
      assert(dbGuid && "Db GUID not found");
      auto dbLoc = dbCG->getOp().getLoc();

      /// For single-dimension datablocks.
      if (dbCG->hasSingleSize()) {
        auto dbIndices = dbCG->getIndices();
        if (!dbIndices.empty())
          dbGuid = builder.create<memref::LoadOp>(dbLoc, dbGuid, dbIndices);
        AC.incrementDbLatchCount(dbGuid, dbLoc);
        continue;
      }

      /// For multidimensional datablocks, increment latch counts recursively.
      const auto dbSizes = dbCG->getSizes();
      const auto dbOffsets = dbCG->getOffsets();
      const auto dbIndices = dbCG->getIndices();
      const unsigned dbRank = dbSizes.size();
      auto outSlotAlloc =
          builder.create<memref::AllocOp>(dbLoc, indexMemRefType);
      builder.create<memref::StoreOp>(dbLoc, dbCG->getEdtSlot(), outSlotAlloc);

      std::function<void(unsigned, Value, SmallVector<Value, 4> &)>
          incrementLatchRecursive = [&](unsigned dim, Value curSlot,
                                        SmallVector<Value, 4> &indices) {
            if (dim == dbRank) {
              SmallVector<Value> loadedDbIndices(dbIndices);
              loadedDbIndices.append(indices.begin(), indices.end());
              auto loadedDbGuid = builder.create<memref::LoadOp>(
                  dbLoc, dbGuid, loadedDbIndices);
              AC.incrementDbLatchCount(loadedDbGuid, dbLoc);
              auto loadedSlot = builder.create<memref::LoadOp>(dbLoc, curSlot);
              auto nextSlot =
                  builder
                      .create<arith::AddIOp>(dbLoc, loadedSlot,
                                             AC.createIndexConstant(1, dbLoc))
                      .getResult();
              builder.create<memref::StoreOp>(dbLoc, nextSlot, curSlot);
              return;
            } else {
              auto lowerBound = dbOffsets[dim];
              auto upperBound = builder.create<arith::AddIOp>(dbLoc, lowerBound,
                                                              dbSizes[dim]);
              auto step = AC.createIndexConstant(1, dbLoc);
              auto loopOp = builder.create<scf::ForOp>(
                  dbLoc, lowerBound, upperBound, step, curSlot);
              auto &loopBlock = loopOp.getRegion().front();
              builder.setInsertionPointToStart(&loopBlock);
              indices.push_back(loopBlock.getArgument(0));
              Value nextSlot = loopBlock.getArgument(1);
              incrementLatchRecursive(dim + 1, nextSlot, indices);
              indices.pop_back();
              builder.create<scf::YieldOp>(dbLoc, nextSlot);
              builder.setInsertionPointAfter(loopOp);
            }
          };

      SmallVector<Value, 4> indices;
      incrementLatchRecursive(0, outSlotAlloc.getResult(), indices);
    }
  }

  /// ---------------------------------------------------------------------
  /// Replace EDT Dependency Uses
  /// Replace all remaining uses of EDT dependencies with the corresponding
  /// db pointers.
  /// ---------------------------------------------------------------------
  LLVM_DEBUG(dbgs() << "- Replacing EDT dependency uses\n");
  for (auto &dep : deps) {
    auto *db = AC.getDb(dep);
    assert(db && "Db not found");
    if (!db->getPtr()) {
      LLVM_DEBUG(dbgs() << "Db not found or pointer not set\n");
      continue;
    }
    db->getOp().replaceAllUsesWith(db->getPtr());
  }
}

void EdtCodegen::outlineRegion(Location loc) {
  ConversionPatternRewriter rewriter(builder.getContext());
  Region &funcRegion = func.getBody();
  Block &funcEntryBlock = funcRegion.front();
  Block &entryBlock = region->front();
  rewriter.inlineRegionBefore(*region, funcRegion, funcRegion.end());

  /// Move all operations from entryBlock to funcEntryBlock.
  funcEntryBlock.getOperations().splice(funcEntryBlock.end(),
                                        entryBlock.getOperations());
  entryBlock.erase();

  /// Replace each arts.yield terminator with a return.
  returnOp = nullptr;
  for (auto &block : funcRegion) {
    if (auto yieldOp = dyn_cast<arts::YieldOp>(block.getTerminator())) {
      AC.setInsertionPoint(yieldOp);
      assert(!returnOp &&
             "Multiple yields in the same block are not supported");
      returnOp = builder.create<func::ReturnOp>(loc);
      yieldOp->erase();
    }
  }
}

Value EdtCodegen::createGuid(Value node, Location loc) {
  auto guidEdtType = AC.createIntConstant(1, AC.ArtsType, loc);
  auto reserveGuidCall = AC.createRuntimeCall(ARTSRTL_artsReserveGuidRoute,
                                              {guidEdtType, node}, loc);
  return reserveGuidCall.getResult(0);
}

func::FuncOp EdtCodegen::createFn(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(AC.module);
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, AC.EdtFn);
  edtFuncOp.setPrivate();
  AC.module.push_back(edtFuncOp);
  /// Add entry basic block to the function and return operation.
  auto *entryBlock = edtFuncOp.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);
  builder.create<func::ReturnOp>(loc);
  return edtFuncOp;
}

void EdtCodegen::createEntry(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  /// Create an entry block and remove the terminator.
  auto *entryBlock = &func.getBody().front();
  entryBlock->getTerminator()->erase();

  /// Get references to the block arguments.
  builder.setInsertionPointToStart(entryBlock);
  fnParamV = entryBlock->getArgument(1);
  fnDepV = entryBlock->getArgument(3);

  AC.createPrintfCall(loc, METADATA "Executing EDT with guid %u\n",
                      AC.getCurrentEdtGuid(loc));
  /// Clone constants.
  for (auto &oldConst : consts) {
    rewireMap[oldConst] =
        builder.clone(*oldConst.getDefiningOp())->getResult(0);
  }

  /// Insert the parameters.
  for (unsigned i = 0, e = params.size(); i < e; ++i) {
    auto idx = AC.createIndexConstant(i, loc);
    auto paramElem =
        builder.create<memref::LoadOp>(loc, fnParamV, ValueRange{idx});
    /// Cast it back to the original type.
    rewireMap[params[i]] =
        AC.castParameter(params[i].getType(), paramElem, loc);
  }

  /// Constants
  auto zeroConst = AC.createIndexConstant(0, loc);
  auto oneConst = AC.createIndexConstant(1, loc);

  /// Insert the dependencies.
  auto indexAlloc = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, builder.getIndexType()));
  builder.create<memref::StoreOp>(loc, zeroConst, indexAlloc);

  /// Helper function to load the current index.
  auto loadIndex = [&]() -> Value {
    return builder.create<memref::LoadOp>(loc, indexAlloc.getResult());
  };

  /// Process each dependency.
  const auto depStructSize =
      AC.castToIndex(builder.create<polygeist::TypeSizeOp>(
                         loc, builder.getIndexType(), AC.ArtsEdtDep),
                     loc);
  auto fnDepVPtr = AC.castToLLVMPtr(fnDepV, loc);

  for (auto &dep : deps) {
    /// Get corresponding DB.
    auto *db = AC.getDb(dep);
    assert(db && "Db not found");

    /// Skip DBs that are not in mode.
    if (!db->isInMode())
      continue;

    /// Iterate over uses to see if another DB is using this one. If
    /// so, set the guid and ptr for the other DB.
    auto updateUserDb = [&](Value entryGuid, Value entryPtr) {
      for (auto *user : db->getOp().getUsers()) {
        /// We are only concerned with datablocks in the same EDT region.
        if (!region->isAncestor(user->getParentRegion()))
          continue;
        /// If not a db, skip.
        auto userOp = dyn_cast<arts::DbControlOp>(user);
        if (!userOp)
          continue;
        /// Get the db codegen and set the entry information.
        auto userDb = AC.getDb(userOp);
        assert(userDb && "User db not found");
        userDb->setGuid(entryGuid);
        userDb->setPtr(entryPtr);
      }
    };

    /// Handle single DB
    if (db->hasSingleSize()) {
      auto curIndex = loadIndex();
      auto curGep = AC.castToInt(
          AC.Int32, builder.create<arith::MulIOp>(loc, curIndex, depStructSize),
          loc);
      auto depVElem = builder
                          .create<LLVM::GEPOp>(loc, AC.llvmPtr, AC.PtrSize,
                                               fnDepVPtr, ValueRange{curGep})
                          .getResult();
      auto entryGuid = AC.getGuidFromEdtDep(depVElem, loc);
      auto entryPtr = AC.getPtrFromEdtDep(depVElem, loc);
      entryDbs[db].guid = entryGuid;
      entryDbs[db].ptr = entryPtr;
      /// Increment the index.
      auto newIndex =
          builder.create<arith::AddIOp>(loc, curIndex, oneConst).getResult();
      builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
      /// Rewire the DB to the pointer.
      updateUserDb(entryGuid, entryPtr);
      rewireMap[db->getOp()] = entryPtr;
      continue;
    }

    /// Handle multi-dimensional DB
    const auto &dbSizes = db->getSizes();
    const auto &dbOffsets = db->getOffsets();
    const auto dbRank = dbSizes.size();
    auto &entrySizes = entryDbs[db].sizes;
    auto &entryOffsets = entryDbs[db].offsets;
    entrySizes.reserve(dbRank);
    entryOffsets.reserve(dbRank);
    for (unsigned i = 0; i < dbRank; ++i) {
      /// Sizes
      if (entryDbs[db].sizeIndex.count(i))
        entrySizes.push_back(rewireMap[params[entryDbs[db].sizeIndex[i]]]);
      else if (auto cstOp = dbSizes[i].getDefiningOp<arith::ConstantIndexOp>())
        entrySizes.push_back(builder.clone(*cstOp)->getResult(0));
      else
        llvm_unreachable("Db size is not a constant");

      /// Offsets
      if (entryDbs[db].offsetIndex.count(i))
        entryOffsets.push_back(rewireMap[params[entryDbs[db].offsetIndex[i]]]);
      else if (auto cstOp =
                   dbOffsets[i].getDefiningOp<arith::ConstantIndexOp>())
        entryOffsets.push_back(builder.clone(*cstOp)->getResult(0));
      else
        llvm_unreachable("Db offset is not a constant");
    }

    /// CLEAN APPROACH: Use memref.view to create N-dimensional views from flat fnDepV.
    /// This maintains the original multi-dimensional structure while avoiding reallocation.
    
    auto curIndex = loadIndex();
    
    /// Compute total elements and advance the index
    Value totalElements = AC.createIndexConstant(1, loc);
    for (auto size : entrySizes) {
      totalElements = builder.create<arith::MulIOp>(loc, totalElements, size);
    }
    auto nextIndex = builder.create<arith::AddIOp>(loc, curIndex, totalElements);
    builder.create<memref::StoreOp>(loc, nextIndex, indexAlloc);

    /// Cast fnDepV to a flat i8 buffer (required by memref.view)
    auto flatI8Type = MemRefType::get({ShapedType::kDynamic}, AC.getBuilder().getI8Type());
    auto fnDepVPtr_i8 = AC.castToLLVMPtr(fnDepV, loc);
    auto flatBuffer = builder.create<polygeist::Pointer2MemrefOp>(loc, flatI8Type, fnDepVPtr_i8);
    
    /// Compute byte offset for this db section  
    auto offsetInBytes = builder.create<arith::MulIOp>(loc, curIndex, depStructSize);
    
    /// Create target memref types with original dimensions (keeping offsets & sizes!)
    auto guidViewType = MemRefType::get(
        SmallVector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.ArtsGuid);
    auto ptrViewType = MemRefType::get(
        SmallVector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.VoidPtr);
    
    /// Use memref.view to create N-dimensional views maintaining original structure!
    /// GUID view: offset points to guid field (first field in artsEdtDep_t)
    auto entryGuid = builder.create<memref::ViewOp>(
        loc, guidViewType, flatBuffer, offsetInBytes, entrySizes);
    
    /// PTR view: Create a view that directly accesses the .ptr field from each artsEdtDep_t
    /// The stride between elements should be sizeof(artsEdtDep_t), and we offset to the .ptr field
    auto ptrFieldOffset = AC.createIndexConstant(16, loc); // Offset to .ptr field in artsEdtDep_t
    auto ptrByteOffset = builder.create<arith::AddIOp>(loc, offsetInBytes, ptrFieldOffset);
    auto entryPtr = builder.create<memref::ViewOp>(
        loc, ptrViewType, flatBuffer, ptrByteOffset, entrySizes);

    /// Store the views - now we preserve the original multi-dimensional structure!
    entryDbs[db].guid = entryGuid;
    entryDbs[db].ptr = entryPtr;

    /// For updateUserDb and rewireMap, we need to provide the actual .ptr field access
    /// Get the first element as representative and extract its .ptr field
    SmallVector<Value> zeroIndices(entrySizes.size(), AC.createIndexConstant(0, loc));
    auto firstGuid = builder.create<memref::LoadOp>(loc, entryGuid, zeroIndices);
    auto firstPtr = builder.create<memref::LoadOp>(loc, entryPtr, zeroIndices);
    
    updateUserDb(firstGuid, firstPtr);
    /// Replace uses with the actual ptr field from depv (not the whole structure)
    rewireMap[db->getOp()] = firstPtr;
  }

  /// Replace all uses in the region.
  replaceInRegion(*region, rewireMap, false);
}

/// ---------------------------- ARTS Codegen ---------------------------- ///
ArtsCodegen::ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
                         mlir::DataLayout &mlirDL, bool debug)
    : module(module), builder(OpBuilder(module->getContext())), llvmDL(llvmDL),
      mlirDL(mlirDL), debug(debug) {
  initializeTypes();
  collectGlobalLLVMStrings();
}

ArtsCodegen::~ArtsCodegen() {
  for (auto &db : datablocks)
    delete db.second;
  for (auto &edt : edts)
    delete edt.second;
}

/// OPTIMIZATION: Cache function declarations to avoid repeated lookups
static DenseMap<RuntimeFunction, func::FuncOp> runtimeFunctionCache;

func::FuncOp
ArtsCodegen::getOrCreateRuntimeFunction(types::RuntimeFunction FnID) {
  /// Check cache first
  auto cacheIt = runtimeFunctionCache.find(FnID);
  if (cacheIt != runtimeFunctionCache.end()) {
    return cacheIt->second;
  }

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
DataBlockCodegen *ArtsCodegen::getDb(Value op) {
  return getDb(cast<DbControlOp>(op.getDefiningOp()));
}

DataBlockCodegen *ArtsCodegen::getDb(arts::DbControlOp dbOp) {
  if (!dbOp)
    return nullptr;
  auto it = datablocks.find(dbOp);
  return (it != datablocks.end()) ? it->second : nullptr;
}

DataBlockCodegen *ArtsCodegen::createDb(arts::DbControlOp dbOp,
                                               Location loc) {
  assert(!getDb(dbOp) && "Db already exists");
  datablocks[dbOp] = new DataBlockCodegen(*this, dbOp, loc);
  return datablocks[dbOp];
}

DataBlockCodegen *ArtsCodegen::getOrCreateDb(arts::DbControlOp dbOp,
                                                    Location loc) {
  if (auto db = getDb(dbOp))
    return db;
  return createDb(dbOp, loc);
}

void ArtsCodegen::addDbDependency(Value dbGuid, Value edtGuid, Value edtSlot,
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

Value ArtsCodegen::getDbMode(StringRef mode, Location loc) {
  int enumValue = 10; // ARTS_DB_PIN (default)
  if (mode == "in")
    enumValue = 8; // ARTS_DB_READ
  else if (mode == "out")
    enumValue = 9; // ARTS_DB_WRITE (assuming this enum value)
  // inout uses default ARTS_DB_PIN
  
  return createIntConstant(enumValue, Int32, loc);
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
  auto zeroInt = createIntConstant(0, Int32, loc);
  auto guidValue = builder.create<LLVM::GEPOp>(loc, llvmPtr, ArtsEdtDep, dep,
                                               ValueRange{zeroInt, zeroInt});
  auto loadGuid =
      builder.create<LLVM::LoadOp>(loc, ArtsGuid, guidValue.getResult());
  return loadGuid.getResult();
}

Value ArtsCodegen::getPtrFromEdtDep(Value dep, Location loc) {
  auto gepOp =
      builder.create<LLVM::GEPOp>(loc, llvmPtr, ArtsEdtDep, dep,
                                  ValueRange{createIntConstant(0, Int32, loc),
                                             createIntConstant(2, Int32, loc)});
  return builder.create<LLVM::LoadOp>(loc, VoidPtr, gepOp.getResult());
}

Value ArtsCodegen::getCurrentEpochGuid(Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetCurrentEpochGuid, {}, loc)
      .getResult(0);
}

Value ArtsCodegen::getCurrentEdtGuid(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentGuid);
  assert(func && "Runtime function should exist");
  // Note: Do NOT mark as readnone - this returns runtime-dependent values
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);
  return callOp.getResult(0);
}

Value ArtsCodegen::getTotalWorkers(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetTotalWorkers);
  assert(func && "Runtime function should exist");
  // Mark as readnone since total workers is constant during execution
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);

  return callOp.getResult(0);
}

Value ArtsCodegen::getTotalNodes(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetTotalNodes);
  assert(func && "Runtime function should exist");
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);

  return callOp.getResult(0);
}

Value ArtsCodegen::getCurrentWorker(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentWorker);
  assert(func && "Runtime function should exist");
  // Mark as readnone - worker ID is constant within EDT execution for CSE optimization
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);

  return callOp.getResult(0);
}

Value ArtsCodegen::getCurrentNode(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentNode);
  assert(func && "Runtime function should exist");
  // Mark as readnone - node ID is constant within EDT execution for CSE optimization
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);
  return callOp.getResult(0);
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
  auto newFunc =
      builder.create<func::FuncOp>(loc, "initPerWorker", InitPerWorkerFn);
  newFunc.setPublic();
  module.push_back(newFunc);
  auto *entryBlock = newFunc.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);
  builder.create<func::ReturnOp>(loc);
  return newFunc;
}

func::FuncOp ArtsCodegen::insertInitPerNode(Location loc,
                                            func::FuncOp callback) {
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);

  /// Create the function using the InitPerNodeFn type.
  auto newFunc =
      builder.create<func::FuncOp>(loc, "initPerNode", InitPerNodeFn);
  newFunc.setPublic();
  module.push_back(newFunc);

  /// Create the entry block.
  Block *entryBlock = newFunc.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Retrieve the first argument and compare arg with 1.
  auto arg = entryBlock->getArgument(0);
  auto cmp = builder.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge,
                                           castToIndex(arg, loc),
                                           createIndexConstant(1, loc));

  /// Create separate then and merge blocks in the function body.
  auto thenBlock = new Block();
  auto mergeBlock = new Block();
  mergeBlock->addArgument(newFunc.getArgument(1).getType(), loc);
  mergeBlock->addArgument(newFunc.getArgument(2).getType(), loc);
  newFunc.getBody().push_back(thenBlock);
  newFunc.getBody().push_back(mergeBlock);

  /// In the entry block, do a conditional branch: if cmp true, jump to
  /// thenBlock; otherwise, continue in mergeBlock.
  builder.create<cf::CondBranchOp>(
      loc, cmp, thenBlock, ValueRange{}, mergeBlock,
      ValueRange{newFunc.getArgument(1), newFunc.getArgument(2)});

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
      callArgs = {newFunc.getArgument(1), newFunc.getArgument(2)};
    builder.create<func::CallOp>(loc, callback, callArgs);
  }
  createRuntimeCall(ARTSRTL_artsShutdown, {}, loc);
  builder.create<func::ReturnOp>(loc);
  return newFunc;
}

func::FuncOp ArtsCodegen::insertArtsMainFn(Location loc,
                                           func::FuncOp callback) {
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);

  /// Create the function using the "MainEdt" type.
  auto newFunc = builder.create<func::FuncOp>(loc, "artsMain", ArtsMainFn);
  newFunc.setPublic();
  module.push_back(newFunc);

  /// Create the entry block.
  auto *entryBlock = newFunc.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Insert call to 'artsRT' function
  auto callArgs = ValueRange{};
  if (callback.getNumArguments() > 0)
    callArgs = {newFunc.getArgument(0), newFunc.getArgument(1)};
  builder.create<func::CallOp>(loc, callback, callArgs);

  /// Return
  createRuntimeCall(ARTSRTL_artsShutdown, {}, loc);
  builder.create<func::ReturnOp>(loc);
  return newFunc;
}

func::FuncOp ArtsCodegen::insertMainFn(Location loc) {
  /// Save the current insertion point.
  OpBuilder::InsertionGuard IG(builder);
  setInsertionPoint(module);

  /// Create the function using the "MainFn" type.
  auto newFunc = builder.create<func::FuncOp>(loc, "main", MainFn);
  newFunc.setPublic();
  module.push_back(newFunc);

  /// Create the entry block.
  auto *entryBlock = newFunc.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

  /// Insert call to 'artsRT' function
  createRuntimeCall(ARTSRTL_artsRT,
                    {newFunc.getArgument(0), newFunc.getArgument(1)}, loc);

  /// Return 0
  auto zero =
      builder.create<arith::ConstantOp>(loc, builder.getI32IntegerAttr(0));
  builder.create<func::ReturnOp>(loc, zero.getResult());
  return newFunc;
}

void ArtsCodegen::initializeRuntime(Location loc) {
  auto mainFunc = module.lookupSymbol<func::FuncOp>("main");
  if (!mainFunc)
    return;

  /// Rename main function to "mainBody"
  mainFunc.setName("mainBody");

  /// Insert init functions
  insertArtsMainFn(loc, mainFunc);
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
  llvm::errs() << "Unsupported type for casting to integer: " << source << "\n";
  assert(false && "Unsupported type in castToInt");
  return nullptr; /// unreachable
}

Value ArtsCodegen::castToVoidPtr(Value source, Location loc) {
  auto valPtr = source;
  if (!valPtr.getType().isa<LLVM::LLVMPointerType>()) {
    valPtr = castToLLVMPtr(source, loc);
  }
  /// polygeist - pointer2memref
  return builder.create<polygeist::Pointer2MemrefOp>(loc, VoidPtr, valPtr);
}

Value ArtsCodegen::castToLLVMPtr(Value source, Location loc) {
  if (source.getType().isa<LLVM::LLVMPointerType>())
    return source;
  /// polygeist - memref2pointer
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
  /// global.
  if (llvmStringGlobals.find(value.str()) == llvmStringGlobals.end()) {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(module.getBody());
    auto type = LLVM::LLVMArrayType::get(builder.getI8Type(), value.size() + 1);

    /// Create a unique name for the global
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
  auto printfFunc = module.lookupSymbol<LLVM::LLVMFuncOp>("printf");
  if (!printfFunc) {
    /// Create proper printf function type with varargs
    auto i32Type = builder.getI32Type();
    auto i8PtrType = LLVM::LLVMPointerType::get(builder.getContext());
    auto printfType = LLVM::LLVMFunctionType::get(i32Type, {i8PtrType}, true);

    printfFunc = builder.create<LLVM::LLVMFuncOp>(loc, "printf", printfType);
    printfFunc.setLinkage(LLVM::Linkage::External);
  }
  assert(printfFunc && "printf function not found");

  // Get or create the format string global and cast it to a generic pointer
  auto formatStrPtr = getOrCreateGlobalLLVMString(loc, format);
  auto castedFormatPtr =
      builder.create<LLVM::BitcastOp>(loc, llvmPtr, formatStrPtr);

  /// Create the printf call with all arguments
  SmallVector<Value> callArgs;
  callArgs.push_back(castedFormatPtr);
  callArgs.append(args.begin(), args.end());

  // Create the call operation with a symbol reference
  builder.create<LLVM::CallOp>(
      loc, TypeRange{builder.getI32Type()},
      SymbolRefAttr::get(builder.getContext(), printfFunc.getName()), callArgs);
}