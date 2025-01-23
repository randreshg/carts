// ArtsCodegen.h
#ifndef LLVM_ARTS_CODEGEN_H
#define LLVM_ARTS_CODEGEN_H

#include "mlir/Conversion/LLVMCommon/MemRefBuilder.h"
#include "mlir/Dialect/Func/IR/FuncOps.h" // Correct include for FuncOp
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"

#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsTypes.h"
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/StringRef.h"
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
  DataBlockCodegen(ArtsCodegen &AC, arts::DataBlockOp edtDep, Location loc);
  /// Getters
  Value getDb() { return db; }
  Value getNumElements() { return numElements; }
  bool getIsArray() { return isArray; }

  /// Setters
  Value setDb(Value db) { this->db = db; }
  Value setNumElements(Value numElements) { this->numElements = numElements; }
  void setIsArray(bool isArray) { this->isArray = isArray; }

  /// Interface
  void create(arts::DataBlockOp edtDep, Location loc);

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  Value db = nullptr;
  Value numElements = nullptr;
  bool isArray = false;

  /// Utils
  Value getMode(StringRef mode);
};

// ---------------------------- EDTs ---------------------------- ///
class EdtCodegen {
public:
  EdtCodegen(ArtsCodegen &AC);
  EdtCodegen(ArtsCodegen &AC, ValueRange &opDeps, ValueRange &opParams,
             Region &region, Value &epoch, Location loc);
  /// Getters
  func::FuncOp getFunc() { return func; }
  Value getGuid() { return guid; }
  Value getSlot() { return slot; }
  Value getNode() { return node; }
  SmallVector<Value> &getParams() { return params; }
  SmallVector<Value> &getConsts() { return consts; }

  /// Setters
  void setFunc(func::FuncOp func) { this->func = func; }
  void setGuid(Value guid) { this->guid = guid; }
  void setSlot(Value slot) { this->slot = slot; }
  void setNode(Value node) { this->node = node; }
  void setParams(SmallVector<Value> params) { this->params = params; }
  void setConsts(SmallVector<Value> consts) { this->consts = consts; }
  void setDeps(SmallVector<Value> deps) { this->deps = deps; }

  /// Interface
  int insertParam(Value param) {
    params.push_back(param);
    return params.size() - 1;
  }
  void addParam(Value param) { params.push_back(param); }
  void addConst(Value constant) { consts.push_back(constant); }
  void addDep(Value dep) { deps.push_back(dep); }

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  func::FuncOp func = nullptr;
  Value guid = nullptr;
  Value slot = nullptr;
  Value node = nullptr;
  Value epoch = nullptr;
  Value paramC = nullptr;
  Value paramV = nullptr;
  Value depC = nullptr;
  SmallVector<Value> params;
  SmallVector<Value> consts;
  SmallVector<Value> deps;
  /// There are cases where we the size of a db is needed, this is stored as a
  /// parameter, so we need to keep track of it. - Create a map, the key is
  /// the datablock, the value is the index of the parameter
  DenseMap<DataBlockCodegen *, unsigned> dbSizeMap;

  /// Utils
  void processDepsAndParams(ValueRange &deps, ValueRange &params, Location loc);
  Value createGuid(Value node, Location loc);
  func::FuncOp createFn(Location loc);
  void createEntry(Region &region, Location loc);

  /// Static
  static unsigned edtCounter;
  static unsigned increaseEdtCounter() { return ++EdtCodegen::edtCounter; }
};

// ---------------------------- ARTS Codegen ---------------------------- ///
class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, OpBuilder &builder, llvm::DataLayout &llvmDL,
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
  DataBlockCodegen *getDatablock(arts::DataBlockOp edtDep);
  DataBlockCodegen *createDatablock(arts::DataBlockOp edtDep, Location loc);
  DataBlockCodegen *getOrCreateDatablock(arts::DataBlockOp edtDep, Location loc);

  /// Edts
  EdtCodegen *getEdt(Region &region);
  EdtCodegen *createEdt(ValueRange &params, ValueRange &deps, Region &region,
                        Value &epoch, Location loc);

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
  Value getNumDeps(SmallVector<Value> &deps, Location loc);
  Value createArrayFromDeps(Value numElements, Value deps,
                            Value initialSlot, Location loc);

  /// Helpers
  Value createFnPtr(func::FuncOp funcOp, Location loc);
  Value createIntConstant(unsigned value, Type type, Location loc);

  /// Casting
  Value castParameter(mlir::Type targetType, Value source, Location loc);
  Value castDependency(mlir::Type targetType, Value source, Location loc);
  Value castToIndex(Value source, Location loc);
  Value castToFloat(mlir::Type targetType, Value source, Location loc);
  Value castToInt(mlir::Type targetType, Value source, Location loc);
  Value castToPtr(Value source, Location loc);

  /// Insertion point
  void setInsertionPoint(Operation *op) { builder.setInsertionPoint(op); }
  void setInsertionPointAfter(Operation *op) {
    builder.setInsertionPointAfter(op);
  }

private:
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
  ///}
  void initializeTypes();

  // -------------------------- Other Attributes-------------------------- ///
  /// The MLIR module and builder
  ModuleOp &module;
  OpBuilder &builder;
  llvm::DataLayout &llvmDL;
  mlir::DataLayout &mlirDL;
  /// Function counter
  unsigned edtCounter = 0;
  /// Map a arts::DataBlockOp to a DataBlockCodegen
  llvm::DenseMap<Value, DataBlockCodegen *> datablocks;
  /// Map an arts region to a EdtCodegen
  llvm::DenseMap<Region *, EdtCodegen *> edts;
  // Add more types as necessary
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H