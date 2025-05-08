///==========================================================================
/// File: ArtsCodegen.cpp
///==========================================================================

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
#include "arts/Analysis/EdtAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
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

using namespace mlir;
using namespace mlir::func;
using namespace mlir::LLVM;
using namespace mlir::arts;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::cf;

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

  /// If the base is a DB, it will be handled when inserting the EDT Entry
  if (hasPtrDb())
    return;

  /// Set insertion point to the DatablockOp
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);

  auto currentNode = AC.getCurrentNode(loc);
  auto elementTypeSize = dbOp.getElementTypeSize();
  const auto tySize = AC.castToInt(AC.Int64, elementTypeSize, loc);

  /// Handle the case of a single datablock
  if (hasSingleSize()) {
    auto modeVal = getMode(dbOp.getMode());
    guid = createGuid(currentNode, modeVal, loc);
    /// Allocate a single pointer by directly assigning the runtime call result
    ptr =
        AC.createRuntimeCall(ARTSRTL_artsDbCreateWithGuid, {guid, tySize}, loc)
            ->getResult(0);
    return;
  }

  /// Create an array of guids and pointers based on the sizes of the dbOp
  auto dbSizes = dbOp.getSizes();
  const auto dbDim = dbSizes.size();
  auto modeVal = getMode(dbOp.getMode());
  auto guidType = MemRefType::get(
      std::vector<int64_t>(dbDim, ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, dbSizes);
  auto ptrType = dbOp.getResult().getType().cast<MemRefType>();
  if (ptrType.hasStaticShape()) {
    ptr = builder.create<memref::AllocaOp>(loc, ptrType);
  } else {
    ptr = builder.create<memref::AllocaOp>(loc, ptrType, dbSizes);
  }

  /// Recursively create datablocks for each element
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
        /// Create loop for current dimension
        auto lowerBound = AC.createIndexConstant(0, loc);
        auto upperBound = dbSizes[dim];
        auto step = AC.createIndexConstant(1, loc);
        auto loopOp =
            builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);
        /// Set insertion point inside the loop body
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        /// Append the induction variable for the current dimension
        indices.push_back(loopOp.getInductionVar());
        /// Recurse to create the next level of loop
        createDbs(dim + 1, indices);
        /// Remove the current induction variable and reset insertion point
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

// ---------------------------- Events ---------------------------- ///
EventCodegen::EventCodegen(ArtsCodegen &AC, arts::EventOp eventOp, Location loc)
    : AC(AC), builder(AC.builder), eventOp(eventOp) {
  assert(eventOp && "EventOp must be provided");
  /// Create the event
  create(loc);
}

void EventCodegen::create(Location loc) {
  /// Create a single persistent event with latchCount = 0
  auto createEvent = [&](Value node, Location loc) -> Value {
    Value latchCount = AC.createIntConstant(0, AC.Int32, loc);
    dataGuid = AC.createIntConstant(0, AC.Int64, loc);
    return AC
        .createRuntimeCall(ARTSRTL_artsPersistentEventCreate,
                           {node, latchCount, dataGuid}, loc)
        .getResult(0);
  };

  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(eventOp);
  auto node = AC.getCurrentNode(loc);

  /// If the event is a single event, create it and return
  if (eventOp.hasSingleSize()) {
    guid = createEvent(node, loc);
    return;
  }

  /// Create an array of guids based on the sizes of the eventOp
  auto sizes = eventOp.getSizes();
  auto guidType = MemRefType::get(
      std::vector<int64_t>(sizes.size(), ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, sizes);

  /// Create nested loops to create the guids
  std::function<void(unsigned, SmallVector<Value, 4> &)> createGuids =
      [&](unsigned dim, SmallVector<Value, 4> &indices) {
        if (dim == sizes.size()) {
          auto guidVal = createEvent(node, loc);
          builder.create<memref::StoreOp>(loc, guidVal, guid, indices);
          return;
        }
        /// Create loop for current dimension
        auto lowerBound = AC.createIndexConstant(0, loc);
        auto upperBound = sizes[dim];
        auto step = AC.createIndexConstant(1, loc);
        auto loopOp =
            builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);
        /// Set insertion point inside the loop body
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        /// Append the induction variable for the current dimension
        indices.push_back(loopOp.getInductionVar());
        /// Recurse to create the next level of loop
        createGuids(dim + 1, indices);
        /// Remove the current induction variable and reset insertion point
        indices.pop_back();
        builder.setInsertionPointAfter(loopOp);
      };
  SmallVector<Value, 4> indices;
  createGuids(0, indices);
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
  createFnEntry(loc);
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
  auto totalEventsToSatisfy =
      builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, zeroConst, totalEventsToSatisfy);

  /// Allocate dependency count (depC) and initialize to zero.
  depC = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, zeroConst, depC);

  /// Insert a dynamic size as a parameter if it is non-constant.
  auto insertSizeAsParameter = [&](DataBlockCodegen *db, Value size,
                                   uint64_t sizeIdx) {
    if (isValueConstant(size))
      return;
    auto paramPair = insertParam(size);
    entryDbs[db].sizeIndex[sizeIdx] = paramPair.second;
  };

  /// Insert a dynamic offset as a parameter if it is non-constant.
  auto insertOffsetAsParameter = [&](DataBlockCodegen *db, Value offset,
                                     uint64_t offsetIdx) {
    if (isValueConstant(offset))
      return;
    auto paramPair = insertParam(offset);
    entryDbs[db].offsetIndex[offsetIdx] = paramPair.second;
    ;
  };

  /// Process a datablock event dependency
  auto processEvent = [&](DataBlockCodegen *db, Value &dbNumElements) {
    /// Datablocks with 'in' events will be recorded
    if (db->isInMode()) {
      assert(db->getInEvent() && "Datablock missing 'in' event");
      depsToRecord.push_back(db);
    }
    /// Datablocks with 'out' events will be satisfied
    if (db->getOutEvent() && db->isOutMode()) {
      depsToSatisfy.push_back(db);
      auto currEvents =
          builder.create<memref::LoadOp>(loc, totalEventsToSatisfy.getResult());
      auto newEvents =
          builder.create<arith::AddIOp>(loc, currEvents, dbNumElements)
              .getResult();
      builder.create<memref::StoreOp>(loc, newEvents, totalEventsToSatisfy);
    }
  };

  /// Process each dependency.
  if (!deps.empty()) {
    for (const auto &dep : deps) {
      auto db = AC.getDatablock(dep);
      assert(db && "Datablock not found");

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
        auto sizes = db->getSizes();
        auto offsets = db->getOffsets();
        auto rank = sizes.size();
        for (uint64_t rankItr = 0; rankItr < rank; ++rankItr) {
          insertSizeAsParameter(db, sizes[rankItr], rankItr);
          insertOffsetAsParameter(db, offsets[rankItr], rankItr);
          product = builder.create<arith::MulIOp>(loc, product, sizes[rankItr])
                        .getResult();
        }
        dbNumElements = product;
      }

      /// For input DBs, accumulate dependency count.
      if (db->isInMode()) {
        auto currDep = builder.create<memref::LoadOp>(loc, depC);
        auto newDep = builder.create<arith::AddIOp>(loc, currDep, dbNumElements)
                          .getResult();
        builder.create<memref::StoreOp>(loc, newDep, depC);
      }

      /// Process events for the given DB
      processEvent(db, dbNumElements);
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

  /// If events exist, add their count to the parameter count.
  if (!depsToSatisfy.empty()) {
    auto loadedParamC = builder.create<memref::LoadOp>(loc, paramC);
    auto totalEventsVal =
        builder.create<memref::LoadOp>(loc, totalEventsToSatisfy.getResult());
    auto totalParamC =
        builder.create<arith::AddIOp>(loc, loadedParamC, totalEventsVal)
            .getResult();
    builder.create<memref::StoreOp>(loc, totalParamC, paramC);
  }

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

  /// If no events need to be satisfied, we are done.
  if (depsToSatisfy.empty())
    return;

  /// Allocate a running index for event parameters starting from the static
  /// count.
  auto paramIndex = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(
      loc, AC.createIndexConstant(staticParamCount, loc), paramIndex);

  /// Insert event GUIDs into paramV.
  LLVM_DEBUG(dbgs() << "Inserting event GUIDs into paramV\n");
  for (auto db : depsToSatisfy) {
    auto eventOp = dyn_cast<arts::EventOp>(db->getOutEvent().getDefiningOp());
    auto eventCG = AC.getEvent(eventOp);
    assert(eventCG && "Event not found");

    if (eventOp.hasSingleSize()) {
      auto curParamIdx =
          builder.create<memref::LoadOp>(loc, paramIndex.getResult());
      builder.create<memref::StoreOp>(loc, eventCG->getGuid(), paramV,
                                      ValueRange{curParamIdx});
      auto newParamIdx =
          builder
              .create<arith::AddIOp>(loc, curParamIdx,
                                     AC.createIndexConstant(1, loc))
              .getResult();
      builder.create<memref::StoreOp>(loc, newParamIdx, paramIndex);
    } else {
      /// Recursive function to insert all GUIDs for multi-dimensional events.
      const auto dbSizes = db->getSizes();
      const auto dbOffsets = db->getOffsets();
      const auto dbRank = dbSizes.size();
      std::function<void(unsigned, SmallVector<Value, 4> &)> createEvents =
          [&](unsigned dim, SmallVector<Value, 4> &indices) {
            if (dim == dbRank) {
              auto eventGuid = builder.create<memref::LoadOp>(
                  loc, eventCG->getGuid(), indices);
              auto curParamIdx =
                  builder.create<memref::LoadOp>(loc, paramIndex.getResult());
              builder.create<memref::StoreOp>(loc, eventGuid, paramV,
                                              ValueRange{curParamIdx});
              auto newParamIdx =
                  builder
                      .create<arith::AddIOp>(loc, curParamIdx,
                                             AC.createIndexConstant(1, loc))
                      .getResult();
              builder.create<memref::StoreOp>(loc, newParamIdx, paramIndex);
              return;
            }
            auto lowerBound = dbOffsets[dim];
            auto upperBound =
                builder.create<arith::AddIOp>(loc, lowerBound, dbSizes[dim]);
            auto step = AC.createIndexConstant(1, loc);
            auto loopOp =
                builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);
            auto &loopBody = loopOp.getRegion().front();
            builder.setInsertionPointToStart(&loopBody);
            indices.push_back(loopOp.getInductionVar());
            createEvents(dim + 1, indices);
            indices.pop_back();
            builder.setInsertionPointAfter(loopOp);
          };

      SmallVector<Value, 4> indices = db->getIndices();
      createEvents(0, indices);
    }
  }
  LLVM_DEBUG(dbgs() << "Inserted event GUIDs into paramV\n");
}

void EdtCodegen::processDependencies(Location loc) {
  LLVM_DEBUG(DBGS() << "Processing dependencies for EDT\n" << func << "\n");

  /// Set the insertion point at the EDT function return to satisfy
  /// dependencies.
  builder.setInsertionPoint(returnOp);
  auto unknownLoc = UnknownLoc::get(builder.getContext());
  const auto indexType = IndexType::get(builder.getContext());
  const auto indexMemRefType = MemRefType::get({}, indexType);

  /// ---------------------------------------------------------------------
  /// Satisfy Out-Mode Dependencies
  /// Decrement the event latch counts for all out-mode (write) datablock
  /// dependencies. This is done in the EDT Function, so we need to use the
  /// datablock GUIDs from the entryDbs map to access the correct datablock
  /// pointers and sizes.
  /// ---------------------------------------------------------------------
  LLVM_DEBUG(dbgs() << "- Satisfying out-mode dependencies: "
                    << depsToSatisfy.size() << "\n");
  if (!depsToSatisfy.empty()) {
    auto eventSlotAlloc =
        builder.create<memref::AllocaOp>(loc, indexMemRefType);
    const auto initialEventSlot = AC.createIndexConstant(params.size(), loc);
    builder.create<memref::StoreOp>(loc, initialEventSlot, eventSlotAlloc);
    auto fnParamVPtr = AC.castToLLVMPtr(fnParamV, loc);

    for (auto *dbCG : depsToSatisfy) {
      assert(dbCG->getOutEvent() && "Datablock missing out event");

      /// For single-dimension datablocks, satisfy dependency directly.
      if (dbCG->hasSingleSize()) {
        auto currentSlot =
            builder.create<memref::LoadOp>(loc, eventSlotAlloc.getResult());
        auto eventGuidAtSlot = builder.create<polygeist::SubIndexOp>(
            loc, MemRefType::get({}, AC.ArtsGuid), fnParamV, currentSlot);
        auto loadedEventGuid =
            builder.create<memref::LoadOp>(loc, eventGuidAtSlot.getResult())
                .getResult();
        AC.decrementEventLatchCount(loadedEventGuid, entryDbs[dbCG].guid,
                                    unknownLoc);

        /// Increment the parameter slot.
        auto newSlot = builder
                           .create<arith::AddIOp>(
                               loc, currentSlot, AC.createIndexConstant(1, loc))
                           .getResult();
        builder.create<memref::StoreOp>(loc, newSlot, eventSlotAlloc);
        continue;
      }

      /// For multidimensional datablocks, satisfy dependencies recursively for
      /// each element.
      const auto &dbSizes = entryDbs[dbCG].sizes;
      const unsigned dbRank = dbSizes.size();

      std::function<Value(unsigned, SmallVector<Value, 4> &, Value)>
          satisfyRecursive = [&](unsigned dim, SmallVector<Value, 4> &indices,
                                 Value curSlot) -> Value {
        if (dim == dbRank) {
          auto loadedSlot = builder.create<memref::LoadOp>(loc, curSlot);
          auto slotInt = AC.castToInt(AC.Int32, loadedSlot, loc);
          auto eventGuidPtr =
              builder
                  .create<LLVM::GEPOp>(loc, AC.llvmPtr, AC.ArtsGuid,
                                       fnParamVPtr, ValueRange{slotInt})
                  ->getResult(0);
          auto loadedEventGuid =
              builder.create<LLVM::LoadOp>(loc, AC.ArtsGuid, eventGuidPtr)
                  .getResult();

          /// Load the datablock GUID from the entry using the current indices.
          auto loadedDbGuid = entryDbs[dbCG].guid;
          for (auto idx : indices) {
            auto mt = loadedDbGuid.getType().cast<MemRefType>();
            auto shape = std::vector<int64_t>(mt.getShape());
            shape.erase(shape.begin());
            auto mt0 = mlir::MemRefType::get(shape, mt.getElementType(),
                                             MemRefLayoutAttrInterface(),
                                             mt.getMemorySpace());
            loadedDbGuid = builder.create<polygeist::SubIndexOp>(
                loc, mt0, loadedDbGuid, idx);
          }
          loadedDbGuid =
              builder.create<memref::LoadOp>(loc, loadedDbGuid).getResult();

          /// Satisfy the event dependency by decrementing the latch count.
          AC.decrementEventLatchCount(loadedEventGuid, loadedDbGuid, loc);

          /// Increment the slot value.
          auto newSlot =
              builder
                  .create<arith::AddIOp>(loc, loadedSlot,
                                         AC.createIndexConstant(1, loc))
                  .getResult();
          builder.create<memref::StoreOp>(loc, newSlot, curSlot);
          return curSlot;
        }

        auto lowerBound = AC.createIndexConstant(0, loc);
        auto upperBound = dbSizes[dim];
        auto step = AC.createIndexConstant(1, loc);
        auto loopOp = builder.create<scf::ForOp>(loc, lowerBound, upperBound,
                                                 step, curSlot);
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        indices.push_back(loopBlock.getArgument(0));
        Value newSlot =
            satisfyRecursive(dim + 1, indices, loopBlock.getArgument(1));
        indices.pop_back();
        builder.create<scf::YieldOp>(loc, newSlot);
        builder.setInsertionPointAfter(loopOp);
        return loopOp.getResult(0);
      };

      SmallVector<Value, 4> indices;
      satisfyRecursive(0, indices, eventSlotAlloc);
    }
  }

  /// ---------------------------------------------------------------------
  /// Record In-Mode Dependencies
  /// After the EDT is created, add dependencies for all in-mode (read)
  /// datablocks.
  /// ---------------------------------------------------------------------
  LLVM_DEBUG(dbgs() << "- Recording in-mode dependencies: "
                    << depsToRecord.size() << "\n");
  builder.setInsertionPointAfter(guid.getDefiningOp());
  for (auto *dbCG : depsToRecord) {
    /// Ensure the datablock has a valid in-event and retrieve the event
    const auto inEvent = dbCG->getInEvent();
    assert(inEvent && "Datablock missing in event");
    EventCodegen *eventCG =
        AC.getEvent(dyn_cast<arts::EventOp>(inEvent.getDefiningOp()));
    assert(eventCG && "Event not found");
    auto eventGuid = eventCG->getGuid();

    /// Retrieve the datablock GUID and location.
    auto dbGuid = dbCG->getGuid();
    auto dbLoc = dbCG->getOp().getLoc();

    /// For single-dimension datablocks, add the dependency directly.
    if (dbCG->hasSingleSize()) {
      auto dbIndices = dbCG->getIndices();
      auto loadedEventGuid =
          builder.create<memref::LoadOp>(dbLoc, eventGuid, dbIndices);
      if (!dbIndices.empty())
        dbGuid = builder.create<memref::LoadOp>(dbLoc, dbGuid, dbIndices);
      AC.addEventDependency(loadedEventGuid, guid, dbCG->getEdtSlot(), dbGuid,
                            dbLoc);
      continue;
    }

    /// For multidimensional datablocks, add dependencies for each element.
    const auto dbSizes = dbCG->getSizes();
    const auto dbOffsets = dbCG->getOffsets();
    const auto dbIndices = dbCG->getIndices();
    const unsigned dbRank = dbSizes.size();
    auto inSlotAlloc = builder.create<memref::AllocOp>(dbLoc, indexMemRefType);
    builder.create<memref::StoreOp>(dbLoc, dbCG->getEdtSlot(), inSlotAlloc);

    std::function<void(unsigned, SmallVector<Value, 4> &)>
        addDependenciesRecursive = [&](unsigned dim,
                                       SmallVector<Value, 4> &indices) {
          if (dim == dbRank) {
            auto loadedEventGuid =
                builder.create<memref::LoadOp>(dbLoc, eventGuid, indices);
            auto currentSlot =
                builder.create<memref::LoadOp>(dbLoc, inSlotAlloc.getResult());
            SmallVector<Value> loadedDbIndices(dbIndices);
            loadedDbIndices.append(indices.begin(), indices.end());
            auto loadedDbGuid =
                builder.create<memref::LoadOp>(dbLoc, dbGuid, loadedDbIndices);
            AC.addEventDependency(loadedEventGuid, guid, currentSlot,
                                  loadedDbGuid, dbLoc);
            /// Increment the slot for the next dependency.
            auto nextSlot =
                builder
                    .create<arith::AddIOp>(dbLoc, currentSlot,
                                           AC.createIndexConstant(1, loc))
                    .getResult();
            builder.create<memref::StoreOp>(dbLoc, nextSlot, inSlotAlloc);
            return;
          }
          auto lowerBound = dbOffsets[dim];
          auto upperBound =
              builder.create<arith::AddIOp>(dbLoc, lowerBound, dbSizes[dim]);
          auto step = builder.create<arith::ConstantIndexOp>(dbLoc, 1);
          auto loopOp =
              builder.create<scf::ForOp>(dbLoc, lowerBound, upperBound, step);
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

  /// ---------------------------------------------------------------------
  /// Increment Latch Counts for Out-Mode Dependencies
  /// ---------------------------------------------------------------------
  LLVM_DEBUG(dbgs() << "- Incrementing latch counts for out-mode dependencies: "
                    << depsToSatisfy.size() << "\n");
  if (!depsToSatisfy.empty()) {
    for (auto *dbCG : depsToSatisfy) {
      /// Retrieve the associated event
      Value dbEvent = dbCG->getOutEvent();
      assert(dbEvent && "Datablock missing out event");
      EventCodegen *eventCG =
          AC.getEvent(dyn_cast<arts::EventOp>(dbEvent.getDefiningOp()));
      assert(eventCG && "Event not found");
      auto eventGuid = eventCG->getGuid();

      /// Retrieve the datablock GUID
      auto dbGuid = dbCG->getGuid();
      assert(dbGuid && "Datablock GUID not found");
      auto dbLoc = dbCG->getOp().getLoc();

      /// For single-dimension datablocks.
      if (dbCG->hasSingleSize()) {
        auto dbIndices = dbCG->getIndices();
        auto loadedEventGuid =
            builder.create<memref::LoadOp>(dbLoc, eventGuid, dbIndices);
        if (!dbIndices.empty())
          dbGuid = builder.create<memref::LoadOp>(dbLoc, dbGuid, dbIndices);
        AC.incrementEventLatchCount(loadedEventGuid, dbGuid, dbLoc);
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
              auto loadedEventGuid =
                  builder.create<memref::LoadOp>(dbLoc, eventGuid, indices);
              SmallVector<Value> loadedDbIndices(dbIndices);
              loadedDbIndices.append(indices.begin(), indices.end());
              auto loadedDbGuid = builder.create<memref::LoadOp>(
                  dbLoc, dbGuid, loadedDbIndices);
              AC.incrementEventLatchCount(loadedEventGuid, loadedDbGuid, dbLoc);
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
  /// datablock pointers.
  /// ---------------------------------------------------------------------
  LLVM_DEBUG(dbgs() << "- Replacing EDT dependency uses\n");
  for (auto &dep : deps) {
    auto *db = AC.getDatablock(dep);
    assert(db && "Datablock not found");
    if (!db->getPtr()) {
      LLVM_DEBUG(dbgs() << "Datablock not found or pointer not set\n");
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

void EdtCodegen::createFnEntry(Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  /// Create an entry block and remove the terminator.
  auto *entryBlock = &func.getBody().front();
  entryBlock->getTerminator()->erase();

  /// Get references to the block arguments.
  builder.setInsertionPointToStart(entryBlock);
  fnParamV = entryBlock->getArgument(1);
  fnDepV = entryBlock->getArgument(3);

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
    auto *db = AC.getDatablock(dep);
    assert(db && "Datablock not found");

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
        /// If not a datablock, skip.
        auto userOp = dyn_cast<arts::DataBlockOp>(user);
        if (!userOp)
          continue;
        /// Get the datablock codegen and set the entry information.
        auto userDb = AC.getDatablock(userOp);
        assert(userDb && "User datablock not found");
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
        llvm_unreachable("Datablock size is not a constant");

      /// Offsets
      if (entryDbs[db].offsetIndex.count(i))
        entryOffsets.push_back(rewireMap[params[entryDbs[db].offsetIndex[i]]]);
      else if (auto cstOp =
                   dbOffsets[i].getDefiningOp<arith::ConstantIndexOp>())
        entryOffsets.push_back(builder.clone(*cstOp)->getResult(0));
      else
        llvm_unreachable("Datablock offset is not a constant");
    }

    /// Allocate arrays for GUIDs and pointers.
    auto guidType = MemRefType::get(
        std::vector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.ArtsGuid);
    /// TODO: We might need to allocate dynamic memory for the entryGuid and
    /// entryPtr
    auto entryGuid =
        builder.create<memref::AllocaOp>(loc, guidType, entrySizes);
    auto ptrType = MemRefType::get(
        std::vector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.VoidPtr);
    auto entryPtr = builder.create<memref::AllocaOp>(loc, ptrType, entrySizes);

    /// Load the DB entry info for each element.
    std::function<void(unsigned, SmallVector<Value, 4> &)> createDbs =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == entrySizes.size()) {
            auto curIndex = loadIndex();
            auto curGep = AC.castToInt(
                AC.Int32,
                builder.create<arith::MulIOp>(loc, curIndex, depStructSize),
                loc);
            auto depVElem =
                builder
                    .create<LLVM::GEPOp>(loc, AC.llvmPtr, AC.PtrSize, fnDepVPtr,
                                         ValueRange{curGep})
                    .getResult();
            auto entryGuidElem = AC.getGuidFromEdtDep(depVElem, loc);
            auto entryPtrElem = AC.getPtrFromEdtDep(depVElem, loc);
            builder.create<memref::StoreOp>(loc, entryGuidElem, entryGuid,
                                            indices);
            builder.create<memref::StoreOp>(loc, entryPtrElem, entryPtr,
                                            indices);
            /// Increment the index.
            auto newIndex =
                builder.create<arith::AddIOp>(loc, curIndex, oneConst)
                    .getResult();
            builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
            return;
          }

          /// Create loop for current dimension.
          auto lowerBound = AC.createIndexConstant(0, loc);
          auto upperBound = entrySizes[dim];
          auto loopOp =
              builder.create<scf::ForOp>(loc, lowerBound, upperBound, oneConst);
          auto &loopBlock = loopOp.getRegion().front();
          builder.setInsertionPointToStart(&loopBlock);
          indices.push_back(loopOp.getInductionVar());
          createDbs(dim + 1, indices);
          indices.pop_back();
          builder.setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> indices;
    createDbs(0, indices);
    entryDbs[db].guid = entryGuid;
    entryDbs[db].ptr = entryPtr;

    /// Update the datablock in the region.
    updateUserDb(entryGuid, entryPtr);
    rewireMap[db->getOp()] = entryPtr;
  }

  /// Replace all uses in the region.
  replaceInRegion(*region, rewireMap, false);
}

/// ---------------------------- ARTS Codegen ---------------------------- ///
ArtsCodegen::ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
                         mlir::DataLayout &mlirDL)
    : module(module), builder(OpBuilder(module->getContext())), llvmDL(llvmDL),
      mlirDL(mlirDL) {
  initializeTypes();
}

ArtsCodegen::~ArtsCodegen() {
  for (auto &db : datablocks)
    delete db.second;
  for (auto &event : events)
    delete event.second;
  for (auto &edt : edts)
    delete edt.second;
}

func::FuncOp
ArtsCodegen::getOrCreateRuntimeFunction(types::RuntimeFunction FnID) {
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
DataBlockCodegen *ArtsCodegen::getDatablock(Value op) {
  return getDatablock(cast<DataBlockOp>(op.getDefiningOp()));
}

DataBlockCodegen *ArtsCodegen::getDatablock(arts::DataBlockOp dbOp) {
  if (!dbOp)
    return nullptr;
  auto it = datablocks.find(dbOp);
  return (it != datablocks.end()) ? it->second : nullptr;
}

DataBlockCodegen *ArtsCodegen::createDatablock(arts::DataBlockOp dbOp,
                                               Location loc) {
  assert(!getDatablock(dbOp) && "Datablock already exists");
  datablocks[dbOp] = new DataBlockCodegen(*this, dbOp, loc);
  return datablocks[dbOp];
}

DataBlockCodegen *ArtsCodegen::getOrCreateDatablock(arts::DataBlockOp dbOp,
                                                    Location loc) {
  if (auto db = getDatablock(dbOp))
    return db;
  return createDatablock(dbOp, loc);
}

/// Events
EventCodegen *ArtsCodegen::getEvent(arts::EventOp eventOp) {
  auto it = events.find(eventOp);
  return (it != events.end()) ? it->second : nullptr;
}

EventCodegen *ArtsCodegen::getOrCreateEvent(arts::EventOp eventOp,
                                            Location loc) {
  if (auto event = getEvent(eventOp))
    return event;
  events[eventOp] = new EventCodegen(*this, eventOp, loc);
  return events[eventOp];
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
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);
  return callOp.getResult(0);
}

Value ArtsCodegen::getTotalWorkers(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetTotalWorkers);
  assert(func && "Runtime function should exist");
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
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  auto callOp = builder.create<func::CallOp>(loc, func);

  return callOp.getResult(0);
}

Value ArtsCodegen::getCurrentNode(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentNode);
  assert(func && "Runtime function should exist");
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  func->setAttr("llvm.nounwind", builder.getUnitAttr());
  // ArrayRef<Value> args;
  auto callOp = builder.create<func::CallOp>(loc, func);
  return callOp.getResult(0);
}

void ArtsCodegen::satisfyEventDependency(Value eventGuid, Value dbGuid,
                                         Location loc) {
  auto const ARTS_EVENT_LATCH_DECR_SLOT = createIntConstant(0, Int32, loc);
  createRuntimeCall(ARTSRTL_artsEventSatisfySlot,
                    {eventGuid, dbGuid, ARTS_EVENT_LATCH_DECR_SLOT}, loc);
}

void ArtsCodegen::addEventDependency(Value eventGuid, Value edtGuid,
                                     Value edtSlot, Value dataGuid,
                                     Location loc) {
  auto edtSlotInt = castToInt(Int32, edtSlot, loc);
  createRuntimeCall(ARTSRTL_artsAddDependenceToPersistentEvent,
                    {eventGuid, edtGuid, edtSlotInt, dataGuid}, loc);
}

void ArtsCodegen::incrementEventLatchCount(Value eventGuid, Value dataGuid,
                                           Location loc) {
  createRuntimeCall(ARTSRTL_artsPersistentEventIncrementLatch,
                    {eventGuid, dataGuid}, loc);
}

void ArtsCodegen::decrementEventLatchCount(Value eventGuid, Value dataGuid,
                                           Location loc) {
  createRuntimeCall(ARTSRTL_artsPersistentEventDecrementLatch,
                    {eventGuid, dataGuid}, loc);
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

func::FuncOp ArtsCodegen::insertArtsMainFn(Location loc, func::FuncOp callback) {
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