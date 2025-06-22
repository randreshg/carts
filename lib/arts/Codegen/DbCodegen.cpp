///==========================================================================
/// File: DbCodegen.cpp
///==========================================================================

#include "arts/Codegen/DbCodegen.h"
#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsTypes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-codegen"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")
#define METADATA "-----------------------------------------\n[artsCodegen] "

using namespace mlir;
using namespace mlir::arts;

class ArtsCodegen;

/// DbAllocCodegen
DbAllocCodegen::DbAllocCodegen(ArtsCodegen &AC, arts::DbAllocOp dbOp,
                               Location loc)
    : DbCodegen(Kind::Alloc), AC(AC), builder(AC.getBuilder()), dbOp(dbOp) {
  create(dbOp, loc);
}

Value DbAllocCodegen::getOp() { return dbOp.getResult(); }
Value DbAllocCodegen::getEdtSlot() { return edtSlot; }
Value DbAllocCodegen::getGuid() { return guid; }
Value DbAllocCodegen::getPtr() { return ptr; }

bool DbAllocCodegen::hasSingleSize() { return dbOp.getSizes().size() <= 1; }

ValueRange DbAllocCodegen::getSizes() { return dbOp.getSizes(); }

ValueRange DbAllocCodegen::getOffsets() { return ValueRange{}; }

ValueRange DbAllocCodegen::getIndices() { return ValueRange{}; }

void DbAllocCodegen::setEdtSlot(Value v) { edtSlot = v; }
void DbAllocCodegen::setGuid(Value v) { guid = v; }
void DbAllocCodegen::setPtr(Value v) { ptr = v; }

bool DbAllocCodegen::isOutMode() {
  StringRef mode = dbOp.getMode();
  return (mode == "out" || mode == "inout");
}

bool DbAllocCodegen::isInMode() {
  StringRef mode = dbOp.getMode();
  return (mode == "in" || mode == "inout");
}

void DbAllocCodegen::create(arts::DbAllocOp depOp, Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);

  auto currentNode = AC.getCurrentNode(loc);
  Value elementTypeSize;
  const auto tySize = AC.castToInt(AC.Int64, elementTypeSize, loc);

  /// Handle the case of a single db
  if (hasSingleSize()) {
    AC.createPrintfCall(loc, METADATA "Creating single DB\n", {});
    guid = createGuid(currentNode, getMode(), loc);
    ptr = AC.createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                               {guid, tySize}, loc)
              ->getResult(0);
    return;
  }

  /// Create an array of guids and pointers based on the sizes of the dbOp
  AC.createPrintfCall(loc, METADATA "Creating array of DBs\n", {});
  auto dbSizes = dbOp.getSizes();
  const auto dbDim = dbSizes.size();
  auto guidType = MemRefType::get(
      SmallVector<int64_t>(dbDim, ShapedType::kDynamic), AC.ArtsGuid);
  guid = builder.create<memref::AllocaOp>(loc, guidType, dbSizes);
  auto ptrType = dbOp.getResult().getType().cast<MemRefType>();
  if (ptrType.hasStaticShape()) {
    ptr = builder.create<memref::AllocaOp>(loc, ptrType);
  } else {
    ptr = builder.create<memref::AllocaOp>(loc, ptrType, dbSizes);
  }

  /// Flatten loops and batch GUID creation for better performance
  std::function<void(unsigned, SmallVector<Value, 4> &)> createDbs =
      [&](unsigned dim, SmallVector<Value, 4> &indices) {
        if (dim == dbDim) {
          auto guidVal = createGuid(currentNode, getMode(), loc);
          auto ptrVal =
              AC.createRuntimeCall(types::ARTSRTL_artsDbCreateWithGuid,
                                   {guidVal, tySize}, loc)
                  ->getResult(0);
          builder.create<memref::StoreOp>(loc, guidVal, guid, indices);
          builder.create<memref::StoreOp>(loc, ptrVal, ptr, indices);
          return;
        }

        Value lowerBound = AC.createIndexConstant(0, loc);
        Value upperBound = dbSizes[dim];
        Value step = AC.createIndexConstant(1, loc);

        auto loopOp =
            builder.create<scf::ForOp>(loc, lowerBound, upperBound, step);
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

Value DbAllocCodegen::getMode() {
  auto Loc = UnknownLoc::get(builder.getContext());
  /// Values from Arts.h
  /// ARTS_DB_PIN = 10
  auto enumValue = 10;
  /// ARTS_DB_READ = 8
  if (isInMode())
    enumValue = 8;
  return AC.createIntConstant(enumValue, AC.Int32, Loc);
}

Value DbAllocCodegen::createGuid(Value node, Value mode, Location loc) {
  auto reserveGuidCall = AC.createRuntimeCall(
      types::ARTSRTL_artsReserveGuidRoute, {mode, node}, loc);
  return reserveGuidCall.getResult(0);
}

// DbAccessCodegen implementation
DbAccessCodegen::DbAccessCodegen(ArtsCodegen &AC, arts::DbControlOp dbOp,
                                 Location loc)
    : DbCodegen(Kind::Access), AC(AC), builder(AC.getBuilder()), dbOp(dbOp) {}

Value DbAccessCodegen::getOp() { return dbOp.getResult(); }
Value DbAccessCodegen::getEdtSlot() { return edtSlot; }
Value DbAccessCodegen::getGuid() { return guid; }
Value DbAccessCodegen::getPtr() { return ptr; }

bool DbAccessCodegen::hasSingleSize() { return dbOp.getSizes().size() <= 1; }
ValueRange DbAccessCodegen::getSizes() { return dbOp.getSizes(); }
ValueRange DbAccessCodegen::getOffsets() { return dbOp.getOffsets(); }
ValueRange DbAccessCodegen::getIndices() { return dbOp.getIndices(); }
void DbAccessCodegen::setEdtSlot(Value v) { edtSlot = v; }
void DbAccessCodegen::setGuid(Value v) { guid = v; }
void DbAccessCodegen::setPtr(Value v) { ptr = v; }

bool DbAccessCodegen::isOutMode() {
  StringRef mode = dbOp.getModeAttr().getValue();
  return (mode == "out" || mode == "inout");
}

bool DbAccessCodegen::isInMode() {
  StringRef mode = dbOp.getModeAttr().getValue();
  return (mode == "in" || mode == "inout");
}