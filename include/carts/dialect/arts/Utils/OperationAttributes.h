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

inline void copyStencilContractAttrs(Operation *source, Operation *dest);

namespace AttrNames {

// Module-level marker attributes (Module::RuntimeConfigPath,
// RuntimeConfigData, RuntimeTotalWorkers, RuntimeTotalNodes,
// RuntimeStaticWorkers) live in carts/dialect/arts/Utils/ArtsAttrNames.h.

/// Operation-level attributes used across ARTS passes
namespace Operation {
using namespace llvm;

// ArtsId, ArtsCreateId, OutlinedFunc, and StripMiningGenerated live in
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
// `distributed_reject_reason` is an ODS-declared OptionalAttr<StrAttr> on
// arts.db_alloc (stamped by DbOwnerMapRealizationPass); consumers must use
// the generated getDistributedRejectReasonAttr() /
// setDistributedRejectReason(StringRef) /
// removeDistributedRejectReasonAttr() accessors.

// `local_only` and `read_only_after_init` are ODS-declared UnitAttrs on
// arts.db_alloc (set by DbModeTighteningPass); consumers must use the
// generated getLocalOnly()/setLocalOnly(...)/removeLocalOnlyAttr() and the
// matching ReadOnlyAfterInit accessors rather than raw strings.

// LoweringContractOp attribute names.
//
// `owner_dims`, `supported_block_halo`, `stencil_independent_dims`,
// `post_db_refined`, and `spatial_dims` had zero in-tree consumers and were
// dropped. `narrowable_dep` is a cross-dialect
// discardable key (propagated via copySemanticContractAttrs onto non-ARTS
// source ops like memref.alloc); it lives in
// carts/dialect/arts/Utils/ArtsAttrNames.h under the Contract namespace.

// Proof-driven ownership attributes (Proof::OwnerDimReachability,
// PartitionAccessMapping, HaloLegality, DepSliceSoundness) live in
// carts/dialect/arts/Utils/ArtsAttrNames.h.

} // namespace Operation

} // namespace AttrNames

inline ArrayAttr getPlanOwnerDimsAttr(Operation *op) {
  if (!op)
    return nullptr;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    return edtOp.getPlanOwnerDimsAttr();
  if (auto epochOp = dyn_cast<EpochOp>(op))
    return epochOp.getPlanOwnerDimsAttr();
  if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    return dbAllocOp.getPlanOwnerDimsAttr();
  return nullptr;
}

inline ArrayAttr getPlanPhysicalBlockShapeAttr(Operation *op) {
  if (!op)
    return nullptr;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    return edtOp.getPlanPhysicalBlockShapeAttr();
  if (auto epochOp = dyn_cast<EpochOp>(op))
    return epochOp.getPlanPhysicalBlockShapeAttr();
  if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    return dbAllocOp.getPlanPhysicalBlockShapeAttr();
  return nullptr;
}

inline ArrayAttr getPlanLogicalWorkerSliceAttr(Operation *op) {
  if (!op)
    return nullptr;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    return edtOp.getPlanLogicalWorkerSliceAttr();
  if (auto epochOp = dyn_cast<EpochOp>(op))
    return epochOp.getPlanLogicalWorkerSliceAttr();
  if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    return dbAllocOp.getPlanLogicalWorkerSliceAttr();
  return nullptr;
}

inline ArrayAttr getPlanHaloShapeAttr(Operation *op) {
  if (!op)
    return nullptr;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    return edtOp.getPlanHaloShapeAttr();
  if (auto epochOp = dyn_cast<EpochOp>(op))
    return epochOp.getPlanHaloShapeAttr();
  if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    return dbAllocOp.getPlanHaloShapeAttr();
  return nullptr;
}

inline ArtsPlanIterationTopologyAttr
getPlanIterationTopologyAttr(Operation *op) {
  if (!op)
    return nullptr;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    return edtOp.getPlanIterationTopologyAttr();
  if (auto epochOp = dyn_cast<EpochOp>(op))
    return epochOp.getPlanIterationTopologyAttr();
  if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    return dbAllocOp.getPlanIterationTopologyAttr();
  return nullptr;
}

inline ArtsPlanRepetitionStructureAttr
getPlanRepetitionStructureAttr(Operation *op) {
  if (!op)
    return nullptr;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    return edtOp.getPlanRepetitionStructureAttr();
  if (auto epochOp = dyn_cast<EpochOp>(op))
    return epochOp.getPlanRepetitionStructureAttr();
  if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    return dbAllocOp.getPlanRepetitionStructureAttr();
  return nullptr;
}

inline void setPlanOwnerDimsAttr(Operation *op, ArrayAttr attr) {
  if (!op || !attr)
    return;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    edtOp.setPlanOwnerDimsAttr(attr);
  else if (auto epochOp = dyn_cast<EpochOp>(op))
    epochOp.setPlanOwnerDimsAttr(attr);
  else if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    dbAllocOp.setPlanOwnerDimsAttr(attr);
}

inline void setPlanPhysicalBlockShapeAttr(Operation *op, ArrayAttr attr) {
  if (!op || !attr)
    return;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    edtOp.setPlanPhysicalBlockShapeAttr(attr);
  else if (auto epochOp = dyn_cast<EpochOp>(op))
    epochOp.setPlanPhysicalBlockShapeAttr(attr);
  else if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    dbAllocOp.setPlanPhysicalBlockShapeAttr(attr);
}

inline void setPlanLogicalWorkerSliceAttr(Operation *op, ArrayAttr attr) {
  if (!op || !attr)
    return;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    edtOp.setPlanLogicalWorkerSliceAttr(attr);
  else if (auto epochOp = dyn_cast<EpochOp>(op))
    epochOp.setPlanLogicalWorkerSliceAttr(attr);
  else if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    dbAllocOp.setPlanLogicalWorkerSliceAttr(attr);
}

inline void setPlanHaloShapeAttr(Operation *op, ArrayAttr attr) {
  if (!op || !attr)
    return;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    edtOp.setPlanHaloShapeAttr(attr);
  else if (auto epochOp = dyn_cast<EpochOp>(op))
    epochOp.setPlanHaloShapeAttr(attr);
  else if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    dbAllocOp.setPlanHaloShapeAttr(attr);
}

inline void setPlanIterationTopologyAttr(Operation *op,
                                         ArtsPlanIterationTopologyAttr attr) {
  if (!op || !attr)
    return;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    edtOp.setPlanIterationTopologyAttr(attr);
  else if (auto epochOp = dyn_cast<EpochOp>(op))
    epochOp.setPlanIterationTopologyAttr(attr);
  else if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    dbAllocOp.setPlanIterationTopologyAttr(attr);
}

inline void
setPlanRepetitionStructureAttr(Operation *op,
                               ArtsPlanRepetitionStructureAttr attr) {
  if (!op || !attr)
    return;
  if (auto edtOp = dyn_cast<EdtOp>(op))
    edtOp.setPlanRepetitionStructureAttr(attr);
  else if (auto epochOp = dyn_cast<EpochOp>(op))
    epochOp.setPlanRepetitionStructureAttr(attr);
  else if (auto dbAllocOp = dyn_cast<DbAllocOp>(op))
    dbAllocOp.setPlanRepetitionStructureAttr(attr);
}

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
/// This intentionally transfers only distribution contracts:
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
/// restamp the specific attrs they still mean instead of cloning all metadata.
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

/// Copy only the semantic contract attrs that specialized pattern detection
/// stamps before DB values exist. Structural rewrites should use this helper
/// when they want to preserve pattern meaning without also copying unrelated
/// ids or bookkeeping metadata.
inline void copySemanticContractAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  copyPatternAttrs(source, dest);
  copyStencilContractAttrs(source, dest);
  if (source->hasAttr(AttrNames::Contract::NarrowableDep))
    dest->setAttr(AttrNames::Contract::NarrowableDep,
                  UnitAttr::get(dest->getContext()));
  else
    dest->removeAttr(AttrNames::Contract::NarrowableDep);
}

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_OPERATIONATTRIBUTES_H
