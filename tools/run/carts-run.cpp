///==========================================================================///
/// File: carts-run.cpp
/// Main entry point for the CARTS runtime execution tool.
///==========================================================================///

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
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/Passes.h"
#include "polygeist/Dialect.h"
#include "polygeist/Passes/Passes.h"

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsDebug.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace mlir;

ARTS_DEBUG_SETUP(carts_run)

///===----------------------------------------------------------------------===///
// Interface Attachments
///===----------------------------------------------------------------------===///
/// Use the original models for attaching type interfaces.
class MemRefInsider
    : public MemRefElementTypeInterface::FallbackModel<MemRefInsider> {};

template <typename T>
struct PtrElementModel
    : public LLVM::PointerElementTypeInterface::ExternalModel<
          PtrElementModel<T>, T> {};

///===----------------------------------------------------------------------===///
// Command Line Options
///===----------------------------------------------------------------------===///
static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"),
                                           cl::value_desc("filename"),
                                           cl::init("-"));

static cl::opt<bool> Opt("O3", cl::desc("Apply Optimizations"),
                         cl::init(false));

static cl::opt<bool> EmitLLVM("emit-llvm", cl::desc("Emit LLVM IR output"),
                              cl::init(false));

static cl::opt<bool> Debug("g", cl::desc("Enable debug mode"), cl::init(false));

static cl::opt<std::string> ArtsConfig("arts-config",
                                       cl::desc("ARTS configuration file path"),
                                       cl::value_desc("config_file"),
                                       cl::init(""));

/// Metadata file path
static cl::opt<std::string> MetadataFile("metadata-file",
                                         cl::desc("Path to metadata JSON file"),
                                         cl::value_desc("filename"),
                                         cl::init(".carts-metadata.json"));

///===----------------------------------------------------------------------===///
// Pipeline Stop Options
///===----------------------------------------------------------------------===///
enum class PipelineStage {
  CanonicalizeMemrefs,
  InitialCleanup,
  CollectMetadata,
  OpenMPToArts,
  EdtTransforms,
  CreateDbs,
  DbOpt,
  EdtOpt,
  Concurrency,
  ConcurrencyOpt,
  Epochs,
  PreLowering,
  ArtsToLLVM,
  Complete
};

static cl::opt<PipelineStage> StopAt(
    cl::desc("Stop pipeline at specified stage:"),
    cl::values(clEnumValN(PipelineStage::CanonicalizeMemrefs,
                          "canonicalize-memrefs",
                          "Stop after canonicalizing memrefs pass"),
               clEnumValN(PipelineStage::CollectMetadata, "collect-metadata",
                          "Stop after collecting metadata"),
               clEnumValN(PipelineStage::InitialCleanup, "initial-cleanup",
                          "Stop after initial cleanup and simplification"),
               clEnumValN(PipelineStage::OpenMPToArts, "openmp-to-arts",
                          "Stop after OpenMP to Arts conversion"),
               clEnumValN(PipelineStage::EdtTransforms, "edt-transforms",
                          "Stop after EDT transformations"),
               clEnumValN(PipelineStage::CreateDbs, "create-dbs",
                          "Stop after Dbs creation"),
               clEnumValN(PipelineStage::DbOpt, "db-opt",
                          "Stop after Dbs optimization"),
               clEnumValN(PipelineStage::Epochs, "epochs",
                          "Stop after Epochs creation"),
               clEnumValN(PipelineStage::EdtOpt, "edt-opt",
                          "Stop after EDT optimizations"),
               clEnumValN(PipelineStage::Concurrency, "concurrency",
                          "Stop after concurrency"),
               clEnumValN(PipelineStage::ConcurrencyOpt, "concurrency-opt",
                          "Stop after concurrency optimization"),
               clEnumValN(PipelineStage::PreLowering, "pre-lowering",
                          "Stop after pre-lowering transformations"),
               clEnumValN(PipelineStage::ArtsToLLVM, "arts-to-llvm",
                          "Stop after Arts to LLVM conversion"),
               clEnumValN(PipelineStage::Complete, "complete",
                          "Run complete pipeline (default)")),
    cl::init(PipelineStage::Complete));

///===----------------------------------------------------------------------===///
// Helper Functions for Initialization and Pass Setup
///===----------------------------------------------------------------------===///
/// Register standard MLIR dialects, passes, and translations.
void registerDialects(DialectRegistry &registry) {
  registry.insert<polygeist::PolygeistDialect, arts::ArtsDialect>();
  registerAllPasses();
  registerAllTranslations();
  registerpolygeistPasses();
  func::registerInlinerExtension(registry);
  registerAllDialects(registry);
  registerAllExtensions(registry);
  registerAllFromLLVMIRTranslations(registry);
  registerBuiltinDialectTranslation(registry);
  registerLLVMDialectTranslation(registry);
}

/// Initialize the MLIR context by loading necessary dialects and attaching
/// type interfaces.
void initializeContext(MLIRContext &context) {
  context.disableMultithreading(true);
  context.getOrLoadDialect<affine::AffineDialect>();
  context.getOrLoadDialect<func::FuncDialect>();
  context.getOrLoadDialect<DLTIDialect>();
  context.getOrLoadDialect<scf::SCFDialect>();
  context.getOrLoadDialect<async::AsyncDialect>();
  context.getOrLoadDialect<LLVM::LLVMDialect>();
  context.getOrLoadDialect<NVVM::NVVMDialect>();
  context.getOrLoadDialect<ROCDL::ROCDLDialect>();
  context.getOrLoadDialect<gpu::GPUDialect>();
  context.getOrLoadDialect<omp::OpenMPDialect>();
  context.getOrLoadDialect<math::MathDialect>();
  context.getOrLoadDialect<memref::MemRefDialect>();
  context.getOrLoadDialect<linalg::LinalgDialect>();
  context.getOrLoadDialect<polygeist::PolygeistDialect>();
  context.getOrLoadDialect<arts::ArtsDialect>();
  context.getOrLoadDialect<cf::ControlFlowDialect>();

  /// Register all necessary interfaces for LLVM conversion
  LLVM::LLVMFunctionType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMArrayType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMPointerType::attachInterface<MemRefInsider>(context);
  LLVM::LLVMStructType::attachInterface<MemRefInsider>(context);
  MemRefType::attachInterface<PtrElementModel<MemRefType>>(context);
  LLVM::LLVMStructType::attachInterface<PtrElementModel<LLVM::LLVMStructType>>(
      context);
  LLVM::LLVMPointerType::attachInterface<
      PtrElementModel<LLVM::LLVMPointerType>>(context);
  LLVM::LLVMArrayType::attachInterface<PtrElementModel<LLVM::LLVMArrayType>>(
      context);
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

/// Inliner and canonicalize memrefs pass.
void setupCanonicalizeMemrefs(PassManager &pm) {
  OpPassManager &optPM = pm.nest<func::FuncOp>();
  optPM.addPass(createLowerAffinePass());
  pm.addPass(createCSEPass());
  pm.addPass(createInlinerPass());
  pm.addPass(arts::createArtsInlinerPass());
  pm.addPass(arts::createCanonicalizeMemrefsPass());
}

/// Metadata collection pass.
void setupCollectMetadata(PassManager &pm, bool shouldExport = false,
                          StringRef metadataFile = "") {
  std::string actualMetadataFile =
      metadataFile.empty() ? MetadataFile.getValue() : metadataFile.str();
  /// Raise to affine first
  OpPassManager &optPM = pm.nest<func::FuncOp>();
  optPM.addPass(polygeist::replaceAffineCFGPass());
  optPM.addPass(polygeist::createRaiseSCFToAffinePass());
  optPM.addPass(polygeist::replaceAffineCFGPass());
  optPM.addPass(polygeist::createRaiseSCFToAffinePass());
  optPM.addPass(createCSEPass());
  if (shouldExport)
    pm.addPass(arts::createCollectMetadataPass(true, actualMetadataFile));
  else
    pm.addPass(arts::createCollectMetadataPass());
}

/// Initial cleanup and simplification passes.
void setupInitialCleanup(OpPassManager &optPM, bool optEnabled) {
  if (optEnabled) {
    optPM.addPass(createCSEPass());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());
    optPM.addPass(createMem2Reg());
    optPM.addPass(polygeist::createPolygeistCanonicalizePass());
    optPM.addPass(polygeist::replaceAffineCFGPass());
    optPM.addPass(affine::createAffineScalarReplacementPass());
    optPM.addPass(polygeist::replaceAffineCFGPass());
    optPM.addPass(createLoopInvariantCodeMotionPass());
    optPM.addPass(createCSEPass());
  }

  optPM.addPass(createLowerAffinePass());
  optPM.addPass(createCSEPass());
  optPM.addPass(polygeist::createCanonicalizeForPass());
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// OpenMP to ARTS conversion pass.
void setupOpenMPToArts(PassManager &pm) {
  pm.addPass(arts::createConvertOpenMPtoArtsPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createCSEPass());
}

/// EDT transformation passes.
void setupEdtTransforms(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(arts::createEdtPass(AM, false));
  pm.addPass(arts::createEdtInvariantCodeMotionPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createCSEPass());
}

/// Db creation pass.
void setupCreateDbs(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(arts::createCreateDbsPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// Db creation and optimization passes.
void setupDbOpt(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(arts::createDbPass(AM));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(createMem2Reg());
}

/// EDT optimization passes.
void setupEdtOpt(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createEdtPass(AM, true));
  pm.addPass(createCSEPass());
}

/// Concurrency passes.
void setupConcurrency(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createConcurrencyPass(AM));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createForLoweringPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
}

/// Concurrency optimization passes.
void setupConcurrencyOpt(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(arts::createDbPass(AM));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(createMem2Reg());
  // pm.addPass(createConcurrencyOptPass());
}

/// Epoch creation passes.
void setupEpochs(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createCreateEpochsPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  // pm.addPass(arts::createEpochsPass());
  // pm.addPass(createMem2Reg());
  // pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// Pre-lowering passes.
void setupPreLowering(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  pm.addPass(arts::createParallelEdtLoweringPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createDbLoweringPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createEdtLoweringPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createEpochLoweringPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
}

/// Setup ARTS to LLVM conversion passes.
void setupArtsToLLVM(PassManager &pm, bool debug) {
  pm.addPass(arts::createConvertArtsToLLVMPass(debug));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createControlFlowSinkPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// Setup additional optimizations (post-ARTS pipeline).
void setupAdditionalOptimizations(OpPassManager &optPM) {
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
  optPM.addPass(createControlFlowSinkPass());
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
  optPM.addPass(createLoopInvariantCodeMotionPass());
  optPM.addPass(createCSEPass());
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// Setup LLVM IR emission passes.
void setupLLVMIREmission(PassManager &pm) {
  pm.addPass(createCSEPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(polygeist::createConvertPolygeistToLLVMPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
}

/// Configure the pass manager with the optimization passes.
void setupPassManager(ModuleOp module, MLIRContext &context,
                      PipelineStage stopAt = PipelineStage::Complete) {
  /// Create module-level analysis manager for caching across functions
  std::unique_ptr<arts::ArtsAnalysisManager> AM =
      std::make_unique<arts::ArtsAnalysisManager>(module, ArtsConfig);

  /// Canonicalize memrefs
  {
    PassManager pm(&context);
    setupCanonicalizeMemrefs(pm);
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when canonicalizing memrefs");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::CanonicalizeMemrefs)
    return;

  /// Metadata collection
  if (stopAt == PipelineStage::CollectMetadata) {
    PassManager pm(&context);
    setupCollectMetadata(pm, true);
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when collecting metadata");
      module->dump();
      return;
    }
    return;
  }

  /// Initial cleanup
  {
    PassManager pm(&context);
    OpPassManager &optPM = pm.nest<func::FuncOp>();
    setupInitialCleanup(optPM, Opt);

    if (failed(pm.run(module))) {
      ARTS_ERROR("Error simplyfing the IR");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::InitialCleanup)
    return;

  /// OpenMP to ARTS conversion
  {
    PassManager pm(&context);
    setupOpenMPToArts(pm);
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when converting OpenMP to ARTS");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::OpenMPToArts)
    return;

  /// EDT transformations
  {
    PassManager pm(&context);
    setupEdtTransforms(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when running EDT transformations");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::EdtTransforms)
    return;

  /// Create Dbs
  {
    PassManager pm(&context);
    setupCreateDbs(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when creating Dbs");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::CreateDbs)
    return;

  /// Db optimizations
  {
    PassManager pm(&context);
    setupDbOpt(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when optimizing Dbs");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::DbOpt)
    return;

  /// Edt optimizations
  {
    PassManager pm(&context);
    setupEdtOpt(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when optimizing EDTs");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::EdtOpt)
    return;

  /// Concurrency
  {
    PassManager pm(&context);
    setupConcurrency(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when running concurrency");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::Concurrency)
    return;

  /// Concurrency optimizations
  {
    PassManager pm(&context);
    setupConcurrencyOpt(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when optimizing concurrency");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::ConcurrencyOpt)
    return;

  /// Epochs: creation and optimization
  {
    PassManager pm(&context);
    setupEpochs(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when creating and optimizing epochs");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::Epochs)
    return;

  /// Pre-lowering
  {
    PassManager pm(&context);
    setupPreLowering(pm, AM.get());
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when pre-lowering DBs, EDTs, and Epochs");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::PreLowering)
    return;

  /// Convert ARTS to LLVM
  {
    PassManager pm(&context);
    setupArtsToLLVM(pm, Debug);
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when converting ARTS to LLVM");
      module->dump();
      return;
    }
  }
  if (stopAt == PipelineStage::ArtsToLLVM)
    return;

  /// Optimizations
  if (Opt) {
    PassManager pm(&context);
    OpPassManager &optPM = pm.nest<func::FuncOp>();
    setupAdditionalOptimizations(optPM);

    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when running classical optimizations");
      module->dump();
      return;
    }
  }

  /// LLVM IR conversion
  if (EmitLLVM) {
    PassManager pm(&context);
    setupLLVMIREmission(pm);
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when emitting LLVM IR");
      module->dump();
      return;
    }
  }
}

///===----------------------------------------------------------------------===///
// Main Function
///===----------------------------------------------------------------------===///
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
    ARTS_ERROR("Could not open input file: " << InputFilename);
    return 1;
  }

  /// Parse the input module.
  auto module = parseSourceString<ModuleOp>(file->getBuffer(), &context);
  if (!module) {
    ARTS_ERROR("Could not parse input file");
    return 1;
  }

  /// Run the pass pipeline once.
  setupPassManager(module.get(), context, StopAt);

  /// Translate the optimized module to LLVM IR and write output.
  if (EmitLLVM) {
    LLVMContext llvmContext;
    auto llvmModule = translateModuleToLLVMIR(module.get(), llvmContext);
    if (!llvmModule) {
      module->dump();
      ARTS_ERROR("Failed to emit LLVM IR");
      return -1;
    }
    std::string llvmIR;
    raw_string_ostream llvmStream(llvmIR);
    llvmModule->print(llvmStream, nullptr);

    auto output = openOutputFile(OutputFilename);
    if (!output) {
      ARTS_ERROR("Could not open output file: " << OutputFilename);
      return 1;
    }
    output->os() << llvmStream.str();
    output->keep();
  } else {
    /// Otherwise, print the final MLIR module.
    auto output = openOutputFile(OutputFilename);
    if (!output) {
      ARTS_ERROR("Could not open output file: " << OutputFilename);
      return 1;
    }
    module->print(output->os());
    output->keep();
  }

  return 0;
}
