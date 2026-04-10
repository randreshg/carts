///==========================================================================///
/// File: DbPartitionState.h
///
/// Formalized partition state with explicit Known/Assumed distinction.
/// Follows the Attributor discipline: facts live in explicit state objects,
/// combines are monotone, and IR-visible vs transient state are separate.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_DB_DBPARTITIONSTATE_H
#define ARTS_DIALECT_CORE_ANALYSIS_DB_DBPARTITIONSTATE_H

#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/graphs/db/DbAccessPattern.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir {
namespace arts {

/// Provenance of a partition property -- where did this fact come from?
enum class FactProvenance : uint8_t {
  /// Proved from IR contract attributes (arts.lowering_contract).
  IRContract,
  /// Inferred from graph-backed analysis (DbAcquireNode, PartitionBounds).
  GraphInferred,
  /// Assumed from heuristic context (no evidence, best guess).
  HeuristicAssumption,
};

/// A single partition property with provenance tracking.
template <typename T> struct ProvenancedFact {
  T value;
  FactProvenance provenance;

  /// A fact from IR contracts is "known" (conservative).
  bool isKnown() const { return provenance == FactProvenance::IRContract; }

  /// A fact from heuristics is "assumed" (optimistic).
  bool isAssumed() const {
    return provenance == FactProvenance::HeuristicAssumption;
  }

  /// A fact from graph analysis is intermediate.
  bool isGraphInferred() const {
    return provenance == FactProvenance::GraphInferred;
  }

  /// Strengthen: replace an assumed/inferred fact with a known one.
  /// Returns true if the provenance actually changed.
  bool strengthen(T newValue, FactProvenance newProvenance) {
    if (newProvenance < provenance) {
      value = newValue;
      provenance = newProvenance;
      return true;
    }
    return false;
  }
};

/// Formalized partition state for a DB allocation.
/// Invariant: Known <= Assumed (known facts are always a subset of assumed).
///
/// This struct is declarative infrastructure for future adoption.
/// Current code continues to use LoweringContractInfo + graph queries.
/// Migration path: passes gradually adopt ProvenancedFact to distinguish
/// between IR-proved and heuristic-assumed properties.
struct DbPartitionState {
  /// Partition mode (coarse, block, fine-grained, etc.)
  std::optional<ProvenancedFact<PartitionMode>> partitionMode;

  /// Owner dimensions (which dims are partitioned).
  std::optional<ProvenancedFact<SmallVector<int64_t, 4>>> ownerDims;

  /// Block shape (static block sizes per dim).
  std::optional<ProvenancedFact<SmallVector<int64_t, 4>>> blockShape;

  /// Halo offsets (stencil min/max).
  std::optional<ProvenancedFact<SmallVector<int64_t, 4>>> minOffsets;
  std::optional<ProvenancedFact<SmallVector<int64_t, 4>>> maxOffsets;

  /// Access pattern (uniform, stencil, indexed, etc.)
  std::optional<ProvenancedFact<AccessPattern>> accessPattern;

  /// Check if all essential properties are known from IR contracts.
  bool isFullyKnown() const {
    return partitionMode && partitionMode->isKnown() && ownerDims &&
           ownerDims->isKnown();
  }

  /// Check if the state has any assumed (unproved) properties.
  bool hasAssumedProperties() const {
    return (partitionMode && partitionMode->isAssumed()) ||
           (ownerDims && ownerDims->isAssumed()) ||
           (blockShape && blockShape->isAssumed()) ||
           (accessPattern && accessPattern->isAssumed());
  }

  /// Monotone meet: narrow assumed state toward known.
  /// Returns true if any property was strengthened.
  bool meet(const DbPartitionState &other) {
    bool changed = false;
    changed |= meetField(partitionMode, other.partitionMode);
    changed |= meetField(ownerDims, other.ownerDims);
    changed |= meetField(blockShape, other.blockShape);
    changed |= meetField(minOffsets, other.minOffsets);
    changed |= meetField(maxOffsets, other.maxOffsets);
    changed |= meetField(accessPattern, other.accessPattern);
    return changed;
  }

private:
  template <typename T>
  static bool meetField(std::optional<ProvenancedFact<T>> &dst,
                        const std::optional<ProvenancedFact<T>> &src) {
    if (!src)
      return false;
    if (!dst) {
      dst = *src;
      return true;
    }
    return dst->strengthen(src->value, src->provenance);
  }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_DB_DBPARTITIONSTATE_H
