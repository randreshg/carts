///==========================================================================///
/// File: SDECostModel.h
///
/// Target-agnostic cost model for SDE optimization decisions.
/// The dialect boundary provides a concrete implementation. SDE passes see
/// ONLY this interface, never target object types.
///
/// All methods use SDE-level concepts: tasks, barriers, data movement.
/// Target object terminology belongs at the dialect boundary.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_SDECOSTMODEL_H
#define CARTS_DIALECT_SDE_UTILS_SDECOSTMODEL_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace mlir::carts::sde {

class SDECostModel {
public:
  virtual ~SDECostModel() = default;

  // --- Task lifecycle costs (normalized cycles) ---
  virtual double getTaskCreationCost() const = 0;
  virtual double getTaskSyncCost() const = 0;

  // --- Reduction costs ---
  virtual double getReductionCost(int64_t workerCount) const = 0;
  virtual double getAtomicUpdateCost() const = 0;

  // --- Generic memory-access cost ---
  virtual double getDataAccessCost() const = 0;

  // --- Abstract execution capacity ---
  virtual int getLogicalWorkerCapacity() const = 0;

  // --- Abstract execution topology ---
  // SDE may use these target-neutral locality groups to choose enough source
  // tasks for both intra-group and inter-group parallelism. The model is
  // intentionally abstract: target dialects map locality groups to concrete
  // concepts such as nodes, sockets, or accelerator islands, while SDE only
  // reasons about source-level work availability.
  virtual int getWorkerLocalityGroupCount() const { return 1; }

  // --- Derived thresholds (computed, not hardcoded) ---
  virtual int64_t getMinIterationsPerWorker() const {
    return std::max<int64_t>(1,
                             static_cast<int64_t>(getTaskCreationCost() /
                                                  (getDataAccessCost() + 1.0)));
  }

  // Owner-local pipeline compute units execute multiple local stages before
  // exposing completion. Keep their owner slices large enough to amortize task
  // lifecycle and synchronization, while scaling the floor sublinearly with
  // logical capacity so large runs do not create one tiny pipeline task per
  // worker.
  virtual int64_t getMinPipelineOwnerIterationsPerTask() const {
    int64_t lifecycleIterations =
        std::max<int64_t>(1, static_cast<int64_t>(std::ceil(
                                 (getTaskCreationCost() + getTaskSyncCost()) /
                                 (getDataAccessCost() + 1.0))));
    int64_t capacityIterations = std::max<int64_t>(
        1, static_cast<int64_t>(std::ceil(std::log2(
               static_cast<double>(std::max(2, getLogicalWorkerCapacity()))))));
    return std::max(
        {getMinIterationsPerWorker(), lifecycleIterations, capacityIterations});
  }

  virtual int64_t getInterLocalityTaskWaves() const {
    int64_t localityGroups =
        std::max<int64_t>(1, getWorkerLocalityGroupCount());
    if (localityGroups <= 1)
      return 1;
    // Cross-locality launches need at least one spare wave so every locality
    // can receive work while earlier tasks are paying cross-locality startup
    // costs. Scale sublinearly to avoid exploding tiny kernels.
    return std::max<int64_t>(
        2, 1 + static_cast<int64_t>(std::ceil(std::log2(static_cast<double>(
                   std::max<int64_t>(2, localityGroups))))));
  }

  virtual int64_t getOwnerLocalPipelineTargetTaskWaves() const {
    // Owner-local pipelines already perform multiple local stages per owner
    // slice. Additional launch waves increase downstream realization
    // pressure without exposing more machine concurrency: a single wave still
    // provides one task per logical worker when the owner domain is large
    // enough.
    return 1;
  }
};

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECOSTMODEL_H
