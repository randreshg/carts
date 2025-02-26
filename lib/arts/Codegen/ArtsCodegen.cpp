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
  opPtr = dbOp.getPtr();
  isPtrDb = dbOp.isPtrDb();
  elementType = dbOp.getElementType();
  elementTypeSize = dbOp.getElementTypeSize();
  isSingle = dbOp.isSingle();

  /// If the base is a DB, it will be handled when inserting the EDT Entry
  if (isPtrDb) {
    LLVM_DEBUG(DBGS() << "Base is a datablock: " << dbOp << "\n");
    return;
  }

  /// Set insertion point to the DatablockOp
  OpBuilder::InsertionGuard guard(builder);
  AC.setInsertionPoint(dbOp);

  auto currentNode = AC.getCurrentNode(loc);
  /// Handle the case of a single datablock
  if (isSingle) {
    auto modeVal = getMode(dbOp.getMode());
    guid = createGuid(currentNode, modeVal, loc);
    ptr = AC.createRuntimeCall(
                ARTSRTL_artsDbCreateWithGuidAndData,
                {AC.castToVoidPtr(opPtr, loc), elementTypeSize, modeVal, guid},
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
// EventCodegen::EventCodegen(ArtsCodegen &AC) : AC(AC), builder(AC.builder) {}

// EventCodegen::EventCodegen(ArtsCodegen &AC, arts::EventOp eventOp, Location
// loc)
//     : AC(AC), builder(AC.builder) {
//   create(eventOp, loc);
// }

// void EventCodegen::create(arts::EventOp eventOp, Location loc) {
//   OpBuilder::InsertionGuard guard(builder);
//   AC.setInsertionPoint(eventOp);
//   isSingle = eventOp.isSingle();
//   indices = eventOp.getIndices();
//   /// If the event is a single event, load the guid
//   if (isSingle) {
//     guid = builder
//                .create<memref::LoadOp>(loc, eventOp.getSource(),
//                                        eventOp.getIndices())
//                .getResult();
//     return;
//   }

//   /// Otherwise, the guids are stored in an array.
//   /// No need to load them here, this will be done when processing the
//   /// EDT, since the events guids have to be loaded to be sent as
//   /// parameters to the EDT.
//   guid = eventOp.getSource();

//   // /// Create an array of guids based on the sizes of the eventOp
//   // auto sizes = eventOp.getSizes();
//   // auto guidType = MemRefType::get(
//   //     std::vector<int64_t>(sizes.size(), ShapedType::kDynamic),
//   AC.ArtsGuid);
//   // guid = builder.create<memref::AllocaOp>(loc, guidType, sizes);
//   // /// Create nested loops to create the guids
//   // std::function<void(unsigned, SmallVector<Value, 4> &)> createGuids =
//   //     [&](unsigned dim, SmallVector<Value, 4> &indices) {
//   //       if (dim == sizes.size()) {
//   //         auto guidVal =
//   //             builder.create<memref::LoadOp>(loc, eventOp.getSource(),
//   //             indices)
//   //                 .getResult();
//   //         builder.create<memref::StoreOp>(loc, guidVal, guid, indices);
//   //         return;
//   //       }
//   //       /// Create loop for current dimension
//   //       auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
//   //       auto upper = sizes[dim];
//   //       auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
//   //       auto loopOp = builder.create<scf::ForOp>(loc, lower, upper, step);
//   //       /// Set insertion point inside the loop body
//   //       auto &loopBlock = loopOp.getRegion().front();
//   //       builder.setInsertionPointToStart(&loopBlock);
//   //       /// Append the induction variable for the current dimension
//   //       indices.push_back(loopOp.getInductionVar());
//   //       /// Recurse to create the next level of loop
//   //       createGuids(dim + 1, indices);
//   //       /// Remove the current induction variable and reset insertion
//   point
//   //       indices.pop_back();
//   //       builder.setInsertionPointAfter(loopOp);
//   //     };
//   // SmallVector<Value, 4> indices;
//   // createGuids(0, indices);
// }

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
    params.append(envManager.getParameters().begin(),
                  envManager.getParameters().end());
    consts.append(envManager.getConstants().begin(),
                  envManager.getConstants().end());
  }
  deps = opDeps ? *opDeps : SmallVector<Value>();
  // events = opEvents ? *opEvents : SmallVector<Value>();
  process(curLoc);

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

  /// Create the rewriter and inline the region into the function.
  ConversionPatternRewriter rewriter(builder.getContext());
  createEntry(loc);
  Region &funcRegion = func.getBody();
  Block &funcEntryBlock = funcRegion.front();
  Block &entryBlock = region->front();
  rewriter.inlineRegionBefore(*region, funcRegion, funcRegion.end());

  /// Move all operations from entryBlock to funcEntryBlock.
  funcEntryBlock.getOperations().splice(funcEntryBlock.end(),
                                        entryBlock.getOperations());
  entryBlock.erase();

  /// Replace each arts.yield terminator with a return.
  func::ReturnOp returnOp = nullptr;
  for (auto &block : funcRegion) {
    if (auto yieldOp = dyn_cast<arts::YieldOp>(block.getTerminator())) {
      AC.setInsertionPoint(yieldOp);
      assert(!returnOp &&
             "Multiple yields in the same block are not supported");
      returnOp = builder.create<func::ReturnOp>(loc);
      yieldOp->erase();
    }
  }

  /// Satisfy dependencies
  // AC.setInsertionPoint(returnOp);
  // for (size_t i = 0, e = events.size(); i < e; ++i) {
  //   auto event = events[i];
  //   auto it = entryEvents.find(event);
  //   if (it == entryEvents.end())
  //     continue;
  //   auto eventGuid = rewireMap[params[it->second]];
  //   assert(eventGuid && "Event guid not found");
  //   DataBlockCodegen *db = AC.getDatablock(deps[i]);
  //   AC.satisfyDep(eventGuid, entryDbs[db].guid, loc);
  // }

  /// Clear rewireMap
  rewireMap.clear();
}

void EdtCodegen::process(Location loc) {
  /// Process dependencies
  if (!deps.empty()) {
    /// Initialize dependency count to zero.
    depC = builder.create<memref::AllocaOp>(loc, MemRefType::get({}, AC.Int32),
                                            ValueRange());
    builder.create<memref::StoreOp>(loc, AC.createIndexConstant(0, loc), depC);
    /// Process each dependency and add sizes as parameters
    for (const auto &dep : deps) {
      auto dbOp = dyn_cast<arts::DataBlockOp>(dep.getDefiningOp());
      assert(dbOp && "Dependency is not a datablock op");
      auto db = AC.getDatablock(dbOp);
      assert(db && "Datablock not found");

      int insertedSizes = 0;
      if (!db->isSingleDb()) {
        int sizeItr = 0;
        for (auto size : db->getSizes()) {
          auto sizeOp = size.getDefiningOp();
          /// If the size is a constant, skip it
          if (sizeOp && isa<arith::ConstantIndexOp>(sizeOp)) {
            ++sizeItr;
            continue;
          }
          /// Insert parameter and store the index where it was inserted
          entryDbs[db].sizeIndex[sizeItr] = insertParam(size);
          ++sizeItr;
          ++insertedSizes;
        }
      }
      auto add = builder
                     .create<arith::AddIOp>(
                         loc, depC, AC.createIndexConstant(insertedSizes, loc))
                     .getResult();
      builder.create<memref::StoreOp>(loc, add, depC);
    }
    depC = AC.castToInt(AC.Int32, depC, loc);
  }

  /// Process parameters.
  // unsigned paramSize = params.size();
  // paramC = builder.create<memref::AllocaOp>(loc, MemRefType::get({},
  // AC.Int32),
  //                                           ValueRange());
  // auto totalParamSize = builder.create<arith::AddIOp>(
  //     loc, AC.createIndexConstant(paramSize, loc),
  //     builder.create<memref::LoadOp>(loc, eventsCount));
  // builder.create<memref::StoreOp>(loc, totalParamSize, paramC);

  // auto totalParams = builder.create<memref::LoadOp>(loc, paramC);
  // auto paramVArray = builder.create<memref::AllocaOp>(
  //     loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64),
  //     ValueRange{totalParams});
  // for (unsigned i = 0; i < paramSize; ++i) {
  //   auto param = AC.castToInt(AC.Int64, params[i], loc);
  //   auto paramIndex = AC.createIndexConstant(i, loc);
  //   builder.create<affine::AffineStoreOp>(loc, param, paramVArray,
  //                                         ValueRange{paramIndex});
  // }

  // /// Process events
  // auto eventsCount = builder.create<memref::AllocaOp>(
  //     loc, MemRefType::get({}, AC.Int32), ValueRange());
  // builder.create<memref::StoreOp>(loc, AC.createIndexConstant(0, loc),
  //                                 eventsCount);
  // if (!events.empty()) {
  //   /// Helper to increment the event count.
  //   auto incEventCount = [&]() {
  //     auto newCount = builder
  //                         .create<arith::AddIOp>(loc, eventsCount,
  //                                                AC.createIndexConstant(1,
  //                                                loc))
  //                         .getResult();
  //     builder.create<memref::StoreOp>(loc, newCount, eventsCount);
  //   };

  //   for (auto event : events) {
  //     /// Skip if the defining op is not an arts::EventOp.
  //     auto eventOp = dyn_cast<arts::EventOp>(event.getDefiningOp());
  //     if (!eventOp)
  //       continue;

  //     /// If this is a single event, load its guid directly.
  //     if (eventOp.isSingle()) {
  //       builder.setInsertionPoint(eventOp);
  //       auto eventGuid =
  //           builder.create<memref::LoadOp>(loc, event, eventOp.getIndices())
  //               .getResult();
  //       insertParam(eventGuid);
  //       incEventCount();
  //       continue;
  //     }

  //     /// Otherwise, create nested loops to load the guids.
  //     auto sizes = eventOp.getSizes();
  //     const unsigned eventDim = sizes.size();

  //     /// Recursive lambda to generate loops.
  //     std::function<void(unsigned, SmallVector<Value, 4> &)> createEvents =
  //         [&](unsigned dim, SmallVector<Value, 4> &indices) {
  //           if (dim == eventDim) {
  //             auto eventGuid =
  //                 builder.create<memref::LoadOp>(loc, event, indices)
  //                     .getResult();
  //             insertParam(eventGuid);
  //             incEventCount();
  //             return;
  //           }
  //           auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
  //           auto upper = sizes[dim];
  //           auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
  //           auto loopOp = builder.create<scf::ForOp>(loc, lower, upper,
  //           step); auto &loopBlock = loopOp.getRegion().front();
  //           builder.setInsertionPointToStart(&loopBlock);
  //           indices.push_back(loopOp.getInductionVar());
  //           createEvents(dim + 1, indices);
  //           indices.pop_back();
  //           builder.setInsertionPointAfter(loopOp);
  //         };

  //     SmallVector<Value, 4> indices;
  //     createEvents(0, indices);
  //   }
  // }

  // /// Process parameters.
  // unsigned paramSize = params.size();
  // paramC = builder.create<memref::AllocaOp>(loc, MemRefType::get({},
  // AC.Int32),
  //                                           ValueRange());
  // auto totalParamSize = builder.create<arith::AddIOp>(
  //     loc, AC.createIndexConstant(paramSize, loc),
  //     builder.create<memref::LoadOp>(loc, eventsCount));
  // builder.create<memref::StoreOp>(loc, totalParamSize, paramC);

  // auto totalParams = builder.create<memref::LoadOp>(loc, paramC);
  // auto paramVArray = builder.create<memref::AllocaOp>(
  //     loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64),
  //     ValueRange{totalParams});
  // /// Create a loop from 0 to totalParams and store each parameter into
  // /// paramVArray.
  // auto lowerBound = builder.create<arith::ConstantIndexOp>(loc, 0);
  // auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
  // auto forOp = builder.create<scf::ForOp>(loc, lowerBound, totalParams,
  // step);
  // {
  //   /// Set the insertion point to the start of the loop body.
  //   Block &loopBody = forOp.getRegion().front();
  //   builder.setInsertionPointToStart(&loopBody);
  //   Value iv = forOp.getInductionVar();
  //   auto paramVal = builder.create<memref::LoadOp>(loc, paramsMemref, iv);
  //   // Store the parameter into the paramVArray at position iv.
  //   builder.create<memref::StoreOp>(loc, paramVal, paramVArray, iv);
  //   builder.create<scf::YieldOp>(loc);
  // }
  // paramV = builder.create<memref::CastOp>(
  //     loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64), paramVArray);

  // paramV = builder.create<memref::CastOp>(
  //     loc, MemRefType::get({ShapedType::kDynamic}, AC.Int64), paramVArray);
}

void EdtCodegen::createEntry(Location loc) {
  OpBuilder::InsertionGuard guard(builder);

  auto *entryBlock = func.addEntryBlock();
  builder.setInsertionPointToStart(entryBlock);

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

  /// Insert the dependencies
  auto indexAlloc = builder.create<memref::AllocaOp>(
      loc, MemRefType::get({}, builder.getIndexType()));
  auto zero = builder.create<arith::ConstantIndexOp>(loc, 0);
  builder.create<memref::StoreOp>(loc, zero, indexAlloc);
  Value oneIdx = builder.create<arith::ConstantIndexOp>(loc, 1);

  /// Helper to load the current index.
  auto loadIndex = [&]() -> Value {
    return builder.create<memref::LoadOp>(loc, ValueRange(indexAlloc));
  };

  for (auto &dep : deps) {
    /// Get DataBlock
    auto *db = AC.getDatablock(dep);
    assert(db && "Datablock not found");

    /// If the db is not an array, get the dependency ptr
    if (db->isSingleDb()) {
      auto curIndex = loadIndex();
      auto depVElem =
          builder.create<memref::LoadOp>(loc, fnDepV, ValueRange({curIndex}));
      auto entryGuid = AC.getGuidFromEdtDep(depVElem, loc);
      auto entryPtr = AC.getPtrFromEdtDep(depVElem, loc);
      entryDbs[db].guid = entryGuid;
      entryDbs[db].ptr = entryPtr;
      /// Increment the index.
      auto newIndex =
          builder.create<arith::AddIOp>(loc, curIndex, oneIdx).getResult();
      builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
      /// Add it to the rewire map.
      rewireMap[db->getOp()] = entryPtr;
      continue;
    }

    /// Get the entry sizes.
    SmallVector<Value> entrySizes;
    const auto &dbSizes = db->getSizes();
    entrySizes.reserve(dbSizes.size());

    for (unsigned i = 0, e = dbSizes.size(); i < e; ++i) {
      auto entryItr = entryDbs[db].sizeIndex.find(i);
      if (entryItr != entryDbs[db].sizeIndex.end()) {
        entrySizes.push_back(rewireMap[params[entryItr->second]]);
      } else {
        if (auto cstOp = dbSizes[i].getDefiningOp<arith::ConstantIndexOp>())
          entrySizes.push_back(
              builder.clone(*cstOp.getOperation())->getResult(0));
        else
          llvm_unreachable("Datablock size is not a constant");
      }
    }

    /// If it is an array, recover the array of guids and pointers.
    auto guidType = MemRefType::get(
        std::vector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.ArtsGuid);
    auto entryGuid =
        builder.create<memref::AllocaOp>(loc, guidType, entrySizes);
    auto ptrType = MemRefType::get(
        std::vector<int64_t>(entrySizes.size(), ShapedType::kDynamic),
        AC.VoidPtr);
    auto entryPtr = builder.create<memref::AllocaOp>(loc, ptrType, entrySizes);
    /// Recursively get the datablock entry info for each element.
    std::function<void(unsigned, SmallVector<Value, 4> &)> createDbs =
        [&](unsigned dim, SmallVector<Value, 4> &indices) {
          if (dim == entrySizes.size()) {
            auto curIndex = loadIndex();
            auto depVElem =
                builder
                    .create<memref::LoadOp>(loc, fnDepV, ValueRange({curIndex}))
                    .getResult();
            auto entryGuidElem = AC.getGuidFromEdtDep(depVElem, loc);
            auto entryPtrElem = AC.getPtrFromEdtDep(depVElem, loc);
            builder.create<memref::StoreOp>(loc, entryGuidElem, entryGuid,
                                            indices);
            builder.create<memref::StoreOp>(loc, entryPtrElem, entryPtr,
                                            indices);
            /// Increment the index.
            auto newIndex = builder.create<arith::AddIOp>(loc, curIndex, oneIdx)
                                .getResult();
            builder.create<memref::StoreOp>(loc, newIndex, indexAlloc);
            return;
          }
          /// Create loop for current dimension.
          auto lower = builder.create<arith::ConstantIndexOp>(loc, 0);
          auto upper = entrySizes[dim];
          auto step = builder.create<arith::ConstantIndexOp>(loc, 1);
          auto loopOp = builder.create<scf::ForOp>(loc, lower, upper, step);
          /// Set insertion point inside the loop body.
          auto &loopBlock = loopOp.getRegion().front();
          builder.setInsertionPointToStart(&loopBlock);
          /// Append the induction variable for the current dimension.
          indices.push_back(loopOp.getInductionVar());
          /// Recurse to create the next level of loop.
          createDbs(dim + 1, indices);
          /// Remove the current induction variable and reset insertion point.
          indices.pop_back();
          builder.setInsertionPointAfter(loopOp);
        };

    SmallVector<Value, 4> indices;
    createDbs(0, indices);
    entryDbs[db].guid = entryGuid;
    LLVM_DEBUG(DBGS() << "Entry guid: " << entryGuid << "\n");
    entryDbs[db].ptr = entryPtr;
    rewireMap[db->getOp()] = entryPtr;
  }

  /// Replace all uses in the region.
  replaceInRegion(*region, rewireMap, false);
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

ArtsCodegen::~ArtsCodegen() {
  for (auto &db : datablocks)
    delete db.second;
  for (auto &edt : edts)
    delete edt.second;
}

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
Value ArtsCodegen::allocEvent(arts::AllocEventOp allocEventOp, Location loc) {
  auto createEvent = [&](Value node, Location loc) -> Value {
    Value latchCount = createIntConstant(1, Int32, loc);
    auto eventVal =
        createRuntimeCall(ARTSRTL_artsEventCreate, {node, latchCount}, loc)
            .getResult(0);
    return eventVal;
  };

  OpBuilder::InsertionGuard guard(builder);
  setInsertionPoint(allocEventOp);
  auto node = getCurrentNode(loc);

  /// If the event is a single event, create it and return
  if (allocEventOp.isSingle())
    return createEvent(node, loc);

  /// Create an array of guids based on the sizes of the eventOp
  auto sizes = allocEventOp.getSizes();
  auto guidType = MemRefType::get(
      std::vector<int64_t>(sizes.size(), ShapedType::kDynamic), ArtsGuid);
  Value guid = builder.create<memref::AllocaOp>(loc, guidType, sizes);

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
  return guid;
}

// EventCodegen *ArtsCodegen::getEvent(arts::EventOp eventOp) {
//   if (!eventOp)
//     return nullptr;
//   auto it = events.find(eventOp);
//   return (it != events.end()) ? it->second : nullptr;
// }

// EventCodegen *ArtsCodegen::getOrCreateEvent(arts::EventOp eventOp,
//                                             Location loc) {
//   auto &event = events[eventOp];
//   if (!event)
//     event = new EventCodegen(*this, eventOp, loc);
//   return event;
// }

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
  func->setAttr("llvm.readnone", builder.getUnitAttr());
  ArrayRef<Value> args;
  auto callOp = builder.create<func::CallOp>(loc, func, args);
  return callOp.getResult(0);
  // return createRuntimeCall(ARTSRTL_artsGetCurrentGuid, {}, loc).getResult(0);
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
  llvm::errs() << "Unsupported type for casting to integer: "
               << source.getType() << "\n";
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