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
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
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
#include "mlir/Interfaces/FunctionInterfaces.h"
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
#include "polygeist/Ops.h"
#include "polygeist/Passes/Passes.h"

#include "carts/dialect/arts-rt/IR/RtDialect.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/ARTSCostModel.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/passes/Passes.h"
#include "carts/utils/Debug.h"
#include "carts/utils/ExecutionResourceAttrs.h"
#include "carts/utils/PassInstrumentation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
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
using namespace mlir::carts;
using mlir::carts::debugStream;

ARTS_DEBUG_SETUP(compile)

namespace {
constexpr const char *kDefaultDiagnoseOutput = ".carts-diagnose.json";
constexpr uint64_t kDefaultArtsIdStride = 1000;
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

static cl::opt<bool> Opt0("O0",
                          cl::desc("Do not run staged CARTS optimizations"),
                          cl::init(false));

static cl::opt<bool> Opt1("O1", cl::desc("Run staged CARTS optimizations"),
                          cl::init(false));

static cl::opt<bool> Opt2("O2", cl::desc("Run staged CARTS optimizations"),
                          cl::init(false));

static cl::opt<bool> Opt("O3", cl::desc("Apply Optimizations"),
                         cl::init(false));

static cl::opt<bool> EmitLLVM("emit-llvm", cl::desc("Emit LLVM IR output"),
                              cl::init(false));

static cl::opt<bool> Debug("g", cl::desc("Enable debug mode"), cl::init(false));

static cl::opt<std::string> ArtsConfig("arts-config",
                                       cl::desc("ARTS configuration file path"),
                                       cl::value_desc("config_file"),
                                       cl::init(""));

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
    cl::desc("Fold runtime_query<total_workers> to the configured cluster-wide "
             "worker count when the module embeds a valid ARTS config"),
    cl::init(false));

///===----------------------------------------------------------------------===///
/// Pipeline Stop Options
///===----------------------------------------------------------------------===///
enum class StageId {
  SdeInputNormalization,
  InitialCleanup,
  SdePlanning,
  SdeToArts,
  EdtDepRealization,
  EdtLocalCleanup,
  CreateDbs,
  DbOpt,
  PostDbRefinement,
  LateConcurrencyCleanup,
  Epochs,
  PreLowering,
  ArtsRtToLLVM,
  PostO3Opt,
  LLVMIREmission
};

enum class StageKind { Core, Epilogue };

struct StageExecutionContext {
  ModuleOp module;
  MLIRContext &context;
  sde::SDECostModel *costModel = nullptr;
  const arts::RuntimeConfig *machine = nullptr;
  bool stopAfterStage = false;
  bool runAdditionalOpt = false;
  bool emitLLVM = false;
  /// Distribute by default on multinode configs.
  bool enableDistributedDb = false;
};

using StageBuilderFn = void (*)(PassManager &, const StageExecutionContext &);
using StageEnabledFn = bool (*)(const StageExecutionContext &);

struct StageDescriptor {
  StageId id;
  llvm::StringLiteral token;
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

struct DialectGroupDescriptor {
  llvm::StringLiteral name;
  llvm::StringLiteral status;
  llvm::StringLiteral summary;
  llvm::ArrayRef<llvm::StringLiteral> layers;
  llvm::ArrayRef<llvm::StringLiteral> stages;
};

static constexpr llvm::StringLiteral kCompletePipelineToken = "complete";
static constexpr llvm::StringLiteral kPostO3OptToken = "post-o3-opt";
static constexpr llvm::StringLiteral kLLVMIREmissionToken = "llvm-ir-emission";
static constexpr llvm::StringLiteral kArtsRtToLLVMToken = "arts-rt-to-llvm";

static cl::opt<std::string> Pipeline(
    "pipeline",
    cl::desc("Stop pipeline at specified stage token "
             "(use --print-pipeline-manifest-json to inspect valid tokens)"),
    cl::value_desc("stage"), cl::init("complete"));

static cl::opt<std::string> StartFrom(
    "start-from",
    cl::desc("Resume pipeline from specified stage token "
             "(use --print-pipeline-manifest-json to inspect valid tokens)"),
    cl::value_desc("stage"), cl::init("sde-input-normalization"));

static cl::opt<bool> PrintPipelineManifestJSON(
    "print-pipeline-manifest-json",
    cl::desc("Print pipeline step/pass manifest as JSON and exit"),
    cl::init(false));

static cl::opt<std::string> CustomPassPipeline(
    "pass-pipeline",
    cl::desc("Run a textual MLIR pass pipeline instead of the staged CARTS "
             "pipeline"),
    cl::value_desc("pipeline"), cl::init(""));

static const std::array<llvm::StringLiteral, 12> kSdeInputNormalizationPasses =
    {"PromoteTargetAttrs",
     "SimplifyAffineStructures(func)",
     "CSE",
     "PolygeistCanonicalize",
     "SdeInputInliner",
     "PolygeistCanonicalize",
     "ScalarForwarding",
     "PolygeistCanonicalize",
     "SdeMemrefNormalization",
     "SdeHandleDeps",
     "SdeDeadStateCleanup",
     "CSE"};
static const std::array<llvm::StringLiteral, 2> kInitialCleanupPasses = {
    "CSE(func)", "PolygeistCanonicalizeFor(func)"};
static const std::array<llvm::StringLiteral, 22> kSdePlanningPasses = {
    "ConvertOpenMPToSde",
    "RaiseToSde",
    "LayoutAssignment",
    "LoopInterchange",
    "Tiling",
    "AffineCFG(func)",
    "RaiseSCFToAffine(func)",
    "SimplifyAffineStructures(func)",
    "RaiseToSde",
    "DistributionPlanning",
    "BarrierElimination",
    "MemoryUnitRealization",
    "SdeAtomicReductionRealization",
    "SdeCuNormalization",
    "SdeRankExpandMu",
    "SdeScalarBlockReduction",
    "MuAccessWindowSyncOpt",
    "VerifySdeMuAccessWindowSync",
    "SdeRedistribute",
    "SdeCoarseAvoidance",
    "VerifySdeCoarseAvoidance",
    "VerifySde"};
static const std::array<llvm::StringLiteral, 4> kSdeToArtsPasses = {
    "SdeStorageToArtsDb", "SdeAccessesToArtsDeps", "FinalizeSdeToArts",
    "VerifyArtsObjectsOnly"};
static const std::array<llvm::StringLiteral, 3> kEdtDepRealizationPasses = {
    "RealizeEdtDistribution", "VerifySdeLowered", "VerifyArtsObjectsOnly"};
static const std::array<llvm::StringLiteral, 6> kEdtLocalCleanupPasses = {
    "EdtAllocaSinking", "EdtInlineNoDepTasks", "ArtsDeadCodeElimination",
    "SymbolDCE",        "CSE(arts.edt)",       "EdtPtrRematerialization"};
static const std::array<llvm::StringLiteral, 6> kCreateDbsPasses = {
    "CreateDbs", "PolygeistCanonicalize", "CSE(arts.edt)", "SymbolDCE",
    "Mem2Reg",   "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 4> kDbOptPasses = {
    "DbModeTightening", "PolygeistCanonicalize", "CSE(arts.edt)", "Mem2Reg"};
static const std::array<llvm::StringLiteral, 13> kPostDbRefinementPasses = {
    "DbModeTightening",
    "EdtDeadDepElimination",
    "DbConsolidateStencilHalos",
    "DbStorageBridgeCopyPlacement",
    "DbShortenLifetimes",
    "DbDeadRootElimination",
    "PartialReductionSplit",
    "BlockContractionSplit",
    "DbScratchElimination",
    "DbDistributedOwnershipRealization",
    "PolygeistCanonicalize",
    "CSE(arts.edt)",
    "DistributedLaunchConsistency"};
static const std::array<llvm::StringLiteral, 6> kLateConcurrencyCleanupPasses =
    {"Hoisting",         "PolygeistCanonicalize",   "CSE(arts.edt)",
     "EdtAllocaSinking", "ArtsDeadCodeElimination", "Mem2Reg"};
static const std::array<llvm::StringLiteral, 8> kEpochsPasses = {
    "PolygeistCanonicalize",
    "CreateEpochs",
    "EpochAmortizeRepeatedLoop",
    "EpochTailContinuation",
    "PolygeistCanonicalize",
    "EdtAllocaSinking",
    "DbCommitDistributedDeps (conditional)",
    "VerifyArtsCdag"};
static const std::array<llvm::StringLiteral, 23> kPreLoweringPasses = {
    "EdtAllocaSinking",
    "PolygeistCanonicalize",
    "CSE(arts.edt)",
    "DbLowering",
    "DbDistributedRuntimeInit",
    "PolygeistCanonicalize",
    "CSE(arts.edt)",
    "EdtLowering",
    "PolygeistCanonicalize",
    "CSE",
    "VerifyEdtLowered",
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
    "VerifyEpochLowered",
    "VerifyPreLowered"};
static const std::array<llvm::StringLiteral, 13> kArtsRtToLLVMPasses = {
    "LowerAffine(func)",
    "ConvertArtsRtToLLVM",
    "GuidRangeCallOpt",
    "RuntimeCallOpt",
    "DataPtrHoisting",
    "PolygeistCanonicalize",
    "CSE",
    "Mem2Reg",
    "PolygeistCanonicalize",
    "ControlFlowSink",
    "PolygeistCanonicalize",
    "VerifyDbLowered",
    "VerifyLowered"};

static const std::array<llvm::StringLiteral, 6> kPostO3OptPasses = {
    "PolygeistCanonicalize",
    "ControlFlowSink",
    "PolygeistCanonicalize",
    "LICM",
    "CSE",
    "PolygeistCanonicalize"};
static const std::array<llvm::StringLiteral, 16> kLLVMIREmissionPasses = {
    "CSE",
    "PolygeistCanonicalize",
    "RealizeArtsFunctionPointers",
    "ConvertOpenMPToLLVM",
    "ArithExpandOps",
    "ConvertSCFToCF",
    "ResidualHostOpenMPMemrefCleanup",
    "ConvertPolygeistToLLVM",
    "ConvertIndexToLLVM",
    "ConvertControlFlowToLLVM",
    "ReconcileUnrealizedCasts",
    "AliasScopeGen",
    "LoopVectorizationHints",
    "AttachFastMathOnEdt",
    "PolygeistCanonicalize",
    "CSE"};

static constexpr llvm::StringLiteral kFrontendLayers[] = {"polygeist", "memref",
                                                          "scf"};
static constexpr llvm::StringLiteral kArtsLayers[] = {"arts"};
static constexpr llvm::StringLiteral kArtsRtLayers[] = {"arts-rt", "llvm"};
static constexpr llvm::StringLiteral kSdeLayers[] = {"sde"};
static constexpr llvm::StringLiteral kSdeToArtsLayers[] = {"sde", "arts"};

static constexpr llvm::StringLiteral kFrontendStages[] = {
    "sde-input-normalization", "initial-cleanup"};
static constexpr llvm::StringLiteral kSdePlanningStages[] = {"sde-planning"};
static constexpr llvm::StringLiteral kSdeToArtsStages[] = {"sde-to-arts"};
static constexpr llvm::StringLiteral kArtsStages[] = {
    "edt-dep-realization", "edt-local-cleanup",        "create-dbs", "db-opt",
    "post-db-refinement",  "late-concurrency-cleanup", "epochs"};
static constexpr llvm::StringLiteral kArtsRtStages[] = {"pre-lowering",
                                                        "arts-rt-to-llvm"};

static const std::array<DialectGroupDescriptor, 5> kDialectGroups = {{
    {"frontend-normalization", "canonical",
     "Normalize frontend IR before entering the CARTS dialect stack.",
     kFrontendLayers, kFrontendStages},
    {"sde", "canonical",
     "SDE proves source semantics and authors MU/CU/SU shape facts.",
     kSdeLayers, kSdePlanningStages},
    {"sde-to-arts", "canonical",
     "Mechanically lower committed SDE storage, window, scheduling, and "
     "control facts directly into ARTS DB/EDT objects.",
     kSdeToArtsLayers, kSdeToArtsStages},
    {"arts", "canonical",
     "Refine abstract ARTS DB, EDT, epoch, dep, and placement objects.",
     kArtsLayers, kArtsStages},
    {"arts-rt", "canonical",
     "Lower the chosen ARTS object graph to runtime ABI and LLVM.",
     kArtsRtLayers, kArtsRtStages},
}};

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

static void printCorePipelineSequence(llvm::raw_ostream &os) {
  os << "[";
  bool first = true;
  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Core)
      continue;
    if (!first)
      os << ", ";
    first = false;
    os << "\"" << stage.token << "\"";
  }
  os << "]";
}

static void
printDialectGroupArray(llvm::raw_ostream &os,
                       llvm::ArrayRef<DialectGroupDescriptor> groups) {
  os << "[\n";
  for (size_t i = 0; i < groups.size(); ++i) {
    const DialectGroupDescriptor &group = groups[i];
    if (i != 0)
      os << ",\n";
    os << "      {\"name\": \"" << group.name << "\", \"status\": \""
       << group.status << "\", \"summary\": \"" << group.summary
       << "\", \"layers\": ";
    printStringArray(os, group.layers);
    os << ", \"stages\": ";
    printStringArray(os, group.stages);
    os << "}";
  }
  os << "\n    ]";
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
  os << "\n  ],\n";
  os << "  \"dialect_groups\": {\n";
  os << "    \"canonical\": ";
  printDialectGroupArray(os, kDialectGroups);
  os << "\n  },\n";
  os << "  \"optimization_levels\": {\n";
  os << "    \"O0\": {\"pipeline_sequence\": [], "
        "\"epilogue_sequence\": []},\n";
  os << "    \"O1\": {\"pipeline_sequence\": ";
  printCorePipelineSequence(os);
  os << ", \"epilogue_sequence\": []},\n";
  os << "    \"O2\": {\"pipeline_sequence\": ";
  printCorePipelineSequence(os);
  os << ", \"epilogue_sequence\": []},\n";
  os << "    \"O3\": {\"pipeline_sequence\": ";
  printCorePipelineSequence(os);
  os << ", \"epilogue_sequence\": [\"" << kPostO3OptToken << "\"]}\n";
  os << "  }\n";
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

static LogicalResult
configurePassManager(PassManager &pm, PassTimingData *timingData = nullptr) {
  pm.enableVerifier(true);
  if (failed(applyPassManagerCLOptions(pm)))
    return failure();
  applyDefaultTimingPassManagerCLOptions(pm);
  if (timingData)
    pm.addInstrumentation(
        std::make_unique<CartsPassInstrumentation>(timingData));
  return success();
}

///===----------------------------------------------------------------------===///
/// Helper Functions for Initialization and Pass Setup
///===----------------------------------------------------------------------===///
/// Register standard MLIR dialects, passes, and translations.
void registerDialects(DialectRegistry &registry) {
  registry.insert<polygeist::PolygeistDialect, arts::ArtsDialect,
                  arts_rt::ArtsRtDialect, sde::CartsSdeDialect>();
  registerAllPasses();
  /// ARTS pass registration is intentionally selective: lowering-only helpers
  /// are registered here, while staged compiler pipelines wire pass ordering.
  registerDeadCodeElimination();
  registerSdeStorageToArtsDb();
  registerSdeAccessesToArtsDeps();
  registerFinalizeSdeToArts();
  registerPartialReductionSplit();
  registerBlockContractionSplit();
  registerDistributedLaunchConsistency();
  registerRealizeEdtDistribution();
  registerDbDistributedRuntimeInit();
  registerEpochTailContinuation();
  registerVerifyArtsCdag();
  registerDbCommitDistributedDeps();
  registerVerifyArtsObjectsOnly();
  registerArtsRtPasses();
  sde::registerCartsSdePasses();
  registerAllTranslations();
  registerpolygeistPasses();
  func::registerInlinerExtension(registry);
  registerAllDialects(registry);
  registerAllExtensions(registry);
  registerAllFromLLVMIRTranslations(registry);
  registerAllToLLVMIRTranslations(registry);
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
  context.getOrLoadDialect<bufferization::BufferizationDialect>();
  context.getOrLoadDialect<polygeist::PolygeistDialect>();
  context.getOrLoadDialect<arts::ArtsDialect>();
  context.getOrLoadDialect<arts_rt::ArtsRtDialect>();
  context.getOrLoadDialect<sde::CartsSdeDialect>();
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

static void ensureRuntimeConfigDataVisibleForValidation(ModuleOp module) {
  std::optional<StringRef> configData = arts::getRuntimeConfigData(module);
  if (!configData || configData->empty())
    return;
  constexpr llvm::StringLiteral kConfigGlobalName = "__carts_embedded_arts_cfg";
  if (module.lookupSymbol<LLVM::GlobalOp>(kConfigGlobalName))
    return;

  OpBuilder builder(module.getContext());
  builder.setInsertionPointToStart(module.getBody());
  auto i8 = mlir::IntegerType::get(module.getContext(), 8);
  auto type = LLVM::LLVMArrayType::get(i8, configData->size() + 1);
  LLVM::GlobalOp::create(builder, module.getLoc(), type, /*isConstant=*/true,
                         LLVM::Linkage::Internal, kConfigGlobalName,
                         builder.getStringAttr(configData->str() + '\0'));
}

static bool hasResidualOpenMP(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (op->getDialect() && op->getDialect()->getNamespace() == "omp") {
      found = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return found;
}

static bool isArtsOutlinedEdtName(StringRef name) {
  return name.starts_with("__arts_edt_");
}

static LogicalResult promoteOutlinedEdtToLLVMFunc(func::FuncOp funcOp,
                                                  OpBuilder &builder) {
  mlir::FunctionType funcType = funcOp.getFunctionType();
  MLIRContext *ctx = funcOp.getContext();

  mlir::Type resultType = LLVM::LLVMVoidType::get(ctx);
  if (funcType.getNumResults() > 1)
    return funcOp.emitError("cannot realize ARTS EDT function pointer for "
                            "multi-result function");
  if (funcType.getNumResults() == 1) {
    resultType = funcType.getResult(0);
    if (!LLVM::isCompatibleType(resultType))
      return funcOp.emitError("cannot realize ARTS EDT function pointer "
                              "before LLVM conversion for result type ")
             << resultType;
  }

  SmallVector<mlir::Type, 8> inputTypes;
  inputTypes.reserve(funcType.getNumInputs());
  for (mlir::Type input : funcType.getInputs()) {
    if (!LLVM::isCompatibleType(input))
      return funcOp.emitError("cannot realize ARTS EDT function pointer "
                              "before LLVM conversion for argument type ")
             << input;
    inputTypes.push_back(input);
  }

  auto llvmType =
      LLVM::LLVMFunctionType::get(resultType, inputTypes, /*isVarArg=*/false);
  builder.setInsertionPoint(funcOp);
  auto llvmFunc = LLVM::LLVMFuncOp::create(
      builder, funcOp.getLoc(), funcOp.getName(), llvmType,
      LLVM::Linkage::External, /*dsoLocal=*/false, LLVM::CConv::C);
  cast<FunctionOpInterface>(llvmFunc.getOperation())
      .setVisibility(funcOp.getVisibility());
  llvmFunc.getBody().takeBody(funcOp.getBody());
  SmallVector<func::ReturnOp, 4> returns;
  llvmFunc.walk([&](func::ReturnOp ret) { returns.push_back(ret); });
  for (func::ReturnOp ret : returns) {
    builder.setInsertionPoint(ret);
    LLVM::ReturnOp::create(builder, ret.getLoc(), ret.getOperands());
    ret.erase();
  }
  funcOp.erase();
  return success();
}

struct RealizeArtsFunctionPointersPass
    : public PassWrapper<RealizeArtsFunctionPointersPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(RealizeArtsFunctionPointersPass)

  StringRef getArgument() const final {
    return "carts-realize-arts-function-pointers";
  }

  StringRef getDescription() const final {
    return "Realize ARTS EDT function pointer symbols before mixed "
           "host-OpenMP LLVM emission";
  }

  void runOnOperation() final {
    ModuleOp module = getOperation();
    if (!hasResidualOpenMP(module))
      return;

    SmallVector<polygeist::GetFuncOp, 8> getFuncOps;
    module.walk([&](polygeist::GetFuncOp op) {
      if (isArtsOutlinedEdtName(op.getName()))
        getFuncOps.push_back(op);
    });
    if (getFuncOps.empty())
      return;

    OpBuilder builder(module.getContext());
    for (polygeist::GetFuncOp getFunc : getFuncOps) {
      StringRef name = getFunc.getName();
      if (!module.lookupSymbol<LLVM::LLVMFuncOp>(name)) {
        auto funcOp = module.lookupSymbol<func::FuncOp>(name);
        if (!funcOp) {
          getFunc.emitError("ARTS EDT function pointer target was not found: ")
              << name;
          signalPassFailure();
          return;
        }
        if (failed(promoteOutlinedEdtToLLVMFunc(funcOp, builder))) {
          signalPassFailure();
          return;
        }
      }

      builder.setInsertionPoint(getFunc);
      auto address = LLVM::AddressOfOp::create(builder, getFunc.getLoc(),
                                               getFunc.getType(), name);
      getFunc.getResult().replaceAllUsesWith(address.getResult());
      getFunc.erase();
    }
  }
};

static bool isLLVMMemRefDescriptorLike(mlir::Type type) {
  auto structType = dyn_cast<LLVM::LLVMStructType>(type);
  if (!structType || structType.getBody().size() < 2)
    return false;
  return isa<LLVM::LLVMPointerType>(structType.getBody()[0]) &&
         isa<LLVM::LLVMPointerType>(structType.getBody()[1]);
}

static void foldResidualHostOpenMPMemrefPointerCasts(ModuleOp module) {
  SmallVector<UnrealizedConversionCastOp> casts;
  module.walk([&](UnrealizedConversionCastOp cast) {
    if (cast.getNumOperands() != 1 || cast.getNumResults() != 1)
      return;
    if (!isa<LLVM::LLVMPointerType>(cast.getResult(0).getType()))
      return;
    if (!isa<MemRefType>(cast.getOperand(0).getType()))
      return;
    casts.push_back(cast);
  });

  for (UnrealizedConversionCastOp cast : casts) {
    if (!cast || cast->use_empty())
      continue;
    auto sourceCast =
        cast.getOperand(0).getDefiningOp<UnrealizedConversionCastOp>();
    if (!sourceCast || sourceCast.getNumOperands() != 1 ||
        sourceCast.getNumResults() != 1)
      continue;
    mlir::Value descriptor = sourceCast.getOperand(0);
    if (!isLLVMMemRefDescriptorLike(descriptor.getType()))
      continue;

    OpBuilder builder(cast);
    mlir::Value ptr = LLVM::ExtractValueOp::create(builder, cast.getLoc(),
                                                   descriptor, /*position=*/1);
    if (ptr.getType() != cast.getResult(0).getType())
      ptr = LLVM::AddrSpaceCastOp::create(builder, cast.getLoc(),
                                          cast.getResult(0).getType(), ptr);
    cast.getResult(0).replaceAllUsesWith(ptr);
    cast.erase();
    if (sourceCast->use_empty())
      sourceCast.erase();
  }
}

static mlir::Value createLLVMIndexConstant(OpBuilder &builder, Location loc,
                                           mlir::Type type, int64_t value) {
  return LLVM::ConstantOp::create(builder, loc, type,
                                  builder.getIntegerAttr(type, value));
}

static mlir::Value
buildRankOneDescriptorFromBarePtr(OpBuilder &builder, Location loc,
                                  mlir::Value ptr, mlir::Type descriptorType) {
  auto structType = dyn_cast<LLVM::LLVMStructType>(descriptorType);
  if (!structType || structType.getBody().size() < 5)
    return {};

  mlir::Type indexType = structType.getBody()[2];
  mlir::Value zero = createLLVMIndexConstant(builder, loc, indexType, 0);
  mlir::Value one = createLLVMIndexConstant(builder, loc, indexType, 1);

  mlir::Value descriptor = LLVM::PoisonOp::create(builder, loc, descriptorType);
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, ptr,
                                           ArrayRef<int64_t>{0});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, ptr,
                                           ArrayRef<int64_t>{1});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, zero,
                                           ArrayRef<int64_t>{2});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, zero,
                                           ArrayRef<int64_t>{3, 0});
  descriptor = LLVM::InsertValueOp::create(builder, loc, descriptor, one,
                                           ArrayRef<int64_t>{4, 0});
  return descriptor;
}

static void cleanupResidualHostOpenMPMemrefs(ModuleOp module) {
  SmallVector<UnrealizedConversionCastOp> castsToDescriptor;
  module.walk([&](UnrealizedConversionCastOp cast) {
    if (cast.getNumOperands() != 1 || cast.getNumResults() != 1)
      return;
    if (!isLLVMMemRefDescriptorLike(cast.getResult(0).getType()))
      return;
    auto ptrToMemref =
        cast.getOperand(0).getDefiningOp<polygeist::Pointer2MemrefOp>();
    if (!ptrToMemref)
      return;
    auto memrefType = dyn_cast<MemRefType>(ptrToMemref.getType());
    if (!memrefType || memrefType.getRank() != 1 || memrefType.hasStaticShape())
      return;
    castsToDescriptor.push_back(cast);
  });

  for (UnrealizedConversionCastOp cast : castsToDescriptor) {
    if (!cast)
      continue;
    auto ptrToMemref =
        cast.getOperand(0).getDefiningOp<polygeist::Pointer2MemrefOp>();
    if (!ptrToMemref)
      continue;
    OpBuilder builder(cast);
    mlir::Value descriptor = buildRankOneDescriptorFromBarePtr(
        builder, cast.getLoc(), ptrToMemref.getSource(),
        cast.getResult(0).getType());
    if (!descriptor)
      continue;
    cast.getResult(0).replaceAllUsesWith(descriptor);
    cast.erase();
    if (ptrToMemref->use_empty())
      ptrToMemref.erase();
  }

  SmallVector<polygeist::Memref2PointerOp> memrefToPointerOps;
  module.walk([&](polygeist::Memref2PointerOp op) {
    auto sourceCast =
        op.getSource().getDefiningOp<UnrealizedConversionCastOp>();
    if (!sourceCast || sourceCast.getNumOperands() != 1 ||
        !isLLVMMemRefDescriptorLike(sourceCast.getOperand(0).getType()))
      return;
    memrefToPointerOps.push_back(op);
  });

  for (polygeist::Memref2PointerOp op : memrefToPointerOps) {
    if (!op)
      continue;
    auto sourceCast =
        op.getSource().getDefiningOp<UnrealizedConversionCastOp>();
    if (!sourceCast)
      continue;
    OpBuilder builder(op);
    mlir::Value ptr = LLVM::ExtractValueOp::create(
        builder, op.getLoc(), sourceCast.getOperand(0), /*position=*/1);
    if (ptr.getType() != op.getType())
      ptr = LLVM::AddrSpaceCastOp::create(builder, op.getLoc(), op.getType(),
                                          ptr);
    op.getResult().replaceAllUsesWith(ptr);
    op.erase();
    if (sourceCast->use_empty())
      sourceCast.erase();
  }
}

struct ResidualHostOpenMPMemrefCleanupPass
    : public PassWrapper<ResidualHostOpenMPMemrefCleanupPass,
                         OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(
      ResidualHostOpenMPMemrefCleanupPass)

  StringRef getArgument() const final {
    return "carts-residual-host-openmp-memref-cleanup";
  }

  StringRef getDescription() const final {
    return "Clean residual host-OpenMP memref/pointer bridges before LLVM "
           "emission";
  }

  void runOnOperation() final {
    ModuleOp module = getOperation();
    if (!hasResidualOpenMP(module))
      return;
    cleanupResidualHostOpenMPMemrefs(module);
  }
};

static bool hasArtsRuntimeCallsOutsideHostWrappers(llvm::Module &module) {
  for (llvm::Function &function : module) {
    if (function.isDeclaration())
      continue;
    if (function.getName() == "main" || function.getName() == "main_edt")
      continue;

    for (llvm::BasicBlock &block : function) {
      for (llvm::Instruction &instruction : block) {
        auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
        if (!call)
          continue;
        llvm::Function *callee = call->getCalledFunction();
        if (callee && callee->getName().starts_with("arts_"))
          return true;
      }
    }
  }
  return false;
}

static void bypassArtsRuntimeForHostOpenMP(llvm::Module &module) {
  llvm::Function *main = module.getFunction("main");
  llvm::Function *mainBody = module.getFunction("mainBody");
  if (!main || !mainBody || main->arg_size() != 2)
    return;
  if (mainBody->arg_size() != 0 && mainBody->arg_size() != 2)
    return;
  if (hasArtsRuntimeCallsOutsideHostWrappers(module))
    return;

  main->deleteBody();
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(module.getContext(), "entry", main);
  llvm::IRBuilder<> builder(entry);
  SmallVector<llvm::Value *, 2> args;
  if (mainBody->arg_size() == 2)
    for (llvm::Argument &arg : main->args())
      args.push_back(&arg);
  llvm::CallInst *result = builder.CreateCall(mainBody, args);
  if (main->getReturnType()->isVoidTy())
    builder.CreateRetVoid();
  else
    builder.CreateRet(result);
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

static void addEdtLocalCSE(PassManager &pm) {
  /// Generic module CSE can replace an EDT-local scalar with an equivalent
  /// outer SSA value because arts.edt is intentionally not IsolatedFromAbove.
  /// Run CSE at the EDT root while abstract EDTs are live so cleanup stays
  /// within the explicit dep/param boundary.
  pm.addNestedPass<arts::EdtOp>(createCSEPass());
}

static void addCanonicalizeAndEdtLocalCSE(PassManager &pm) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  addEdtLocalCSE(pm);
}

static void addAffineRecoveryBundle(OpPassManager &pm) {
  pm.addPass(polygeist::replaceAffineCFGPass());
  pm.addPass(polygeist::createRaiseSCFToAffinePass());
  pm.addPass(affine::createSimplifyAffineStructuresPass());
}

/// Normalize frontend storage and dependency shape before SDE conversion.
void buildSdeInputNormalizationPipeline(PassManager &pm) {
  /// PromoteTargetAttrs runs first so every downstream stage observes the
  /// host target through target-neutral CARTS module attrs, even after
  /// upstream conversion passes drop the polygeist-prefixed originals.
  pm.addPass(sde::createPromoteTargetAttrs());
  OpPassManager &optPM = pm.nest<func::FuncOp>();
  // architecture.md S3: keep affine through normalization; normalize instead of
  // lowering so memref normalization can read affine.load/store maps natively.
  optPM.addPass(affine::createSimplifyAffineStructuresPass());
  pm.addPass(createCSEPass());
  // Fold constant control flow (e.g. cgeist's `scf.if %true` / execute_region
  // nests around a `min(NI,NJ)` loop bound) BEFORE inlining. Otherwise a helper
  // whose `affine.for` bound is such a non-constant value is a valid affine
  // symbol only at the callee's top level, and inlining it produces invalid
  // affine IR ("operand cannot be used as a symbol"). Folding the bound to a
  // constant keeps the helper inlinable (its accesses must land on the caller's
  // DB-backed storage for SDE-to-ARTS realization).
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(sde::createSdeInputInlinerPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(sde::createScalarForwardingPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(sde::createSdeMemrefNormalizationPass());
  pm.addPass(sde::createSdeHandleDepsPass());
  pm.addPass(sde::createSdeDeadStateCleanupPass());
  pm.addPass(createCSEPass());
}

/// Initial cleanup and simplification passes.
void buildInitialCleanupPipeline(OpPassManager &optPM) {
  optPM.addPass(createCSEPass());
  optPM.addPass(polygeist::createCanonicalizeForPass());
}

/// OpenMP to SDE fact conversion. ARTS objects are intentionally not lowered
/// here; SDE facts feed the direct `sde-to-arts` boundary.
void buildSdePlanningPipeline(PassManager &pm,
                              sde::SDECostModel *costModel = nullptr) {
  pm.addPass(sde::createConvertOpenMPToSdePass());
  // raise-to-sde CORE promotes proven-independent host nests (scf or affine)
  // and folds the initial cu-normalization.
  pm.addPass(sde::createRaiseToSdePass());
  // Module-scoped per-array BLOCK layout assignment. Runs before
  // Tiling/Interchange split the parallel axes.
  pm.addPass(sde::createLayoutAssignmentPass(costModel));
  // S4: Interchange handles affine stencil nests via permuteLoops; Tiling still
  // emits scf owner tile loops. Keep affine through this window; S4d re-raises
  // serial inner loops after Tiling. Final LowerAffine remains in ARTS-RT
  // (:1277).
  pm.addPass(sde::createLoopInterchangePass());
  pm.addPass(sde::createTilingPass(costModel));
  // S4d: recover affine CFG/loops where legal for MemRefAccess-based analysis.
  addAffineRecoveryBundle(pm.nest<func::FuncOp>());
  // S6: re-run raise-to-sde after shape transforms expose new parallelism.
  pm.addPass(sde::createRaiseToSdePass());
  pm.addPass(sde::createDistributionPlanningPass(costModel));
  pm.addPass(sde::createBarrierEliminationPass(costModel));
  pm.addPass(sde::createMemoryUnitRealizationPass());
  pm.addPass(sde::createSdeAtomicReductionRealizationPass());
  pm.addPass(sde::createSdeCuNormalizationPass());
  // Each real SDE grain transform is immediately gated by its companion
  // verifier in the production order, so a stale or mismatched physical grain
  // fails closed at the SDE boundary instead of being repaired downstream.
  pm.addPass(sde::createSdeRankExpandMuPass());
  pm.addPass(sde::createSdeScalarBlockReductionPass());
  pm.addPass(sde::createMuAccessWindowSyncOptPass());
  pm.addPass(sde::createVerifySdeMuAccessWindowSyncPass());
  pm.addPass(sde::createSdeRedistributePass());
  pm.addPass(sde::createSdeCoarseAvoidancePass());
  pm.addPass(sde::createVerifySdeCoarseAvoidancePass());
  pm.addPass(sde::createVerifySdePass());
}

/// SDE-to-ARTS conversion. SDE owns transformed facts; ARTS realizes DB,
/// acquire, EDT, and control objects directly from those committed facts.
void buildSdeToArtsPipeline(PassManager &pm) {
  pm.addPass(arts::createSdeStorageToArtsDbPass());
  pm.addPass(arts::createSdeAccessesToArtsDepsPass());
  pm.addPass(arts::createFinalizeSdeToArtsPass());
  pm.addPass(arts::createVerifyArtsObjectsOnlyPass());
}

/// EDT dependency realization over direct SDE-to-ARTS objects.
void buildEdtDepRealizationPipeline(PassManager &pm) {
  pm.addPass(arts::createRealizeEdtDistributionPass());
  pm.addPass(sde::createVerifySdeLoweredPass());
  pm.addPass(arts::createVerifyArtsObjectsOnlyPass());
}

/// EDT-local cleanup passes.
void buildEdtLocalCleanupPipeline(PassManager &pm) {
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createEdtInlineNoDepTasksPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createSymbolDCEPass());
  addEdtLocalCSE(pm);
  pm.addPass(arts::createEdtPtrRematerializationPass());
}

/// DB creation pass.
void buildCreateDbsPipeline(PassManager &pm) {
  pm.addPass(arts::createCreateDbsPass());
  addCanonicalizeAndEdtLocalCSE(pm);
  pm.addPass(createSymbolDCEPass());
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}

/// DB creation and optimization passes.
void buildDbOptPipeline(PassManager &pm) {
  pm.addPass(arts::createDbModeTighteningPass());
  addCanonicalizeAndEdtLocalCSE(pm);
  pm.addPass(createMem2Reg());
}

/// Tighten DB modes and persist post-partition refinement facts.
void buildPostDbRefinementPipeline(PassManager &pm) {
  /// DbModeTighteningPass performs local DB cleanup after mode adjustment,
  /// which can expose new zero-dependency or degenerate EDTs before epoch
  /// shaping. Mode tightening must run before EDT transforms so affinity and
  /// reduction analysis see accurate writer/reader modes.
  pm.addPass(arts::createDbModeTighteningPass());
  pm.addPass(arts::createEdtDeadDepEliminationPass());
  /// Re-run DB-local refinement after EDT dep pruning so cleanup-only acquires
  /// and now-unreachable DB roots are removed in the DB layer.
  pm.addPass(arts::createDbConsolidateStencilHalosPass());
  pm.addPass(arts::createDbStorageBridgeCopyPlacementPass());
  pm.addPass(arts::createDbShortenLifetimesPass());
  pm.addPass(arts::createDbDeadRootEliminationPass());
  pm.addPass(arts::createPartialReductionSplitPass());
  pm.addPass(arts::createBlockContractionSplitPass());
  pm.addPass(arts::createDbScratchEliminationPass());
  pm.addPass(arts::createDbDistributedOwnershipRealizationPass());
  addCanonicalizeAndEdtLocalCSE(pm);
  pm.addPass(arts::createDistributedLaunchConsistencyPass());
}

/// Apply late DB-aware loop cleanup and final stack/SSA simplification.
void buildLateConcurrencyCleanupPipeline(PassManager &pm) {
  pm.addPass(arts::createHoistingPass());
  addCanonicalizeAndEdtLocalCSE(pm);
  /// TODO(PERF): EdtAllocaSinkingPass runs twice (here and pre-lowering).
  pm.addPass(arts::createEdtAllocaSinkingPass());
  pm.addPass(arts::createDCEPass());
  pm.addPass(createMem2Reg());
}

/// Epoch creation passes.
void buildEpochsPipeline(PassManager &pm, bool enableDistributedDb) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createCreateEpochsPass());
  /// Realize committed repeated-timestep epoch loops on newly created epochs.
  pm.addPass(arts::createEpochAmortizeRepeatedLoopPass());
  pm.addPass(arts::createEpochTailContinuationPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createEdtAllocaSinkingPass());
  /// Commit distributed acquire DB modes / halo after epoch shaping, then
  /// verify the canonical-owner DAG fails closed before pre-lowering.
  if (enableDistributedDb) {
    pm.addPass(arts::createDbCommitDistributedDepsPass());
  }
  pm.addPass(arts::createVerifyArtsCdagPass());
}

/// Pre-lowering passes.
void buildPreLoweringPipeline(PassManager &pm) {
  /// TODO(PERF): EdtAllocaSinkingPass runs twice (late concurrency cleanup
  /// and here).
  pm.addPass(arts::createEdtAllocaSinkingPass());
  addCanonicalizeAndEdtLocalCSE(pm);
  pm.addPass(arts_rt::createDbLoweringPass(ArtsIdStride));
  pm.addPass(arts::createDbDistributedRuntimeInitPass());
  addCanonicalizeAndEdtLocalCSE(pm);
  pm.addPass(arts_rt::createEdtLoweringPass(ArtsIdStride));
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts_rt::createVerifyEdtLoweredPass());
  pm.addPass(createLoopInvariantCodeMotionPass());
  /// Hoist loop-invariant DB/dep pointer loads before scalar replacement;
  /// buildArtsRtToLLVMPipeline runs hoisting again after ARTS-RT-to-LLVM
  /// realizes new loads.
  pm.addPass(arts_rt::createDataPtrHoistingPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts_rt::createScalarReplacementPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts_rt::createEpochLoweringPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(arts_rt::createVerifyEpochLoweredPass());
  pm.addPass(arts_rt::createVerifyPreLoweredPass());
}

/// ARTS-RT to LLVM conversion passes.
void buildArtsRtToLLVMPipeline(PassManager &pm, bool debug,
                               bool distributedInitPerWorker,
                               const arts::RuntimeConfig *machine) {
  pm.addNestedPass<func::FuncOp>(createLowerAffinePass());
  pm.addPass(arts_rt::createConvertArtsRtToLLVMPass(
      debug, distributedInitPerWorker, machine));
  pm.addPass(arts_rt::createGuidRangeCallOptPass());
  pm.addPass(arts_rt::createRuntimeCallOptPass());
  /// Hoist loop-invariant loads after ARTS-RT-to-LLVM lowering for
  /// vectorization/LICM.
  pm.addPass(arts_rt::createDataPtrHoistingPass());
  addCanonicalizeAndCSE(pm);
  pm.addPass(createMem2Reg());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(createControlFlowSinkPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts_rt::createVerifyDbLoweredPass());
  pm.addPass(arts_rt::createVerifyLoweredPass());
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
void buildLLVMIREmissionPipeline(PassManager &pm, bool convertOpenMP) {
  pm.addPass(createCSEPass());
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(std::make_unique<RealizeArtsFunctionPointersPass>());
  if (convertOpenMP)
    pm.addPass(createConvertOpenMPToLLVMPass());
  pm.addPass(arith::createArithExpandOpsPass());
  pm.addPass(createSCFToControlFlowPass());
  pm.addPass(std::make_unique<ResidualHostOpenMPMemrefCleanupPass>());
  pm.addPass(polygeist::createConvertPolygeistToLLVMPass());
  pm.addPass(createConvertIndexToLLVMPass());
  pm.addPass(createConvertControlFlowToLLVMPass());
  pm.addPass(createReconcileUnrealizedCastsPass());
  pm.addPass(arts_rt::createAliasScopeGenPass());
  pm.addPass(arts_rt::createLoopVectorizationHintsPass());
  pm.addPass(arts_rt::createAttachFastMathOnEdtPass());
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
static constexpr llvm::StringLiteral kDepSdeInputNormalization[] = {
    "sde-input-normalization"};
static constexpr llvm::StringLiteral kDepInitialCleanup[] = {"initial-cleanup"};
static constexpr llvm::StringLiteral kDepSdePlanning[] = {"sde-planning"};
static constexpr llvm::StringLiteral kDepSdeToArts[] = {"sde-to-arts"};
static constexpr llvm::StringLiteral kDepEdtDepRealization[] = {
    "edt-dep-realization"};
static constexpr llvm::StringLiteral kDepCreateDbs[] = {"create-dbs"};
static constexpr llvm::StringLiteral kDepPostDbRefinement[] = {
    "post-db-refinement"};
static constexpr llvm::StringLiteral kDepPreLowering[] = {
    "epochs", "late-concurrency-cleanup"};
static constexpr llvm::StringLiteral kDepArtsRtToLLVM[] = {"pre-lowering"};
static ArrayRef<StageDescriptor> getStageRegistry() {
  static const std::array<StageDescriptor, 15> kStageRegistry = {{
      {StageId::SdeInputNormalization, "sde-input-normalization",
       StageKind::Core, true, true, false, "Error when normalizing SDE input",
       kSdeInputNormalizationPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildSdeInputNormalizationPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/llvm::ArrayRef<llvm::StringLiteral>()},
      {StageId::InitialCleanup, "initial-cleanup", StageKind::Core, true, true,
       false, "Error simplifying the IR", kInitialCleanupPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         OpPassManager &optPM = pm.nest<func::FuncOp>();
         buildInitialCleanupPipeline(optPM);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepSdeInputNormalization},
      {StageId::SdePlanning, "sde-planning", StageKind::Core, true, true, false,
       "Error when converting OpenMP to SDE fact IR", kSdePlanningPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildSdePlanningPipeline(pm, ctx.costModel);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepInitialCleanup},
      {StageId::SdeToArts, "sde-to-arts", StageKind::Core, true, true, false,
       "Error when converting SDE to ARTS", kSdeToArtsPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildSdeToArtsPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepSdePlanning},
      {StageId::EdtDepRealization, "edt-dep-realization", StageKind::Core, true,
       true, false, "Error when realizing EDT dependencies",
       kEdtDepRealizationPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildEdtDepRealizationPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepSdeToArts},
      {StageId::EdtLocalCleanup, "edt-local-cleanup", StageKind::Core, true,
       true, false, "Error when running EDT-local cleanup",
       kEdtLocalCleanupPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildEdtLocalCleanupPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepEdtDepRealization},
      {StageId::CreateDbs, "create-dbs", StageKind::Core, true, true, false,
       "Error when creating DBs", kCreateDbsPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildCreateDbsPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepEdtDepRealization},
      {StageId::DbOpt, "db-opt", StageKind::Core, true, true, false,
       "Error when optimizing DBs", kDbOptPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildDbOptPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepCreateDbs},
      {StageId::PostDbRefinement, "post-db-refinement", StageKind::Core, true,
       true, false, "Error when refining post-partition DB facts",
       kPostDbRefinementPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildPostDbRefinementPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepCreateDbs},
      {StageId::LateConcurrencyCleanup, "late-concurrency-cleanup",
       StageKind::Core, true, true, false,
       "Error when running late concurrency cleanup",
       kLateConcurrencyCleanupPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildLateConcurrencyCleanupPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepPostDbRefinement},
      {StageId::Epochs, "epochs", StageKind::Core, true, true, false,
       "Error when creating and optimizing epochs", kEpochsPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildEpochsPipeline(pm, ctx.enableDistributedDb);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepPostDbRefinement},
      {StageId::PreLowering, "pre-lowering", StageKind::Core, true, true, true,
       "Error when pre-lowering DBs, EDTs, and Epochs", kPreLoweringPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         buildPreLoweringPipeline(pm);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepPreLowering},
      {StageId::ArtsRtToLLVM, kArtsRtToLLVMToken, StageKind::Core, true, true,
       false, "Error when lowering ARTS-RT to LLVM", kArtsRtToLLVMPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildArtsRtToLLVMPipeline(pm, Debug, ctx.enableDistributedDb,
                                   ctx.machine);
       },
       isStageEnabledAlways,
       /*dependsOn=*/kDepArtsRtToLLVM},
      {StageId::PostO3Opt, kPostO3OptToken, StageKind::Epilogue, false, false,
       false, "Error when running classical optimizations", kPostO3OptPasses,
       [](PassManager &pm, const StageExecutionContext &) {
         OpPassManager &optPM = pm.nest<func::FuncOp>();
         buildAdditionalOptPipeline(optPM);
       },
       isStageEnabledWhenOptRequested,
       /*dependsOn=*/kDepArtsRtToLLVM},
      {StageId::LLVMIREmission, kLLVMIREmissionToken, StageKind::Epilogue,
       false, false, false, "Error when emitting LLVM IR",
       kLLVMIREmissionPasses,
       [](PassManager &pm, const StageExecutionContext &ctx) {
         buildLLVMIREmissionPipeline(pm, hasResidualOpenMP(ctx.module));
       },
       isStageEnabledWhenEmitLLVMRequested,
       /*dependsOn=*/kDepArtsRtToLLVM},
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
    if (!(startFrom ? stage.allowStartFrom : stage.allowPipelineStop))
      continue;
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
                 StageId startFrom = StageId::SdeInputNormalization,
                 PipelineHooks *hooks = nullptr) {
  assert(validatePipelineDAG(getStageRegistry()) &&
         "Pipeline dependency DAG is invalid");

  int startIndex = stageIndex(startFrom, StageKind::Core);
  int stopIndex = stopAt ? stageIndex(*stopAt, StageKind::Core)
                         : stageIndex(StageId::ArtsRtToLLVM, StageKind::Core);
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

  arts::RuntimeConfig machine(ArtsConfig);
  if (!machine.hasConfigFile() || !machine.isValid()) {
    ARTS_ERROR("Invalid ARTS configuration. Provide a valid --arts-config path "
               "or place a valid arts.cfg in the working directory.");
    return failure();
  }

  arts::ARTSCostModel costModel(machine);

  /// Embed config file contents into the module so generated binaries are
  /// self-contained — no external config file needed at runtime.
  if (machine.hasConfigFile() && !machine.getConfigPath().empty()) {
    auto configContents = llvm::MemoryBuffer::getFile(machine.getConfigPath());
    if (configContents)
      arts::setRuntimeConfigData(module, (*configContents)->getBuffer());
    else
      arts::setRuntimeConfigPath(module, machine.getConfigPath());
  }
  setLogicalTotalWorkers(module, machine.getRuntimeTotalWorkers());
  setLogicalTotalLocalities(module, machine.getNodeCount());
  arts::setRuntimeTotalWorkers(module, machine.getRuntimeTotalWorkers());
  arts::setRuntimeTotalNodes(module, machine.getNodeCount());
  arts::setRuntimeStaticWorkers(module, RuntimeStaticWorkers);

  /// Multinode configs distribute by default. Single-node configs have nothing
  /// to distribute.
  const bool enableDistributedDb = machine.getNodeCount() > 1;
  /// Create shared timing data for pass instrumentation.
  PassTimingData timingData;
  PassTimingData *timingDataPtr = PassTiming ? &timingData : nullptr;

  auto runStage = [&](const StageDescriptor &stage,
                      bool stopAfterStage) -> LogicalResult {
    if (hooks && hooks->beforeStep)
      hooks->beforeStep(stage.id);

    PassManager pm(&context);
    if (failed(configurePassManager(pm, timingDataPtr))) {
      ARTS_ERROR("Error configuring pass manager for pipeline step "
                 << stage.token);
      return failure();
    }
    StageExecutionContext stageContext{
        module,         context, &costModel, &machine,
        stopAfterStage, Opt,     EmitLLVM,   enableDistributedDb};
    stage.build(pm, stageContext);
    auto result = pm.run(module);

    if (hooks && hooks->afterStep)
      hooks->afterStep(stage.id, result);

    if (failed(result)) {
      ARTS_ERROR(stage.errorMessage);
      module->dump();
      return failure();
    }
    return success();
  };

  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Core)
      continue;
    int currentIndex = stageIndex(stage.id, StageKind::Core);
    if (currentIndex < startIndex)
      continue;
    if (!stage.enabled(StageExecutionContext{module, context, &costModel,
                                             &machine, false, Opt, EmitLLVM,
                                             enableDistributedDb}))
      continue;
    bool stopHere = stopAt && *stopAt == stage.id;
    if (failed(runStage(stage, stopHere)))
      return failure();
    if (stopHere) {
      if (PassTiming)
        timingData.printTimingReport(llvm::errs());
      return success();
    }
  }

  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Epilogue)
      continue;
    StageExecutionContext stageContext{
        module, context, &costModel, &machine,
        false,  Opt,     EmitLLVM,   enableDistributedDb};
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
  unsigned selectedOptLevels =
      (Opt0 ? 1 : 0) + (Opt1 ? 1 : 0) + (Opt2 ? 1 : 0) + (Opt ? 1 : 0);
  if (selectedOptLevels > 1) {
    ARTS_ERROR("Select only one optimization level: -O0, -O1, -O2, or -O3");
    return 1;
  }
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

  if (Opt0 && CustomPassPipeline.empty()) {
    ARTS_ERROR("-O0 does not run the staged CARTS optimization pipeline");
    return 1;
  }

  FailureOr<StageId> resolvedStartFrom =
      resolveRequiredStageToken(StartFrom.getValue(), /*allowStartFrom=*/true);
  if (failed(resolvedStartFrom)) {
    emitUnknownStageTokenError(StartFrom, /*startFrom=*/true);
    return 1;
  }

  std::string effectivePipelineToken = Pipeline;

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

  if (!CustomPassPipeline.empty()) {
    PassManager pm(&context);
    if (failed(configurePassManager(pm))) {
      ARTS_ERROR("Error configuring pass manager for --pass-pipeline");
      return 1;
    }
    StringRef passPipeline = CustomPassPipeline;
    std::string unwrappedPassPipeline;
    if (passPipeline.consume_front("builtin.module(") &&
        passPipeline.consume_back(")")) {
      unwrappedPassPipeline = passPipeline.str();
      passPipeline = unwrappedPassPipeline;
    } else {
      passPipeline = CustomPassPipeline;
    }
    if (failed(parsePassPipeline(passPipeline, pm))) {
      ARTS_ERROR("Could not parse --pass-pipeline: " << CustomPassPipeline);
      return 1;
    }
    if (failed(pm.run(module.get()))) {
      ARTS_ERROR("Error running --pass-pipeline: " << CustomPassPipeline);
      module->dump();
      return 1;
    }

    auto output = openOutputFile(OutputFilename);
    if (!output) {
      ARTS_ERROR("Could not open output file: " << OutputFilename);
      return 1;
    }
    module->print(output->os());
    output->keep();
    return 0;
  }

  /// Set up optional pipeline hooks for diagnostics.
  PipelineHooks hooks;
  PipelineHooks *hooksPtr = nullptr;
  if (Diagnose) {
    hooks.afterStep = [](StageId stage, LogicalResult result) {
      ARTS_INFO("Pipeline " << stageName(stage)
                            << (succeeded(result) ? " completed" : " FAILED"));
    };
    hooksPtr = &hooks;
  }

  if (failed(buildPassManager(module.get(), context, *resolvedStopAt,
                              *resolvedStartFrom, hooksPtr))) {
    return 1;
  }

  /// Export lightweight pipeline diagnostics if requested. The old DB graph
  /// diagnostic dump was removed with the ARTS analysis manager.
  if (Diagnose) {
    if (!DiagnoseOutput.empty()) {
      std::error_code EC;
      llvm::raw_fd_ostream outputFile(DiagnoseOutput, EC);
      if (EC) {
        ARTS_ERROR(
            "Could not open diagnostics output file: " << DiagnoseOutput);
        return 1;
      }
      outputFile << "{ \"diagnostics\": \"pipeline-only\" }\n";
      outputFile.close();
    } else {
      llvm::outs() << "{ \"diagnostics\": \"pipeline-only\" }\n";
    }
  }

  /// Translate the optimized module to LLVM IR and write output.
  if (EmitLLVM) {
    bool hasHostOpenMP = hasResidualOpenMP(module.get());
    if (hasHostOpenMP)
      foldResidualHostOpenMPMemrefPointerCasts(module.get());
    ensureRuntimeConfigDataVisibleForValidation(module.get());
    LLVMContext llvmContext;
    auto llvmModule = translateModuleToLLVMIR(module.get(), llvmContext);
    if (!llvmModule) {
      module->dump();
      ARTS_ERROR("Failed to emit LLVM IR");
      return -1;
    }
    if (hasHostOpenMP)
      bypassArtsRuntimeForHostOpenMP(*llvmModule);
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
