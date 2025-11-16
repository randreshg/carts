///==========================================================================///
/// File: ArtsAnalysis.h
///
/// Lightweight base interface for all ARTS analyses. Provides consistent
/// access to the shared ArtsAnalysisManager plus a standard invalidate hook.
///==========================================================================///

#ifndef ARTS_ANALYSIS_ARTSANALYSIS_H
#define ARTS_ANALYSIS_ARTSANALYSIS_H

namespace mlir {
namespace arts {

class ArtsAnalysisManager;

class ArtsAnalysis {
public:
  explicit ArtsAnalysis(ArtsAnalysisManager &manager) : manager(manager) {}
  virtual ~ArtsAnalysis() = default;

  ArtsAnalysisManager &getAnalysisManager() const { return manager; }

  virtual void invalidate() = 0;

protected:
  ArtsAnalysisManager &manager;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ARTSANALYSIS_H
