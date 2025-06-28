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
    : AC(AC), builder(AC.getBuilder()), dbOp(dbOp) {
  create(dbOp, loc);
}

Value DbAllocCodegen::getOp() { return dbOp.getResult(); }
Value DbAllocCodegen::getEdtSlot() { return edtSlot; }
Value DbAllocCodegen::getGuid() { return guid; }
Value DbAllocCodegen::getPtr() { return ptr; }

bool DbAllocCodegen::hasSingleSize() { return dbOp.getSizes().size() <= 1; }

ValueRange DbAllocCodegen::getSizes() { return dbOp.getSizes(); }

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

  /// Calculate the element type size
  /// For arts.db_alloc operations, the result type is memref<memref<?xi8>>
  /// We need to get the element type from the inner memref type
  auto resultType = dbOp.getResult().getType().cast<MemRefType>();
  auto innerMemrefType = resultType.getElementType().cast<MemRefType>();
  auto elementType = innerMemrefType.getElementType();
  auto elementSizeInBits =
      AC.getMLIRDataLayout().getTypeSizeInBits(elementType);
  auto elementSizeValue =
      AC.createIntConstant(elementSizeInBits / 8, AC.Int64, loc);
  const auto tySize = elementSizeValue;

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
  if (ptrType.hasStaticShape())
    ptr = builder.create<memref::AllocaOp>(loc, ptrType);
  else
    ptr = builder.create<memref::AllocaOp>(loc, ptrType, dbSizes);

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

// DbDepCodegen implementation
DbDepCodegen::DbDepCodegen(ArtsCodegen &AC, arts::DbDepOp dbOp, Location loc)
    : AC(AC), builder(AC.getBuilder()), dbOp(dbOp) {
  create(dbOp, loc);
}

Value DbDepCodegen::getOp() { return dbOp.getResult(); }
Value DbDepCodegen::getEdtSlot() { return edtSlot; }
Value DbDepCodegen::getGuid() { return guid; }
Value DbDepCodegen::getPtr() { return ptr; }

bool DbDepCodegen::hasSingleSize() { return dbOp.getSizes().size() <= 1; }
ValueRange DbDepCodegen::getSizes() { return dbOp.getSizes(); }
ValueRange DbDepCodegen::getOffsets() { return dbOp.getOffsets(); }
ValueRange DbDepCodegen::getIndices() { return dbOp.getIndices(); }
void DbDepCodegen::setEdtSlot(Value v) { edtSlot = v; }
void DbDepCodegen::setGuid(Value v) { guid = v; }
void DbDepCodegen::setPtr(Value v) { ptr = v; }

bool DbDepCodegen::isOutMode() {
  StringRef mode = dbOp.getModeAttr().getValue();
  return (mode == "out" || mode == "inout");
}

bool DbDepCodegen::isInMode() {
  StringRef mode = dbOp.getModeAttr().getValue();
  return (mode == "in" || mode == "inout");
}

void DbDepCodegen::create(arts::DbDepOp depOp, Location loc) {
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);

  /// Check if GUID and pointer are already set (e.g., from depv processing in
  /// EdtCodegen)
  if (guid && ptr) {
    LLVM_DEBUG(
        DBGS() << "DbDep already has GUID/ptr from depv - skipping creation\n");
    return;
  }

  /// For DbDep operations, we need to extract the GUID and pointer
  /// from the source datablock (which should be a DbAllocOp or another DbDepOp)
  Value source = dbOp.getSource();
  DbAllocCodegen *sourceDbAlloc = nullptr;
  DbDepCodegen *sourceDbDep = nullptr;
  if (auto sourceDbAllocOp = source.getDefiningOp<arts::DbAllocOp>()) {
    sourceDbAlloc = AC.getDbAlloc(sourceDbAllocOp);
    assert(sourceDbAlloc && "Source DbAlloc not found");
  } else if (auto sourceDbDepOp = source.getDefiningOp<arts::DbDepOp>()) {
    sourceDbDep = AC.getDbDep(sourceDbDepOp);
    assert(sourceDbDep && "Source DbDep not found");
  }
  else
    llvm_unreachable("Source is not a DbAllocOp or DbDepOp");

  /// Set GUID from the source datablock if not already set
  if (!guid)
    guid = sourceDbAlloc ? sourceDbAlloc->getGuid() : sourceDbDep->getGuid();

  /// For dependencies, we may need to create a subview of the source
  if (!ptr) {
    auto resultType = dbOp.getResult().getType().cast<MemRefType>();
    auto sourceType = source.getType().cast<MemRefType>();
    Value sourcePtr =
        sourceDbAlloc ? sourceDbAlloc->getPtr() : sourceDbDep->getPtr();

    /// If the types match exactly, just use the source pointer
    if (resultType == sourceType) {
      ptr = sourcePtr;
    } else {
      /// Create a subview with the provided offsets and sizes
      auto offsets = dbOp.getOffsets();
      auto sizes = dbOp.getSizes();
      if (!offsets.empty() && !sizes.empty()) {
        SmallVector<Value> strides;
        for (size_t i = 0; i < sizes.size(); ++i)
          strides.push_back(AC.createIndexConstant(1, loc));

        ptr = builder.create<memref::SubViewOp>(loc, resultType, sourcePtr,
                                                offsets, sizes, strides);
      } else {
        /// Fallback to cast
        ptr = builder.create<memref::CastOp>(loc, resultType, sourcePtr);
      }
    }
  }

  AC.createPrintfCall(loc, METADATA "Created DbDep dependency\n", {});
}