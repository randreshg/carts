///==========================================================================///
/// File: DbDistributedEligibility.h
///
/// Eligibility analysis for distributed DB ownership marking.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_UTILS_DBDISTRIBUTEDELIGIBILITY_H
#define CARTS_DIALECT_ARTS_UTILS_DBDISTRIBUTEDELIGIBILITY_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include <optional>

namespace mlir {
namespace carts::arts {

enum class DistributedDbEligibilityRejectReason {
  None,
  NestedInEdt,
  GlobalAllocType,
  SingleBlock,
  UnsupportedShape,
  MissingPhysicalDbLayout,
  UnsupportedPhysicalDbLayoutForOwnerRoute,
  StencilReadInternodeUse,
  UnsupportedPtrUsers,
  UnsupportedGuidUsers,
  NonEdtAcquireUse,
  NoDistributedOwnerUse,
  /// Per-block all-gather replica: a block DB that must stay REPLICATED
  /// (every block on every node), not block-distributed. Distributing it would
  /// scatter the gathered blocks back across nodes and defeat the all-gather.
  PerBlockReplicated,
};

struct DistributedDbEligibilityResult {
  bool eligible = false;
  DistributedDbEligibilityRejectReason reason =
      DistributedDbEligibilityRejectReason::None;
  /// Optional distribution kind selected by the current DB/acquire facts.
  std::optional<EdtDistributionKind> distributionKind = std::nullopt;
};

const char *toString(DistributedDbEligibilityRejectReason reason);

DistributedDbEligibilityResult
evaluateDistributedDbEligibility(DbAllocOp alloc);

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_DBDISTRIBUTEDELIGIBILITY_H
