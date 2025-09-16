///==========================================================================
/// File: DbAliasAnalysis.h
/// Alias analysis helpers for DB nodes.
///==========================================================================

#ifndef ARTS_ANALYSIS_DB_DBALIASANALYSIS_H
#define ARTS_ANALYSIS_DB_DBALIASANALYSIS_H

#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"

namespace mlir {
namespace arts {
class DbAnalysis;
class NodeBase;
class DbAcquireNode;
class DbReleaseNode;
class DbAllocNode;

class DbAliasAnalysis {
public:
  explicit DbAliasAnalysis(DbAnalysis *analysis);

  bool mayAlias(const NodeBase &a, const NodeBase &b,
                const std::string &indent = "");

  Value getUnderlyingValue(const NodeBase &node);

private:
  DbAnalysis *analysis;
  DenseMap<std::pair<Value, Value>, bool> aliasCache;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBALIASANALYSIS_H