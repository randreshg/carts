///==========================================================================///
/// File: KernelTransform.h
///
/// Public interface for kernel-form transformations.
///
/// These transforms preserve the mathematical result but change how a kernel
/// is expressed so later ARTS passes can distribute, partition, or localize it
/// more effectively. This is intentionally separate from dependence/schedule
/// rewrites in DepTransforms.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_KERNEL_KERNELTRANSFORM_H
#define ARTS_TRANSFORMS_KERNEL_KERNELTRANSFORM_H

#include "arts/transforms/loop/LoopNormalizer.h"

namespace mlir {
namespace arts {

class KernelPatternTransform : public LoopPattern {
public:
  ~KernelPatternTransform() override = default;
};

/// Create MatmulReductionPattern for dot-product -> k-j update distribution.
std::unique_ptr<KernelPatternTransform>
createMatmulReductionPattern(bool enableTiling = true, int64_t tileJ = 64,
                             int64_t minTripCount = 128);

/// Fuse consecutive sibling pointwise loops over the same iteration space into
/// a single pipeline loop so downstream passes see one uniform kernel family.
int applyElementwisePipelineTransform(ModuleOp module);

/// Create an N-D stencil contract matcher for out-of-place affine stencils.
std::unique_ptr<KernelPatternTransform> createStencilTilingNDPattern();

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_KERNEL_KERNELTRANSFORM_H
