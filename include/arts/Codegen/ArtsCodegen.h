///==========================================================================
/// File: ArtsCodegen.h
///==========================================================================
#ifndef CARTS_CODEGEN_ARTSCODEGEN_H
#define CARTS_CODEGEN_ARTSCODEGEN_H

#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsTypes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/DataTypes.h"

namespace mlir {
namespace arts {
using namespace types;

/// Forward declarations
class DbAllocCodegen;
class DbDepCodegen;
class EdtCodegen;

class ArtsCodegen {
public:
  ArtsCodegen(ModuleOp &module, llvm::DataLayout &llvmDL,
              mlir::DataLayout &mlirDL, bool debug = false);
  ~ArtsCodegen();

  /// Getters
  OpBuilder &getBuilder() { return builder; }
  ModuleOp &getModule() { return module; }
  llvm::DataLayout &getLLVMDataLayout() { return llvmDL; }
  mlir::DataLayout &getMLIRDataLayout() { return mlirDL; }
  bool isDebug() const { return debug; }
  Location getUnknownLoc() { return UnknownLoc::get(module.getContext()); }

  /// Runtime function management
  func::FuncOp getOrCreateRuntimeFunction(RuntimeFunction FnID);
  func::CallOp createRuntimeCall(RuntimeFunction FnID, ArrayRef<Value> args,
                                 Location loc);

  /// DB management
  DbAllocCodegen *getDbAlloc(Value op);
  DbAllocCodegen *getDbAlloc(DbAllocOp dbOp);
  DbDepCodegen *getDbDep(Value op);
  DbDepCodegen *getDbDep(DbDepOp dbOp);
  DbAllocCodegen *createDbAlloc(DbAllocOp dbOp, Location loc);
  DbDepCodegen *createDbDep(DbDepOp dbOp, Operation *parentOp = nullptr);
  DbAllocCodegen *getOrCreateDbAlloc(DbAllocOp dbOp, Location loc);
  DbDepCodegen *getOrCreateDbDep(DbDepOp dbOp, Operation *parentOp = nullptr);

  void addDbDep(Value dbGuid, Value edtGuid, Value edtSlot, Location loc);
  void incrementDbLatchCount(Value dbGuid, Location loc);
  void decrementDbLatchCount(Value dbGuid, Location loc);

  /// EDT management
  EdtCodegen *getEdt(Region *region);
  EdtCodegen *createEdt(SmallVector<Value> *opDeps, Region *region,
                        Value *epoch, Location *loc, bool build = true);

  /// Epoch management
  Value createEpoch(Value finishEdtGuid, Value finishEdtSlot, Location loc);

  /// Utility functions
  Value getGuidFromEdtDep(Value dep, Location loc);
  Value getPtrFromEdtDep(Value dep, Location loc);
  Value getCurrentEpochGuid(Location loc);
  Value getCurrentEdtGuid(Location loc);
  Value getTotalWorkers(Location loc);
  Value getTotalNodes(Location loc);
  Value getCurrentWorker(Location loc);
  Value getCurrentNode(Location loc);
  func::CallOp signalEdt(Value edtGuid, Value edtSlot, Value dbGuid,
                         Location loc);
  void waitOnHandle(Value epochGuid, Location loc);

  /// Function creation
  func::FuncOp insertInitPerWorker(Location loc);
  func::FuncOp insertInitPerNode(Location loc, func::FuncOp callback);
  func::FuncOp insertArtsMainFn(Location loc, func::FuncOp callback);
  func::FuncOp insertMainFn(Location loc);
  void initRT(Location loc);

  /// Helper functions
  Value createFnPtr(func::FuncOp funcOp, Location loc);
  Value createIndexConstant(int value, Location loc);
  Value createIntConstant(int value, Type type, Location loc);
  Value createPtr(Value source, Location loc);
  Value castParameter(Type targetType, Value source, Location loc);
  Value castPtr(Type targetType, Value source, Location loc);
  Value castToIndex(Value source, Location loc);
  Value castToFloat(Type targetType, Value source, Location loc);
  Value castToInt(Type targetType, Value source, Location loc);
  Value castToVoidPtr(Value source, Location loc);
  Value castToLLVMPtr(Value source, Location loc);

  /// Debug
  void collectGlobalLLVMStrings();
  Value getOrCreateGlobalLLVMString(Location loc, StringRef value);
  void createPrintfCall(Location loc, StringRef format, ValueRange args);

  /// Insertion point management
  void setInsertionPoint(Operation *op);
  void setInsertionPointAfter(Operation *op);
  void setInsertionPoint(ModuleOp &module);

  /// Replacement map
  void addReplacement(Value original, Value newValue, Region *region = nullptr);
  void applyReplacements(Region *region);
  void applyReplacements();

/// Types
///{
#define ARTS_TYPE(VarName, InitValue) Type VarName = nullptr;
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  FunctionType VarName = nullptr;                                              \
  MemRefType VarName##Ptr = nullptr;
#define ARTS_STRUCT_TYPE(VarName, StructName, ...)                             \
  LLVM::LLVMStructType VarName = nullptr;                                      \
  MemRefType VarName##Ptr = nullptr;
#include "arts/Codegen/ARTSKinds.def"
  LLVM::LLVMPointerType llvmPtr = nullptr;
  ///}

private:
  ModuleOp &module;
  OpBuilder builder;
  llvm::DataLayout &llvmDL;
  mlir::DataLayout &mlirDL;

  /// Other Attributes
  // unsigned edtCounter = 0;
  llvm::DenseMap<Value, DbAllocCodegen *> dbAllocs;
  llvm::DenseMap<Value, DbDepCodegen *> dbDeps;
  llvm::DenseMap<Region *, EdtCodegen *> edts;
  llvm::DenseMap<RuntimeFunction, func::FuncOp> runtimeFunctionCache;
  llvm::StringMap<LLVM::GlobalOp> llvmStringGlobals;
  llvm::DenseMap<Region *, llvm::DenseMap<Value, Value>> replacementMap;
  bool debug = false;

  /// Helper functions
  void initializeTypes();

  /// Aux
  Region emptyRegion;
};

} // namespace arts
} // namespace mlir

#endif // CARTS_CODEGEN_ARTSCODEGEN_H