///==========================================================================///
/// File: CodirToArtsBridgePartitionEvidence.h
///
/// Partition-score and partition-graph evidence readers for bridge grouping.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPARTITIONEVIDENCE_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPARTITIONEVIDENCE_H

#include "CodirToArtsBridgePayload.h"

namespace {

static inline std::optional<int64_t> readPositiveI64(DictionaryAttr dict,
                                                     StringRef key) {
  if (!dict)
    return std::nullopt;
  auto attr = dyn_cast_or_null<IntegerAttr>(dict.get(key));
  if (!attr || attr.getInt() <= 0)
    return std::nullopt;
  return attr.getInt();
}

static inline int64_t
readPartitionScoreConcurrencyFloor(codir::CodeletOp codelet) {
  if (!codelet)
    return 0;
  auto score = dyn_cast_or_null<DictionaryAttr>(
      codelet->getAttr(codir::AttrNames::PartitionScore));
  if (!score)
    return 0;

  std::optional<int64_t> targetWorkers = readPositiveI64(
      score, codir::AttrNames::PartitionScoreKeys::TargetLogicalWorkers);
  std::optional<int64_t> exposedCuCount = readPositiveI64(
      score, codir::AttrNames::PartitionScoreKeys::ExposedCuCount);
  if (targetWorkers && exposedCuCount)
    return std::min(*targetWorkers, *exposedCuCount);
  if (targetWorkers)
    return *targetWorkers;
  if (exposedCuCount)
    return *exposedCuCount;
  return 0;
}

struct BridgePartitionGraphEvidence {
  int64_t muBlockCount = 0;
  int64_t cuGroupSize = 0;
};

static inline bool
bridgePartitionGraphRoleMatches(DictionaryAttr entry,
                                codir::CodirAccessMode mode) {
  auto role = dyn_cast_or_null<StringAttr>(
      entry ? entry.get(codir::AttrNames::PartitionGraphKeys::Role)
            : Attribute{});
  if (!role)
    return true;
  if (codirAccessMayWrite(mode) &&
      role.getValue() == codir::AttrNames::LayoutGraphValues::RoleWrite)
    return true;
  if (codirAccessMayRead(mode) &&
      role.getValue() == codir::AttrNames::LayoutGraphValues::RoleRead)
    return true;
  return false;
}

static inline BridgePartitionGraphEvidence
readBridgePartitionGraphEvidence(const BridgePlan &plan) {
  BridgePartitionGraphEvidence evidence;
  codir::CodeletOp codelet = plan.seedCodelet;
  if (!codelet)
    return evidence;

  std::optional<int64_t> depArrayId =
      codir::getDepArrayId(codelet, plan.seedDepIndex);
  if (!depArrayId)
    return evidence;

  auto graph = dyn_cast_or_null<ArrayAttr>(
      codelet->getAttr(codir::AttrNames::PartitionGraph));
  if (!graph)
    return evidence;

  codir::CodirAccessMode mode =
      codir::getDepAccessMode(codelet, plan.seedDepIndex)
          .value_or(codir::CodirAccessMode::readwrite);
  for (Attribute attr : graph) {
    auto entry = dyn_cast<DictionaryAttr>(attr);
    if (!entry)
      continue;
    auto edgeClass = dyn_cast_or_null<StringAttr>(
        entry.get(codir::AttrNames::PartitionGraphKeys::EdgeClass));
    if (!edgeClass ||
        edgeClass.getValue() !=
            codir::AttrNames::PartitionGraphValues::EdgeLayoutMismatch)
      continue;
    if (auto layoutKind = dyn_cast_or_null<StringAttr>(
            entry.get(codir::AttrNames::PartitionGraphKeys::LayoutKind)))
      if (layoutKind.getValue() ==
          codir::AttrNames::PartitionGraphValues::OwnerBlock)
        continue;
    if (!bridgePartitionGraphRoleMatches(entry, mode))
      continue;
    auto muId = dyn_cast_or_null<IntegerAttr>(
        entry.get(codir::AttrNames::PartitionGraphKeys::MuId));
    if (!muId || muId.getInt() != *depArrayId)
      continue;

    if (std::optional<int64_t> blocks = readPositiveI64(
            entry, codir::AttrNames::PartitionGraphKeys::MuBlockCount))
      evidence.muBlockCount = std::max(evidence.muBlockCount, *blocks);
    if (std::optional<int64_t> group = readPositiveI64(
            entry, codir::AttrNames::PartitionGraphKeys::CuGroupSize))
      evidence.cuGroupSize = std::max(evidence.cuGroupSize, *group);
  }
  return evidence;
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BRIDGEPARTITIONEVIDENCE_H
