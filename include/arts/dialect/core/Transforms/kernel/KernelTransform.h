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

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_KERNEL_KERNELTRANSFORM_H
#define ARTS_DIALECT_CORE_TRANSFORMS_KERNEL_KERNELTRANSFORM_H

#include "arts/dialect/core/Transforms/loop/LoopNormalizer.h"
#include "arts/dialect/core/Transforms/pattern/PatternTransform.h"
#include <optional>

namespace mlir {
namespace arts {

class KernelPatternTransform : public LoopPattern {
public:
  ~KernelPatternTransform() override = default;
};

/// Create MatmulReductionPattern for dot-product -> k-j update distribution.
std::unique_ptr<KernelPatternTransform>
createMatmulReductionPattern(bool enableTiling, int64_t tileJ,
                             int64_t minTripCount);

/// Create an N-D stencil contract matcher for out-of-place affine stencils.
std::unique_ptr<KernelPatternTransform> createStencilTilingNDPattern();

/// Infer the canonical explicit stencil contract for one `arts.for` when the
/// loop is a valid out-of-place N-D stencil.
std::optional<StencilNDPatternContract>
matchExplicitStencilNDContract(ForOp artsFor, int64_t revision = 1);

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_KERNEL_KERNELTRANSFORM_H
