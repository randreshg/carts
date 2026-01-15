///==========================================================================///
/// File: HeuristicBase.h
///
/// This file defines the abstract base class for all heuristics in the CARTS
/// compiler framework. Heuristics are used to make compile-time optimization
/// decisions based on program analysis and machine configuration.
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_HEURISTICBASE_H
#define ARTS_ANALYSIS_HEURISTICS_HEURISTICBASE_H

#include "llvm/ADT/StringRef.h"

namespace mlir::arts {

/// Abstract base class for all heuristics.
class HeuristicBase {
public:
  virtual ~HeuristicBase() = default;

  /// Unique name for diagnostics (e.g., "H1.1", "H1.2")
  virtual llvm::StringRef getName() const = 0;

  /// Human-readable description of what this heuristic does
  virtual llvm::StringRef getDescription() const = 0;

  /// Priority - higher priority heuristics are evaluated first.
  /// Range: 0-100, where 100 is highest priority.
  virtual int getPriority() const = 0;
};

} // namespace mlir::arts

#endif // ARTS_ANALYSIS_HEURISTICS_HEURISTICBASE_H
