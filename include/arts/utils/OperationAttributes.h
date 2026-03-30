#ifndef CARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_UTILS_OPERATIONATTRIBUTES_H

#include "arts/Dialect.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <limits>
#include <optional>

namespace mlir {
namespace arts {
namespace AttrNames {

/// Module-level attributes used across ARTS passes/codegen.
namespace Module {
using namespace llvm;
constexpr StringLiteral RuntimeConfigPath = "arts.runtime_config_path";
constexpr StringLiteral RuntimeConfigData = "arts.runtime_config_data";
constexpr StringLiteral RuntimeTotalWorkers = "arts.runtime_total_workers";
constexpr StringLiteral RuntimeTotalNodes = "arts.runtime_total_nodes";
constexpr StringLiteral RuntimeStaticWorkers = "arts.runtime_static_workers";
} // namespace Module

/// Operation-level attributes used across ARTS passes
namespace Operation {
using namespace llvm;

/// Common ARTS attributes
constexpr StringLiteral Workers = "workers";
constexpr StringLiteral WorkersPerNode = "workers_per_node";
constexpr StringLiteral ArtsId = "arts.id";
constexpr StringLiteral ArtsCreateId = "arts.create_id";
constexpr StringLiteral OutlinedFunc = "outlined_func";
constexpr StringLiteral Nowait = "nowait";
constexpr StringLiteral PreserveAccessMode = "preserve_access_mode";
constexpr StringLiteral PreserveDepEdge = "preserve_dep_edge";
constexpr StringLiteral LocalityOnly = "arts.locality_only";

/// Partition-related attributes (TableGen-generated names)
constexpr StringLiteral PartitionMode = "partition_mode";
constexpr StringLiteral PartitionHint = "arts.partition_hint";
constexpr StringLiteral AccessPattern = "access_pattern";
constexpr StringLiteral Distributed = "distributed";
constexpr StringLiteral DistributionKind = "distribution_kind";
constexpr StringLiteral DistributionPattern = "distribution_pattern";
/// ODS-generated attribute name for EdtOp dep pattern
constexpr StringLiteral DepPatternAttr = "depPattern";
constexpr StringLiteral DistributionVersion = "distribution_version";
constexpr StringLiteral PatternRevision = "arts.pattern_revision";

/// Epoch without caller-active count (CPS continuation-path inner epochs)
constexpr StringLiteral NoStartEpoch = "arts.no_start_epoch";

/// Marks the outer epoch of a CPS chain (the one main waits on)
constexpr StringLiteral CPSOuterEpoch = "arts.cps_outer_epoch";

/// DB storage-type inference annotations (set by DbModeTighteningPass)
constexpr StringLiteral LocalOnly = "arts.local_only";
constexpr StringLiteral ReadOnlyAfterInit = "arts.read_only_after_init";

/// CPS chain / epoch continuation attributes
constexpr StringLiteral ContinuationForEpoch = "arts.continuation_for_epoch";
constexpr StringLiteral CPSChainId = "arts.cps_chain_id";
constexpr StringLiteral CPSOuterEpochParamIdx =
    "arts.cps_outer_epoch_param_idx";
constexpr StringLiteral CPSIterCounterParamIdx =
    "arts.cps_iter_counter_param_idx";
constexpr StringLiteral CPSParamPerm = "arts.cps_param_perm";
constexpr StringLiteral CPSInitIter = "arts.cps_init_iter";
constexpr StringLiteral CPSAdditiveParams = "arts.cps_additive_params";
constexpr StringLiteral CPSNumCarry = "arts.cps_num_carry";
constexpr StringLiteral CPSLoopContinuation = "arts.cps_loop_continuation";
constexpr StringLiteral CPSAdvanceHasIvArg = "arts.cps_advance_has_iv_arg";

/// Preserves compile-time DB outer extents on rehydrated handle values when
/// outlining breaks the original DbAllocOp def-use chain.
constexpr StringLiteral DbStaticOuterShape = "arts.db_static_outer_shape";

/// GUID range detection annotations (set by GUIDRangeDetection)
constexpr StringLiteral GuidRangeTripCount = "guid_range_trip_count";
constexpr StringLiteral HasGuidRangeAlloc = "has_guid_range_alloc";

/// LoweringContractOp attribute names (used in Dialect.cpp build method)
namespace Contract {
using namespace llvm;
/// Contract attribute name for dep pattern
constexpr StringLiteral DepPatternKey = "dep_pattern";
/// These are the same as Operation:: versions; kept as aliases for readability.
constexpr auto DistributionKind = Operation::DistributionKind;
constexpr auto DistributionPattern = Operation::DistributionPattern;
constexpr auto DistributionVersion = Operation::DistributionVersion;
constexpr StringLiteral OwnerDims = "owner_dims";
constexpr StringLiteral SupportedBlockHalo = "supported_block_halo";
constexpr StringLiteral StencilIndependentDims = "stencil_independent_dims";
constexpr StringLiteral PostDbRefined = "post_db_refined";
constexpr StringLiteral CriticalPathDistance = "critical_path_distance";
constexpr StringLiteral AffinityDb = "affinity_db";
constexpr StringLiteral ReductionStrategy = "arts.reduction_strategy";
constexpr StringLiteral NarrowableDep = "narrowable_dep";
constexpr StringLiteral ContractKindKey = "contract_kind";
} // namespace Contract

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

inline std::optional<SmallVector<int64_t, 4>>
getDbStaticOuterShape(Operation *op) {
  return readI64ArrayAttr(op, AttrNames::Operation::DbStaticOuterShape);
}

inline std::optional<SmallVector<int64_t, 4>> getDbStaticOuterShape(Value value) {
  return value ? getDbStaticOuterShape(value.getDefiningOp()) : std::nullopt;
}

inline void setDbStaticOuterShape(Operation *op, ArrayRef<int64_t> shape) {
  if (!op || shape.empty())
    return;
  op->setAttr(AttrNames::Operation::DbStaticOuterShape,
              buildI64ArrayAttr(op, shape));
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

/// Forward declaration - defined in PartitioningHeuristics.h
struct PartitioningHint;

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

inline std::optional<int64_t> getArtsCreateId(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsCreateId))
    return attr.getInt();
  return std::nullopt;
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

inline std::optional<StringRef> getOutlinedFunc(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<StringAttr>(AttrNames::Operation::OutlinedFunc))
    return attr.getValue();
  return std::nullopt;
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

inline bool hasNowait(Operation *op) {
  return op && op->hasAttr(AttrNames::Operation::Nowait);
}

inline void setNowait(Operation *op, bool enabled = true) {
  if (!op)
    return;
  if (enabled) {
    op->setAttr(AttrNames::Operation::Nowait, UnitAttr::get(op->getContext()));
    return;
  }
  op->removeAttr(AttrNames::Operation::Nowait);
}

inline std::optional<PartitionMode> getPartitionMode(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<PartitionModeAttr>(
          AttrNames::Operation::PartitionMode))
    return attr.getValue();
  return std::nullopt;
}

inline void setPartitionMode(Operation *op, PartitionMode mode) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::PartitionMode,
              PartitionModeAttr::get(op->getContext(), mode));
}

inline std::optional<DbAccessPattern> getDbAccessPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<DbAccessPatternAttr>(
          AttrNames::Operation::AccessPattern))
    return attr.getValue();
  return std::nullopt;
}

inline void setDbAccessPattern(Operation *op, DbAccessPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::AccessPattern,
              DbAccessPatternAttr::get(op->getContext(), pattern));
}

inline bool hasDistributedDbAllocation(Operation *op) {
  if (!op)
    return false;
  return op->hasAttr(AttrNames::Operation::Distributed);
}

inline void setDistributedDbAllocation(Operation *op, bool enabled) {
  if (!op)
    return;
  if (enabled) {
    op->setAttr(AttrNames::Operation::Distributed,
                UnitAttr::get(op->getContext()));
    return;
  }
  op->removeAttr(AttrNames::Operation::Distributed);
}

inline std::optional<int64_t> getWorkers(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<workersAttr>(AttrNames::Operation::Workers))
    return static_cast<int64_t>(attr.getValue());
  if (auto attr = op->getAttrOfType<IntegerAttr>(AttrNames::Operation::Workers))
    return attr.getInt();
  return std::nullopt;
}

inline void setWorkers(Operation *op, int64_t workers) {
  if (!op)
    return;
  if (workers <= 0) {
    op->removeAttr(AttrNames::Operation::Workers);
    return;
  }
  int64_t clamped =
      std::min<int64_t>(workers, std::numeric_limits<int32_t>::max());
  op->setAttr(
      AttrNames::Operation::Workers,
      workersAttr::get(op->getContext(), static_cast<int32_t>(clamped)));
}

inline std::optional<int64_t> getWorkersPerNode(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<IntegerAttr>(AttrNames::Operation::WorkersPerNode))
    return attr.getInt();
  return std::nullopt;
}

inline void setWorkersPerNode(Operation *op, int64_t workersPerNode) {
  if (!op)
    return;
  if (workersPerNode <= 0) {
    op->removeAttr(AttrNames::Operation::WorkersPerNode);
    return;
  }
  op->setAttr(
      AttrNames::Operation::WorkersPerNode,
      IntegerAttr::get(IntegerType::get(op->getContext(), 64), workersPerNode));
}

inline bool isLocalityOnly(Operation *op) {
  return op && op->hasAttr(AttrNames::Operation::LocalityOnly);
}

inline void copyWorkerTopologyAttrs(Operation *from, Operation *to) {
  if (!to)
    return;
  if (!from) {
    setWorkers(to, 0);
    setWorkersPerNode(to, 0);
    return;
  }
  setWorkers(to, getWorkers(from).value_or(0));
  setWorkersPerNode(to, getWorkersPerNode(from).value_or(0));
}

inline std::optional<EdtDistributionKind>
getEdtDistributionKind(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionKindAttr>(
          AttrNames::Operation::DistributionKind))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionKind(Operation *op, EdtDistributionKind kind) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::DistributionKind,
              EdtDistributionKindAttr::get(op->getContext(), kind));
}

inline std::optional<EdtDistributionPattern>
getEdtDistributionPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionPatternAttr>(
          AttrNames::Operation::DistributionPattern))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionPattern(Operation *op,
                                      EdtDistributionPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::DistributionPattern,
              EdtDistributionPatternAttr::get(op->getContext(), pattern));
}

inline std::optional<ArtsDepPattern> getDepPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<ArtsDepPatternAttr>(
          AttrNames::Operation::DepPatternAttr))
    return attr.getValue();
  return std::nullopt;
}

inline void setDepPattern(Operation *op, ArtsDepPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::DepPatternAttr,
              ArtsDepPatternAttr::get(op->getContext(), pattern));
}

inline bool isStencilFamilyDepPattern(ArtsDepPattern pattern) {
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
}

inline bool isStencilHaloDepPattern(ArtsDepPattern pattern) {
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
}

inline bool isUniformFamilyDepPattern(ArtsDepPattern pattern) {
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
}

inline std::optional<ArtsDepPattern> getEffectiveDepPattern(Operation *op) {
  for (Operation *current = op; current; current = current->getParentOp()) {
    if (auto pattern = getDepPattern(current))
      return pattern;
  }
  return std::nullopt;
}

inline std::optional<int64_t> getPatternRevision(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<IntegerAttr>(AttrNames::Operation::PatternRevision))
    return attr.getInt();
  return std::nullopt;
}

inline void setPatternRevision(Operation *op, int64_t revision) {
  if (!op)
    return;
  if (revision <= 0) {
    op->removeAttr(AttrNames::Operation::PatternRevision);
    return;
  }
  op->setAttr(
      AttrNames::Operation::PatternRevision,
      IntegerAttr::get(IntegerType::get(op->getContext(), 64), revision));
}

inline void copyDepPatternAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  if (auto pattern = getDepPattern(source))
    setDepPattern(dest, *pattern);
  else
    dest->removeAttr(AttrNames::Operation::DepPatternAttr);
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

  if (auto kind = getEdtDistributionKind(source))
    setEdtDistributionKind(dest, *kind);
  else
    dest->removeAttr(AttrNames::Operation::DistributionKind);

  if (auto pattern = getEdtDistributionPattern(source))
    setEdtDistributionPattern(dest, *pattern);
  else
    dest->removeAttr(AttrNames::Operation::DistributionPattern);

  if (auto version = source->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::DistributionVersion))
    dest->setAttr(AttrNames::Operation::DistributionVersion, version);
  else
    dest->removeAttr(AttrNames::Operation::DistributionVersion);
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

  if (!dest->hasAttr(AttrNames::Operation::DistributionVersion))
    if (auto version = source->getAttrOfType<IntegerAttr>(
            AttrNames::Operation::DistributionVersion))
      dest->setAttr(AttrNames::Operation::DistributionVersion, version);
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
  if (auto revision = getPatternRevision(source))
    setPatternRevision(dest, *revision);
  else
    dest->removeAttr(AttrNames::Operation::PatternRevision);
}

inline void inheritPatternAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  inheritDistributionAttrs(source, dest);
  inheritDepPatternAttrs(source, dest);
  if (!getPatternRevision(dest))
    if (auto revision = getPatternRevision(source))
      setPatternRevision(dest, *revision);
}

/// Full implementation in OperationAttributes.cpp.
/// Use only when the destination preserves the same loop semantics/identity as
/// the source. Structural rewrites that create a new iteration space should
/// restamp the specific attrs they still mean instead of cloning all metadata.
void copyArtsMetadataAttrs(Operation *source, Operation *dest);

/// Copy only the semantic contract attrs that specialized pattern detection
/// stamps before DB values exist. Structural rewrites should use this helper
/// when they want to preserve pattern meaning without also copying unrelated
/// ids or bookkeeping metadata.
inline void copySemanticContractAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  copyPatternAttrs(source, dest);
  copyStencilContractAttrs(source, dest);
  if (auto contractKind = source->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Contract::ContractKindKey))
    dest->setAttr(AttrNames::Operation::Contract::ContractKindKey,
                  contractKind);
  else
    dest->removeAttr(AttrNames::Operation::Contract::ContractKindKey);
  if (source->hasAttr(AttrNames::Operation::Contract::NarrowableDep))
    dest->setAttr(AttrNames::Operation::Contract::NarrowableDep,
                  UnitAttr::get(dest->getContext()));
  else
    dest->removeAttr(AttrNames::Operation::Contract::NarrowableDep);
}

inline void inheritSemanticContractAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  inheritPatternAttrs(source, dest);
  inheritStencilContractAttrs(source, dest);
  if (!dest->hasAttr(AttrNames::Operation::Contract::ContractKindKey))
    if (auto contractKind = source->getAttrOfType<IntegerAttr>(
            AttrNames::Operation::Contract::ContractKindKey))
      dest->setAttr(AttrNames::Operation::Contract::ContractKindKey,
                    contractKind);
  if (!dest->hasAttr(AttrNames::Operation::Contract::NarrowableDep) &&
      source->hasAttr(AttrNames::Operation::Contract::NarrowableDep))
    dest->setAttr(AttrNames::Operation::Contract::NarrowableDep,
                  UnitAttr::get(dest->getContext()));
}

/// Copy contract attrs
inline void copyContractAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;
  copySemanticContractAttrs(source, dest);
  if (!getDepPattern(dest))
    if (auto pattern = getEffectiveDepPattern(source))
      setDepPattern(dest, *pattern);
}

/// Full implementation in PartitioningHeuristics.cpp (uses DictionaryAttr).
std::optional<PartitioningHint> getPartitioningHint(Operation *op);
void setPartitioningHint(Operation *op, const PartitioningHint &hint);

} // namespace arts
} // namespace mlir

#endif /// CARTS_UTILS_OPERATIONATTRIBUTES_H
