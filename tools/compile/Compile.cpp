///==========================================================================///
/// File: Compile.cpp
/// Main entry point for the CARTS compilation pipeline tool.
///==========================================================================///

#include "mlir/Conversion/Passes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Arith/Transforms/Passes.h"
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

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/passes/Passes.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

using namespace llvm;
using namespace mlir;

ARTS_DEBUG_SETUP(compile)

namespace {
constexpr const char *kDefaultMetadataFile = ".carts-metadata.json";
constexpr const char *kDefaultDiagnoseOutput = ".carts-diagnose.json";
constexpr uint64_t kDefaultArtsIdStride = 1000;
constexpr int64_t kDefaultKernelTransformsTileJ = 64;
constexpr int64_t kDefaultKernelTransformsMinTripCount = 128;
} // namespace

///===----------------------------------------------------------------------===///
/// Interface Attachments
///===----------------------------------------------------------------------===///
/// Use the original models for attaching type interfaces.
class MemRefInsider
    : public MemRefElementTypeInterface::FallbackModel<MemRefInsider> {};

template <typename T>
struct PtrElementModel
    : public LLVM::PointerElementTypeInterface::ExternalModel<
          PtrElementModel<T>, T> {};

///===----------------------------------------------------------------------===///
/// Command Line Options
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
                                         cl::init(kDefaultMetadataFile));

/// Shorthand for metadata export in carts_cli.py / benchmark scripts.
/// Equivalent to --pipeline=collect-metadata.
static cl::opt<bool> CollectMetadataFlag(
    "collect-metadata",
    cl::desc("Collect and export metadata, then stop the pipeline"),
    cl::init(false));

static cl::opt<uint64_t>
    ArtsIdStride("arts-id-stride",
                 cl::desc("Stride multiplier for arts ids (EDTs/DBs)"),
                 cl::init(kDefaultArtsIdStride));

static cl::opt<bool>
    Diagnose("diagnose",
             cl::desc("Export diagnostic information about compilation"),
             cl::init(false));

static cl::opt<std::string> DiagnoseOutput(
    "diagnose-output", cl::desc("Output file for diagnostic JSON export"),
    cl::value_desc("filename"), cl::init(kDefaultDiagnoseOutput));

static cl::opt<std::string> ArtsDebug(
    "arts-debug",
    cl::desc("Enable ARTS_INFO/ARTS_DEBUG channels (comma-separated)"),
    cl::value_desc("debug_types"), cl::init(""));

/// Kernel transform options (Pipeline step: pattern-pipeline)
static cl::opt<bool> KernelTransformsEnableElementwisePipeline(
    "loop-transforms-enable-elementwise-pipeline",
    cl::desc("Enable semantic fusion of sibling pointwise loop pipelines"),
    cl::init(true));

static cl::opt<bool> KernelTransformsEnableMatmul(
    "loop-transforms-enable-matmul",
    cl::desc("Enable reduction-aware matmul transforms (dot -> update form)"),
    cl::init(true));

static cl::opt<bool> KernelTransformsEnableTiling(
    "loop-transforms-enable-tiling",
    cl::desc("Enable tiling for kernel transforms"), cl::init(true));

static cl::opt<int64_t> KernelTransformsTileJ(
    "loop-transforms-tile-j",
    cl::desc("Tile size for j-dimension in kernel transforms"),
    cl::init(kDefaultKernelTransformsTileJ));

static cl::opt<int64_t> KernelTransformsMinTripCount(
    "loop-transforms-min-trip-count",
    cl::desc("Minimum constant trip count required to apply tiling"),
    cl::init(kDefaultKernelTransformsMinTripCount));

/// Partition fallback strategy
enum class PartitionFallback { Coarse, Fine };

static cl::opt<PartitionFallback> PartitionFallbackMode(
    "partition-fallback",
    cl::desc("Fallback strategy for non-partitionable accesses:"),
    cl::values(
        clEnumValN(PartitionFallback::Coarse, "coarse",
                   "Use coarse allocation - serializes access (default)"),
        clEnumValN(PartitionFallback::Fine, "fine",
                   "Use fine-grained (element-wise) allocation")),
    cl::init(PartitionFallback::Coarse));

/// Distributed DB allocation enablement (after DbPartitioning).
static cl::opt<bool> DistributedDb(
    "distributed-db",
    cl::desc("Attempt distributed DB allocation "
             "(ownership marking + parallel initPerWorker creation; "
             "auto-enables distributed host loop outlining for eligible "
             "producer loops)"),
    cl::init(false));

/// Enable epoch finish-continuation lowering (replaces blocking
/// arts_wait_on_handle with ARTS-native finish-EDT continuation scheduling
/// for eligible epoch patterns). See RFC:
/// docs/compiler/epoch-finish-continuation-rfc.md
static cl::opt<bool> EpochFinishContinuation(
    "arts-epoch-finish-continuation",
    cl::desc("Enable epoch finish-continuation lowering (replace blocking "
             "epoch waits with ARTS-native finish-EDT continuation scheduling "
             "for eligible patterns)"),
    cl::init(true));

///===----------------------------------------------------------------------===///
/// Pipeline Stop Options
///===----------------------------------------------------------------------===///
enum class PipelineStep {
  RaiseMemRefDimensionality,
  CollectMetadata,
  InitialCleanup,
  OpenMPToArts,
  PatternPipeline,
  EdtTransforms,
  CreateDbs,
  DbOpt,
  EdtOpt,
  Concurrency,
  EdtDistribution,
  ConcurrencyOpt,
  Epochs,
  PreLowering,
  ArtsToLLVM,
  Complete
};

static cl::opt<PipelineStep> Pipeline(
    "pipeline", cl::desc("Stop pipeline at specified step:"),
    cl::values(clEnumValN(PipelineStep::RaiseMemRefDimensionality,
                          "raise-memref-dimensionality",
                          "Stop after the raise-memref-dimensionality pipeline step"),
               clEnumValN(PipelineStep::RaiseMemRefDimensionality,
                          "canonicalize-memrefs",
                          "Alias for raise-memref-dimensionality"),
               clEnumValN(PipelineStep::CollectMetadata, "collect-metadata",
                          "Stop after collecting metadata"),
               clEnumValN(PipelineStep::InitialCleanup, "initial-cleanup",
                          "Stop after initial cleanup and simplification"),
               clEnumValN(PipelineStep::OpenMPToArts, "openmp-to-arts",
                          "Stop after OpenMP to Arts conversion"),
               clEnumValN(PipelineStep::PatternPipeline, "pattern-pipeline",
                          "Stop after the dedicated semantic pattern pipeline"),
               clEnumValN(PipelineStep::EdtTransforms, "edt-transforms",
                          "Stop after EDT transformations"),
               clEnumValN(PipelineStep::CreateDbs, "create-dbs",
                          "Stop after DB creation"),
               clEnumValN(PipelineStep::DbOpt, "db-opt",
                          "Stop after DB optimization"),
               clEnumValN(PipelineStep::EdtOpt, "edt-opt",
                          "Stop after EDT optimizations"),
               clEnumValN(PipelineStep::Concurrency, "concurrency",
                          "Stop after concurrency"),
               clEnumValN(PipelineStep::EdtDistribution, "edt-distribution",
                          "Stop after EDT distribution and for lowering"),
               clEnumValN(PipelineStep::ConcurrencyOpt, "concurrency-opt",
                          "Stop after concurrency optimization"),
               clEnumValN(PipelineStep::Epochs, "epochs",
                          "Stop after Epochs creation"),
               clEnumValN(PipelineStep::PreLowering, "pre-lowering",
                          "Stop after pre-lowering transformations"),
               clEnumValN(PipelineStep::ArtsToLLVM, "arts-to-llvm",
                          "Stop after Arts to LLVM conversion"),
               clEnumValN(PipelineStep::Complete, "complete",
                          "Run complete pipeline (default)")),
    cl::init(PipelineStep::Complete));

static cl::opt<PipelineStep> StartFrom(
    "start-from", cl::desc("Resume pipeline from specified step:"),
    cl::values(clEnumValN(PipelineStep::RaiseMemRefDimensionality,
                          "raise-memref-dimensionality",
                          "Start from the raise-memref-dimensionality pipeline step "
                          "(default)"),
               clEnumValN(PipelineStep::RaiseMemRefDimensionality,
                          "canonicalize-memrefs",
                          "Alias for raise-memref-dimensionality"),
               clEnumValN(PipelineStep::CollectMetadata, "collect-metadata",
                          "Start from collecting metadata"),
               clEnumValN(PipelineStep::InitialCleanup, "initial-cleanup",
                          "Start from initial cleanup and simplification"),
               clEnumValN(PipelineStep::OpenMPToArts, "openmp-to-arts",
                          "Start from OpenMP to Arts conversion"),
               clEnumValN(PipelineStep::PatternPipeline, "pattern-pipeline",
                          "Start from the semantic pattern pipeline"),
               clEnumValN(PipelineStep::EdtTransforms, "edt-transforms",
                          "Start from EDT transformations"),
               clEnumValN(PipelineStep::CreateDbs, "create-dbs",
                          "Start from DB creation"),
               clEnumValN(PipelineStep::DbOpt, "db-opt",
                          "Start from DB optimization"),
               clEnumValN(PipelineStep::EdtOpt, "edt-opt",
                          "Start from EDT optimizations"),
               clEnumValN(PipelineStep::Concurrency, "concurrency",
                          "Start from concurrency"),
               clEnumValN(PipelineStep::EdtDistribution, "edt-distribution",
                          "Start from EDT distribution and for lowering"),
               clEnumValN(PipelineStep::ConcurrencyOpt, "concurrency-opt",
                          "Start from concurrency optimization"),
               clEnumValN(PipelineStep::Epochs, "epochs",
                          "Start from Epochs creation"),
               clEnumValN(PipelineStep::PreLowering, "pre-lowering",
                          "Start from pre-lowering transformations"),
               clEnumValN(PipelineStep::ArtsToLLVM, "arts-to-llvm",
                          "Start from Arts to LLVM conversion")),
    cl::init(PipelineStep::RaiseMemRefDimensionality));

static cl::opt<bool> PrintPipelineTokensJSON(
    "print-pipeline-tokens-json",
    cl::desc("Print canonical pipeline step tokens as JSON and exit"),
    cl::init(false));

static cl::opt<bool> PrintPipelineManifestJSON(
    "print-pipeline-manifest-json",
    cl::desc("Print pipeline step/pass manifest as JSON and exit"),
    cl::init(false));

static const std::array<llvm::StringLiteral, 9> kRaiseMemRefDimensionalityPasses = {
    "LowerAffine(func)",
    "CSE",
    "Inliner",
    "ArtsInliner",
    "PolygeistCanonicalize",
    "RaiseMemRefDimensionality",
    "HandleDeps",
    "DeadCodeElimination",
    "CSE"};
static const std::array<llvm::StringLiteral, 7> kCollectMetadataPasses = {
    "replaceAffineCFG(func)",
    "RaiseSCFToAffine(func)",
    "replaceAffineCFG(func)",
    "RaiseSCFToAffine(func)",
    "CSE(func)",
    "CollectMetadata",
    "VerifyMetadata (diagnose mode)"};
static const std::array<llvm::StringLiteral, 3> kInitialCleanupPasses = {
    "LowerAffine(func)", "CSE(func)", "PolygeistCanonicalizeFor(func)"};
static const std::array<llvm::StringLiteral, 3> kOpenMPToArtsPasses = {
    "ConvertOpenMPToArts", "DeadCodeElimination", "CSE"};
static const std::array<llvm::StringLiteral, 8> kPatternPipelinePasses = {
    "PatternDiscovery(seed)", "DepTransforms",
    "LoopNormalization",      "StencilBoundaryPeeling",
    "LoopReordering",         "PatternDiscovery(refine)",
    "KernelTransforms",       "CSE"};
static const std::array<llvm::StringLiteral, 6> kEdtTransformsPasses = {
    "EdtStructuralOpt(runAnalysis=false)",
    "EdtICM",
    "DeadCodeElimination",
    "SymbolDCE",
    "CSE",
    "EdtPtrRematerialization"};
static const std::array<llvm::StringLiteral, 9> kCreateDbsPasses = {
    "DistributedHostLoopOutlining (conditional)",
    "CreateDbs",
    "CSE (bridge cleanup, conditional)",
    "PolygeistCanonicalize",
    "CSE",
    "SymbolDCE",
    "Mem2Reg",
    "PolygeistCanonicalize",
    "VerifyDbCreated"};
static const std::array<llvm::StringLiteral, 4> kDbOptPasses = {
    "DbModeTightening", "PolygeistCanonicalize", "CSE", "Mem2Reg"};
static const std::array<llvm::StringLiteral, 4> kEdtOptPasses = {
    "PolygeistCanonicalize", "EdtStructuralOpt(runAnalysis=true)", "LoopFusion",
    "CSE"};
static const std::array<llvm::StringLiteral, 4> kConcurrencyPasses = {
    "PolygeistCanonicalize", "Concurrency", "ForOpt", "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 3> kEdtDistributionPasses = {
    "EdtDistribution", "ForLowering", "VerifyForLowered"};
static const std::array<llvm::StringLiteral, 24> kConcurrencyOptPasses = {
    "EdtStructuralOpt(runAnalysis=false)",
    "DeadCodeElimination",
    "PolygeistCanonicalize",
    "CSE",
    "EdtStructuralOpt(runAnalysis=false)",
    "EpochOpt",
    "PolygeistCanonicalize",
    "CSE",
    "DbPartitioning",
    "DbDistributedOwnership (conditional)",
    "DbTransforms",
    "DbModeTightening",
    "EdtTransforms",
    "ContractValidation",
    "DbScratchElimination",
    "PolygeistCanonicalize",
    "CSE",
    "BlockLoopStripMining(func)",
    "Hoisting",
    "PolygeistCanonicalize",
    "CSE",
    "EdtAllocaSinking",
    "DeadCodeElimination",
    "Mem2Reg"};
static const std::array<llvm::StringLiteral, 4> kEpochsPasses = {
    "PolygeistCanonicalize", "CreateEpochs",
    "EpochContinuationPrep (conditional)", "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 23> kPreLoweringPasses = {
    "EdtAllocaSinking",
    "ParallelEdtLowering",
    "EpochContinuationPrep (conditional)",
    "PolygeistCanonicalize",
    "CSE",
    "DbLowering",
    "PolygeistCanonicalize",
    "CSE",
    "EdtLowering",
    "PolygeistCanonicalize",
    "CSE",
    "LICM",
    "DataPtrHoisting",
    "PolygeistCanonicalize",
    "CSE",
    "ScalarReplacement",
    "PolygeistCanonicalize",
    "CSE",
    "EpochLowering",
    "LoweringContractCleanup",
    "PolygeistCanonicalize",
    "CSE",
    "VerifyPreLowered"};
static const std::array<llvm::StringLiteral, 11> kArtsToLLVMPasses = {
    "ConvertArtsToLLVM",
    "GuidRangCallOpt",
    "RuntimeCallOpt",
    "DataPtrHoisting",
    "PolygeistCanonicalize",
    "CSE",
    "Mem2Reg",
    "PolygeistCanonicalize",
    "ControlFlowSink",
    "PolygeistCanonicalize",
    "VerifyLowered"};

struct PipelineStepTokenSpec {
  PipelineStep step;
  llvm::StringLiteral token;
  bool allowPipelineStop;
  bool allowStartFrom;
  bool inPipelineSequence;
  llvm::ArrayRef<llvm::StringLiteral> passes;
};

static const std::array<PipelineStepTokenSpec, 16> kPipelineStepSpecs = {{
    {PipelineStep::RaiseMemRefDimensionality, "raise-memref-dimensionality", true, true,
     true, kRaiseMemRefDimensionalityPasses},
    {PipelineStep::CollectMetadata, "collect-metadata", true, true, true,
     kCollectMetadataPasses},
    {PipelineStep::InitialCleanup, "initial-cleanup", true, true, true,
     kInitialCleanupPasses},
    {PipelineStep::OpenMPToArts, "openmp-to-arts", true, true, true,
     kOpenMPToArtsPasses},
    {PipelineStep::PatternPipeline, "pattern-pipeline", true, true, true,
     kPatternPipelinePasses},
    {PipelineStep::EdtTransforms, "edt-transforms", true, true, true,
     kEdtTransformsPasses},
    {PipelineStep::CreateDbs, "create-dbs", true, true, true, kCreateDbsPasses},
    {PipelineStep::DbOpt, "db-opt", true, true, true, kDbOptPasses},
    {PipelineStep::EdtOpt, "edt-opt", true, true, true, kEdtOptPasses},
    {PipelineStep::Concurrency, "concurrency", true, true, true,
     kConcurrencyPasses},
    {PipelineStep::EdtDistribution, "edt-distribution", true, true, true,
     kEdtDistributionPasses},
    {PipelineStep::ConcurrencyOpt, "concurrency-opt", true, true, true,
     kConcurrencyOptPasses},
    {PipelineStep::Epochs, "epochs", true, true, true, kEpochsPasses},
    {PipelineStep::PreLowering, "pre-lowering", true, true, true,
     kPreLoweringPasses},
    {PipelineStep::ArtsToLLVM, "arts-to-llvm", true, true, true,
     kArtsToLLVMPasses},
    {PipelineStep::Complete, "complete", true, false, false,
     llvm::ArrayRef<llvm::StringLiteral>()},
}};

static const PipelineStepTokenSpec *findPipelineStepSpec(PipelineStep step) {
  for (const auto &spec : kPipelineStepSpecs) {
    if (spec.step == step)
      return &spec;
  }
  return nullptr;
}

static int pipelineStepIndex(PipelineStep step) {
  for (size_t i = 0; i < kPipelineStepSpecs.size(); ++i) {
    if (kPipelineStepSpecs[i].step == step)
      return static_cast<int>(i);
  }
  return -1;
}

static llvm::StringRef pipelineStepName(PipelineStep step) {
  if (const auto *spec = findPipelineStepSpec(step))
    return spec->token;
  return "unknown";
}

template <typename Predicate>
static void printPipelineTokenArray(llvm::raw_ostream &os, llvm::StringRef key,
                                    Predicate include) {
  os << "  \"" << key << "\": [";
  bool first = true;
  for (const auto &spec : kPipelineStepSpecs) {
    if (!include(spec))
      continue;
    if (!first)
      os << ", ";
    first = false;
    os << "\"" << spec.token << "\"";
  }
  os << "]";
}

static void printStringArray(llvm::raw_ostream &os,
                             llvm::ArrayRef<llvm::StringLiteral> values) {
  os << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0)
      os << ", ";
    os << "\"" << values[i] << "\"";
  }
  os << "]";
}

static void printPipelineTokensAsJSON(llvm::raw_ostream &os) {
  os << "{\n";
  printPipelineTokenArray(
      os, "pipeline",
      [](const PipelineStepTokenSpec &spec) { return spec.allowPipelineStop; });
  os << ",\n";
  printPipelineTokenArray(
      os, "start_from",
      [](const PipelineStepTokenSpec &spec) { return spec.allowStartFrom; });
  os << ",\n";
  printPipelineTokenArray(os, "pipeline_sequence",
                          [](const PipelineStepTokenSpec &spec) {
                            return spec.inPipelineSequence;
                          });
  os << "\n";
  os << "}\n";
}

static void printPipelineManifestAsJSON(llvm::raw_ostream &os) {
  os << "{\n";
  printPipelineTokenArray(
      os, "pipeline",
      [](const PipelineStepTokenSpec &spec) { return spec.allowPipelineStop; });
  os << ",\n";
  printPipelineTokenArray(
      os, "start_from",
      [](const PipelineStepTokenSpec &spec) { return spec.allowStartFrom; });
  os << ",\n";
  printPipelineTokenArray(os, "pipeline_sequence",
                          [](const PipelineStepTokenSpec &spec) {
                            return spec.inPipelineSequence;
                          });
  os << ",\n";
  os << "  \"pipeline_steps\": [\n";
  bool firstStep = true;
  for (const auto &spec : kPipelineStepSpecs) {
    if (!spec.inPipelineSequence)
      continue;
    if (!firstStep)
      os << ",\n";
    firstStep = false;
    os << "    {\"name\": \"" << spec.token << "\", \"passes\": ";
    printStringArray(os, spec.passes);
    os << "}";
  }
  os << "\n  ]\n";
  os << "}\n";
}

static void configureArtsDebugChannels(llvm::StringRef channels) {
  if (channels.empty())
    return;

  llvm::SmallVector<llvm::StringRef, 8> splitChannels;
  channels.split(splitChannels, ',', -1, false);

  llvm::SmallVector<std::string, 8> ownedChannels;
  ownedChannels.reserve(splitChannels.size());
  for (llvm::StringRef channel : splitChannels) {
    llvm::StringRef trimmed = channel.trim();
    if (!trimmed.empty())
      ownedChannels.push_back(trimmed.str());
  }
  if (ownedChannels.empty())
    return;

  llvm::DebugFlag = true;
  llvm::SmallVector<const char *, 8> debugTypes;
  debugTypes.reserve(ownedChannels.size());
  for (const std::string &owned : ownedChannels)
    debugTypes.push_back(owned.c_str());
  llvm::setCurrentDebugTypes(debugTypes.data(), debugTypes.size());
}

static bool shouldExportDetailedDiagnose(PipelineStep stopAt) {
  if (stopAt == PipelineStep::Complete)
    return true;
  int stopIndex = pipelineStepIndex(stopAt);
  int preLoweringIndex = pipelineStepIndex(PipelineStep::PreLowering);
  return stopIndex >= 0 && preLoweringIndex >= 0 &&
         stopIndex >= preLoweringIndex;
}

///===----------------------------------------------------------------------===///
/// Helper Functions for Initialization and Pass Setup
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
  IndexType::attachInterface<PtrElementModel<IndexType>>(context);
  LLVM::LLVMStructType::attachInterface<PtrElementModel<LLVM::LLVMStructType>>(
      context);
  LLVM::LLVMPointerType::attachInterface<
      PtrElementModel<LLVM::LLVMPointerType>>(context);
  LLVM::LLVMArrayType::attachInterface<PtrElementModel<LLVM::LLVMArrayType>>(
      context);
}

///===----------------------------------------------------------------------===///
/// Pipeline Builders
///===----------------------------------------------------------------------===///
/// Each build*Pipeline function populates a PassManager with the passes for
/// one logical compilation step. There is a 1:1 mapping between
/// PipelineStep enum values and these builders.

static void addCanonicalizeAndCSE(PassManager &pm) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
}

/// Raise nested pointer allocations to N-dimensional memrefs.
void buildRaiseMemRefDimensionalityPipeline(PassManager &pm) {
  OpPassManager &optPM = pm.nest<func::FuncOp>();
  optPM.addPass(createLowerAffinePass());
  pm.addPass(createCSEPass());
  pm.addPass(createInlinerPass());
  pm.addPass(arts::createArtsInlinerPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createRaiseMemRefDimensionalityPass());
  pm.addPass(arts::createHandleDepsPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createCSEPass());
}

/// Metadata collection pass.
void buildCollectMetadataPipeline(PassManager &pm,
                                  arts::AnalysisManager *AM = nullptr,
                                  bool shouldExport = false,
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
  if (AM)
    pm.addPass(arts::createVerifyMetadataPass(AM));
}

/// Initial cleanup and simplification passes.
void buildInitialCleanupPipeline(OpPassManager &optPM) {
  optPM.addPass(createLowerAffinePass());
  optPM.addPass(createCSEPass());
  optPM.addPass(polygeist::createCanonicalizeForPass());
}

/// OpenMP to ARTS conversion pass.
void buildOpenMPToArtsPipeline(PassManager &pm) {
  pm.addPass(arts::createConvertOpenMPToArtsPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createCSEPass());
}

/// EDT transformation passes.
void buildEdtTransformsPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(arts::createEdtStructuralOptPass(AM, false));
  pm.addPass(arts::createEdtICMPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createEdtPtrRematerializationPass());
}

/// Dedicated semantic pattern pipeline that teaches ARTS about supported loop
/// and dependence families before DB creation.
void buildPatternPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(arts::createPatternDiscoveryPass(AM, /*refine=*/false));
  pm.addPass(arts::createDepTransformsPass());
  pm.addPass(arts::createLoopNormalizationPass(AM));
  pm.addPass(arts::createStencilBoundaryPeelingPass());
  pm.addPass(arts::createLoopReorderingPass(AM));
  pm.addPass(arts::createPatternDiscoveryPass(AM, /*refine=*/true));
  pm.addPass(arts::createKernelTransformsPass(
      AM, KernelTransformsEnableElementwisePipeline,
      KernelTransformsEnableMatmul, KernelTransformsEnableTiling,
      KernelTransformsTileJ, KernelTransformsMinTripCount));
  pm.addPass(createCSEPass());
}

/// Bridge eligible host producer loops into the regular ARTS loop/EDT path
/// before DB creation without making this outlining part of the semantic
/// pattern pipeline itself.
void buildPreCreateDbsBridgingPipeline(PassManager &pm,
                                       arts::AnalysisManager *AM) {
  /// Multinode execution needs eligible host producer loops to flow through
  /// the regular arts.for/EDT pipeline before CreateDbs; otherwise serial host
  /// writes between distributed phases bypass the normal DB acquire/release
  /// coherence path. --distributed-db relies on the same outlining for
  /// ownership inference, but correctness requires it for general multinode
  /// block partitioning as well.
  bool enableDistributedHostLoopOutlining = false;
  if (AM) {
    const auto &machine = AM->getAbstractMachine();
    enableDistributedHostLoopOutlining =
        DistributedDb ||
        (machine.hasValidNodeCount() && machine.getNodeCount() > 1);
  } else {
    enableDistributedHostLoopOutlining = DistributedDb;
  }
  if (enableDistributedHostLoopOutlining) {
    pm.addPass(arts::createDistributedHostLoopOutliningPass(AM));
    pm.addPass(createCSEPass());
  }
}

/// DB creation pass.
void buildCreateDbsPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  buildPreCreateDbsBridgingPipeline(pm, AM);
  pm.addPass(arts::createCreateDbsPass(AM));
  addCanonicalizeAndCSE(pm);
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createVerifyDbCreatedPass());
}

/// DB creation and optimization passes.
void buildDbOptPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(arts::createDbModeTighteningPass(AM));
  addCanonicalizeAndCSE(pm);
  pm.addPass(createMem2Reg());
}

/// EDT optimization passes.
void buildEdtOptPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createEdtStructuralOptPass(AM, /*runAnalysis*/ true));
  pm.addPass(arts::createLoopFusionPass(AM));
  pm.addPass(createCSEPass());
}

/// Concurrency passes.
void buildConcurrencyPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createConcurrencyPass(AM));
  pm.addPass(arts::createForOptPass(AM));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// EDT distribution and loop lowering passes.
void buildEdtDistributionPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(arts::createEdtDistributionPass(AM));
  pm.addPass(arts::createForLoweringPass(AM));
  pm.addPass(arts::createVerifyForLoweredPass());
}

/// Concurrency optimization passes.
void buildConcurrencyOptPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  /// EDT optimization
  pm.addPass(arts::createEdtStructuralOptPass(AM, /*runAnalysis*/ false));
  pm.addPass(arts::createDCEPass());
  addCanonicalizeAndCSE(pm);
  /// Re-run structural opts after cleanup to catch newly exposed degenerate
  /// EDTs before epoch shaping.
  /// TODO(PERF): EdtStructuralOptPass runs 4 times; evaluate if second call
  /// needed.
  pm.addPass(arts::createEdtStructuralOptPass(AM, /*runAnalysis*/ false));
  pm.addPass(arts::createEpochOptPass());
  addCanonicalizeAndCSE(pm);
  /// Partition DBs and run DbModeTighteningPass again to adjust modes.
  pm.addPass(arts::createDbPartitioningPass(AM));
  /// Run distributed-ownership eligibility while EDT/DB ops are still in
  /// high-level form so analysis can reason about DbAcquire<->Edt users.
  if (DistributedDb)
    pm.addPass(arts::createDbDistributedOwnershipPass(AM));
  pm.addPass(arts::createDbTransformsPass(AM));
  /// DbModeTighteningPass performs local DB cleanup after mode adjustment,
  /// which can expose new zero-dependency or degenerate EDTs before epoch
  /// shaping. Mode tightening must run before EDT transforms so affinity and
  /// reduction analysis see accurate writer/reader modes.
  pm.addPass(arts::createDbModeTighteningPass(AM));
  pm.addPass(arts::createEdtTransformsPass(AM));
  pm.addPass(arts::createContractValidationPass());
  pm.addPass(arts::createDbScratchEliminationPass());
  addCanonicalizeAndCSE(pm);
  pm.addNestedPass<func::FuncOp>(arts::createBlockLoopStripMiningPass());
  pm.addPass(arts::createHoistingPass());
  addCanonicalizeAndCSE(pm);
  /// Others
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createMem2Reg());
}

/// Epoch creation passes.
void buildEpochsPipeline(PassManager &pm, bool enableContinuation) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createCreateEpochsPass());
  if (enableContinuation)
    pm.addPass(arts::createEpochContinuationPrepPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// Pre-lowering passes.
void buildPreLoweringPipeline(PassManager &pm, arts::AnalysisManager *AM,
                              bool enableContinuation) {
  /// TODO(PERF): EdtAllocaSinkingPass runs twice (buildConcurrencyOptPipeline
  /// and here).
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createParallelEdtLoweringPass());
  if (enableContinuation)
    pm.addPass(arts::createEpochContinuationPrepPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createDbLoweringPass(ArtsIdStride));
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createEdtLoweringPass(AM, ArtsIdStride));
  addCanonicalizeAndCSE(pm);
  pm.addPass(createLoopInvariantCodeMotionPass());
  /// Hoist loop-invariant DB/dep pointer loads before scalar replacement;
  /// buildArtsToLLVMPipeline runs hoisting again after Arts->LLVM materializes
  /// new loads.
  pm.addPass(arts::createDataPtrHoistingPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createScalarReplacementPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createEpochLoweringPass());
  pm.addPass(arts::createLoweringContractCleanupPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createVerifyPreLoweredPass());
}

/// ARTS to LLVM conversion passes.
void buildArtsToLLVMPipeline(PassManager &pm, bool debug,
                             bool distributedInitPerWorker,
                             const arts::AbstractMachine *machine) {
  pm.addPass(arts::createConvertArtsToLLVMPass(debug, distributedInitPerWorker,
                                               machine));
  pm.addPass(arts::createGuidRangCallOptPass());
  pm.addPass(arts::createRuntimeCallOptPass());
  /// Hoist loop-invariant loads after Arts->LLVM lowering for
  /// vectorization/LICM.
  pm.addPass(arts::createDataPtrHoistingPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createControlFlowSinkPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createVerifyLoweredPass());
}

/// Additional optimizations (post-ARTS pipeline).
void buildAdditionalOptPipeline(OpPassManager &optPM) {
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
  optPM.addPass(createControlFlowSinkPass());
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
  optPM.addPass(createLoopInvariantCodeMotionPass());
  optPM.addPass(createCSEPass());
  optPM.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// LLVM IR emission passes.
void buildLLVMIREmissionPipeline(PassManager &pm) {
  pm.addPass(createCSEPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arith::createArithExpandOpsPass());
  pm.addPass(polygeist::createConvertPolygeistToLLVMPass());
  pm.addPass(arts::createAliasScopeGenPass());
  pm.addPass(arts::createLoopVectorizationHintsPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
}

/// Hooks invoked around each pipeline step for diagnostics or custom logging.
struct PipelineHooks {
  std::function<void(PipelineStep)> beforeStep;
  std::function<void(PipelineStep, LogicalResult)> afterStep;
};

/// Configure the pass manager with the optimization passes.
LogicalResult
buildPassManager(ModuleOp module, MLIRContext &context,
                 PipelineStep stopAt = PipelineStep::Complete,
                 PipelineStep startFrom = PipelineStep::RaiseMemRefDimensionality,
                 std::unique_ptr<arts::AnalysisManager> *outAM = nullptr,
                 PipelineHooks *hooks = nullptr) {
  int startIndex = pipelineStepIndex(startFrom);
  int stopIndex = pipelineStepIndex(stopAt);
  if (startIndex < 0 || stopIndex < 0) {
    ARTS_ERROR("Invalid pipeline selection: --start-from="
               << pipelineStepName(startFrom)
               << ", --pipeline=" << pipelineStepName(stopAt));
    return failure();
  }
  if (stopAt != PipelineStep::Complete && startIndex > stopIndex) {
    ARTS_ERROR("Invalid pipeline range: --start-from="
               << pipelineStepName(startFrom)
               << " is after --pipeline=" << pipelineStepName(stopAt));
    return failure();
  }

  /// Create module-level analysis manager for caching across functions
  arts::PartitionFallback partitionFallback =
      (PartitionFallbackMode == PartitionFallback::Fine)
          ? arts::PartitionFallback::FineGrained
          : arts::PartitionFallback::Coarse;
  std::unique_ptr<arts::AnalysisManager> AM =
      std::make_unique<arts::AnalysisManager>(module, ArtsConfig, MetadataFile,
                                              partitionFallback);

  auto &machine = AM->getAbstractMachine();
  if (!machine.hasConfigFile() || !machine.isValid()) {
    ARTS_ERROR("Invalid ARTS configuration. Provide a valid --arts-config path "
               "or place a valid arts.cfg in the working directory.");
    return failure();
  }

  /// Embed config file contents into the module so generated binaries are
  /// self-contained — no external config file needed at runtime.
  if (machine.hasConfigFile() && !machine.getConfigPath().empty()) {
    auto configContents = llvm::MemoryBuffer::getFile(machine.getConfigPath());
    if (configContents)
      arts::setRuntimeConfigData(module, (*configContents)->getBuffer());
    else
      arts::setRuntimeConfigPath(module, machine.getConfigPath());
  }
  arts::setRuntimeTotalWorkers(module, machine.getRuntimeTotalWorkers());
  arts::setRuntimeTotalNodes(module, machine.getNodeCount());

  if (machine.getNodeCount() > 1 && !DistributedDb)
    llvm::errs() << "NOTE: Multi-node execution without --distributed-db: "
                    "all DBs will be created on their origin node.\n";

  /// Load metadata from JSON file
  (void)AM->getMetadataManager();

  struct StepSpec {
    PipelineStep step;
    const char *errorMessage;
    std::function<void(PassManager &)> setup;
  };

  auto runStep = [&](const StepSpec &spec) -> LogicalResult {
    if (hooks && hooks->beforeStep)
      hooks->beforeStep(spec.step);

    if (spec.step == PipelineStep::PreLowering && outAM)
      AM->captureDiagnostics();

    PassManager pm(&context);
    spec.setup(pm);
    auto result = pm.run(module);

    if (hooks && hooks->afterStep)
      hooks->afterStep(spec.step, result);

    if (failed(result)) {
      ARTS_ERROR(spec.errorMessage);
      module->dump();
      return failure();
    }
    return success();
  };

  auto releaseAnalysisManager = [&]() {
    if (outAM)
      *outAM = std::move(AM);
  };

  const std::vector<StepSpec> runtimeSteps = {
      {PipelineStep::RaiseMemRefDimensionality, "Error when raising memref dimensionality",
       [&](PassManager &pm) { buildRaiseMemRefDimensionalityPipeline(pm); }},
      {PipelineStep::CollectMetadata, "Error when collecting metadata",
       [&](PassManager &pm) {
         bool shouldExport = (stopAt == PipelineStep::CollectMetadata);
         buildCollectMetadataPipeline(pm, outAM ? AM.get() : nullptr,
                                      shouldExport);
       }},
      {PipelineStep::InitialCleanup, "Error simplifying the IR",
       [&](PassManager &pm) {
         OpPassManager &optPM = pm.nest<func::FuncOp>();
         buildInitialCleanupPipeline(optPM);
       }},
      {PipelineStep::OpenMPToArts, "Error when converting OpenMP to ARTS",
       [&](PassManager &pm) { buildOpenMPToArtsPipeline(pm); }},
      {PipelineStep::PatternPipeline,
       "Error when applying the semantic pattern pipeline",
       [&](PassManager &pm) { buildPatternPipeline(pm, AM.get()); }},
      {PipelineStep::EdtTransforms, "Error when running EDT transformations",
       [&](PassManager &pm) { buildEdtTransformsPipeline(pm, AM.get()); }},
      {PipelineStep::CreateDbs, "Error when creating DBs",
       [&](PassManager &pm) { buildCreateDbsPipeline(pm, AM.get()); }},
      {PipelineStep::DbOpt, "Error when optimizing DBs",
       [&](PassManager &pm) { buildDbOptPipeline(pm, AM.get()); }},
      {PipelineStep::EdtOpt, "Error when optimizing EDTs",
       [&](PassManager &pm) { buildEdtOptPipeline(pm, AM.get()); }},
      {PipelineStep::Concurrency, "Error when running concurrency",
       [&](PassManager &pm) { buildConcurrencyPipeline(pm, AM.get()); }},
      {PipelineStep::EdtDistribution, "Error when running EDT distribution",
       [&](PassManager &pm) { buildEdtDistributionPipeline(pm, AM.get()); }},
      {PipelineStep::ConcurrencyOpt, "Error when optimizing concurrency",
       [&](PassManager &pm) { buildConcurrencyOptPipeline(pm, AM.get()); }},
      {PipelineStep::Epochs, "Error when creating and optimizing epochs",
       [&](PassManager &pm) {
         buildEpochsPipeline(pm, EpochFinishContinuation);
       }},
      {PipelineStep::PreLowering,
       "Error when pre-lowering DBs, EDTs, and Epochs",
       [&](PassManager &pm) {
         buildPreLoweringPipeline(pm, AM.get(), EpochFinishContinuation);
       }},
      {PipelineStep::ArtsToLLVM, "Error when converting ARTS to LLVM",
       [&](PassManager &pm) {
         buildArtsToLLVMPipeline(pm, Debug, DistributedDb, &machine);
       }},
  };

  auto findRuntimeStep = [&](PipelineStep step) -> const StepSpec * {
    for (const StepSpec &spec : runtimeSteps) {
      if (spec.step == step)
        return &spec;
    }
    return nullptr;
  };

  for (const auto &stepSpec : kPipelineStepSpecs) {
    if (!stepSpec.inPipelineSequence)
      continue;
    int currentIndex = pipelineStepIndex(stepSpec.step);
    if (currentIndex < startIndex)
      continue;
    const StepSpec *runtimeSpec = findRuntimeStep(stepSpec.step);
    if (!runtimeSpec) {
      ARTS_ERROR(
          "Missing runtime handler for pipeline step: " << stepSpec.token);
      return failure();
    }
    if (failed(runStep(*runtimeSpec)))
      return failure();
    if (stopAt == stepSpec.step) {
      releaseAnalysisManager();
      return success();
    }
  }

  /// Optimizations
  if (Opt) {
    PassManager pm(&context);
    OpPassManager &optPM = pm.nest<func::FuncOp>();
    buildAdditionalOptPipeline(optPM);

    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when running classical optimizations");
      module->dump();
      return failure();
    }
  }

  /// LLVM IR conversion
  if (EmitLLVM) {
    PassManager pm(&context);
    buildLLVMIREmissionPipeline(pm);
    if (failed(pm.run(module))) {
      ARTS_ERROR("Error when emitting LLVM IR");
      module->dump();
      return failure();
    }
  }

  /// Return analysis manager if requested
  releaseAnalysisManager();

  return success();
}

///===----------------------------------------------------------------------===///
/// Main Function
///===----------------------------------------------------------------------===///
int main(int argc, char **argv) {
  InitLLVM y(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "MLIR Optimization Driver\n");
  configureArtsDebugChannels(ArtsDebug);

  if (PrintPipelineTokensJSON) {
    printPipelineTokensAsJSON(llvm::outs());
    return 0;
  }
  if (PrintPipelineManifestJSON) {
    printPipelineManifestAsJSON(llvm::outs());
    return 0;
  }

  PipelineStep effectiveStopAt = Pipeline;
  if (CollectMetadataFlag)
    effectiveStopAt = PipelineStep::CollectMetadata;

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

  /// Set up optional pipeline hooks for diagnostics.
  PipelineHooks hooks;
  PipelineHooks *hooksPtr = nullptr;
  if (Diagnose) {
    hooks.afterStep = [](PipelineStep step, LogicalResult result) {
      llvm::errs() << "[carts-compile] Pipeline " << pipelineStepName(step)
                   << (succeeded(result) ? " completed" : " FAILED") << "\n";
    };
    hooksPtr = &hooks;
  }

  /// Run the pass pipeline once.
  std::unique_ptr<arts::AnalysisManager> AM;
  if (failed(buildPassManager(module.get(), context, effectiveStopAt, StartFrom,
                              Diagnose ? &AM : nullptr, hooksPtr))) {
    return 1;
  }

  /// Export diagnostics if requested
  if (Diagnose && AM) {
    bool includeAnalysis = shouldExportDetailedDiagnose(effectiveStopAt) &&
                           AM->hasCapturedDiagnostics();
    if (!DiagnoseOutput.empty()) {
      /// Export to file
      std::error_code EC;
      llvm::raw_fd_ostream outputFile(DiagnoseOutput, EC);
      if (EC) {
        ARTS_ERROR(
            "Could not open diagnostics output file: " << DiagnoseOutput);
        return 1;
      }
      AM->exportToJson(outputFile, includeAnalysis);
      outputFile.close();
    } else {
      /// Export to stdout
      AM->exportToJson(llvm::outs(), includeAnalysis);
    }
  }

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
