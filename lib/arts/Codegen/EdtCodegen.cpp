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

  /// Process the EDT: Computes depc, paramc, loads params in paramv
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

  /// Cache parameter insertions to avoid duplicates
  DenseMap<Value, unsigned> parameterCache;

  auto insertSizeAsParameter = [&](DbDepCodegen *dbDep, Value size,
                                   uint64_t sizeIdx) {
    if (isValueConstant(size))
      return;
    auto cacheIt = parameterCache.find(size);
    if (cacheIt != parameterCache.end()) {
      dbDep->getSizeIndexInEdt()[sizeIdx] = cacheIt->second;
      return;
    }

    auto paramPair = insertParam(size);
    parameterCache[size] = paramPair.second;
    dbDep->getSizeIndexInEdt()[sizeIdx] = paramPair.second;
  };

  auto insertOffsetAsParameter = [&](DbDepCodegen *dbDep, Value offset,
                                     uint64_t offsetIdx) {
    if (isValueConstant(offset))
      return;
    auto cacheIt = parameterCache.find(offset);
    if (cacheIt != parameterCache.end()) {
      dbDep->getOffsetIndexInEdt()[offsetIdx] = cacheIt->second;
      return;
    }

    auto paramPair = insertParam(offset);
    parameterCache[offset] = paramPair.second;
    dbDep->getOffsetIndexInEdt()[offsetIdx] = paramPair.second;
  };

  /// Process each dependency.
  if (!deps.empty()) {
    for (const auto &dep : deps) {
      /// Check if this is a DbDepOp (which we need for EDT context)
      if (auto dbDepOp = dep.getDefiningOp<arts::DbDepOp>()) {
        auto *dbDep = AC.getOrCreateDbDep(dbDepOp, loc);
        assert(dbDep && "DbDep not found");

        /// Set the EDT slot for the DB
        dbDep->setEdtSlot(
            builder.create<memref::LoadOp>(loc, depC).getResult());
        Value dbNumElements = nullptr;

        /// For single-dimensional DBs, use the size directly.
        dbNumElements = AC.createIndexConstant(1, loc);
        auto sizes = dbDepOp.getSizes();
        auto offsets = dbDepOp.getOffsets();
        auto rank = sizes.size();

        if (rank <= 1) {
          insertSizeAsParameter(dbDep, dbNumElements, 0);
        } else {
          /// For multi-dimensional DBs, compute the product of sizes.
          Value product = AC.createIndexConstant(1, loc);
          for (uint64_t rankItr = 0; rankItr < rank; ++rankItr) {
            insertSizeAsParameter(dbDep, sizes[rankItr], rankItr);
            insertOffsetAsParameter(dbDep, offsets[rankItr], rankItr);
            product =
                builder.create<arith::MulIOp>(loc, product, sizes[rankItr])
                    .getResult();
          }
          dbNumElements = product;
        }

        /// Input DBs are recorded, accumulate dependency count.
        if (dbDep->isInMode()) {
          auto currDep = builder.create<memref::LoadOp>(loc, depC);
          auto newDep =
              builder.create<arith::AddIOp>(loc, currDep, dbNumElements)
                  .getResult();
          builder.create<memref::StoreOp>(loc, newDep, depC);
          depsToRecord.push_back(dbDep);
        }

        /// Output DBs are satisfied
        if (dbDep->isOutMode())
          depsToSatisfy.push_back(dbDep);
      }
      /// Handle DbAllocOp if needed (though they typically don't need EDT
      /// processing)
      else if (auto dbAllocOp = dep.getDefiningOp<arts::DbAllocOp>()) {
        auto *dbAlloc = AC.getOrCreateDbAlloc(dbAllocOp, loc);
        // DbAlloc ops don't typically need EDT context processing
        // They are handled directly when used
      }
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
    auto idx = AC.createIndexConstant(i, loc);
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    builder.create<memref::StoreOp>(loc, param, paramV, ValueRange{idx});
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
      auto dbLoc = dbCG->getOp().getDefiningOp()->getLoc();

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

      /// Pre-compute loop bounds and cache step constants
      SmallVector<std::pair<Value, Value>> precomputedBounds;
      precomputedBounds.reserve(dbRank);
      for (unsigned i = 0; i < dbRank; ++i) {
        auto lowerBound = dbOffsets[i];
        auto upperBound =
            builder.create<arith::AddIOp>(dbLoc, lowerBound, dbSizes[i]);
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
              auto nextSlot = builder
                                  .create<arith::AddIOp>(dbLoc, currentSlot,
                                                         incrementConstant)
                                  .getResult();
              builder.create<memref::StoreOp>(dbLoc, nextSlot, inSlotAlloc);
              return;
            }
            /// Use pre-computed bounds
            auto [lowerBound, upperBound] = precomputedBounds[dim];
            auto loopOp = builder.create<scf::ForOp>(dbLoc, lowerBound,
                                                     upperBound, stepConstant);
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
      auto dbLoc = dbCG->getOp().getDefiningOp()->getLoc();

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
    auto *dbAlloc = AC.getDbAlloc(dep);
    auto *dbDep = AC.getDbDep(dep);

    if (dbAlloc) {
      if (!dbAlloc->getPtr()) {
        LLVM_DEBUG(dbgs() << "DbAlloc not found or pointer not set\n");
        continue;
      }
      dbAlloc->getOp().replaceAllUsesWith(dbAlloc->getPtr());
    } else if (dbDep) {
      if (!dbDep->getPtr()) {
        LLVM_DEBUG(dbgs() << "DbDep not found or pointer not set\n");
        continue;
      }
      dbDep->getOp().replaceAllUsesWith(dbDep->getPtr());
    }
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
  auto reserveGuidCall = AC.createRuntimeCall(
      types::ARTSRTL_artsReserveGuidRoute, {guidEdtType, node}, loc);
  return reserveGuidCall.getResult(0);
}

func::FuncOp EdtCodegen::createFn(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(AC.getModule());
  auto edtFuncName = "__arts_edt_" + std::to_string(increaseEdtCounter());
  auto edtFuncOp = builder.create<func::FuncOp>(loc, edtFuncName, AC.EdtFn);
  edtFuncOp.setPrivate();
  /// Don't manually push_back - builder.create already adds it to the module
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
    auto *db = AC.getDbDep(dep);
    assert(db && "Db not found");

    /// Skip DBs that are not in mode.
    if (!db->isInMode())
      continue;

    /// Iterate over uses to see if another DB is using this one. If
    /// so, set the guid and ptr for the other DB.
    auto updateUserDb = [&](Value entryGuid, Value entryPtr) {
      for (auto *user : db->getOp().getDefiningOp()->getUsers()) {
        /// We are only concerned with datablocks in the same EDT region.
        if (!region->isAncestor(user->getParentRegion()))
          continue;
        /// If not a db, skip.
        if (!arts::isDbDepOp(user))
          continue;
        /// Get the db codegen and set the entry information.
        if (auto userDbDepOp = dyn_cast<arts::DbDepOp>(user)) {
          auto *userDb = AC.getOrCreateDbDep(userDbDepOp, userDbDepOp.getLoc());
          if (userDb) {
            LLVM_DEBUG(dbgs() << "Setting dependent DB attributes from depv: "
                              << userDbDepOp << "\n");
            userDb->setGuidInEdt(entryGuid);
            userDb->setPtrInEdt(entryPtr);
          }
        }
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

      /// Set EDT context attributes once!
      db->setGuidInEdt(entryGuid);
      db->setPtrInEdt(entryPtr);

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
    auto *dbDep = static_cast<DbDepCodegen *>(db);
    auto &entrySizes = dbDep->getSizesInEdt();
    auto &entryOffsets = dbDep->getOffsetsInEdt();
    entrySizes.reserve(dbRank);
    entryOffsets.reserve(dbRank);
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
      else if (auto cstOp =
                   dbOffsets[i].getDefiningOp<arith::ConstantIndexOp>())
        entryOffsets.push_back(builder.clone(*cstOp)->getResult(0));
      else
        llvm_unreachable("Db offset is not a constant");
    }

    /// CLEAN APPROACH: Use memref.view to create N-dimensional views from flat
    /// fnDepV. This maintains the original multi-dimensional structure while
    /// avoiding reallocation.

    auto curIndex = loadIndex();

    /// Compute total elements and advance the index
    Value totalElements = AC.createIndexConstant(1, loc);
    for (auto size : entrySizes) {
      totalElements = builder.create<arith::MulIOp>(loc, totalElements, size);
    }
    auto nextIndex =
        builder.create<arith::AddIOp>(loc, curIndex, totalElements);
    builder.create<memref::StoreOp>(loc, nextIndex, indexAlloc);

    /// Cast fnDepV to a flat i8 buffer (required by memref.view)
    auto flatI8Type =
        MemRefType::get({ShapedType::kDynamic}, AC.getBuilder().getI8Type());
    auto fnDepVPtr_i8 = AC.castToLLVMPtr(fnDepV, loc);
    auto flatBuffer = builder.create<polygeist::Pointer2MemrefOp>(
        loc, flatI8Type, fnDepVPtr_i8);

    /// Compute byte offset for this db section
    auto offsetInBytes =
        builder.create<arith::MulIOp>(loc, curIndex, depStructSize);

    /// Create target memref types with original dimensions (keeping offsets &
    /// sizes!)
    auto guidViewType = MemRefType::get(
        SmallVector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.ArtsGuid);
    auto ptrViewType = MemRefType::get(
        SmallVector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.VoidPtr);

    /// Use memref.view to create N-dimensional views maintaining original
    /// structure! GUID view: offset points to guid field (first field in
    /// artsEdtDep_t)
    auto entryGuid = builder.create<memref::ViewOp>(
        loc, guidViewType, flatBuffer, offsetInBytes, entrySizes);

    /// PTR view: Create a view that directly depes the .ptr field from each
    /// artsEdtDep_t The stride between elements should be sizeof(artsEdtDep_t),
    /// and we offset to the .ptr field
    auto ptrFieldOffset = AC.createIndexConstant(
        16, loc); /// Offset to .ptr field in artsEdtDep_t
    auto ptrByteOffset =
        builder.create<arith::AddIOp>(loc, offsetInBytes, ptrFieldOffset);
    auto entryPtr = builder.create<memref::ViewOp>(loc, ptrViewType, flatBuffer,
                                                   ptrByteOffset, entrySizes);

    /// Store the views - now we preserve the original multi-dimensional
    /// structure!
    db->setGuidInEdt(entryGuid);
    db->setPtrInEdt(entryPtr);

    /// For updateUserDb and rewireMap, we need to provide the actual .ptr field
    /// dep Get the first element as representative and extract its .ptr
    /// field
    SmallVector<Value> zeroIndices(entrySizes.size(),
                                   AC.createIndexConstant(0, loc));
    auto firstGuid =
        builder.create<memref::LoadOp>(loc, entryGuid, zeroIndices);
    auto firstPtr = builder.create<memref::LoadOp>(loc, entryPtr, zeroIndices);

    /// Set EDT context attributes once!
    db->setGuidInEdt(firstGuid);
    db->setPtrInEdt(firstPtr);

    updateUserDb(firstGuid, firstPtr);
    /// Replace uses with the actual ptr field from depv (not the whole
    /// structure)
    rewireMap[db->getOp()] = firstPtr;
  }

  /// Ensure all DB operations within the EDT region are properly rewired
  /// This includes dependent DbDepOps that might not be direct dependencies
  rewireAllDbsInRegion();

  /// Replace all uses in the region.
  replaceInRegion(*region, rewireMap, false);
}

void EdtCodegen::rewireAllDbsInRegion() {
  if (!region)
    return;

  LLVM_DEBUG(dbgs() << "Rewiring all DB operations in EDT region\n");

  /// Walk through all operations in the region to find DB operations
  region->walk([&](Operation *op) {
    /// Handle DbAllocOp
    if (auto dbAllocOp = dyn_cast<arts::DbAllocOp>(op)) {
      Value dbResult = dbAllocOp.getResult();

      /// Check if this DB is already in rewireMap (as a dependency)
      if (rewireMap.find(dbResult) == rewireMap.end()) {
        /// This DB is not a direct dependency, but we still need to handle it
        /// Look for it in the deps list to see if it's there indirectly
        for (auto &dep : deps) {
          auto *dbAlloc = AC.getDbAlloc(dep);
          if (dbAlloc && dbAlloc->getOp() == dbResult) {
            LLVM_DEBUG(dbgs()
                       << "Found DbAlloc in deps, should be rewired already\n");
            break;
          }
        }
      }
    }

    /// Handle DbDepOp
    else if (auto dbDepOp = dyn_cast<arts::DbDepOp>(op)) {
      Value dbResult = dbDepOp.getResult();

      /// Check if this DB is already in rewireMap
      if (rewireMap.find(dbResult) == rewireMap.end()) {
        /// This DbDep is not directly rewired yet
        /// Check if its source has EDT context attributes set
        Value source = dbDepOp.getSource();
        auto *sourceDb = AC.getDbDep(source);

        if (sourceDb && sourceDb->getPtrInEdt()) {
          /// Source has EDT context values, use them for dependent DB
          LLVM_DEBUG(dbgs() << "Rewiring dependent DbDep using attributes: "
                            << dbDepOp << "\n");
          auto *depDb = AC.getDbDep(dbResult);
          if (depDb) {
            depDb->setGuidInEdt(sourceDb->getGuidInEdt());
            depDb->setPtrInEdt(sourceDb->getPtrInEdt());
            rewireMap[dbResult] = sourceDb->getPtrInEdt();
          }
        } else {
          LLVM_DEBUG(dbgs() << "ERROR: DbDep source has no EDT context: "
                            << dbDepOp << "\n");
        }
      }
    }
  });

  /// Replace all uses in the region.
  replaceInRegion(*region, rewireMap, false);
}

/// Getter and setter implementations
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
