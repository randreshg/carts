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
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringExtras.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

using namespace llvm;
using namespace mlir;

ARTS_DEBUG_SETUP(carts_compile)

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
                                         cl::init(".carts-metadata.json"));

/// Compatibility flag used by carts_cli.py / benchmark scripts.
/// Equivalent to --pipeline=collect-metadata.
static cl::opt<bool> CollectMetadataFlag(
    "collect-metadata",
    cl::desc("Collect and export metadata, then stop the pipeline"),
    cl::init(false));

static cl::opt<uint64_t>
    ArtsIdStride("arts-id-stride",
                 cl::desc("Stride multiplier for arts ids (EDTs/DBs)"),
                 cl::init(1000));

static cl::opt<bool>
    Diagnose("diagnose",
             cl::desc("Export diagnostic information about compilation"),
             cl::init(false));

static cl::opt<std::string> DiagnoseOutput(
    "diagnose-output", cl::desc("Output file for diagnostic JSON export"),
    cl::value_desc("filename"), cl::init(".carts-diagnose.json"));

static cl::opt<std::string> ArtsOnly(
    "arts-only",
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
    cl::desc("Tile size for j-dimension in kernel transforms"), cl::init(64));

static cl::opt<int64_t> KernelTransformsMinTripCount(
    "loop-transforms-min-trip-count",
    cl::desc("Minimum constant trip count required to apply tiling"),
    cl::init(128));

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
    cl::init(false));

///===----------------------------------------------------------------------===///
/// Pipeline Stop Options
///===----------------------------------------------------------------------===///
enum class PipelineStage {
  CanonicalizeMemrefs,
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

static cl::opt<PipelineStage> Pipeline(
    "pipeline", cl::desc("Stop pipeline at specified step:"),
    cl::values(clEnumValN(PipelineStage::CanonicalizeMemrefs,
                          "canonicalize-memrefs",
                          "Stop after canonicalizing memrefs pass"),
               clEnumValN(PipelineStage::CollectMetadata, "collect-metadata",
                          "Stop after collecting metadata"),
               clEnumValN(PipelineStage::InitialCleanup, "initial-cleanup",
                          "Stop after initial cleanup and simplification"),
               clEnumValN(PipelineStage::OpenMPToArts, "openmp-to-arts",
                          "Stop after OpenMP to Arts conversion"),
               clEnumValN(PipelineStage::PatternPipeline, "pattern-pipeline",
                          "Stop after the dedicated semantic pattern pipeline"),
               clEnumValN(PipelineStage::EdtTransforms, "edt-transforms",
                          "Stop after EDT transformations"),
               clEnumValN(PipelineStage::CreateDbs, "create-dbs",
                          "Stop after Dbs creation"),
               clEnumValN(PipelineStage::DbOpt, "db-opt",
                          "Stop after Dbs optimization"),
               clEnumValN(PipelineStage::EdtOpt, "edt-opt",
                          "Stop after EDT optimizations"),
               clEnumValN(PipelineStage::Concurrency, "concurrency",
                          "Stop after concurrency"),
               clEnumValN(PipelineStage::EdtDistribution, "edt-distribution",
                          "Stop after EDT distribution and for lowering"),
               clEnumValN(PipelineStage::ConcurrencyOpt, "concurrency-opt",
                          "Stop after concurrency optimization"),
               clEnumValN(PipelineStage::Epochs, "epochs",
                          "Stop after Epochs creation"),
               clEnumValN(PipelineStage::PreLowering, "pre-lowering",
                          "Stop after pre-lowering transformations"),
               clEnumValN(PipelineStage::ArtsToLLVM, "arts-to-llvm",
                          "Stop after Arts to LLVM conversion"),
               clEnumValN(PipelineStage::Complete, "complete",
                          "Run complete pipeline (default)")),
    cl::init(PipelineStage::Complete));

static cl::opt<PipelineStage> StartFrom(
    "start-from", cl::desc("Resume pipeline from specified step:"),
    cl::values(clEnumValN(PipelineStage::CanonicalizeMemrefs,
                          "canonicalize-memrefs",
                          "Start from canonicalizing memrefs pass (default)"),
               clEnumValN(PipelineStage::CollectMetadata, "collect-metadata",
                          "Start from collecting metadata"),
               clEnumValN(PipelineStage::InitialCleanup, "initial-cleanup",
                          "Start from initial cleanup and simplification"),
               clEnumValN(PipelineStage::OpenMPToArts, "openmp-to-arts",
                          "Start from OpenMP to Arts conversion"),
               clEnumValN(PipelineStage::PatternPipeline, "pattern-pipeline",
                          "Start from the semantic pattern pipeline"),
               clEnumValN(PipelineStage::EdtTransforms, "edt-transforms",
                          "Start from EDT transformations"),
               clEnumValN(PipelineStage::CreateDbs, "create-dbs",
                          "Start from Dbs creation"),
               clEnumValN(PipelineStage::DbOpt, "db-opt",
                          "Start from Dbs optimization"),
               clEnumValN(PipelineStage::EdtOpt, "edt-opt",
                          "Start from EDT optimizations"),
               clEnumValN(PipelineStage::Concurrency, "concurrency",
                          "Start from concurrency"),
               clEnumValN(PipelineStage::EdtDistribution, "edt-distribution",
                          "Start from EDT distribution and for lowering"),
               clEnumValN(PipelineStage::ConcurrencyOpt, "concurrency-opt",
                          "Start from concurrency optimization"),
               clEnumValN(PipelineStage::Epochs, "epochs",
                          "Start from Epochs creation"),
               clEnumValN(PipelineStage::PreLowering, "pre-lowering",
                          "Start from pre-lowering transformations"),
               clEnumValN(PipelineStage::ArtsToLLVM, "arts-to-llvm",
                          "Start from Arts to LLVM conversion")),
    cl::init(PipelineStage::CanonicalizeMemrefs));

static cl::opt<bool> PrintPipelineTokensJSON(
    "print-pipeline-tokens-json",
    cl::desc("Print canonical pipeline step tokens as JSON and exit"),
    cl::init(false));

static cl::opt<bool> PrintPipelineManifestJSON(
    "print-pipeline-manifest-json",
    cl::desc("Print pipeline step/pass manifest as JSON and exit"),
    cl::init(false));

static const std::array<llvm::StringLiteral, 8> kCanonicalizeMemrefsPasses = {
    "LowerAffine(func)",      "CSE",         "Inliner", "ArtsInliner",
    "PolygeistCanonicalize",  "CanonicalizeMemrefs",
    "DeadCodeElimination",    "CSE"};
static const std::array<llvm::StringLiteral, 7> kCollectMetadataPasses = {
    "replaceAffineCFG(func)", "RaiseSCFToAffine(func)",
    "replaceAffineCFG(func)", "RaiseSCFToAffine(func)", "CSE(func)",
    "CollectMetadata", "VerifyMetadata (diagnose mode)"};
static const std::array<llvm::StringLiteral, 3> kInitialCleanupPasses = {
    "LowerAffine(func)", "CSE(func)", "PolygeistCanonicalizeFor(func)"};
static const std::array<llvm::StringLiteral, 3> kOpenMPToArtsPasses = {
    "ConvertOpenMPToArts", "DeadCodeElimination", "CSE"};
static const std::array<llvm::StringLiteral, 8> kPatternPipelinePasses = {
    "PatternDiscovery(seed)", "DepTransforms", "LoopNormalization",
    "StencilBoundaryPeeling", "LoopReordering", "PatternDiscovery(refine)",
    "KernelTransforms", "CSE"};
static const std::array<llvm::StringLiteral, 6> kEdtTransformsPasses = {
    "EdtStructuralOpt(runAnalysis=false)", "EdtICM", "DeadCodeElimination",
    "SymbolDCE", "CSE", "EdtPtrRematerialization"};
static const std::array<llvm::StringLiteral, 8> kCreateDbsPasses = {
    "DistributedHostLoopOutlining (conditional)", "CreateDbs",
    "PolygeistCanonicalize", "CSE", "SymbolDCE", "Mem2Reg",
    "PolygeistCanonicalize", "VerifyDbCreated"};
static const std::array<llvm::StringLiteral, 4> kDbOptPasses = {
    "DbModeTightening", "PolygeistCanonicalize", "CSE", "Mem2Reg"};
static const std::array<llvm::StringLiteral, 4> kEdtOptPasses = {
    "PolygeistCanonicalize", "EdtStructuralOpt(runAnalysis=true)", "LoopFusion",
    "CSE"};
static const std::array<llvm::StringLiteral, 4> kConcurrencyPasses = {
    "PolygeistCanonicalize", "Concurrency", "ForOpt",
    "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 3> kEdtDistributionPasses = {
    "EdtDistribution", "ForLowering", "VerifyForLowered"};
static const std::array<llvm::StringLiteral, 24> kConcurrencyOptPasses = {
    "EdtStructuralOpt(runAnalysis=false)", "DeadCodeElimination",
    "PolygeistCanonicalize", "CSE",
    "EdtStructuralOpt(runAnalysis=false)", "EpochOpt",
    "PolygeistCanonicalize", "CSE", "DbPartitioning",
    "DbDistributedOwnership (conditional)", "DbTransforms", "DbModeTightening",
    "EdtTransforms", "ContractValidation", "DbScratchElimination",
    "PolygeistCanonicalize", "CSE", "BlockLoopStripMining(func)", "Hoisting",
    "PolygeistCanonicalize", "CSE", "EdtAllocaSinking",
    "DeadCodeElimination", "Mem2Reg"};
static const std::array<llvm::StringLiteral, 4> kEpochsPasses = {
    "PolygeistCanonicalize", "CreateEpochs",
    "EpochContinuationPrep (conditional)", "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 22> kPreLoweringPasses = {
    "EdtAllocaSinking", "ParallelEdtLowering", "PolygeistCanonicalize", "CSE",
    "DbLowering", "PolygeistCanonicalize", "CSE", "EdtLowering",
    "PolygeistCanonicalize", "CSE", "LICM", "DataPtrHoisting",
    "PolygeistCanonicalize", "CSE", "ScalarReplacement",
    "PolygeistCanonicalize", "CSE", "EpochLowering",
    "LoweringContractCleanup", "PolygeistCanonicalize", "CSE",
    "VerifyPreLowered"};
static const std::array<llvm::StringLiteral, 9> kArtsToLLVMPasses = {
    "ConvertArtsToLLVM", "DataPtrHoisting", "PolygeistCanonicalize", "CSE",
    "Mem2Reg", "PolygeistCanonicalize", "ControlFlowSink",
    "PolygeistCanonicalize", "VerifyLowered"};

struct PipelineStageTokenSpec {
  PipelineStage stage;
  llvm::StringLiteral token;
  bool allowPipelineStop;
  bool allowStartFrom;
  bool inPipelineSequence;
  llvm::ArrayRef<llvm::StringLiteral> passes;
};

static const std::array<PipelineStageTokenSpec, 16> kPipelineStageSpecs = {{
    {PipelineStage::CanonicalizeMemrefs, "canonicalize-memrefs", true, true,
     true, kCanonicalizeMemrefsPasses},
    {PipelineStage::CollectMetadata, "collect-metadata", true, true, true,
     kCollectMetadataPasses},
    {PipelineStage::InitialCleanup, "initial-cleanup", true, true, true,
     kInitialCleanupPasses},
    {PipelineStage::OpenMPToArts, "openmp-to-arts", true, true, true,
     kOpenMPToArtsPasses},
    {PipelineStage::PatternPipeline, "pattern-pipeline", true, true, true,
     kPatternPipelinePasses},
    {PipelineStage::EdtTransforms, "edt-transforms", true, true, true,
     kEdtTransformsPasses},
    {PipelineStage::CreateDbs, "create-dbs", true, true, true, kCreateDbsPasses},
    {PipelineStage::DbOpt, "db-opt", true, true, true, kDbOptPasses},
    {PipelineStage::EdtOpt, "edt-opt", true, true, true, kEdtOptPasses},
    {PipelineStage::Concurrency, "concurrency", true, true, true,
     kConcurrencyPasses},
    {PipelineStage::EdtDistribution, "edt-distribution", true, true, true,
     kEdtDistributionPasses},
    {PipelineStage::ConcurrencyOpt, "concurrency-opt", true, true, true,
     kConcurrencyOptPasses},
    {PipelineStage::Epochs, "epochs", true, true, true, kEpochsPasses},
    {PipelineStage::PreLowering, "pre-lowering", true, true, true,
     kPreLoweringPasses},
    {PipelineStage::ArtsToLLVM, "arts-to-llvm", true, true, true,
     kArtsToLLVMPasses},
    {PipelineStage::Complete, "complete", true, false, false,
     llvm::ArrayRef<llvm::StringLiteral>()},
}};

static const PipelineStageTokenSpec *findPipelineStageSpec(PipelineStage stage) {
  for (const auto &spec : kPipelineStageSpecs) {
    if (spec.stage == stage)
      return &spec;
  }
  return nullptr;
}

static int pipelineStageIndex(PipelineStage stage) {
  for (size_t i = 0; i < kPipelineStageSpecs.size(); ++i) {
    if (kPipelineStageSpecs[i].stage == stage)
      return static_cast<int>(i);
  }
  return -1;
}

static llvm::StringRef pipelineStageName(PipelineStage stage) {
  if (const auto *spec = findPipelineStageSpec(stage))
    return spec->token;
  return "unknown";
}

template <typename Predicate>
static void printStageTokenArray(llvm::raw_ostream &os, llvm::StringRef key,
                                 Predicate include) {
  os << "  \"" << key << "\": [";
  bool first = true;
  for (const auto &spec : kPipelineStageSpecs) {
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

static void printStageTokensAsJSON(llvm::raw_ostream &os) {
  os << "{\n";
  printStageTokenArray(os, "pipeline",
                       [](const PipelineStageTokenSpec &spec) {
                         return spec.allowPipelineStop;
                       });
  os << ",\n";
  printStageTokenArray(os, "start_from",
                       [](const PipelineStageTokenSpec &spec) {
                         return spec.allowStartFrom;
                       });
  os << ",\n";
  printStageTokenArray(os, "pipeline_sequence",
                       [](const PipelineStageTokenSpec &spec) {
                         return spec.inPipelineSequence;
                       });
  os << ",\n";
  os << "  \"aliases\": {}\n";
  os << "}\n";
}

static void printPipelineManifestAsJSON(llvm::raw_ostream &os) {
  os << "{\n";
  printStageTokenArray(os, "pipeline",
                       [](const PipelineStageTokenSpec &spec) {
                         return spec.allowPipelineStop;
                       });
  os << ",\n";
  printStageTokenArray(os, "start_from",
                       [](const PipelineStageTokenSpec &spec) {
                         return spec.allowStartFrom;
                       });
  os << ",\n";
  printStageTokenArray(os, "pipeline_sequence",
                       [](const PipelineStageTokenSpec &spec) {
                         return spec.inPipelineSequence;
                       });
  os << ",\n";
  os << "  \"aliases\": {},\n";
  os << "  \"pipeline_steps\": [\n";
  bool firstStage = true;
  for (const auto &spec : kPipelineStageSpecs) {
    if (!spec.inPipelineSequence)
      continue;
    if (!firstStage)
      os << ",\n";
    firstStage = false;
    os << "    {\"name\": \"" << spec.token << "\", \"passes\": ";
    printStringArray(os, spec.passes);
    os << "}";
  }
  os << "\n  ]\n";
  os << "}\n";
}

static void configureArtsOnlyDebugChannels(llvm::StringRef channels) {
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

static bool shouldExportDetailedDiagnose(PipelineStage stopAt) {
  if (stopAt == PipelineStage::Complete)
    return true;
  int stopIndex = pipelineStageIndex(stopAt);
  int preLoweringIndex = pipelineStageIndex(PipelineStage::PreLowering);
  return stopIndex >= 0 && preLoweringIndex >= 0 && stopIndex >= preLoweringIndex;
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

/// TODO(PERF): PolygeistCanonicalizePass is added 26 times; audit which
/// invocations are needed — when IR is already canonical, each is a wasted
/// walk.
/// TODO(PERF): CSEPass is added 24 times; audit which invocations are needed.

///===----------------------------------------------------------------------===///
/// Pipeline Builders
///===----------------------------------------------------------------------===///
/// Each build*Pipeline function populates a PassManager with the passes for
/// one logical compilation stage. There is a 1:1 mapping between
/// PipelineStage enum values and these builders.

/// Inliner and canonicalize memrefs pass.
void buildCanonicalizeMemrefsPipeline(PassManager &pm) {
  OpPassManager &optPM = pm.nest<func::FuncOp>();
  optPM.addPass(createLowerAffinePass());
  pm.addPass(createCSEPass());
  pm.addPass(createInlinerPass());
  pm.addPass(arts::createArtsInlinerPass());
  /// pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  /// pm.addPass(createCSEPass());

  pm.addPass(arts::createCanonicalizeMemrefsPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createCSEPass());
}

/// Metadata collection pass.
void buildCollectMetadataPipeline(PassManager &pm, arts::AnalysisManager *AM = nullptr,
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
void buildPatternDiscoveryPipeline(PassManager &pm, arts::AnalysisManager *AM) {
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
void buildPreCreateDbsBridgingPipeline(PassManager &pm, arts::AnalysisManager *AM) {
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
  if (enableDistributedHostLoopOutlining)
    pm.addPass(arts::createDistributedHostLoopOutliningPass(AM));
  pm.addPass(createCSEPass());
}

/// DB creation pass.
void buildCreateDbsPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  buildPreCreateDbsBridgingPipeline(pm, AM);
  pm.addPass(arts::createCreateDbsPass(AM));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createVerifyDbCreatedPass());
}

/// DB creation and optimization passes.
void buildDbOptPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(arts::createDbModeTighteningPass(AM));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
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
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  /// Re-run structural opts after cleanup to catch newly exposed degenerate
  /// EDTs before epoch shaping.
  /// TODO(PERF): EdtStructuralOptPass runs 4 times; evaluate if second call
  /// needed.
  pm.addPass(arts::createEdtStructuralOptPass(AM, /*runAnalysis*/ false));
  pm.addPass(arts::createEpochOptPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
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
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addNestedPass<func::FuncOp>(arts::createBlockLoopStripMiningPass());
  pm.addPass(arts::createHoistingPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
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
void buildPreLoweringPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  /// TODO(PERF): EdtAllocaSinkingPass runs twice (buildConcurrencyOptPipeline and
  /// here).
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createParallelEdtLoweringPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createDbLoweringPass(ArtsIdStride));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createEdtLoweringPass(AM, ArtsIdStride));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(createLoopInvariantCodeMotionPass());
  /// Hoist loop-invariant DB/dep pointer loads before scalar replacement;
  /// buildArtsToLLVMPipeline runs hoisting again after Arts->LLVM materializes new
  /// loads.
  pm.addPass(arts::createDataPtrHoistingPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createScalarReplacementPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createEpochLoweringPass());
  pm.addPass(arts::createLoweringContractCleanupPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createVerifyPreLoweredPass());
}

/// ARTS to LLVM conversion passes.
void buildArtsToLLVMPipeline(PassManager &pm, bool debug, bool distributedInitPerWorker,
                     const arts::AbstractMachine *machine) {
  pm.addPass(arts::createConvertArtsToLLVMPass(debug, distributedInitPerWorker,
                                               machine));
  /// Hoist loop-invariant loads after Arts->LLVM lowering for
  /// vectorization/LICM.
  pm.addPass(arts::createDataPtrHoistingPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
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
  std::function<void(PipelineStage)> beforePhase;
  std::function<void(PipelineStage, LogicalResult)> afterPhase;
};

/// Configure the pass manager with the optimization passes.
LogicalResult
buildPassManager(ModuleOp module, MLIRContext &context,
                 PipelineStage stopAt = PipelineStage::Complete,
                 PipelineStage startFrom = PipelineStage::CanonicalizeMemrefs,
                 std::unique_ptr<arts::AnalysisManager> *outAM = nullptr,
                 PipelineHooks *hooks = nullptr) {
  int startIndex = pipelineStageIndex(startFrom);
  int stopIndex = pipelineStageIndex(stopAt);
  if (startIndex < 0 || stopIndex < 0) {
    ARTS_ERROR("Invalid pipeline selection: --start-from="
               << pipelineStageName(startFrom) << ", --pipeline="
               << pipelineStageName(stopAt));
    return failure();
  }
  if (stopAt != PipelineStage::Complete && startIndex > stopIndex) {
    ARTS_ERROR("Invalid pipeline range: --start-from="
               << pipelineStageName(startFrom) << " is after --pipeline="
               << pipelineStageName(stopAt));
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

  struct StageSpec {
    PipelineStage stage;
    const char *errorMessage;
    std::function<void(PassManager &)> setup;
  };

  auto runStage = [&](const StageSpec &spec) -> LogicalResult {
    if (hooks && hooks->beforePhase)
      hooks->beforePhase(spec.stage);

    if (spec.stage == PipelineStage::PreLowering && outAM)
      AM->captureDiagnostics();

    PassManager pm(&context);
    spec.setup(pm);
    auto result = pm.run(module);

    if (hooks && hooks->afterPhase)
      hooks->afterPhase(spec.stage, result);

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

  const std::vector<StageSpec> runtimeStages = {
      {PipelineStage::CanonicalizeMemrefs, "Error when canonicalizing memrefs",
       [&](PassManager &pm) { buildCanonicalizeMemrefsPipeline(pm); }},
      {PipelineStage::CollectMetadata, "Error when collecting metadata",
       [&](PassManager &pm) {
         bool shouldExport = (stopAt == PipelineStage::CollectMetadata);
         buildCollectMetadataPipeline(pm, outAM ? AM.get() : nullptr, shouldExport);
       }},
      {PipelineStage::InitialCleanup, "Error simplifying the IR",
       [&](PassManager &pm) {
         OpPassManager &optPM = pm.nest<func::FuncOp>();
         buildInitialCleanupPipeline(optPM);
       }},
      {PipelineStage::OpenMPToArts, "Error when converting OpenMP to ARTS",
       [&](PassManager &pm) { buildOpenMPToArtsPipeline(pm); }},
      {PipelineStage::PatternPipeline,
       "Error when applying the semantic pattern pipeline",
       [&](PassManager &pm) { buildPatternDiscoveryPipeline(pm, AM.get()); }},
      {PipelineStage::EdtTransforms, "Error when running EDT transformations",
       [&](PassManager &pm) { buildEdtTransformsPipeline(pm, AM.get()); }},
      {PipelineStage::CreateDbs, "Error when creating Dbs",
       [&](PassManager &pm) { buildCreateDbsPipeline(pm, AM.get()); }},
      {PipelineStage::DbOpt, "Error when optimizing Dbs",
       [&](PassManager &pm) { buildDbOptPipeline(pm, AM.get()); }},
      {PipelineStage::EdtOpt, "Error when optimizing EDTs",
       [&](PassManager &pm) { buildEdtOptPipeline(pm, AM.get()); }},
      {PipelineStage::Concurrency, "Error when running concurrency",
       [&](PassManager &pm) { buildConcurrencyPipeline(pm, AM.get()); }},
      {PipelineStage::EdtDistribution, "Error when running EDT distribution",
       [&](PassManager &pm) { buildEdtDistributionPipeline(pm, AM.get()); }},
      {PipelineStage::ConcurrencyOpt, "Error when optimizing concurrency",
       [&](PassManager &pm) { buildConcurrencyOptPipeline(pm, AM.get()); }},
      {PipelineStage::Epochs, "Error when creating and optimizing epochs",
       [&](PassManager &pm) { buildEpochsPipeline(pm, EpochFinishContinuation); }},
      {PipelineStage::PreLowering,
       "Error when pre-lowering DBs, EDTs, and Epochs",
       [&](PassManager &pm) { buildPreLoweringPipeline(pm, AM.get()); }},
      {PipelineStage::ArtsToLLVM, "Error when converting ARTS to LLVM",
       [&](PassManager &pm) {
         buildArtsToLLVMPipeline(pm, Debug, DistributedDb, &machine);
       }},
  };

  auto findRuntimeStage = [&](PipelineStage stage) -> const StageSpec * {
    for (const StageSpec &spec : runtimeStages) {
      if (spec.stage == stage)
        return &spec;
    }
    return nullptr;
  };

  for (const auto &stageSpec : kPipelineStageSpecs) {
    if (!stageSpec.inPipelineSequence)
      continue;
    int currentIndex = pipelineStageIndex(stageSpec.stage);
    if (currentIndex < startIndex)
      continue;
    const StageSpec *runtimeSpec = findRuntimeStage(stageSpec.stage);
    if (!runtimeSpec) {
      ARTS_ERROR("Missing runtime handler for pipeline step: "
                 << stageSpec.token);
      return failure();
    }
    if (failed(runStage(*runtimeSpec)))
      return failure();
    if (stopAt == stageSpec.stage) {
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
  configureArtsOnlyDebugChannels(ArtsOnly);

  if (PrintPipelineTokensJSON) {
    printStageTokensAsJSON(llvm::outs());
    return 0;
  }
  if (PrintPipelineManifestJSON) {
    printPipelineManifestAsJSON(llvm::outs());
    return 0;
  }

  PipelineStage effectiveStopAt = Pipeline;
  if (CollectMetadataFlag)
    effectiveStopAt = PipelineStage::CollectMetadata;

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
    hooks.afterPhase = [](PipelineStage stage, LogicalResult result) {
      llvm::errs() << "[carts-compile] Pipeline " << pipelineStageName(stage)
                   << (succeeded(result) ? " completed" : " FAILED") << "\n";
    };
    hooksPtr = &hooks;
  }

  /// Run the pass pipeline once.
  std::unique_ptr<arts::AnalysisManager> AM;
  if (failed(buildPassManager(module.get(), context, effectiveStopAt,
                              StartFrom, Diagnose ? &AM : nullptr,
                              hooksPtr))) {
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
