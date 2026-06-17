#ifndef CARTS_DIALECT_ARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_DIALECT_ARTS_UTILS_OPERATIONATTRIBUTES_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/ArtsAttrNames.h"
#include "carts/dialect/arts/Utils/StencilAttributes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace carts::arts {

inline void copyStencilFactAttrs(Operation *source, Operation *dest);

namespace AttrNames {

// Module-level marker attributes (Module::RuntimeConfigPath,
// RuntimeConfigData, RuntimeTotalWorkers, RuntimeTotalNodes,
// RuntimeStaticWorkers) live in carts/dialect/arts/Utils/ArtsAttrNames.h.

/// Operation-level attributes used across ARTS passes
namespace Operation {
using namespace llvm;

// ArtsId, ArtsCreateId, and OutlinedFunc live in
// carts/dialect/arts/Utils/ArtsAttrNames.h.

// `nowait`, `preserve_access_mode`, and `preserve_dep_edge` are ODS-declared
// attributes on their owning ops (sde.* loop/region ops and arts.db_acquire);
// consumers must use the generated `op.get<Name>AttrName()` / `op.get<Name>()`
// / `op.remove<Name>Attr()` accessors rather than raw strings.

/// Partition-related attributes (TableGen-generated names).
/// `partition_mode` is owned by ArtsPartitionedOpInterface, `distributed`
/// by ArtsDistributedDbOpInterface, and the distribution family
/// (`distribution_kind`, `distribution_pattern`, `distribution_version`,
/// `depPattern`) by ArtsDistributedOpInterface; their attr names are
/// obtained from the implementing op rather than via raw strings.
// `local_only` and `read_only_after_init` are ODS-declared UnitAttrs on
// arts.db_alloc (set by DbModeTighteningPass); consumers must use the
// generated getLocalOnly()/setLocalOnly(...)/removeLocalOnlyAttr() and the
// matching ReadOnlyAfterInit accessors rather than raw strings.

// Extra cross-dialect semantic marker names live in
// carts/dialect/arts/Utils/ArtsAttrNames.h. Prefer ODS accessors for ARTS ops
// and these shared constants only for discardable attrs on non-ARTS source ops.

} // namespace Operation

} // namespace AttrNames

inline std::optional<StringRef> getRuntimeConfigPath(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<StringAttr>(
          AttrNames::Module::RuntimeConfigPath))
    return attr.getValue();
  return std::nullopt;
}

inline void setRuntimeConfigPath(ModuleOp module, StringRef path) {
  if (!module || path.empty())
    return;
  module->setAttr(AttrNames::Module::RuntimeConfigPath,
                  StringAttr::get(module.getContext(), path));
}

inline std::optional<StringRef> getRuntimeConfigData(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<StringAttr>(
          AttrNames::Module::RuntimeConfigData))
    return attr.getValue();
  return std::nullopt;
}

inline void setRuntimeConfigData(ModuleOp module, StringRef data) {
  if (!module || data.empty())
    return;
  module->setAttr(AttrNames::Module::RuntimeConfigData,
                  StringAttr::get(module.getContext(), data));
}

inline std::optional<int64_t> getRuntimeTotalWorkers(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<IntegerAttr>(
          AttrNames::Module::RuntimeTotalWorkers))
    return attr.getInt();
  return std::nullopt;
}

inline void setRuntimeTotalWorkers(ModuleOp module, int64_t workers) {
  if (!module || workers <= 0)
    return;
  module->setAttr(
      AttrNames::Module::RuntimeTotalWorkers,
      IntegerAttr::get(IntegerType::get(module.getContext(), 64), workers));
}

inline bool getRuntimeStaticWorkers(ModuleOp module) {
  if (!module)
    return false;
  if (auto attr = module->getAttrOfType<BoolAttr>(
          AttrNames::Module::RuntimeStaticWorkers))
    return attr.getValue();
  return false;
}

inline void setRuntimeStaticWorkers(ModuleOp module, bool enabled) {
  if (!module)
    return;
  module->setAttr(AttrNames::Module::RuntimeStaticWorkers,
                  BoolAttr::get(module.getContext(), enabled));
}

inline std::optional<int64_t> getRuntimeTotalNodes(ModuleOp module) {
  if (!module)
    return std::nullopt;
  if (auto attr = module->getAttrOfType<IntegerAttr>(
          AttrNames::Module::RuntimeTotalNodes))
    return attr.getInt();
  return std::nullopt;
}

inline void setRuntimeTotalNodes(ModuleOp module, int64_t nodes) {
  if (!module || nodes <= 0)
    return;
  module->setAttr(
      AttrNames::Module::RuntimeTotalNodes,
      IntegerAttr::get(IntegerType::get(module.getContext(), 64), nodes));
}

inline int64_t getArtsId(Operation *op) {
  if (!op)
    return 0;
  if (auto attr = op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId))
    return attr.getInt();
  return 0;
}

inline void setArtsId(Operation *op, int64_t id) {
  if (!op || id <= 0)
    return;
  auto *ctx = op->getContext();
  auto type = IntegerType::get(ctx, 64);
  op->setAttr(AttrNames::Operation::ArtsId, IntegerAttr::get(type, id));
}

inline void setArtsCreateId(Operation *op, int64_t id) {
  if (!op)
    return;
  if (id <= 0) {
    op->removeAttr(AttrNames::Operation::ArtsCreateId);
    return;
  }
  auto *ctx = op->getContext();
  auto type = IntegerType::get(ctx, 64);
  op->setAttr(AttrNames::Operation::ArtsCreateId, IntegerAttr::get(type, id));
}

inline void setOutlinedFunc(Operation *op, StringRef name) {
  if (!op)
    return;
  if (name.empty()) {
    op->removeAttr(AttrNames::Operation::OutlinedFunc);
    return;
  }
  op->setAttr(AttrNames::Operation::OutlinedFunc,
              StringAttr::get(op->getContext(), name));
}

inline std::optional<PartitionMode> getPartitionMode(Operation *op) {
  if (!op)
    return std::nullopt;
  auto partitioned = dyn_cast<ArtsPartitionedOpInterface>(op);
  if (!partitioned)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<PartitionModeAttr>(
          partitioned.getPartitionModeAttrName()))
    return attr.getValue();
  return std::nullopt;
}

inline bool hasDistributedDbAllocation(Operation *op) {
  if (!op)
    return false;
  auto distributed = dyn_cast<ArtsDistributedDbOpInterface>(op);
  if (!distributed)
    return false;
  return op->hasAttr(distributed.getDistributedAttrName());
}

inline void setDistributedDbAllocation(Operation *op, bool enabled) {
  if (!op)
    return;
  auto distributed = dyn_cast<ArtsDistributedDbOpInterface>(op);
  if (!distributed)
    return;
  auto name = distributed.getDistributedAttrName();
  if (enabled) {
    op->setAttr(name, UnitAttr::get(op->getContext()));
    return;
  }
  op->removeAttr(name);
}

inline std::optional<EdtDistributionKind>
getEdtDistributionKind(Operation *op) {
  if (!op)
    return std::nullopt;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionKindAttr>(
          distributed.getDistributionKindAttrName()))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionKind(Operation *op, EdtDistributionKind kind) {
  if (!op)
    return;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return;
  op->setAttr(distributed.getDistributionKindAttrName(),
              EdtDistributionKindAttr::get(op->getContext(), kind));
}

inline std::optional<EdtDistributionPattern>
getEdtDistributionPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionPatternAttr>(
          distributed.getDistributionPatternAttrName()))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionPattern(Operation *op,
                                      EdtDistributionPattern pattern) {
  if (!op)
    return;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return;
  op->setAttr(distributed.getDistributionPatternAttrName(),
              EdtDistributionPatternAttr::get(op->getContext(), pattern));
}

inline std::optional<ArtsDepPattern> getDepPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<ArtsDepPatternAttr>(
          distributed.getDepPatternAttrName()))
    return attr.getValue();
  return std::nullopt;
}

inline void setDepPattern(Operation *op, ArtsDepPattern pattern) {
  if (!op)
    return;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return;
  op->setAttr(distributed.getDepPatternAttrName(),
              ArtsDepPatternAttr::get(op->getContext(), pattern));
}

inline IntegerAttr getDistributionVersionAttr(Operation *op) {
  if (!op)
    return nullptr;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return nullptr;
  return op->getAttrOfType<IntegerAttr>(
      distributed.getDistributionVersionAttrName());
}

inline void setDistributionVersionAttr(Operation *op, IntegerAttr attr) {
  if (!op)
    return;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return;
  auto name = distributed.getDistributionVersionAttrName();
  if (!attr) {
    op->removeAttr(name);
    return;
  }
  op->setAttr(name, attr);
}

inline void setDistributionVersion(Operation *op, int64_t version) {
  if (!op)
    return;
  auto distributed = dyn_cast<ArtsDistributedOpInterface>(op);
  if (!distributed)
    return;
  if (version <= 0) {
    op->removeAttr(distributed.getDistributionVersionAttrName());
    return;
  }
  setDistributionVersionAttr(
      op, IntegerAttr::get(IntegerType::get(op->getContext(), 32), version));
}

inline bool isStencilFamilyDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::alternating_buffer_stencil:
    return true;
  case ArtsDepPattern::unknown:
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::triangular:
  case ArtsDepPattern::matmul:
  case ArtsDepPattern::elementwise_pipeline:
  case ArtsDepPattern::reduction:
    return false;
  }
}

inline bool isStencilHaloDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::alternating_buffer_stencil:
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
  case ArtsDepPattern::reduction:
    return false;
  }
}

inline bool isUniformFamilyDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
  case ArtsDepPattern::reduction:
    return true;
  case ArtsDepPattern::unknown:
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::matmul:
  case ArtsDepPattern::triangular:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::alternating_buffer_stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
    return false;
  }
}

inline std::optional<EdtDistributionPattern>
getDistributionPatternForDepPattern(ArtsDepPattern pattern) {
  switch (pattern) {
  case ArtsDepPattern::unknown:
    return std::nullopt;
  case ArtsDepPattern::uniform:
  case ArtsDepPattern::elementwise_pipeline:
  case ArtsDepPattern::reduction:
    return EdtDistributionPattern::uniform;
  case ArtsDepPattern::stencil:
  case ArtsDepPattern::stencil_tiling_nd:
  case ArtsDepPattern::cross_dim_stencil_3d:
  case ArtsDepPattern::higher_order_stencil:
  case ArtsDepPattern::wavefront_2d:
  case ArtsDepPattern::alternating_buffer_stencil:
    return EdtDistributionPattern::stencil;
  case ArtsDepPattern::matmul:
    return EdtDistributionPattern::matmul;
  case ArtsDepPattern::triangular:
    return EdtDistributionPattern::triangular;
  }
}

inline std::optional<ArtsDepPattern> getEffectiveDepPattern(Operation *op) {
  for (Operation *current = op; current; current = current->getParentOp()) {
    if (auto pattern = getDepPattern(current))
      return pattern;
  }
  return std::nullopt;
}

inline void copyDepPatternAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  if (auto pattern = getDepPattern(source)) {
    setDepPattern(dest, *pattern);
    return;
  }
  if (auto distributed = dyn_cast<ArtsDistributedOpInterface>(dest))
    dest->removeAttr(distributed.getDepPatternAttrName());
}

inline void inheritDepPatternAttrs(Operation *source, Operation *dest) {
  if (!source || !dest || getDepPattern(dest))
    return;
  if (auto pattern = getDepPattern(source))
    setDepPattern(dest, *pattern);
}

/// Copy distribution_* attributes between operations.
/// This intentionally transfers only distribution facts:
///   - distribution_kind
///   - distribution_pattern
///   - distribution_version
inline void copyDistributionAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  auto destDistributed = dyn_cast<ArtsDistributedOpInterface>(dest);

  if (auto kind = getEdtDistributionKind(source))
    setEdtDistributionKind(dest, *kind);
  else if (destDistributed)
    dest->removeAttr(destDistributed.getDistributionKindAttrName());

  if (auto pattern = getEdtDistributionPattern(source))
    setEdtDistributionPattern(dest, *pattern);
  else if (destDistributed)
    dest->removeAttr(destDistributed.getDistributionPatternAttrName());

  if (auto version = getDistributionVersionAttr(source))
    setDistributionVersionAttr(dest, version);
  else if (destDistributed)
    dest->removeAttr(destDistributed.getDistributionVersionAttrName());
}

inline void inheritDistributionAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  if (!getEdtDistributionKind(dest))
    if (auto kind = getEdtDistributionKind(source))
      setEdtDistributionKind(dest, *kind);

  if (!getEdtDistributionPattern(dest))
    if (auto pattern = getEdtDistributionPattern(source))
      setEdtDistributionPattern(dest, *pattern);

  if (!getDistributionVersionAttr(dest))
    if (auto version = getDistributionVersionAttr(source))
      setDistributionVersionAttr(dest, version);
}

namespace detail {
template <typename SourceOpT, typename DestOpT>
inline void copyGeneratedDistributionAttrs(SourceOpT source, DestOpT dest) {
  if (!source || !dest)
    return;

  if (auto attr = source.getDistributionKindAttr())
    dest.setDistributionKindAttr(attr);
  else
    dest.removeDistributionKindAttr();

  if (auto attr = source.getDistributionPatternAttr())
    dest.setDistributionPatternAttr(attr);
  else
    dest.removeDistributionPatternAttr();

  if (auto attr = source.getDistributionVersionAttr())
    dest.setDistributionVersionAttr(attr);
  else
    dest.removeDistributionVersionAttr();
}

template <typename SourceOpT, typename DestOpT>
inline void inheritGeneratedDistributionAttrs(SourceOpT source, DestOpT dest) {
  if (!source || !dest)
    return;

  if (!dest.getDistributionKindAttr())
    if (auto attr = source.getDistributionKindAttr())
      dest.setDistributionKindAttr(attr);

  if (!dest.getDistributionPatternAttr())
    if (auto attr = source.getDistributionPatternAttr())
      dest.setDistributionPatternAttr(attr);

  if (!dest.getDistributionVersionAttr())
    if (auto attr = source.getDistributionVersionAttr())
      dest.setDistributionVersionAttr(attr);
}
} // namespace detail

/// Preserve generated distribution fact attrs when cloning or replacing a DB
/// alloc.
/// This typed wrapper keeps callers on ODS-generated accessors instead of the
/// broader Operation* propagation bridge.
inline void copyDbAllocDistributionFactAttrs(DbAllocOp source, DbAllocOp dest) {
  detail::copyGeneratedDistributionAttrs(source, dest);
}

inline void copyDbAcquireDistributionFactAttrs(DbAcquireOp source,
                                               DbAcquireOp dest) {
  detail::copyGeneratedDistributionAttrs(source, dest);
}

/// Inherit EDT distribution fact attrs without overwriting attrs already
/// authored on the destination EDT.
inline void inheritEdtDistributionFactAttrs(EdtOp source, EdtOp dest) {
  detail::inheritGeneratedDistributionAttrs(source, dest);
}

/// Inherit distribution fact attrs from a DB source onto a derived acquire.
inline void inheritDbAcquireDistributionFactAttrs(DbAllocOp source,
                                                  DbAcquireOp dest) {
  detail::inheritGeneratedDistributionAttrs(source, dest);
}

inline void inheritDbAcquireDistributionFactAttrs(DbAcquireOp source,
                                                  DbAcquireOp dest) {
  detail::inheritGeneratedDistributionAttrs(source, dest);
}

/// Copy semantic pattern attributes between operations.
/// This is the canonical helper for structural rewrites that replace a loop,
/// EDT, or epoch with an equivalent operation and want downstream passes to
/// keep seeing the same high-level dep/distribution family.
inline void copyPatternAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  copyDistributionAttrs(source, dest);
  copyDepPatternAttrs(source, dest);
}

/// Use only when the destination preserves the same loop semantics/identity as
/// the source. Structural rewrites that create a new iteration space should
/// write the specific attrs they still mean instead of cloning all attrs.
inline void copyArtsMetadataAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  if (auto id =
          source->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId))
    dest->setAttr(AttrNames::Operation::ArtsId, id);
  auto sourcePartitioned = dyn_cast<ArtsPartitionedOpInterface>(source);
  auto destPartitioned = dyn_cast<ArtsPartitionedOpInterface>(dest);
  if (sourcePartitioned && destPartitioned) {
    if (auto mode = source->getAttrOfType<PartitionModeAttr>(
            sourcePartitioned.getPartitionModeAttrName()))
      dest->setAttr(destPartitioned.getPartitionModeAttrName(), mode);
  }
}

/// Copy only the semantic fact attrs that specialized pattern detection
/// records before DB values exist. Structural rewrites should use this helper
/// when they want to preserve pattern meaning without also copying unrelated
/// ids or bookkeeping attrs.
inline void copySemanticFactAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  copyPatternAttrs(source, dest);
  copyStencilFactAttrs(source, dest);
  if (source->hasAttr(AttrNames::Semantic::NarrowableDep))
    dest->setAttr(AttrNames::Semantic::NarrowableDep,
                  UnitAttr::get(dest->getContext()));
  else
    dest->removeAttr(AttrNames::Semantic::NarrowableDep);
}

inline void copyDbAcquireSemanticFactAttrs(DbAcquireOp source,
                                           DbAcquireOp dest) {
  if (!source || !dest)
    return;
  copyDbAcquireDistributionFactAttrs(source, dest);
  copyDepPatternAttrs(source.getOperation(), dest.getOperation());
  copyStencilFactAttrs(source.getOperation(), dest.getOperation());
  if (source->hasAttr(AttrNames::Semantic::NarrowableDep))
    dest->setAttr(AttrNames::Semantic::NarrowableDep,
                  UnitAttr::get(dest.getContext()));
  else
    dest->removeAttr(AttrNames::Semantic::NarrowableDep);
}

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_OPERATIONATTRIBUTES_H
