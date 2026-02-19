///==========================================================================///
/// File: DistributedDbOwnershipPolicy.h
///
/// Eligibility policy for distributed DB ownership marking.
///==========================================================================///

#ifndef ARTS_ANALYSIS_DB_DISTRIBUTEDDBOWNERSHIPPOLICY_H
#define ARTS_ANALYSIS_DB_DISTRIBUTEDDBOWNERSHIPPOLICY_H

#include "arts/ArtsDialect.h"

namespace mlir {
namespace arts {

class DbAnalysis;

enum class DistributedDbOwnershipRejectReason {
  None,
  NestedInEdt,
  GlobalAllocType,
  SingleBlock,
  UnsupportedShape,
  StencilReadInternodeUse,
  ReadOnlyInternodeUse,
  UnsupportedPtrUsers,
  UnsupportedGuidUsers,
  NonEdtAcquireUse,
  NoInternodeEdtUse,
};

struct DistributedDbOwnershipEligibilityResult {
  bool eligible = false;
  DistributedDbOwnershipRejectReason reason =
      DistributedDbOwnershipRejectReason::None;
};

const char *toString(DistributedDbOwnershipRejectReason reason);

DistributedDbOwnershipEligibilityResult
evaluateDistributedDbOwnershipEligibility(DbAllocOp alloc,
                                          DbAnalysis &dbAnalysis);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DISTRIBUTEDDBOWNERSHIPPOLICY_H
