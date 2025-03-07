// ArtsCodegen.h
#ifndef MLIR_CODEGEN_ARTSCODEGEN_H
#define MLIR_CODEGEN_ARTSCODEGEN_H

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsTypes.h"
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/Conversion/LLVMCommon/MemRefBuilder.h"
/// Other
#include "mlir/Conversion/LLVMCommon/MemRefBuilder.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include <sys/types.h>

namespace mlir {
namespace arts {
using namespace mlir::LLVM;
using namespace types;
class ArtsCodegen;

// ---------------------------- Datablocks ---------------------------- ///
class DataBlockCodegen {
public:
  DataBlockCodegen(ArtsCodegen &AC);
  DataBlockCodegen(ArtsCodegen &AC, arts::DataBlockOp dbOp, Location loc);

  /// Getters
  Value getOp() { return dbOp; }
  Value getEdtSlot() { return edtSlot; }
  Value getGuid() { return guid; }
  StringRef getMode() { return dbOp.getMode(); }
  Value getInEvent() { return dbOp.getInEvent(); }
  Value getOutEvent() { return dbOp.getOutEvent(); }
  bool hasEvent() { return !dbOp.getInEvent() || !dbOp.getOutEvent(); }
  bool hasPtrDb() { return dbOp.hasPtrDb(); }
  bool isSingle() { return dbOp.isSingle(); }
  ValueRange getIndices() { return dbOp.getIndices(); }
  ValueRange getSizes() { return dbOp.getSizes(); }

  /// Setters
  void setEdtSlot(Value edtSlot) { this->edtSlot = edtSlot; }

  /// Interface
  void create(arts::DataBlockOp dbOp, Location loc);
  bool isOutMode() {
    StringRef mode = dbOp.getMode();
    return mode == "out" || mode == "inout";
  }
  bool isInMode() {
    return dbOp.getMode() == "in" || dbOp.getMode() == "inout";
  }

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  DataBlockOp dbOp = nullptr;
  Value edtSlot = nullptr;

  /// DataBlock info
  Value guid = nullptr;
  Value ptr = nullptr;

  /// Utils
  Value createGuid(Value node, Value mode, Location loc);
  Value getMode(StringRef mode);
};

// ---------------------------- EDTs ---------------------------- ///
class EdtCodegen {
public:
  EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps = nullptr,
             Region *region = nullptr, Value *epoch = nullptr,
             Location *loc = nullptr, bool buildEdt = false);
  void build(Location loc);

  /// Getters
  func::FuncOp getFunc() { return func; }
  Value getGuid() { return guid; }
  Value getNode() { return node; }
  SmallVector<Value> &getParams() { return params; }

  /// Setters
  void setFunc(func::FuncOp func) { this->func = func; }
  void setGuid(Value guid) { this->guid = guid; }
  void setNode(Value node) { this->node = node; }
  void setParams(SmallVector<Value> params) { this->params = params; }
  void setDeps(SmallVector<Value> deps) { this->deps = deps; }
  void setDepC(Value depC) { this->depC = depC; }

  /// Interface
  int insertParam(Value param) {
    params.push_back(param);
    return params.size() - 1;
  }
  void addParam(Value param) { params.push_back(param); }
  void addDep(Value dep) { deps.push_back(dep); }

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
  SmallVector<Value> deps, params, consts;
  DenseMap<Value, Value> rewireMap;
  func::ReturnOp returnOp = nullptr;
  bool built = false;
  /// Dependencies info
  SmallVector<DataBlockCodegen *> depsToSatisfy, depsToRecord, depsToSignal;

  /// Entry info
  struct DatablockEntry {
    Value guid, ptr;
    SmallVector<Value> sizes;
    DenseMap<unsigned, unsigned> sizeIndex;
  };
  DenseMap<arts::DataBlockCodegen *, DatablockEntry> entryDbs;
  /// Entry events
  DenseMap<Value, unsigned> entryEvents;
  /// Entry Function arguments
  Value fnParamV = nullptr;
  Value fnDepV = nullptr;

  /// Utils
  void process(Location loc);
  void processDependencies(Location loc);
  void outlineRegion(Location loc);
  Value createGuid(Value node, Location loc);
  func::FuncOp createFn(Location loc);
  void createFnEntry(Location loc);

  /// Static
  static unsigned edtCounter;
  static unsigned increaseEdtCounter() { return ++EdtCodegen::edtCounter; }
};

// ---------------------------- Events ---------------------------- ///
class EventCodegen {
public:
  EventCodegen(ArtsCodegen &AC, arts::EventOp eventOp, Location loc);

  /// Getters
  Value getGuid() { return guid; }
  ValueRange getSizes() { return eventOp.getSizes(); }

  /// Interface
  void create(arts::EventOp eventOp, Location loc);

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  EventOp eventOp = nullptr;
  Value guid = nullptr;
};

// ---------------------------- ARTS Codegen ---------------------------- ///
class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
              mlir::DataLayout &mlirDL);
  ~ArtsCodegen();

  /// Friend classes
  friend class DataBlockCodegen;
  friend class EventCodegen;
  friend class EdtCodegen;
  friend class ArtsTypes;

  /// Get or create a runtime function
  func::FuncOp getOrCreateRuntimeFunction(RuntimeFunction FnID);
  /// Generate a call to a runtime function
  func::CallOp createRuntimeCall(RuntimeFunction FnID, ArrayRef<Value> args,
                                 Location loc);
  /// Builder
  OpBuilder &getBuilder() { return builder; }

  /// Datablock
  DataBlockCodegen *getDatablock(Value op);
  DataBlockCodegen *getDatablock(arts::DataBlockOp dbOp);
  DataBlockCodegen *createDatablock(arts::DataBlockOp dbOp, Location loc);
  DataBlockCodegen *getOrCreateDatablock(arts::DataBlockOp dbOp, Location loc);

  /// Events
  // Value allocEvent(arts::EventOp allocEventOp, Location loc);
  EventCodegen *getEvent(arts::EventOp eventOp);
  EventCodegen *getOrCreateEvent(arts::EventOp eventOp, Location loc);

  /// Edts
  EdtCodegen *getEdt(Region *region);
  EdtCodegen *createEdt(SmallVector<Value> *opDeps = nullptr,
                        Region *region = nullptr, Value *epoch = nullptr,
                        Location *loc = nullptr, bool build = true);

  /// Epoch
  Value createEpoch(Value finishEdtGuid, Value finishEdtSlot, Location loc);

  /// Utils
  Value getGuidFromDb(Value db, Location loc);
  Value getPtrFromDb(Value db, Location loc);
  Value getGuidFromEdtDep(Value dep, Location loc);
  Value getPtrFromEdtDep(Value dep, Location loc);
  Value getCurrentEpochGuid(Location loc);
  Value getCurrentEdtGuid(Location loc);
  Value getCurrentNode(Location loc);
  void satisfyDep(Value eventGuid, Value depGuid, Location loc);
  void addDep(Value eventGuid, Value edtGuid, Value edtSlot, Location loc);
  void signalEdt(Value edtGuid, Value edtSlot, Value dbGuid, Location loc);
  void waitOnHandle(Value epochGuid, Location loc);
  func::FuncOp insertInitPerWorker(Location loc);
  func::FuncOp insertInitPerNode(Location loc, func::FuncOp callback = nullptr);
  func::FuncOp insertMain(Location loc);
  void initializeRuntime(Location loc);

  /// Helpers
  Value createFnPtr(func::FuncOp funcOp, Location loc);
  Value createIntConstant(int value, Type type, Location loc);
  Value createIndexConstant(int value, Location loc);
  Value createPtr(Value source, Location loc);

  /// Casting
  Value castParameter(mlir::Type targetType, Value source, Location loc);
  Value castPointer(mlir::Type targetType, Value source, Location loc);
  Value castToIndex(Value source, Location loc);
  Value castToFloat(mlir::Type targetType, Value source, Location loc);
  Value castToInt(mlir::Type targetType, Value source, Location loc);
  Value castToVoidPtr(Value source, Location loc);
  Value castToLLVMPtr(Value source, Location loc);

  /// Insertion point
  void setInsertionPoint(Operation *op) { builder.setInsertionPoint(op); }
  void setInsertionPointAfter(Operation *op) {
    builder.setInsertionPointAfter(op);
  }

  // ---------------------------- Types ---------------------------- ///
  ///{
#define ARTS_TYPE(VarName, InitValue) mlir::Type VarName = nullptr;
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  FunctionType VarName = nullptr;                                              \
  MemRefType VarName##Ptr = nullptr;
#define ARTS_STRUCT_TYPE(VarName, StructName, ...)                             \
  LLVMStructType VarName = nullptr;                                            \
  MemRefType VarName##Ptr = nullptr;
#include "ARTSKinds.def"
  LLVMPointerType llvmPtr = nullptr;
  ///}
private:
  void initializeTypes();

  // -------------------------- Other Attributes-------------------------- ///
  /// The MLIR module and builder
  ModuleOp &module;
  OpBuilder builder;
  llvm::DataLayout &llvmDL;
  mlir::DataLayout &mlirDL;
  /// Function counter
  unsigned edtCounter = 0;
  /// Map a arts::DataBlockOp to a DataBlockCodegen
  llvm::DenseMap<Value, DataBlockCodegen *> datablocks;
  /// Map an arts::EventOp to a EventCodegen
  llvm::DenseMap<Value, EventCodegen *> events;
  /// Map an arts region to a EdtCodegen
  llvm::DenseMap<Region *, EdtCodegen *> edts;
};

} // namespace arts
} // namespace mlir
#endif // MLIR_CODEGEN_ARTSCODEGEN_H