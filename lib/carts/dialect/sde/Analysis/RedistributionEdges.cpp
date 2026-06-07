///==========================================================================///
/// File: RedistributionEdges.cpp
///
/// See RedistributionEdges.h.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/StructuredOpAnalysis.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

using namespace mlir;

namespace mlir::carts::sde {

namespace {

/// The committed home (producer) layout of one array root.
struct HomeLayout {
  SdeSuIterateOp writer;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

} // namespace

RedistributionEdges collectRedistributionEdges(Operation *moduleOp) {
  RedistributionEdges result;

  // Module access relations + the single arrayId numbering shared with
  // sde-layout-assignment, so a committed `arrayId` joins back to its root.
  ModuleAccessRelations relations = buildModuleAccessRelations(moduleOp);
  llvm::DenseMap<int64_t, Value> rootByArrayId;
  for (const auto &kv : assignStableArrayIds(relations))
    rootByArrayId[kv.second] = kv.first;
  llvm::DenseMap<Operation *, unsigned> suIdOf;
  for (auto [i, su] : llvm::enumerate(relations.schedulingUnits))
    suIdOf[su.getOperation()] = i;

  // Committed home (writer-role) layout per arrayId, read verbatim.
  llvm::DenseMap<int64_t, HomeLayout> homeByArrayId;
  llvm::DenseSet<int64_t> conflictingHome;
  moduleOp->walk([&](SdeSuIterateOp su) {
    ArrayAttr layout = su.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const LayoutGraphFact &f : parseArrayLayoutFacts(layout)) {
      if (f.role != LayoutGraphRole::write)
        continue;
      HomeLayout home;
      home.writer = su;
      home.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
      home.blockShape.assign(f.blockShape.begin(), f.blockShape.end());
      auto it = homeByArrayId.find(f.id);
      if (it == homeByArrayId.end())
        homeByArrayId[f.id] = std::move(home);
      else if (it->second.ownerDims != home.ownerDims ||
               it->second.blockShape != home.blockShape)
        conflictingHome.insert(f.id);
    }
  });

  moduleOp->walk([&](SdeSuIterateOp reader) {
    std::optional<SmallVector<int64_t, 4>> disagree =
        readI64ArrayAttr(reader.getLayoutsDisagreeAttr());
    if (!disagree)
      return;
    auto suIdIt = suIdOf.find(reader.getOperation());

    for (int64_t arrayId : *disagree) {
      auto fail = [&](StringRef reason) {
        result.failures.push_back({reader, arrayId, reason.str()});
      };

      if (conflictingHome.contains(arrayId)) {
        fail("conflicting committed writer layouts; home is non-reconcilable");
        continue;
      }
      auto homeIt = homeByArrayId.find(arrayId);
      if (homeIt == homeByArrayId.end()) {
        fail("redistribution edge has no committed writer layout to "
             "redistribute from");
        continue;
      }
      const HomeLayout &home = homeIt->second;
      SdeSuIterateOp writer = home.writer; // op accessors are non-const
      if (home.ownerDims.empty()) {
        fail("committed home layout is replicated; no partitioned source to "
             "redistribute");
        continue;
      }
      auto rootIt = rootByArrayId.find(arrayId);
      if (rootIt == rootByArrayId.end()) {
        fail("cannot ground the redistribution edge to an array root");
        continue;
      }
      Value root = rootIt->second;
      auto muType = dyn_cast<MemRefType>(root.getType());
      if (!muType || !muType.hasStaticShape()) {
        fail("dynamic array shape has no static redistribution layout");
        continue;
      }
      if (writer.getOperation() == reader.getOperation() ||
          writer.getInPlaceSafe() || reader.getInPlaceSafe()) {
        fail("in-place scheduling unit exposes no redistribution boundary");
        continue;
      }
      if (muRootHasUnsupportedUse(root)) {
        fail("aliasing array-root use blocks redistribution");
        continue;
      }

      // Movement family from the consumer's grounded access kind. Only a
      // cross-OWNER reduction read is currently representable from committed
      // facts: the consumer reduces the home's owned axis, so the target is the
      // home block layout itself (the movement is the reduction, not a
      // re-layout) and no geometry is invented. A re-layout edge
      // (transpose/gather/halo), or a reduction on a non-owner axis, would need
      // the consumer's required target layout, which `sde-layout-assignment`
      // discards — those fail closed.
      bool hasOwnerReduction = false;
      if (suIdIt != suIdOf.end()) {
        auto pit = relations.profiles.find(root);
        if (pit != relations.profiles.end())
          for (auto [pos, posUses] : llvm::enumerate(pit->second.positionUses))
            for (const ArrayPositionUse &use : posUses)
              if (use.suId == suIdIt->second && !use.isWrite &&
                  use.kind == ArrayDimKind::reductionIndexed &&
                  llvm::is_contained(home.ownerDims, static_cast<int64_t>(pos)))
                hasOwnerReduction = true;
      }
      if (!hasOwnerReduction) {
        fail("redistribution edge is not a cross-owner reduction; the target "
             "layout would have to be invented");
        continue;
      }

      RedistributionEdge edge;
      edge.root = root;
      edge.arrayId = arrayId;
      edge.consumer = reader;
      edge.family = SdeMovementFamily::reduce_scatter_like;
      edge.sourceOwnerDims.assign(home.ownerDims.begin(), home.ownerDims.end());
      edge.sourceBlockShape.assign(home.blockShape.begin(),
                                   home.blockShape.end());
      edge.targetOwnerDims = edge.sourceOwnerDims;
      edge.targetBlockShape = edge.sourceBlockShape;

      // Advisory committed edge cost, if the reader stamped one.
      if (ArrayAttr readerLayout = reader.getArrayLayoutAttr())
        for (const LayoutGraphFact &f : parseArrayLayoutFacts(readerLayout))
          if (f.id == arrayId && f.role == LayoutGraphRole::read &&
              f.commVolumeBytes > 0)
            edge.commVolumeBytes = f.commVolumeBytes;

      result.edges.push_back(std::move(edge));
    }
  });

  return result;
}

bool redistMatchesEdge(SdeRedistOp redist, const RedistributionEdge &edge) {
  if (redist.getMu() != edge.root || redist.getFamily() != edge.family)
    return false;
  std::optional<SmallVector<int64_t, 4>> so =
      readI64ArrayAttr(redist.getSourceOwnerDims());
  std::optional<SmallVector<int64_t, 4>> sb =
      readI64ArrayAttr(redist.getSourceBlockShape());
  std::optional<SmallVector<int64_t, 4>> to =
      readI64ArrayAttr(redist.getTargetOwnerDims());
  std::optional<SmallVector<int64_t, 4>> tb =
      readI64ArrayAttr(redist.getTargetBlockShape());
  return so && sb && to && tb &&
         ArrayRef<int64_t>(*so) == ArrayRef<int64_t>(edge.sourceOwnerDims) &&
         ArrayRef<int64_t>(*sb) == ArrayRef<int64_t>(edge.sourceBlockShape) &&
         ArrayRef<int64_t>(*to) == ArrayRef<int64_t>(edge.targetOwnerDims) &&
         ArrayRef<int64_t>(*tb) == ArrayRef<int64_t>(edge.targetBlockShape);
}

} // namespace mlir::carts::sde
