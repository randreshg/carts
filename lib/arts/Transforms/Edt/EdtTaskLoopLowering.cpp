///==========================================================================///
/// File: EdtTaskLoopLowering.cpp
///
/// Factory for strategy-specific task loop lowerers.
///==========================================================================///

#include "arts/Transforms/Edt/EdtTaskLoopLowering.h"

using namespace mlir;
using namespace mlir::arts;

std::unique_ptr<EdtTaskLoopLowering>
EdtTaskLoopLowering::create(DistributionKind kind) {
  switch (kind) {
  case DistributionKind::Flat:
  case DistributionKind::TwoLevel:
    return detail::createBlockTaskLoopLowering();
  case DistributionKind::BlockCyclic:
    return detail::createBlockCyclicTaskLoopLowering();
  case DistributionKind::Tiling2D:
    return detail::createTiling2DTaskLoopLowering();
  }
  return detail::createBlockTaskLoopLowering();
}
