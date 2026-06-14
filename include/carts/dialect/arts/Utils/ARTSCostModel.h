///==========================================================================///
/// File: ARTSCostModel.h
///
/// ARTS-backed SDECostModel: maps runtime worker/node counts into SDE's
/// structural capacity interface. No fabricated task/access cycle costs.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_ANALYSIS_ARTSCOSTMODEL_H
#define CARTS_DIALECT_ARTS_ANALYSIS_ARTSCOSTMODEL_H

#include "carts/dialect/arts/Utils/RuntimeConfig.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"

#include <algorithm>

namespace mlir::carts::arts {

class ARTSCostModel : public carts::sde::SDECostModel {
  const RuntimeConfig &machine;

public:
  explicit ARTSCostModel(const RuntimeConfig &am) : machine(am) {}

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
