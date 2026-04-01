///==========================================================================///
/// File: DepTransform.h
///
/// Dependence-shape transforms that preserve program semantics while changing
/// the execution schedule seen by later ARTS passes.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DEPENDENCE_DEPENDENCETRANSFORM_H
#define ARTS_TRANSFORMS_DEPENDENCE_DEPENDENCETRANSFORM_H

#include "arts/transforms/pattern/PatternTransform.h"

namespace mlir {
namespace arts {

class MetadataManager;

/// Rewrite Jacobi copy+stencil timestep loops into alternating-buffer stencil
/// phases before DB creation.
std::unique_ptr<DepPatternTransform> createJacobiAlternatingBuffersPattern();

/// Rewrite Seidel-style in-place 2D stencils into a tiled wavefront schedule
/// after OpenMP-to-ARTS conversion so later task/DB passes see a scalable
/// dependence shape.
std::unique_ptr<DepPatternTransform>
createSeidel2DWavefrontPattern(MetadataManager &metadataManager);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DEPENDENCE_DEPENDENCETRANSFORM_H
