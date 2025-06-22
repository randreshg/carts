///==========================================================================
/// File: DbCodegen.h
///==========================================================================
#ifndef CARTS_CODEGEN_DBCODEGEN_H
#define CARTS_CODEGEN_DBCODEGEN_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Location.h"
#include "arts/ArtsDialect.h"
#include <vector>

namespace mlir {
namespace arts {

class ArtsCodegen;

class DbCodegen {
public:
  enum class Kind { Alloc, Access };
  DbCodegen(Kind kind) : kind(kind) {}
  virtual ~DbCodegen() = default;
  Kind getKind() const { return kind; }
  virtual Value getOp() = 0;
  virtual Value getEdtSlot() = 0;
  virtual Value getGuid() = 0;
  virtual Value getPtr() = 0;
  virtual bool hasSingleSize() = 0;
  virtual ValueRange getSizes() = 0;
  virtual ValueRange getOffsets() = 0;
  virtual ValueRange getIndices() = 0;
  virtual void setEdtSlot(Value) = 0;
  virtual void setGuid(Value) = 0;
  virtual void setPtr(Value) = 0;
  virtual bool isOutMode() = 0;
  virtual bool isInMode() = 0;
protected:
  Kind kind;
  Value edtSlot = nullptr;
  Value guid = nullptr;
  Value ptr = nullptr;
};

class DbAllocCodegen : public DbCodegen {
public:
  DbAllocCodegen(ArtsCodegen &AC, arts::DbAllocOp dbOp, Location loc);
  Value getOp() override;
  Value getEdtSlot() override;
  Value getGuid() override;
  Value getPtr() override;
  bool hasSingleSize() override;
  ValueRange getSizes() override;
  ValueRange getOffsets() override;
  ValueRange getIndices() override;
  void setEdtSlot(Value v) override;
  void setGuid(Value v) override;
  void setPtr(Value v) override;
  bool isOutMode() override;
  bool isInMode() override;
  void create(arts::DbAllocOp dbOp, Location loc);
private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  arts::DbAllocOp dbOp;
  Value createGuid(Value node, Value mode, Location loc);
  Value getMode();
};

class DbAccessCodegen : public DbCodegen {
public:
  DbAccessCodegen(ArtsCodegen &AC, arts::DbControlOp dbOp, Location loc);
  Value getOp() override;
  Value getEdtSlot() override;
  Value getGuid() override;
  Value getPtr() override;
  bool hasSingleSize() override;
  ValueRange getSizes() override;
  ValueRange getOffsets() override;
  ValueRange getIndices() override;
  void setEdtSlot(Value v) override;
  void setGuid(Value v) override;
  void setPtr(Value v) override;
  bool isOutMode() override;
  bool isInMode() override;
private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  arts::DbControlOp dbOp;
};

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_DBCODEGEN_H 