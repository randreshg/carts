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
#include <sys/types.h>

namespace mlir {
namespace arts {
using namespace mlir::LLVM;
using namespace types;

struct DataBlockCodegen {
  DataBlockCodegen(mlir::Value memref) : memref(memref) {}
  mlir::Value memref = nullptr;
  llvm::SmallVector<mlir::Value, 4> sizes;
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
  func::CallOp createRuntimeCall(RuntimeFunction FnID,
                                 ArrayRef<mlir::Value> args, Location loc);

  /// Create Datablock
  DataBlockCodegen *getOrCreateDatablock(mlir::Value memref);
  mlir::Value createDatablock(arts::MakeDepOp edtDep, Location loc);

  /// Create Edt
  mlir::Value createEdt(mlir::Value edtGuid, mlir::Value edtPtr,
                        mlir::Value edtSlotPtr, Location loc);
  func::FuncOp createEdtFunction(Location loc);
  func::CallOp createEdtCallWithGuid(func::FuncOp edtFunc,
                                     SmallVector<mlir::Value> parametersValues,
                                     unsigned numDeps, unsigned route,
                                     Location loc);
  func::CallOp createEdtCallWithGuid(arts::EdtOp edtOp, func::FuncOp edtFunc,
                                     unsigned route, Location loc);
  func::CallOp createEdtCallWithGuid(arts::ParallelOp parOp,
                                     func::FuncOp edtFunc, unsigned route,
                                     Location loc);
  func::CallOp createEdtCallWithEpoch(arts::EdtOp edtOp, func::FuncOp edtFunc,
                                      unsigned route, Value epoch,
                                      Location loc);
  mlir::Value createEdtGuid(uint64_t node, Location loc);
  mlir::Value insertEdtEntry(func::FuncOp edtFunc,
                             SmallVector<Value> &opParameters,
                             SmallVector<Value> &opDependencies, Location loc);
  /// Helpers
  Value createFunctionPointer(func::FuncOp funcOp, Location loc);
  Value createParamV(SmallVector<Value> &parameters, Location loc);
  Value createIntConstant(unsigned value, Type type, Location loc);
  Value castToInt64(Value value, Location loc);

  unsigned increaseEdtCounter() { return ++edtCounter; }

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

  /// The MLIR module and builder
  ModuleOp &module;
  OpBuilder &builder;
  llvm::DataLayout &llvmDL;
  mlir::DataLayout &mlirDL;
  /// Function counter
  unsigned edtCounter = 0;
  /// Map of known datablocks
  llvm::DenseMap<mlir::Value, DataBlockCodegen *> datablocks;
  // Add more types as necessary
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H