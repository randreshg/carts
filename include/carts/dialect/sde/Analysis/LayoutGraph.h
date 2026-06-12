///==========================================================================///
/// File: LayoutGraph.h
///
/// Neutral SDE layout graph payload.
///
/// These objects model element-space evidence produced by SDE analysis:
/// compute vertices, source-order regions, memory nets, pins, affine access
/// relations, and abstract layout/partitioning cost.  They intentionally stop
/// before any downstream realization vocabulary.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_LAYOUTGRAPH_H
#define CARTS_DIALECT_SDE_ANALYSIS_LAYOUTGRAPH_H

#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <optional>
#include <string>

namespace mlir::carts::sde {

inline constexpr unsigned kLayoutGraphSchemaVersion = 1;

enum class LayoutGraphAccessKind {
  read,
  write,
  readWrite,
  reduction,
};

enum class LayoutGraphRole {
  unknown,
  read,
  write,
};

struct LayoutGraphProvenance {
  std::string producerLayer = "sde";
  unsigned schemaVersion = kLayoutGraphSchemaVersion;
  std::string factId;
  std::string sourceFactId;
  bool committed = false;
};

struct AccessRelation {
  Value root;
  AffineMap indexingMap;
  Operation *op = nullptr;
  LayoutGraphAccessKind kind = LayoutGraphAccessKind::read;
  SmallVector<AffineDimOffset, 4> subscripts;

  bool reads() const;
  bool writes() const;
};

struct MuPin {
  unsigned id = 0;
  unsigned muId = 0;
  unsigned cuId = 0;
  AccessRelation relation;
};

struct CuVertex {
  unsigned id = 0;
  SdeSuIterateOp op;
  std::optional<unsigned> suRegionId;
  int64_t workWeight = 1;
  SmallVector<AccessRelation, 4> accesses;
  SmallVector<unsigned, 4> pinIds;
};

struct SuRegion {
  unsigned id = 0;
  SdeSuIterateOp op;
  SmallVector<unsigned, 4> cuIds;
};

struct MuNet {
  unsigned id = 0;
  Value root;
  unsigned elementRank = 0;
  SmallVector<int64_t, 4> staticShape;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
  ArrayLayoutKind layoutKind = ArrayLayoutKind::replicated;
  int64_t elementBytes = 0;
  int64_t commVolumeBytes = 0;
  int64_t muBlockCount = 1;
  int64_t tilePayloadBytes = 0;
  SmallVector<unsigned, 4> pinIds;
};

struct LayoutGraph {
  LayoutGraphProvenance provenance;
  SmallVector<SuRegion, 4> suRegions;
  SmallVector<CuVertex, 8> cuVertices;
  SmallVector<MuNet, 8> muNets;
  SmallVector<MuPin, 16> muPins;

  SuRegion &addSuRegion(SuRegion region);
  CuVertex &addCuVertex(CuVertex vertex);
  MuNet &addMuNet(MuNet net);
  MuPin &addMuPin(MuPin pin);

  MuNet *findMuNetByRoot(Value root);
  CuVertex *findCuVertex(unsigned cuId);
};

/// Typed view of the compatibility string attrs that existing passes still
/// write while the query model is being consolidated.
struct LayoutGraphFact {
  int64_t id = -1;
  LayoutGraphRole role = LayoutGraphRole::unknown;
  ArrayLayoutKind layoutKind = ArrayLayoutKind::replicated;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
  // Node-agnostic, budget-sized DB/MU grain (a function of problem size + a
  // target block-byte budget, not of node/worker count). Empty/1 on facts that
  // predate the N-node migration or carry no budget grain.
  SmallVector<int64_t, 4> budgetBlockShape;
  int64_t muBlockCount = 1;
  int64_t commVolumeBytes = 0;
};

/// Runtime-neutral graph balance summary. The score fields are abstract SDE
/// evidence only: they name no storage object, route, rank, task, or
/// communication operation.
struct LayoutGraphBalanceOptions {
  int64_t targetCuPartitions = 0;
};

struct LayoutGraphBalanceSummary {
  int64_t cuCount = 0;
  int64_t muCount = 0;
  int64_t pinCount = 0;
  int64_t totalCuWorkWeight = 0;
  int64_t maxCuWorkWeight = 0;
  double workImbalance = 0.0;
  int64_t maxMuFanout = 0;
  int64_t totalRemoteFanout = 0;
  int64_t communicationBytes = 0;
  double fanoutScore = 0.0;
};

using LayoutGraphCuWeightFn = llvm::function_ref<int64_t(const CuVertex &)>;
using LayoutGraphMuTrafficFn = llvm::function_ref<int64_t(const MuNet &)>;

LayoutGraphAccessKind accessKindFromReadWrite(bool isRead, bool isWrite);

StringRef stringifyLayoutKind(ArrayLayoutKind kind);
ArrayLayoutKind parseLayoutKind(StringRef value);
StringRef stringifyLayoutGraphRole(LayoutGraphRole role);
LayoutGraphRole parseLayoutGraphRole(StringRef value);

std::optional<LayoutGraphFact> parseArrayLayoutFact(DictionaryAttr dict);
SmallVector<LayoutGraphFact, 4> parseArrayLayoutFacts(ArrayAttr attr);

/// Reproduce the module-stable `arrayId` numbering that `sde-layout-assignment`
/// writes into the per-array layout facts: every array root in
/// `relations.profiles` gets an incrementing id in MapVector insertion order,
/// skipping rank-0 / empty-shape profiles. Any consumer that joins an
/// `arrayLayout` `arrayId` back to its array root (e.g. redistribution
/// realization) MUST use this one numbering so producer and consumer never
/// drift.
llvm::MapVector<Value, int64_t>
assignStableArrayIds(const ModuleSuAccessRelations &relations);
SmallVector<CuMuHyperedgePressure, 4>
collectCuMuHyperedgePressures(ArrayRef<LayoutGraphFact> facts);
SmallVector<CuMuHyperedgePressure, 4>
collectCuMuHyperedgePressures(const LayoutGraph &graph);

LayoutGraphBalanceSummary
summarizeLayoutGraphBalance(const LayoutGraph &graph,
                            LayoutGraphBalanceOptions options = {});
LayoutGraphBalanceSummary summarizeLayoutGraphBalance(
    const LayoutGraph &graph, LayoutGraphCuWeightFn cuWeight,
    LayoutGraphMuTrafficFn muTraffic, LayoutGraphBalanceOptions options = {});

AccessRelation makeAccessRelation(Value root, const MemrefAccessEntry &entry,
                                  LayoutGraphAccessKind kind);
CuVertex makeCuVertex(unsigned cuId, SdeSuIterateOp op,
                      const SuLoopAccessSummary &summary);
MuNet makeMuNet(
    unsigned muId, const ArrayAccessProfile &profile,
    std::optional<AssignedArrayLayout> assignedLayout = std::nullopt);
MuNet makeMuNet(
    unsigned muId, const CuMuMemoryUnit &memory,
    std::optional<CuMuPartitionChoice> partitionChoice = std::nullopt,
    std::optional<ArrayLayoutKind> layoutKind = std::nullopt);

/// Build the first neutral graph slice from existing module access facts.
/// Current SDE code still writes compatibility attrs separately; this helper
/// lets new analysis and future payload writers consume the same typed model.
LayoutGraph buildLayoutGraph(const ModuleSuAccessRelations &relations,
                             const llvm::MapVector<Value, AssignedArrayLayout>
                                 *assignedLayouts = nullptr);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_ANALYSIS_LAYOUTGRAPH_H
