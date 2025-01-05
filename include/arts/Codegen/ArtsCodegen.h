// ArtsCodegen.h
#ifndef LLVM_ARTS_CODEGEN_H
#define LLVM_ARTS_CODEGEN_H

#include "mlir/Dialect/Func/IR/FuncOps.h" // Correct include for FuncOp
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"

#include "arts/Utils/ArtsTypes.h"
#include "mlir/Analysis/DataLayoutAnalysis.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"

namespace mlir {
namespace arts {
using namespace mlir::LLVM;
using namespace types;

class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, OpBuilder &builder);
  ~ArtsCodegen();

  /// Initialize types and runtime function declarations
  void initialize();

  /// Finalize code generation, if necessary
  void finalize();

  /// Get or create a runtime function
  func::FuncOp getOrCreateRuntimeFunction(RuntimeFunction FnID);

  /// Generate a call to a runtime function
  mlir::Value createRuntimeCall(RuntimeFunction FnID,
                                ArrayRef<mlir::Value> args, Location loc);

  // ---------------------------- Types ---------------------------- ///
  /// Declarations for LLVM-IR types (simple, array, function and structure) are
  /// generated below. Their names are defined and used in ARTSKinds.def. Here
  /// we provide the declarations, the initializeTypes function will provide the
  /// values.
  ///
  ///{
#define ARTS_TYPE(VarName, InitValue) mlir::Type VarName = nullptr;
#define ARTS_ARRAY_TYPE(VarName, ElemTy, ArraySize)                            \
  LLVMArrayType VarName##ArrayTy = nullptr;                                    \
  LLVMPointerType VarName##PtrTy = nullptr;
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  LLVMFunctionType VarName = nullptr;                                          \
  LLVMPointerType VarName##Ptr = nullptr;
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
  DataLayout dataLayout;
  // Add more types as necessary
};

} // namespace arts
} // namespace mlir
#endif // LLVM_ARTS_CODEGEN_H