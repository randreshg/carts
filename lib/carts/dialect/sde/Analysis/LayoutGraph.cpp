///==========================================================================///
/// File: LayoutGraph.cpp
///
/// Neutral SDE layout graph payload helpers.
///==========================================================================///

#include "carts/dialect/sde/Analysis/LayoutGraph.h"

#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/Numeric.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <limits>

using namespace mlir;

namespace mlir::carts::sde {
namespace {

using carts::ceilDivPositive;
using carts::saturatingAddPositive;
using carts::saturatingMulPositive;

static Value stripRoot(Value value) {
  return ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
}

static std::optional<int64_t> getI64(DictionaryAttr dict, StringRef key) {
  if (!dict)
    return std::nullopt;
  auto attr = dyn_cast_or_null<IntegerAttr>(dict.get(key));
  if (!attr)
    return std::nullopt;
  return attr.getInt();
}

static std::optional<StringRef> getString(DictionaryAttr dict, StringRef key) {
  if (!dict)
    return std::nullopt;
  auto attr = dyn_cast_or_null<StringAttr>(dict.get(key));
  if (!attr)
    return std::nullopt;
  return attr.getValue();
}

static SmallVector<int64_t, 4> getI64Array(DictionaryAttr dict, StringRef key) {
  SmallVector<int64_t, 4> values;
  if (!dict)
    return values;
  auto attr = dyn_cast_or_null<ArrayAttr>(dict.get(key));
  if (!attr)
    return values;
  for (Attribute element : attr) {
    auto intAttr = dyn_cast<IntegerAttr>(element);
    if (!intAttr)
      return {};
    values.push_back(intAttr.getInt());
  }
  return values;
}

static int64_t getElementBytes(Value root) {
  auto type = dyn_cast_or_null<ShapedType>(root ? root.getType() : Type{});
  if (!type)
    return 0;
  Type elementType = type.getElementType();
  if (elementType.isIntOrFloat()) {
    unsigned bits = elementType.getIntOrFloatBitWidth();
    return std::max<int64_t>(1, (static_cast<int64_t>(bits) + 7) / 8);
  }
  return 0;
}

static int64_t inferBlockCount(ArrayRef<int64_t> shape,
                               ArrayRef<int64_t> ownerDims,
                               ArrayRef<int64_t> blockShape) {
  if (shape.empty() || ownerDims.empty() || shape.size() != blockShape.size())
    return 1;

  int64_t count = 1;
  for (int64_t dim : ownerDims) {
    if (dim < 0 || static_cast<size_t>(dim) >= shape.size())
      return 1;
    int64_t extent = shape[dim];
    int64_t block = blockShape[dim];
    if (extent <= 0 || block <= 0)
      return 1;
    int64_t blocks = ceilDivPositive(extent, block);
    if (count > std::numeric_limits<int64_t>::max() / blocks)
      return std::numeric_limits<int64_t>::max();
    count *= blocks;
  }
  return std::max<int64_t>(1, count);
}

static int64_t defaultMuTrafficBytes(const MuNet &net) {
  if (net.tilePayloadBytes > 0)
    return net.tilePayloadBytes;
  if (net.staticShape.empty() || net.elementBytes <= 0)
    return 0;
  return tilePayloadBytes(net.staticShape, net.elementBytes);
}

static void appendSubscriptsFromMap(AffineMap map,
                                    SmallVectorImpl<AffineDimOffset> &out) {
  if (!map)
    return;
  for (AffineExpr expr : map.getResults()) {
    std::optional<AffineDimOffset> offset = extractDimOffset(expr);
    if (!offset) {
      out.push_back(AffineDimOffset{});
      continue;
    }
    out.push_back(*offset);
  }
}

static LayoutGraphFact parseLayoutCommon(DictionaryAttr dict) {
  LayoutGraphFact fact;
  if (std::optional<int64_t> id = getI64(dict, AttrNames::LayoutGraph::ArrayId))
    fact.id = *id;
  if (std::optional<StringRef> role =
          getString(dict, AttrNames::LayoutGraph::Role))
    fact.role = parseLayoutGraphRole(*role);
  if (std::optional<StringRef> kind =
          getString(dict, AttrNames::LayoutGraph::Kind))
    fact.layoutKind = parseLayoutKind(*kind);
  fact.ownerDims = getI64Array(dict, AttrNames::LayoutGraph::OwnerDims);
  fact.blockShape = getI64Array(dict, AttrNames::LayoutGraph::BlockShape);
  fact.budgetBlockShape =
      getI64Array(dict, AttrNames::LayoutGraph::BudgetBlockShape);
  if (std::optional<int64_t> value =
          getI64(dict, AttrNames::LayoutGraph::MuBlockCount))
    fact.muBlockCount = std::max<int64_t>(1, *value);
  return fact;
}

} // namespace

bool AccessRelation::reads() const {
  return kind == LayoutGraphAccessKind::read ||
         kind == LayoutGraphAccessKind::readWrite ||
         kind == LayoutGraphAccessKind::reduction;
}

bool AccessRelation::writes() const {
  return kind == LayoutGraphAccessKind::write ||
         kind == LayoutGraphAccessKind::readWrite ||
         kind == LayoutGraphAccessKind::reduction;
}

SuRegion &LayoutGraph::addSuRegion(SuRegion region) {
  region.id = suRegions.size();
  suRegions.push_back(std::move(region));
  return suRegions.back();
}

CuVertex &LayoutGraph::addCuVertex(CuVertex vertex) {
  vertex.id = cuVertices.size();
  std::optional<unsigned> suRegionId = vertex.suRegionId;
  cuVertices.push_back(std::move(vertex));
  if (suRegionId && *suRegionId < suRegions.size()) {
    SmallVector<unsigned, 4> &cuIds = suRegions[*suRegionId].cuIds;
    if (!llvm::is_contained(cuIds, cuVertices.back().id))
      cuIds.push_back(cuVertices.back().id);
  }
  return cuVertices.back();
}

MuNet &LayoutGraph::addMuNet(MuNet net) {
  net.id = muNets.size();
  muNets.push_back(std::move(net));
  return muNets.back();
}

MuPin &LayoutGraph::addMuPin(MuPin pin) {
  pin.id = muPins.size();
  muPins.push_back(std::move(pin));
  MuPin &stored = muPins.back();
  if (CuVertex *cu = findCuVertex(stored.cuId))
    cu->pinIds.push_back(stored.id);
  if (stored.muId < muNets.size())
    muNets[stored.muId].pinIds.push_back(stored.id);
  return stored;
}

MuNet *LayoutGraph::findMuNetByRoot(Value root) {
  root = stripRoot(root);
  for (MuNet &net : muNets)
    if (net.root == root)
      return &net;
  return nullptr;
}

CuVertex *LayoutGraph::findCuVertex(unsigned cuId) {
  if (cuId >= cuVertices.size())
    return nullptr;
  return &cuVertices[cuId];
}

LayoutGraphAccessKind accessKindFromReadWrite(bool isRead, bool isWrite) {
  if (isRead && isWrite)
    return LayoutGraphAccessKind::readWrite;
  if (isWrite)
    return LayoutGraphAccessKind::write;
  return LayoutGraphAccessKind::read;
}

StringRef stringifyLayoutKind(ArrayLayoutKind kind) {
  switch (kind) {
  case ArrayLayoutKind::blockParallel:
    return AttrNames::LayoutGraph::BlockParallel;
  case ArrayLayoutKind::blockContraction:
    return AttrNames::LayoutGraph::BlockContraction;
  case ArrayLayoutKind::replicated:
    return AttrNames::LayoutGraph::Replicated;
  }
  return AttrNames::LayoutGraph::Replicated;
}

ArrayLayoutKind parseLayoutKind(StringRef value) {
  if (value == AttrNames::LayoutGraph::BlockParallel)
    return ArrayLayoutKind::blockParallel;
  if (value == AttrNames::LayoutGraph::BlockContraction)
    return ArrayLayoutKind::blockContraction;
  return ArrayLayoutKind::replicated;
}

StringRef stringifyLayoutGraphRole(LayoutGraphRole role) {
  switch (role) {
  case LayoutGraphRole::read:
    return AttrNames::LayoutGraphValues::RoleRead;
  case LayoutGraphRole::write:
    return AttrNames::LayoutGraphValues::RoleWrite;
  case LayoutGraphRole::unknown:
    return AttrNames::LayoutGraphValues::RoleUnknown;
  }
  return AttrNames::LayoutGraphValues::RoleUnknown;
}

LayoutGraphRole parseLayoutGraphRole(StringRef value) {
  if (value == AttrNames::LayoutGraphValues::RoleRead)
    return LayoutGraphRole::read;
  if (value == AttrNames::LayoutGraphValues::RoleWrite)
    return LayoutGraphRole::write;
  return LayoutGraphRole::unknown;
}

std::optional<LayoutGraphFact> parseArrayLayoutFact(DictionaryAttr dict) {
  if (!dict || !dict.get(AttrNames::LayoutGraph::ArrayId))
    return std::nullopt;
  return parseLayoutCommon(dict);
}

SmallVector<LayoutGraphFact, 4> parseArrayLayoutFacts(ArrayAttr attr) {
  SmallVector<LayoutGraphFact, 4> facts;
  if (!attr)
    return facts;
  for (Attribute entry : attr)
    if (auto dict = dyn_cast<DictionaryAttr>(entry))
      if (std::optional<LayoutGraphFact> fact = parseArrayLayoutFact(dict))
        facts.push_back(std::move(*fact));
  return facts;
}

llvm::MapVector<Value, int64_t>
assignStableArrayIds(const ModuleSuAccessRelations &relations) {
  llvm::MapVector<Value, int64_t> ids;
  int64_t next = 0;
  for (const auto &kv : relations.profiles) {
    const ArrayAccessProfile &profile = kv.second;
    if (profile.rank == 0 || profile.staticShape.empty())
      continue;
    ids.try_emplace(profile.root, next++);
  }
  return ids;
}

SmallVector<CuMuHyperedgePressure, 4>
collectCuMuHyperedgePressures(ArrayRef<LayoutGraphFact> facts) {
  SmallVector<CuMuHyperedgePressure, 4> pressures;
  for (const LayoutGraphFact &fact : facts) {
    if (fact.muBlockCount <= 1)
      continue;
    int64_t remoteFanout = std::max<int64_t>(0, fact.muBlockCount - 1);
    if (remoteFanout <= 0)
      continue;
    int64_t trafficBytes = 1;
    for (int64_t dim : fact.blockShape)
      trafficBytes *= std::max<int64_t>(1, dim);
    pressures.push_back(CuMuHyperedgePressure{remoteFanout, trafficBytes});
  }
  return pressures;
}

SmallVector<CuMuHyperedgePressure, 4>
collectCuMuHyperedgePressures(const LayoutGraph &graph) {
  SmallVector<CuMuHyperedgePressure, 4> pressures;
  for (const MuNet &net : graph.muNets) {
    llvm::DenseSet<unsigned> cuIds;
    for (unsigned pinId : net.pinIds) {
      if (pinId >= graph.muPins.size())
        continue;
      cuIds.insert(graph.muPins[pinId].cuId);
    }
    int64_t remoteFanout =
        std::max<int64_t>(0, static_cast<int64_t>(cuIds.size()) - 1);
    if (remoteFanout <= 0)
      continue;
    pressures.push_back(
        CuMuHyperedgePressure{remoteFanout, defaultMuTrafficBytes(net)});
  }
  return pressures;
}

LayoutGraphBalanceSummary
summarizeLayoutGraphBalance(const LayoutGraph &graph,
                            LayoutGraphBalanceOptions options) {
  return summarizeLayoutGraphBalance(
      graph,
      [](const CuVertex &cu) { return std::max<int64_t>(1, cu.workWeight); },
      [](const MuNet &net) { return defaultMuTrafficBytes(net); }, options);
}

LayoutGraphBalanceSummary summarizeLayoutGraphBalance(
    const LayoutGraph &graph, LayoutGraphCuWeightFn cuWeight,
    LayoutGraphMuTrafficFn muTraffic, LayoutGraphBalanceOptions options) {
  LayoutGraphBalanceSummary summary;
  summary.cuCount = graph.cuVertices.size();
  summary.muCount = graph.muNets.size();
  summary.pinCount = graph.muPins.size();

  for (const CuVertex &cu : graph.cuVertices) {
    int64_t weight = std::max<int64_t>(1, cuWeight(cu));
    summary.totalCuWorkWeight =
        saturatingAddPositive(summary.totalCuWorkWeight, weight);
    summary.maxCuWorkWeight = std::max(summary.maxCuWorkWeight, weight);
  }

  int64_t targetPartitions = options.targetCuPartitions > 0
                                 ? options.targetCuPartitions
                                 : summary.cuCount;
  targetPartitions = std::clamp<int64_t>(targetPartitions, int64_t{1},
                                         std::max<int64_t>(1, summary.cuCount));
  if (summary.totalCuWorkWeight > 0 && targetPartitions > 0) {
    double ideal = static_cast<double>(summary.totalCuWorkWeight) /
                   static_cast<double>(targetPartitions);
    summary.workImbalance =
        ideal > 0.0 ? std::max(0.0, summary.maxCuWorkWeight / ideal - 1.0)
                    : 0.0;
  }

  for (const MuNet &net : graph.muNets) {
    llvm::DenseSet<unsigned> cuIds;
    for (unsigned pinId : net.pinIds) {
      if (pinId >= graph.muPins.size())
        continue;
      cuIds.insert(graph.muPins[pinId].cuId);
    }

    int64_t fanout = static_cast<int64_t>(cuIds.size());
    summary.maxMuFanout = std::max(summary.maxMuFanout, fanout);
    int64_t remoteFanout = std::max<int64_t>(0, fanout - 1);
    if (remoteFanout <= 0)
      continue;

    int64_t trafficBytes = std::max<int64_t>(0, muTraffic(net));
    summary.totalRemoteFanout =
        saturatingAddPositive(summary.totalRemoteFanout, remoteFanout);
    summary.communicationBytes =
        saturatingAddPositive(summary.communicationBytes, trafficBytes);
    summary.fanoutScore += static_cast<double>(saturatingMulPositive(
        remoteFanout, std::max<int64_t>(1, trafficBytes)));
  }

  return summary;
}

AccessRelation makeAccessRelation(Value root, const MemrefAccessEntry &entry,
                                  LayoutGraphAccessKind kind) {
  AccessRelation relation;
  relation.root = stripRoot(root ? root : entry.memref);
  relation.indexingMap = entry.indexingMap;
  relation.op = entry.op;
  relation.kind = kind;
  appendSubscriptsFromMap(entry.indexingMap, relation.subscripts);
  return relation;
}

CuVertex makeCuVertex(unsigned cuId, SdeSuIterateOp op,
                      const SuLoopAccessSummary &summary) {
  CuVertex vertex;
  vertex.id = cuId;
  vertex.op = op;

  for (const MemrefAccessEntry &entry : summary.reads) {
    vertex.accesses.push_back(
        makeAccessRelation(entry.memref, entry, LayoutGraphAccessKind::read));
  }
  for (const MemrefAccessEntry &entry : summary.writes) {
    vertex.accesses.push_back(
        makeAccessRelation(entry.memref, entry, LayoutGraphAccessKind::write));
  }
  return vertex;
}

MuNet makeMuNet(unsigned muId, const ArrayAccessProfile &profile,
                std::optional<AssignedArrayLayout> assignedLayout) {
  MuNet net;
  net.id = muId;
  net.root = stripRoot(profile.root);
  net.elementRank = profile.staticShape.size();
  net.staticShape.assign(profile.staticShape.begin(),
                         profile.staticShape.end());
  net.elementBytes = getElementBytes(net.root);

  if (assignedLayout) {
    net.layoutKind = assignedLayout->layout.kind;
    net.ownerDims.assign(assignedLayout->layout.ownerPositions.begin(),
                         assignedLayout->layout.ownerPositions.end());
    net.blockShape.assign(assignedLayout->layout.blockShape.begin(),
                          assignedLayout->layout.blockShape.end());
    net.muBlockCount =
        inferBlockCount(net.staticShape, net.ownerDims, net.blockShape);
    net.tilePayloadBytes = tilePayloadBytes(net.blockShape, net.elementBytes);
  }

  return net;
}

MuNet makeMuNet(unsigned muId, const CuMuMemoryUnit &memory,
                std::optional<CuMuPartitionChoice> partitionChoice,
                std::optional<ArrayLayoutKind> layoutKind) {
  MuNet net;
  net.id = muId;
  net.elementRank = memory.shape.size();
  net.staticShape.assign(memory.shape.begin(), memory.shape.end());
  net.ownerDims.assign(memory.ownerPhysicalDims.begin(),
                       memory.ownerPhysicalDims.end());
  net.elementBytes = memory.elementBytes;
  if (layoutKind)
    net.layoutKind = *layoutKind;
  else if (!net.ownerDims.empty())
    net.layoutKind = ArrayLayoutKind::blockParallel;

  if (partitionChoice) {
    net.blockShape.assign(partitionChoice->physicalBlockShape.begin(),
                          partitionChoice->physicalBlockShape.end());
    net.tilePayloadBytes = partitionChoice->tilePayloadBytes;
    net.muBlockCount = std::max<int64_t>(1, partitionChoice->computeUnits);
  }
  return net;
}

LayoutGraph buildLayoutGraph(
    const ModuleSuAccessRelations &relations,
    const llvm::MapVector<Value, AssignedArrayLayout> *assignedLayouts) {
  LayoutGraph graph;

  for (SdeSuIterateOp op : relations.schedulingUnits) {
    SuRegion region;
    region.op = op;
    SuRegion &storedRegion = graph.addSuRegion(std::move(region));

    CuVertex vertex;
    vertex.op = op;
    vertex.suRegionId = storedRegion.id;
    graph.addCuVertex(std::move(vertex));
  }

  llvm::MapVector<Value, unsigned> muIds;
  for (const auto &entry : relations.profiles) {
    Value root = entry.first;
    const ArrayAccessProfile &profile = entry.second;
    std::optional<AssignedArrayLayout> assigned;
    if (assignedLayouts) {
      auto it = assignedLayouts->find(root);
      if (it != assignedLayouts->end())
        assigned = it->second;
    }
    MuNet &net = graph.addMuNet(
        makeMuNet(graph.muNets.size(), profile, std::move(assigned)));
    muIds[stripRoot(root)] = net.id;
  }

  for (const auto &entry : relations.profiles) {
    Value root = stripRoot(entry.first);
    auto muIt = muIds.find(root);
    if (muIt == muIds.end())
      continue;
    unsigned muId = muIt->second;
    const ArrayAccessProfile &profile = entry.second;

    for (ArrayRef<ArrayPositionUse> positionUses : profile.positionUses) {
      for (const ArrayPositionUse &use : positionUses) {
        if (use.suId >= graph.cuVertices.size())
          continue;
        AccessRelation relation;
        relation.root = root;
        relation.kind =
            accessKindFromReadWrite(/*isRead=*/!use.isWrite, use.isWrite);

        MuPin pin;
        pin.muId = muId;
        pin.cuId = use.suId;
        pin.relation = std::move(relation);
        graph.addMuPin(std::move(pin));
      }
    }
  }

  return graph;
}

} // namespace mlir::carts::sde
