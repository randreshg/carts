///==========================================================================///
/// File: DbDistributedEligibility.h
///
/// Eligibility analysis for distributed DB ownership marking.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_DB_DBDISTRIBUTEDELIGIBILITY_H
#define ARTS_DIALECT_CORE_ANALYSIS_DB_DBDISTRIBUTEDELIGIBILITY_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include <optional>

namespace mlir {
namespace carts::arts {

class DbAnalysis;

enum class DistributedDbEligibilityRejectReason {
  None,
  NestedInEdt,
  GlobalAllocType,
  SingleBlock,
  UnsupportedShape,
  StencilReadInternodeUse,
  UnsupportedPtrUsers,
  UnsupportedGuidUsers,
  NonEdtAcquireUse,
  NoInternodeEdtUse,
  /// Per-block all-gather replica: a block DB that must stay REPLICATED
  /// (every block on every node), not block-distributed. Distributing it would
  /// scatter the gathered blocks back across nodes and defeat the all-gather.
  PerBlockReplicated,
};

struct DistributedDbEligibilityResult {
  bool eligible = false;
  DistributedDbEligibilityRejectReason reason =
      DistributedDbEligibilityRejectReason::None;
  /// When set, the pass should stamp this distribution kind on the alloc op.
  std::optional<EdtDistributionKind> distributionKind = std::nullopt;
};

const char *toString(DistributedDbEligibilityRejectReason reason);

DistributedDbEligibilityResult
evaluateDistributedDbEligibility(DbAllocOp alloc, DbAnalysis &dbAnalysis);

} // namespace carts::arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_DB_DBDISTRIBUTEDELIGIBILITY_H
