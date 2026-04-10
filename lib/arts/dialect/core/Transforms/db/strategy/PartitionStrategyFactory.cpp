///==========================================================================///
/// File: PartitionStrategyFactory.cpp
///
/// Factory implementation for creating partition strategies.
///==========================================================================///

#include "arts/transforms/db/PartitionStrategy.h"

using namespace mlir;
using namespace mlir::arts;

namespace mlir {
namespace arts {

llvm::SmallVector<std::unique_ptr<PartitionStrategy>>
PartitionStrategyFactory::createStandardStrategies() {
  llvm::SmallVector<std::unique_ptr<PartitionStrategy>> strategies;

  /// Priority order (lower = higher priority):
  ///   0:   CoarsePartitionStrategy   (H1.C0-H1.C5)
  ///   100: BlockPartitionStrategy    (H1.B1-H1.B6)
  ///   200: StencilPartitionStrategy  (H1.S0-H1.S2)
  ///   300: FineGrainedPartitionStrategy (H1.E1-H1.E3)
  strategies.push_back(createCoarseStrategy());
  strategies.push_back(createBlockStrategy());
  strategies.push_back(createStencilStrategy());
  strategies.push_back(createFineGrainedStrategy());

  return strategies;
}

} // namespace arts
} // namespace mlir
