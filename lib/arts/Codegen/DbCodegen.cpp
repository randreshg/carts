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

// ARTS DB Mode Constants
namespace {
/// Taken from arts.h
constexpr int ARTS_DB_PIN = 10; // Pin mode for write access
constexpr int ARTS_DB_READ = 8; // Read-only mode
} // namespace

class ArtsCodegen;

/// DbAllocCodegen
DbAllocCodegen::DbAllocCodegen(ArtsCodegen &AC, arts::DbAllocOp dbOp,
                               Location loc)
    : AC(AC), builder(AC.getBuilder()), dbOp(dbOp) {
  LLVM_DEBUG(dbgs() << "DbAllocCodegen Constructor\n");
  /// Set parent Op to child op
  for (auto *op : dbOp->getUsers()) {
    if (auto dbDepOp = dyn_cast<arts::DbDepOp>(op)) {
      LLVM_DEBUG(dbgs() << " - DbDepOp: " << dbDepOp << "\n");
      auto dbDep = AC.getOrCreateDbDep(dbDepOp, dbOp);
      assert(dbDep && "DbDep could not be created");
    }
  }
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
  const auto tySize = calculateElementSize(loc);

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
  auto ptrType = dbOp.getResult().getType().dyn_cast<MemRefType>();
  assert(ptrType && "Expected a MemRefType");
  if (ptrType && ptrType.hasStaticShape())
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

Value DbAllocCodegen::calculateElementSize(Location loc) {
  /// Calculate the element type size
  /// For arts.db_alloc operations, the result type is memref<memref<?xi8>>
  /// We need to get the element type from the inner memref type
  auto resultType = dbOp.getResult().getType().dyn_cast<MemRefType>();
  if (!resultType) {
    // Handle non-MemRef types gracefully - return default element size
    return AC.createIntConstant(4, AC.Int64, loc); // Default to 4 bytes (i32)
  }
  auto innerMemrefType = resultType.getElementType().dyn_cast<MemRefType>();
  if (!innerMemrefType) {
    // If inner type is not a memref, use the element type directly
    auto elementSizeInBits =
        AC.getMLIRDataLayout().getTypeSizeInBits(resultType.getElementType());
    return AC.createIntConstant(elementSizeInBits / 8, AC.Int64, loc);
  }
  auto elementType = innerMemrefType.getElementType();
  auto elementSizeInBits =
      AC.getMLIRDataLayout().getTypeSizeInBits(elementType);
  return AC.createIntConstant(elementSizeInBits / 8, AC.Int64, loc);
}

Value DbAllocCodegen::getMode() {
  auto enumValue = ARTS_DB_PIN;
  if (isInMode())
    enumValue = ARTS_DB_READ;
  return AC.createIntConstant(enumValue, AC.Int32, AC.getUnknownLoc());
}

Value DbAllocCodegen::createGuid(Value node, Value mode, Location loc) {
  auto reserveGuidCall = AC.createRuntimeCall(
      types::ARTSRTL_artsReserveGuidRoute, {mode, node}, loc);
  return reserveGuidCall.getResult(0);
}

DbDepCodegen::DbDepCodegen(ArtsCodegen &AC, arts::DbDepOp dbOp,
                           Operation *parentOp)
    : AC(AC), builder(AC.getBuilder()), dbOp(dbOp), parentOp(parentOp) {
  auto edtParent = dbOp->getParentOfType<EdtOp>();
  if (parentOp->getParentOfType<EdtOp>() != edtParent)
    parentOpIsInSameEdt = false;
  assert(parentOp && "Parent op not found");
  LLVM_DEBUG(dbgs() << "  - DbDepCodegen Constructor\n");
  /// Propagate the parent op to the child op
  for (auto *op : dbOp->getUsers()) {
    if (auto dbDepOp = dyn_cast<arts::DbDepOp>(op)) {
      LLVM_DEBUG(dbgs() << "   - DbDepOp: " << dbDepOp << "\n");
      auto dbDep = AC.getOrCreateDbDep(dbDepOp, dbOp);
      assert(dbDep && "DbDep could not be created");
    }
  }
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

void DbDepCodegen::init(Location loc) {
  LLVM_DEBUG(dbgs() << "DbDepCodegen::init() - initializing DbDep for "
                    << dbOp);
  /// The init function sets the guid and ptr for the DbDep.
  if (guid || ptr)
    return;

  /// For DbDep operations, we need to extract the GUID from the source Db
  DbAllocCodegen *sourceDbAlloc = nullptr;
  DbDepCodegen *sourceDbDep = nullptr;
  if (auto sourceDbAllocOp = dyn_cast<arts::DbAllocOp>(parentOp)) {
    sourceDbAlloc = AC.getDbAlloc(sourceDbAllocOp);
    assert(sourceDbAlloc && "Source DbAlloc not found");
    /// If the source Db is in the same EDT, we can use the existing Db
    if (parentOpIsInSameEdt) {
      guid = sourceDbAlloc->getGuid();
      ptr = sourceDbAlloc->getPtr();
    } else {
      llvm_unreachable("Source DbAlloc is in a different EDT");
    }
  } else if (auto sourceDbDepOp = dyn_cast<arts::DbDepOp>(parentOp)) {
    sourceDbDep = AC.getDbDep(sourceDbDepOp);
    assert(sourceDbDep && "Source DbDep not found");
    /// If the source Db is in the same EDT, we can use the existing Db
    if (parentOpIsInSameEdt) {
      guid = sourceDbDep->getGuid();
      ptr = sourceDbDep->getPtr();
    }
    /// If the source Db is in a different EDT, we get the guid and ptr from
    /// the EDT context.
    else {
      guid = sourceDbDep->getGuidInEdt();
      ptr = sourceDbDep->getPtrInEdt();
      assert(guid && ptr &&
             "EDT context values not available yet for source "
             "DbDep");
    }
  } else {
    LLVM_DEBUG(DBGS() << "Source is not a DbAllocOp or DbDepOp: " << parentOp
                      << "\n"
                      << dbOp);
    llvm_unreachable("Source is not a DbAllocOp or DbDepOp");
  }
  /// Debug
  OpBuilder::InsertionGuard IG(builder);
  AC.setInsertionPoint(dbOp);
  AC.createPrintfCall(loc, METADATA "Created DbDep dependency\n", {});
}