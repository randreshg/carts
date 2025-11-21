///==========================================================================///
/// File: ArtsAnalysisManager.cpp
///
/// This file implements the ArtsAnalysisManager for centralized management
/// of all ARTS analysis objects with clean separation from graph management.
///==========================================================================///

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

using namespace mlir;
using namespace mlir::arts;

ArtsAnalysisManager::ArtsAnalysisManager(ModuleOp module,
                                         const std::string &configFile)
    : module(module), configFile(configFile), abstractMachine(configFile) {}

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
    metadataManager->importFromJsonFile(module, ".carts-metadata.json");
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

void ArtsAnalysisManager::exportToJson(llvm::raw_ostream &os,
                                       bool includeAnalysis) {
  using namespace llvm::json;

  Object root;
  root["module"] = module.getName() ? module.getName()->str() : "unnamed";
  root["built"] = built;
  root["valid"] = true;

  /// Export graphs via analysis objects
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
}
