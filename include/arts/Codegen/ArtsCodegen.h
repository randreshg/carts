// ArtsCodegen.h
#ifndef LLVM_ARTS_CODEGEN_H
#define LLVM_ARTS_CODEGEN_H

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

class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, OpBuilder &builder, llvm::DataLayout &dataLayout);
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

  /// Create Edt
  mlir::Value createEdt(mlir::Value edtGuid, mlir::Value edtPtr,
                        mlir::Value edtSlotPtr, Location loc);
  func::FuncOp createEdtFunction(Location loc);
  func::CallOp insertEdtCall(arts::EdtOp edtOp, func::FuncOp edtFunc,
                             Location loc);
  func::CallOp createEdtCall(func::FuncOp edtFunc, mlir::Value edtGuid,
                             SmallVector<Value> &edtParameters,
                             unsigned edtDepNum, Location loc);
  mlir::Value createEdtGuid(uint64_t edtNode, Location loc);
  mlir::Value insertEdtEntry(func::FuncOp edtFunc,
                             SmallVector<Value> &opParameters,
                             SmallVector<Value> &opDependencies, Location loc);

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
  FunctionType VarName = nullptr;
#define ARTS_STRUCT_TYPE(VarName, StructName, ...)                             \
  LLVMStructType VarName = nullptr;                                            \
  LLVMPointerType VarName##Ptr = nullptr;
#include "ARTSKinds.def"
  ///}
private:
  /// Internal helper functions
  void initializeTypes();

  /// The MLIR module and builder
  ModuleOp &module;
  OpBuilder &builder;
  llvm::DataLayout &dataLayout;
  /// Function counter
  unsigned edtCounter = 0;
  // Add more types as necessary
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H