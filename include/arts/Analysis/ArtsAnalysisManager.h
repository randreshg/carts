///==========================================================================///
/// File: ArtsAnalysisManager.h
///
/// This file defines the ArtsAnalysisManager for centralized management
/// of all ARTS analysis objects. Provides a clean interface that exposes
/// only analysis objects, while internally managing graphs through
/// the respective analysis classes.
///==========================================================================///

#ifndef ARTS_ANALYSIS_ARTSANALYSISMANAGER_H
#define ARTS_ANALYSIS_ARTSANALYSISMANAGER_H

#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/HeuristicsConfig.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Analysis/StringAnalysis.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include <memory>

namespace mlir {

class Pass;

namespace arts {

class DbGraph;
class EdtGraph;

/// Centralized manager for all ARTS analysis objects.
class ArtsAnalysisManager {
public:
  ArtsAnalysisManager(ModuleOp module, const std::string &configFile = "",
                      const std::string &metadataFile = "");
  ~ArtsAnalysisManager();

  /// Invalidate all analysis objects and graphs
  void invalidate();

  /// Get analysis objects
  DbAnalysis &getDbAnalysis();
  EdtAnalysis &getEdtAnalysis();
  LoopAnalysis &getLoopAnalysis();
  StringAnalysis &getStringAnalysis();
  HeuristicsConfig &getHeuristicsConfig();

  /// Graph getters
  DbGraph &getDbGraph(func::FuncOp func);
  EdtGraph &getEdtGraph(func::FuncOp func);

  /// Per-function invalidation
  bool invalidateFunction(func::FuncOp func);

  /// Get the module containing the function
  ModuleOp &getModule() { return module; }

  /// Get the configuration file path
  const std::string &getConfigFile() const { return configFile; }

  /// Get the metadata file path
  const std::string &getMetadataFile() const { return metadataFile; }

  /// Get the ARTS abstract machine
  ArtsAbstractMachine &getAbstractMachine() { return abstractMachine; }
  const ArtsAbstractMachine &getAbstractMachine() const {
    return abstractMachine;
  }

  /// Get the metadata manager
  ArtsMetadataManager &getMetadataManager();
  const ArtsMetadataManager &getMetadataManager() const;
  const StringAnalysis &getStringAnalysis() const;

  /// Print summary of analysis objects and their graphs
  void print(llvm::raw_ostream &os);

  /// Capture diagnostic data
  void captureDiagnostics();

  /// Export analysis objects and graphs to JSON
  void exportToJson(llvm::raw_ostream &os, bool includeAnalysis = false);

  /// Metadata coverage data (set by VerifyMetadataPass)
  struct MetadataCoverage {
    int64_t loopsAnalyzed = 0;
    int64_t loopsTotal = 0;
    int64_t memrefsAnalyzed = 0;
    int64_t memrefsTotal = 0;
    bool hasData = false;
  };

  /// Set metadata coverage data (called by VerifyMetadataPass)
  void setMetadataCoverage(int64_t loopsAnalyzed, int64_t loopsTotal,
                           int64_t memrefsAnalyzed, int64_t memrefsTotal) {
    metadataCoverage_.loopsAnalyzed = loopsAnalyzed;
    metadataCoverage_.loopsTotal = loopsTotal;
    metadataCoverage_.memrefsAnalyzed = memrefsAnalyzed;
    metadataCoverage_.memrefsTotal = memrefsTotal;
    metadataCoverage_.hasData = true;
  }

  /// Get metadata coverage data
  const MetadataCoverage &getMetadataCoverage() const {
    return metadataCoverage_;
  }

private:
  ModuleOp module;
  std::string configFile;
  std::string metadataFile;
  ArtsAbstractMachine abstractMachine;
  bool built = false;

  std::unique_ptr<DbAnalysis> dbAnalysis;
  std::unique_ptr<EdtAnalysis> edtAnalysis;
  std::unique_ptr<HeuristicsConfig> heuristicsConfig;
  std::unique_ptr<LoopAnalysis> loopAnalysis;
  std::unique_ptr<StringAnalysis> stringAnalysis;
  std::unique_ptr<ArtsMetadataManager> metadataManager;

  /// Cached diagnostic data (captured before LLVM lowering)
  std::optional<std::string> cachedDiagnosticJson;

  /// Metadata coverage from VerifyMetadataPass
  MetadataCoverage metadataCoverage_;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ARTSANALYSISMANAGER_H
