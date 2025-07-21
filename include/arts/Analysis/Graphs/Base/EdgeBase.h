//===----------------------------------------------------------------------===//
// EdgeBase.h - Abstract base class for graph edges
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_GRAPHS_EDGEBASE_H
#define ARTS_ANALYSIS_GRAPHS_EDGEBASE_H

#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {

class NodeBase;

/// Abstract base class for all edges (e.g., dep edges, alloc edges).
/// Provides common interface for from/to, type, and printing.
class EdgeBase {
public:
  virtual ~EdgeBase() = default;

  /// Get source and destination nodes.
  virtual NodeBase *getFrom() const = 0;
  virtual NodeBase *getTo() const = 0;

  /// Get the edge type (e.g., "Read", "Write", or custom).
  virtual llvm::StringRef getType() const = 0;

  /// Print edge details.
  virtual void print(llvm::raw_ostream &os) const = 0;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_EDGEBASE_H