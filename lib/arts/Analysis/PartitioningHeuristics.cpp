///==========================================================================///
/// File: PartitioningHeuristics.cpp
///
/// H1 partitioning heuristics evaluation and PartitioningHint utilities.
///==========================================================================///

#include "arts/Analysis/PartitioningHeuristics.h"
#include "arts/Analysis/ArtsHeuristics.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/IR/BuiltinAttributes.h"

ARTS_DEBUG_SETUP(partitioning_heuristics)

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// H1: Partitioning Heuristics Evaluation
///===----------------------------------------------------------------------===///

PartitioningDecision
mlir::arts::evaluatePartitioningHeuristics(const PartitioningContext &ctx,
                                           const ArtsAbstractMachine *machine,
                                           PartitionFallback fallback) {
  ARTS_DEBUG("evaluatePartitioningHeuristics: canElementWise="
             << ctx.canElementWise << ", canBlock=" << ctx.canBlock
             << ", pinnedDimCount=" << ctx.pinnedDimCount
             << ", accessMode=" << static_cast<int>(ctx.accessMode)
             << ", allBlockFullRange=" << ctx.allBlockFullRange);

  const auto &patterns = ctx.accessPatterns;

  bool isReadOnly = !ctx.acquires.empty() ? ctx.allReadOnly()
                                          : (ctx.accessMode == ArtsMode::in);
  bool isSingleNode = machine && machine->getNodeCount() == 1;
  bool hasExplicitFineGrained =
      llvm::any_of(ctx.acquires, [](const AcquireInfo &info) {
        if (info.partitionMode == PartitionMode::fine_grained)
          return true;
        for (const auto &pinfo : info.partitionInfos) {
          if (pinfo.isFineGrained() && !pinfo.indices.empty())
            return true;
        }
        return false;
      });

  /// H1.1: Read-Only Single-Node -> Coarse
  if (isSingleNode) {
    if (isReadOnly && !ctx.canBlock && !ctx.canElementWise) {
      ARTS_DEBUG("H1.1 applied: Read-only single-node prefers coarse");
      return PartitioningDecision::coarse(
          ctx, "H1.1: All acquires read-only on single-node prefers coarse");
    }

    /// H1.1a: Any explicit coarse acquire on single-node -> Coarse.
    /// A later whole-array consumer outweighs any earlier parallel producer on
    /// a single node. Preserving coarse allocation avoids mixed-mode
    /// block-localization for values that are fundamentally consumed as a
    /// single read-mostly input.
    if (ctx.anyCoarseAcquire()) {
      ARTS_DEBUG(
          "H1.1a applied: Single-node coarse acquire prefers coarse");
      return PartitioningDecision::coarse(
          ctx, "H1.1a: Single-node coarse acquire prefers coarse");
    }

    /// H1.1b: Read-only + all full-range block acquires -> Coarse
    if (isReadOnly && ctx.allBlockFullRange) {
      ARTS_DEBUG("H1.1b applied: Read-only full-range acquires prefer coarse");
      return PartitioningDecision::coarse(
          ctx,
          "H1.1b: Read-only full-range acquires on single-node prefer coarse");
    }
  }

  /// H1.1c: Read-only + full-range block acquires on multi-node -> Coarse.
  /// When every read acquire spans all blocks, block partitioning gives no
  /// locality benefit and can inflate dependency wiring.
  if (!isSingleNode && isReadOnly && ctx.allBlockFullRange) {
    ARTS_DEBUG("H1.1c applied: Read-only full-range acquires on multi-node "
               "prefer coarse");
    return PartitioningDecision::coarse(
        ctx,
        "H1.1c: Read-only full-range acquires on multi-node prefer coarse");
  }

  /// H1.2: Mixed Access (Block Writes + Indirect Reads) -> Block
  if (ctx.canBlock && ctx.hasIndirectRead && !ctx.hasIndirectWrite &&
      ctx.hasDirectAccess) {
    ARTS_DEBUG("H1.2 applied: Mixed access pattern");
    return PartitioningDecision::block(
        ctx, "H1.2: Mixed access (block writes + full-range indirect reads)");
  }

  /// H1.2b: Mixed Access (Direct + Indirect Writes) -> Block
  if (ctx.canBlock && ctx.hasIndirectWrite && ctx.hasDirectAccess) {
    ARTS_DEBUG("H1.2b applied: Mixed direct+indirect writes");
    return PartitioningDecision::block(
        ctx,
        "H1.2b: Mixed direct+indirect writes prefers block with full-range "
        "indirect acquires");
  }

  /// H1.3: Stencil/Indexed Patterns
  if (patterns.hasStencil) {
    if (hasExplicitFineGrained) {
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      ARTS_DEBUG("H1.3 applied: Stencil with explicit dependences prefers "
                 "element-wise");
      return PartitioningDecision::elementWise(
          ctx, outerRank,
          "H1.3: Stencil with explicit dependences prefers element-wise");
    }
    if (isSingleNode && isReadOnly) {
      ARTS_DEBUG(
          "H1.3 applied: Read-only stencil on single-node prefers coarse");
      return PartitioningDecision::coarse(
          ctx, "H1.3: Read-only stencil on single-node prefers coarse");
    }

    if (!ctx.canBlock || ctx.hasIndirectAccess) {
      ARTS_DEBUG("H1.3 applied: Stencil but block unsupported; fallback");
      return PartitioningDecision::elementWise(
          ctx, 1,
          "H1.3: Stencil detected but block unsupported/indirect; fallback to "
          "element-wise");
    }

    ARTS_DEBUG("H1.3 applied: Stencil uses ESD mode");
    return PartitioningDecision::stencil(
        ctx, patterns.hasUniform
                 ? "H1.3: Mixed (uniform+stencil) uses Stencil mode - block "
                   "handles both"
                 : "H1.3: Pure stencil uses ESD mode");
  }

  if (patterns.hasIndexed) {
    if (ctx.canBlock) {
      ARTS_DEBUG("H1.3 applied: Indexed access prefers block when supported");
      return PartitioningDecision::block(
          ctx, "H1.3: Indexed access prefers block when supported");
    }

    unsigned minDim = ctx.minPinnedDimCount();
    unsigned maxDim = ctx.maxPinnedDimCount();
    unsigned outerRank = minDim > 0 ? minDim : 1;

    if (minDim != maxDim && minDim > 0 && maxDim > 0) {
      ARTS_DEBUG("H1.3: Non-uniform partition indices (min="
                 << minDim << ", max=" << maxDim << "), using min");
    }
    ARTS_DEBUG("H1.3 applied: Indexed access requires element-wise (outerRank="
               << outerRank << ")");
    return PartitioningDecision::elementWise(
        ctx, outerRank, "H1.3: Indexed access requires element-wise");
  }

  /// H1.4: Uniform direct access -> Block
  bool blockSizeFits = !ctx.totalElements || !ctx.blockSize ||
                       *ctx.totalElements >= *ctx.blockSize;
  if (ctx.canBlock && ctx.hasDirectAccess && !ctx.hasIndirectAccess &&
      patterns.hasUniform && !ctx.elementTypeIsMemRef && blockSizeFits) {
    ARTS_DEBUG("H1.4 applied: Uniform direct access prefers block");
    return PartitioningDecision::block(
        ctx, "H1.4: Uniform direct access prefers block");
  }

  /// H1.5: Multi-Node -> Fine-Grained for Network Efficiency
  if (machine && machine->getNodeCount() > 1) {
    bool canDoBlock = ctx.canBlock || ctx.anyCanBlock();
    bool canDoElementWise = ctx.canElementWise || ctx.anyCanElementWise();

    if (canDoBlock) {
      ARTS_DEBUG("H1.5 applied: Multi-node prefers block");
      return PartitioningDecision::block(
          ctx, "H1.5: Multi-node prefers block for network efficiency");
    }

    if (canDoElementWise) {
      ARTS_DEBUG("H1.5 applied: Multi-node prefers fine-grained");
      unsigned outerRank = ctx.minPinnedDimCount();
      outerRank = outerRank > 0 ? outerRank : 1;
      return PartitioningDecision::elementWise(
          ctx, outerRank, "H1.5: Multi-node prefers fine-grained");
    }
  }

  /// H1.6: Non-Uniform Access -> Coarse
  if (!ctx.isUniformAccess && !ctx.canElementWise && !ctx.canBlock) {
    ARTS_DEBUG("H1.6 applied: Non-uniform access prefers coarse");
    return PartitioningDecision::coarse(
        ctx, "H1.6: Non-uniform access prefers coarse");
  }

  /// Fallback: Respect user partition-fallback preference
  if (fallback == PartitionFallback::FineGrained) {
    ARTS_DEBUG("Fallback applied: No heuristic matched, using fine-grained "
               "(user preference)");
    return PartitioningDecision::elementWise(
        ctx, 1, "Fallback: User preference for fine-grained partitioning");
  }

  ARTS_DEBUG("Fallback applied: No heuristic matched, using coarse (user "
             "preference)");
  return PartitioningDecision::coarse(
      ctx, "Fallback: User preference for coarse partitioning");
}

///===----------------------------------------------------------------------===///
/// PartitioningHint Implementation
///===----------------------------------------------------------------------===///

DictionaryAttr PartitioningHint::toAttribute(MLIRContext *ctx) const {
  SmallVector<NamedAttribute> attrs;
  attrs.push_back(
      {StringAttr::get(ctx, "mode"),
       IntegerAttr::get(IntegerType::get(ctx, 8), static_cast<uint8_t>(mode))});
  if (blockSize)
    attrs.push_back({StringAttr::get(ctx, "blockSize"),
                     IntegerAttr::get(IntegerType::get(ctx, 64), *blockSize)});
  return DictionaryAttr::get(ctx, attrs);
}

std::optional<PartitioningHint>
PartitioningHint::fromAttribute(Attribute attr) {
  auto dictAttr = dyn_cast_or_null<DictionaryAttr>(attr);
  if (!dictAttr)
    return std::nullopt;

  PartitioningHint hint;

  if (auto modeAttr = dictAttr.getAs<IntegerAttr>("mode"))
    hint.mode = static_cast<PartitionMode>(modeAttr.getInt());

  if (auto chunkAttr = dictAttr.getAs<IntegerAttr>("blockSize"))
    hint.blockSize = chunkAttr.getInt();

  return hint;
}

/// PartitioningHint Accessor Functions
namespace mlir {
namespace arts {

std::optional<PartitioningHint> getPartitioningHint(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttr(AttrNames::Operation::PartitionHint))
    return PartitioningHint::fromAttribute(attr);
  return std::nullopt;
}

void setPartitioningHint(Operation *op, const PartitioningHint &hint) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::PartitionHint,
              hint.toAttribute(op->getContext()));
}

void copyArtsMetadataAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  ARTS_DEBUG("copyArtsMetadataAttrs: " << source->getName() << " -> "
                                       << dest->getName());

  /// Transfer arts.id
  if (auto id =
          source->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId)) {
    dest->setAttr(AttrNames::Operation::ArtsId, id);
    ARTS_DEBUG("  -> transferred arts.id=" << id.getInt());
  }

  /// Transfer partition_mode
  if (auto mode = source->getAttrOfType<PartitionModeAttr>(
          AttrNames::Operation::PartitionMode))
    dest->setAttr(AttrNames::Operation::PartitionMode, mode);

  /// Transfer arts.partition_hint
  if (auto hint = source->getAttr(AttrNames::Operation::PartitionHint))
    dest->setAttr(AttrNames::Operation::PartitionHint, hint);

  /// Transfer arts.loop metadata (trip count, parallelism info, etc.)
  if (auto loopAttr = source->getAttr(AttrNames::LoopMetadata::Name))
    dest->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
}

} // namespace arts
} // namespace mlir
