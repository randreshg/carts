///==========================================================================///
/// File: KernelTransform.h
///
/// Public interface for kernel-form transformations.
///
/// These transforms preserve the mathematical result but change how a kernel
/// is expressed so later ARTS passes can distribute, partition, or localize it
/// more effectively. This is intentionally separate from dependence/schedule
/// rewrites in ArtsDepTransforms.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_KERNEL_KERNELTRANSFORM_H
#define ARTS_TRANSFORMS_KERNEL_KERNELTRANSFORM_H

#include "arts/Transforms/Loop/LoopNormalizer.h"

namespace mlir {
namespace arts {

/// Create MatmulReductionPattern for dot-product -> k-j update distribution.
std::unique_ptr<LoopPattern>
createMatmulReductionPattern(bool enableTiling = true, int64_t tileJ = 64,
                             int64_t minTripCount = 128);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_KERNEL_KERNELTRANSFORM_H
