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