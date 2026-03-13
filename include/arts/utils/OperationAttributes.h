#ifndef CARTS_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_UTILS_OPERATIONATTRIBUTES_H

#include "arts/Dialect.h"
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
} // namespace Module

/// Operation-level attributes used across ARTS passes
namespace Operation {
using namespace llvm;

/// Worker topology attributes.
namespace Worker {
constexpr StringLiteral Workers("workers");
constexpr StringLiteral WorkersPerNode("workers_per_node");
} // namespace Worker

/// Partitioning attributes.
namespace Partition {
constexpr StringLiteral PartitionMode("partition_mode");
constexpr StringLiteral PartitionHint("arts.partition_hint");
constexpr StringLiteral AccessPattern("access_pattern");
} // namespace Partition

/// Distributed execution attributes.
namespace Distribution {
constexpr StringLiteral Distributed("distributed");
constexpr StringLiteral DistributionKind("distribution_kind");
constexpr StringLiteral DistributionPattern("distribution_pattern");
constexpr StringLiteral DistributionVersion("distribution_version");
} // namespace Distribution

/// Structural metadata attributes.
namespace Metadata {
constexpr StringLiteral ArtsId("arts.id");
constexpr StringLiteral ArtsCreateId("arts.create_id");
constexpr StringLiteral OutlinedFunc("outlined_func");
constexpr StringLiteral Nowait("nowait");
constexpr StringLiteral PreserveDependencyMode("preserve_dep_mode");
constexpr StringLiteral PreserveDependency("preserve_dep");
} // namespace Metadata

} // namespace Operation

} // namespace AttrNames

// ===== Module-Level Attributes =====

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

// ===== Worker Topology =====

inline std::optional<int64_t> getWorkers(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<workersAttr>(AttrNames::Operation::Worker::Workers))
    return static_cast<int64_t>(attr.getValue());
  if (auto attr = op->getAttrOfType<IntegerAttr>(AttrNames::Operation::Worker::Workers))
    return attr.getInt();
  return std::nullopt;
}

inline void setWorkers(Operation *op, int64_t workers) {
  if (!op)
    return;
  if (workers <= 0) {
    op->removeAttr(AttrNames::Operation::Worker::Workers);
    return;
  }
  int64_t clamped =
      std::min<int64_t>(workers, std::numeric_limits<int32_t>::max());
  op->setAttr(
      AttrNames::Operation::Worker::Workers,
      workersAttr::get(op->getContext(), static_cast<int32_t>(clamped)));
}

inline std::optional<int64_t> getWorkersPerNode(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr =
          op->getAttrOfType<IntegerAttr>(AttrNames::Operation::Worker::WorkersPerNode))
    return attr.getInt();
  return std::nullopt;
}

inline void setWorkersPerNode(Operation *op, int64_t workersPerNode) {
  if (!op)
    return;
  if (workersPerNode <= 0) {
    op->removeAttr(AttrNames::Operation::Worker::WorkersPerNode);
    return;
  }
  op->setAttr(
      AttrNames::Operation::Worker::WorkersPerNode,
      IntegerAttr::get(IntegerType::get(op->getContext(), 64), workersPerNode));
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

// ===== Partitioning =====

inline std::optional<PartitionMode> getPartitionMode(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<PartitionModeAttr>(
          AttrNames::Operation::Partition::PartitionMode))
    return attr.getValue();
  return std::nullopt;
}

inline void setPartitionMode(Operation *op, PartitionMode mode) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Partition::PartitionMode,
              PartitionModeAttr::get(op->getContext(), mode));
}

inline std::optional<DbAccessPattern> getDbAccessPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<DbAccessPatternAttr>(
          AttrNames::Operation::Partition::AccessPattern))
    return attr.getValue();
  return std::nullopt;
}

inline void setDbAccessPattern(Operation *op, DbAccessPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Partition::AccessPattern,
              DbAccessPatternAttr::get(op->getContext(), pattern));
}

/// Forward declaration - defined in PartitioningHeuristics.h
struct PartitioningHint;

/// Full implementation in HeuristicsConfig.cpp (uses DictionaryAttr).
std::optional<PartitioningHint> getPartitioningHint(Operation *op);
void setPartitioningHint(Operation *op, const PartitioningHint &hint);

// ===== Distribution =====

inline bool hasDistributedDbAllocation(Operation *op) {
  if (!op)
    return false;
  return op->hasAttr(AttrNames::Operation::Distribution::Distributed);
}

inline void setDistributedDbAllocation(Operation *op, bool enabled) {
  if (!op)
    return;
  if (enabled) {
    op->setAttr(AttrNames::Operation::Distribution::Distributed,
                UnitAttr::get(op->getContext()));
    return;
  }
  op->removeAttr(AttrNames::Operation::Distribution::Distributed);
}

inline std::optional<EdtDistributionKind>
getEdtDistributionKind(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionKindAttr>(
          AttrNames::Operation::Distribution::DistributionKind))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionKind(Operation *op, EdtDistributionKind kind) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Distribution::DistributionKind,
              EdtDistributionKindAttr::get(op->getContext(), kind));
}

inline std::optional<EdtDistributionPattern>
getEdtDistributionPattern(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<EdtDistributionPatternAttr>(
          AttrNames::Operation::Distribution::DistributionPattern))
    return attr.getValue();
  return std::nullopt;
}

inline void setEdtDistributionPattern(Operation *op,
                                      EdtDistributionPattern pattern) {
  if (!op)
    return;
  op->setAttr(AttrNames::Operation::Distribution::DistributionPattern,
              EdtDistributionPatternAttr::get(op->getContext(), pattern));
}

/// Copy distribution_* attributes between operations.
inline void copyDistributionAttrs(Operation *source, Operation *dest) {
  if (!source || !dest)
    return;

  if (auto kind = getEdtDistributionKind(source))
    setEdtDistributionKind(dest, *kind);
  else
    dest->removeAttr(AttrNames::Operation::Distribution::DistributionKind);

  if (auto pattern = getEdtDistributionPattern(source))
    setEdtDistributionPattern(dest, *pattern);
  else
    dest->removeAttr(AttrNames::Operation::Distribution::DistributionPattern);

  if (auto version = source->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Distribution::DistributionVersion))
    dest->setAttr(AttrNames::Operation::Distribution::DistributionVersion, version);
  else
    dest->removeAttr(AttrNames::Operation::Distribution::DistributionVersion);
}

// ===== Metadata =====

inline int64_t getArtsId(Operation *op) {
  if (!op)
    return 0;
  if (auto attr = op->getAttrOfType<IntegerAttr>(AttrNames::Operation::Metadata::ArtsId))
    return attr.getInt();
  return 0;
}

inline std::optional<int64_t> getArtsCreateId(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<IntegerAttr>(
          AttrNames::Operation::Metadata::ArtsCreateId))
    return attr.getInt();
  return std::nullopt;
}

inline void setArtsCreateId(Operation *op, int64_t id) {
  if (!op || id <= 0)
    return;
  op->setAttr(AttrNames::Operation::Metadata::ArtsCreateId,
              IntegerAttr::get(IntegerType::get(op->getContext(), 64), id));
}

inline std::optional<StringRef> getOutlinedFunc(Operation *op) {
  if (!op)
    return std::nullopt;
  if (auto attr = op->getAttrOfType<StringAttr>(
          AttrNames::Operation::Metadata::OutlinedFunc))
    return attr.getValue();
  return std::nullopt;
}

inline void setOutlinedFunc(Operation *op, StringRef funcName) {
  if (!op || funcName.empty())
    return;
  op->setAttr(AttrNames::Operation::Metadata::OutlinedFunc,
              StringAttr::get(op->getContext(), funcName));
}

inline bool hasNowait(Operation *op) {
  return op && op->hasAttr(AttrNames::Operation::Metadata::Nowait);
}

inline void setNowait(Operation *op, bool enabled) {
  if (!op)
    return;
  if (enabled)
    op->setAttr(AttrNames::Operation::Metadata::Nowait,
                UnitAttr::get(op->getContext()));
  else
    op->removeAttr(AttrNames::Operation::Metadata::Nowait);
}

} // namespace arts
} // namespace mlir

#endif /// CARTS_UTILS_OPERATIONATTRIBUTES_H
