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
#include <functional>
#include <vector>

namespace mlir {
namespace arts {

class ArtsCodegen;
class DbAllocCodegen;
class DbDepCodegen;

class EdtCodegen {
public:
  //===--------------------------------------------------------------------===//
  // Public Interface
  //===--------------------------------------------------------------------===//

  EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps = nullptr,
             Region *region = nullptr, Value *epoch = nullptr,
             Location *loc = nullptr, bool buildEdt = false);

  void build(Location loc);

  /// Getters
  func::FuncOp getFunc();
  Value getGuid();
  Value getNode();
  SmallVector<Value> &getParams();

  /// Setters
  void setFunc(func::FuncOp func);
  void setGuid(Value guid);
  void setNode(Value node);
  void setParams(SmallVector<Value> params);
  void setDeps(SmallVector<Value> deps);
  void setDepC(Value depC);

  /// Parameter management
  std::pair<bool, int> insertParam(Value param);

private:
  /// Member Variables
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
  llvm::SmallVector<DbDepCodegen *> depsToSatisfy, depsToRecord;
  llvm::DenseMap<Value, unsigned> entryEvents;
  Value fnParamV = nullptr;
  Value fnDepV = nullptr;

  /// Cache parameter insertions to avoid duplicates across multiple calls
  llvm::DenseMap<Value, unsigned> parameterCache;

  /// Core Processing
  void process(Location loc);
  void analyzeDependencies(Location loc);
  void setupParameters(Location loc);
  void satisfyDependencies(Location loc);

  /// Function Creation and Outlining
  func::FuncOp createFn(Location loc);
  Value createGuid(Value node, Location loc);
  void createEntry(Location loc);
  void outlineRegion(Location loc);

  /// Entry Creation Helpers
  void handleSingleDep(DbDepCodegen *db, Value dep,
                                       Value indexAlloc,
                                       const Value &depStructSize,
                                       const Value &fnDepVPtr, Location loc);
  void handleMultiDep(DbDepCodegen *db, Value dep,
                                      Value indexAlloc,
                                      const Value &depStructSize,
                                      const Value &fnDepVPtr, Location loc);

  /// Parameter Insertion Helpers
  void insertValueAsParameter(Value value, uint64_t index,
                              const std::function<void(unsigned)> &setIndex);
  void insertSizeParameter(DbDepCodegen *dbDep, Value size, uint64_t sizeIdx);
  void insertOffsetParameter(DbDepCodegen *dbDep, Value offset,
                             uint64_t offsetIdx);

  /// Dependency Processing Helpers
  void recordInDeps(Location loc);
  void incrementOutLatchCounts(Location loc);
  void replaceEdtDepUses(Location loc);

  /// Datablock Processing Helpers
  void recordSingleInDep(DbDepCodegen *dbCG, Location loc);
  void recordMultiInDep(DbDepCodegen *dbCG, Location loc);
  void incrementSingleOutDep(DbDepCodegen *dbCG, Location loc);
  void incrementMultiOutDep(DbDepCodegen *dbCG, Location loc);

  /// Recursive Datablock Processing
  void addDepsForMultiDb(Value dbGuid, Value guid, Value inSlotAlloc,
                                    const SmallVector<Value> &dbSizes,
                                    const SmallVector<Value> &dbOffsets,
                                    const SmallVector<Value> &dbIndices,
                                    Location loc);
  void incrementLatchCountsForMultiDb(Value dbGuid, Value outSlotAlloc,
                                         const SmallVector<Value> &dbSizes,
                                         const SmallVector<Value> &dbOffsets,
                                         const SmallVector<Value> &dbIndices,
                                         Location loc);

  /// Static Members
  static unsigned edtCounter;
  static unsigned increaseEdtCounter() { return ++EdtCodegen::edtCounter; }
};

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_EDTCODEGEN_H