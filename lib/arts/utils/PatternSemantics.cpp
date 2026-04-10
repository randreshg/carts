///==========================================================================///
/// PatternSemantics.cpp - Semantic pattern classification utilities
///
/// Centralizes pattern-family logic previously scattered across heuristics
/// and partitioning code.
///==========================================================================///

#include "arts/utils/PatternSemantics.h"
#include "arts/dialect/core/Analysis/heuristics/DistributionHeuristics.h"

namespace mlir {
namespace arts {

bool PatternSemantics::isStencilFamily(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::jacobi_alternating_buffers:
    return true;
  case ArtsDepPattern::unknown:
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::triangular:
  case ArtsDepPattern::matmul:
  case ArtsDepPattern::elementwise_pipeline:
    return false;
  }
  return false;
}

bool PatternSemantics::requiresHaloExchange(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::jacobi_alternating_buffers:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
    return true;
  case ArtsDepPattern::unknown:
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::triangular:
  case ArtsDepPattern::matmul:
  case ArtsDepPattern::elementwise_pipeline:
  case ArtsDepPattern::wavefront_2d:
    return false;
  }
  return false;
}

bool PatternSemantics::isUniformFamily(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
    return true;
  case ArtsDepPattern::unknown:
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::matmul:
  case ArtsDepPattern::triangular:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::jacobi_alternating_buffers:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
    return false;
  }
  return false;
}

bool PatternSemantics::isTriangularFamily(ArtsDepPattern pattern) {
  return pattern == ArtsDepPattern::triangular;
}

bool PatternSemantics::isMatmulFamily(ArtsDepPattern pattern) {
  return pattern == ArtsDepPattern::matmul;
}

bool PatternSemantics::isWavefrontFamily(ArtsDepPattern pattern) {
  return pattern == ArtsDepPattern::wavefront_2d;
}

std::optional<EdtDistributionPattern>
PatternSemantics::inferDistributionPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::unknown:
    return std::nullopt;
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
    return EdtDistributionPattern::uniform;
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::jacobi_alternating_buffers:
    return EdtDistributionPattern::stencil;
  case ArtsDepPattern::matmul:
    return EdtDistributionPattern::matmul;
  case ArtsDepPattern::triangular:
    return EdtDistributionPattern::triangular;
  }
  return std::nullopt;
}

std::optional<EdtDistributionKind>
PatternSemantics::getPreferredDistributionKind(
    ArtsDepPattern depPattern, const DistributionStrategy &strategy) {
  /// Matmul override: prefer tiling_2d for TwoLevel, block otherwise.
  if (isMatmulFamily(depPattern)) {
    if (strategy.kind == DistributionKind::TwoLevel)
      return EdtDistributionKind::tiling_2d;
    return EdtDistributionKind::block;
  }

  /// No other pattern-specific overrides at this layer.
  /// The caller (DistributionHeuristics) applies strategy-based dispatch.
  return std::nullopt;
}

bool PatternSemantics::prefersStencilCoarsening(ArtsDepPattern pattern) {
  /// Stencil-family patterns benefit from specialized strip coarsening.
  return isStencilFamily(pattern);
}

bool PatternSemantics::prefersStencilCoarsening(
    EdtDistributionPattern pattern) {
  /// Distribution-level stencil patterns also prefer strip coarsening.
  return pattern == EdtDistributionPattern::stencil;
}

} // namespace arts
} // namespace mlir
