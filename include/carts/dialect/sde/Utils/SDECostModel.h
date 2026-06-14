///==========================================================================///
/// File: SDECostModel.h
///
/// Target-agnostic structural capacity model for SDE grain decisions.
/// Exposes logical worker capacity and locality topology only; iteration
/// amortization floors are fixed constants, not fabricated cycle costs.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_SDECOSTMODEL_H
#define CARTS_DIALECT_SDE_UTILS_SDECOSTMODEL_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mlir::carts::sde {

class SDECostModel {
public:
  virtual ~SDECostModel() = default;

  virtual int getLogicalWorkerCapacity() const = 0;

  virtual int getWorkerLocalityGroupCount() const { return 1; }

  /// Fixed amortization floor for per-worker tile grain (~2 iterations).
  virtual int64_t getMinIterationsPerWorker() const { return 2; }

  /// Owner-local pipeline slices must cover enough iterations to amortize
  /// multi-stage local work; scales sublinearly with logical capacity.
  virtual int64_t getMinPipelineOwnerIterationsPerTask() const {
    int64_t capacityFloor = std::max<int64_t>(
        1, static_cast<int64_t>(std::ceil(std::log2(static_cast<double>(
               std::max(2, getLogicalWorkerCapacity()))))));
    return std::max(getMinIterationsPerWorker(), capacityFloor);
  }

  /// Owner-local pipelines already perform multiple local stages per slice.
  virtual int64_t getOwnerLocalPipelineTargetTaskWaves() const { return 1; }

  /// Cross-locality launches need at least one spare wave when groups > 1.
  virtual int64_t getInterLocalityTaskWaves() const {
    int64_t localityGroups =
        std::max<int64_t>(1, getWorkerLocalityGroupCount());
    if (localityGroups <= 1)
      return 1;
    return std::max<int64_t>(
        2, 1 + static_cast<int64_t>(std::ceil(std::log2(static_cast<double>(
                   std::max<int64_t>(2, localityGroups))))));
  }
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECOSTMODEL_H
