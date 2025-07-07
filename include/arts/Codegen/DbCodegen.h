///==========================================================================
/// File: DbCodegen.h
///==========================================================================
#ifndef CARTS_CODEGEN_DBCODEGEN_H
#define CARTS_CODEGEN_DBCODEGEN_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include <vector>

namespace mlir {
namespace arts {

class ArtsCodegen;

/// DbAllocCodegen - Handles datablock allocation (local creation)
/// No EDT context needed - when used in EDT context, consumer sees it as
/// DbDepOp
class DbAllocCodegen {
public:
  DbAllocCodegen(ArtsCodegen &AC, arts::DbAllocOp dbOp, Location loc);

  Value getOp();
  Value getEdtSlot();
  Value getGuid();
  Value getPtr();
  bool hasSingleSize();
  ValueRange getSizes();
  void setEdtSlot(Value v);
  void setGuid(Value v);
  void setPtr(Value v);
  bool isOutMode();
  bool isInMode();
  void create(arts::DbAllocOp dbOp, Location loc);

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  arts::DbAllocOp dbOp;
  Value edtSlot = nullptr;
  Value guid = nullptr;
  Value ptr = nullptr;

  Value createGuid(Value node, Value mode, Location loc);
  Value getMode();
  Value calculateElementSize(Location loc);
};

/// DbDepCodegen - Handles datablock dependencies (accessing existing DBs from
/// depv)
///
/// EDT Context Semantics:
/// - "InEdt" means in the EDT that USES/CONSUMES the datablock, not the EDT
/// that creates it
/// - When a DbAllocOp is used in an EDT context, it's ALWAYS accessed through a
/// DbDepOp
/// - The consumer EDT sees it as a DbDepOp, not a DbAllocOp
/// - This class contains all EDT context information for accessing datablocks
/// from depv parameters
class DbDepCodegen {
public:
  DbDepCodegen(ArtsCodegen &AC, arts::DbDepOp dbOp,
               Operation *parentOp = nullptr);

  Value getOp();
  Value getEdtSlot();
  Value getGuid();
  Value getPtr();
  void setEdtSlot(Value v);
  void setGuid(Value v);
  void setPtr(Value v);
  bool isOutMode();
  bool isInMode();
  bool hasSingleSize();
  ValueRange getSizes();
  ValueRange getOffsets();
  ValueRange getIndices();
  void init(Location loc);

  /// Parent op
  bool isParentOpInSameEdt();

  /// EDT context values (set once during EDT entry construction)
  Value getGuidInEdt() { return guidInEdt; }
  Value getPtrInEdt() { return ptrInEdt; }
  void setGuidInEdt(Value v) { guidInEdt = v; }
  void setPtrInEdt(Value v) { ptrInEdt = v; }

  /// EDT context information for accessing existing DBs from depv
  llvm::SmallVector<Value> &getSizesInEdt() { return sizesInEdt; }
  llvm::SmallVector<Value> &getOffsetsInEdt() { return offsetsInEdt; }
  llvm::DenseMap<unsigned, unsigned> &getSizeIndexInEdt() {
    return sizeIndexInEdt;
  }
  llvm::DenseMap<unsigned, unsigned> &getOffsetIndexInEdt() {
    return offsetIndexInEdt;
  }

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  arts::DbDepOp dbOp;
  Value edtSlot = nullptr;
  Value guid = nullptr;
  Value ptr = nullptr;

  /// Parent op
  Operation *parentOp = nullptr;
  bool parentOpIsInSameEdt = true;

  /// EDT context values - for accessing existing datablocks from depv
  /// parameters
  Value guidInEdt = nullptr; // GUID in EDT consumer context
  Value ptrInEdt = nullptr;  // Pointer in EDT consumer context

  /// EDT context data for multi-dimensional access from depv parameters
  llvm::SmallVector<Value> sizesInEdt;
  llvm::SmallVector<Value> offsetsInEdt;
  llvm::DenseMap<unsigned, unsigned> sizeIndexInEdt;
  llvm::DenseMap<unsigned, unsigned> offsetIndexInEdt;
};

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_DBCODEGEN_H