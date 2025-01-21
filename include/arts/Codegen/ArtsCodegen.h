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
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <sys/types.h>

namespace mlir {
namespace arts {
using namespace mlir::LLVM;
using namespace types;
class ArtsCodegen;
class DataBlockCodegen {
public:
  DataBlockCodegen(ArtsCodegen &AC);
  DataBlockCodegen(ArtsCodegen &AC, arts::MakeDepOp edtDep, Location loc);
  /// Getters
  Value getDb() { return db; }
  Value getNumElements() { return numElements; }
  bool getIsArray() { return isArray; }

  /// Setters
  Value setDb(Value db) { this->db = db; }
  Value setNumElements(Value numElements) { this->numElements = numElements; }
  void setIsArray(bool isArray) { this->isArray = isArray; }

  /// Interface
  void create(arts::MakeDepOp edtDep, Location loc);

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  Value db = nullptr;
  Value numElements = nullptr;
  bool isArray = false;

  /// Utils
  Value getMode(StringRef mode);
};

class EdtCodegen {
public:
  EdtCodegen(ArtsCodegen &AC);
  /// Getters
  func::FuncOp getFunc() { return func; }
  Value getGuid() { return guid; }
  Value getSlot() { return slot; }
  Value getNode() { return node; }
  SetVector<Value> getParams() { return params; }
  SetVector<Value> getConstants() { return constants; }

  /// Setters
  void setFunc(func::FuncOp func) { this->func = func; }
  void setGuid(Value guid) { this->guid = guid; }
  void setSlot(Value slot) { this->slot = slot; }
  void setNode(Value node) { this->node = node; }
  void setParams(SetVector<Value> params) { this->params = params; }
  void setConstants(SetVector<Value> constants) { this->constants = constants; }
  void setDeps(SetVector<Value> deps) { this->deps = deps; }

  /// Interface
  void addParam(Value param) { params.insert(param); }
  void addConstant(Value constant) { constants.insert(constant); }
  void addDep(Value dep) { deps.insert(dep); }

private:
  ArtsCodegen &AC;
  OpBuilder &builder;
  func::FuncOp func = nullptr;
  Value guid = nullptr;
  Value slot = nullptr;
  Value node = nullptr;
  SetVector<Value> params;
  SetVector<Value> constants;
  SetVector<Value> deps;

  /// Utils
  Value create(func::FuncOp edtFunc, SmallVector<mlir::Value> params,
               Value depC, Value node, Location loc);
  Value createWithEpoch(func::FuncOp edtFunc, SmallVector<Value> params,
                        Value depC, Value node, Value epoch, Location loc);

  Value createGuid(Value node, Location loc);
  func::FuncOp createFn(Location loc);
  func::CallOp createCallWithGuid(func::FuncOp edtFunc, Value guid,
                                  SmallVector<Value> params, Value depC,
                                  Value node, Location loc);
  void insertFnEntry(func::FuncOp func, Region &region,
                     SmallVector<Value> &params, SmallVector<Value> &constants,
                     SmallVector<Value> &deps,
                     DenseMap<Value, Value> &rewireMap);
  Value createParamV(SmallVector<Value> &parameters, Location loc);

  /// Static
  static unsigned edtCounter;
  static unsigned increaseEdtCounter() { return ++EdtCodegen::edtCounter; }
};

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
  Value getDatablockMode(llvm::StringRef mode);
  DataBlockCodegen *getDatablock(arts::MakeDepOp edtDep);
  DataBlockCodegen *createDatablock(arts::MakeDepOp edtDep, Location loc);
  DataBlockCodegen *getOrCreateDatablock(arts::MakeDepOp edtDep, Location loc);

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
  /// Map a arts::MakeDepOp to a DataBlockCodegen
  llvm::DenseMap<Value, DataBlockCodegen *> datablocks;
  /// Map a arts::EdtOp  to a EdtCodegen
  llvm::DenseMap<Value, EdtCodegen *> edts;
  // Add more types as necessary
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H