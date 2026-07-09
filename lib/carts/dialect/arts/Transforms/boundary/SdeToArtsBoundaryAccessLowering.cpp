///==========================================================================///
/// File: SdeToArtsBoundaryAccessLowering.cpp
/// SDE access-window analysis, halo realization, and carrier lowering.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCoarseSu.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCuTask.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHaloLowering.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundarySuIterate.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbLayoutFactsUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineMap.h"
#include "llvm/ADT/DenseSet.h"
#include <algorithm>
#include <functional>
#include <limits>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

static bool hasBlockAlignedDispatchBase(Value lower, int64_t workerSpan,
                                        int64_t coordinateBlockSize) {
  if (workerSpan <= 0 || coordinateBlockSize <= 0 ||
      workerSpan % coordinateBlockSize != 0)
    return false;
  int64_t lowerConstant = 0;
  return ValueAnalysis::getConstantIndex(lower, lowerConstant) &&
         lowerConstant % coordinateBlockSize == 0;
}

static int64_t getStaticAccessGroupCount(int64_t workerSpan,
                                         int64_t coordinateBlockSize,
                                         int64_t minElementOffset,
                                         int64_t maxElementOffset,
                                         bool exactBlockAlignedDispatch) {
  if (workerSpan <= 0 || coordinateBlockSize <= 0)
    return 1;
  if (exactBlockAlignedDispatch && minElementOffset == 0 &&
      maxElementOffset == 0)
    return std::max<int64_t>(
        1, ceilDivPositiveI64(workerSpan, coordinateBlockSize));
  int64_t offsetSpan =
      std::max<int64_t>(0, maxElementOffset - minElementOffset);
  int64_t spanForStaticBound =
      workerSpan + coordinateBlockSize - 1 + offsetSpan;
  return std::max<int64_t>(
      1, ceilDivPositiveI64(spanForStaticBound, coordinateBlockSize));
}

static LogicalResult
validatePositiveGroupBlockCounts(sde::SdeSuIterateOp source,
                                 ArrayRef<int64_t> groupBlockCounts,
                                 unsigned ownerDimCount) {
  if (groupBlockCounts.size() != ownerDimCount)
    return source.emitOpError()
           << "commits groupBlockCount whose rank does not match the "
              "committed owner rank";
  for (int64_t count : groupBlockCounts)
    if (count <= 0)
      return source.emitOpError()
             << "commits groupBlockCount that cannot be represented as "
                "a whole-number group of physical DB blocks";
  return success();
}

static bool hasCleanPayloadElementShape(DirectDepSpec &dep) {
  return dep.alloc.getElementSizes().size() == dep.validExtents.size();
}

static bool hasRankExpandedPayloadElementShape(DirectDepSpec &dep) {
  return dep.alloc.getElementSizes().size() ==
         dep.ownerDimCount + dep.validExtents.size();
}

static FailureOr<unsigned> getCommittedPayloadRank(sde::SdeSuIterateOp source,
                                                   DirectDepSpec &dep,
                                                   StringRef diagnosticName) {
  if (dep.validExtents.empty()) {
    return source.emitOpError()
           << "commits an empty access-window payload shape for ARTS "
           << diagnosticName << " realization";
  }
  if (dep.alloc.getSizes().size() != dep.ownerDimCount ||
      (!hasCleanPayloadElementShape(dep) &&
       !hasRankExpandedPayloadElementShape(dep))) {
    return source.emitOpError()
           << "commits a rank shape that ARTS " << diagnosticName
           << " realization cannot represent; refusing a full-block halo "
              "byte-window";
  }
  return static_cast<unsigned>(dep.validExtents.size());
}

static unsigned getDbPayloadElementBase(DirectDepSpec &dep) {
  return hasCleanPayloadElementShape(dep) ? 0 : dep.ownerDimCount;
}

static FailureOr<Value> getPayloadElementExtent(sde::SdeSuIterateOp source,
                                                DirectDepSpec &dep,
                                                unsigned payloadDim,
                                                StringRef name) {
  if (payloadDim >= dep.validExtents.size())
    return source.emitOpError()
           << "access-window valid extent rank does not cover " << name;
  unsigned elementDim = getDbPayloadElementBase(dep) + payloadDim;
  if (elementDim >= dep.alloc.getElementSizes().size())
    return source.emitOpError()
           << "DB payload shape does not cover access-window " << name;
  FailureOr<int64_t> extent = requireStaticPositiveIndex(
      dep.alloc.getElementSizes()[elementDim], source.getOperation(), name);
  if (failed(extent))
    return failure();
  if (*extent != dep.validExtents[payloadDim])
    return source.emitOpError()
           << "DB payload extent disagrees with SDE access-window " << name;
  return dep.alloc.getElementSizes()[elementDim];
}

static void appendElementWindowForPayloadShape(OpBuilder &builder, Location loc,
                                               bool cleanPayloadShape,
                                               unsigned ownerDimCount,
                                               ArrayRef<Value> payloadOffsets,
                                               ArrayRef<Value> payloadSizes,
                                               SmallVectorImpl<Value> &offsets,
                                               SmallVectorImpl<Value> &sizes) {
  offsets.clear();
  sizes.clear();
  if (!cleanPayloadShape) {
    for (unsigned idx = 0; idx < ownerDimCount; ++idx) {
      offsets.push_back(createZeroIndex(builder, loc));
      sizes.push_back(createOneIndex(builder, loc));
    }
  }
  offsets.append(payloadOffsets.begin(), payloadOffsets.end());
  sizes.append(payloadSizes.begin(), payloadSizes.end());
}

class BoundaryTaskDependencyBuilder {
public:
  BoundaryTaskDependencyBuilder(MLIRContext *context,
                                bool recordPartialReductionMaps)
      : context(context),
        recordPartialReductionMaps(recordPartialReductionMaps) {}

  void reserve(unsigned count) {
    deps.reserve(count);
    if (recordPartialReductionMaps)
      partialReductionDepResultDimMaps.reserve(count);
    depBlockOffsets.reserve(count);
    depRequiresDbRef.reserve(count);
    depOwnerDimCounts.reserve(count);
    depGroupBlockCounts.reserve(count);
  }

  unsigned append(Value depPtr, ArrayRef<Value> blockOffsets,
                  bool requiresDbRef, unsigned ownerDimCount,
                  ArrayRef<int64_t> groupBlockCounts,
                  ArrayAttr depResultDimMap) {
    unsigned taskDepIndex = static_cast<unsigned>(deps.size());
    deps.push_back(depPtr);
    if (recordPartialReductionMaps)
      partialReductionDepResultDimMaps.push_back(
          depResultDimMap ? depResultDimMap
                          : Builder(context).getArrayAttr({}));
    depBlockOffsets.push_back(
        SmallVector<Value, 4>(blockOffsets.begin(), blockOffsets.end()));
    depRequiresDbRef.push_back(requiresDbRef);
    depOwnerDimCounts.push_back(ownerDimCount);
    depGroupBlockCounts.push_back(SmallVector<int64_t, 4>(
        groupBlockCounts.begin(), groupBlockCounts.end()));
    return taskDepIndex;
  }

  ArrayRef<Value> values() const { return deps; }
  ArrayRef<Attribute> partialReductionMaps() const {
    return partialReductionDepResultDimMaps;
  }
  ArrayRef<SmallVector<Value, 4>> blockOffsets() const {
    return depBlockOffsets;
  }
  ArrayRef<bool> requiresDbRefs() const { return depRequiresDbRef; }
  ArrayRef<unsigned> ownerDimCounts() const { return depOwnerDimCounts; }
  ArrayRef<SmallVector<int64_t, 4>> groupBlockCounts() const {
    return depGroupBlockCounts;
  }

private:
  MLIRContext *context = nullptr;
  bool recordPartialReductionMaps = false;
  SmallVector<Value, 4> deps;
  SmallVector<Attribute, 4> partialReductionDepResultDimMaps;
  SmallVector<SmallVector<Value, 4>> depBlockOffsets;
  SmallVector<bool> depRequiresDbRef;
  SmallVector<unsigned, 4> depOwnerDimCounts;
  SmallVector<SmallVector<int64_t, 4>> depGroupBlockCounts;
};

struct OwnerLocalStepRewrite {
  Operation *sourceOp = nullptr;
  unsigned operandIndex = 0;
  unsigned ownerSlot = 0;
};

static bool propagatesOwnerIndexDependency(Operation *op) {
  return isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivUIOp,
             arith::RemUIOp, arith::MinUIOp, arith::MaxUIOp, arith::MinSIOp,
             arith::MaxSIOp, arith::IndexCastOp>(op);
}

static std::optional<unsigned>
getUniqueOwnerSlot(ArrayRef<std::optional<unsigned>> operandSlots) {
  std::optional<unsigned> ownerSlot;
  for (std::optional<unsigned> slot : operandSlots) {
    if (!slot)
      continue;
    if (ownerSlot && *ownerSlot != *slot)
      return std::nullopt;
    ownerSlot = *slot;
  }
  return ownerSlot;
}

static SmallVector<OwnerLocalStepRewrite, 4>
collectOwnerLocalStepRewrites(sde::SdeSuIterateOp source, Block *computeBlock,
                              ArrayRef<unsigned> ownerLoopDims) {
  SmallVector<OwnerLocalStepRewrite, 4> rewrites;
  if (!computeBlock)
    return rewrites;

  DenseMap<Value, unsigned> ownerSlotByValue;
  DenseMap<Operation *, SmallVector<OwnerLocalStepRewrite, 2>> candidatesByOp;
  for (auto [slot, loopDim] : llvm::enumerate(ownerLoopDims)) {
    if (loopDim < source.getBody().front().getNumArguments())
      ownerSlotByValue.try_emplace(
          source.getBody().front().getArgument(loopDim),
          static_cast<unsigned>(slot));
  }

  computeBlock->walk([&](Operation *op) {
    SmallVector<std::optional<unsigned>, 4> operandSlots;
    operandSlots.reserve(op->getNumOperands());
    for (Value operand : op->getOperands()) {
      auto it = ownerSlotByValue.find(operand);
      operandSlots.push_back(it == ownerSlotByValue.end()
                                 ? std::optional<unsigned>{}
                                 : std::optional<unsigned>{it->second});
    }

    std::optional<unsigned> ownerSlot = getUniqueOwnerSlot(operandSlots);
    if (ownerSlot && isa<arith::AddIOp, arith::SubIOp>(op)) {
      unsigned loopDim = ownerLoopDims[*ownerSlot];
      if (loopDim < source.getSteps().size()) {
        Value sourceStep = source.getSteps()[loopDim];
        for (auto [operandIndex, operand] :
             llvm::enumerate(op->getOperands())) {
          if (operand == sourceStep) {
            candidatesByOp[op].push_back(
                {op, static_cast<unsigned>(operandIndex), *ownerSlot});
          }
        }
      }
    }

    if (!ownerSlot || !propagatesOwnerIndexDependency(op))
      return;
    for (Value result : op->getResults())
      if (result.getType().isIndex())
        ownerSlotByValue.try_emplace(result, *ownerSlot);
  });

  auto appendUpperBoundRewrites = [&](Value value, unsigned ownerSlot,
                                      DenseSet<Operation *> &visited) {
    std::function<void(Value)> visit = [&](Value current) {
      Operation *def = current.getDefiningOp();
      if (!def || !visited.insert(def).second)
        return;
      auto candidateIt = candidatesByOp.find(def);
      if (candidateIt != candidatesByOp.end())
        for (const OwnerLocalStepRewrite &rewrite : candidateIt->second)
          if (rewrite.ownerSlot == ownerSlot)
            rewrites.push_back(rewrite);
      for (Value operand : def->getOperands())
        visit(operand);
    };
    visit(value);
  };

  computeBlock->walk([&](scf::ForOp loop) {
    auto lowerIt = ownerSlotByValue.find(loop.getLowerBound());
    if (lowerIt == ownerSlotByValue.end())
      return;
    DenseSet<Operation *> visited;
    appendUpperBoundRewrites(loop.getUpperBound(), lowerIt->second, visited);
  });

  return rewrites;
}

static LogicalResult applyOwnerLocalStepRewrites(
    sde::SdeSuIterateOp source, ArrayRef<OwnerLocalStepRewrite> rewrites,
    IRMapping &mapper, ArrayRef<Value> ownerLocalSteps) {
  for (const OwnerLocalStepRewrite &rewrite : rewrites) {
    if (!rewrite.sourceOp || rewrite.sourceOp->getNumResults() == 0)
      return source.emitOpError()
             << "lost owner-local step rewrite source while cloning ARTS task";
    if (rewrite.ownerSlot >= ownerLocalSteps.size())
      return source.emitOpError()
             << "owner-local step rewrite maps outside the owner rank";
    Value clonedResult = mapper.lookupOrNull(rewrite.sourceOp->getResult(0));
    if (!clonedResult || !clonedResult.getDefiningOp())
      return source.emitOpError()
             << "lost cloned owner-local step expression while lowering ARTS "
                "task";
    Operation *clonedOp = clonedResult.getDefiningOp();
    if (rewrite.operandIndex >= clonedOp->getNumOperands())
      return source.emitOpError()
             << "cloned owner-local step expression has incompatible operands";
    clonedOp->setOperand(rewrite.operandIndex,
                         ownerLocalSteps[rewrite.ownerSlot]);
  }
  return success();
}

FailureOr<CompactHaloColumnSpec>
realizeCompactHaloColumnPacks(sde::SdeSuIterateOp source, DirectDepSpec dep,
                              ArrayRef<int64_t> groupBlockCounts,
                              OpBuilder &builder, Location loc) {
  if (dep.ownerDimCount != 2) {
    return source.emitOpError()
           << "commits a halo dependency whose owner rank is not supported by "
              "ARTS compact 2D unit-halo face realization; refusing a "
              "full-block halo byte-window";
  }
  if (failed(validatePositiveGroupBlockCounts(source, groupBlockCounts,
                                              dep.ownerDimCount)))
    return failure();

  FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
      dep.haloShape, dep.ownerDimCount, source.getOperation());
  if (failed(haloRadii))
    return failure();
  if (haloRadii->size() != 2 || (*haloRadii)[0] != 1 || (*haloRadii)[1] != 1) {
    return source.emitOpError()
           << "commits a halo dependency outside the supported 2D unit-halo "
              "shape; ARTS must realize the exact halo graph instead of "
              "widening to a full-block byte window";
  }

  FailureOr<unsigned> payloadRank =
      getCommittedPayloadRank(source, dep, "compact 2D unit-halo face");
  if (failed(payloadRank))
    return failure();
  if (*payloadRank != 2)
    return source.emitOpError()
           << "commits a payload rank that ARTS compact 2D unit-halo face "
              "realization cannot represent";

  FailureOr<Value> rowExtentValue =
      getPayloadElementExtent(source, dep, 0, "halo row extent");
  FailureOr<Value> colExtentValue =
      getPayloadElementExtent(source, dep, 1, "halo column extent");
  if (failed(rowExtentValue) || failed(colExtentValue))
    return failure();

  SmallVector<int64_t, 4> physicalBlockShape;
  physicalBlockShape.reserve(dep.alloc.getElementSizes().size());
  for (Value size : dep.alloc.getElementSizes()) {
    FailureOr<int64_t> constant = requireStaticPositiveIndex(
        size, source.getOperation(), "compact halo block extent");
    if (failed(constant))
      return failure();
    physicalBlockShape.push_back(*constant);
  }

  MLIRContext *ctx = source.getContext();
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  Value rowExtent = *rowExtentValue;
  Value colExtent = *colExtentValue;
  bool cleanPayloadShape = hasCleanPayloadElementShape(dep);
  SmallVector<Value, 4> compactElementSizes;
  if (!cleanPayloadShape) {
    compactElementSizes.push_back(one);
    compactElementSizes.push_back(one);
  }
  compactElementSizes.push_back(rowExtent);
  compactElementSizes.push_back(one);
  SmallVector<int64_t, 4> compactPhysicalBlockShape(physicalBlockShape);
  compactPhysicalBlockShape[getDbPayloadElementBase(dep) + 1] = 1;

  auto createCompactDb = [&]() -> FailureOr<arts::DbAllocOp> {
    auto db = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        dep.alloc.getElementType(),
        SmallVector<Value>(dep.alloc.getSizes().begin(),
                           dep.alloc.getSizes().end()),
        SmallVector<Value>(compactElementSizes.begin(),
                           compactElementSizes.end()),
        PartitionMode::block);
    db.setCompactHaloPayloadAttr(UnitAttr::get(ctx));
    copyDbAllocDistributionFactAttrs(dep.alloc, db);
    if (hasDistributedDbAllocation(dep.alloc.getOperation()) &&
        !destDbOwnerRouteMatchesSourcePolicy(dep.alloc, db))
      return db.emitOpError()
             << "cannot derive compact halo owner routes from destination DB "
                "grid and source distribution kind";
    return db;
  };

  CompactHaloColumnSpec spec;
  FailureOr<arts::DbAllocOp> leftColumnDb = createCompactDb();
  if (failed(leftColumnDb))
    return failure();
  FailureOr<arts::DbAllocOp> rightColumnDb = createCompactDb();
  if (failed(rightColumnDb))
    return failure();
  spec.leftColumnDb = *leftColumnDb;
  spec.rightColumnDb = *rightColumnDb;
  spec.rowExtent = rowExtent;
  spec.colExtent = colExtent;
  spec.cleanPayloadShape = cleanPayloadShape;

  Value rowGroupCount = createConstantIndex(builder, loc, groupBlockCounts[0]);
  Value colGroupCount = createConstantIndex(builder, loc, groupBlockCounts[1]);
  auto outer = scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[0],
                                  rowGroupCount);
  builder.setInsertionPointToStart(outer.getBody());
  auto inner = scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[1],
                                  colGroupCount);
  builder.setInsertionPointToStart(inner.getBody());

  Value blockI = outer.getInductionVar();
  Value blockJ = inner.getInductionVar();
  Value rowBlockSpan = arith::MinUIOp::create(
      builder, loc,
      arith::SubIOp::create(builder, loc, dep.alloc.getSizes()[0], blockI),
      rowGroupCount);
  Value colBlockSpan = arith::MinUIOp::create(
      builder, loc,
      arith::SubIOp::create(builder, loc, dep.alloc.getSizes()[1], blockJ),
      colGroupCount);
  SmallVector<Value> blockOffsets{blockI, blockJ};
  SmallVector<Value> blockSizes{rowBlockSpan, colBlockSpan};

  auto sourceAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  sourceAcquire.setPreserveAccessMode();
  auto leftAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::out, spec.leftColumnDb.getGuid(),
      spec.leftColumnDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  leftAcquire.setPreserveAccessMode();
  auto rightAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::out, spec.rightColumnDb.getGuid(),
      spec.rightColumnDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  rightAcquire.setPreserveAccessMode();

  SmallVector<Value, 4> packDeps{sourceAcquire.getPtr(), leftAcquire.getPtr(),
                                 rightAcquire.getPtr()};
  SmallVector<Value, 4> packParams{rowBlockSpan, colBlockSpan, rowExtent,
                                   colExtent};
  auto packEdt = arts::EdtOp::create(
      builder, loc, arts::EdtType::task, arts::EdtConcurrency::intranode,
      arts::createCurrentNodeRoute(builder, loc), packDeps, packParams);
  packEdt.setCompactHaloPackAttr(UnitAttr::get(ctx));

  Block &packBlock = packEdt.getBody().front();
  for (Value depValue : packDeps)
    packBlock.addArgument(depValue.getType(), loc);
  unsigned paramOffset = packBlock.getNumArguments();
  for (Value param : packParams)
    packBlock.addArgument(param.getType(), loc);

  OpBuilder bodyBuilder(packEdt.getContext());
  bodyBuilder.setInsertionPointToStart(&packBlock);
  Value sourceBlocks = packBlock.getArgument(0);
  Value leftBlocks = packBlock.getArgument(1);
  Value rightBlocks = packBlock.getArgument(2);
  Value rowBlockLimit = packBlock.getArgument(paramOffset);
  Value colBlockLimit = packBlock.getArgument(paramOffset + 1);
  Value rowLimit = packBlock.getArgument(paramOffset + 2);
  Value colLimit = packBlock.getArgument(paramOffset + 3);
  Value lastCol = arith::SubIOp::create(bodyBuilder, loc, colLimit,
                                        createOneIndex(bodyBuilder, loc));
  auto localBlockRowLoop =
      scf::ForOp::create(bodyBuilder, loc, createZeroIndex(bodyBuilder, loc),
                         rowBlockLimit, createOneIndex(bodyBuilder, loc));
  bodyBuilder.setInsertionPointToStart(localBlockRowLoop.getBody());
  auto localBlockColLoop =
      scf::ForOp::create(bodyBuilder, loc, createZeroIndex(bodyBuilder, loc),
                         colBlockLimit, createOneIndex(bodyBuilder, loc));
  bodyBuilder.setInsertionPointToStart(localBlockColLoop.getBody());
  SmallVector<Value, 4> localBlockIndices{localBlockRowLoop.getInductionVar(),
                                          localBlockColLoop.getInductionVar()};
  Value sourceBlockPayload =
      arts::DbRefOp::create(bodyBuilder, loc, sourceBlocks, localBlockIndices);
  Value leftBlockPayload =
      arts::DbRefOp::create(bodyBuilder, loc, leftBlocks, localBlockIndices);
  Value rightBlockPayload =
      arts::DbRefOp::create(bodyBuilder, loc, rightBlocks, localBlockIndices);
  auto rowLoop =
      scf::ForOp::create(bodyBuilder, loc, createZeroIndex(bodyBuilder, loc),
                         rowLimit, createOneIndex(bodyBuilder, loc));
  bodyBuilder.setInsertionPointToStart(rowLoop.getBody());
  Value row = rowLoop.getInductionVar();
  Value bodyZero = createZeroIndex(bodyBuilder, loc);
  FailureOr<SmallVector<Value, 4>> sourceLeftIdx = buildPayloadElementIndices(
      bodyBuilder, loc, sourceBlockPayload, dep.ownerDimCount,
      ArrayRef<Value>{row, bodyZero});
  FailureOr<SmallVector<Value, 4>> compactLeftIdx = buildPayloadElementIndices(
      bodyBuilder, loc, leftBlockPayload, dep.ownerDimCount,
      ArrayRef<Value>{row, bodyZero});
  if (failed(sourceLeftIdx) || failed(compactLeftIdx))
    return failure();
  Value leftValue = memref::LoadOp::create(bodyBuilder, loc, sourceBlockPayload,
                                           *sourceLeftIdx);
  memref::StoreOp::create(bodyBuilder, loc, leftValue, leftBlockPayload,
                          *compactLeftIdx);
  FailureOr<SmallVector<Value, 4>> sourceRightIdx = buildPayloadElementIndices(
      bodyBuilder, loc, sourceBlockPayload, dep.ownerDimCount,
      ArrayRef<Value>{row, lastCol});
  FailureOr<SmallVector<Value, 4>> compactRightIdx = buildPayloadElementIndices(
      bodyBuilder, loc, rightBlockPayload, dep.ownerDimCount,
      ArrayRef<Value>{row, bodyZero});
  if (failed(sourceRightIdx) || failed(compactRightIdx))
    return failure();
  Value rightValue = memref::LoadOp::create(
      bodyBuilder, loc, sourceBlockPayload, *sourceRightIdx);
  memref::StoreOp::create(bodyBuilder, loc, rightValue, rightBlockPayload,
                          *compactRightIdx);
  bodyBuilder.setInsertionPointToEnd(&packBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(outer);
  return spec;
}

arts::DbAcquireOp create2DUnitRowHaloAcquire(
    sde::SdeSuIterateOp source, DirectDepSpec dep,
    ArrayRef<Value> centerOffsets, ArrayRef<Value> centerSizes,
    CompactHaloColumnSpec spec, bool topFace, SmallVectorImpl<Value> &dbOffsets,
    OpBuilder &builder, Location loc) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockI = centerOffsets[0];
  Value blockJ = centerOffsets[1];
  Value rowSpan = centerSizes[0];
  Value colSpan = centerSizes[1];
  Value sourceI;
  Value boundsValid;
  Value elementRowOffset;
  if (topFace) {
    Value canShift = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, blockI, one);
    Value shifted = arith::SubIOp::create(builder, loc, blockI, one);
    sourceI = arith::SelectOp::create(builder, loc, canShift, shifted, zero);
    boundsValid = canShift;
    elementRowOffset = arith::SubIOp::create(builder, loc, spec.rowExtent, one);
  } else {
    sourceI = arith::AddIOp::create(builder, loc, blockI, rowSpan);
    boundsValid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                        sourceI, dep.alloc.getSizes()[0]);
    sourceI =
        arith::SelectOp::create(builder, loc, boundsValid, sourceI, blockI);
    elementRowOffset = zero;
  }

  SmallVector<Value> offsets{sourceI, blockJ};
  SmallVector<Value> sizes{one, colSpan};
  dbOffsets.assign(offsets.begin(), offsets.end());
  SmallVector<Value, 4> payloadElementOffsets{elementRowOffset, zero};
  SmallVector<Value, 4> payloadElementSizes{one, spec.colExtent};
  SmallVector<Value> elementOffsets;
  SmallVector<Value> elementSizes;
  appendElementWindowForPayloadShape(
      builder, loc, spec.cleanPayloadShape, dep.ownerDimCount,
      payloadElementOffsets, payloadElementSizes, elementOffsets, elementSizes);
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid, elementOffsets,
      elementSizes);
  acquire.setPreserveAccessMode();
  if (topFace)
    attachStencilHaloAcquireFacts(source, acquire, {-1, 0}, {0, 0});
  else
    attachStencilHaloAcquireFacts(source, acquire, {0, 0}, {1, 0});
  return acquire;
}

arts::DbAcquireOp create2DUnitCompactColumnAcquire(
    DirectDepSpec dep, ArrayRef<Value> centerOffsets,
    ArrayRef<Value> centerSizes, CompactHaloColumnSpec spec, bool leftFace,
    SmallVectorImpl<Value> &dbOffsets, OpBuilder &builder, Location loc) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockI = centerOffsets[0];
  Value blockJ = centerOffsets[1];
  Value rowSpan = centerSizes[0];
  Value colSpan = centerSizes[1];
  Value sourceJ;
  Value boundsValid;
  arts::DbAllocOp compactDb;
  if (leftFace) {
    Value canShift = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, blockJ, one);
    Value shifted = arith::SubIOp::create(builder, loc, blockJ, one);
    sourceJ = arith::SelectOp::create(builder, loc, canShift, shifted, zero);
    boundsValid = canShift;
    compactDb = spec.rightColumnDb;
  } else {
    sourceJ = arith::AddIOp::create(builder, loc, blockJ, colSpan);
    boundsValid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                        sourceJ, dep.alloc.getSizes()[1]);
    sourceJ =
        arith::SelectOp::create(builder, loc, boundsValid, sourceJ, blockJ);
    compactDb = spec.leftColumnDb;
  }

  SmallVector<Value> offsets{blockI, sourceJ};
  SmallVector<Value> sizes{rowSpan, one};
  dbOffsets.assign(offsets.begin(), offsets.end());
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, compactDb.getGuid(), compactDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid,
      SmallVector<Value>{}, SmallVector<Value>{});
  acquire.setPreserveAccessMode();
  return acquire;
}

FailureOr<CompactHaloNdSpec>
realizeCompactHaloNdPacks(sde::SdeSuIterateOp source, DirectDepSpec dep,
                          ArrayRef<int64_t> groupBlockCounts,
                          Block *computeBlock, ArrayRef<unsigned> ownerLoopDims,
                          OpBuilder &builder, Location loc) {
  unsigned ownerDimCount = dep.ownerDimCount;
  if (ownerDimCount == 0) {
    return source.emitOpError()
           << "requires positive owner rank for compact N-D halo "
              "realization, got "
           << ownerDimCount;
  }
  if (ownerDimCount > 3) {
    return source.emitOpError()
           << "commits a halo dependency whose owner rank exceeds the "
              "implemented ARTS compact N-D unit-halo realization; refusing a "
              "full-block halo byte-window";
  }
  if (failed(validatePositiveGroupBlockCounts(source, groupBlockCounts,
                                              ownerDimCount)))
    return failure();

  FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
      dep.haloShape, dep.ownerDimCount, source.getOperation());
  if (failed(haloRadii))
    return failure();
  if (llvm::any_of(*haloRadii, [](int64_t radius) { return radius != 1; })) {
    return source.emitOpError()
           << "commits a halo dependency outside the supported unit-halo "
              "shape; ARTS must realize the exact halo graph instead of "
              "widening to a full-block byte window";
  }

  FailureOr<unsigned> payloadRankOr =
      getCommittedPayloadRank(source, dep, "compact N-D unit-halo");
  if (failed(payloadRankOr))
    return failure();
  unsigned payloadRank = *payloadRankOr;
  if (ownerDimCount > payloadRank) {
    return source.emitOpError()
           << "commits more owner dimensions than payload dimensions for "
              "ARTS compact N-D unit-halo realization";
  }
  SmallVector<unsigned, 4> ownerPayloadDims;
  ownerPayloadDims.reserve(ownerDimCount);
  if (dep.arrayOwnerDims && dep.arrayOwnerDims->size() == ownerDimCount) {
    llvm::SmallDenseSet<unsigned, 4> usedPayloadDims;
    for (int64_t rawDim : *dep.arrayOwnerDims) {
      if (rawDim < 0 || static_cast<unsigned>(rawDim) >= payloadRank)
        return source.emitOpError()
               << "commits an owner dimension outside the payload rank for "
                  "ARTS compact N-D unit-halo realization";
      unsigned payloadDim = static_cast<unsigned>(rawDim);
      if (!usedPayloadDims.insert(payloadDim).second)
        return source.emitOpError()
               << "commits duplicate owner payload dimensions for ARTS compact "
                  "N-D unit-halo realization";
      ownerPayloadDims.push_back(payloadDim);
    }
  } else {
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      ownerPayloadDims.push_back(slot);
  }

  SmallVector<Value, 4> elementExtents;
  elementExtents.reserve(payloadRank);
  for (unsigned slot = 0; slot < payloadRank; ++slot) {
    FailureOr<Value> extent =
        getPayloadElementExtent(source, dep, slot, "halo element extent");
    if (failed(extent))
      return failure();
    elementExtents.push_back(*extent);
  }

  SmallVector<int64_t, 4> physicalBlockShape;
  physicalBlockShape.reserve(dep.alloc.getElementSizes().size());
  for (Value size : dep.alloc.getElementSizes()) {
    FailureOr<int64_t> constant = requireStaticPositiveIndex(
        size, source.getOperation(), "compact halo block extent");
    if (failed(constant))
      return failure();
    physicalBlockShape.push_back(*constant);
  }

  MLIRContext *ctx = source.getContext();
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value route = arts::createCurrentNodeRoute(builder, loc);

  CompactHaloNdSpec spec;
  spec.ownerDimCount = ownerDimCount;
  spec.ownerPayloadDims.assign(ownerPayloadDims.begin(),
                               ownerPayloadDims.end());
  spec.elementExtents.assign(elementExtents.begin(), elementExtents.end());
  bool cleanPayloadShape = hasCleanPayloadElementShape(dep);

  FailureOr<SmallVector<SmallVector<int64_t, 4>, 8>> sideOffsets =
      collectRequiredNdUnitHaloSourceOffsets(source, dep, computeBlock,
                                             ownerLoopDims, ownerPayloadDims,
                                             elementExtents);
  if (failed(sideOffsets))
    return failure();
  spec.sides.reserve(sideOffsets->size());

  for (ArrayRef<int64_t> sideOffset : *sideOffsets) {
    SmallVector<Value> compactElementSizes;
    compactElementSizes.reserve(dep.alloc.getElementSizes().size());
    if (!cleanPayloadShape)
      for (unsigned slot = 0; slot < ownerDimCount; ++slot)
        compactElementSizes.push_back(one);
    for (unsigned slot = 0; slot < payloadRank; ++slot) {
      auto ownerIt = llvm::find(ownerPayloadDims, slot);
      bool sideOwnsPayload = ownerIt != ownerPayloadDims.end() &&
                             sideOffset[static_cast<unsigned>(
                                 ownerIt - ownerPayloadDims.begin())] != 0;
      compactElementSizes.push_back(sideOwnsPayload ? one
                                                    : elementExtents[slot]);
    }

    SmallVector<int64_t, 4> compactPhysicalBlockShape(physicalBlockShape);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      unsigned payloadDim = ownerPayloadDims[slot];
      if (sideOffset[slot] != 0)
        compactPhysicalBlockShape[getDbPayloadElementBase(dep) + payloadDim] =
            1;
    }

    auto payloadDb = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        dep.alloc.getElementType(),
        SmallVector<Value>(dep.alloc.getSizes().begin(),
                           dep.alloc.getSizes().end()),
        SmallVector<Value>(compactElementSizes.begin(),
                           compactElementSizes.end()),
        PartitionMode::block);
    payloadDb.setCompactHaloPayloadAttr(UnitAttr::get(ctx));
    copyDbAllocDistributionFactAttrs(dep.alloc, payloadDb);
    if (hasDistributedDbAllocation(dep.alloc.getOperation()) &&
        !destDbOwnerRouteMatchesSourcePolicy(dep.alloc, payloadDb))
      return payloadDb.emitOpError()
             << "cannot derive compact halo owner routes from destination DB "
                "grid and source distribution kind";
    spec.sides.push_back(
        {SmallVector<int64_t, 4>(sideOffset.begin(), sideOffset.end()),
         payloadDb});
  }

  if (spec.sides.empty())
    return spec;

  SmallVector<Value> blockOffsets;
  SmallVector<Value> blockSizes;
  blockOffsets.reserve(ownerDimCount);
  blockSizes.reserve(ownerDimCount);
  scf::ForOp outerLoop;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    Value groupCount =
        createConstantIndex(builder, loc, groupBlockCounts[slot]);
    auto loop = scf::ForOp::create(builder, loc, zero,
                                   dep.alloc.getSizes()[slot], groupCount);
    if (!outerLoop)
      outerLoop = loop;
    builder.setInsertionPointToStart(loop.getBody());
    Value blockOffset = loop.getInductionVar();
    blockOffsets.push_back(blockOffset);
    blockSizes.push_back(arith::MinUIOp::create(
        builder, loc,
        arith::SubIOp::create(builder, loc, dep.alloc.getSizes()[slot],
                              blockOffset),
        groupCount));
  }

  auto sourceAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{},
      SmallVector<Value>(blockOffsets.begin(), blockOffsets.end()), blockSizes,
      SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{}, Value{},
      SmallVector<Value>{}, SmallVector<Value>{});
  sourceAcquire.setPreserveAccessMode();

  SmallVector<Value, 8> packDeps{sourceAcquire.getPtr()};
  packDeps.reserve(1 + spec.sides.size());
  for (CompactHaloNdSideSpec &side : spec.sides) {
    auto payloadAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::out, side.payloadDb.getGuid(),
        side.payloadDb.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{},
        SmallVector<Value>(blockOffsets.begin(), blockOffsets.end()),
        blockSizes, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    payloadAcquire.setPreserveAccessMode();
    packDeps.push_back(payloadAcquire.getPtr());
  }

  SmallVector<Value, 4> packParams(blockSizes.begin(), blockSizes.end());
  packParams.append(elementExtents.begin(), elementExtents.end());
  auto packEdt = arts::EdtOp::create(
      builder, loc, arts::EdtType::task, arts::EdtConcurrency::intranode,
      arts::createCurrentNodeRoute(builder, loc), packDeps, packParams);
  packEdt.setCompactHaloPackAttr(UnitAttr::get(ctx));

  Block &packBlock = packEdt.getBody().front();
  for (Value depValue : packDeps)
    packBlock.addArgument(depValue.getType(), loc);
  unsigned paramOffset = packBlock.getNumArguments();
  for (Value param : packParams)
    packBlock.addArgument(param.getType(), loc);

  OpBuilder bodyBuilder(packEdt.getContext());
  bodyBuilder.setInsertionPointToStart(&packBlock);
  Value sourceBlocks = packBlock.getArgument(0);
  SmallVector<Value, 8> compactBlocks;
  compactBlocks.reserve(spec.sides.size());
  for (unsigned sideIdx = 0; sideIdx < spec.sides.size(); ++sideIdx)
    compactBlocks.push_back(packBlock.getArgument(1 + sideIdx));
  SmallVector<Value, 4> bodyBlockSizes;
  bodyBlockSizes.reserve(ownerDimCount);
  for (unsigned slot = 0; slot < ownerDimCount; ++slot)
    bodyBlockSizes.push_back(packBlock.getArgument(paramOffset + slot));
  SmallVector<Value, 4> bodyElementExtents;
  bodyElementExtents.reserve(elementExtents.size());
  for (unsigned slot = 0; slot < elementExtents.size(); ++slot)
    bodyElementExtents.push_back(
        packBlock.getArgument(paramOffset + ownerDimCount + slot));
  SmallVector<Value, 4> localBlockIndices(ownerDimCount);
  std::function<LogicalResult(unsigned)> emitBlockLoop = [&](unsigned dim) {
    if (dim == ownerDimCount) {
      Value sourceBlockPayload = arts::DbRefOp::create(
          bodyBuilder, loc, sourceBlocks, localBlockIndices);
      for (auto [side, compactBlocksArg] :
           llvm::zip_equal(spec.sides, compactBlocks)) {
        Value compactBlockPayload = arts::DbRefOp::create(
            bodyBuilder, loc, compactBlocksArg, localBlockIndices);
        if (failed(emitCompactHaloCopy(bodyBuilder, loc, ownerDimCount,
                                       ownerPayloadDims, side.sourceOffsets,
                                       bodyElementExtents, sourceBlockPayload,
                                       compactBlockPayload)))
          return failure();
      }
      return success();
    }
    auto loop = scf::ForOp::create(
        bodyBuilder, loc, createZeroIndex(bodyBuilder, loc),
        bodyBlockSizes[dim], createOneIndex(bodyBuilder, loc));
    bodyBuilder.setInsertionPointToStart(loop.getBody());
    localBlockIndices[dim] = loop.getInductionVar();
    if (failed(emitBlockLoop(dim + 1)))
      return failure();
    return success();
  };
  if (failed(emitBlockLoop(/*dim=*/0)))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&packBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(outerLoop);
  return spec;
}

static FailureOr<SmallVector<int64_t, 4>>
deriveDepGroupBlockCounts(sde::SdeSuIterateOp source, const DirectDepSpec &dep,
                          ArrayRef<unsigned> dispatchLoopDims,
                          ArrayRef<int64_t> dispatchWorkerSpans) {
  SmallVector<int64_t, 4> counts(dep.ownerDimCount, 1);
  if (dep.ownerDimCount == 0 || dep.reduceScatter)
    return counts;
  if (dep.accessSlots.size() != dep.ownerDimCount)
    return source.emitOpError()
           << "dependency access-window coordinates do not cover owner rank";

  for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
    const DepOwnerAccessSlot &access = dep.accessSlots[slot];
    if (access.fullWindow) {
      counts[slot] =
          std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
      continue;
    }
    if (access.fixedBlock) {
      counts[slot] = 1;
      continue;
    }
    if (!access.loopDim || *access.loopDim >= source.getUpperBounds().size())
      return source.emitOpError()
             << "dependency access-window coordinate has no loop dimension";

    int64_t coordinateBlockSize = access.coordinateBlockSize;
    if (coordinateBlockSize <= 0)
      return source.emitOpError() << "dependency access-window coordinate has "
                                     "non-positive block size";

    auto dispatchIt = llvm::find(dispatchLoopDims, *access.loopDim);
    if (dispatchIt == dispatchLoopDims.end()) {
      counts[slot] =
          std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
      continue;
    }

    unsigned dispatchSlot = static_cast<unsigned>(
        std::distance(dispatchLoopDims.begin(), dispatchIt));
    if (dispatchSlot >= dispatchWorkerSpans.size())
      return source.emitOpError() << "dependency access-window coordinate has "
                                     "no dispatch worker span";
    int64_t workerSpan = dispatchWorkerSpans[dispatchSlot];
    bool exactBlockAlignedDispatch =
        hasBlockAlignedDispatchBase(source.getLowerBounds()[*access.loopDim],
                                    workerSpan, coordinateBlockSize);
    counts[slot] = getStaticAccessGroupCount(
        workerSpan, coordinateBlockSize, access.minElementOffset,
        access.maxElementOffset, exactBlockAlignedDispatch);
  }
  arts::DbAllocOp alloc = dep.alloc;
  if (!hasArtsDbPhysicalLayout(alloc.getOperation()) ||
      !llvm::any_of(counts, [](int64_t count) { return count > 1; }))
    return counts;

  std::optional<int64_t> totalNodes =
      arts::getRuntimeTotalNodes(source->getParentOfType<ModuleOp>());
  if (!totalNodes)
    return source.emitOpError()
           << "requires runtime node count to keep compact halo payload "
              "writers owner-local";
  int64_t validationNodes = std::max<int64_t>(*totalNodes, 2);
  if (validationNodes <= 1)
    return counts;

  std::optional<DbOwnerRouteFacts> ownerFacts =
      deriveDbOwnerRouteFactsFromDbGrid(alloc);
  if (!ownerFacts)
    return source.emitOpError()
           << "cannot prove compact halo payload writer grouping owner-local "
              "from the source DB block grid";
  SmallVector<Value, 4> dbSizeValues(alloc.getSizes().begin(),
                                     alloc.getSizes().end());
  std::optional<SmallVector<int64_t, 4>> dbSizes =
      foldStaticDbIndexValues(dbSizeValues);
  if (!dbSizes)
    return source.emitOpError()
           << "cannot prove compact halo payload writer grouping owner-local "
              "from static DB block-grid facts";
  if (dbSizes->size() != counts.size())
    return source.emitOpError()
           << "compact halo payload writer grouping rank does not match the "
              "DB block grid";

  WriterGroupingSpec writerSpec;
  writerSpec.dbSizes.assign(dbSizes->begin(), dbSizes->end());
  writerSpec.ownerFacts = *ownerFacts;
  SmallVector<WriterGroupingSpec, 1> writerSpecs;
  writerSpecs.push_back(std::move(writerSpec));
  std::optional<SmallVector<int64_t, 4>> ownerLocalCounts =
      findLargestOwnerLocalGroupCounts(writerSpecs, counts, validationNodes);
  if (!ownerLocalCounts)
    return source.emitOpError()
           << "cannot split compact halo payload writer grouping into proven "
              "owner-local block ranges from the DB block grid";
  counts.assign(ownerLocalCounts->begin(), ownerLocalCounts->end());
  return counts;
}

static FailureOr<SmallVector<unsigned, 4>>
getDependencyOwnerLoopDims(sde::SdeSuIterateOp source, const DirectDepSpec &dep,
                           StringRef diagnosticName) {
  SmallVector<unsigned, 4> loopDims;
  loopDims.reserve(dep.ownerDimCount);
  for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
    if (slot >= dep.accessSlots.size() || !dep.accessSlots[slot].loopDim)
      return source.emitOpError()
             << "commits a " << diagnosticName
             << " dependency without owner-loop mapping";
    loopDims.push_back(*dep.accessSlots[slot].loopDim);
  }
  return loopDims;
}

arts::DbAcquireOp createNdCompactHaloAcquire(DirectDepSpec dep,
                                             ArrayRef<Value> centerOffsets,
                                             ArrayRef<Value> centerSizes,
                                             CompactHaloNdSideSpec side,
                                             SmallVectorImpl<Value> &dbOffsets,
                                             SmallVectorImpl<Value> &dbSizes,
                                             OpBuilder &builder, Location loc) {
  Value one = createOneIndex(builder, loc);
  Value boundsValid = {};
  auto appendBounds = [&](Value condition) {
    boundsValid = boundsValid ? arith::AndIOp::create(builder, loc, boundsValid,
                                                      condition)
                              : condition;
  };

  SmallVector<Value> offsets;
  offsets.reserve(centerOffsets.size());
  for (auto [slot, sourceOffset] : llvm::enumerate(side.sourceOffsets)) {
    Value current = centerOffsets[slot];
    Value centerSpan = centerSizes[slot];
    if (sourceOffset < 0) {
      Value canShift = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::uge, current, one);
      Value shifted = arith::SubIOp::create(builder, loc, current, one);
      offsets.push_back(
          arith::SelectOp::create(builder, loc, canShift, shifted, current));
      dbSizes.push_back(one);
      appendBounds(canShift);
      continue;
    }
    if (sourceOffset > 0) {
      Value shifted = arith::AddIOp::create(builder, loc, current, centerSpan);
      Value canShift =
          arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                shifted, dep.alloc.getSizes()[slot]);
      offsets.push_back(
          arith::SelectOp::create(builder, loc, canShift, shifted, current));
      dbSizes.push_back(one);
      appendBounds(canShift);
      continue;
    }
    offsets.push_back(current);
    dbSizes.push_back(centerSpan);
  }

  if (!boundsValid)
    boundsValid = arith::ConstantIntOp::create(builder, loc, 1, 1);
  dbOffsets.assign(offsets.begin(), offsets.end());
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, side.payloadDb.getGuid(),
      side.payloadDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets,
      SmallVector<Value>(dbSizes.begin(), dbSizes.end()), SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid,
      SmallVector<Value>{}, SmallVector<Value>{});
  acquire.setPreserveAccessMode();
  return acquire;
}

template <typename RewriteT>
LogicalResult recordClonedHaloLoadRewrites(
    Operation *original, Operation *cloned,
    const DenseMap<Operation *, RewriteT> &originalRewrites,
    DenseMap<Operation *, RewriteT> &clonedRewrites) {
  auto recordIfMapped = [&](Operation *originalLoad, Operation *clonedLoad) {
    auto it = originalRewrites.find(originalLoad);
    if (it != originalRewrites.end())
      clonedRewrites[clonedLoad] = it->second;
  };

  SmallVector<memref::LoadOp, 8> originalLoads;
  SmallVector<memref::LoadOp, 8> clonedLoads;
  if (auto load = dyn_cast<memref::LoadOp>(original))
    originalLoads.push_back(load);
  else
    original->walk([&](memref::LoadOp load) { originalLoads.push_back(load); });
  if (auto load = dyn_cast<memref::LoadOp>(cloned))
    clonedLoads.push_back(load);
  else
    cloned->walk([&](memref::LoadOp load) { clonedLoads.push_back(load); });

  bool hasMappedLoad = llvm::any_of(originalLoads, [&](memref::LoadOp load) {
    return originalRewrites.contains(load.getOperation());
  });
  if (!hasMappedLoad)
    return success();
  if (originalLoads.size() != clonedLoads.size())
    return cloned->emitError()
           << "could not preserve compact halo load rewrite mapping while "
              "cloning SDE compute body";
  for (auto [originalLoad, clonedLoad] : llvm::zip(originalLoads, clonedLoads))
    recordIfMapped(originalLoad.getOperation(), clonedLoad.getOperation());
  return success();
}

LogicalResult rewriteOwnerIndicesToLocal(
    arts::EdtOp task, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs,
    ArrayRef<bool> depRequiresDbRef, ArrayRef<unsigned> depOwnerDimCounts,
    ArrayRef<SmallVector<int64_t, 4>> depGroupBlockCounts) {
  if (payloads.size() != depBlockOffsetArgs.size() ||
      payloads.size() != depRequiresDbRef.size() ||
      payloads.size() != depOwnerDimCounts.size() ||
      payloads.size() != depGroupBlockCounts.size())
    return task.emitOpError() << "has inconsistent owner grouping facts";
  for (auto [offsets, ownerDimCount, groupCounts] : llvm::zip_equal(
           depBlockOffsetArgs, depOwnerDimCounts, depGroupBlockCounts))
    if (offsets.size() != ownerDimCount || groupCounts.size() != ownerDimCount)
      return task.emitOpError() << "has inconsistent dependency block offsets";

  auto depHasGroupedBlocks = [&](unsigned depIdx) {
    return llvm::any_of(depGroupBlockCounts[depIdx],
                        [](int64_t count) { return count > 1; });
  };

  DenseMap<Value, unsigned> payloadToDepIndex;
  DenseMap<Value, Value> payloadSources;
  for (auto [idx, payload] : llvm::enumerate(payloads)) {
    auto [it, inserted] = payloadToDepIndex.try_emplace(payload, idx);
    if (!inserted)
      return task.emitOpError()
             << "has duplicate dependency payload during owner-index rewrite";
    if (!depHasGroupedBlocks(idx) && !depRequiresDbRef[idx])
      continue;
    auto ref = payload.getDefiningOp<arts::DbRefOp>();
    if (!ref)
      return task.emitOpError()
             << "cannot reindex owner blocks without a DB-ref payload";
    payloadSources[payload] = ref.getSource();
  }

  OpBuilder builder(task.getContext());
  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    auto depIt = payloadToDepIndex.find(root);
    if (depIt == payloadToDepIndex.end())
      return WalkResult::advance();
    unsigned depIdx = depIt->second;
    unsigned ownerDimCount = depOwnerDimCounts[depIdx];
    if (ownerDimCount == 0)
      return WalkResult::advance();
    if (indices.size() < ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }
    auto rootType = dyn_cast<MemRefType>(root.getType());
    if (!rootType) {
      op->emitError() << "DB payload access does not use a memref payload";
      return WalkResult::interrupt();
    }
    bool usesCollapsedPayload =
        rootType.getRank() + static_cast<int64_t>(ownerDimCount) ==
        static_cast<int64_t>(indices.size());
    bool usesRankExpandedPayload =
        rootType.getRank() == static_cast<int64_t>(indices.size());
    if (!usesCollapsedPayload && !usesRankExpandedPayload) {
      op->emitError()
          << "DB payload rank is inconsistent with committed owner dimensions";
      return WalkResult::interrupt();
    }
    builder.setInsertionPoint(op);
    if (depHasGroupedBlocks(depIdx) || depRequiresDbRef[depIdx]) {
      if (root != memref) {
        op->emitError()
            << "grouped DB access through a memref view is not realized; "
               "SDE-to-ARTS must rewrite the view or fail closed";
        return WalkResult::interrupt();
      }
      auto sourceIt = payloadSources.find(root);
      if (sourceIt == payloadSources.end()) {
        op->emitError() << "has no grouped dependency source";
        return WalkResult::interrupt();
      }
      ArrayRef<Value> blockOffsets = depBlockOffsetArgs[depIdx];
      SmallVector<Value, 4> localBlockIndices;
      localBlockIndices.reserve(ownerDimCount);
      for (unsigned idx = 0; idx < ownerDimCount; ++idx) {
        Value local = arith::SubIOp::create(
            builder, op->getLoc(), indices[idx].get(), blockOffsets[idx]);
        localBlockIndices.push_back(local);
      }
      Value groupedPayload = arts::DbRefOp::create(
          builder, op->getLoc(), sourceIt->second, localBlockIndices);
      if (auto load = dyn_cast<memref::LoadOp>(op))
        load->setOperand(0, groupedPayload);
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        store->setOperand(1, groupedPayload);
      else {
        op->emitError() << "unsupported grouped DB payload access";
        return WalkResult::interrupt();
      }
    }
    if (usesCollapsedPayload) {
      SmallVector<Value, 4> payloadIndices;
      payloadIndices.reserve(indices.size() - ownerDimCount);
      for (unsigned idx = ownerDimCount; idx < indices.size(); ++idx)
        payloadIndices.push_back(indices[idx].get());
      indices.assign(payloadIndices);
    } else {
      for (unsigned idx = 0; idx < ownerDimCount; ++idx)
        indices[idx].set(createZeroIndex(builder, op->getLoc()));
    }
    return WalkResult::advance();
  };

  WalkResult result = task.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      return rewriteAccess(op, load.getMemref(), load.getIndicesMutable());
    }
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      return rewriteAccess(op, store.getMemref(), store.getIndicesMutable());
    }
    return WalkResult::advance();
  });
  if (result.wasInterrupted())
    return failure();

  for (Value payload : payloads)
    if (Operation *op = payload.getDefiningOp())
      if (op->use_empty())
        op->erase();
  return result.wasInterrupted() ? failure() : success();
}

static FailureOr<ArtsOwnerSlotMapping> resolveWindowedDispatchOwnerSlotMapping(
    sde::SdeSuIterateOp source, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> blockShape, unsigned loopRank,
    ArrayRef<DirectDepSpec> deps);

static bool writerHasNonDispatchedFullWindowOwnerSlot(
    ArrayRef<DirectDepSpec> deps, ArrayRef<int64_t> dispatchedOwnerDims);

LogicalResult
convertSuIterate(sde::SdeSuIterateOp source,
                 DenseSet<Operation *> &consumedCuLevelAccessWindows,
                 SmallVectorImpl<Operation *> &consumedRedists) {
  if (source.getNumResults() != 0 || !source.getReductionAccumulators().empty())
    return source.emitOpError()
           << "direct SDE-to-ARTS lowering requires reduction/result facts to "
              "be authored as explicit SDE-to-ARTS reduction operations";

  SmallVector<DirectDepSpec, 4> deps;
  if (failed(collectSuDependencies(source, deps, consumedCuLevelAccessWindows,
                                   consumedRedists)))
    return failure();
  if (deps.empty()) {
    if (hasCommittedPartialReductionFacts(source))
      return source.emitOpError()
             << "commits partial-reduction facts without SDE access windows; "
                "SDE must expose block dependencies before ARTS lowering";
    return tryConvertCoarseSuIterate(source);
  }

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  std::optional<CommittedPhysicalLayout> physicalLayout =
      readCommittedPhysicalLayout(source, deps);
  const bool requiresCommittedPhysicalLayout =
      llvm::any_of(deps, [](const DirectDepSpec &dep) {
        return dep.ownerDimCount != 0;
      });
  if (!physicalLayout || physicalLayout->blockShape.empty() ||
      (physicalLayout->ownerDims.empty() && requiresCommittedPhysicalLayout)) {
    if (requiresCommittedPhysicalLayout)
      return source.emitOpError()
             << "has owner-ranked SDE access windows without a committed SDE "
                "physical layout fact; refusing to reconstruct layout at the "
                "ARTS boundary";
    return tryConvertCoarseSuIterate(source);
  }

  ArrayRef<int64_t> ownerDims = physicalLayout->ownerDims;
  ArrayRef<int64_t> blockShape = physicalLayout->blockShape;

  FailureOr<ArtsOwnerSlotMapping> ownerRouteping =
      resolveWindowedDispatchOwnerSlotMapping(source, ownerDims, blockShape,
                                              loopRank, deps);
  if (failed(ownerRouteping))
    return failure();

  ArrayRef<int64_t> ownerSlotDims = ownerRouteping->ownerDims;
  unsigned ownerDimCount = ownerSlotDims.size();
  for (DirectDepSpec &dep : deps)
    if (dep.accessSlots.size() != dep.ownerDimCount && !dep.reduceScatter)
      return source.emitOpError()
             << "dependency access-window coordinates do not match owner rank";

  SmallVector<int64_t, 4> ownerBlockSizes(ownerRouteping->blockSizes.begin(),
                                          ownerRouteping->blockSizes.end());

  SmallVector<int64_t, 4> workerSpans(ownerBlockSizes.begin(),
                                      ownerBlockSizes.end());
  SmallVector<int64_t, 4> groupBlockCounts(ownerDimCount, 1);
  if (sde::SdeCuRegionOp computeCu = sde::findSuComputeCuRegion(source)) {
    if (auto groupCounts =
            readI64ArrayAttr(computeCu.getGroupBlockCountAttr())) {
      // groupBlockCount may be committed per-owner-slot (size == ownerDimCount)
      // or per-array-dim (a full-rank elementwise writer commits one count per
      // logical dim, with count 1 on undistributed dims). Accept both: index
      // per-array-dim counts by the resolved owner dims.
      bool perOwner = groupCounts->size() == ownerDimCount;
      bool perArrayDim =
          !perOwner && llvm::all_of(ownerSlotDims, [&](int64_t d) {
            return d >= 0 && static_cast<size_t>(d) < groupCounts->size();
          });
      if (!perOwner && !perArrayDim)
        return source.emitOpError()
               << "commits groupBlockCount whose rank does not match the "
                  "committed owner rank";
      for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
        int64_t count = perOwner ? (*groupCounts)[slot]
                                 : (*groupCounts)[ownerSlotDims[slot]];
        int64_t blockSize = ownerBlockSizes[slot];
        if (count <= 0)
          return source.emitOpError()
                 << "commits groupBlockCount that cannot be represented as "
                    "a whole-number group of physical DB blocks";
        workerSpans[slot] = blockSize * count;
        groupBlockCounts[slot] = count;
      }
    }
  }

  bool splitToOwnerLocalGroups = false;
  if (hasDistributedWriterStorageFacts(deps)) {
    std::optional<int64_t> totalNodes =
        arts::getRuntimeTotalNodes(source->getParentOfType<ModuleOp>());
    if (!totalNodes)
      return source.emitOpError()
             << "requires runtime node count to keep grouped distributed "
                "writers owner-local";
    if (*totalNodes > 1 &&
        writerHasNonDispatchedFullWindowOwnerSlot(deps,
                                                  ownerRouteping->ownerDims))
      return source.emitOpError()
             << "commits a writable full-window owner slot that is not "
                "dispatched; distributed lowering requires SDE to expose that "
                "owner loop";
    if (failed(ensureDistributedWriterOwnerLocalGroups(
            source, deps, groupBlockCounts, workerSpans, ownerBlockSizes,
            ownerRouteping->ownerDims, ownerRouteping->loopDims,
            *totalNodes, splitToOwnerLocalGroups)))
      return failure();
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  SmallVector<CompactHaloColumnSpec, 4> compactHaloColumnSpecs;
  DenseMap<unsigned, unsigned> compactColumnSpecByDepIndex;
  SmallVector<CompactHaloNdSpec, 4> compactHaloNdSpecs;
  DenseMap<unsigned, unsigned> compactNdSpecByDepIndex;
  for (auto [depIndex, dep] : llvm::enumerate(deps)) {
    if (!dep.haloShape)
      continue;
    FailureOr<SmallVector<int64_t, 4>> depGroupBlockCounts =
        deriveDepGroupBlockCounts(source, dep, ownerRouteping->loopDims,
                                  workerSpans);
    if (failed(depGroupBlockCounts))
      return failure();
    if (dep.mode != ArtsMode::in)
      return source.emitOpError()
             << "commits a halo dependency that is not read-only; ARTS cannot "
                "realize a writable halo window";
    FailureOr<SmallVector<unsigned, 4>> depOwnerLoopDims =
        getDependencyOwnerLoopDims(source, dep, "compact halo");
    if (failed(depOwnerLoopDims))
      return failure();
    bool useExactNdHalo = false;
    if (dep.ownerDimCount == 2) {
      FailureOr<bool> needsExact = needsExactNdHaloFor2D(
          source, dep, computeBlock, *depOwnerLoopDims);
      if (failed(needsExact))
        return failure();
      useExactNdHalo = *needsExact;
    }
    if (dep.ownerDimCount == 2 && !useExactNdHalo) {
      FailureOr<CompactHaloColumnSpec> compactSpec =
          realizeCompactHaloColumnPacks(source, dep, *depGroupBlockCounts,
                                        builder, loc);
      if (failed(compactSpec))
        return failure();
      compactColumnSpecByDepIndex[depIndex] =
          static_cast<unsigned>(compactHaloColumnSpecs.size());
      compactHaloColumnSpecs.push_back(*compactSpec);
      continue;
    }
    FailureOr<CompactHaloNdSpec> compactSpec = realizeCompactHaloNdPacks(
        source, dep, *depGroupBlockCounts, computeBlock,
        *depOwnerLoopDims, builder, loc);
    if (failed(compactSpec))
      return failure();
    compactNdSpecByDepIndex[depIndex] =
        static_cast<unsigned>(compactHaloNdSpecs.size());
    compactHaloNdSpecs.push_back(*compactSpec);
  }
  bool hasCompactNdPackWork =
      llvm::any_of(compactHaloNdSpecs, [](const CompactHaloNdSpec &spec) {
        return !spec.sides.empty();
      });
  if (!compactHaloColumnSpecs.empty() || hasCompactNdPackWork) {
    auto reason = arts::ArtsBarrierReasonAttr::get(
        source.getContext(), arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(builder, loc, reason);
  }

  SmallVector<Value, 4> dispatchIvs;
  SmallVector<Value, 4> dispatchBases;
  SmallVector<Value, 4> dispatchEnds;
  SmallVector<Value, 4> dispatchBlockOffsets;
  scf::ForOp dispatchRoot;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerRouteping->loopDims[slot];
    Value step = createConstantIndex(builder, loc, workerSpans[slot]);
    auto loop =
        scf::ForOp::create(builder, loc, source.getLowerBounds()[physicalDim],
                           source.getUpperBounds()[physicalDim], step);
    if (!dispatchRoot)
      dispatchRoot = loop;
    dispatchIvs.push_back(loop.getInductionVar());
    builder.setInsertionPointToStart(loop.getBody());
  }

  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerRouteping->loopDims[slot];
    Value base = dispatchIvs[slot];
    Value lower = source.getLowerBounds()[physicalDim];
    Value upper = source.getUpperBounds()[physicalDim];
    Value blockSize = createConstantIndex(builder, loc, ownerBlockSizes[slot]);
    Value blockOffset = arith::DivUIOp::create(builder, loc, base, blockSize);
    Value blockStart =
        arith::MulIOp::create(builder, loc, blockOffset, blockSize);
    Value groupBlockCount =
        createConstantIndex(builder, loc, groupBlockCounts[slot]);
    Value blockEndOffset =
        arith::AddIOp::create(builder, loc, blockOffset, groupBlockCount);
    Value blockEnd =
        arith::MulIOp::create(builder, loc, blockEndOffset, blockSize);
    dispatchBlockOffsets.push_back(blockOffset);
    dispatchBases.push_back(
        arith::MaxUIOp::create(builder, loc, blockStart, lower));
    dispatchEnds.push_back(
        arith::MinUIOp::create(builder, loc, blockEnd, upper));
  }
  DenseMap<unsigned, unsigned> dispatchSlotByLoopDim;
  for (auto [slot, loopDim] : llvm::enumerate(ownerRouteping->loopDims))
    dispatchSlotByLoopDim.try_emplace(loopDim, static_cast<unsigned>(slot));
  auto findDispatchSlotForDepOwnerSlot =
      [&](const DirectDepSpec &dep,
          unsigned depSlot) -> std::optional<unsigned> {
    int64_t physicalDim =
        dep.arrayOwnerDims && depSlot < dep.arrayOwnerDims->size()
            ? (*dep.arrayOwnerDims)[depSlot]
            : static_cast<int64_t>(depSlot);
    auto it = llvm::find(ownerRouteping->ownerDims, physicalDim);
    std::optional<unsigned> physicalSlot;
    if (it != ownerRouteping->ownerDims.end())
      physicalSlot = static_cast<unsigned>(
          std::distance(ownerRouteping->ownerDims.begin(), it));
    if (!arts::DbUtils::isWriterMode(dep.mode) && !dep.arrayOwnerDims &&
        depSlot < dep.accessSlots.size()) {
      const DepOwnerAccessSlot &access = dep.accessSlots[depSlot];
      if (access.loopDim) {
        auto loopIt = llvm::find(ownerRouteping->loopDims, *access.loopDim);
        if (loopIt != ownerRouteping->loopDims.end()) {
          unsigned loopSlot = static_cast<unsigned>(
              std::distance(ownerRouteping->loopDims.begin(), loopIt));
          if (!physicalSlot ||
              ownerRouteping->loopDims[*physicalSlot] != *access.loopDim)
            return loopSlot;
        }
      }
    }
    return physicalSlot;
  };

  unsigned ndHaloSideCount = 0;
  for (const CompactHaloNdSpec &spec : compactHaloNdSpecs)
    ndHaloSideCount += spec.sides.size();
  unsigned taskDepReserve =
      deps.size() + compactHaloColumnSpecs.size() * 4 + ndHaloSideCount;
  const bool hasPartialReduction = hasCommittedPartialReductionFacts(source);
  BoundaryTaskDependencyBuilder taskDepBuilder(source.getContext(),
                                               hasPartialReduction);
  taskDepBuilder.reserve(taskDepReserve);
  SmallVector<unsigned, 4> primaryTaskDepForDep(deps.size(), 0);
  SmallVector<Halo2DTaskWork, 4> haloTaskWorks;
  DenseMap<unsigned, unsigned> haloTaskWorkByDepIndex;
  SmallVector<HaloNdTaskWork, 4> haloNdTaskWorks;
  DenseMap<unsigned, unsigned> haloNdTaskWorkByDepIndex;

  for (auto [depIndex, dep] : llvm::enumerate(deps)) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    SmallVector<int64_t, 4> depGroupBlockCounts(dep.ownerDimCount, 1);
    offsets.reserve(dep.ownerDimCount);
    sizes.reserve(dep.ownerDimCount);

    if (dep.reduceScatter) {
      for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
        offsets.push_back(createZeroIndex(builder, loc));
        sizes.push_back(dep.alloc.getSizes()[slot]);
      }
    } else {
      if (dep.accessSlots.size() != dep.ownerDimCount)
        return source.emitOpError() << "dependency access-window coordinates "
                                       "do not cover owner rank";
      for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
        const DepOwnerAccessSlot &access = dep.accessSlots[slot];
        if (access.fullWindow) {
          if (arts::DbUtils::isWriterMode(dep.mode)) {
            std::optional<unsigned> dispatchSlot =
                findDispatchSlotForDepOwnerSlot(dep, slot);
            if (!dispatchSlot) {
              if (slot < dep.blockLo.size() && slot < dep.blockHi.size() &&
                  dep.blockHi[slot] - dep.blockLo[slot] <= 1) {
                offsets.push_back(
                    createConstantIndex(builder, loc, dep.blockLo[slot]));
                sizes.push_back(createOneIndex(builder, loc));
                depGroupBlockCounts[slot] = 1;
                continue;
              }
              return source.emitOpError()
                     << "commits a writable full-window owner slot that is "
                        "not dispatched; distributed lowering requires SDE to "
                        "expose that owner loop";
            }
            if (*dispatchSlot >= ownerBlockSizes.size() ||
                *dispatchSlot >= workerSpans.size())
              return source.emitOpError()
                     << "writable full-window owner slot maps outside the "
                        "resolved dispatch layout";
            int64_t coordinateBlockSize = ownerBlockSizes[*dispatchSlot];
            if (coordinateBlockSize <= 0)
              return source.emitOpError()
                     << "writable full-window owner slot has non-positive "
                        "block size";

            Value coordinateBlockSizeValue =
                createConstantIndex(builder, loc, coordinateBlockSize);
            Value rawOffset = arith::DivUIOp::create(
                builder, loc, dispatchBases[*dispatchSlot],
                coordinateBlockSizeValue);
            int64_t staticGroupCount = std::max<int64_t>(
                1, ceilDivPositiveI64(workerSpans[*dispatchSlot],
                                      coordinateBlockSize));
            Value rawGroupEnd = arith::AddIOp::create(
                builder, loc, rawOffset,
                createConstantIndex(builder, loc, staticGroupCount));
            Value rawEnd = arith::MinUIOp::create(
                builder, loc,
                ceilDivPositiveIndex(builder, loc, dispatchEnds[*dispatchSlot],
                                     coordinateBlockSizeValue),
                rawGroupEnd);
            Value offset = rawOffset;
            if (dep.blockLo[slot] != 0) {
              Value windowLo =
                  createConstantIndex(builder, loc, dep.blockLo[slot]);
              offset =
                  arith::MaxUIOp::create(builder, loc, rawOffset, windowLo);
            }
            Value windowHi =
                createConstantIndex(builder, loc, dep.blockHi[slot]);
            Value end = arith::MinUIOp::create(builder, loc, rawEnd, windowHi);
            Value remaining = arith::SubIOp::create(
                builder, loc, dep.alloc.getSizes()[slot], offset);
            Value count = arith::SubIOp::create(builder, loc, end, offset);
            count = arith::MinUIOp::create(
                builder, loc, count,
                createConstantIndex(builder, loc, staticGroupCount));
            offsets.push_back(offset);
            sizes.push_back(
                arith::MinUIOp::create(builder, loc, remaining, count));
            depGroupBlockCounts[slot] = staticGroupCount;
            continue;
          }
          Value offset = createConstantIndex(builder, loc, dep.blockLo[slot]);
          int64_t staticGroupCount =
              std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
          Value requested = createConstantIndex(builder, loc, staticGroupCount);
          Value remaining = arith::SubIOp::create(
              builder, loc, dep.alloc.getSizes()[slot], offset);
          offsets.push_back(offset);
          sizes.push_back(
              arith::MinUIOp::create(builder, loc, remaining, requested));
          depGroupBlockCounts[slot] = staticGroupCount;
          continue;
        }
        if (access.fixedBlock) {
          offsets.push_back(
              createConstantIndex(builder, loc, *access.fixedBlock));
          sizes.push_back(createOneIndex(builder, loc));
          depGroupBlockCounts[slot] = 1;
          continue;
        }
        if (!access.loopDim ||
            *access.loopDim >= source.getUpperBounds().size())
          return source.emitOpError()
                 << "dependency access-window coordinate has no loop dimension";
        unsigned physicalDim = *access.loopDim;
        FailureOr<unsigned> depPayloadDim =
            getAccessWindowPayloadDim(source, dep, slot, physicalDim);
        if (failed(depPayloadDim))
          return failure();
        FailureOr<int64_t> payloadExtent =
            getAccessWindowPayloadExtent(source, dep, *depPayloadDim);
        if (failed(payloadExtent))
          return failure();
        (void)payloadExtent;
        int64_t coordinateBlockSize = access.coordinateBlockSize;
        if (coordinateBlockSize <= 0)
          return source.emitOpError()
                 << "dependency access-window coordinate has non-positive "
                    "block size";
        Value lower = source.getLowerBounds()[physicalDim];
        Value upper = source.getUpperBounds()[physicalDim];
        Value coordinateBlockSizeValue =
            createConstantIndex(builder, loc, coordinateBlockSize);
        Value base = lower;
        Value groupEnd = upper;
        int64_t staticGroupCount =
            std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
        auto dispatchIt = dispatchSlotByLoopDim.find(physicalDim);
        if (dispatchIt != dispatchSlotByLoopDim.end()) {
          unsigned dispatchSlot = dispatchIt->second;
          base = dispatchBases[dispatchSlot];
          if (arts::DbUtils::isWriterMode(dep.mode) && !dep.haloShape) {
            staticGroupCount = std::max<int64_t>(
                1, ceilDivPositiveI64(workerSpans[dispatchSlot],
                                      coordinateBlockSize));
            groupEnd = dispatchEnds[dispatchSlot];
          } else {
            bool exactBlockAlignedDispatch = hasBlockAlignedDispatchBase(
                lower, workerSpans[dispatchSlot], coordinateBlockSize);
            staticGroupCount = getStaticAccessGroupCount(
                workerSpans[dispatchSlot], coordinateBlockSize,
                access.minElementOffset, access.maxElementOffset,
                exactBlockAlignedDispatch);
            groupEnd = dispatchEnds[dispatchSlot];
          }
        }
        Value zero = createZeroIndex(builder, loc);
        Value startElement = base;
        if (access.minElementOffset != 0)
          startElement = arith::AddIOp::create(
              builder, loc, startElement,
              createConstantIndex(builder, loc, access.minElementOffset));
        startElement = arith::MaxSIOp::create(builder, loc, startElement, zero);
        Value endElement = groupEnd;
        if (access.maxElementOffset != 0)
          endElement = arith::AddIOp::create(
              builder, loc, endElement,
              createConstantIndex(builder, loc, access.maxElementOffset));
        endElement = arith::MaxSIOp::create(builder, loc, endElement, zero);
        Value rawOffset = arith::DivUIOp::create(builder, loc, startElement,
                                                 coordinateBlockSizeValue);
        Value rawEnd = ceilDivPositiveIndex(builder, loc, endElement,
                                            coordinateBlockSizeValue);
        Value offset = rawOffset;
        if (dep.blockLo[slot] != 0) {
          Value windowLo = createConstantIndex(builder, loc, dep.blockLo[slot]);
          offset = arith::MaxUIOp::create(builder, loc, rawOffset, windowLo);
        }
        Value windowHi = createConstantIndex(builder, loc, dep.blockHi[slot]);
        Value end = arith::MinUIOp::create(builder, loc, rawEnd, windowHi);
        Value remaining = arith::SubIOp::create(
            builder, loc, dep.alloc.getSizes()[slot], offset);
        Value count = arith::SubIOp::create(builder, loc, end, offset);
        // Bound the span by the static group count so
        // DistributedLaunchConsistency can prove owner-local routing (a
        // dynamic-only span has no known bound).
        Value staticBound = createConstantIndex(builder, loc, staticGroupCount);
        count = arith::MinUIOp::create(builder, loc, count, staticBound);
        sizes.push_back(arith::MinUIOp::create(builder, loc, remaining, count));
        offsets.push_back(offset);
        depGroupBlockCounts[slot] = staticGroupCount;
      }
    }
    bool requiresDbRef = dep.reduceScatter.has_value() ||
                         llvm::any_of(depGroupBlockCounts,
                                      [](int64_t count) { return count > 1; });
    FailureOr<ArrayAttr> depResultDimMap =
        buildPartialReductionDepResultDimMap(source, dep);
    if (failed(depResultDimMap))
      return failure();

    if (dep.haloShape) {
      auto centerAcquire = arts::DbAcquireOp::create(
          builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
          std::optional<arts::PartitionMode>(arts::PartitionMode::block),
          SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
          SmallVector<Value>{}, SmallVector<Value>{}, Value{},
          SmallVector<Value>{}, SmallVector<Value>{});
      centerAcquire.setPreserveAccessMode();
      unsigned centerTaskDep = taskDepBuilder.append(
          centerAcquire.getPtr(), offsets, requiresDbRef, dep.ownerDimCount,
          depGroupBlockCounts, *depResultDimMap);
      primaryTaskDepForDep[depIndex] = centerTaskDep;

      auto columnSpecIt = compactColumnSpecByDepIndex.find(depIndex);
      if (columnSpecIt != compactColumnSpecByDepIndex.end()) {
        const CompactHaloColumnSpec &compactSpec =
            compactHaloColumnSpecs[columnSpecIt->second];
        SmallVector<int64_t, 4> rowFaceGroupCounts(depGroupBlockCounts.begin(),
                                                   depGroupBlockCounts.end());
        rowFaceGroupCounts[0] = 1;
        SmallVector<int64_t, 4> columnFaceGroupCounts(
            depGroupBlockCounts.begin(), depGroupBlockCounts.end());
        columnFaceGroupCounts[1] = 1;

        SmallVector<Value, 4> topOffsets;
        auto topAcquire = create2DUnitRowHaloAcquire(
            source, dep, offsets, sizes, compactSpec,
            /*topFace=*/true, topOffsets, builder, loc);
        unsigned topTaskDep = taskDepBuilder.append(
            topAcquire.getPtr(), topOffsets,
            /*requiresDbRef=*/hasGroupedOwnerBlocks(rowFaceGroupCounts),
            dep.ownerDimCount, rowFaceGroupCounts, ArrayAttr{});

        SmallVector<Value, 4> bottomOffsets;
        auto bottomAcquire = create2DUnitRowHaloAcquire(
            source, dep, offsets, sizes, compactSpec,
            /*topFace=*/false, bottomOffsets, builder, loc);
        unsigned bottomTaskDep = taskDepBuilder.append(
            bottomAcquire.getPtr(), bottomOffsets,
            /*requiresDbRef=*/hasGroupedOwnerBlocks(rowFaceGroupCounts),
            dep.ownerDimCount, rowFaceGroupCounts, ArrayAttr{});

        SmallVector<Value, 4> leftOffsets;
        auto leftAcquire = create2DUnitCompactColumnAcquire(
            dep, offsets, sizes, compactSpec,
            /*leftFace=*/true, leftOffsets, builder, loc);
        unsigned leftTaskDep = taskDepBuilder.append(
            leftAcquire.getPtr(), leftOffsets,
            /*requiresDbRef=*/hasGroupedOwnerBlocks(columnFaceGroupCounts),
            dep.ownerDimCount, columnFaceGroupCounts, ArrayAttr{});

        SmallVector<Value, 4> rightOffsets;
        auto rightAcquire = create2DUnitCompactColumnAcquire(
            dep, offsets, sizes, compactSpec,
            /*leftFace=*/false, rightOffsets, builder, loc);
        unsigned rightTaskDep = taskDepBuilder.append(
            rightAcquire.getPtr(), rightOffsets,
            /*requiresDbRef=*/hasGroupedOwnerBlocks(columnFaceGroupCounts),
            dep.ownerDimCount, columnFaceGroupCounts, ArrayAttr{});

        Halo2DTaskWork haloTaskWork;
        haloTaskWork.centerTaskDepIndex = centerTaskDep;
        haloTaskWork.topTaskDepIndex = topTaskDep;
        haloTaskWork.bottomTaskDepIndex = bottomTaskDep;
        haloTaskWork.leftTaskDepIndex = leftTaskDep;
        haloTaskWork.rightTaskDepIndex = rightTaskDep;
        haloTaskWork.centerGroupBlockCounts.assign(depGroupBlockCounts.begin(),
                                                   depGroupBlockCounts.end());
        haloTaskWork.rowExtent = compactSpec.rowExtent;
        haloTaskWork.colExtent = compactSpec.colExtent;
        haloTaskWorkByDepIndex[depIndex] =
            static_cast<unsigned>(haloTaskWorks.size());
        haloTaskWorks.push_back(haloTaskWork);
        continue;
      }

      auto specIt = compactNdSpecByDepIndex.find(depIndex);
      if (specIt == compactNdSpecByDepIndex.end())
        return source.emitOpError()
               << "lost compact N-D halo payload state for committed halo "
                  "dependency";
      const CompactHaloNdSpec &compactSpec = compactHaloNdSpecs[specIt->second];
      HaloNdTaskWork haloTaskWork;
      haloTaskWork.ownerDimCount = compactSpec.ownerDimCount;
      haloTaskWork.centerTaskDepIndex = centerTaskDep;
      haloTaskWork.ownerPayloadDims.assign(compactSpec.ownerPayloadDims.begin(),
                                           compactSpec.ownerPayloadDims.end());
      FailureOr<SmallVector<unsigned, 4>> depOwnerLoopDims =
          getDependencyOwnerLoopDims(source, dep, "compact halo");
      if (failed(depOwnerLoopDims))
        return failure();
      haloTaskWork.ownerLoopDims.assign(depOwnerLoopDims->begin(),
                                        depOwnerLoopDims->end());
      haloTaskWork.centerGroupBlockCounts.assign(depGroupBlockCounts.begin(),
                                                 depGroupBlockCounts.end());
      haloTaskWork.elementExtents.assign(compactSpec.elementExtents.begin(),
                                         compactSpec.elementExtents.end());
      for (const CompactHaloNdSideSpec &side : compactSpec.sides) {
        SmallVector<Value, 4> sideOffsets;
        SmallVector<Value, 4> sideSizes;
        auto sideAcquire = createNdCompactHaloAcquire(
            dep, offsets, sizes, side, sideOffsets, sideSizes, builder, loc);
        SmallVector<int64_t, 4> sideGroupCounts(depGroupBlockCounts.begin(),
                                                depGroupBlockCounts.end());
        for (auto [slot, sourceOffset] : llvm::enumerate(side.sourceOffsets))
          if (sourceOffset != 0)
            sideGroupCounts[slot] = 1;
        unsigned sideTaskDep = taskDepBuilder.append(
            sideAcquire.getPtr(), sideOffsets,
            hasGroupedOwnerBlocks(sideGroupCounts), dep.ownerDimCount,
            sideGroupCounts, ArrayAttr{});
        haloTaskWork.sideSourceOffsets.push_back(side.sourceOffsets);
        haloTaskWork.sideTaskDepIndices.push_back(sideTaskDep);
      }
      haloNdTaskWorkByDepIndex[depIndex] =
          static_cast<unsigned>(haloNdTaskWorks.size());
      haloNdTaskWorks.push_back(std::move(haloTaskWork));
      continue;
    }

    std::optional<arts::PartitionMode> partitionMode =
        std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    if (dep.ownerDimCount == 0) {
      buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
      partitionMode = arts::PartitionMode::coarse;
    }
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    if (dep.reduceScatter)
      acquire.setReplicatedReadAttr(UnitAttr::get(source.getContext()));
    primaryTaskDepForDep[depIndex] = taskDepBuilder.append(
        acquire.getPtr(), offsets, requiresDbRef, dep.ownerDimCount,
        depGroupBlockCounts, *depResultDimMap);
  }

  SmallVector<Value, 8> taskParams;
  taskParams.append(dispatchBases.begin(), dispatchBases.end());
  taskParams.append(dispatchBlockOffsets.begin(), dispatchBlockOffsets.end());
  taskParams.append(dispatchEnds.begin(), dispatchEnds.end());

  auto appendParamIfMissing = [&](Value value) -> unsigned {
    auto it = llvm::find(taskParams, value);
    if (it != taskParams.end())
      return static_cast<unsigned>(std::distance(taskParams.begin(), it));
    taskParams.push_back(value);
    return taskParams.size() - 1;
  };

  SmallVector<SmallVector<unsigned, 4>> depBlockOffsetParamIndices;
  depBlockOffsetParamIndices.reserve(taskDepBuilder.blockOffsets().size());
  for (const SmallVector<Value, 4> &offsets : taskDepBuilder.blockOffsets()) {
    SmallVector<unsigned, 4> paramIndices;
    paramIndices.reserve(ownerDimCount);
    for (Value offset : offsets)
      paramIndices.push_back(appendParamIfMissing(offset));
    depBlockOffsetParamIndices.push_back(std::move(paramIndices));
  }

  for (Value capture : scalarCaptures)
    appendParamIfMissing(capture);

  arts::ArtsLaunchPolicy launch = arts::resolveArtsLaunchPolicy(
      source->getParentOfType<ModuleOp>(), dispatchRoot,
      hasDistributedLaunchStorageFacts(deps), builder, loc);
  Value route =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          route, taskDepBuilder.values(), taskParams);
  if (failed(attachCommittedSdeFacts(source, task)))
    return failure();
  if (hasPartialReduction) {
    if (taskDepBuilder.partialReductionMaps().size() !=
        taskDepBuilder.values().size())
      return source.emitOpError()
             << "lost partial-reduction dependency/result mapping while "
                "building ARTS task dependencies";
    task.setPartialReductionDepResultDimMapsAttr(
        builder.getArrayAttr(taskDepBuilder.partialReductionMaps()));
  }
  if (splitToOwnerLocalGroups)
    task.setOwnerLocalWriterSplitAttr(UnitAttr::get(source.getContext()));

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDepBuilder.values())
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  SmallVector<Value, 4> payloads;
  payloads.reserve(taskDepBuilder.values().size());
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (unsigned idx = 0, e = taskDepBuilder.values().size(); idx < e; ++idx) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    payloads.push_back(payload);
  }
  auto mapIfAbsent = [&](Value from, Value to) {
    if (from && !mapper.lookupOrNull(from))
      mapper.map(from, to);
  };
  for (auto [depIdx, dep] : llvm::enumerate(deps)) {
    unsigned taskDepIndex = primaryTaskDepForDep[depIdx];
    mapIfAbsent(dep.alloc.getPtr(), taskBlock.getArgument(taskDepIndex));
  }
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));
  for (auto [idx, base] : llvm::enumerate(dispatchBases))
    mapper.map(base, taskBlock.getArgument(paramOffset + idx));
  for (auto [idx, offset] : llvm::enumerate(dispatchBlockOffsets))
    mapper.map(offset,
               taskBlock.getArgument(paramOffset + ownerDimCount + idx));
  for (auto [idx, end] : llvm::enumerate(dispatchEnds))
    mapper.map(end,
               taskBlock.getArgument(paramOffset + ownerDimCount * 2 + idx));
  SmallVector<SmallVector<Value, 4>> taskDepBlockOffsetArgs;
  taskDepBlockOffsetArgs.reserve(taskDepBuilder.blockOffsets().size());
  for (unsigned depIdx = 0; depIdx < taskDepBuilder.blockOffsets().size();
       ++depIdx) {
    SmallVector<Value, 4> offsets;
    unsigned depOwnerDimCount = taskDepBuilder.ownerDimCounts()[depIdx];
    offsets.reserve(depOwnerDimCount);
    for (unsigned slot = 0; slot < depOwnerDimCount; ++slot) {
      unsigned paramIndex = depBlockOffsetParamIndices[depIdx][slot];
      offsets.push_back(taskBlock.getArgument(paramOffset + paramIndex));
    }
    taskDepBlockOffsetArgs.push_back(std::move(offsets));
  }

  WalkResult accessWindowMapResult =
      source.getBody().walk([&](arts::DbAccessWindowOp window) {
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(window.getMu()));
        if (!alloc) {
          window.emitOpError() << "lost backing DB allocation during direct "
                                  "SDE-to-ARTS lowering";
          return WalkResult::interrupt();
        }
        auto it = llvm::find_if(
            deps, [&](const DirectDepSpec &dep) { return dep.alloc == alloc; });
        if (it == deps.end()) {
          window.emitOpError() << "has no matching direct ARTS dependency";
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
  if (accessWindowMapResult.wasInterrupted())
    return failure();

  DenseMap<Operation *, HaloLoadRewrite> originalHaloLoadRewrites;
  if (!haloTaskWorks.empty()) {
    if (ownerDimCount != 2 || ownerRouteping->loopDims.size() != 2)
      return source.emitOpError() << "commits a halo dependency whose owner "
                                     "rank is not supported by "
                                     "ARTS compact 2D unit-halo load rewriting";
    WalkResult classifyResult = computeBlock->walk([&](memref::LoadOp load) {
      arts::DbAllocOp alloc = resolveBoundaryDbAlloc(load.getMemref());
      if (!alloc)
        return WalkResult::advance();
      auto depIt = llvm::find_if(
          deps, [&](const DirectDepSpec &dep) { return dep.alloc == alloc; });
      if (depIt == deps.end())
        return WalkResult::advance();
      unsigned depIdx =
          static_cast<unsigned>(std::distance(deps.begin(), depIt));
      auto haloIt = haloTaskWorkByDepIndex.find(depIdx);
      if (haloIt == haloTaskWorkByDepIndex.end())
        return WalkResult::advance();

      FailureOr<SmallVector<Value, 4>> ownerLoopIvs =
          getCompactHaloOwnerLoopIvs(source, load, ownerRouteping->loopDims,
                                     "2D");
      if (failed(ownerLoopIvs))
        return WalkResult::interrupt();
      FailureOr<std::optional<HaloLoadRewrite>> rewrite =
          classify2DUnitHaloLoad(load, haloIt->second,
                                 haloTaskWorks[haloIt->second],
                                 (*ownerLoopIvs)[0], (*ownerLoopIvs)[1]);
      if (failed(rewrite))
        return WalkResult::interrupt();
      if (rewrite->has_value())
        originalHaloLoadRewrites[load.getOperation()] = **rewrite;
      return WalkResult::advance();
    });
    if (classifyResult.wasInterrupted())
      return failure();
  }

  DenseMap<Operation *, HaloNdLoadRewrite> originalHaloNdLoadRewrites;
  if (!haloNdTaskWorks.empty()) {
    WalkResult classifyResult =
        computeBlock->walk([&](memref::LoadOp load) {
          arts::DbAllocOp alloc = resolveBoundaryDbAlloc(load.getMemref());
          if (!alloc)
            return WalkResult::advance();
          auto depIt = llvm::find_if(deps, [&](const DirectDepSpec &dep) {
            return dep.alloc == alloc;
          });
          if (depIt == deps.end())
            return WalkResult::advance();
          unsigned depIdx =
              static_cast<unsigned>(std::distance(deps.begin(), depIt));
          auto haloIt = haloNdTaskWorkByDepIndex.find(depIdx);
          if (haloIt == haloNdTaskWorkByDepIndex.end())
            return WalkResult::advance();

          const HaloNdTaskWork &work = haloNdTaskWorks[haloIt->second];
          unsigned ownerRank = work.ownerDimCount;
          if (work.ownerLoopDims.size() != ownerRank) {
            load.emitOpError()
                << "is not nested in the N-D compute loops required for "
                   "ARTS compact unit-halo load rewriting";
            return WalkResult::interrupt();
          }
          FailureOr<SmallVector<Value, 4>> ownerLoopIvs =
              getCompactHaloOwnerLoopIvs(source, load, work.ownerLoopDims,
                                         "N-D");
          if (failed(ownerLoopIvs))
            return WalkResult::interrupt();
          FailureOr<std::optional<HaloNdLoadRewrite>> rewrite =
              classifyNdUnitHaloLoad(load, haloIt->second, work, *ownerLoopIvs);
          if (failed(rewrite))
            return WalkResult::interrupt();
          if (rewrite->has_value())
            originalHaloNdLoadRewrites[load.getOperation()] = **rewrite;
          return WalkResult::advance();
        });
    if (classifyResult.wasInterrupted())
      return failure();
  }

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    auto ownerIt = llvm::find(ownerRouteping->loopDims, dim);
    Value lower;
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    if (ownerIt != ownerRouteping->loopDims.end()) {
      unsigned slot = static_cast<unsigned>(
          std::distance(ownerRouteping->loopDims.begin(), ownerIt));
      lower = mapper.lookup(dispatchBases[slot]);
      upper = mapper.lookup(dispatchEnds[slot]);
    } else {
      lower = remapOrSelf(mapper, source.getLowerBounds()[dim]);
    }
    auto localLoop = scf::ForOp::create(bodyBuilder, loc, lower, upper, step);
    mapper.map(source.getBody().front().getArgument(dim),
               localLoop.getInductionVar());
    bodyBuilder.setInsertionPointToStart(localLoop.getBody());
  }
  SmallVector<Value, 4> ownerLocalSteps;
  ownerLocalSteps.reserve(ownerDimCount);
  for (int64_t workerSpan : workerSpans)
    ownerLocalSteps.push_back(
        createConstantIndex(bodyBuilder, loc, workerSpan));
  SmallVector<OwnerLocalStepRewrite, 4> ownerLocalStepRewrites;
  if (splitToOwnerLocalGroups)
    ownerLocalStepRewrites = collectOwnerLocalStepRewrites(
        source, computeBlock, ownerRouteping->loopDims);

  DenseMap<Operation *, HaloLoadRewrite> clonedHaloLoadRewrites;
  DenseMap<Operation *, HaloNdLoadRewrite> clonedHaloNdLoadRewrites;
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessWindowOp>(&nested))
      continue;
    Operation *cloned = nested.clone(mapper);
    bodyBuilder.insert(cloned);
    if (!originalHaloLoadRewrites.empty())
      if (failed(recordClonedHaloLoadRewrites(&nested, cloned,
                                              originalHaloLoadRewrites,
                                              clonedHaloLoadRewrites)))
        return failure();
    if (!originalHaloNdLoadRewrites.empty())
      if (failed(recordClonedHaloLoadRewrites(&nested, cloned,
                                              originalHaloNdLoadRewrites,
                                              clonedHaloNdLoadRewrites)))
        return failure();
  }
  if (splitToOwnerLocalGroups &&
      failed(applyOwnerLocalStepRewrites(source, ownerLocalStepRewrites, mapper,
                                         ownerLocalSteps)))
    return failure();

  auto rewriteClonedAccess = [&](Operation *op, Value memref,
                                 ArtsMode mode) -> WalkResult {
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return WalkResult::advance();
    std::optional<unsigned> depIdx =
        findDirectDepIndexForAccess(deps, alloc, mode,
                                    /*preferHaloRead=*/true);
    if (!depIdx) {
      op->emitError()
          << "has no committed SDE access-window dependency for direct "
             "ARTS lowering";
      return WalkResult::interrupt();
    }
    unsigned taskDepIndex = primaryTaskDepForDep[*depIdx];
    if (taskDepIndex >= payloads.size()) {
      op->emitError() << "lost direct dependency payload while lowering "
                         "SDE access window";
      return WalkResult::interrupt();
    }
    if (auto load = dyn_cast<memref::LoadOp>(op))
      load.getMemrefMutable().assign(payloads[taskDepIndex]);
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      store.getMemrefMutable().assign(payloads[taskDepIndex]);
    return WalkResult::advance();
  };
  WalkResult rewriteResult = task.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteClonedAccess(op, load.getMemref(), ArtsMode::in);
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteClonedAccess(op, store.getMemref(), ArtsMode::out);
    return WalkResult::advance();
  });
  if (rewriteResult.wasInterrupted())
    return failure();

  if (failed(rewriteCloned2DUnitHaloLoads(task, clonedHaloLoadRewrites,
                                          haloTaskWorks, payloads,
                                          taskDepBlockOffsetArgs)))
    return failure();
  if (failed(rewriteClonedNdUnitHaloLoads(task, clonedHaloNdLoadRewrites,
                                          haloNdTaskWorks, payloads,
                                          taskDepBlockOffsetArgs)))
    return failure();
  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  if (failed(rewriteOwnerIndicesToLocal(task, payloads, taskDepBlockOffsetArgs,
                                        taskDepBuilder.requiresDbRefs(),
                                        taskDepBuilder.ownerDimCounts(),
                                        taskDepBuilder.groupBlockCounts())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  bool needsCompletionBarrier = !source.getNowaitAttr();
  MLIRContext *ctx = source.getContext();
  source.erase();
  if (needsCompletionBarrier) {
    OpBuilder barrierBuilder(dispatchRoot);
    barrierBuilder.setInsertionPointAfter(dispatchRoot);
    auto reason = arts::ArtsBarrierReasonAttr::get(
        ctx, arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(barrierBuilder, loc, reason);
  }
  return success();
}

static FailureOr<ArtsOwnerSlotMapping> resolveWindowedDispatchOwnerSlotMapping(
    sde::SdeSuIterateOp source, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> blockShape, unsigned loopRank,
    ArrayRef<DirectDepSpec> deps) {
  SmallVector<std::optional<unsigned>, 4> loopDimByRawSlot(ownerDims.size());
  auto dispatchSlotForPhysicalDim =
      [&](int64_t physicalDim) -> std::optional<unsigned> {
    auto it = llvm::find(ownerDims, physicalDim);
    if (it == ownerDims.end())
      return std::nullopt;
    return static_cast<unsigned>(std::distance(ownerDims.begin(), it));
  };
  auto existingDispatchSlotForLoopDim =
      [&](unsigned loopDim) -> std::optional<unsigned> {
    for (auto [rawSlot, mappedLoopDim] : llvm::enumerate(loopDimByRawSlot))
      if (mappedLoopDim && *mappedLoopDim == loopDim)
        return static_cast<unsigned>(rawSlot);
    return std::nullopt;
  };
  auto dispatchSlotForDepSlot =
      [&](const DirectDepSpec &dep, unsigned depSlot,
          const DepOwnerAccessSlot &slot) -> std::optional<unsigned> {
    int64_t physicalDim =
        dep.arrayOwnerDims && depSlot < dep.arrayOwnerDims->size()
            ? (*dep.arrayOwnerDims)[depSlot]
            : static_cast<int64_t>(depSlot);
    std::optional<unsigned> physicalSlot =
        dispatchSlotForPhysicalDim(physicalDim);
    if (arts::DbUtils::isWriterMode(dep.mode) || !slot.loopDim)
      return physicalSlot;
    if (dep.arrayOwnerDims) {
      if (physicalSlot && loopDimByRawSlot[*physicalSlot]) {
        if (*loopDimByRawSlot[*physicalSlot] == *slot.loopDim)
          return physicalSlot;
        if (std::optional<unsigned> loopSlot =
                existingDispatchSlotForLoopDim(*slot.loopDim))
          return loopSlot;
      }
      return physicalSlot;
    }
    if (physicalSlot && loopDimByRawSlot[*physicalSlot] &&
        *loopDimByRawSlot[*physicalSlot] == *slot.loopDim)
      return physicalSlot;
    if (std::optional<unsigned> loopSlot =
            existingDispatchSlotForLoopDim(*slot.loopDim))
      return loopSlot;
    return physicalSlot;
  };

  auto recordDeps = [&](bool writerPhase) -> LogicalResult {
    for (const DirectDepSpec &dep : deps) {
      if (arts::DbUtils::isWriterMode(dep.mode) != writerPhase)
        continue;
      unsigned limit = dep.accessSlots.size();
      for (unsigned depSlot = 0; depSlot < limit; ++depSlot) {
        const DepOwnerAccessSlot &slot = dep.accessSlots[depSlot];
        std::optional<unsigned> rawSlot =
            dispatchSlotForDepSlot(dep, depSlot, slot);
        if (!rawSlot)
          continue;
        if (!slot.loopDim) {
          if (!slot.fullWindow && !slot.fixedBlock)
            return source.emitOpError()
                   << "cannot lower an owner slot that is neither dispatched "
                      "nor proven to be a full/fixed DB window";
          continue;
        }
        if (*slot.loopDim >= loopRank)
          return source.emitOpError()
                 << "dependency access-window coordinate names a loop "
                    "dimension outside the SDE dispatch rank";
        if (loopDimByRawSlot[*rawSlot] &&
            *loopDimByRawSlot[*rawSlot] != *slot.loopDim)
          return source.emitOpError()
                 << "commits conflicting dispatch loop dimensions for one "
                    "owner slot";
        loopDimByRawSlot[*rawSlot] = *slot.loopDim;
      }
    }
    return success();
  };
  if (failed(recordDeps(/*writerPhase=*/true)) ||
      failed(recordDeps(/*writerPhase=*/false)))
    return failure();

  bool sawAccessMappedSlot =
      llvm::any_of(loopDimByRawSlot, [](std::optional<unsigned> loopDim) {
        return loopDim.has_value();
      });
  if (!sawAccessMappedSlot) {
    if (ownerDims.size() <= loopRank)
      return resolveArtsOwnerSlotMapping(ownerDims, blockShape, loopRank,
                                         source.getOperation());
    return source.emitOpError()
           << "commits owner slots but no dispatchable owner loop";
  }

  if (ownerDims.size() <= loopRank) {
    for (unsigned rawSlot = 0; rawSlot < ownerDims.size(); ++rawSlot) {
      if (loopDimByRawSlot[rawSlot])
        continue;
      int64_t ownerDim = ownerDims[rawSlot];
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= loopRank)
        return source.emitOpError()
               << "cannot infer a dispatch loop for an unmapped owner slot";
      loopDimByRawSlot[rawSlot] = static_cast<unsigned>(ownerDim);
    }
  }

  SmallVector<char, 4> seenLoop(loopRank, 0);
  SmallVector<std::tuple<int64_t, unsigned, int64_t, unsigned>, 4> slots;
  for (auto [rawSlot, loopDim] : llvm::enumerate(loopDimByRawSlot)) {
    if (!loopDim)
      continue;
    int64_t ownerDim = ownerDims[rawSlot];
    if (ownerDim < 0)
      return source.emitOpError() << "owner dim is negative";
    if (seenLoop[*loopDim])
      return source.emitOpError()
             << "maps multiple physical owner dimensions to one loop "
                "dimension";
    seenLoop[*loopDim] = 1;

    int64_t blockSize = 0;
    if (static_cast<size_t>(ownerDim) < blockShape.size())
      blockSize = blockShape[ownerDim];
    else if (rawSlot < blockShape.size())
      blockSize = blockShape[rawSlot];
    else
      return source.emitOpError()
             << "physicalOwnerDims must index physicalBlockShape dimensions";
    if (blockSize <= 0)
      return source.emitOpError()
             << "requires a positive physical block size for every owner dim";
    slots.emplace_back(ownerDim, *loopDim, blockSize,
                       static_cast<unsigned>(rawSlot));
  }

  if (slots.empty())
    return source.emitOpError()
           << "commits owner slots but no dispatchable owner loop";
  llvm::sort(slots, [](const auto &lhs, const auto &rhs) {
    return std::get<0>(lhs) < std::get<0>(rhs);
  });

  ArtsOwnerSlotMapping mapping;
  for (auto [ownerDim, loopDim, blockSize, rawSlot] : slots) {
    mapping.ownerDims.push_back(ownerDim);
    mapping.loopDims.push_back(loopDim);
    mapping.blockSizes.push_back(blockSize);
    mapping.rawSlots.push_back(rawSlot);
  }
  return mapping;
}

static bool writerHasNonDispatchedFullWindowOwnerSlot(
    ArrayRef<DirectDepSpec> deps, ArrayRef<int64_t> dispatchedOwnerDims) {
  auto isDispatched = [&](int64_t ownerDim) {
    return llvm::is_contained(dispatchedOwnerDims, ownerDim);
  };
  for (const DirectDepSpec &dep : deps) {
    if (!arts::DbUtils::isWriterMode(dep.mode))
      continue;
    for (unsigned slot = 0; slot < dep.accessSlots.size(); ++slot) {
      const DepOwnerAccessSlot &access = dep.accessSlots[slot];
      if (!access.fullWindow || slot >= dep.blockLo.size() ||
          slot >= dep.blockHi.size() ||
          dep.blockHi[slot] - dep.blockLo[slot] <= 1)
        continue;
      int64_t ownerDim = dep.arrayOwnerDims && slot < dep.arrayOwnerDims->size()
                             ? (*dep.arrayOwnerDims)[slot]
                             : static_cast<int64_t>(slot);
      if (!isDispatched(ownerDim))
        return true;
    }
  }
  return false;
}

LogicalResult collectTaskDependencies(sde::SdeCuTaskOp source,
                                      SmallVectorImpl<TaskDepSpec> &deps) {
  WalkResult result =
      source.getBody().walk([&](sde::SdeMuDepOp dep) {
        if (!dep.getDep().use_empty()) {
          dep.emitOpError()
              << "result is consumed; SDE task dependencies must remain local "
                 "declarations before ARTS realization";
          return WalkResult::interrupt();
        }
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(dep.getSource()));
        if (!alloc) {
          dep.emitOpError()
              << "does not reference an ARTS DB-backed memref after storage "
                 "realization";
          return WalkResult::interrupt();
        }
        FailureOr<ArtsMode> mode = convertAccessMode(dep.getMode(), dep);
        if (failed(mode))
          return WalkResult::interrupt();
        deps.push_back({dep, alloc, *mode,
                        SmallVector<Value, 4>(dep.getOffsets().begin(),
                                              dep.getOffsets().end()),
                        SmallVector<Value, 4>(dep.getSizes().begin(),
                                              dep.getSizes().end())});
        return WalkResult::advance();
      });
  return result.wasInterrupted() ? failure() : success();
}

LogicalResult convertCuTask(sde::SdeCuTaskOp source) {
  SmallVector<TaskDepSpec, 4> deps;
  if (failed(collectTaskDependencies(source, deps)))
    return failure();

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (TaskDepSpec &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
    std::optional<arts::PartitionMode> partitionMode =
        dep.offsets.empty() && dep.sizes.empty()
            ? std::optional<arts::PartitionMode>(arts::PartitionMode::coarse)
            : std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{},
        SmallVector<Value>(dep.offsets.begin(), dep.offsets.end()),
        SmallVector<Value>(dep.sizes.begin(), dep.sizes.end()), Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    acquire.setPreserveDepEdge();
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  for (Value capture : scalarCaptures)
    taskParams.push_back(capture);
  auto appendParamIfMissing = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value) || llvm::is_contained(taskParams, value))
      return;
    taskParams.push_back(value);
  };
  for (TaskDepSpec &dep : deps) {
    for (Value size : dep.alloc.getSizes())
      appendParamIfMissing(size);
    for (Value elementSize : dep.alloc.getElementSizes())
      appendParamIfMissing(elementSize);
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto task = arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  arts::EdtConcurrency::intranode, route,
                                  taskDeps, taskParams);
  if (auto pattern = source.getPatternAttr()) {
    FailureOr<ArtsDepPattern> depPattern =
        convertPattern(pattern.getValue(), source.getOperation());
    if (failed(depPattern))
      return failure();
    arts::setDepPattern(task.getOperation(), *depPattern);
  }

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
    Value sourceMemref = dep.dep.getSource();
    mapper.map(sourceMemref, payload);
    if (Value root = ValueAnalysis::stripMemrefViewOps(sourceMemref))
      mapper.map(root, payload);
  }
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  for (Operation &nested : source.getBody().front()) {
    if (isa<sde::SdeMuDepOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  source.erase();
  return success();
}

} // namespace mlir::carts::arts::boundary
