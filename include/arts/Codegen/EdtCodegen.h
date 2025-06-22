///==========================================================================
/// File: EdtCodegen.h
///==========================================================================
#ifndef CARTS_CODEGEN_EDTCODEGEN_H
#define CARTS_CODEGEN_EDTCODEGEN_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <vector>

namespace mlir {
namespace arts {

class ArtsCodegen;
class DbCodegen;

class EdtCodegen {
public:
  EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps = nullptr,
             Region *region = nullptr, Value *epoch = nullptr,
             Location *loc = nullptr, bool buildEdt = false);
  void build(Location loc);
  func::FuncOp getFunc();
  Value getGuid();
  Value getNode();
  SmallVector<Value> &getParams();
  void setFunc(func::FuncOp func);
  void setGuid(Value guid);
  void setNode(Value node);
  void setParams(SmallVector<Value> params);
  void setDeps(SmallVector<Value> deps);
  void setDepC(Value depC);
  std::pair<bool, int> insertParam(Value param);

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  Region *region = nullptr;
  Value *epoch = nullptr;
  func::FuncOp func = nullptr;
  Value guid = nullptr;
  Value node = nullptr;
  Value paramC = nullptr;
  Value paramV = nullptr;
  Value depC = nullptr;
  llvm::SmallVector<Value> deps, params, consts;
  llvm::DenseMap<Value, Value> rewireMap;
  func::ReturnOp returnOp = nullptr;
  bool built = false;
  llvm::SmallVector<DbCodegen *> depsToSatisfy, depsToRecord;
  struct DbEntry {
    Value guid, ptr;
    llvm::SmallVector<Value> sizes, offsets;
    llvm::DenseMap<unsigned, unsigned> sizeIndex;
    llvm::DenseMap<unsigned, unsigned> offsetIndex;
  };
  llvm::DenseMap<DbCodegen *, DbEntry> entryDbs;
  llvm::DenseMap<Value, unsigned> entryEvents;
  Value fnParamV = nullptr;
  Value fnDepV = nullptr;
  void process(Location loc);
  void processDependencies(Location loc);
  void processSubviewDependency(Value subview, Location loc);
  void outlineRegion(Location loc);
  Value createGuid(Value node, Location loc);
  func::FuncOp createFn(Location loc);
  void createEntry(Location loc);
  static unsigned edtCounter;
  static unsigned increaseEdtCounter() { return ++EdtCodegen::edtCounter; }
};

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_EDTCODEGEN_H