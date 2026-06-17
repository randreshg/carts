#ifndef CARTS_TOOLS_COMPILE_COMPILEDRIVER_H
#define CARTS_TOOLS_COMPILE_COMPILEDRIVER_H

#include "carts/utils/PassInstrumentation.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "llvm/ADT/StringRef.h"
#include <memory>

namespace llvm {
class Module;
} // namespace llvm

namespace mlir::carts::compile_driver {

void configureArtsDebugChannels(llvm::StringRef channels);

LogicalResult configurePassManager(PassManager &pm,
                                   PassTimingData *timingData = nullptr);

void registerDialects(DialectRegistry &registry);
void initializeContext(MLIRContext &context);

bool hasResidualOpenMP(ModuleOp module);
std::unique_ptr<Pass> createRealizeArtsFunctionPointersPass();
std::unique_ptr<Pass> createResidualHostOpenMPMemrefCleanupPass();

void foldResidualHostOpenMPMemrefPointerCasts(ModuleOp module);
void ensureRuntimeConfigDataVisibleForValidation(ModuleOp module);
void bypassArtsRuntimeForHostOpenMP(llvm::Module &module);

} // namespace mlir::carts::compile_driver

#endif // CARTS_TOOLS_COMPILE_COMPILEDRIVER_H
