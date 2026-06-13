///==========================================================================///
/// File: ElementwiseFusion.cpp
///
/// Fuse consecutive sibling SDE elementwise scheduling units before boundary
/// conversion.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_ELEMENTWISEFUSION
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

struct MemrefAccess {
  Value root;
  SmallVector<Value, 4> indices;
};

struct ElementwiseStage {
  sde::SdeSuIterateOp op;
  Operation *root = nullptr;
  SmallVector<MemrefAccess, 4> reads;
  SmallVector<MemrefAccess, 4> writes;
};

struct MergedLayoutEntry {
  DictionaryAttr entry;
  bool sawWrite = false;
  int64_t commVolumeBytes = 0;
};

static bool isSkippableInterStageOp(Operation *op) {
  return op && op->getNumRegions() == 0 && isMemoryEffectFree(op);
}

static DictionaryAttr
rebuildMergedLayoutEntry(MLIRContext *ctx, const MergedLayoutEntry &merged) {
  if (!merged.entry)
    return {};
  Builder builder(ctx);
  SmallVector<NamedAttribute, 10> attrs;
  bool sawRole = false;
  bool sawComm = false;
  attrs.reserve(merged.entry.size());
  for (NamedAttribute attr : merged.entry) {
    if (attr.getName() == sde::AttrNames::LayoutGraph::Role) {
      sawRole = true;
      attrs.push_back(builder.getNamedAttr(
          sde::AttrNames::LayoutGraph::Role,
          builder.getStringAttr(
              merged.sawWrite ? sde::AttrNames::LayoutGraphValues::RoleWrite
                              : sde::AttrNames::LayoutGraphValues::RoleRead)));
      continue;
    }
    if (attr.getName() == sde::AttrNames::LayoutGraph::CommVolumeBytes) {
      sawComm = true;
      attrs.push_back(builder.getNamedAttr(
          sde::AttrNames::LayoutGraph::CommVolumeBytes,
          builder.getI64IntegerAttr(merged.commVolumeBytes)));
      continue;
    }
    attrs.push_back(attr);
  }
  if (!sawRole)
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::LayoutGraph::Role,
        builder.getStringAttr(
            merged.sawWrite ? sde::AttrNames::LayoutGraphValues::RoleWrite
                            : sde::AttrNames::LayoutGraphValues::RoleRead)));
  if (!sawComm)
    attrs.push_back(builder.getNamedAttr(
        sde::AttrNames::LayoutGraph::CommVolumeBytes,
        builder.getI64IntegerAttr(merged.commVolumeBytes)));
  return builder.getDictionaryAttr(attrs);
}

static void applyMergedLayoutAttrs(sde::SdeSuIterateOp fused,
                                   MutableArrayRef<ElementwiseStage> stages) {
  MLIRContext *ctx = fused.getContext();
  Builder builder(ctx);
  SmallVector<int64_t, 4> order;
  llvm::DenseMap<int64_t, unsigned> indexByArrayId;
  SmallVector<MergedLayoutEntry, 4> entries;
  llvm::SmallDenseSet<int64_t, 4> disagreeIds;

  for (ElementwiseStage &stage : stages) {
    if (auto disagree = stage.op.getLayoutsDisagreeAttr()) {
      for (Attribute attr : disagree)
        if (auto id = dyn_cast<IntegerAttr>(attr); id && id.getInt() >= 0)
          disagreeIds.insert(id.getInt());
    }

    ArrayAttr layout = stage.op.getArrayLayoutAttr();
    if (!layout)
      continue;
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<sde::LayoutGraphFact> fact =
          sde::parseArrayLayoutFact(dict);
      if (!dict || !fact || fact->id < 0)
        continue;
      int64_t arrayId = fact->id;
      auto [it, inserted] = indexByArrayId.try_emplace(arrayId, entries.size());
      if (inserted) {
        order.push_back(arrayId);
        entries.push_back(
            MergedLayoutEntry{dict, fact->role == sde::LayoutGraphRole::write,
                              std::max<int64_t>(0, fact->commVolumeBytes)});
        continue;
      }

      MergedLayoutEntry &merged = entries[it->second];
      bool candidateWrites = fact->role == sde::LayoutGraphRole::write;
      int64_t candidateComm = std::max<int64_t>(0, fact->commVolumeBytes);
      merged.commVolumeBytes += candidateComm;
      if (candidateComm > 0)
        disagreeIds.insert(arrayId);
      if (candidateWrites && !merged.sawWrite) {
        merged.entry = dict;
        merged.sawWrite = true;
      } else if (!merged.sawWrite && candidateComm > 0 &&
                 sde::parseArrayLayoutFact(merged.entry)->commVolumeBytes ==
                     0) {
        merged.entry = dict;
      }
    }
  }

  if (entries.empty()) {
    fused->removeAttr(fused.getArrayLayoutAttrName());
    fused->removeAttr(fused.getLayoutsDisagreeAttrName());
    fused->removeAttr(fused.getCommVolumeBytesAttrName());
    return;
  }

  SmallVector<Attribute, 4> layoutAttrs;
  layoutAttrs.reserve(order.size());
  int64_t totalCommBytes = 0;
  for (int64_t arrayId : order) {
    MergedLayoutEntry &merged = entries[indexByArrayId.lookup(arrayId)];
    totalCommBytes += merged.commVolumeBytes;
    layoutAttrs.push_back(rebuildMergedLayoutEntry(ctx, merged));
  }
  fused.setArrayLayoutAttr(builder.getArrayAttr(layoutAttrs));
  fused.setCommVolumeBytesAttr(builder.getI64IntegerAttr(totalCommBytes));

  SmallVector<Attribute, 4> disagreeAttrs;
  for (int64_t arrayId : order)
    if (disagreeIds.contains(arrayId))
      disagreeAttrs.push_back(builder.getI64IntegerAttr(arrayId));
  if (disagreeAttrs.empty())
    fused->removeAttr(fused.getLayoutsDisagreeAttrName());
  else
    fused.setLayoutsDisagreeAttr(builder.getArrayAttr(disagreeAttrs));
}

static std::optional<sde::SdeAccessMode>
modeForRole(sde::LayoutGraphRole role) {
  switch (role) {
  case sde::LayoutGraphRole::read:
    return sde::SdeAccessMode::read;
  case sde::LayoutGraphRole::write:
    return sde::SdeAccessMode::write;
  default:
    return std::nullopt;
  }
}

static void insertMergedLayoutRoots(sde::SdeSuIterateOp fused,
                                    MutableArrayRef<ElementwiseStage> stages,
                                    IRRewriter &rewriter) {
  ArrayAttr layout = fused.getArrayLayoutAttr();
  if (!layout)
    return;

  struct RootKey {
    int64_t arrayId = -1;
    sde::SdeAccessMode mode = sde::SdeAccessMode::read;
  };
  SmallVector<RootKey, 4> needed;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    std::optional<sde::SdeAccessMode> mode = modeForRole(fact.role);
    if (mode && fact.id >= 0)
      needed.push_back({fact.id, *mode});
  }
  if (needed.empty())
    return;

  Block &body = sde::ensureBlock(fused.getBody());
  OpBuilder::InsertionGuard guard(rewriter);
  rewriter.setInsertionPointToStart(&body);

  for (RootKey key : needed) {
    bool alreadyPresent = false;
    for (auto existing : body.getOps<sde::SdeArrayLayoutRootOp>()) {
      if (static_cast<int64_t>(existing.getArrayId()) == key.arrayId &&
          existing.getMode() == key.mode) {
        alreadyPresent = true;
        break;
      }
    }
    if (alreadyPresent)
      continue;

    for (ElementwiseStage &stage : stages) {
      bool inserted = false;
      for (auto root :
           stage.op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
        if (static_cast<int64_t>(root.getArrayId()) != key.arrayId ||
            root.getMode() != key.mode)
          continue;
        sde::SdeArrayLayoutRootOp::create(rewriter, root.getLoc(),
                                          root.getRoot(), root.getModeAttr(),
                                          root.getArrayIdAttr());
        inserted = true;
        break;
      }
      if (inserted)
        break;
    }
  }
}

static sde::SdeSuIterateOp getStageSuIterate(Operation *op) {
  if (auto suIter = dyn_cast_or_null<sde::SdeSuIterateOp>(op))
    return suIter;

  auto cuRegion = dyn_cast_or_null<sde::SdeCuRegionOp>(op);
  if (!cuRegion || cuRegion.getKind() != sde::SdeCuKind::parallel ||
      !cuRegion.getBody().hasOneBlock())
    return {};

  sde::SdeSuIterateOp nested;
  for (Operation &inner : cuRegion.getBody().front().without_terminator()) {
    if (auto suIter = dyn_cast<sde::SdeSuIterateOp>(inner)) {
      if (nested)
        return {};
      nested = suIter;
      continue;
    }
    if (isSkippableInterStageOp(&inner))
      continue;
    return {};
  }
  return nested;
}

static bool areStageValuesEquivalent(Value lhs, sde::SdeSuIterateOp lhsStage,
                                     Value rhs, sde::SdeSuIterateOp rhsStage,
                                     unsigned depth = 0);

static bool isCorrespondingStageLocalForIv(Value lhs,
                                           sde::SdeSuIterateOp lhsStage,
                                           Value rhs,
                                           sde::SdeSuIterateOp rhsStage,
                                           unsigned depth) {
  auto lhsArg = dyn_cast<BlockArgument>(lhs);
  auto rhsArg = dyn_cast<BlockArgument>(rhs);
  if (!lhsArg || !rhsArg || lhsArg.getArgNumber() != 0 ||
      rhsArg.getArgNumber() != 0)
    return false;

  auto lhsFor = dyn_cast_or_null<scf::ForOp>(lhsArg.getOwner()->getParentOp());
  auto rhsFor = dyn_cast_or_null<scf::ForOp>(rhsArg.getOwner()->getParentOp());
  if (!lhsFor || !rhsFor)
    return false;
  if (lhsFor->getParentOfType<sde::SdeSuIterateOp>() != lhsStage ||
      rhsFor->getParentOfType<sde::SdeSuIterateOp>() != rhsStage)
    return false;

  if (!areStageValuesEquivalent(lhsFor.getLowerBound(), lhsStage,
                                rhsFor.getLowerBound(), rhsStage, depth + 1))
    return false;
  if (!areStageValuesEquivalent(lhsFor.getUpperBound(), lhsStage,
                                rhsFor.getUpperBound(), rhsStage, depth + 1))
    return false;
  if (!areStageValuesEquivalent(lhsFor.getStep(), lhsStage, rhsFor.getStep(),
                                rhsStage, depth + 1))
    return false;
  return true;
}

static SmallVector<Value, 4> mapValues(ValueRange values, IRMapping &mapping) {
  SmallVector<Value, 4> mapped;
  mapped.reserve(values.size());
  for (Value value : values)
    mapped.push_back(mapping.lookupOrDefault(value));
  return mapped;
}

static void cloneSkippablePrefixOps(Operation *root, sde::SdeSuIterateOp until,
                                    IRRewriter &rewriter, IRMapping &mapping) {
  auto cuRegion = dyn_cast_or_null<sde::SdeCuRegionOp>(root);
  if (!cuRegion || !cuRegion.getBody().hasOneBlock())
    return;

  for (Operation &inner : cuRegion.getBody().front().without_terminator()) {
    if (&inner == until.getOperation())
      return;
    if (!isSkippableInterStageOp(&inner))
      return;
    rewriter.clone(inner, mapping);
  }
}

static bool haveSameIterationSpace(sde::SdeSuIterateOp lhs,
                                   sde::SdeSuIterateOp rhs) {
  if (lhs.getLowerBounds().size() != rhs.getLowerBounds().size() ||
      lhs.getUpperBounds().size() != rhs.getUpperBounds().size() ||
      lhs.getSteps().size() != rhs.getSteps().size())
    return false;

  for (auto [a, b] : llvm::zip(lhs.getLowerBounds(), rhs.getLowerBounds())) {
    if (!::mlir::carts::ValueAnalysis::areValuesEquivalent(a, b))
      return false;
  }
  for (auto [a, b] : llvm::zip(lhs.getUpperBounds(), rhs.getUpperBounds())) {
    if (!::mlir::carts::ValueAnalysis::areValuesEquivalent(a, b))
      return false;
  }
  for (auto [a, b] : llvm::zip(lhs.getSteps(), rhs.getSteps())) {
    if (!::mlir::carts::ValueAnalysis::areValuesEquivalent(a, b))
      return false;
  }
  return true;
}

static bool haveCompatibleSchedule(sde::SdeSuIterateOp lhs,
                                   sde::SdeSuIterateOp rhs) {
  return lhs.getNowaitAttr() == rhs.getNowaitAttr();
}

static Value getWriteRoot(Value value) {
  return ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
}

static bool hasDisjointWrites(ArrayRef<ElementwiseStage> stages) {
  llvm::DenseSet<Value> writtenTargets;
  for (const ElementwiseStage &stage : stages) {
    for (const MemrefAccess &write : stage.writes) {
      if (!write.root || !writtenTargets.insert(write.root).second)
        return false;
    }
  }
  return true;
}

static bool hasWriteToRoot(const ElementwiseStage &stage, Value root) {
  for (const MemrefAccess &write : stage.writes)
    if (write.root == root)
      return true;
  return false;
}

static bool isCorrespondingStageArg(Value lhs, sde::SdeSuIterateOp lhsStage,
                                    Value rhs, sde::SdeSuIterateOp rhsStage) {
  auto lhsArg = dyn_cast<BlockArgument>(lhs);
  auto rhsArg = dyn_cast<BlockArgument>(rhs);
  if (!lhsArg || !rhsArg)
    return false;
  if (lhsArg.getOwner() != &lhsStage.getBody().front() ||
      rhsArg.getOwner() != &rhsStage.getBody().front())
    return false;
  return lhsArg.getArgNumber() == rhsArg.getArgNumber();
}

static bool areStageValuesEquivalent(Value lhs, sde::SdeSuIterateOp lhsStage,
                                     Value rhs, sde::SdeSuIterateOp rhsStage,
                                     unsigned depth) {
  if (!lhs || !rhs || depth > 8)
    return false;
  lhs = ::mlir::carts::ValueAnalysis::stripNumericCasts(lhs);
  rhs = ::mlir::carts::ValueAnalysis::stripNumericCasts(rhs);
  if (::mlir::carts::ValueAnalysis::sameValue(lhs, rhs))
    return true;
  if (isCorrespondingStageArg(lhs, lhsStage, rhs, rhsStage))
    return true;
  if (isCorrespondingStageLocalForIv(lhs, lhsStage, rhs, rhsStage, depth))
    return true;

  auto lhsConst = ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(lhs);
  auto rhsConst = ::mlir::carts::ValueAnalysis::tryFoldConstantIndex(rhs);
  if (lhsConst || rhsConst)
    return lhsConst && rhsConst && *lhsConst == *rhsConst;

  Operation *lhsDef = lhs.getDefiningOp();
  Operation *rhsDef = rhs.getDefiningOp();
  if (!lhsDef || !rhsDef || lhsDef->getName() != rhsDef->getName() ||
      lhsDef->getNumOperands() != rhsDef->getNumOperands())
    return false;

  for (auto [lhsOperand, rhsOperand] :
       llvm::zip(lhsDef->getOperands(), rhsDef->getOperands()))
    if (!areStageValuesEquivalent(lhsOperand, lhsStage, rhsOperand, rhsStage,
                                  depth + 1))
      return false;
  return true;
}

static bool areStageAccessIndicesEquivalent(ValueRange lhs,
                                            sde::SdeSuIterateOp lhsStage,
                                            ValueRange rhs,
                                            sde::SdeSuIterateOp rhsStage) {
  if (lhs.size() != rhs.size())
    return false;
  for (auto [lhsIndex, rhsIndex] : llvm::zip(lhs, rhs))
    if (!areStageValuesEquivalent(lhsIndex, lhsStage, rhsIndex, rhsStage))
      return false;
  return true;
}

static bool readMatchesProducerWrite(const MemrefAccess &read,
                                     const ElementwiseStage &consumer,
                                     const ElementwiseStage &producer) {
  for (const MemrefAccess &write : producer.writes) {
    if (write.root != read.root)
      continue;
    if (areStageAccessIndicesEquivalent(read.indices, consumer.op,
                                        write.indices, producer.op))
      return true;
  }
  return false;
}

static bool hasUnsafeReadAfterWrite(ArrayRef<ElementwiseStage> stages) {
  for (unsigned consumerIdx = 1; consumerIdx < stages.size(); ++consumerIdx) {
    const ElementwiseStage &consumer = stages[consumerIdx];
    for (const MemrefAccess &read : consumer.reads) {
      for (unsigned producerIdx = 0; producerIdx < consumerIdx; ++producerIdx) {
        const ElementwiseStage &producer = stages[producerIdx];
        if (!hasWriteToRoot(producer, read.root))
          continue;
        if (!readMatchesProducerWrite(read, consumer, producer))
          return true;
      }
    }
  }
  return false;
}

static bool isElementwiseStage(Operation *root, ElementwiseStage &stage) {
  sde::SdeSuIterateOp op = getStageSuIterate(root);
  if (!op || !op.getStructuredClassificationAttr() ||
      *op.getStructuredClassification() !=
          sde::SdeStructuredClassification::elementwise)
    return false;
  if (!op.getReductionAccumulators().empty())
    return false;

  llvm::DenseSet<Value> seenWrites;
  SmallVector<MemrefAccess, 4> reads;
  SmallVector<MemrefAccess, 4> writes;
  bool hasDuplicateWrite = false;

  op.getBody().walk([&](memref::LoadOp loadOp) {
    Value target = getWriteRoot(loadOp.getMemref());
    if (!target)
      return;
    if (sde::isDefinedInside(op.getOperation(), target))
      return;
    reads.push_back(
        MemrefAccess{target, SmallVector<Value, 4>(loadOp.getIndices())});
  });
  op.getBody().walk([&](memref::StoreOp storeOp) {
    Value target = getWriteRoot(storeOp.getMemref());
    if (!target)
      return;
    if (sde::isDefinedInside(op.getOperation(), target))
      return;
    if (!seenWrites.insert(target).second) {
      hasDuplicateWrite = true;
      return;
    }
    writes.push_back(
        MemrefAccess{target, SmallVector<Value, 4>(storeOp.getIndices())});
  });

  if (writes.empty() || hasDuplicateWrite)
    return false;

  stage = {op, root, std::move(reads), std::move(writes)};
  return true;
}

static sde::SdeSuIterateOp fuseStages(MutableArrayRef<ElementwiseStage> stages,
                                      IRRewriter &rewriter) {
  assert(stages.size() >= 2 && "expected at least two stages");

  sde::SdeSuIterateOp first = stages.front().op;
  Location loc = first.getLoc();

  IRMapping fusedOperandMapping;
  rewriter.setInsertionPoint(stages.back().root);

  SmallVector<Value, 4> lowerBounds =
      mapValues(first.getLowerBounds(), fusedOperandMapping);
  SmallVector<Value, 4> upperBounds =
      mapValues(first.getUpperBounds(), fusedOperandMapping);
  SmallVector<Value, 4> steps =
      mapValues(first.getSteps(), fusedOperandMapping);

  sde::SuIterateAttrs suAttrs = sde::SuIterateAttrs::fromOp(first);
  suAttrs.structuredClassification = sde::SdeStructuredClassificationAttr::get(
      first.getContext(), sde::SdeStructuredClassification::elementwise_pipeline);
  suAttrs.pattern = sde::SdePatternAttr::get(
      first.getContext(), sde::SdePattern::elementwise_pipeline);
  auto fused = sde::buildSuIterate(rewriter, loc, lowerBounds, upperBounds, steps,
                                   suAttrs, first.getReductionAccumulators());
  fused->setAttrs(sde::getRewrittenAttrs(first));
  fused.setStructuredClassificationAttr(
      sde::SdeStructuredClassificationAttr::get(
          first.getContext(),
          sde::SdeStructuredClassification::elementwise_pipeline));
  fused.setPatternAttr(sde::SdePatternAttr::get(
      first.getContext(), sde::SdePattern::elementwise_pipeline));
  applyMergedLayoutAttrs(fused, stages);

  Block &dst = sde::ensureBlock(fused.getBody());
  if (dst.getNumArguments() == 0) {
    for (BlockArgument arg : stages.front().op.getBody().front().getArguments())
      dst.addArgument(arg.getType(), loc);
  }

  OpBuilder::InsertionGuard guard(rewriter);
  rewriter.setInsertionPointToStart(&dst);
  insertMergedLayoutRoots(fused, stages, rewriter);
  Operation *lastRoot = nullptr;
  for (Operation &op : dst) {
    if (!isa<sde::SdeArrayLayoutRootOp>(op))
      break;
    lastRoot = &op;
  }
  if (lastRoot)
    rewriter.setInsertionPointAfter(lastRoot);
  else
    rewriter.setInsertionPointToStart(&dst);
  auto innerCuRegion = sde::buildCuRegion(
      rewriter, loc,
      sde::SdeCuKindAttr::get(rewriter.getContext(), sde::SdeCuKind::parallel));
  Block &innerBlk = sde::ensureBlock(innerCuRegion.getBody());
  rewriter.setInsertionPointToStart(&innerBlk);

  for (ElementwiseStage &stage : stages) {
    Block *srcBody = sde::getSuIterateComputeBlock(stage.op);
    IRMapping mapper;
    // Map the su_iterate block args (induction vars).
    for (auto [srcArg, dstArg] : llvm::zip(
             stage.op.getBody().front().getArguments(), dst.getArguments()))
      mapper.map(srcArg, dstArg);
    cloneSkippablePrefixOps(stage.root, stage.op, rewriter, mapper);
    for (Operation &nested : srcBody->without_terminator())
      rewriter.clone(nested, mapper);
  }
  sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

  // Yield at su_iterate level.
  rewriter.setInsertionPointAfter(innerCuRegion);
  sde::SdeYieldOp::create(rewriter, loc, ValueRange{});

  return fused;
}

struct ElementwiseFusionPass
    : public sde::impl::ElementwiseFusionBase<ElementwiseFusionPass> {
  void runOnOperation() override {
    bool changed = true;
    while (changed) {
      changed = false;

      SmallVector<Block *> blocks;
      getOperation().walk([&](Operation *op) {
        for (Region &region : op->getRegions())
          for (Block &block : region)
            blocks.push_back(&block);
      });

      for (Block *block : blocks) {
        if (!block)
          continue;

        for (auto it = block->begin(), e = block->end(); it != e; ++it) {
          ElementwiseStage firstStage;
          if (!isElementwiseStage(&*it, firstStage))
            continue;
          sde::SdeSuIterateOp first = firstStage.op;

          SmallVector<ElementwiseStage, 4> stages;
          stages.push_back(std::move(firstStage));
          for (auto nextIt = std::next(it); nextIt != e; ++nextIt) {
            if (isSkippableInterStageOp(&*nextIt))
              continue;

            ElementwiseStage nextStage;
            if (!isElementwiseStage(&*nextIt, nextStage) ||
                !haveSameIterationSpace(first, nextStage.op) ||
                !haveCompatibleSchedule(first, nextStage.op))
              break;

            stages.push_back(std::move(nextStage));
            if (!hasDisjointWrites(stages) || hasUnsafeReadAfterWrite(stages)) {
              stages.pop_back();
              break;
            }
          }

          if (stages.size() < 2)
            continue;

          IRRewriter rewriter(&getContext());
          sde::SdeSuIterateOp fused = fuseStages(stages, rewriter);
          (void)fused;
          for (const ElementwiseStage &stage : stages)
            rewriter.eraseOp(stage.root);
          changed = true;
          break;
        }

        if (changed)
          break;
      }
    }
  }
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createElementwiseFusionPass() {
  return std::make_unique<ElementwiseFusionPass>();
}

} // namespace mlir::carts::sde
