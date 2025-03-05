///==========================================================================
/// File: ArtsCodegen.cpp
///==========================================================================

/// Other dialects
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
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

  /// If the base is a DB, it will be handled when inserting the EDT Entry
  if (hasPtrDb())
    return;

  /// Set insertion point to the DatablockOp
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);

  auto currentNode = AC.getCurrentNode(loc);
  auto elementTypeSize = dbOp.getElementTypeSize();

  /// Handle the case of a single datablock
  if (isSingle()) {
    auto modeVal = getMode(dbOp.getMode());
    guid = createGuid(currentNode, modeVal, loc);
    ptr = AC.createRuntimeCall(ARTSRTL_artsDbCreateWithGuidAndData,
                               {AC.castToVoidPtr(dbOp.getPtr(), loc),
                                elementTypeSize, modeVal, guid},
                               loc)
              ->getResult(0);
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

  /// Recursively create datablocks for each element
  std::function<void(unsigned, SmallVector<Value, 4> &)> createDbs =
      [&](unsigned dim, SmallVector<Value, 4> &indices) {
        if (dim == dbDim) {
          auto guidVal = createGuid(currentNode, modeVal, loc);
          auto tySize = AC.castToInt(AC.Int64, elementTypeSize, loc);
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
  auto createEvent = [&](Value node, Location loc) -> Value {
    Value latchCount = AC.createIntConstant(1, AC.Int32, loc);
    auto eventVal =
        AC.createRuntimeCall(ARTSRTL_artsEventCreate, {node, latchCount}, loc)
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
  /// Allocate and initialize total events counter to zero.
  auto zeroConstant = AC.createIntConstant(0, AC.Int32, loc);
  Value totalEventsToSatisfy = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, AC.Int32), ValueRange());
  builder.create<memref::StoreOp>(loc, zeroConstant, totalEventsToSatisfy);

  /// Process dependencies
  if (!deps.empty()) {
    /// Initialize dependency count to zero.
    depC = builder.create<memref::AllocaOp>(loc, MemRefType::get({}, AC.Int32),
                                            ValueRange());
    builder.create<memref::StoreOp>(loc, zeroConstant, depC);

    /// Insert db size as parameter if not a constant.
    auto insertSizeAsParameter = [&](DataBlockCodegen *db, Value size,
                                     uint64_t sizeItr,
                                     uint64_t &sizesInserted) {
      if (auto *defOp = size.getDefiningOp())
        if (isa<arith::ConstantIndexOp>(defOp) ||
            (isa<arith::ConstantOp>(defOp) &&
             cast<arith::ConstantOp>(defOp).getValue().isa<IntegerAttr>()))
          return;

      /// Otherwise, insert parameter and store the index where it was inserted.
      LLVM_DEBUG(dbgs() << "Inserting size as parameter: " << size << "\n");
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
        if (inEvent)
          depsToRecord.push_back(db);
        else
          depsToSignal.push_back(db);
        return;
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
      db->setEdtSlot(builder.create<memref::LoadOp>(loc, depC).getResult());
      uint64_t sizesInserted = 0;
      Value dbNumElements = nullptr;
      if (db->isSingle()) {
        /// For a single datablock, the number of elements is its only size.
        dbNumElements = AC.createIntConstant(1, AC.Int32, loc);
        insertSizeAsParameter(db, dbNumElements, 0, sizesInserted);
      } else {
        /// Allocate a temporary to hold the product of sizes.
        auto tmpAlloca = builder.create<memref::AllocaOp>(
            loc, MemRefType::get({}, AC.Int32), ValueRange());
        builder.create<memref::StoreOp>(
            loc, AC.createIntConstant(1, AC.Int32, loc), tmpAlloca);
        for (auto size : db->getSizes()) {
          insertSizeAsParameter(db, size, sizesInserted, sizesInserted);
          auto currVal =
              builder.create<memref::LoadOp>(loc, tmpAlloca.getResult());
          auto sizeInt = AC.castToInt(AC.Int32, size, loc);
          auto prod =
              builder.create<arith::MulIOp>(loc, currVal, sizeInt).getResult();
          builder.create<memref::StoreOp>(loc, prod, tmpAlloca.getResult());
        }
        /// Load the computed number of elements.
        dbNumElements =
            builder.create<memref::LoadOp>(loc, tmpAlloca.getResult());
      }

      /// Update dependency count by adding dbNumElements.
      auto currDep = builder.create<memref::LoadOp>(loc, depC);
      auto newDep = builder.create<arith::AddIOp>(loc, currDep, dbNumElements)
                        .getResult();
      builder.create<memref::StoreOp>(loc, newDep, depC);

      /// Process events.
      processEvent(db, dbNumElements);
    }

    /// Cast depC to int32.
    depC = builder.create<memref::LoadOp>(loc, depC);
  }

  /// Process parameters.
  /// The total number of parameters is the sum of static elements,
  /// the dynamic sizes of datablocks, and the number of events.
  unsigned paramSize = params.size();
  paramC = builder.create<memref::AllocaOp>(loc, MemRefType::get({}, AC.Int32),
                                            ValueRange());
  builder.create<memref::StoreOp>(
      loc, AC.createIntConstant(paramSize, AC.Int32, loc), paramC);
  /// Add the number of events if any
  if (!depsToSatisfy.empty()) {
    auto loadedParamC = builder.create<memref::LoadOp>(loc, paramC).getResult();
    auto totalEventsVal =
        builder.create<memref::LoadOp>(loc, totalEventsToSatisfy).getResult();
    auto totalParamC =
        builder.create<arith::AddIOp>(loc, loadedParamC, totalEventsVal)
            .getResult();
    builder.create<memref::StoreOp>(loc, totalParamC, paramC);
  }

  /// Allocate the paramV array.
  paramC = builder.create<memref::LoadOp>(loc, paramC);
  paramV = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64),
      ValueRange{AC.castToIndex(paramC, loc)});

  /// Start by loading the parameters that have already been collected.
  for (unsigned i = 0; i < paramSize; ++i) {
    auto param = AC.castToInt(AC.Int64, params[i], loc);
    auto paramIndex = AC.createIndexConstant(i, loc);
    builder.create<affine::AffineStoreOp>(loc, param, paramV,
                                          ValueRange{paramIndex});
  }

  /// If there are no events to satisfy, we are done.
  if (depsToSatisfy.empty())
    return;

  /// For each event that has to be satisfied insert the event GUIDs into
  /// paramVArray. Initialize the parameter index to the current number of
  /// parameters.
  auto paramIndex = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, AC.Int32), ValueRange());
  builder.create<memref::StoreOp>(
      loc, AC.createIntConstant(paramSize, AC.Int32, loc), paramIndex);
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
      auto curParamIdxIndex = AC.castToIndex(curParamIdx, loc);
      builder.create<memref::StoreOp>(loc, eventGuid, paramV,
                                      ValueRange{curParamIdxIndex});
      auto newParamIdx = builder.create<arith::AddIOp>(
          loc, curParamIdx, AC.createIntConstant(1, AC.Int32, loc));
      builder.create<memref::StoreOp>(loc, newParamIdx, paramIndex);
      continue;
    }

    /// Handle multi-dimensional events
    auto eventDim = eventOp.getSizes().size();
    /// Load event GUIDs into paramVArray.
    std::function<void(unsigned, SmallVector<Value, 4> &)> createEvents =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == eventDim) {
            /// Load the event GUID at the current indices.
            auto eventGuid =
                builder.create<memref::LoadOp>(loc, eventCG->getGuid(), indices)
                    .getResult();
            /// Get and convert the current parameter index.
            auto curParamIdx =
                builder.create<memref::LoadOp>(loc, paramIndex.getResult());
            auto curParamIdxIndex = AC.castToIndex(curParamIdx, loc);
            /// Store the event GUID into the paramVArray.
            builder.create<memref::StoreOp>(loc, eventGuid, paramV,
                                            ValueRange{curParamIdxIndex});
            /// Increment the parameter index.
            auto newParamIdx = builder
                                   .create<arith::AddIOp>(
                                       loc, curParamIdx,
                                       AC.createIntConstant(1, AC.Int32, loc))
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

  /// Process out-mode dependencies that must be satisfied.
  if (!depsToSatisfy.empty()) {
    /// Variable to keep track of the current parameter index.
    auto currentSlot = builder.create<memref::AllocaOp>(
        loc, MemRefType::get({}, AC.Int32), ValueRange());
    builder.create<memref::StoreOp>(
        loc, AC.createIntConstant(0, AC.Int32, unknownLoc), currentSlot);

    /// Iterate over the dependencies to satisfy.
    for (auto *db : depsToSatisfy) {
      /// Ensure the datablock has an event and is in out mode.
      assert(db->getOutEvent() && "Datablock missing out event");

      if (db->isSingle()) {
        /// For single datablocks, directly satisfy the dependency.
        auto currIndex =
            builder.create<memref::LoadOp>(loc, currentSlot.getResult())
                .getResult();
        auto currIndexIndex = AC.castToIndex(currIndex, loc);
        auto eventGuid =
            builder.create<memref::LoadOp>(loc, fnParamV, currIndexIndex)
                .getResult();
        AC.satisfyDep(eventGuid, entryDbs[db].guid, unknownLoc);

        /// Increment the parameter index.
        auto newIndex = builder.create<arith::AddIOp>(
            loc, currIndex, AC.createIntConstant(1, AC.Int32, loc));
        builder.create<memref::StoreOp>(loc, newIndex, currentSlot);
        continue;
      }

      const auto &sizes = entryDbs[db].sizes;
      const unsigned dbDim = sizes.size();

      /// Lambda to recursively satisfy dependencies for each element.
      std::function<Value(unsigned, SmallVector<Value, 4> &, Value)>
          satisfyDependencies = [&](unsigned dim,
                                    SmallVector<Value, 4> &indices,
                                    Value curSlot) -> Value {
        if (dim == dbDim) {
          /// Load the current index from the carried slot.
          auto currIndex =
              builder.create<memref::LoadOp>(loc, curSlot).getResult();
          /// Convert the current index to an index type.
          auto currIndexIndex = AC.castToIndex(currIndex, loc);
          /// Load the event GUID using the current index.
          auto eventGuid =
              builder.create<memref::LoadOp>(loc, fnParamV, currIndexIndex)
                  .getResult();
          /// Load the datablock GUID from the entry using the current
          /// indices.
          auto dbGuid =
              builder.create<memref::LoadOp>(loc, entryDbs[db].guid, indices)
                  .getResult();
          /// Satisfy the dependency between the event and the datablock.
          AC.satisfyDep(eventGuid, dbGuid, unknownLoc);
          /// Increment the current index.
          auto newIndex =
              builder
                  .create<arith::AddIOp>(loc, currIndex,
                                         AC.createIntConstant(1, AC.Int32, loc))
                  .getResult();
          /// Update the current carried slot in-place.
          builder.create<memref::StoreOp>(loc, newIndex, curSlot);
          return curSlot;
        }
        auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
        auto upper = sizes[dim];
        auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
        // Pass curSlot as the initial loop-carried value.
        auto loopOp =
            builder.create<scf::ForOp>(loc, lower, upper, step, curSlot);
        auto &loopBlock = loopOp.getRegion().front();
        builder.setInsertionPointToStart(&loopBlock);
        indices.push_back(loopBlock.getArgument(0));
        Value newCurSlot =
            satisfyDependencies(dim + 1, indices, loopBlock.getArgument(1));
        indices.pop_back();
        builder.create<scf::YieldOp>(loc, newCurSlot);
        builder.setInsertionPointAfter(loopOp);
        return loopOp.getResult(0);
      };

      SmallVector<Value, 4> indices;
      satisfyDependencies(0, indices, currentSlot);
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
      AC.addDep(eventCG->getGuid(), guid, db->getEdtSlot(), dbLoc);
      continue;
    }

    /// For multi-dimensional datablocks, add dependencies recursively.
    SmallVector<Value> sizes = eventCG->getSizes();
    const unsigned eventDim = sizes.size();
    auto currentSlot =
        builder.create<memref::AllocOp>(dbLoc, MemRefType::get({}, AC.Int32));
    builder.create<memref::StoreOp>(dbLoc, db->getEdtSlot(), currentSlot);

    std::function<void(unsigned, SmallVector<Value, 4> &)> addDependencies =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == eventDim) {
            auto eventGuid =
                builder
                    .create<memref::LoadOp>(dbLoc, eventCG->getGuid(), indices)
                    .getResult();
            auto slot =
                builder.create<memref::LoadOp>(dbLoc, currentSlot.getResult());
            AC.addDep(eventGuid, guid, slot, dbLoc);
            auto newSlot =
                builder
                    .create<arith::AddIOp>(
                        dbLoc, slot, AC.createIntConstant(1, AC.Int32, dbLoc))
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
            OpBuilder::InsertionGuard guard(builder);
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

  /// Signal the dependencies after they are recorded.
  for (auto *db : depsToSignal) {
    assert(db && "Datablock not found");
    Value dbGuidMemref = db->getGuid();
    assert(dbGuidMemref && "Datablock GUID not found");
    auto dbLoc = db->getOp().getLoc();
    if (db->isSingle()) {
      AC.signalEdt(guid, db->getEdtSlot(), dbGuidMemref, dbLoc);
    } else {
      auto sizes = db->getSizes();
      const unsigned dbDim = sizes.size();
      auto currentSlot =
          builder.create<memref::AllocOp>(dbLoc, MemRefType::get({}, AC.Int32));
      builder.create<memref::StoreOp>(dbLoc, db->getEdtSlot(), currentSlot);

      std::function<void(unsigned, Value, SmallVector<Value, 4> &)>
          signalEdtDependencies = [&](unsigned dim, Value curSlot,
                                      SmallVector<Value, 4> &indices) {
            if (dim == dbDim) {
              auto loadedDbGuid =
                  builder.create<memref::LoadOp>(dbLoc, dbGuidMemref, indices)
                      .getResult();
              auto edtSlot = builder.create<memref::LoadOp>(dbLoc, curSlot,
                                                            ArrayRef<Value>{});
              AC.signalEdt(guid, edtSlot, loadedDbGuid, dbLoc);
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
                signalEdtDependencies(dim + 1, newSlot, indices);
                indices.pop_back();
                builder.create<scf::YieldOp>(dbLoc, newSlot);
              }
              builder.setInsertionPointAfter(loopOp);
            }
          };

      SmallVector<Value, 4> indices;
      signalEdtDependencies(0, currentSlot.getResult(), indices);
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

  /// Create an entry block and set the insertion point.
  auto *entryBlock = &func.getBody().front();

  /// Remove previous terminator, it will be inserted later
  entryBlock->getTerminator()->erase();

  /// Set insertion point to the start of the entry block.
  builder.setInsertionPointToStart(entryBlock);

  /// Get references to the block arguments.
  fnParamV = entryBlock->getArgument(1);
  fnDepV = entryBlock->getArgument(3);

  /// Clone constants.
  for (auto &oldConst : consts) {
    rewireMap[oldConst] =
        builder.clone(*oldConst.getDefiningOp())->getResult(0);
  }

  /// Insert the parameters.
  for (unsigned i = 0, e = params.size(); i < e; ++i) {
    auto idx = builder.create<arith::ConstantIndexOp>(loc, i);
    auto paramElem =
        builder.create<memref::LoadOp>(loc, fnParamV, ValueRange{idx});
    /// Cast it back to the original type.
    rewireMap[params[i]] =
        AC.castParameter(params[i].getType(), paramElem, loc);
  }

  /// Insert the dependencies.
  auto indexAlloc = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, builder.getIndexType()));
  auto constZero = builder.create<arith::ConstantIndexOp>(loc, 0);
  builder.create<memref::StoreOp>(loc, constZero, indexAlloc);
  auto constOne = builder.create<arith::ConstantIndexOp>(loc, 1);

  /// Helper lambda to load the current index.
  auto loadIndex = [&]() -> Value {
    return builder.create<memref::LoadOp>(loc, indexAlloc.getResult());
  };

  /// Process each dependency.
  for (auto &dep : deps) {
    /// Get corresponding DataBlock.
    auto *db = AC.getDatablock(dep);
    assert(db && "Datablock not found");

    /// Handle single datablock
    if (db->isSingle()) {
      auto curIndex = loadIndex();
      auto depVElem =
          builder.create<memref::LoadOp>(loc, fnDepV, ValueRange{curIndex});
      auto entryGuid = AC.getGuidFromEdtDep(depVElem, loc);
      auto entryPtr = AC.getPtrFromEdtDep(depVElem, loc);
      entryDbs[db].guid = entryGuid;
      entryDbs[db].ptr = entryPtr;
      /// Increment the index.
      auto newIndex =
          builder.create<arith::AddIOp>(loc, curIndex, constOne).getResult();
      builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
      rewireMap[db->getOp()] = entryPtr;
      continue;
    }

    /// Handle multi-dimensional datablock
    /// Fill out the entry size using the sizeIndex map and rewired parameters.
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
            auto depVElem =
                builder
                    .create<memref::LoadOp>(loc, fnDepV, ValueRange{curIndex})
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
    rewireMap[db->getOp()] = entryPtr;
  }

  /// Replace all uses in the region.
  replaceInRegion(*region, rewireMap, false);
}

// ---------------------------- ARTS Codegen ---------------------------- ///
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
  auto guidValue = builder.create<LLVM::ExtractValueOp>(
      loc, ArtsGuid, dep, builder.getDenseI64ArrayAttr(0));
  return guidValue.getResult();
}

Value ArtsCodegen::getPtrFromEdtDep(Value dep, Location loc) {
  auto ptrValue = builder.create<LLVM::ExtractValueOp>(
      loc, llvmPtr, dep, builder.getDenseI64ArrayAttr(2));
  return castToVoidPtr(ptrValue, loc);
}

Value ArtsCodegen::getCurrentEpochGuid(Location loc) {
  return createRuntimeCall(ARTSRTL_artsGetCurrentEpochGuid, {}, loc)
      .getResult(0);
}

Value ArtsCodegen::getCurrentEdtGuid(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentGuid);
  assert(func && "Runtime function should exist");
  // Mark the function as having no memory effects.
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  ArrayRef<Value> args;
  auto callOp = builder.create<func::CallOp>(loc, func, args);
  return callOp.getResult(0);
  // return createRuntimeCall(ARTSRTL_artsGetCurrentGuid, {},
  // loc).getResult(0);
}

Value ArtsCodegen::getCurrentNode(Location loc) {
  func::FuncOp func = getOrCreateRuntimeFunction(ARTSRTL_artsGetCurrentNode);
  assert(func && "Runtime function should exist");
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  ArrayRef<Value> args;
  auto callOp = builder.create<func::CallOp>(loc, func, args);
  return callOp.getResult(0);
}

void ArtsCodegen::satisfyDep(Value eventGuid, Value dbGuid, Location loc) {
  auto const ARTS_EVENT_LATCH_DECR_SLOT = createIntConstant(0, Int32, loc);
  createRuntimeCall(ARTSRTL_artsEventSatisfySlot,
                    {eventGuid, dbGuid, ARTS_EVENT_LATCH_DECR_SLOT}, loc);
}

void ArtsCodegen::addDep(Value eventGuid, Value edtGuid, Value edtSlot,
                         Location loc) {
  createRuntimeCall(ARTSRTL_artsAddDependence, {eventGuid, edtGuid, edtSlot},
                    loc);
}

void ArtsCodegen::signalEdt(Value edtGuid, Value edtSlot, Value dbGuid,
                            Location loc) {
  createRuntimeCall(ARTSRTL_artsSignalEdt, {edtGuid, edtSlot, dbGuid}, loc);
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