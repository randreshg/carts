///==========================================================================///
/// File: AnalysisManager.cpp
///
/// This file implements the AnalysisManager for centralized management
/// of all ARTS analysis objects with clean separation from graph management.
///==========================================================================///

#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/graphs/edt/EdtGraph.h"
#include "arts/analysis/graphs/edt/EdtNode.h"
#include "arts/analysis/loop/LoopNode.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

using namespace mlir;
using namespace mlir::arts;

AnalysisManager::AnalysisManager(ModuleOp module, const std::string &configFile,
                                 const std::string &metadataFile,
                                 PartitionFallback fallback)
    : module(module), configFile(configFile),
      metadataFile(metadataFile.empty() ? ".carts-metadata.json"
                                        : metadataFile),
      abstractMachine(configFile), partitionFallback(fallback) {}

AnalysisManager::~AnalysisManager() {}

void AnalysisManager::invalidate() {
  /// Invalidate dependents before dependencies:
  /// EdtAnalysis depends on DbAnalysis depends on LoopAnalysis
  if (epochAnalysis)
    epochAnalysis->invalidate();
  if (edtAnalysis)
    edtAnalysis->invalidate();
  if (dbAnalysis)
    dbAnalysis->invalidate();
  if (loopAnalysis)
    loopAnalysis->invalidate();
  if (stringAnalysis)
    stringAnalysis->invalidate();
  if (edtHeuristics)
    edtHeuristics->invalidate();
  if (dbHeuristics)
    dbHeuristics->clearDecisions();
  if (metadataManager)
    metadataManager->clear();
  metadataCoverage = MetadataCoverage{};
  cachedDiagnosticJson.reset();
}

DbAnalysis &AnalysisManager::getDbAnalysis() {
  if (!dbAnalysis)
    dbAnalysis = std::make_unique<DbAnalysis>(*this);
  return *dbAnalysis;
}

EdtAnalysis &AnalysisManager::getEdtAnalysis() {
  if (!edtAnalysis)
    edtAnalysis = std::make_unique<EdtAnalysis>(*this);
  return *edtAnalysis;
}

EpochAnalysis &AnalysisManager::getEpochAnalysis() {
  if (!epochAnalysis)
    epochAnalysis = std::make_unique<EpochAnalysis>(*this);
  return *epochAnalysis;
}

LoopAnalysis &AnalysisManager::getLoopAnalysis() {
  if (!loopAnalysis)
    loopAnalysis = std::make_unique<LoopAnalysis>(*this);
  return *loopAnalysis;
}

StringAnalysis &AnalysisManager::getStringAnalysis() {
  if (!stringAnalysis)
    stringAnalysis = std::make_unique<StringAnalysis>(*this);
  return *stringAnalysis;
}

MetadataManager &AnalysisManager::getMetadataManager() {
  if (!metadataManager) {
    metadataManager = std::make_unique<MetadataManager>(module.getContext());
    metadataManager->getIdRegistry().initializeFromModule(module);
    metadataManager->importFromJsonFile(module, metadataFile);
  }
  return *metadataManager;
}

const MetadataManager &AnalysisManager::getMetadataManager() const {
  assert(metadataManager && "Metadata manager not initialized. Call non-const "
                            "getMetadataManager() first.");
  return *metadataManager;
}

const StringAnalysis &AnalysisManager::getStringAnalysis() const {
  assert(stringAnalysis && "String analysis not initialized. Call non-const "
                           "getStringAnalysis() first.");
  return *stringAnalysis;
}

DbHeuristics &AnalysisManager::getDbHeuristics() {
  if (!dbHeuristics) {
    /// Ensure MetadataManager and IdRegistry are initialized first
    auto &idRegistry = getMetadataManager().getIdRegistry();
    dbHeuristics = std::make_unique<DbHeuristics>(abstractMachine, idRegistry,
                                                  partitionFallback);
  }
  return *dbHeuristics;
}

EdtHeuristics &AnalysisManager::getEdtHeuristics() {
  if (!edtHeuristics)
    edtHeuristics = std::make_unique<EdtHeuristics>(*this);
  return *edtHeuristics;
}

std::optional<DbAnalysis::LoopDbAccessSummary>
AnalysisManager::getLoopDbAccessSummary(Operation *loopOp) {
  if (!loopOp)
    return std::nullopt;
  auto forOp = dyn_cast<ForOp>(loopOp);
  if (!forOp)
    return std::nullopt;
  return getDbAnalysis().getLoopDbAccessSummary(forOp);
}

std::optional<EdtDistributionPattern>
AnalysisManager::getLoopDistributionPattern(Operation *loopOp) {
  if (!loopOp)
    return std::nullopt;
  auto forOp = dyn_cast<ForOp>(loopOp);
  if (!forOp)
    return std::nullopt;
  return getDbAnalysis().getLoopDistributionPattern(forOp);
}

void AnalysisManager::invalidateAndRebuildGraphs(ModuleOp module) {
  module.walk([&](func::FuncOp func) {
    invalidateFunction(func);
    (void)getDbAnalysis().getOrCreateGraph(func);
  });
}

bool AnalysisManager::invalidateFunction(func::FuncOp func) {
  bool invalidated = false;
  /// Invalidate dependents before dependencies
  if (epochAnalysis)
    epochAnalysis->invalidate();
  if (edtAnalysis)
    invalidated |= edtAnalysis->invalidateGraph(func);
  if (dbAnalysis)
    invalidated |= dbAnalysis->invalidateGraph(func);
  if (loopAnalysis)
    loopAnalysis->invalidate();
  if (stringAnalysis)
    stringAnalysis->invalidate();
  metadataCoverage = MetadataCoverage{};
  cachedDiagnosticJson.reset();
  return invalidated;
}

void AnalysisManager::print(llvm::raw_ostream &os) {
  os << "AnalysisManager for module: "
     << (module.getName() ? module.getName()->str() : "unnamed") << "\n";

  for (auto func : module.getOps<func::FuncOp>()) {
    os << "  Function: " << func.getName() << "\n";
    bool hasDbGraph = dbAnalysis != nullptr;
    bool hasEdtGraph = edtAnalysis != nullptr;
    os << "    DB Graph: " << (hasDbGraph ? "Available" : "Not built") << "\n";
    os << "    EDT Graph: " << (hasEdtGraph ? "Available" : "Not built")
       << "\n";
  }
}

void AnalysisManager::captureDiagnostics() {
  using namespace llvm::json;

  Object root;

  /// Version
  root["version"] = "1.0";

  /// Program information
  Object program;
  program["name"] = module.getName() ? module.getName()->str() : "unnamed";
  root["program"] = std::move(program);

  /// Machine configuration (expanded)
  Object machine;
  const auto &am = getAbstractMachine();
  machine["node_count"] = am.getNodeCount();
  machine["threads"] = am.getThreads();

  /// Execution mode as string
  switch (am.getExecutionMode()) {
  case ExecutionMode::SingleThreaded:
    machine["execution_mode"] = "SingleThreaded";
    break;
  case ExecutionMode::IntraNode:
    machine["execution_mode"] = "IntraNode";
    break;
  case ExecutionMode::InterNode:
    machine["execution_mode"] = "InterNode";
    break;
  }

  root["machine"] = std::move(machine);

  /// Unified entities array
  Array entities;
  DenseMap<Operation *, int64_t> loopToEdtMap;
  auto &idRegistry = getMetadataManager().getIdRegistry();
  bool capturedSourceFile = false;

  for (auto func : module.getOps<func::FuncOp>()) {
    if (!capturedSourceFile) {
      if (auto fileLoc = dyn_cast<FileLineColLoc>(func.getLoc())) {
        root["program"].getAsObject()->operator[]("source_file") =
            fileLoc.getFilename().str();
        capturedSourceFile = true;
      }
    }

    auto &edtGraph = getEdtAnalysis().getOrCreateEdtGraph(func);
    llvm::json::Value edtEntitiesValue =
        edtGraph.exportToJsonValue(/*includeAnalysis=*/true);
    if (auto *edtArray = edtEntitiesValue.getAsArray()) {
      for (auto &edt : *edtArray) {
        if (auto *obj = edt.getAsObject()) {
          (*obj)["entity_type"] = "edt";
          entities.push_back(std::move(edt));
        }
      }
    }
    edtGraph.forEachNode([&](NodeBase *node) {
      auto *edtNode = dyn_cast<EdtNode>(node);
      if (!edtNode)
        return;
      int64_t edtId = idRegistry.get(edtNode->getOp());
      if (edtId == 0)
        return;
      for (auto *loop : edtNode->getAssociatedLoops()) {
        loopToEdtMap[loop->getOp()] = edtId;
      }
    });

    auto &dbGraph = getDbAnalysis().getOrCreateGraph(func);
    llvm::json::Value dbEntitiesValue =
        dbGraph.exportToJsonValue(/*includeAnalysis=*/true);
    if (auto *dbArray = dbEntitiesValue.getAsArray()) {
      for (auto &db : *dbArray) {
        if (auto *obj = db.getAsObject()) {
          (*obj)["entity_type"] = "db";
          entities.push_back(std::move(db));
        }
      }
    }
  }

  /// Collect loops from MetadataManager (entity_type = "loop")
  /// Flattened properties - no nested static_analysis
  auto &mm = getMetadataManager();
  for (auto *loopOp : mm.getLoopOperations()) {
    auto *meta = mm.getLoopMetadata(loopOp);
    if (!meta)
      continue;

    Object loop;

    /// ARTS ID
    int64_t artsId = mm.getIdRegistry().get(loopOp);
    if (artsId != 0)
      loop["arts_id"] = artsId;

    loop["entity_type"] = "loop";

    /// Source location
    if (meta->locationMetadata.isValid())
      loop["source_location"] = meta->locationMetadata.file + ":" +
                                std::to_string(meta->locationMetadata.line);
    else
      loop["source_location"] = "unknown";

    /// Containing EDT (bidirectional relationship)
    if (auto it = loopToEdtMap.find(loopOp); it != loopToEdtMap.end())
      loop["containing_edt_id"] = it->second;

    /// Core loop properties (flattened - no nested static_analysis)
    loop["potentially_parallel"] = meta->potentiallyParallel;

    if (meta->tripCount)
      loop["trip_count"] = *meta->tripCount;
    if (meta->nestingLevel)
      loop["nesting_level"] = *meta->nestingLevel;

    /// Loop reordering legality (for ArtsMate to know if hint is safe)
    loop["reorder_legal"] = !meta->reorderNestTo.empty();

    entities.push_back(std::move(loop));
  }

  root["entities"] = std::move(entities);

  /// Export applied optimizations with PGO-style mapping for ArtsMate
  /// correlation Pattern: compile-time decision -> affected runtime DB IDs
  Array appliedOptimizations;
  for (const auto &decision : getDbHeuristics().getDecisions()) {
    if (!decision.applied)
      continue; /// Skip rejected - ArtsMate decides what to suggest

    Object d;
    d["target_id"] = decision.affectedArtsId;
    d["type"] = decision.heuristic;
    d["heuristic"] = decision.heuristic;

    /// PGO-style mapping: compile-time → runtime correlation
    if (decision.affectedAllocId != 0)
      d["alloc_id"] = decision.affectedAllocId;

    if (!decision.affectedDbIds.empty()) {
      Array dbIds;
      for (int64_t id : decision.affectedDbIds)
        dbIds.push_back(id);
      d["affected_db_ids"] = std::move(dbIds);
    }

    if (!decision.sourceLocation.empty())
      d["source_location"] = decision.sourceLocation;

    if (!decision.costModelInputs.empty()) {
      Object params;
      for (const auto &input : decision.costModelInputs)
        params[input.first()] = input.second;
      d["parameters"] = std::move(params);
    }

    appliedOptimizations.push_back(std::move(d));
  }
  root["applied_optimizations"] = std::move(appliedOptimizations);

  /// Store as cached JSON
  std::string jsonStr;
  llvm::raw_string_ostream stream(jsonStr);
  stream << llvm::json::Value(std::move(root));
  cachedDiagnosticJson = stream.str();
}

void AnalysisManager::exportToJson(llvm::raw_ostream &os,
                                   bool includeAnalysis) {
  using namespace llvm::json;

  if (!includeAnalysis) {
    Object root;
    root["module"] = module.getName() ? module.getName()->str() : "unnamed";
    root["valid"] = true;

    Array functions;
    for (auto func : module.getOps<func::FuncOp>()) {
      Object funcObj;
      funcObj["function"] = func.getName().str();
      Object graphs;
      graphs["db"] = "available";
      graphs["edt"] = "available";
      funcObj["graphs"] = std::move(graphs);
      functions.push_back(std::move(funcObj));
    }
    root["functions"] = std::move(functions);

    if (metadataCoverage.hasData) {
      Object coverage;
      coverage["loops_analyzed"] = metadataCoverage.loopsAnalyzed;
      coverage["loops_total"] = metadataCoverage.loopsTotal;
      coverage["memrefs_analyzed"] = metadataCoverage.memrefsAnalyzed;
      coverage["memrefs_total"] = metadataCoverage.memrefsTotal;
      root["metadata_coverage"] = std::move(coverage);
    }
    os << llvm::json::Value(std::move(root)) << "\n";
    return;
  }

  /// Use cached diagnostics if available
  if (cachedDiagnosticJson) {
    os << *cachedDiagnosticJson << "\n";
    return;
  }

  /// Fallback: generate on-demand
  captureDiagnostics();
  if (cachedDiagnosticJson)
    os << *cachedDiagnosticJson << "\n";
}
