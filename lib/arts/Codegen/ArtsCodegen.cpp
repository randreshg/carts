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
  if (hasPtrDb()) {
    LLVM_DEBUG(DBGS() << "DB has a pointer to another datablock...\n");
    return;
  }

  /// Set insertion point to the DatablockOp
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);

  auto currentNode = AC.getCurrentNode(loc);
  auto elementTypeSize = dbOp.getElementTypeSize();
  const auto tySize = AC.castToInt(AC.Int64, elementTypeSize, loc);

  /// Handle the case of a single datablock
  if (isSingle()) {
    auto modeVal = getMode(dbOp.getMode());
    guid = createGuid(currentNode, modeVal, loc);
    // Allocate a single pointer by directly assigning the runtime call result
    ptr =
        AC.createRuntimeCall(ARTSRTL_artsDbCreateWithGuid, {guid, tySize}, loc)
            ->getResult(0);
    LLVM_DEBUG(DBGS() << "Created single datablock: " << ptr << "\n");
    return;
  }

  /// Create an array of guids and pointers based on the sizes of the dbOp
  auto sizes = dbOp.getSizes();
  const auto dbDim = sizes.size();
  auto modeVal = getMode(dbOp.getMode());
  auto guidType = MemRefType::get(
      std::vector<int64_t>(dbDim, ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, sizes);
  auto ptrType = MemRefType::get(
      std::vector<int64_t>(dbDim, ShapedType::kDynamic), AC.VoidPtr);
  ptr = builder.create<memref::AllocaOp>(loc, ptrType, sizes);
  LLVM_DEBUG(DBGS() << "Created array of datablocks: " << ptr << "\n");

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

// ---------------------------- Events ---------------------------- ///
EventCodegen::EventCodegen(ArtsCodegen &AC, arts::EventOp eventOp, Location loc)
    : AC(AC), builder(AC.builder) {
  create(eventOp, loc);
}

void EventCodegen::create(arts::EventOp eventOp, Location loc) {
  /// Create a single persistent event with latchCount = 0
  auto createEvent = [&](Value node, Location loc) -> Value {
    Value latchCount = AC.createIntConstant(0, AC.Int32, loc);
    auto eventVal = AC.createRuntimeCall(ARTSRTL_artsPersistentEventCreate,
                                         {node, latchCount}, loc)
                        .getResult(0);
    return eventVal;
  };

  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(eventOp);
  auto node = AC.getCurrentNode(loc);

  /// If the event is a single event, create it and return
  if (eventOp.isSingle()) {
    guid = createEvent(node, loc);
    return;
  }

  /// Create an array of guids based on the sizes of the eventOp
  auto sizes = eventOp.getSizes();
  auto guidType = MemRefType::get(
      std::vector<int64_t>(sizes.size(), ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, sizes);
  this->eventOp = eventOp;

  /// Create nested loops to create the guids
  std::function<void(unsigned, SmallVector<Value, 4> &)> createGuids =
      [&](unsigned dim, SmallVector<Value, 4> &indices) {
        if (dim == sizes.size()) {
          auto guidVal = createEvent(node, loc);
          builder.create<memref::StoreOp>(loc, guidVal, guid, indices);
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
  OpBuilder::InsertionGuard IG(builder);
  const auto indexType = IndexType::get(builder.getContext());
  const auto indexMemRefType = MemRefType::get({}, indexType);

  /// Allocate and initialize total events counter to zero.
  auto zeroConstant = AC.createIndexConstant(0, loc);
  Value totalEventsToSatisfy =
      builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, zeroConstant, totalEventsToSatisfy);

  /// Initialize dependency count to zero.
  depC = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, zeroConstant, depC);

  /// Process dependencies
  if (!deps.empty()) {
    /// Insert db size as parameter if not a constant.
    auto insertSizeAsParameter = [&](DataBlockCodegen *db, Value size,
                                     uint64_t sizeItr,
                                     uint64_t &sizesInserted) {
      if (auto *defOp = size.getDefiningOp())
        if (isa<arith::ConstantIndexOp>(defOp) ||
            (isa<arith::ConstantOp>(defOp) &&
             cast<arith::ConstantOp>(defOp).getValue().isa<IntegerAttr>()))
          return;

      /// TODO: Check if the size is already inserted. If so, reuse the index.
      /// Otherwise, insert parameter and store the index where it was inserted.
      entryDbs[db].sizeIndex[sizeItr] = insertParam(size);
      ++sizesInserted;
    };

    /// Process the event. If the db has 'out' mode, satisfy the event and
    /// accumulate the number of events, since they will be satisfied in the EDT
    /// and the GUID has to be sent as a parameter. If the db has 'in' mode,
    /// record the event.
    auto processEvent = [&](DataBlockCodegen *db, Value &dbNumElements) {
      auto inEvent = db->getInEvent();
      auto outEvent = db->getOutEvent();

      /// If it is an input datablock, and it has 'in' event, record it.
      /// Otherwise, signal it
      if (db->isInMode()) {
        assert((inEvent) && "Datablock has no 'in' event");
        depsToRecord.push_back(db);
      }

      /// Satisfy the event and accumulate the number of events to satisfy.
      if (!outEvent)
        return;
      assert((db->isOutMode()) && "Datablock is not 'out' mode");
      depsToSatisfy.push_back(db);
      auto currEvents =
          builder.create<memref::LoadOp>(loc, totalEventsToSatisfy);
      auto newEvents =
          builder.create<arith::AddIOp>(loc, currEvents, dbNumElements)
              .getResult();
      builder.create<memref::StoreOp>(loc, newEvents, totalEventsToSatisfy);
    };

    /// Process each dependency to:
    /// - Add db sizes as parameters
    /// - Compute the dependency count (depc)
    /// - Compute the total number of events to satisfy
    for (const auto &dep : deps) {
      auto db = AC.getDatablock(dep);
      assert(db && "Datablock not found");

      /// Load the current dependency count and set EDT slot.
      db->setEdtSlot(builder.create<memref::LoadOp>(loc, depC).getResult());
      uint64_t sizesInserted = 0;
      Value dbNumElements = nullptr;

      /// For a single datablock, the number of elements is 1.
      if (db->isSingle()) {
        dbNumElements = AC.createIndexConstant(1, loc);
        insertSizeAsParameter(db, dbNumElements, 0, sizesInserted);
      }
      /// For multi-dimensional datablocks, compute the number of elements by
      /// multiplying the sizes.
      else {
        Value product = AC.createIndexConstant(1, loc);
        for (auto size : db->getSizes()) {
          insertSizeAsParameter(db, size, sizesInserted, sizesInserted);
          product =
              builder.create<arith::MulIOp>(loc, product, size).getResult();
        }
        dbNumElements = product;
      }

      /// Dependency count by adding dbNumElements only if is an input datablock
      if (db->isInMode()) {
        auto currDep = builder.create<memref::LoadOp>(loc, depC);
        auto newDep = builder.create<arith::AddIOp>(loc, currDep, dbNumElements)
                          .getResult();
        builder.create<memref::StoreOp>(loc, newDep, depC);
      }

      /// Process events.
      processEvent(db, dbNumElements);
    }
  }

  /// Cast depC to int32.
  auto loadedDepC = builder.create<memref::LoadOp>(loc, depC);
  depC = AC.castToInt(AC.Int32, loadedDepC, loc);

  /// Process parameters.
  /// The total number of parameters is the sum of static elements,
  /// the dynamic sizes of datablocks, and the number of events.
  unsigned paramSize = params.size();
  paramC = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  auto initParamSize = AC.createIndexConstant(paramSize, loc);
  builder.create<memref::StoreOp>(loc, initParamSize, paramC);

  /// Add the number of events if any
  if (!depsToSatisfy.empty()) {
    auto loadedParamC = builder.create<memref::LoadOp>(loc, paramC);
    auto totalEventsVal =
        builder.create<memref::LoadOp>(loc, totalEventsToSatisfy);
    auto totalParamC =
        builder.create<arith::AddIOp>(loc, loadedParamC, totalEventsVal)
            .getResult();
    builder.create<memref::StoreOp>(loc, totalParamC, paramC);
  }

  /// Store ParamC
  auto loadedParamC = builder.create<memref::LoadOp>(loc, paramC);
  paramC = AC.castToInt(AC.Int32, loadedParamC, loc);

  /// Allocate the paramV array
  paramV = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64),
      ValueRange{loadedParamC});

  /// Start by loading the parameters that have already been collected.
  for (unsigned i = 0; i < paramSize; ++i) {
    auto paramIndex = AC.createIndexConstant(i, loc);
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    builder.create<memref::StoreOp>(loc, param, paramV, ValueRange{paramIndex});
  }

  /// If there are no events to satisfy, we are done.
  if (depsToSatisfy.empty())
    return;

  /// For each event that has to be satisfied insert the event GUIDs into
  /// paramVArray. Initialize the parameter index to the current number of
  /// parameters.
  auto paramIndex = builder.create<memref::AllocaOp>(loc, indexMemRefType);
  builder.create<memref::StoreOp>(loc, initParamSize, paramIndex);
  for (auto db : depsToSatisfy) {
    arts::EventOp eventOp =
        dyn_cast<arts::EventOp>(db->getOutEvent().getDefiningOp());
    EventCodegen *eventCG = AC.getEvent(eventOp);
    assert(eventCG && "Event not found");

    /// Handle single events
    if (eventOp.isSingle()) {
      auto eventGuid = eventCG->getGuid();
      auto curParamIdx =
          builder.create<memref::LoadOp>(loc, paramIndex.getResult());
      builder.create<memref::StoreOp>(loc, eventGuid, paramV,
                                      ValueRange{curParamIdx});
      auto newParamIdx = builder.create<arith::AddIOp>(
          loc, curParamIdx, AC.createIndexConstant(1, loc));
      builder.create<memref::StoreOp>(loc, newParamIdx, paramIndex);
      continue;
    }

    /// Handle multi-dimensional events
    auto eventDim = eventOp.getSizes().size();
    std::function<void(unsigned, SmallVector<Value, 4> &)> createEvents =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == eventDim) {
            /// Load the event GUID at the current indices.
            auto eventGuid = builder.create<memref::LoadOp>(
                loc, eventCG->getGuid(), indices);
            /// Get and convert the current parameter index.
            auto curParamIdx =
                builder.create<memref::LoadOp>(loc, paramIndex.getResult());
            /// Store the event GUID into the paramVArray.
            builder.create<memref::StoreOp>(loc, eventGuid, paramV,
                                            ValueRange{curParamIdx});
            /// Increment the parameter index.
            auto newParamIdx =
                builder
                    .create<arith::AddIOp>(loc, curParamIdx,
                                           AC.createIndexConstant(1, loc))
                    .getResult();
            builder.create<memref::StoreOp>(loc, newParamIdx, paramIndex);
            return;
          }
          /// Create a loop for the current dimension.
          auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
          auto upper = eventOp.getSizes()[dim];
          auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
          auto loopOp = builder.create<scf::ForOp>(loc, lower, upper, step);
          auto &loopBody = loopOp.getRegion().front();
          builder.setInsertionPointToStart(&loopBody);
          /// Append the induction variable to the current indices.
          indices.push_back(loopOp.getInductionVar());
          /// Recurse to process the next dimension.
          createEvents(dim + 1, indices);
          /// Clean up the indices and reset the insertion point.
          indices.pop_back();
          builder.setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> indices = db->getIndices();
    createEvents(indices.size(), indices);
  }
}

void EdtCodegen::processDependencies(Location loc) {
  /// Set the insertion point at the EDT function return to satisfy
  /// dependencies.
  builder.setInsertionPoint(returnOp);
  auto unknownLoc = UnknownLoc::get(builder.getContext());
  const auto indexType = IndexType::get(builder.getContext());
  const auto indexMemRefType = MemRefType::get({}, indexType);

  /// Process out-mode dependencies.
  /// This first step consist of decrementing the latch count of the out
  /// dependencies in the edt function.
  if (!depsToSatisfy.empty()) {
    auto currentSlot = builder.create<memref::AllocaOp>(loc, indexMemRefType);
    auto initialSlot = AC.createIndexConstant(params.size(), loc);
    builder.create<memref::StoreOp>(loc, initialSlot, currentSlot);
    auto fnParamVPtr = AC.castToLLVMPtr(fnParamV, loc);

    /// Iterate over the dependencies to satisfy.
    for (auto *db : depsToSatisfy) {
      assert(db->getOutEvent() && "Datablock missing out event");

      /// For single datablocks, directly satisfy the dependency.
      if (db->isSingle()) {
        auto currIndex =
            builder.create<memref::LoadOp>(loc, currentSlot.getResult());
        auto currIndexInt = AC.castToInt(AC.Int32, currIndex, loc);
        auto eventGuid =
            builder
                .create<LLVM::GEPOp>(loc, AC.llvmPtr, AC.ArtsGuid, fnParamVPtr,
                                     ValueRange{currIndexInt})
                .getResult();
        auto loadedEventGuid =
            builder.create<LLVM::LoadOp>(loc, AC.ArtsGuid, eventGuid)
                .getResult();
        AC.decrementEventLatchCount(loadedEventGuid, entryDbs[db].guid,
                                    unknownLoc);

        /// Increment the parameter index.
        auto newIndex = builder.create<arith::AddIOp>(
            loc, currIndex, AC.createIndexConstant(1, loc));
        builder.create<memref::StoreOp>(loc, newIndex, currentSlot);
        continue;
      }

      const auto &sizes = entryDbs[db].sizes;
      const unsigned dbDim = sizes.size();

      /// For mutidimensional datablocks, satisfy dependencies for each element.
      std::function<Value(unsigned, SmallVector<Value, 4> &, Value)>
          satisfyEventDependencies = [&](unsigned dim,
                                         SmallVector<Value, 4> &indices,
                                         Value curSlot) -> Value {
        if (dim == dbDim) {
          auto currIndex = builder.create<memref::LoadOp>(loc, curSlot);
          auto currIndexInt = AC.castToInt(AC.Int32, currIndex, loc);
          auto eventGuid =
              builder
                  .create<LLVM::GEPOp>(loc, AC.llvmPtr, AC.ArtsGuid,
                                       fnParamVPtr, ValueRange{currIndexInt})
                  .getResult();
          auto loadedEventGuid =
              builder.create<LLVM::LoadOp>(loc, AC.ArtsGuid, eventGuid)
                  .getResult();

          /// Load the datablock GUID from the entry using the current indices.
          auto entryDbGuidPtr = AC.castToLLVMPtr(entryDbs[db].guid, loc);
          SmallVector<Value, 4> intIndices;
          for (auto index : indices)
            intIndices.push_back(AC.castToInt(AC.Int32, index, loc));
          auto dbGuid = builder.create<LLVM::GEPOp>(
              loc, AC.llvmPtr, AC.ArtsGuid, entryDbGuidPtr, intIndices);
          auto loadedDbGuid =
              builder.create<LLVM::LoadOp>(loc, AC.ArtsGuid, dbGuid)
                  .getResult();

          /// Satisfy the dependency between the event and the datablock.
          AC.decrementEventLatchCount(loadedEventGuid, loadedDbGuid, loc);

          /// Increment the current index.
          auto newIndex =
              builder
                  .create<arith::AddIOp>(loc, currIndex,
                                         AC.createIndexConstant(1, loc))
                  .getResult();

          /// Update the current carried slot in-place.
          builder.create<memref::StoreOp>(loc, newIndex, curSlot);
          return curSlot;
        }
        auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
        auto upper = sizes[dim];
        auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
        auto loopOp =
            builder.create<scf::ForOp>(loc, lower, upper, step, curSlot);
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        indices.push_back(loopBlock.getArgument(0));
        Value newCurSlot = satisfyEventDependencies(dim + 1, indices,
                                                    loopBlock.getArgument(1));
        indices.pop_back();
        builder.create<scf::YieldOp>(loc, newCurSlot);
        builder.setInsertionPointAfter(loopOp);
        return loopOp.getResult(0);
      };

      SmallVector<Value, 4> indices;
      satisfyEventDependencies(0, indices, currentSlot);
    }
  }

  /// Record in-mode dependencies after the EDT call.
  builder.setInsertionPointAfter(guid.getDefiningOp());
  for (auto *db : depsToRecord) {
    /// Ensure the datablock has an event and is 'in' mode.
    auto dbEvent = db->getInEvent();
    assert(dbEvent && "Datablock missing event");
    EventCodegen *eventCG =
        AC.getEvent(dyn_cast<arts::EventOp>(dbEvent.getDefiningOp()));
    assert(eventCG && "Event not found");
    auto dbLoc = db->getOp().getLoc();

    /// For single datablocks, load the event GUID and add the dependency.
    if (db->isSingle()) {
      auto eventGuid = eventCG->getGuid();
      auto dbIndices = db->getIndices();
      eventGuid = builder.create<memref::LoadOp>(dbLoc, eventGuid, dbIndices);
      AC.addEventDependency(eventGuid, guid, db->getEdtSlot(), dbLoc);
      continue;
    }

    /// For multi-dimensional datablocks, add dependencies recursively.
    SmallVector<Value> sizes = eventCG->getSizes();
    const unsigned eventDim = sizes.size();
    auto currentSlot = builder.create<memref::AllocOp>(dbLoc, indexMemRefType);
    builder.create<memref::StoreOp>(dbLoc, db->getEdtSlot(), currentSlot);

    std::function<void(unsigned, SmallVector<Value, 4> &)> addDependencies =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == eventDim) {
            auto eventGuid = builder.create<memref::LoadOp>(
                dbLoc, eventCG->getGuid(), indices);
            auto slot =
                builder.create<memref::LoadOp>(dbLoc, currentSlot.getResult());
            AC.addEventDependency(eventGuid, guid, slot, dbLoc);
            auto newSlot = builder
                               .create<arith::AddIOp>(
                                   dbLoc, slot, AC.createIndexConstant(1, loc))
                               .getResult();
            builder.create<memref::StoreOp>(dbLoc, newSlot, currentSlot);
            return;
          }
          auto lower = builder.create<arith::ConstantIndexOp>(dbLoc, 0);
          auto upper = sizes[dim];
          auto step = builder.create<arith::ConstantIndexOp>(dbLoc, 1);
          auto loopOp = builder.create<scf::ForOp>(dbLoc, lower, upper, step);
          auto &loopBlock = loopOp.getRegion().front();
          {
            OpBuilder::InsertionGuard IG(builder);
            builder.setInsertionPointToStart(&loopBlock);
            indices.push_back(loopOp.getInductionVar());
            addDependencies(dim + 1, indices);
            indices.pop_back();
          }
          builder.setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> indices = db->getIndices();
    addDependencies(indices.size(), indices);
  }

  /// Now that all dependencies are recorded, we can need to increment the latch
  /// count of all the out dependencies.
  if (!depsToSatisfy.empty()) {
    /// We need to load the event guid and datablock guid
    for (auto *db : depsToSatisfy) {
      assert(db && "Datablock not found");
      /// Get the event GUID
      Value eventGuid = db->getOutEvent();
      assert(eventGuid && "Datablock missing out event");

      /// Get the datablock GUID
      Value dbGuid = db->getGuid();
      assert(dbGuid && "Datablock GUID not found");
      auto dbLoc = db->getOp().getLoc();

      /// Increment the latch count for single datablocks.
      if (db->isSingle()) {
        auto dbIndices = db->getIndices();
        if (!dbIndices.empty()) {
          dbGuid = builder.create<memref::LoadOp>(dbLoc, dbGuid, dbIndices);
          eventGuid =
              builder.create<memref::LoadOp>(dbLoc, eventGuid, dbIndices);
        }
        AC.incrementEventLatchCount(eventGuid, dbGuid, dbLoc);
        continue;
      }

      /// Increment the latch count for multi-dimensional datablocks.
      auto sizes = db->getSizes();
      const unsigned dbDim = sizes.size();
      auto currentSlot =
          builder.create<memref::AllocOp>(dbLoc, indexMemRefType);
      builder.create<memref::StoreOp>(dbLoc, db->getEdtSlot(), currentSlot);
      std::function<void(unsigned, Value, SmallVector<Value, 4> &)>
          incrementEventLatch = [&](unsigned dim, Value curSlot,
                                    SmallVector<Value, 4> &indices) {
            if (dim == dbDim) {
              auto loadedDbGuid =
                  builder.create<memref::LoadOp>(dbLoc, dbGuid, indices);
              auto loadedEventGuid =
                  builder.create<memref::LoadOp>(dbLoc, eventGuid, indices);
              AC.incrementEventLatchCount(loadedEventGuid, loadedDbGuid, dbLoc);
              /// Increment the slot.
              auto loadedCurSlot =
                  builder.create<memref::LoadOp>(dbLoc, curSlot);
              auto newSlot = builder.create<arith::AddIOp>(
                  dbLoc, loadedCurSlot, AC.createIndexConstant(1, dbLoc));
              builder.create<memref::StoreOp>(dbLoc, newSlot, curSlot);
              return;
            } else {
              auto lower = builder.create<arith::ConstantIndexOp>(dbLoc, 0);
              auto upper = sizes[dim];
              auto step = builder.create<arith::ConstantIndexOp>(dbLoc, 1);
              auto loopOp = builder.create<scf::ForOp>(dbLoc, lower, upper,
                                                       step, curSlot);
              auto &loopBlock = loopOp.getRegion().front();
              {
                OpBuilder::InsertionGuard guard(builder);
                builder.setInsertionPointToStart(&loopBlock);
                indices.push_back(loopBlock.getArgument(0));
                Value newSlot = loopBlock.getArgument(1);
                incrementEventLatch(dim + 1, newSlot, indices);
                indices.pop_back();
                builder.create<scf::YieldOp>(dbLoc, newSlot);
              }
              builder.setInsertionPointAfter(loopOp);
            }
          };

      SmallVector<Value, 4> indices;
      incrementEventLatch(0, currentSlot.getResult(), indices);
    }
  }

  /// Iterate over the edt dependencies and replace all its uses with the
  /// corresponding values.
  for (auto &dep : deps) {
    auto *db = AC.getDatablock(dep);
    auto dbPtr = db->getPtr();
    if (!dbPtr)
      continue;
    db->getOp().replaceAllUsesWith(dbPtr);
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
  auto constZero = AC.createIndexConstant(0, loc);
  auto constOne = AC.createIndexConstant(1, loc);

  /// Insert the dependencies.
  auto indexAlloc = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, builder.getIndexType()));
  builder.create<memref::StoreOp>(loc, constZero, indexAlloc);

  /// Helper lambda to load the current index.
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
    /// Get corresponding DataBlock.
    auto *db = AC.getDatablock(dep);
    assert(db && "Datablock not found");

    /// Skip datablocks that are not in mode.
    if (!db->isInMode())
      continue;

    /// Iterate over uses to see if another datablock is using this one. If
    /// so, set the guid and ptr for the other datablock.
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

    /// Handle single datablock
    if (db->isSingle()) {
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
          builder.create<arith::AddIOp>(loc, curIndex, constOne).getResult();
      builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
      /// Rewire the datablock to the pointer.
      updateUserDb(entryGuid, entryPtr);
      rewireMap[db->getOp()] = entryPtr;
      continue;
    }

    /// Handle multi-dimensional datablock
    const auto &dbSizes = db->getSizes();
    auto &entrySizes = entryDbs[db].sizes;
    entrySizes.reserve(dbSizes.size());
    for (unsigned i = 0, e = dbSizes.size(); i < e; ++i) {
      if (entryDbs[db].sizeIndex.count(i))
        entrySizes.push_back(rewireMap[params[entryDbs[db].sizeIndex[i]]]);
      else if (auto cstOp = dbSizes[i].getDefiningOp<arith::ConstantIndexOp>())
        entrySizes.push_back(builder.clone(*cstOp)->getResult(0));
      else
        llvm_unreachable("Datablock size is not a constant");
    }

    /// Allocate arrays for GUIDs and pointers.
    auto guidType = MemRefType::get(
        std::vector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.ArtsGuid);
    auto entryGuid =
        builder.create<memref::AllocaOp>(loc, guidType, entrySizes);
    auto ptrType = MemRefType::get(
        std::vector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.VoidPtr);
    auto entryPtr = builder.create<memref::AllocaOp>(loc, ptrType, entrySizes);

    /// Recursively load the datablock entry info for each element.
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
                builder.create<arith::AddIOp>(loc, curIndex, constOne)
                    .getResult();
            builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
            return;
          }

          /// Create loop for current dimension.
          auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
          auto upper = entrySizes[dim];
          auto loopOp = builder.create<scf::ForOp>(loc, lower, upper, constOne);
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
                                     Value edtSlot, Location loc) {
  auto edtSlotInt = castToInt(Int32, edtSlot, loc);
  createRuntimeCall(ARTSRTL_artsAddDependenceToPersistentEvent,
                    {eventGuid, edtGuid, edtSlotInt}, loc);
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

func::FuncOp ArtsCodegen::insertMain(Location loc) {
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

  /// Duplicate main function and change its name to "mainBody"
  auto mainBodyFunc = mainFunc.clone();
  mainBodyFunc.setName("mainBody");
  module.push_back(mainBodyFunc);

  /// Remove the original main function
  mainFunc.erase();

  /// Insert init functions
  insertInitPerWorker(loc);
  insertInitPerNode(loc, mainBodyFunc);
  insertMain(loc);
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