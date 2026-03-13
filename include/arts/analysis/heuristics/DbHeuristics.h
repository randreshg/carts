///==========================================================================///
/// File: DbHeuristics.h
///
/// DB-specific heuristic policy for partitioning decisions.
///
/// Responsibility split:
///   - DbGraph / DbNode own canonical facts
///   - DbAnalysis exposes those facts
///   - DbHeuristics consumes prepared DB contexts/facts and returns policy
///     decisions only
///   - DbPartitioning remains the controller that applies those decisions
///==========================================================================///

#ifndef ARTS_ANALYSIS_HEURISTICS_DBHEURISTICS_H
#define ARTS_ANALYSIS_HEURISTICS_DBHEURISTICS_H

#include "arts/analysis/heuristics/PartitioningHeuristics.h"
#include "arts/utils/abstract_machine/AbstractMachine.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <string>

namespace mlir {

class Operation;

namespace arts {

class IdRegistry;
struct DbAcquirePartitionFacts;

struct HeuristicDecision {
  std::string heuristic;
  bool applied;
  std::string rationale;
  int64_t affectedArtsId;
  int64_t affectedAllocId;
  llvm::SmallVector<int64_t> affectedDbIds;
  std::string sourceLocation;
  llvm::StringMap<int64_t> costModelInputs;
};

class DbHeuristics {
public:
  DbHeuristics(const mlir::arts::AbstractMachine &machine,
               IdRegistry &idRegistry,
               PartitionFallback fallback = PartitionFallback::Coarse);

  bool isSingleNode() const;
  bool isValid() const;

  static constexpr int64_t kMaxOuterDBs = 1024;
  static constexpr int64_t kMaxDepsPerEDT = 8;
  static constexpr int64_t kMinInnerBytes = 64;

  PartitioningDecision choosePartitioning(const PartitioningContext &ctx);
  SmallVector<AcquireDecision>
  chooseAcquirePolicies(ArrayRef<const DbAcquirePartitionFacts *> acquireFacts);

  void recordDecision(llvm::StringRef heuristic, bool applied,
                      llvm::StringRef rationale, Operation *op,
                      const llvm::StringMap<int64_t> &inputs = {});
  llvm::ArrayRef<HeuristicDecision> getDecisions() const;
  void exportDecisionsToJson(llvm::json::OStream &J) const;
  void clearDecisions() { decisions.clear(); }

private:
  const mlir::arts::AbstractMachine &machine;
  IdRegistry &idRegistry;
  llvm::SmallVector<HeuristicDecision> decisions;
  PartitionFallback partitionFallback;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_HEURISTICS_DBHEURISTICS_H
