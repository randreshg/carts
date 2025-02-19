// ArtsCodegen.h
#ifndef LLVM_ARTS_CODEGEN_H
#define LLVM_ARTS_CODEGEN_H

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
  Value getGuid() { return entryGuid ? entryGuid : guid; }
  Value getPtr() { return entryPtr ? entryPtr : memref; }
  Type getElementType() { return elementType; }
  Value getElementTypeSize() { return elementTypeSize; }
  bool isBaseDb() { return ptrIsDb; }
  bool isArray() { return dbIsArray; }
  Value getEntryGuid() { return entryGuid; }
  Value getEntryPtr() { return entryPtr; }
  DenseMap<unsigned, unsigned> &getEntrySizes() { return entrySizes; }
  ValueRange getIndices() { return dbOp.getIndices(); }
  ValueRange getSizes() { return dbOp.getSizes(); }

  /// Setters
  void setEntryInfo(Value entryGuid, Value entryPtr) {
    this->entryGuid = entryGuid;
    this->entryPtr = entryPtr;
  }

  void insertEntrySize(unsigned sizeIndex, unsigned paramIndex) {
    entrySizes[sizeIndex] = paramIndex;
  }

  /// Interface
  void create(arts::DataBlockOp dbOp, Location loc);

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  DataBlockOp dbOp = nullptr;
  /// DataBlock info
  Value guid = nullptr;
  Value ptr = nullptr;
  /// Op info
  Value memref = nullptr;
  Type elementType = nullptr;
  Value elementTypeSize = nullptr;
  bool ptrIsDb = false;
  bool dbIsArray = false;
  /// Uses in entry
  Value entryGuid = nullptr;
  Value entryPtr = nullptr;
  /// Maps a size index to the index of the parameter in the EDT user entry
  DenseMap<unsigned, unsigned> entrySizes;

  /// Utils
  Value createGuid(Value node, Value mode, Location loc);
  Value getMode(StringRef mode);
};

// ---------------------------- EDTs ---------------------------- ///
class EdtCodegen {
public:
  EdtCodegen(ArtsCodegen &AC, SmallVector<Value> *opDeps = nullptr,
             Region *region = nullptr, Value *epoch = nullptr,
             Location *loc = nullptr, bool build = false);
  void build(Location loc);

  /// Getters
  func::FuncOp getFunc() { return func; }
  Value getGuid() { return guid; }
  Value getSlot() { return slot; }
  Value getNode() { return node; }
  SmallVector<Value> &getParams() { return params; }

  /// Setters
  void setFunc(func::FuncOp func) { this->func = func; }
  void setGuid(Value guid) { this->guid = guid; }
  void setSlot(Value slot) { this->slot = slot; }
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
  Value slot = nullptr;
  Value node = nullptr;
  Value paramC = nullptr;
  Value paramV = nullptr;
  Value depC = nullptr;
  SmallVector<Value> deps;
  SmallVector<Value> params;
  SmallVector<Value> consts;

  /// Utils
  void processDepsAndParams(SmallVector<Value> *deps, Location loc);
  Value createGuid(Value node, Location loc);
  func::FuncOp createFn(Location loc);
  void createEntry(Location loc);

  /// Static
  static unsigned edtCounter;
  static unsigned increaseEdtCounter() { return ++EdtCodegen::edtCounter; }
};

// ---------------------------- ARTS Codegen ---------------------------- ///
class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
              mlir::DataLayout &mlirDL);
  ~ArtsCodegen();

  /// Friend classes
  friend class DataBlockCodegen;
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
  DataBlockCodegen *getDatablock(arts::DataBlockOp dbOp);
  DataBlockCodegen *createDatablock(arts::DataBlockOp dbOp, Location loc);
  DataBlockCodegen *getOrCreateDatablock(arts::DataBlockOp dbOp, Location loc);

  /// Edts
  EdtCodegen *getEdt(Region *region);
  EdtCodegen *createEdt(SmallVector<Value> *opDeps = nullptr,
                        Region *region = nullptr, Value *epoch = nullptr,
                        Location *loc = nullptr, bool build = false);

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
  Value createArrayFromDeps(Value numElements, Value deps, Value initialSlot,
                            Location loc);

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
  Value castToPtr(Value source, Location loc);
  Value castToLLVMPtr(Value source, MemRefType MT, Location loc);

  /// Insertion point
  void setInsertionPoint(Operation *op) { builder.setInsertionPoint(op); }
  void setInsertionPointAfter(Operation *op) {
    builder.setInsertionPointAfter(op);
  }

  // ---------------------------- Types ---------------------------- ///
  /// Declarations for LLVM-IR types (simple, array, function and structure) are
  /// generated below. Their names are defined and used in ARTSKinds.def. Here
  /// we provide the declarations, the initializeTypes function will provide the
  /// values.
  ///
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
  /// Map an arts region to a EdtCodegen
  llvm::DenseMap<Region *, EdtCodegen *> edts;
  /// Maps an entry datablock pointer to its DataBlockCodegen object
  DenseMap<Value, DataBlockCodegen *> rewiredDbs;
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H