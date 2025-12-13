///==========================================================================///
/// File: HeuristicsConfig.cpp
///
/// This file implements the HeuristicsConfig class which centralizes all
/// compile-time heuristic decision logic for single-rank and other
/// optimizations in the CARTS compiler framework.
///==========================================================================///

#include "arts/Analysis/HeuristicsConfig.h"
#include "arts/Utils/ArtsDebug.h"

ARTS_DEBUG_SETUP(heuristics)

using namespace mlir;
using namespace mlir::arts;

HeuristicsConfig::HeuristicsConfig(const ArtsAbstractMachine &machine)
    : machine(machine) {
  ARTS_DEBUG("HeuristicsConfig initialized: singleNode="
             << isSingleNode() << ", valid=" << isValid());
}

bool HeuristicsConfig::isSingleNode() const {
  return machine.getNodeCount() == 1;
}

bool HeuristicsConfig::isValid() const { return machine.isValid(); }

bool HeuristicsConfig::shouldDisableTwinDiff() const {
  // H4: On single-node, twin_diff is pure overhead
  // - No remote owner to send diffs to
  // - Twin allocation + memcpy + diff computation are wasted
  return isSingleNode();
}

bool HeuristicsConfig::shouldPreferCoarseForReadOnly(
    ArtsMode accessMode) const {
  // H1: Read-only memrefs on single-node should prefer coarse allocation
  // - No write contention to relieve via fine-graining
  // - Coarse reduces DB count and per-EDT dependency overhead
  return isSingleNode() && accessMode == ArtsMode::in;
}

bool HeuristicsConfig::shouldUseFineGrained(int64_t outerDBs,
                                            int64_t depsPerEDT,
                                            int64_t innerBytes) const {
  // H2: Cost model for single-node fine-grained allocation
  // Fine-grain only if all conditions are favorable:
  // - outerDBs not too large (DB metadata overhead)
  // - depsPerEDT not too large (acquireDbs() loop cost)
  // - innerBytes large enough (amortize per-DB overhead)

  if (!isSingleNode()) {
    // Multi-node: default to fine-grained if otherwise eligible
    return true;
  }

  bool acceptable = (outerDBs <= kMaxOuterDBs) &&
                    (depsPerEDT <= kMaxDepsPerEDT) &&
                    (innerBytes >= kMinInnerBytes);

  ARTS_DEBUG(
      "  H2 cost model: outerDBs="
      << outerDBs << " (max=" << kMaxOuterDBs << ")"
      << ", depsPerEDT=" << depsPerEDT << " (max=" << kMaxDepsPerEDT << ")"
      << ", innerBytes=" << innerBytes << " (min=" << kMinInnerBytes << ")"
      << " -> " << (acceptable ? "fine" : "coarse"));

  return acceptable;
}

void HeuristicsConfig::recordDecision(llvm::StringRef heuristic, bool applied,
                                      llvm::StringRef rationale, int64_t artsId,
                                      const llvm::StringMap<int64_t> &inputs) {
  decisions_.push_back(
      {heuristic.str(), applied, rationale.str(), artsId, inputs});
  ARTS_DEBUG("Recorded heuristic decision: " << heuristic
                                             << " applied=" << applied
                                             << " for ARTS ID " << artsId);
}

llvm::ArrayRef<HeuristicDecision> HeuristicsConfig::getDecisions() const {
  return decisions_;
}

void HeuristicsConfig::exportDecisionsToJson(llvm::json::OStream &J) const {
  J.attributeArray("heuristic_decisions", [&]() {
    for (const auto &decision : decisions_) {
      J.object([&]() {
        J.attribute("heuristic", decision.heuristic);
        J.attribute("applied", decision.applied);
        J.attribute("rationale", decision.rationale);
        J.attribute("affected_arts_id", decision.affectedArtsId);

        if (!decision.costModelInputs.empty()) {
          J.attributeObject("cost_model_inputs", [&]() {
            for (const auto &input : decision.costModelInputs) {
              J.attribute(input.first(), input.second);
            }
          });
        }
      });
    }
  });
}