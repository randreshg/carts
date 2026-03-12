///==========================================================================///
/// File: DbDistributedEligibility.h
///
/// Eligibility analysis for distributed DB ownership marking.
///==========================================================================///

#ifndef ARTS_ANALYSIS_DB_DISTRIBUTEDDBELIGIBILITY_H
#define ARTS_ANALYSIS_DB_DISTRIBUTEDDBELIGIBILITY_H

#include "arts/Dialect.h"

namespace mlir {
namespace arts {

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
};

struct DistributedDbEligibilityResult {
  bool eligible = false;
  DistributedDbEligibilityRejectReason reason =
      DistributedDbEligibilityRejectReason::None;
};

const char *toString(DistributedDbEligibilityRejectReason reason);

DistributedDbEligibilityResult
evaluateDistributedDbEligibility(DbAllocOp alloc, DbAnalysis &dbAnalysis);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DISTRIBUTEDDBELIGIBILITY_H
