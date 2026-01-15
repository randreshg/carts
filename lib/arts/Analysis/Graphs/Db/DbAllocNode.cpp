///==========================================================================///
/// File: DbAllocNode.cpp
///
/// Implementation of DbAlloc node
///==========================================================================///

#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAliasAnalysis.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinTypes.h"
#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::arts;

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_alloc_node);

///===----------------------------------------------------------------------===///
// DbAllocNode
///===----------------------------------------------------------------------===///
DbAllocNode::DbAllocNode(DbAllocOp op, DbAnalysis *analysis)
    : MemrefMetadata(op.getOperation()), dbAllocOp(op), dbFreeOp(nullptr),
      op(op.getOperation()), analysis(analysis) {
  assert(op && "DbAllocOp is required to create DbAllocNode");
  assert(op.getOperation() && "Operation is required to create DbAllocNode");

  analysis->getAnalysisManager()
      .getMetadataManager()
      .getIdRegistry()
      .getOrCreate(op.getOperation());

  /// Import metadata from operation attributes, falling back to manager
  bool hasMetadata = importFromOp();
  if (!hasMetadata) {
    ArtsMetadataManager &metadataManager =
        analysis->getAnalysisManager().getMetadataManager();
    if (metadataManager.ensureMemrefMetadata(op.getOperation()))
      importFromOp();
  }

  if (Value address = dbAllocOp.getAddress()) {
    auto &stringAnalysis = analysis->getAnalysisManager().getStringAnalysis();
    isStringBacked = stringAnalysis.isStringMemRef(address);
  }

  /// Find the corresponding DbFreeOp for this allocation
  Value dbVal = dbAllocOp.getPtr();
  for (Operation *user : dbVal.getUsers()) {
    if (auto freeOp = dyn_cast<DbFreeOp>(user)) {
      dbFreeOp = freeOp;
      break;
    }
  }
}

void DbAllocNode::print(llvm::raw_ostream &os) const {
  os << "DbAllocNode (" << getHierId() << ")";
  os << "\n";
}

void DbAllocNode::forEachChildNode(
    const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : acquireNodes)
    fn(n.get());
}

DbAcquireNode *DbAllocNode::getOrCreateAcquireNode(DbAcquireOp op) {
  auto it = acquireMap.find(op);
  if (it != acquireMap.end())
    return it->second;
  std::string childId =
      (getHierId() + "." + std::to_string(nextChildId++)).str();
  auto newNode =
      std::make_unique<DbAcquireNode>(op, this, this, analysis, childId);
  DbAcquireNode *ptr = newNode.get();
  acquireNodes.push_back(std::move(newNode));
  acquireMap[op] = ptr;
  return ptr;
}

bool DbAllocNode::isParallelFriendly() const {
  if (isStringDatablock())
    return false;

  if (accessedInParallelLoop && *accessedInParallelLoop)
    return true;

  /// Offset/size hints from ForLowering indicate parallel partition intent
  /// even when loop metadata is missing.
  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      continue;
    if (!acqOp.getOffsetHints().empty() || !acqOp.getSizeHints().empty())
      return true;
  }

  return false;
}

bool DbAllocNode::canBePartitioned(bool useFineGrainedFallback) {
  ARTS_DEBUG("canBePartitioned for alloc " << hierId);

  auto skip = [&](StringRef reason) {
    ARTS_DEBUG("  SKIP alloc " << hierId << ": " << reason);
    return false;
  };

  if (hasNonAffineAccesses && *hasNonAffineAccesses) {
    /// Check if any acquire has offset/size hints from ForLowering.
    /// If hints exist, trust ForLowering's parallel loop analysis over
    /// the static non-affine classification (e.g., indirect indexing A[B[i]]
    /// can still be chunkable when the parallel loop structure is valid).
    bool hasAcquireWithHints = false;
    for (const auto &acqNode : acquireNodes) {
      if (!acqNode)
        continue;
      DbAcquireOp acqOp = acqNode->getDbAcquireOp();
      if (acqOp &&
          (!acqOp.getOffsetHints().empty() || !acqOp.getSizeHints().empty())) {
        hasAcquireWithHints = true;
        ARTS_DEBUG("  Found acquire with hints - allowing non-affine alloc");
        break;
      }
    }
    if (!hasAcquireWithHints) {
      /// If fine-grained fallback is enabled, allow non-affine accesses through.
      /// H1.5 heuristic will choose element-wise partitioning for these.
      if (useFineGrainedFallback) {
        ARTS_DEBUG("  Non-affine access - using fine-grained fallback "
                   "(element-wise partitioning)");
      } else {
        return skip("memref has non-affine accesses (no hints to override)");
      }
    }
  }

  /// Step 1: Allocation-level metadata checks
  ARTS_DEBUG_REGION({
    ARTS_DBGS() << "  accessedInParallelLoop="
                << (accessedInParallelLoop
                        ? (*accessedInParallelLoop ? "true" : "false")
                        : "nullopt")
                << "\n";
    ARTS_DBGS() << "  hasLoopCarriedDeps="
                << (hasLoopCarriedDeps
                        ? (*hasLoopCarriedDeps ? "true" : "false")
                        : "nullopt")
                << "\n";
    ARTS_DBGS() << "  hasUniformAccess="
                << (hasUniformAccess ? (*hasUniformAccess ? "true" : "false")
                                     : "nullopt")
                << "\n";
    ARTS_DBGS() << "  dominantAccessPattern="
                << (dominantAccessPattern ? std::to_string(static_cast<int>(
                                                *dominantAccessPattern))
                                          : "nullopt")
                << "\n";
  });

  bool allocLevelOk = false;
  if (accessedInParallelLoop && *accessedInParallelLoop) {
    if (!hasLoopCarriedDeps || !*hasLoopCarriedDeps) {
      ARTS_DEBUG("  alloc-level OK (parallel loop, no loop-carried deps)");
      allocLevelOk = true;
    }
  }

  if (!allocLevelOk && hasUniformAccess && *hasUniformAccess) {
    if (dominantAccessPattern) {
      auto pattern = *dominantAccessPattern;
      if (pattern == AccessPatternType::Sequential ||
          pattern == AccessPatternType::Strided) {
        ARTS_DEBUG(
            "  alloc-level OK (uniform access, sequential/strided pattern)");
        allocLevelOk = true;
      }
    }
  }

  /// NOTE: We do NOT reject based on allocation-level metadata alone.
  /// Non-uniform access (stencil patterns like A[i-1], A[i], A[i+1]) can
  /// still be partitioned if the acquire-level analysis succeeds.
  /// The acquire-level canPartitionWithOffset() validates that indices
  /// are derived from the partition offset, and analyzeAccessBounds()
  /// correctly computes stencil halo adjustments.
  if (!allocLevelOk) {
    ARTS_DEBUG("  alloc-level metadata inconclusive - deferring to "
               "acquire-level validation");
  }

  /// Step 2: Check ALL acquire children and track stencil patterns
  /// For stencil patterns (uniform write + stencil read), we allow partitioning
  /// even if the uniform write acquire fails offset validation. H1.5 will use
  /// ESD mode which handles mixed access patterns correctly.
  if (acquireNodes.empty()) {
    ARTS_DEBUG("  No acquire nodes - trivially partitionable");
    return true;
  }

  ARTS_DEBUG("  Checking " << acquireNodes.size() << " acquire children...");
  bool anyPassed = false;
  bool anyHasStencilBounds = false;
  bool allFailed = true;

  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;

    bool passed = acqNode->canBePartitioned(useFineGrainedFallback);
    if (passed) {
      anyPassed = true;
      allFailed = false;
      /// Check if this acquire has stencil bounds (minOffset != maxOffset)
      if (auto bounds = acqNode->getStencilBounds()) {
        if (bounds->valid && bounds->isStencil) {
          anyHasStencilBounds = true;
          ARTS_DEBUG("  Acquire has stencil bounds: minOffset="
                     << bounds->minOffset
                     << ", maxOffset=" << bounds->maxOffset);
        }
      }
    } else {
      ARTS_DEBUG("  Acquire failed canBePartitioned()");
    }
  }

  /// Summarize access patterns (for logging)
  AcquirePatternSummary patterns =
      summarizeAcquirePatterns(useFineGrainedFallback);
  ARTS_DEBUG("  Access patterns: hasUniform="
             << patterns.hasUniform << ", hasStencil=" << patterns.hasStencil
             << ", hasIndexed=" << patterns.hasIndexed
             << ", isMixed=" << patterns.isMixed());

  /// Allow partitioning if ANY acquire passed with stencil bounds
  /// This handles mixed access patterns (uniform write + stencil read)
  if (anyHasStencilBounds) {
    ARTS_DEBUG(
        "  Stencil bounds detected - allowing partition for H1.5 ESD mode");
    return true;
  }

  /// If all acquires failed, reject the allocation
  if (allFailed) {
    return skip("all acquire children failed canBePartitioned()");
  }

  /// If at least one passed (but no stencil bounds), allow partitioning
  if (anyPassed) {
    ARTS_DEBUG("  At least one acquire passed validation");
  }

  ARTS_DEBUG("  PASS: alloc " << hierId << " can be partitioned");
  return true;
}

bool DbAllocNode::isFineGrained() const {
  if (!dbAllocOp)
    return false;

  DbAllocOp allocOp = const_cast<DbAllocOp &>(dbAllocOp);
  ValueRange elementSizes = allocOp.getElementSizes();
  if (elementSizes.empty())
    return false;

  /// Fine-grained: ALL elementSizes are constant 1
  /// This means each datablock holds exactly one element
  return llvm::all_of(elementSizes, [](Value v) {
    int64_t cst;
    return ValueUtils::getConstantIndex(v, cst) && cst == 1;
  });
}

///===----------------------------------------------------------------------===///
// Twin-diff overlap analysis methods
///===----------------------------------------------------------------------===///

bool DbAllocNode::hasSingleWriter() const {
  int writeCount = 0;

  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      continue;

    ArtsMode mode = acqOp.getMode();
    if (!DatablockUtils::isWriterMode(mode))
      continue;

    writeCount++;
    if (writeCount > 1)
      return false;

    /// A single DbAcquireOp can still represent many writers if it is
    /// partitioned by dynamic loop hints. Treat as multi-writer.
    if (!DatablockUtils::hasStaticHints(acqOp))
      return false;
  }

  if (writeCount <= 1) {
    ARTS_DEBUG("Single writer detected ("
               << writeCount << " writers) -> no overlap possible");
    return true;
  }

  return false;
}

bool DbAllocNode::hasDynamicWriterOffsets() const {
  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    DbAcquireOp acqOp = acqNode->getDbAcquireOp();
    if (!acqOp)
      continue;

    if (!DatablockUtils::isWriterMode(acqOp.getMode()))
      continue;

    if (!DatablockUtils::hasStaticHints(acqOp)) {
      ARTS_DEBUG("Writer acquire has dynamic offset/size hints");
      return true;
    }
  }

  return false;
}

bool DbAllocNode::allAcquiresWorkerIndexed() const {
  if (acquireNodes.empty())
    return true;

  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    if (!acqNode->isWorkerIndexedAccess())
      return false;
  }

  ARTS_DEBUG("All acquires use worker-indexed pattern -> disjoint");
  return true;
}

bool DbAllocNode::canProveNonOverlapping() const {
  /// No acquires = trivially non-overlapping
  if (acquireNodes.empty())
    return true;

  /// Single acquire is only safe when it does not represent a loop-partitioned
  /// writer (dynamic offset/size hints can imply multiple runtime instances).
  if (acquireNodes.size() == 1) {
    const auto *single = acquireNodes.front().get();
    if (!single)
      return true;
    DbAcquireOp acqOp = single->getDbAcquireOp();
    if (DatablockUtils::hasStaticHints(acqOp))
      return true;
  }

  ARTS_DEBUG("Analyzing overlap for " << acquireNodes.size() << " acquires");

  /// Method 1: Single writer = no overlap possible
  /// If only one EDT writes, there's no possibility of write conflicts
  if (hasSingleWriter())
    return true;

  /// Dynamic writer offsets can represent multiple runtime writers.
  if (hasDynamicWriterOffsets()) {
    ARTS_DEBUG("Dynamic writer offsets -> potential overlap");
    return false;
  }

  /// If the allocation is not actually partitioned, disjoint offset hints are
  /// insufficient to prove non-overlap because each EDT still acquires the full
  /// DB copy (remote updates would clobber each other). Require twin-diff.
  bool isPartitioned = false;
  if (dbAllocOp) {
    auto &mutableAlloc = const_cast<DbAllocOp &>(dbAllocOp);
    if (auto mode = getPartitionMode(mutableAlloc.getOperation()))
      isPartitioned = (*mode != PromotionMode::coarse);
  }
  if (!isPartitioned) {
    ARTS_DEBUG(
        "Unpartitioned alloc with multiple writers -> potential overlap");
    return false;
  }

  /// Method 2: All worker-indexed = disjoint
  /// If all acquires use the pattern array[workerId] with size=1,
  /// they access disjoint slots by construction
  if (allAcquiresWorkerIndexed())
    return true;

  /// Get alias analysis from DbAnalysis (may be null during export)
  DbAliasAnalysis *aliasAnalysis =
      analysis ? analysis->getAliasAnalysis() : nullptr;

  /// Check all acquire pairs using enhanced analysis
  for (size_t i = 0; i < acquireNodes.size(); ++i) {
    for (size_t j = i + 1; j < acquireNodes.size(); ++j) {
      const DbAcquireNode *acqA = acquireNodes[i].get();
      const DbAcquireNode *acqB = acquireNodes[j].get();

      if (!acqA || !acqB)
        continue;

      /// Method 3: Use DbAliasAnalysis overlap estimation (if available)
      if (aliasAnalysis) {
        auto overlap =
            aliasAnalysis->estimateOverlap(const_cast<DbAcquireNode *>(acqA),
                                           const_cast<DbAcquireNode *>(acqB));
        if (overlap == DbAliasAnalysis::OverlapKind::Disjoint) {
          /// Proven disjoint by alias analysis
          ARTS_DEBUG("Pair (" << i << "," << j
                              << "): Disjoint (aliasAnalysis)");
          continue;
        }
      }

      /// Method 4: Check offset_hints/size_hints from ForLowering
      if (acqA->hasDisjointPartitionWith(acqB)) {
        /// Proven disjoint by loop partitioning
        ARTS_DEBUG("Pair (" << i << "," << j
                            << "): Disjoint (partition hints)");
        continue;
      }

      /// Could not prove disjoint - potential overlap
      ARTS_DEBUG("Pair (" << i << "," << j << "): POTENTIAL OVERLAP");
      return false;
    }
  }

  ARTS_DEBUG("All acquire pairs proven disjoint");
  return true; /// All pairs proven disjoint
}

AcquirePatternSummary
DbAllocNode::summarizeAcquirePatterns(bool useFineGrainedFallback) const {
  AcquirePatternSummary summary;

  for (const auto &acqNode : acquireNodes) {
    if (!acqNode)
      continue;
    auto pattern = acqNode->getAccessPattern();
    switch (pattern) {
    case AccessPattern::Uniform:
      summary.hasUniform = true;
      break;
    case AccessPattern::Stencil:
      summary.hasStencil = true;
      break;
    case AccessPattern::Indexed:
      summary.hasIndexed = true;
      break;
    default:
      break;
    }
  }

  /// Fine-grained fallback: treat non-affine or indirect indices as indexed
  /// so H1.5 can select element-wise partitioning.
  if (useFineGrainedFallback) {
    if (hasNonAffineAccesses && *hasNonAffineAccesses) {
      summary.hasIndexed = true;
      ARTS_DEBUG(
          "summarizeAcquirePatterns: non-affine with fine-grained fallback "
          "-> marking hasIndexed=true for element-wise partitioning");
    } else if (!summary.hasIndexed) {
      for (const auto &acqNode : acquireNodes) {
        if (acqNode && acqNode->hasIndirectAccess()) {
          summary.hasIndexed = true;
          ARTS_DEBUG(
              "summarizeAcquirePatterns: indirect index with fine-grained "
              "fallback -> marking hasIndexed=true");
          break;
        }
      }
    }
  }

  return summary;
}

bool DbAllocNode::hasStencilAccess() const {
  return summarizeAcquirePatterns().hasStencil;
}

bool DbAllocNode::hasMixedAccessPatterns() const {
  return summarizeAcquirePatterns().isMixed();
}
