//==========================================================================
/// File: carts.cpp
//==========================================================================
#include "mlir/Conversion/Passes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/Passes.h"
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
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/Passes.h"
#include "polygeist/Dialect.h"
#include "polygeist/Passes/Passes.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace mlir;

//===----------------------------------------------------------------------===//
// Interface Attachments
//===----------------------------------------------------------------------===//
/// Use the original models for attaching type interfaces.
class MemRefInsider
    : public mlir::MemRefElementTypeInterface::FallbackModel<MemRefInsider> {};

template <typename T>
struct PtrElementModel
    : public mlir::LLVM::PointerElementTypeInterface::ExternalModel<
          PtrElementModel<T>, T> {};

//===----------------------------------------------------------------------===//
// Command Line Options
//===----------------------------------------------------------------------===//
static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"),
                                           cl::value_desc("filename"),
                                           cl::init("-"));

static cl::opt<bool> ArtsOpt("arts-opt", cl::desc("Apply ARTS Optimizations"),
                             cl::init(false));

static cl::opt<bool> Opt("O3", cl::desc("Apply Optimizations"),
                         cl::init(false));

static cl::opt<bool> EmitLLVM("emit-llvm", cl::desc("Emit LLVM IR output"),
                              cl::init(false));

static cl::opt<bool> IdentifyDbs("identify-dbs",
                                 cl::desc("Number of optimization iterations"),
                                 cl::init(1));

static cl::opt<unsigned>
    OptIterations("iterations", cl::desc("Number of optimization iterations"),
                  cl::init(1));

static cl::opt<bool> Debug("g", cl::desc("Enable debug mode"), cl::init(false));

//===----------------------------------------------------------------------===//
// Helper Functions for Initialization and Pass Setup
//===----------------------------------------------------------------------===//
/// Register standard MLIR dialects, passes, and translations.
void registerDialects(DialectRegistry &registry) {
  registry.insert<mlir::polygeist::PolygeistDialect, mlir::arts::ArtsDialect>();
  mlir::registerAllPasses();
  mlir::registerAllTranslations();
  mlir::registerpolygeistPasses();
  mlir::func::registerInlinerExtension(registry);
  mlir::registerAllDialects(registry);
  mlir::registerAllExtensions(registry);
  mlir::registerAllFromLLVMIRTranslations(registry);
  mlir::registerBuiltinDialectTranslation(registry);
  mlir::registerLLVMDialectTranslation(registry);
}

/// Initialize the MLIR context by loading necessary dialects and attaching
/// type interfaces.
void initializeContext(MLIRContext &context) {
  context.disableMultithreading(true);
  context.getOrLoadDialect<affine::AffineDialect>();
  context.getOrLoadDialect<func::FuncDialect>();
  context.getOrLoadDialect<DLTIDialect>();
  context.getOrLoadDialect<mlir::scf::SCFDialect>();
  context.getOrLoadDialect<mlir::async::AsyncDialect>();
  context.getOrLoadDialect<mlir::LLVM::LLVMDialect>();
  context.getOrLoadDialect<mlir::NVVM::NVVMDialect>();
  context.getOrLoadDialect<mlir::ROCDL::ROCDLDialect>();
  context.getOrLoadDialect<mlir::gpu::GPUDialect>();
  context.getOrLoadDialect<mlir::omp::OpenMPDialect>();
  context.getOrLoadDialect<mlir::math::MathDialect>();
  context.getOrLoadDialect<mlir::memref::MemRefDialect>();
  context.getOrLoadDialect<mlir::linalg::LinalgDialect>();
  context.getOrLoadDialect<mlir::polygeist::PolygeistDialect>();
  context.getOrLoadDialect<mlir::cf::ControlFlowDialect>();

  LLVM::LLVMFunctionType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMPointerType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMArrayType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMStructType::attachInterface<MemRefInsider>(context);
  MemRefType::attachInterface<PtrElementModel<MemRefType>>(context);
  IndexType::attachInterface<PtrElementModel<IndexType>>(context);
  LLVM::LLVMStructType::attachInterface<PtrElementModel<LLVM::LLVMStructType>>(
      context);
  LLVM::LLVMPointerType::attachInterface<
      PtrElementModel<LLVM::LLVMPointerType>>(context);
  LLVM::LLVMArrayType::attachInterface<PtrElementModel<LLVM::LLVMArrayType>>(
      context);
}

/// Configure the pass manager with the optimization passes.
void setupPassManager(mlir::ModuleOp module, MLIRContext &context) {

  /// Initial cleanup and simplification
  {
    PassManager pm(&context);
    mlir::OpPassManager &optPM = pm.nest<mlir::func::FuncOp>();
    optPM.addPass(createLowerAffinePass());
    optPM.addPass(polygeist::createCanonicalizeForPass());

    if (Opt) {
      optPM.addPass(createCSEPass());
      optPM.addPass(polygeist::createPolygeistCanonicalizePass());
      optPM.addPass(createMem2Reg());
      optPM.addPass(polygeist::createPolygeistCanonicalizePass());
      optPM.addPass(polygeist::replaceAffineCFGPass());
      optPM.addPass(affine::createAffineScalarReplacementPass());
      optPM.addPass(polygeist::replaceAffineCFGPass());

      // optPM.addPass(polygeist::createRaiseSCFToAffinePass());
      // optPM.addPass(polygeist::createPolygeistCanonicalizePass());
      // optPM.addPass(polygeist::replaceAffineCFGPass());
      // optPM.addPass(affine::createAffineExpandIndexOpsPass());
      // optPM.addPass(affine::createAffineScalarReplacementPass());
      // optPM.addPass(polygeist::replaceAffineCFGPass());
      optPM.addPass(createLoopInvariantCodeMotionPass());
      optPM.addPass(createCSEPass());
      optPM.addPass(polygeist::createPolygeistCanonicalizePass());
      optPM.addPass(createLowerAffinePass());
    }

    optPM.addPass(createCSEPass());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());

    if (mlir::failed(pm.run(module))) {
      llvm::errs() << "Error simplyfing the IR";
      module->dump();
      return;
    }
  }

  /// ARTS dialect conversion and optimization
  PassManager pm(&context);
  pm.addPass(arts::createArtsInlinerPass());
  pm.addPass(arts::createConvertOpenMPtoARTSPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(arts::createEdtPass());
  pm.addPass(arts::createEdtInvariantCodeMotionPass());
  pm.addPass(createCanonicalizerPass());

  /// Db pass to identify and optimize data dependencies
  pm.addPass(arts::createCreateDbsPass(IdentifyDbs));
  pm.addPass(arts::createDbPass());
  pm.addPass(createCSEPass());
  pm.addPass(createMem2Reg());

  /// Create epochs
  pm.addPass(arts::createEdtPointerRematerializationPass());
  pm.addPass(arts::createCreateEpochsPass());

  /// Convert ARTS to LLVM
  pm.addPass(arts::createPreprocessDbsPass());
  pm.addPass(arts::createConvertArtsToLLVMPass(Debug));
  if (mlir::failed(pm.run(module))) {
    llvm::errs() << "Error when running ARTS Passes";
    module->dump();
    return;
  }

  /// Optimizations (if enabled)
  if (Opt) {
    PassManager pm2(&context);
    mlir::OpPassManager &optPM = pm2.nest<mlir::func::FuncOp>();
    optPM.addPass(createCSEPass());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());
    optPM.addPass(createMem2Reg());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());
    optPM.addPass(createControlFlowSinkPass());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());
    optPM.addPass(createLoopInvariantCodeMotionPass());
    optPM.addPass(createCSEPass());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());

    if (mlir::failed(pm2.run(module))) {
      llvm::errs() << "Error when running optimizations";
      module->dump();
      return;
    }
  }

  /// LLVM IR conversion (if enabled)
  if (EmitLLVM) {
    PassManager pm3(&context);
    pm3.addPass(createCSEPass());
    pm3.addPass(polygeist::createPolygeistCanonicalizePass());
    pm3.addPass(polygeist::createConvertPolygeistToLLVMPass());
    pm3.addPass(polygeist::createPolygeistCanonicalizePass());
    pm3.addPass(createCSEPass());
    if (mlir::failed(pm3.run(module))) {
      llvm::errs() << "Error when emitting LLVM IR";
      module->dump();
      return;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main Function
//===----------------------------------------------------------------------===//
int main(int argc, char **argv) {
  InitLLVM y(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "MLIR Optimization Driver\n");

  /// Set up the dialect registry and MLIR context.
  DialectRegistry registry;
  registerDialects(registry);
  MLIRContext context(registry);
  initializeContext(context);

  /// Open the input file.
  auto file = openInputFile(InputFilename);
  if (!file) {
    llvm::errs() << "Error: Could not open input file: " << InputFilename
                 << "\n";
    return 1;
  }

  /// Parse the input module.
  auto module = parseSourceString<ModuleOp>(file->getBuffer(), &context);
  if (!module) {
    llvm::errs() << "Error: Could not parse input file\n";
    return 1;
  }

  /// Iteratively run the pass pipeline to drive IR to a fixpoint.
  for (unsigned i = 0; i < OptIterations; ++i)
    setupPassManager(module.get(), context);

  /// Translate the optimized module to LLVM IR and write output.
  if (EmitLLVM) {
    llvm::LLVMContext llvmContext;
    auto llvmModule = translateModuleToLLVMIR(module.get(), llvmContext);
    if (!llvmModule) {
      module->dump();
      llvm::errs() << "Failed to emit LLVM IR\n";
      return -1;
    }
    std::string llvmIR;
    llvm::raw_string_ostream llvmStream(llvmIR);
    llvmModule->print(llvmStream, nullptr);

    auto output = openOutputFile(OutputFilename);
    if (!output) {
      llvm::errs() << "Error: Could not open output file: " << OutputFilename
                   << "\n";
      return 1;
    }
    output->os() << llvmStream.str();
    output->keep();
  } else {
    /// Otherwise, print the final MLIR module.
    auto output = openOutputFile(OutputFilename);
    if (!output) {
      llvm::errs() << "Error: Could not open output file: " << OutputFilename
                   << "\n";
      return 1;
    }
    module->print(output->os());
    output->keep();
  }

  return 0;
}