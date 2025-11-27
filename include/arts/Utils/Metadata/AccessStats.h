///==========================================================================///
/// File: AccessStats.h
///
/// Shared access statistics struct used by LoopMetadata and MemrefMetadata.
/// Eliminates duplicate readCount/writeCount fields across metadata classes.
///==========================================================================///
#ifndef ARTS_UTILS_METADATA_ACCESSSTATS_H
#define ARTS_UTILS_METADATA_ACCESSSTATS_H

#include "llvm/Support/JSON.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace arts {

/// Shared access statistics for loops and memrefs
struct AccessStats {
  std::optional<int64_t> readCount;
  std::optional<int64_t> writeCount;
  std::optional<int64_t> totalAccesses;
  std::optional<double> readWriteRatio;

  /// Compute read/write ratio from counts
  void computeRatio() {
    if (readCount && writeCount) {
      int64_t total = *readCount + *writeCount;
      if (total > 0)
        readWriteRatio = static_cast<double>(*readCount) / total;
    }
  }

  /// Compute total accesses from counts
  void computeTotal() {
    if (readCount && writeCount)
      totalAccesses = *readCount + *writeCount;
  }

  /// Check if any stats are set
  bool hasStats() const {
    return readCount.has_value() || writeCount.has_value() ||
           totalAccesses.has_value();
  }

  /// Import from JSON (reads "read_count", "write_count", etc.)
  void importFromJson(const llvm::json::Object &json);

  /// Export to JSON (writes "read_count", "write_count", etc.)
  void exportToJson(llvm::json::Object &json) const;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_UTILS_METADATA_ACCESSSTATS_H
