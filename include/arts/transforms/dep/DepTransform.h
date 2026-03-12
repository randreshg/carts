///==========================================================================///
/// File: DepTransform.h
///
/// Dependence-shape transforms that preserve program semantics while changing
/// the execution schedule seen by later ARTS passes.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DEPENDENCE_DEPENDENCETRANSFORM_H
#define ARTS_TRANSFORMS_DEPENDENCE_DEPENDENCETRANSFORM_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/StringRef.h"
#include <memory>

namespace mlir {
namespace arts {

class DepTransform {
public:
  virtual ~DepTransform() = default;

  /// Apply the transform across the module. Returns the number of rewrites.
  virtual int run(ModuleOp module) = 0;

  /// Name for logging/debugging.
  virtual StringRef getName() const = 0;
};

/// Rewrite Jacobi copy+stencil timestep loops into alternating-buffer stencil
/// phases before DB creation.
std::unique_ptr<DepTransform> createJacobiAlternatingBuffersPattern();

/// Rewrite Seidel-style in-place 2D stencils into a tiled wavefront schedule
/// after OpenMP-to-ARTS conversion so later task/DB passes see a scalable
/// dependence shape.
std::unique_ptr<DepTransform> createSeidel2DWavefrontPattern();

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DEPENDENCE_DEPENDENCETRANSFORM_H
