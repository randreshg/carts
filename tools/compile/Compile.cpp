///==========================================================================///
/// File: Compile.cpp
/// Main entry point for the CARTS compilation pipeline tool.
///==========================================================================///

#include "mlir/Conversion/Passes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/Transforms/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Arith/Transforms/Passes.h"
#include "mlir/Dialect/Async/IR/Async.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/Extensions/InlinerExtension.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "mlir/Dialect/LLVMIR/ROCDLDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
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
#include "mlir/Support/Timing.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/Passes.h"
#include "polygeist/Dialect.h"
#include "polygeist/Passes/Passes.h"

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/dialect/rt/IR/RtDialect.h"
#include "arts/passes/Passes.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/PassInstrumentation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace llvm;
using namespace mlir;
using mlir::arts::debugStream;

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

static cl::opt<bool> VerifyMetadataIntegrityInput(
    "verify-metadata-integrity",
    cl::desc("Run metadata integrity verification on the current input module "
             "and stop"),
    cl::init(false));

static cl::opt<std::string> DiagnoseOutput(
    "diagnose-output", cl::desc("Output file for diagnostic JSON export"),
    cl::value_desc("filename"), cl::init(kDefaultDiagnoseOutput));

static cl::opt<bool>
    PassTiming("pass-timing",
               cl::desc("Print per-pass timing report after compilation"),
               cl::init(false));

static cl::opt<std::string>
    PassTimingOutput("pass-timing-output",
                     cl::desc("Export per-pass timing data as JSON to a file"),
                     cl::value_desc("filename"), cl::init(""));

static cl::opt<std::string> ArtsDebug(
    "arts-debug",
    cl::desc("Enable ARTS_INFO/ARTS_DEBUG channels (comma-separated)"),
    cl::value_desc("debug_types"), cl::init(""));

static cl::opt<bool> RuntimeStaticWorkers(
    "runtime-static-workers",
    cl::desc("Fold runtime_query<total_workers> to the configured worker count "
             "when the module embeds a valid ARTS config"),
    cl::init(false));

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
    cl::desc("Enable epoch finish-continuation lowering (default: on; replace "
             "blocking epoch waits with ARTS-native finish-EDT continuation "
             "scheduling for eligible patterns)"),
    cl::init(true));

///===----------------------------------------------------------------------===///
/// Pipeline Stop Options
///===----------------------------------------------------------------------===///
enum class StageId {
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
  PostDistributionCleanup,
  DbPartitioning,
  PostDbRefinement,
  LateConcurrencyCleanup,
  Epochs,
  PreLowering,
  ArtsToLLVM,
  PostO3Opt,
  LLVMIREmission
};

enum class StageKind { Core, Epilogue };

struct StageExecutionContext {
  ModuleOp module;
  MLIRContext &context;
  arts::AnalysisManager *analysisManager = nullptr;
  const arts::AbstractMachine *machine = nullptr;
  bool stopAfterStage = false;
  bool runAdditionalOpt = false;
  bool emitLLVM = false;
};

using StageBuilderFn = void (*)(PassManager &, const StageExecutionContext &);
using StageEnabledFn = bool (*)(const StageExecutionContext &);

struct StageDescriptor {
  StageId id;
  llvm::StringLiteral token;
  llvm::ArrayRef<llvm::StringLiteral> aliases;
  StageKind kind;
  bool allowPipelineStop;
  bool allowStartFrom;
  bool captureDiagnosticsBefore;
  llvm::StringLiteral errorMessage;
  llvm::ArrayRef<llvm::StringLiteral> passes;
  StageBuilderFn build;
  StageEnabledFn enabled;
  /// Stage tokens that must run before this stage. Used for --start-from
  /// validation and pipeline ordering verification.
  llvm::ArrayRef<llvm::StringLiteral> dependsOn;
};

static constexpr llvm::StringLiteral kCompletePipelineToken = "complete";
static constexpr llvm::StringLiteral kPostO3OptToken = "post-o3-opt";
static constexpr llvm::StringLiteral kLLVMIREmissionToken = "llvm-ir-emission";

static cl::opt<std::string> Pipeline(
    "pipeline",
    cl::desc("Stop pipeline at specified stage token "
             "(use --print-pipeline-manifest-json to inspect valid tokens)"),
    cl::value_desc("stage"), cl::init("complete"));

static cl::opt<std::string> StartFrom(
    "start-from",
    cl::desc("Resume pipeline from specified stage token "
             "(use --print-pipeline-manifest-json to inspect valid tokens)"),
    cl::value_desc("stage"), cl::init("raise-memref-dimensionality"));

static cl::opt<bool> PrintPipelineManifestJSON(
    "print-pipeline-manifest-json",
    cl::desc("Print pipeline step/pass manifest as JSON and exit"),
    cl::init(false));

static const std::array<llvm::StringLiteral, 8>
    kRaiseMemRefDimensionalityPasses = {"LowerAffine(func)",
                                        "CSE",
                                        "ArtsInliner",
                                        "PolygeistCanonicalize",
                                        "RaiseMemRefDimensionality",
                                        "HandleDeps",
                                        "DeadCodeElimination",
                                        "CSE"};
static const std::array<llvm::StringLiteral, 8> kCollectMetadataPasses = {
    "replaceAffineCFG(func)",
    "RaiseSCFToAffine(func)",
    "replaceAffineCFG(func)",
    "RaiseSCFToAffine(func)",
    "CSE(func)",
    "CollectMetadata",
    "VerifyMetadata (diagnose mode)",
    "VerifyMetadataIntegrity (diagnose mode)"};
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
static const std::array<llvm::StringLiteral, 5> kEdtDistributionPasses = {
    "StructuredKernelPlan", "EdtDistribution", "EdtOrchestrationOpt",
    "ForLowering", "VerifyForLowered"};
static const std::array<llvm::StringLiteral, 8> kPostDistributionCleanupPasses =
    {"EdtStructuralOpt(runAnalysis=false)",
     "DeadCodeElimination",
     "PolygeistCanonicalize",
     "CSE",
     "EdtStructuralOpt(runAnalysis=false)",
     "EpochOpt",
     "PolygeistCanonicalize",
     "CSE"};
static const std::array<llvm::StringLiteral, 3> kDbPartitioningPasses = {
    "DbPartitioning", "DbDistributedOwnership (conditional)", "DbTransforms"};
static const std::array<llvm::StringLiteral, 7> kPostDbRefinementPasses = {
    "DbModeTightening",
    "EdtTransforms",
    "DbTransforms",
    "ContractValidation",
    "DbScratchElimination",
    "PolygeistCanonicalize",
    "CSE"};
static const std::array<llvm::StringLiteral, 7> kLateConcurrencyCleanupPasses =
    {"BlockLoopStripMining(func)",
     "Hoisting",
     "PolygeistCanonicalize",
     "CSE",
     "EdtAllocaSinking",
     "DeadCodeElimination",
     "Mem2Reg"};
static const std::array<llvm::StringLiteral, 5> kEpochsPasses = {
    "PolygeistCanonicalize", "CreateEpochs", "PersistentStructuredRegion",
    "EpochOpt[scheduling] (conditional)", "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 22> kPreLoweringPasses = {
    "EdtAllocaSinking",
    "ParallelEdtLowering",
    "EpochOpt[scheduling] (conditional)",
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
    "PolygeistCanonicalize",
    "CSE",
    "VerifyPreLowered"};
static const std::array<llvm::StringLiteral, 12> kArtsToLLVMPasses = {
    "ConvertArtsToLLVM",
    "LoweringContractCleanup",
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

static const std::array<llvm::StringLiteral, 6> kPostO3OptPasses = {
    "PolygeistCanonicalize",
    "ControlFlowSink",
    "PolygeistCanonicalize",
    "LICM",
    "CSE",
    "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 10> kLLVMIREmissionPasses = {
    "CSE",
    "PolygeistCanonicalize",
    "ArithExpandOps",
    "ConvertPolygeistToLLVM",
    "ConvertIndexToLLVM",
    "ReconcileUnrealizedCasts",
    "AliasScopeGen",
    "LoopVectorizationHints",
    "PolygeistCanonicalize",
    "CSE"};

static ArrayRef<StageDescriptor> getStageRegistry();

static const StageDescriptor *findStageById(StageId id) {
  for (const auto &stage : getStageRegistry()) {
    if (stage.id == id)
      return &stage;
  }
  return nullptr;
}

static const StageDescriptor *findStageByToken(StringRef token) {
  for (const auto &stage : getStageRegistry()) {
    if (stage.token == token)
      return &stage;
    for (StringRef alias : stage.aliases) {
      if (alias == token)
        return &stage;
    }
  }
  return nullptr;
}

static int stageIndex(StageId id, StageKind kind) {
  int index = 0;
  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != kind)
      continue;
    if (stage.id == id)
      return index;
    ++index;
  }
  return -1;
}

static llvm::StringRef stageName(StageId id) {
  if (const auto *stage = findStageById(id))
    return stage->token;
  return "unknown";
}

static bool shouldIncludeStageInJSON(const StageDescriptor &stage,
                                     llvm::StringRef key) {
  if (key == "pipeline")
    return stage.allowPipelineStop;
  if (key == "start_from")
    return stage.allowStartFrom;
  return stage.kind == StageKind::Core;
}

template <typename Predicate>
static void printPipelineTokenArray(llvm::raw_ostream &os, llvm::StringRef key,
                                    Predicate include) {
  os << "  \"" << key << "\": [";
  bool first = true;
  for (const auto &stage : getStageRegistry()) {
    if (!include(stage))
      continue;
    if (!first)
      os << ", ";
    first = false;
    os << "\"" << stage.token << "\"";
  }
  if (key == "pipeline") {
    if (!first)
      os << ", ";
    os << "\"" << kCompletePipelineToken << "\"";
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

static void printPipelineManifestAsJSON(llvm::raw_ostream &os) {
  os << "{\n";
  printPipelineTokenArray(os, "pipeline", [](const StageDescriptor &stage) {
    return shouldIncludeStageInJSON(stage, "pipeline");
  });
  os << ",\n";
  printPipelineTokenArray(os, "start_from", [](const StageDescriptor &stage) {
    return shouldIncludeStageInJSON(stage, "start_from");
  });
  os << ",\n";
  printPipelineTokenArray(
      os, "pipeline_sequence", [](const StageDescriptor &stage) {
        return shouldIncludeStageInJSON(stage, "pipeline_sequence");
      });
  os << ",\n";
  os << "  \"pipeline_steps\": [\n";
  bool firstStep = true;
  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Core)
      continue;
    if (!firstStep)
      os << ",\n";
    firstStep = false;
    os << "    {\"name\": \"" << stage.token << "\", \"passes\": ";
    printStringArray(os, stage.passes);
    os << ", \"dependsOn\": ";
    printStringArray(os, stage.dependsOn);
    os << "}";
  }
  os << "\n  ],\n";
  os << "  \"epilogue_steps\": [\n";
  bool firstEpilogue = true;
  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Epilogue)
      continue;
    if (!firstEpilogue)
      os << ",\n";
    firstEpilogue = false;
    os << "    {\"name\": \"" << stage.token << "\", \"passes\": ";
    printStringArray(os, stage.passes);
    os << ", \"dependsOn\": ";
    printStringArray(os, stage.dependsOn);
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

  static llvm::SmallVector<std::string, 8> ownedChannels;
  ownedChannels.clear();
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

static bool shouldExportDetailedDiagnose(std::optional<StageId> stopAt) {
  if (!stopAt)
    return true;
  int stopIndex = stageIndex(*stopAt, StageKind::Core);
  int preLoweringIndex = stageIndex(StageId::PreLowering, StageKind::Core);
  return stopIndex >= 0 && preLoweringIndex >= 0 &&
         stopIndex >= preLoweringIndex;
}

static LogicalResult
configurePassManager(PassManager &pm,
                     arts::PassTimingData *timingData = nullptr) {
  pm.enableVerifier(true);
  if (failed(applyPassManagerCLOptions(pm)))
    return failure();
  applyDefaultTimingPassManagerCLOptions(pm);
  if (timingData)
    pm.addInstrumentation(
        std::make_unique<arts::CartsPassInstrumentation>(timingData));
  return success();
}

///===----------------------------------------------------------------------===///
/// Helper Functions for Initialization and Pass Setup
///===----------------------------------------------------------------------===///
/// Register standard MLIR dialects, passes, and translations.
void registerDialects(DialectRegistry &registry) {
  registry.insert<polygeist::PolygeistDialect, arts::ArtsDialect,
                  arts::rt::ArtsRtDialect>();
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
  context.getOrLoadDialect<mlir::omp::OpenMPDialect>();
  context.getOrLoadDialect<math::MathDialect>();
  context.getOrLoadDialect<memref::MemRefDialect>();
  context.getOrLoadDialect<linalg::LinalgDialect>();
  context.getOrLoadDialect<polygeist::PolygeistDialect>();
  context.getOrLoadDialect<arts::ArtsDialect>();
  context.getOrLoadDialect<arts::rt::ArtsRtDialect>();
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
  /// Stage contract: normalize affine memory/control ops before the module pass
  /// runs so RaiseMemRefDimensionality only needs to reason about the
  /// memref+SCF form produced by the frontend/inliner pipeline.
  optPM.addPass(createLowerAffinePass());
  pm.addPass(createCSEPass());
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
  if (AM) {
    pm.addPass(arts::createVerifyMetadataPass(AM));
    pm.addPass(arts::createVerifyMetadataIntegrityPass(AM));
  }
}

/// Initial cleanup and simplification passes.
void buildInitialCleanupPipeline(OpPassManager &optPM) {
  optPM.addPass(createLowerAffinePass());
  optPM.addPass(createCSEPass());
  optPM.addPass(polygeist::createCanonicalizeForPass());
}

/// OpenMP to ARTS conversion pass.
void buildOpenMPToArtsPipeline(PassManager &pm,
                               arts::AnalysisManager *AM = nullptr) {
  pm.addPass(arts::createConvertOpenMPToArtsPass(AM));
  pm.addPass(arts::createDCEPass());
  pm.addPass(createCSEPass());
  pm.addPass(arts::createVerifyEdtCreatedPass());
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
  pm.addPass(arts::createDepTransformsPass(AM));
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
  pm.addPass(arts::createStructuredKernelPlanPass(AM));
  pm.addPass(arts::createEdtDistributionPass(AM));
  pm.addPass(arts::createEdtOrchestrationOptPass());
  pm.addPass(arts::createForLoweringPass(AM));
  pm.addPass(arts::createVerifyForLoweredPass());
}

/// Cleanup and re-shape distributed EDT structure after lowering task loops.
void buildPostDistributionCleanupPipeline(PassManager &pm,
                                          arts::AnalysisManager *AM) {
  pm.addPass(arts::createEdtStructuralOptPass(AM, /*runAnalysis*/ false));
  pm.addPass(arts::createDCEPass());
  addCanonicalizeAndCSE(pm);
  /// Re-run structural opts after cleanup to catch newly exposed degenerate
  /// EDTs before epoch shaping.
  /// TODO(PERF): EdtStructuralOptPass runs 4 times; evaluate if second call
  /// needed.
  pm.addPass(arts::createEdtStructuralOptPass(AM, /*runAnalysis*/ false));
  pm.addPass(arts::createEpochOptPass(AM));
  addCanonicalizeAndCSE(pm);
}

/// Partition DBs while high-level EDT/DB structure is still intact.
void buildDbPartitioningPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  pm.addPass(arts::createDbPartitioningPass(AM));
  /// Run distributed-ownership eligibility while EDT/DB ops are still in
  /// high-level form so analysis can reason about DbAcquire<->Edt users.
  if (DistributedDb)
    pm.addPass(arts::createDbDistributedOwnershipPass(AM));
  pm.addPass(arts::createDbTransformsPass(AM));
}

/// Tighten DB modes and persist post-partition refinement contracts.
void buildPostDbRefinementPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  /// DbModeTighteningPass performs local DB cleanup after mode adjustment,
  /// which can expose new zero-dependency or degenerate EDTs before epoch
  /// shaping. Mode tightening must run before EDT transforms so affinity and
  /// reduction analysis see accurate writer/reader modes.
  pm.addPass(arts::createDbModeTighteningPass(AM));
  pm.addPass(arts::createEdtTransformsPass(AM));
  /// Re-run DB transforms after EDT dependency pruning so cleanup-only
  /// acquires and now-unreachable DB roots are removed in the DB layer.
  pm.addPass(arts::createDbTransformsPass(AM));
  pm.addPass(arts::createContractValidationPass());
  pm.addPass(arts::createDbScratchEliminationPass());
  addCanonicalizeAndCSE(pm);
}

/// Apply late DB-aware loop cleanup and final stack/SSA simplification.
void buildLateConcurrencyCleanupPipeline(PassManager &pm) {
  pm.addNestedPass<func::FuncOp>(arts::createBlockLoopStripMiningPass());
  pm.addPass(arts::createHoistingPass());
  addCanonicalizeAndCSE(pm);
  /// TODO(PERF): EdtAllocaSinkingPass runs twice (here and pre-lowering).
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createMem2Reg());
}

/// Epoch creation passes.
void buildEpochsPipeline(PassManager &pm, arts::AnalysisManager *AM,
                         bool enableContinuation) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createCreateEpochsPass());
  pm.addPass(arts::createVerifyEpochCreatedPass());
  /// Gate proven regular epochs for persistent structured region lowering.
  pm.addPass(arts::createPersistentStructuredRegionPass(AM));
  /// Run EpochOpt with structural + scheduling optimizations on newly created
  /// epochs. Amortization is always enabled; continuation/CPS opts are gated.
  pm.addPass(arts::createEpochOptPass(AM,
                                      /*amortization=*/true,
                                      /*continuation=*/enableContinuation,
                                      /*cpsDriver=*/enableContinuation,
                                      /*cpsChain=*/enableContinuation));
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// Pre-lowering passes.
void buildPreLoweringPipeline(PassManager &pm, arts::AnalysisManager *AM,
                              bool enableContinuation) {
  /// TODO(PERF): EdtAllocaSinkingPass runs twice (late concurrency cleanup
  /// and here).
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createParallelEdtLoweringPass());
  if (enableContinuation) {
    /// Run scheduling-only EpochOpt for late epochs from ParallelEdtLowering.
    pm.addPass(arts::createEpochOptSchedulingPass(AM,
                                                  /*amortization=*/true,
                                                  /*continuation=*/true,
                                                  /*cpsDriver=*/true));
  }
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createDbLoweringPass(ArtsIdStride));
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createEdtLoweringPass(AM, ArtsIdStride));
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createVerifyEdtLoweredPass());
  pm.addPass(createLoopInvariantCodeMotionPass());
  /// Hoist loop-invariant DB/dep pointer loads before scalar replacement;
  /// buildArtsToLLVMPipeline runs hoisting again after Arts->LLVM materializes
  /// new loads.
  pm.addPass(arts::createDataPtrHoistingPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createScalarReplacementPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createEpochLoweringPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts::createVerifyEpochLoweredPass());
  pm.addPass(arts::createVerifyPreLoweredPass());
}

/// ARTS to LLVM conversion passes.
void buildArtsToLLVMPipeline(PassManager &pm, bool debug,
                             bool distributedInitPerWorker,
                             const arts::AbstractMachine *machine) {
  pm.addPass(arts::createConvertArtsToLLVMPass(debug, distributedInitPerWorker,
                                               machine));
  /// ConvertArtsToLLVM still consults lowering contracts for late dependency
  /// decisions (for example N-D stencil halo slices). Clean them up only after
  /// that conversion has consumed them.
  pm.addPass(arts::createLoweringContractCleanupPass());
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
  pm.addPass(arts::createVerifyDbLoweredPass());
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
  pm.addPass(createConvertIndexToLLVMPass());
  pm.addPass(createReconcileUnrealizedCastsPass());
  pm.addPass(arts::createAliasScopeGenPass());
  pm.addPass(arts::createLoopVectorizationHintsPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createCSEPass());
}

static bool isStageEnabledAlways(const StageExecutionContext &) { return true; }
static bool isStageEnabledWhenOptRequested(const StageExecutionContext &ctx) {
  return ctx.runAdditionalOpt;
}
static bool
isStageEnabledWhenEmitLLVMRequested(const StageExecutionContext &ctx) {
  return ctx.emitLLVM;
}

// --- Pipeline dependency declarations ---
static constexpr llvm::StringLiteral kDepRaiseMemref[] = {
    "raise-memref-dimensionality"};
static constexpr llvm::StringLiteral kDepCollectMetadata[] = {
    "collect-metadata"};
static constexpr llvm::StringLiteral kDepInitialCleanup[] = {"initial-cleanup"};
static constexpr llvm::StringLiteral kDepOpenMPToArts[] = {"openmp-to-arts"};
static constexpr llvm::StringLiteral kDepCreateDbs[] = {"create-dbs"};
static constexpr llvm::StringLiteral kDepConcurrency[] = {"concurrency"};
static constexpr llvm::StringLiteral kDepEdtDistribution[] = {
    "edt-distribution"};
static constexpr llvm::StringLiteral kDepDbPartitioning[] = {"db-partitioning"};
static constexpr llvm::StringLiteral kDepPostDbRefinement[] = {
    "post-db-refinement"};
static constexpr llvm::StringLiteral kDepPreLowering[] = {
    "epochs", "late-concurrency-cleanup"};
static constexpr llvm::StringLiteral kDepArtsToLLVM[] = {"pre-lowering"};

static ArrayRef<StageDescriptor> getStageRegistry() {
  static const std::array<StageDescriptor, 20> kStageRegistry = {{
      {StageId::RaiseMemRefDimensionality, "raise-memref-dimensionality",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when raising memref dimensionality",
       kRaiseMemRefDimensionalityPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildRaiseMemRefDimensionalityPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/llvm::ArrayRef<llvm::StringLiteral>()},
      {StageId::CollectMetadata, "collect-metadata",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when collecting metadata", kCollectMetadataPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildCollectMetadataPipeline(pm, ctx.analysisManager,
                                      ctx.stopAfterStage);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepRaiseMemref},
      {StageId::InitialCleanup, "initial-cleanup",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error simplifying the IR", kInitialCleanupPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         OpPassManager &optPM = pm.nest<func::FuncOp>();
         buildInitialCleanupPipeline(optPM);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepCollectMetadata},
      {StageId::OpenMPToArts, "openmp-to-arts",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when converting OpenMP to ARTS", kOpenMPToArtsPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildOpenMPToArtsPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepInitialCleanup},
      {StageId::PatternPipeline, "pattern-pipeline",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when applying the semantic pattern pipeline",
       kPatternPipelinePasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildPatternPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepOpenMPToArts},
      {StageId::EdtTransforms, "edt-transforms",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when running EDT transformations", kEdtTransformsPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildEdtTransformsPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepOpenMPToArts},
      {StageId::CreateDbs, "create-dbs", llvm::ArrayRef<llvm::StringLiteral>(),
       StageKind::Core, true, true, false, "Error when creating DBs",
       kCreateDbsPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildCreateDbsPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepOpenMPToArts},
      {StageId::DbOpt, "db-opt", llvm::ArrayRef<llvm::StringLiteral>(),
       StageKind::Core, true, true, false, "Error when optimizing DBs",
       kDbOptPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildDbOptPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepCreateDbs},
      {StageId::EdtOpt, "edt-opt", llvm::ArrayRef<llvm::StringLiteral>(),
       StageKind::Core, true, true, false, "Error when optimizing EDTs",
       kEdtOptPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildEdtOptPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepCreateDbs},
      {StageId::Concurrency, "concurrency",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when running concurrency", kConcurrencyPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildConcurrencyPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepCreateDbs},
      {StageId::EdtDistribution, "edt-distribution",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when running EDT distribution", kEdtDistributionPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildEdtDistributionPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepConcurrency},
      {StageId::PostDistributionCleanup, "post-distribution-cleanup",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when cleaning up post-distribution concurrency",
       kPostDistributionCleanupPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildPostDistributionCleanupPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepEdtDistribution},
      {StageId::DbPartitioning, "db-partitioning",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when partitioning DBs", kDbPartitioningPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildDbPartitioningPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepEdtDistribution},
      {StageId::PostDbRefinement, "post-db-refinement",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when refining post-partition DB contracts",
       kPostDbRefinementPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildPostDbRefinementPipeline(pm, ctx.analysisManager);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepDbPartitioning},
      {StageId::LateConcurrencyCleanup, "late-concurrency-cleanup",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when running late concurrency cleanup",
       kLateConcurrencyCleanupPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildLateConcurrencyCleanupPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepPostDbRefinement},
      {StageId::Epochs, "epochs", llvm::ArrayRef<llvm::StringLiteral>(),
       StageKind::Core, true, true, false,
       "Error when creating and optimizing epochs", kEpochsPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildEpochsPipeline(pm, ctx.analysisManager, EpochFinishContinuation);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepEdtDistribution},
      {StageId::PreLowering, "pre-lowering",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true, true,
       "Error when pre-lowering DBs, EDTs, and Epochs", kPreLoweringPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildPreLoweringPipeline(pm, ctx.analysisManager,
                                  EpochFinishContinuation);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepPreLowering},
      {StageId::ArtsToLLVM, "arts-to-llvm",
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Core, true, true,
       false, "Error when converting ARTS to LLVM", kArtsToLLVMPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildArtsToLLVMPipeline(pm, Debug, DistributedDb, ctx.machine);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepArtsToLLVM},
      {StageId::PostO3Opt, kPostO3OptToken,
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Epilogue, false, false,
       false, "Error when running classical optimizations", kPostO3OptPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         OpPassManager &optPM = pm.nest<func::FuncOp>();
         buildAdditionalOptPipeline(optPM);
       },
       isStageEnabledWhenOptRequested,
       /*dependsOn=*/kDepArtsToLLVM},
      {StageId::LLVMIREmission, kLLVMIREmissionToken,
       llvm::ArrayRef<llvm::StringLiteral>(), StageKind::Epilogue, false, false,
       false, "Error when emitting LLVM IR", kLLVMIREmissionPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildLLVMIREmissionPipeline(pm);
       },
       isStageEnabledWhenEmitLLVMRequested,
       /*dependsOn=*/kDepArtsToLLVM},
  }};
  return kStageRegistry;
}

/// Validate that no stage's dependsOn references a stage that comes after it
/// in the registry order. Call this during pipeline construction.
static bool validatePipelineDAG(ArrayRef<StageDescriptor> stages) {
  llvm::DenseMap<StringRef, unsigned> stageIndex;
  for (unsigned i = 0; i < stages.size(); ++i)
    stageIndex[stages[i].token] = i;

  for (unsigned i = 0; i < stages.size(); ++i) {
    for (StringRef dep : stages[i].dependsOn) {
      auto it = stageIndex.find(dep);
      if (it == stageIndex.end()) {
        llvm::errs() << "Pipeline error: stage '" << stages[i].token
                     << "' depends on unknown stage '" << dep << "'\n";
        return false;
      }
      if (it->second >= i) {
        llvm::errs() << "Pipeline error: stage '" << stages[i].token
                     << "' depends on '" << dep << "' which comes later\n";
        return false;
      }
    }
  }
  return true;
}

static FailureOr<StageId> resolveRequiredStageToken(StringRef token,
                                                    bool allowStartFrom) {
  if (const StageDescriptor *stage = findStageByToken(token)) {
    if (allowStartFrom && !stage->allowStartFrom)
      return failure();
    if (!allowStartFrom && !stage->allowPipelineStop)
      return failure();
    return stage->id;
  }
  return failure();
}

static FailureOr<std::optional<StageId>>
resolvePipelineStopToken(StringRef token) {
  if (token == kCompletePipelineToken)
    return std::optional<StageId>();
  FailureOr<StageId> resolved = resolveRequiredStageToken(token, false);
  if (failed(resolved))
    return failure();
  return std::optional<StageId>(*resolved);
}

static void emitAvailableStageTokens(bool startFrom) {
  ARTS_ERROR("Available " << (startFrom ? "start-from pipeline steps"
                                        : "pipeline steps")
                          << ":");
  for (const auto &stage : getStageRegistry()) {
    if (startFrom ? stage.allowStartFrom : stage.allowPipelineStop)
      ARTS_ERROR("- " << stage.token);
  }
  if (!startFrom)
    ARTS_ERROR("- " << kCompletePipelineToken);
}

static void emitUnknownStageTokenError(StringRef token, bool startFrom) {
  ARTS_ERROR(
      "Unknown " << (startFrom ? "start-from pipeline step" : "pipeline step")
                 << ": '" << token << "'");
  emitAvailableStageTokens(startFrom);
}

/// Hooks invoked around each stage for diagnostics or custom logging.
struct PipelineHooks {
  std::function<void(StageId)> beforeStep;
  std::function<void(StageId, LogicalResult)> afterStep;
};

/// Configure the pass manager with the optimization passes.
LogicalResult
buildPassManager(ModuleOp module, MLIRContext &context,
                 std::optional<StageId> stopAt,
                 StageId startFrom = StageId::RaiseMemRefDimensionality,
                 std::unique_ptr<arts::AnalysisManager> *outAM = nullptr,
                 PipelineHooks *hooks = nullptr) {
  assert(validatePipelineDAG(getStageRegistry()) &&
         "Pipeline dependency DAG is invalid");

  int startIndex = stageIndex(startFrom, StageKind::Core);
  int stopIndex = stopAt ? stageIndex(*stopAt, StageKind::Core)
                         : stageIndex(StageId::ArtsToLLVM, StageKind::Core);
  if (startIndex < 0 || stopIndex < 0) {
    ARTS_ERROR(
        "Invalid pipeline selection: --start-from="
        << stageName(startFrom) << ", --pipeline="
        << (stopAt ? stageName(*stopAt) : StringRef(kCompletePipelineToken)));
    return failure();
  }
  if (startIndex > stopIndex) {
    ARTS_ERROR(
        "Invalid pipeline range: --start-from="
        << stageName(startFrom) << " is after --pipeline="
        << (stopAt ? stageName(*stopAt) : StringRef(kCompletePipelineToken)));
    return failure();
  }

  /// Create module-level analysis manager for caching across functions
  std::unique_ptr<arts::AnalysisManager> AM =
      std::make_unique<arts::AnalysisManager>(module, ArtsConfig, MetadataFile);

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
  arts::setRuntimeStaticWorkers(module, RuntimeStaticWorkers);

  if (machine.getNodeCount() > 1 && !DistributedDb)
    ARTS_WARN("Multi-node execution without --distributed-db: all DBs will "
              "be created on their origin node");
  int collectMetadataIndex =
      stageIndex(StageId::CollectMetadata, StageKind::Core);
  if (startIndex > collectMetadataIndex)
    AM->syncMetadataManagerFromModule(/*allowJsonImportIfUninitialized=*/true);

  /// Create shared timing data for pass instrumentation.
  arts::PassTimingData timingData;
  arts::PassTimingData *timingDataPtr = PassTiming ? &timingData : nullptr;

  auto runStage = [&](const StageDescriptor &stage,
                      bool stopAfterStage) -> LogicalResult {
    if (hooks && hooks->beforeStep)
      hooks->beforeStep(stage.id);

    if (stage.captureDiagnosticsBefore && outAM)
      AM->captureDiagnostics();

    PassManager pm(&context);
    if (failed(configurePassManager(pm, timingDataPtr))) {
      ARTS_ERROR("Error configuring pass manager for pipeline step "
                 << stage.token);
      return failure();
    }
    StageExecutionContext stageContext{
        module, context, AM.get(), &machine, stopAfterStage, Opt, EmitLLVM};
    stage.build(pm, stageContext);
    auto result = pm.run(module);
    if (succeeded(result)) {
      /// Keep the shared metadata registry aligned with the current IR between
      /// stage boundaries. CollectMetadata builds its own metadata manager, and
      /// generic canonicalization passes may erase or replace ops without
      /// updating the cached registry.
      AM->syncMetadataManagerFromModule(
          /*allowJsonImportIfUninitialized=*/false);
    }

    if (hooks && hooks->afterStep)
      hooks->afterStep(stage.id, result);

    if (failed(result)) {
      ARTS_ERROR(stage.errorMessage);
      module->dump();
      return failure();
    }
    return success();
  };

  auto releaseAnalysisManager = [&]() {
    if (outAM)
      *outAM = std::move(AM);
  };

  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Core)
      continue;
    int currentIndex = stageIndex(stage.id, StageKind::Core);
    if (currentIndex < startIndex)
      continue;
    if (!stage.enabled(StageExecutionContext{module, context, AM.get(),
                                             &machine, false, Opt, EmitLLVM}))
      continue;
    bool stopHere = stopAt && *stopAt == stage.id;
    if (failed(runStage(stage, stopHere)))
      return failure();
    if (stopHere) {
      if (PassTiming)
        timingData.printTimingReport(llvm::errs());
      releaseAnalysisManager();
      return success();
    }
  }

  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Epilogue)
      continue;
    StageExecutionContext stageContext{module, context, AM.get(), &machine,
                                       false,  Opt,     EmitLLVM};
    if (!stage.enabled(stageContext))
      continue;
    if (failed(runStage(stage, /*stopAfterStage=*/false)))
      return failure();
  }

  /// Print pass timing report if enabled.
  if (PassTiming)
    timingData.printTimingReport(llvm::errs());
  if (!PassTimingOutput.empty()) {
    std::error_code EC;
    llvm::raw_fd_ostream timingFile(PassTimingOutput, EC);
    if (!EC) {
      timingData.exportTimingJson(timingFile);
      timingFile.close();
    } else {
      ARTS_WARN("Could not open pass timing output file: " << PassTimingOutput);
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
  registerPassManagerCLOptions();
  registerDefaultTimingManagerCLOptions();
  cl::ParseCommandLineOptions(argc, argv, "MLIR Optimization Driver\n");
  std::string effectiveArtsDebug = ArtsDebug;
  if (Diagnose) {
    if (!effectiveArtsDebug.empty())
      effectiveArtsDebug += ",";
    effectiveArtsDebug += "compile";
  }
  configureArtsDebugChannels(effectiveArtsDebug);

  if (PrintPipelineManifestJSON) {
    printPipelineManifestAsJSON(llvm::outs());
    return 0;
  }

  FailureOr<StageId> resolvedStartFrom =
      resolveRequiredStageToken(StartFrom.getValue(), /*allowStartFrom=*/true);
  if (failed(resolvedStartFrom)) {
    emitUnknownStageTokenError(StartFrom, /*startFrom=*/true);
    return 1;
  }

  std::string effectivePipelineToken = Pipeline;
  if (CollectMetadataFlag)
    effectivePipelineToken = std::string("collect-metadata");

  FailureOr<std::optional<StageId>> resolvedStopAt =
      resolvePipelineStopToken(effectivePipelineToken);
  if (failed(resolvedStopAt)) {
    emitUnknownStageTokenError(effectivePipelineToken, /*startFrom=*/false);
    return 1;
  }

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
  if (VerifyMetadataIntegrityInput) {
    auto verifyAM = std::make_unique<arts::AnalysisManager>(
        module.get(), ArtsConfig, MetadataFile);
    PassManager verifyPM(&context);
    if (failed(configurePassManager(verifyPM))) {
      ARTS_ERROR("Error configuring pass manager for metadata integrity "
                 "verification");
      return 1;
    }
    verifyPM.addPass(
        arts::createVerifyMetadataIntegrityPass(/*AM=*/verifyAM.get(),
                                                /*failOnError=*/true));
    if (failed(verifyPM.run(module.get()))) {
      ARTS_ERROR("Metadata integrity verification failed");
      return 1;
    }
    return 0;
  }

  if (Diagnose) {
    hooks.afterStep = [](StageId stage, LogicalResult result) {
      ARTS_INFO("Pipeline " << stageName(stage)
                            << (succeeded(result) ? " completed" : " FAILED"));
    };
    hooksPtr = &hooks;
  }

  /// Run the pass pipeline once.
  std::unique_ptr<arts::AnalysisManager> AM;
  if (failed(buildPassManager(module.get(), context, *resolvedStopAt,
                              *resolvedStartFrom, Diagnose ? &AM : nullptr,
                              hooksPtr))) {
    return 1;
  }

  /// Export diagnostics if requested
  if (Diagnose && AM) {
    bool includeAnalysis = shouldExportDetailedDiagnose(*resolvedStopAt) &&
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
