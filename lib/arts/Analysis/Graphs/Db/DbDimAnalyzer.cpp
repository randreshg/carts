///==========================================================================///
/// File: DbDimAnalyzer.cpp
///
/// Canonical per-entry / per-dimension partition facts for DbAcquireNode.
///==========================================================================///

#include "arts/Analysis/Graphs/Db/DbDimAnalyzer.h"
#include "arts/Analysis/Graphs/Db/MemoryAccessClassifier.h"
#include "arts/Analysis/Graphs/Db/PartitionBoundsAnalyzer.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;
using namespace mlir::arts;

namespace {

static PartitionMode getRequestedMode(DbAcquireOp acquire) {
  if (auto mode = getPartitionMode(acquire.getOperation()))
    return *mode;
  return DbUtils::getPartitionModeFromStructure(acquire);
}

static std::pair<Value, Value>
pickRepresentativeRange(PartitionMode mode, const PartitionInfo &info) {
  auto pickRepresentative = [](ArrayRef<Value> values) -> Value {
    for (Value value : values) {
      if (!value)
        continue;
      int64_t constant = 0;
      if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(value),
                                        constant))
        return value;
    }
    for (Value value : values) {
      if (value)
        return value;
    }
    return Value();
  };

  Value offset;
  if (mode == PartitionMode::fine_grained)
    offset = pickRepresentative(info.indices);
  else
    offset = pickRepresentative(info.offsets);

  Value size;
  if (!info.sizes.empty())
    size = pickRepresentative(info.sizes);
  return {offset, size};
}

static SmallVector<PartitionInfo, 2> collectPartitionEntries(DbAcquireOp acquire) {
  SmallVector<PartitionInfo, 2> entries = acquire.getPartitionInfos();
  if (!entries.empty())
    return entries;

  PartitionMode mode = getRequestedMode(acquire);
  if (mode == PartitionMode::coarse)
    return entries;

  PartitionInfo info;
  info.mode = mode;
  info.indices.append(acquire.getPartitionIndices().begin(),
                      acquire.getPartitionIndices().end());
  info.offsets.append(acquire.getPartitionOffsets().begin(),
                      acquire.getPartitionOffsets().end());
  info.sizes.append(acquire.getPartitionSizes().begin(),
                    acquire.getPartitionSizes().end());
  if (!info.indices.empty() || !info.offsets.empty() || !info.sizes.empty())
    entries.push_back(std::move(info));
  return entries;
}

static unsigned getPartitionRank(DbAcquireNode *node) {
  if (!node)
    return 0;
  DbAllocNode *allocNode = node->getRootAlloc();
  if (!allocNode)
    return 0;
  if (auto memrefType =
          dyn_cast<MemRefType>(allocNode->getDbAllocOp().getElementType()))
    return memrefType.getRank();
  return allocNode->getDbAllocOp().getElementSizes().size();
}

static void initializeDimFacts(DbAcquireNode *node, DbAcquirePartitionFacts &facts) {
  unsigned rank = getPartitionRank(node);
  facts.dims.reserve(rank);

  SmallVector<MemrefMetadata::DimAccessPatternType> allocDimPatterns;
  if (DbAllocNode *allocNode = node->getRootAlloc())
    allocDimPatterns = allocNode->dimAccessPatterns;

  for (unsigned dim = 0; dim < rank; ++dim) {
    DbDimPartitionFact fact;
    fact.dim = dim;
    if (dim < allocDimPatterns.size())
      fact.accessPattern = allocDimPatterns[dim];
    facts.dims.push_back(fact);
  }
}

static void markLeadingDynamicDims(DbAcquireNode *node,
                                   DbAcquirePartitionFacts &facts) {
  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  MemoryAccessClassifier::collectAccessOperations(node, dbRefToMemOps);

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain = DbUtils::collectFullIndexChain(dbRef, memOp);
      if (fullChain.empty())
        continue;

      unsigned memrefStart = dbRef.getIndices().size();
      for (unsigned chainIdx = memrefStart; chainIdx < fullChain.size(); ++chainIdx) {
        int64_t constant = 0;
        if (ValueUtils::getConstantIndex(fullChain[chainIdx], constant))
          continue;

        unsigned dim = chainIdx - memrefStart;
        if (dim < facts.dims.size())
          facts.dims[dim].isLeadingDynamic = true;
        break;
      }
    }
  }
}

static bool hasDistributionContract(DbAcquireOp acquire) {
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
  (void)blockArg;
  return edt && (getEdtDistributionKind(edt.getOperation()) ||
                 getEdtDistributionPattern(edt.getOperation()));
}

} // namespace

DbAcquirePartitionFacts DbDimAnalyzer::compute(DbAcquireNode *node) {
  DbAcquirePartitionFacts facts;
  if (!node)
    return facts;

  DbAcquireOp acquire = node->getDbAcquireOp();
  if (!acquire)
    return facts;

  facts.acquire = acquire;
  facts.requestedMode = getRequestedMode(acquire);
  facts.accessPattern = node->getAccessPattern();
  facts.hasIndirectAccess = node->hasIndirectAccess();
  facts.hasDirectAccess = node->hasDirectAccess();
  facts.hasDistributionContract = hasDistributionContract(acquire);
  facts.explicitCoarseRequest =
      facts.requestedMode == PartitionMode::coarse &&
      acquire.getPartitionIndices().empty() &&
      acquire.getPartitionOffsets().empty() &&
      acquire.getPartitionSizes().empty();
  facts.hasBlockHints = !acquire.getPartitionOffsets().empty() ||
                        !acquire.getPartitionSizes().empty();

  initializeDimFacts(node, facts);
  markLeadingDynamicDims(node, facts);

  SmallVector<PartitionInfo, 2> entries = collectPartitionEntries(acquire);
  facts.inferredBlock =
      llvm::any_of(entries, [](const PartitionInfo &entry) {
        return !entry.offsets.empty() && !entry.sizes.empty();
      });

  llvm::DenseSet<unsigned> fullRangeDims;
  for (const PartitionInfo &entry : entries) {
    DbPartitionEntryFact fact;
    fact.partition = entry;
    std::tie(fact.representativeOffset, fact.representativeSize) =
        pickRepresentativeRange(entry.mode, entry);

    if (fact.representativeOffset) {
      fact.mappedDim =
          node->getPartitionOffsetDim(fact.representativeOffset,
                                      /*requireLeading=*/false);
      fact.needsFullRange = PartitionBoundsAnalyzer::needsFullRange(
          node, fact.representativeOffset);
      fact.preservesDistributedContract =
          PartitionBoundsAnalyzer::shouldPreserveDistributedContract(
              node, fact.representativeOffset);
    }

    if (fact.mappedDim && *fact.mappedDim < facts.dims.size()) {
      facts.dims[*fact.mappedDim].selectedByPartitionEntry = true;
      if (fact.needsFullRange)
        fullRangeDims.insert(*fact.mappedDim);
    }

    facts.entries.push_back(std::move(fact));
  }

  for (unsigned dim : fullRangeDims) {
    if (dim < facts.dims.size())
      facts.dims[dim].needsFullRange = true;
  }

  return facts;
}
