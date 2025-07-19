///==========================================================================
/// File: carts-opt.cpp
///==========================================================================

#include "mlir/Conversion/Passes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/Extensions/InlinerExtension.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllExtensions.h"
#include "mlir/InitAllPasses.h"
#include "mlir/InitAllTranslations.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/Passes.h"
#include "polygeist/Dialect.h"
#include "polygeist/Passes/Passes.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

using namespace mlir;

class MemRefInsider
    : public mlir::MemRefElementTypeInterface::FallbackModel<MemRefInsider> {};

template <typename T>
struct PtrElementModel
    : public mlir::LLVM::PointerElementTypeInterface::ExternalModel<
          PtrElementModel<T>, T> {};

int main(int argc, char **argv) {
  /// Create a DialectRegistry and register all required dialects.
  mlir::DialectRegistry registry;

  registry.insert<mlir::affine::AffineDialect>();
  registry.insert<mlir::arith::ArithDialect>();
  registry.insert<mlir::func::FuncDialect>();
  registry.insert<mlir::LLVM::LLVMDialect>();
  registry.insert<mlir::omp::OpenMPDialect>();
  registry.insert<mlir::math::MathDialect>();
  registry.insert<mlir::memref::MemRefDialect>();
  registry.insert<mlir::scf::SCFDialect>();
  registry.insert<mlir::cf::ControlFlowDialect>();
  registry.insert<mlir::polygeist::PolygeistDialect>();
  registry.insert<mlir::arts::ArtsDialect>();
  registry.insert<DLTIDialect>();

  /// Register all passes and translations.
  mlir::registerArtsPasses();
  mlir::registerpolygeistPasses();
  mlir::func::registerInlinerExtension(registry);
  mlir::registerCSEPass();
  mlir::registerConvertAffineToStandardPass();
  mlir::registerSCCPPass();
  mlir::registerInlinerPass();
  mlir::registerCanonicalizerPass();
  mlir::registerSymbolDCEPass();
  mlir::registerLoopInvariantCodeMotionPass();
  mlir::affine::registerAffinePasses();

  /// Register interfaces.
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMFunctionType::attachInterface<MemRefInsider>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMArrayType::attachInterface<MemRefInsider>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMPointerType::attachInterface<MemRefInsider>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMStructType::attachInterface<MemRefInsider>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, memref::MemRefDialect *dialect) {
    MemRefType::attachInterface<PtrElementModel<MemRefType>>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMStructType::attachInterface<
        PtrElementModel<LLVM::LLVMStructType>>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMPointerType::attachInterface<
        PtrElementModel<LLVM::LLVMPointerType>>(*ctx);
  });
  registry.addExtension(+[](MLIRContext *ctx, LLVM::LLVMDialect *dialect) {
    LLVM::LLVMArrayType::attachInterface<PtrElementModel<LLVM::LLVMArrayType>>(
        *ctx);
  });

  /// Run the MlirOptMain driver with the registered dialects and passes.
  return mlir::failed(mlir::MlirOptMain(argc, argv, "CARTS Driver", registry));
}