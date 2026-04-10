///==========================================================================///
/// File: DepTransform.h
///
/// Dependence-shape transforms that preserve program semantics while changing
/// the execution schedule seen by later ARTS passes.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_DEP_DEPTRANSFORM_H
#define ARTS_DIALECT_CORE_TRANSFORMS_DEP_DEPTRANSFORM_H

#include "arts/dialect/core/Transforms/pattern/PatternTransform.h"

namespace mlir {
namespace arts {

/// Rewrite Jacobi copy+stencil timestep loops into alternating-buffer stencil
/// phases before DB creation.
std::unique_ptr<DepPatternTransform> createJacobiAlternatingBuffersPattern();

/// Rewrite Seidel-style in-place 2D stencils into a tiled wavefront schedule
/// after OpenMP-to-ARTS conversion so later task/DB passes see a scalable
/// dependence shape.
std::unique_ptr<DepPatternTransform> createSeidel2DWavefrontPattern();

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_DEP_DEPTRANSFORM_H
