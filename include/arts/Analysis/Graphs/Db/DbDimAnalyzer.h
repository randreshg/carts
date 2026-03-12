///==========================================================================///
/// File: DbDimAnalyzer.h
///
/// Canonical per-entry / per-dimension partition facts for DbAcquireNode.
///==========================================================================///

#ifndef ARTS_ANALYSIS_GRAPHS_DB_DBDIMANALYZER_H
#define ARTS_ANALYSIS_GRAPHS_DB_DBDIMANALYZER_H

#include "arts/analysis/graphs/db/DbNode.h"

namespace mlir {
namespace arts {

class DbDimAnalyzer {
public:
  static DbAcquirePartitionFacts compute(DbAcquireNode *node);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_GRAPHS_DB_DBDIMANALYZER_H
