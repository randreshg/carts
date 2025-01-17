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
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <sys/types.h>

namespace mlir {
namespace arts {
using namespace mlir::LLVM;
using namespace types;

struct DataBlockCodegen {
  DataBlockCodegen(Value db, Value numElements)
      : db(db), numElements(numElements) {}

  Value getDb() { return db; }
  Value getNumElements() { return numElements; }

private:
  Value db = nullptr;
  Value numElements = nullptr;
};

class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, OpBuilder &builder, llvm::DataLayout &llvmDL,
              mlir::DataLayout &mlirDL);
  ~ArtsCodegen();

  /// Initialize types and runtime function declarations
  void initialize();
  /// Finalize code generation, if necessary
  void finalize();
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

  /// Edt
  Value createEdtGuid(Value node, Location loc);
  func::FuncOp createEdtFunction(Location loc);
  Value createEdt(func::FuncOp edtFunc,
                  SmallVector<mlir::Value> parametersValues, Value depC,
                  Value node, Location loc);
  Value createEdtWithEpoch(func::FuncOp edtFunc,
                           SmallVector<Value> parametersValues, Value depC,
                           Value node, Value epoch, Location loc);
  func::CallOp createEdtCallWithGuid(func::FuncOp edtFunc, Value guid,
                                     SmallVector<Value> parametersValues,
                                     Value depC, Value node, Location loc);
  Value insertEdtEntry(func::FuncOp edtFunc, SmallVector<Value> &params,
                       SmallVector<Value> &deps, Location loc);
  void outlineRegion(func::FuncOp func, Region &region,
                     SmallVector<Value> &params, SmallVector<Value> &deps);

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
  Value createParamV(SmallVector<Value> &parameters, Location loc);
  Value createFunctionPtr(func::FuncOp funcOp, Location loc);
  Value createIntConstant(unsigned value, Type type, Location loc);

  /// Casting
  Value castParameter(mlir::Type targetType, Value value, Location loc);
  Value castToIndex(Value value, Location loc);
  Value castToFloat(mlir::Type targetType, Value value, Location loc);
  Value castToInt(mlir::Type targetType, Value value, Location loc);
  Value castToPtr(Value value, Location loc);

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
  ///}
private:
  /// Internal helper functions
  void initializeTypes();

  unsigned increaseEdtCounter() { return ++edtCounter; }

  /// The MLIR module and builder
  ModuleOp &module;
  OpBuilder &builder;
  llvm::DataLayout &llvmDL;
  mlir::DataLayout &mlirDL;
  /// Function counter
  unsigned edtCounter = 0;
  /// Map a arts::MakeDepOp to a DataBlockCodegen
  llvm::DenseMap<Value, DataBlockCodegen *> datablocks;
  // Add more types as necessary
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H