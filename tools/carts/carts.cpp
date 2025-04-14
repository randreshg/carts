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

static cl::opt<bool> AffineOpt("affine-opt",
                               cl::desc("Apply Affine Optimizations"),
                               cl::init(false));

static cl::opt<bool> EmitLLVM("emit-llvm", cl::desc("Emit LLVM IR output"),
                              cl::init(false));

static cl::opt<bool> IdentifyDatablocks(
    "identify-dbs", cl::desc("Number of optimization iterations"), cl::init(1));

static cl::opt<unsigned>
    OptIterations("iterations", cl::desc("Number of optimization iterations"),
                  cl::init(1));

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
  context.disableMultithreading();
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
void setupPassManager(MLIRContext &context, PassManager &pm) {
  mlir::OpPassManager &optPM = pm.nest<mlir::func::FuncOp>();

  /// Basic inlining and affine lowering.
  pm.addPass(createInlinerPass());
  pm.addPass(createLowerAffinePass());
  pm.addPass(createCSEPass());
  pm.addPass(createCanonicalizerPass());

  /// Convert OpenMP Dialect to ARTS Dialect.
  pm.addPass(arts::createConvertOpenMPtoARTSPass());

  pm.addPass(arts::createEdtPass());

  pm.addPass(arts::createHoistInvariantOpsPass());
  pm.addPass(arts::createCreateDatablocksPass(IdentifyDatablocks));
  pm.addPass(createCSEPass());
  pm.addPass(createCanonicalizerPass());

  pm.addPass(arts::createDatablockPass());

  pm.addPass(arts::createCreateEventsPass());

  pm.addPass(arts::createCreateEpochsPass());
  pm.addPass(createCSEPass());
  pm.addPass(createCanonicalizerPass());

  /// Convert ARTS constructs to LLVM Dialect.
  pm.addPass(arts::createConvertArtsToLLVMPass());

  /// Affine optimizations.
  if (AffineOpt) {
    // optPM.addPass(polygeist::createPolygeistMem2RegPass());
    optPM.addPass(createCSEPass());
    optPM.addPass(createCanonicalizerPass());
    optPM.addPass(polygeist::createRaiseSCFToAffinePass());
    optPM.addPass(createCanonicalizerPass());
    optPM.addPass(polygeist::replaceAffineCFGPass());
    optPM.addPass(affine::createAffineExpandIndexOpsPass());
    optPM.addPass(affine::createAffineScalarReplacementPass());
    optPM.addPass(polygeist::replaceAffineCFGPass());
    optPM.addPass(createLoopInvariantCodeMotionPass());
    optPM.addPass(createCSEPass());
    optPM.addPass(createCanonicalizerPass());
    optPM.addPass(createLowerAffinePass());
  }

  /// Convert the module to LLVM IR.
  pm.addPass(createCSEPass());
  pm.addPass(createCanonicalizerPass());
  pm.addPass(polygeist::createConvertPolygeistToLLVMPass());
  pm.addPass(createCanonicalizerPass());
  pm.addPass(createCSEPass());
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
  for (unsigned i = 0; i < OptIterations; ++i) {
    PassManager pm(&context);
    (void)applyPassManagerCLOptions(pm);
    setupPassManager(context, pm);
    if (failed(pm.run(*module))) {
      llvm::errs() << "Error: Failed to apply passes on iteration " << i
                   << "\n";
      return 1;
    }
  }

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