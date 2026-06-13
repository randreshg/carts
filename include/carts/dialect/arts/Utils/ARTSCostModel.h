///==========================================================================///
/// File: ARTSCostModel.h
///
/// ARTS-backed implementation of SDECostModel. SDE passes consume only the
/// runtime-neutral SDECostModel interface. This adapter may use the configured
/// ARTS runtime to expose total logical worker capacity and abstract locality
/// groups. SDE uses those values only for source-level grain and task-wave
/// decisions; ARTS still owns DB ownership, EDT placement, routes, and runtime
/// memory-model choices after ARTS realization.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_ANALYSIS_ARTSCOSTMODEL_H
#define CARTS_DIALECT_ARTS_ANALYSIS_ARTSCOSTMODEL_H

#include "carts/dialect/arts/Utils/RuntimeConfig.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include <algorithm>
#include <cmath>

namespace mlir::carts::arts {

class ARTSCostModel : public carts::sde::SDECostModel {
  const RuntimeConfig &machine;

public:
  explicit ARTSCostModel(const RuntimeConfig &am) : machine(am) {}

  // --- Task lifecycle (generic worker-level transform costs) ---
  double getTaskCreationCost() const override { return 1800.0; }
  double getTaskSyncCost() const override { return 3000.0; }

  // --- Reduction (maps SDE reductions -> ARTS atomics/trees) ---
  double getReductionCost(int64_t workerCount) const override {
    double treeCost = std::log2(workerCount) * getTaskSyncCost();
    double linearCost = workerCount * getAtomicUpdateCost();
    return std::min(treeCost, linearCost);
  }
  double getAtomicUpdateCost() const override { return 100.0; }

  // --- Generic memory access (no placement/topology scope) ---
  double getDataAccessCost() const override { return 500.0; }



  // --- Abstract execution capacity ---
  int getLogicalWorkerCapacity() const override {
    return machine.getRuntimeTotalWorkers();
  }

  int getWorkerLocalityGroupCount() const override {
    return std::max(1, machine.getNodeCount());
  }

  int64_t getMinIterationsPerWorker() const override {
    int configured = machine.getMinIterationsPerWorker();
    if (configured > 0)
      return configured;
    return carts::sde::SDECostModel::getMinIterationsPerWorker();
  }
};

} // namespace mlir::carts::arts

#endif // CARTS_DIALECT_ARTS_ANALYSIS_ARTSCOSTMODEL_H
