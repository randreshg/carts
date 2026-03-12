///==========================================================================///
/// File: Analysis.h
///
/// Lightweight base interface for all ARTS analyses. Provides consistent
/// access to the shared AnalysisManager plus a standard invalidate hook.
///==========================================================================///

#ifndef ARTS_ANALYSIS_ARTSANALYSIS_H
#define ARTS_ANALYSIS_ARTSANALYSIS_H

namespace mlir {
namespace arts {

class AnalysisManager;

class ArtsAnalysis {
public:
  explicit ArtsAnalysis(AnalysisManager &manager) : manager(manager) {}
  virtual ~ArtsAnalysis() = default;

  AnalysisManager &getAnalysisManager() const { return manager; }

  virtual void invalidate() = 0;

protected:
  AnalysisManager &manager;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ARTSANALYSIS_H
