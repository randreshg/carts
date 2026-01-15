///==========================================================================///
/// File: ArtsAnalysisManager.cpp
///
/// This file implements the ArtsAnalysisManager for centralized management
/// of all ARTS analysis objects with clean separation from graph management.
///==========================================================================///

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

using namespace mlir;
using namespace mlir::arts;

ArtsAnalysisManager::ArtsAnalysisManager(ModuleOp module,
                                         const std::string &configFile,
                                         const std::string &metadataFile)
    : module(module), configFile(configFile),
      metadataFile(metadataFile.empty() ? ".carts-metadata.json"
                                        : metadataFile),
      abstractMachine(configFile) {}

ArtsAnalysisManager::~ArtsAnalysisManager() {}

void ArtsAnalysisManager::invalidate() {
  if (dbAnalysis)
    dbAnalysis->invalidate();
  if (edtAnalysis)
    edtAnalysis->invalidate();
  if (loopAnalysis)
    loopAnalysis->invalidate();
  if (stringAnalysis)
    stringAnalysis->invalidate();
  if (metadataManager)
    metadataManager->clear();
}

DbAnalysis &ArtsAnalysisManager::getDbAnalysis() {
  if (!dbAnalysis)
    dbAnalysis = std::make_unique<DbAnalysis>(*this);
  return *dbAnalysis.get();
}

EdtAnalysis &ArtsAnalysisManager::getEdtAnalysis() {
  if (!edtAnalysis)
    edtAnalysis = std::make_unique<EdtAnalysis>(*this);
  return *edtAnalysis.get();
}

LoopAnalysis &ArtsAnalysisManager::getLoopAnalysis() {
  if (!loopAnalysis)
    loopAnalysis = std::make_unique<LoopAnalysis>(*this);
  return *loopAnalysis.get();
}

StringAnalysis &ArtsAnalysisManager::getStringAnalysis() {
  if (!stringAnalysis)
    stringAnalysis = std::make_unique<StringAnalysis>(*this);
  return *stringAnalysis;
}

ArtsMetadataManager &ArtsAnalysisManager::getMetadataManager() {
  if (!metadataManager) {
    metadataManager =
        std::make_unique<ArtsMetadataManager>(module.getContext());
    metadataManager->getIdRegistry().initializeFromModule(module);
    metadataManager->importFromJsonFile(module, metadataFile);
  }
  return *metadataManager;
}

const ArtsMetadataManager &ArtsAnalysisManager::getMetadataManager() const {
  assert(metadataManager && "Metadata manager not initialized. Call non-const "
                            "getMetadataManager() first.");
  return *metadataManager;
}

const StringAnalysis &ArtsAnalysisManager::getStringAnalysis() const {
  assert(stringAnalysis && "String analysis not initialized. Call non-const "
                           "getStringAnalysis() first.");
  return *stringAnalysis;
}

HeuristicsConfig &ArtsAnalysisManager::getHeuristicsConfig() {
  if (!heuristicsConfig) {
    /// Ensure MetadataManager and IdRegistry are initialized first
    auto &idRegistry = getMetadataManager().getIdRegistry();
    heuristicsConfig =
        std::make_unique<HeuristicsConfig>(abstractMachine, idRegistry);
  }
  return *heuristicsConfig;
}

DbGraph &ArtsAnalysisManager::getDbGraph(func::FuncOp func) {
  return getDbAnalysis().getOrCreateGraph(func);
}

EdtGraph &ArtsAnalysisManager::getEdtGraph(func::FuncOp func) {
  return getEdtAnalysis().getOrCreateEdtGraph(func);
}

bool ArtsAnalysisManager::invalidateFunction(func::FuncOp func) {
  bool invalidated = false;
  if (dbAnalysis)
    invalidated |= dbAnalysis->invalidateGraph(func);
  if (edtAnalysis)
    invalidated |= edtAnalysis->invalidateGraph(func);
  if (loopAnalysis)
    loopAnalysis->invalidate();
  if (stringAnalysis)
    stringAnalysis->invalidate();
  return invalidated;
}

void ArtsAnalysisManager::print(llvm::raw_ostream &os) {
  os << "ArtsAnalysisManager for module: "
     << (module.getName() ? module.getName()->str() : "unnamed") << "\n";

  for (auto func : module.getOps<func::FuncOp>()) {
    os << "  Function: " << func.getName() << "\n";
    os << "    DB Graph: Available\n";
    os << "    EDT Graph: Available\n";
  }
}

void ArtsAnalysisManager::captureDiagnostics() {
  using namespace llvm::json;

  Object root;

  /// Version
  root["version"] = "1.0";

  /// Program information
  Object program;
  program["name"] = module.getName() ? module.getName()->str() : "unnamed";
  /// Try to get source file from first function's location
  for (auto func : module.getOps<func::FuncOp>()) {
    auto loc = func.getLoc();
    if (auto fileLoc = dyn_cast<FileLineColLoc>(loc)) {
      program["source_file"] = fileLoc.getFilename().str();
      break;
    }
  }
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

  /// Collect EDTs from all functions (entity_type = "edt")
  for (auto func : module.getOps<func::FuncOp>()) {
    std::string edtJsonStr;
    llvm::raw_string_ostream edtStream(edtJsonStr);
    getEdtGraph(func).exportToJson(edtStream, /*includeAnalysis=*/true);

    auto edtArray = llvm::json::parse(edtJsonStr);
    if (edtArray && edtArray->getAsArray()) {
      for (auto &edt : *edtArray->getAsArray()) {
        if (auto *obj = edt.getAsObject()) {
          (*obj)["entity_type"] = "edt";
          entities.push_back(std::move(edt));
        }
      }
    }
  }

  /// Collect DBs from all functions (entity_type = "db")
  for (auto func : module.getOps<func::FuncOp>()) {
    std::string dbJsonStr;
    llvm::raw_string_ostream dbStream(dbJsonStr);
    getDbGraph(func).exportToJson(dbStream, /*includeAnalysis=*/true);

    auto dbArray = llvm::json::parse(dbJsonStr);
    if (dbArray && dbArray->getAsArray()) {
      for (auto &db : *dbArray->getAsArray()) {
        if (auto *obj = db.getAsObject()) {
          (*obj)["entity_type"] = "db";
          entities.push_back(std::move(db));
        }
      }
    }
  }

  /// Build reverse mapping: loop Operation* → containing EDT arts_id
  DenseMap<Operation *, int64_t> loopToEdtMap;
  auto &idRegistry = getMetadataManager().getIdRegistry();
  for (auto func : module.getOps<func::FuncOp>()) {
    auto &edtGraph = getEdtGraph(func);
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
    loop["was_reordered"] = false; /// TODO: Track if reordering was applied

    entities.push_back(std::move(loop));
  }

  root["entities"] = std::move(entities);

  /// Export applied optimizations with PGO-style mapping for ArtsMate
  /// correlation Pattern: compile-time decision -> affected runtime DB IDs
  Array appliedOptimizations;
  for (const auto &decision : getHeuristicsConfig().getDecisions()) {
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

void ArtsAnalysisManager::exportToJson(llvm::raw_ostream &os,
                                       bool includeAnalysis) {
  using namespace llvm::json;

  if (!includeAnalysis) {
    Object root;
    root["module"] = module.getName() ? module.getName()->str() : "unnamed";
    root["built"] = built;
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
