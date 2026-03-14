///==========================================================================///
/// File: AnalysisManager.h
///
/// This file defines the AnalysisManager for centralized management
/// of all ARTS analysis objects.
///==========================================================================///

#ifndef ARTS_ANALYSIS_ARTSANALYSISMANAGER_H
#define ARTS_ANALYSIS_ARTSANALYSISMANAGER_H

#include "arts/analysis/StringAnalysis.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/edt/EdtAnalysis.h"
#include "arts/analysis/heuristics/DbHeuristics.h"
#include "arts/analysis/heuristics/EdtHeuristics.h"
#include "arts/analysis/loop/LoopAnalysis.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/utils/abstract_machine/AbstractMachine.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include <memory>
#include <optional>

namespace mlir {

class Pass;

namespace arts {

/// Centralized manager for all ARTS analysis objects.
class AnalysisManager {
public:
  AnalysisManager(
      ModuleOp module, const std::string &configFile = "",
      const std::string &metadataFile = "",
      PartitionFallback partitionFallback = PartitionFallback::Coarse);
  ~AnalysisManager();

  /// Invalidate all analysis objects and graphs
  void invalidate();

  /// Get analysis objects
  DbAnalysis &getDbAnalysis();
  EdtAnalysis &getEdtAnalysis();
  LoopAnalysis &getLoopAnalysis();
  StringAnalysis &getStringAnalysis();
  DbHeuristics &getDbHeuristics();
  EdtHeuristics &getEdtHeuristics();

  /// Unified analysis queries spanning EDT and DB analyses.
  std::optional<DbAccessPattern> getDbAllocAccessPattern(DbAllocOp alloc);
  std::optional<DbAnalysis::LoopDbAccessSummary>
  getLoopDbAccessSummary(Operation *loopOp);
  std::optional<EdtDistributionPattern>
  getLoopDistributionPattern(Operation *loopOp);

  /// Invalidate and rebuild DB graphs for all functions in the module.
  void invalidateAndRebuildGraphs(ModuleOp module);

  /// Other functions
  bool invalidateFunction(func::FuncOp func);

  ModuleOp &getModule() { return module; }
  const std::string &getConfigFile() const { return configFile; }
  const std::string &getMetadataFile() const { return metadataFile; }

  /// Get the ARTS abstract machine
  AbstractMachine &getAbstractMachine() { return abstractMachine; }
  const AbstractMachine &getAbstractMachine() const { return abstractMachine; }

  /// Get the metadata manager
  MetadataManager &getMetadataManager();
  const MetadataManager &getMetadataManager() const;
  const StringAnalysis &getStringAnalysis() const;

  /// Print summary of analysis objects and their graphs
  void print(llvm::raw_ostream &os);

  /// Capture diagnostic data
  void captureDiagnostics();

  /// Export analysis objects and graphs to JSON
  void exportToJson(llvm::raw_ostream &os, bool includeAnalysis = false);

  /// Metadata coverage data
  struct MetadataCoverage {
    int64_t loopsAnalyzed = 0;
    int64_t loopsTotal = 0;
    int64_t memrefsAnalyzed = 0;
    int64_t memrefsTotal = 0;
    bool hasData = false;
  };

  /// Set metadata coverage data
  void setMetadataCoverage(int64_t loopsAnalyzed, int64_t loopsTotal,
                           int64_t memrefsAnalyzed, int64_t memrefsTotal) {
    metadataCoverage.loopsAnalyzed = loopsAnalyzed;
    metadataCoverage.loopsTotal = loopsTotal;
    metadataCoverage.memrefsAnalyzed = memrefsAnalyzed;
    metadataCoverage.memrefsTotal = memrefsTotal;
    metadataCoverage.hasData = true;
  }

  /// Get metadata coverage data
  const MetadataCoverage &getMetadataCoverage() const {
    return metadataCoverage;
  }

private:
  ModuleOp module;
  std::string configFile;
  std::string metadataFile;
  AbstractMachine abstractMachine;
  PartitionFallback partitionFallback;
  std::unique_ptr<DbAnalysis> dbAnalysis;
  std::unique_ptr<EdtAnalysis> edtAnalysis;
  std::unique_ptr<DbHeuristics> dbHeuristics;
  std::unique_ptr<EdtHeuristics> edtHeuristics;
  std::unique_ptr<LoopAnalysis> loopAnalysis;
  std::unique_ptr<StringAnalysis> stringAnalysis;
  std::unique_ptr<MetadataManager> metadataManager;

  /// Cached diagnostic data
  std::optional<std::string> cachedDiagnosticJson;

  /// Metadata coverage from VerifyMetadataPass
  MetadataCoverage metadataCoverage;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ARTSANALYSISMANAGER_H
