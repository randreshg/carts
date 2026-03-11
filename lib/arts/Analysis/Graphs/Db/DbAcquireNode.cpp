///==========================================================================///
/// File: DbAcquireNode.cpp
///
/// Implementation of DbAcquire node -- slim facade that delegates to:
///   - PartitionBoundsAnalyzer (partition bound computation)
///   - MemoryAccessClassifier  (load/store/indirect/direct detection)
///   - BlockInfoComputer       (block offset/size computation)
///==========================================================================///

#include "arts/Analysis/AccessPatternAnalysis.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Db/DbAnalysis.h"
#include "arts/Analysis/Edt/EdtAnalysis.h"
#include "arts/Analysis/Graphs/Db/BlockInfoComputer.h"
#include "arts/Analysis/Graphs/Db/DbGraph.h"
#include "arts/Analysis/Graphs/Db/DbNode.h"
#include "arts/Analysis/Graphs/Db/MemoryAccessClassifier.h"
#include "arts/Analysis/Graphs/Db/PartitionBoundsAnalyzer.h"
#include "arts/Analysis/Graphs/Edt/EdtGraph.h"
#include "arts/Analysis/Graphs/Edt/EdtNode.h"
#include "arts/Analysis/Loop/LoopAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DbUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/OperationAttributes.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>

using namespace mlir;
using namespace mlir::arts;
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_acquire_node);

///===----------------------------------------------------------------------===///
/// DbAcquireNode
///===----------------------------------------------------------------------===///
DbAcquireNode::DbAcquireNode(DbAcquireOp op, NodeBase *parent,
                             DbAllocNode *rootAlloc, DbAnalysis *analysis,
                             std::string initialHierId)
    : dbAcquireOp(op), dbReleaseOp(nullptr), op(op.getOperation()),
      parent(parent), rootAlloc(rootAlloc), analysis(analysis) {
  if (!initialHierId.empty())
    hierId = std::move(initialHierId);

  /// Verify operation is valid
  Operation *opPtr = op.getOperation();
  if (!opPtr) {
    ARTS_ERROR("Cannot create DbAcquireNode: operation pointer is null");
    return;
  }

  /// Verify rootAlloc is valid
  if (!rootAlloc) {
    ARTS_ERROR("Cannot create DbAcquireNode: rootAlloc is null");
    return;
  }

  /// Estimate the footprint of this acquire from constant slice information.
  bool hasUnknown = false;
  unsigned long long totalElems = 1;
  for (Value v : dbAcquireOp.getSizes()) {
    int64_t cst = 0;
    if (!ValueUtils::getConstantIndex(v, cst)) {
      hasUnknown = true;
      break;
    }

    if (cst <= 0)
      continue;
    if (totalElems > std::numeric_limits<unsigned long long>::max() /
                         (unsigned long long)cst) {
      estimatedBytes = std::numeric_limits<unsigned long long>::max();
      hasUnknown = true;
      break;
    }
    totalElems *= (unsigned long long)cst;
  }

  if (!hasUnknown) {
    DbAllocOp rootAllocOp = rootAlloc->getDbAllocOp();
    if (Type elementType = rootAllocOp.getElementType()) {
      unsigned long long elemBytes = arts::getElementTypeByteSize(elementType);
      if (elemBytes != 0) {
        if (totalElems >
            std::numeric_limits<unsigned long long>::max() / elemBytes) {
          estimatedBytes = std::numeric_limits<unsigned long long>::max();
        } else {
          estimatedBytes = totalElems * elemBytes;
        }
      }
    }
  }

  /// Identify the EDT that consumes this acquire and the block argument used
  Value ptrResult = dbAcquireOp.getPtr();
  if (!ptrResult) {
    ARTS_ERROR("DbAcquireOp ptr result is null");
    return;
  }

  /// The ptr result should be passed directly to an EDT or another acquire
  auto users = ptrResult.getUsers();
  Operation *singleUser = users.empty() ? nullptr : *users.begin();
  if (!singleUser && !users.empty()) {
    ARTS_ERROR("DbAcquireOp ptr user is null");
    return;
  }

  /// Capture partition hints for analysis.
  /// Fine-grained mode uses partition indices as element coordinates.
  /// Block/stencil modes use partition offsets as range starts.
  auto pickRepresentative = [](ValueRange vals, unsigned &idx) -> Value {
    idx = 0;
    for (unsigned i = 0; i < vals.size(); ++i) {
      Value v = vals[i];
      if (!v)
        continue;
      int64_t c = 0;
      if (!ValueUtils::getConstantIndex(ValueUtils::stripNumericCasts(v), c)) {
        idx = i;
        return v;
      }
    }
    for (unsigned i = 0; i < vals.size(); ++i) {
      if (vals[i]) {
        idx = i;
        return vals[i];
      }
    }
    return Value();
  };

  unsigned hintIdx = 0;
  auto hintMode = dbAcquireOp.getPartitionMode();
  bool preferPartitionIndices =
      !hintMode || *hintMode == PartitionMode::fine_grained;
  if (preferPartitionIndices && !dbAcquireOp.getPartitionIndices().empty())
    partitionOffset =
        pickRepresentative(dbAcquireOp.getPartitionIndices(), hintIdx);
  else if (!dbAcquireOp.getPartitionOffsets().empty())
    partitionOffset =
        pickRepresentative(dbAcquireOp.getPartitionOffsets(), hintIdx);
  else if (!dbAcquireOp.getPartitionIndices().empty())
    partitionOffset =
        pickRepresentative(dbAcquireOp.getPartitionIndices(), hintIdx);
  if (!dbAcquireOp.getPartitionSizes().empty()) {
    if (hintIdx < dbAcquireOp.getPartitionSizes().size())
      partitionSize = dbAcquireOp.getPartitionSizes()[hintIdx];
    else
      partitionSize = dbAcquireOp.getPartitionSizes().front();
  }

  /// Fallback to DB-space indices/offsets/sizes only when an explicit
  /// non-coarse partition mode is set (legacy paths or already-partitioned
  /// acquires).
  if (!partitionOffset && !partitionSize) {
    if (auto mode = dbAcquireOp.getPartitionMode();
        mode &&
        (*mode == PartitionMode::block || *mode == PartitionMode::stencil ||
         *mode == PartitionMode::fine_grained)) {
      unsigned dbIdx = 0;
      bool preferDbIndices = (*mode == PartitionMode::fine_grained);
      if (preferDbIndices && !dbAcquireOp.getIndices().empty())
        partitionOffset = pickRepresentative(dbAcquireOp.getIndices(), dbIdx);
      else if (!dbAcquireOp.getOffsets().empty())
        partitionOffset = pickRepresentative(dbAcquireOp.getOffsets(), dbIdx);
      else if (!dbAcquireOp.getIndices().empty())
        partitionOffset = pickRepresentative(dbAcquireOp.getIndices(), dbIdx);
      if (!dbAcquireOp.getSizes().empty()) {
        if (dbIdx < dbAcquireOp.getSizes().size())
          partitionSize = dbAcquireOp.getSizes()[dbIdx];
        else
          partitionSize = dbAcquireOp.getSizes().front();
      }
    }
  }

  if (singleUser && isa<DbAcquireOp>(singleUser)) {
    /// Nested acquire; create child node lazily via getOrCreateAcquireNode
    /// The rest of this constructor computes info for this node; nested
    /// children will compute their own. For nested acquires, the EDT user
    /// is found when processing the child acquire node.
    return;
  }

  /// Use utility function to get EDT and block argument
  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(dbAcquireOp);
  edtUserOp = edt.getOperation();
  useInEdt = blockArg;

  if (!edtUserOp || !useInEdt) {
    /// Host-side acquire/release chains are valid in pre-lowering IR
    /// (e.g. serial initialization), but they are not partitionable task
    /// acquires. Keep the node in the graph and treat it as non-EDT.
    ARTS_DEBUG("DbAcquireOp ptr is not consumed by an EDT; treating as "
               "non-partitionable acquire.\n"
               "  Acquire op: "
               << dbAcquireOp
               << "\n"
                  "  Single user: "
               << (singleUser ? *singleUser : *dbAcquireOp.getOperation())
               << "\n"
                  "  User type: "
               << (singleUser ? singleUser->getName().getStringRef()
                              : StringRef("<none>")));
    return;
  }

  /// Find the corresponding DbReleaseOp for this acquire
  for (Operation *user : useInEdt.getUsers()) {
    auto releaseOp = dyn_cast<DbReleaseOp>(user);
    if (!releaseOp)
      continue;
    if (releaseOp.getSource() == useInEdt) {
      dbReleaseOp = releaseOp;
      break;
    }
  }

  /// Get the in/out mode based on the memory accesses
  bool hasLoadAccesses = hasLoads();
  bool hasStoreAccesses = hasStores();
  if (hasLoadAccesses && !hasStoreAccesses) {
    inCount = 1;
    outCount = 0;
  } else if (hasStoreAccesses && !hasLoadAccesses) {
    inCount = 0;
    outCount = 1;
  } else {
    inCount = 1;
    outCount = 1;
  }
}

DbAcquireNode *DbAcquireNode::getOrCreateAcquireNode(DbAcquireOp op) {
  auto it = childMap.find(op);
  if (it != childMap.end())
    return it->second;
  std::string childId = getHierId().str() + "." + std::to_string(nextChildId++);
  auto newNode =
      std::make_unique<DbAcquireNode>(op, this, rootAlloc, analysis, childId);
  DbAcquireNode *ptr = newNode.get();
  childAcquires.push_back(std::move(newNode));
  childMap[op] = ptr;
  return ptr;
}

void DbAcquireNode::forEachChildNode(
    const std::function<void(NodeBase *)> &fn) const {
  for (const auto &n : childAcquires)
    fn(n.get());
}

///===----------------------------------------------------------------------===///
/// Memory Access Methods -- delegate to MemoryAccessClassifier
///===----------------------------------------------------------------------===///

void DbAcquireNode::collectAccessOperations(
    DenseMap<DbRefOp, SetVector<Operation *>> &dbRefToMemOps) {
  MemoryAccessClassifier::collectAccessOperations(this, dbRefToMemOps);
}

void DbAcquireNode::forEachMemoryAccess(
    llvm::function_ref<void(Operation *, bool)> callback) {
  MemoryAccessClassifier::forEachMemoryAccess(this, callback);
}

bool DbAcquireNode::hasLoads() {
  return MemoryAccessClassifier::hasLoads(this);
}

bool DbAcquireNode::hasStores() {
  return MemoryAccessClassifier::hasStores(this);
}

bool DbAcquireNode::isReadOnlyAccess() {
  return MemoryAccessClassifier::isReadOnlyAccess(this);
}

bool DbAcquireNode::isWriterAccess() {
  return MemoryAccessClassifier::isWriterAccess(this);
}

bool DbAcquireNode::hasMemoryAccesses() {
  return MemoryAccessClassifier::hasMemoryAccesses(this);
}

size_t DbAcquireNode::countLoads() {
  return MemoryAccessClassifier::countLoads(this);
}

size_t DbAcquireNode::countStores() {
  return MemoryAccessClassifier::countStores(this);
}

bool DbAcquireNode::hasIndirectAccess() const {
  return MemoryAccessClassifier::hasIndirectAccess(
      const_cast<DbAcquireNode *>(this));
}

bool DbAcquireNode::hasDirectAccess() const {
  return MemoryAccessClassifier::hasDirectAccess(
      const_cast<DbAcquireNode *>(this));
}

///===----------------------------------------------------------------------===///
/// Partition Bounds Methods -- delegate to PartitionBoundsAnalyzer
///===----------------------------------------------------------------------===///

bool DbAcquireNode::hasValidEdtAndAccesses() {
  return PartitionBoundsAnalyzer::hasValidEdtAndAccesses(this);
}

bool DbAcquireNode::computePartitionBounds() {
  return PartitionBoundsAnalyzer::computePartitionBounds(this);
}

bool DbAcquireNode::canBePartitioned() {
  return PartitionBoundsAnalyzer::canBePartitioned(this);
}

bool DbAcquireNode::canPartitionWithOffset(Value offset) {
  return PartitionBoundsAnalyzer::canPartitionWithOffset(this, offset);
}

std::optional<unsigned>
DbAcquireNode::getPartitionOffsetDim(Value offset, bool requireLeading) {
  return PartitionBoundsAnalyzer::getPartitionOffsetDim(this, offset,
                                                        requireLeading);
}

bool DbAcquireNode::needsFullRange(Value partOffset) {
  return PartitionBoundsAnalyzer::needsFullRange(this, partOffset);
}

///===----------------------------------------------------------------------===///
/// Block Info Methods -- delegate to BlockInfoComputer
///===----------------------------------------------------------------------===///

LogicalResult DbAcquireNode::computeBlockInfo(Value &blockOffset,
                                              Value &blockSize) {
  return BlockInfoComputer::computeBlockInfo(this, blockOffset, blockSize);
}

LogicalResult DbAcquireNode::computeBlockInfoFromWhile(scf::WhileOp whileOp,
                                                       Value &blockOffset,
                                                       Value &blockSize,
                                                       Value *offsetForCheck) {
  return BlockInfoComputer::computeBlockInfoFromWhile(
      this, whileOp, blockOffset, blockSize, offsetForCheck);
}

LogicalResult DbAcquireNode::computeBlockInfoFromHints(Value &blockOffset,
                                                       Value &blockSize) {
  return BlockInfoComputer::computeBlockInfoFromHints(this, blockOffset,
                                                      blockSize);
}

LogicalResult
DbAcquireNode::computeBlockInfoFromForLoop(ArrayRef<LoopNode *> loopNodes,
                                           Value &blockOffset, Value &blockSize,
                                           Value *offsetForCheck) {
  return BlockInfoComputer::computeBlockInfoFromForLoop(
      this, loopNodes, blockOffset, blockSize, offsetForCheck);
}

///===----------------------------------------------------------------------===///
/// Access Pattern Classification
///===----------------------------------------------------------------------===///

AccessPattern DbAcquireNode::getAccessPattern() const {
  if (accessPattern)
    return *accessPattern;

  DbAcquireOp acqOp = const_cast<DbAcquireOp &>(dbAcquireOp);

  ARTS_DEBUG("getAccessPattern() for: " << acqOp);
  ARTS_DEBUG(
      "  hasMultiplePartitionEntries: " << acqOp.hasMultiplePartitionEntries());
  ARTS_DEBUG(
      "  hasAllFineGrainedEntries: " << acqOp.hasAllFineGrainedEntries());
  ARTS_DEBUG(
      "  partitionIndices.empty: " << acqOp.getPartitionIndices().empty());
  if (acqOp.hasMultiplePartitionEntries()) {
    ARTS_DEBUG("  numPartitionEntries: " << acqOp.getNumPartitionEntries());
    for (size_t i = 0; i < acqOp.getNumPartitionEntries(); ++i) {
      ARTS_DEBUG("    entry["
                 << i << "] mode: "
                 << static_cast<int>(acqOp.getPartitionEntryMode(i)));
    }
  }

  /// Check multi-entry partition_entry_modes[] for task dependencies.
  /// If all entries are fine_grained with partition indices, return Indexed.
  if (acqOp.hasAllFineGrainedEntries() &&
      !acqOp.getPartitionIndices().empty()) {
    ARTS_DEBUG("  -> Returning Indexed (all fine-grained with indices)");
    accessPattern = AccessPattern::Indexed;
    return *accessPattern;
  }

  /// Check for explicit fine-grained (element-wise) partition hints FIRST.
  /// These come from OMP task deps and should NOT trigger stencil/ESD mode.
  /// Criteria: partition_indices non-empty AND mode is fine_grained (not
  /// block). Hints from for_lowering might have indices but suggest block
  /// partitioning, while hints from OMP task deps are marked as element-wise
  /// (fine_grained).
  if (!acqOp.getPartitionIndices().empty()) {
    /// Check if this is truly fine-grained (from task deps) vs block hints
    auto mode = acqOp.getPartitionMode();
    ARTS_DEBUG("  partitionMode: " << (mode ? static_cast<int>(*mode) : -1));
    if (!mode || *mode == PartitionMode::fine_grained) {
      ARTS_DEBUG("  -> Returning Indexed (has indices, mode is fine_grained or "
                 "unset)");
      accessPattern = AccessPattern::Indexed;
      return *accessPattern;
    }
    /// If mode is block/stencil, fall through to other checks
    ARTS_DEBUG("  mode is block/stencil, falling through...");
  }

  /// Then check stencilBounds (only applies when no explicit fine-grained
  /// hints)
  if (stencilBounds && stencilBounds->valid && stencilBounds->isStencil) {
    ARTS_DEBUG("  -> Returning Stencil (stencilBounds valid and isStencil)");
    accessPattern = AccessPattern::Stencil;
    return *accessPattern;
  }

  /// Check multi-entry stencil pattern (only for acquires without partition
  /// indices). Only treat as stencil if NOT all entries are fine-grained (task
  /// deps).
  if (acqOp.hasMultiplePartitionEntries() &&
      !acqOp.hasAllFineGrainedEntries()) {
    int64_t minOffset = 0, maxOffset = 0;
    if (DbUtils::hasMultiEntryStencilPattern(acqOp, minOffset, maxOffset)) {
      ARTS_DEBUG("  -> Returning Stencil (multi-entry stencil pattern, not all "
                 "fine-grained)");
      accessPattern = AccessPattern::Stencil;
      return *accessPattern;
    }
  }

  ARTS_DEBUG("  [COARSE-CHECK] stencilBounds="
             << (stencilBounds ? "SET" : "null")
             << " edtUserOp=" << (edtUserOp ? "SET" : "null") << " partMode="
             << (acqOp.getPartitionMode()
                     ? static_cast<int>(*acqOp.getPartitionMode())
                     : -1)
             << " partIndicesEmpty=" << acqOp.getPartitionIndices().empty());
  bool isFineGrainedHint =
      acqOp.getPartitionMode() &&
      *acqOp.getPartitionMode() == PartitionMode::fine_grained;
  if (!stencilBounds && edtUserOp && acqOp.getMode() == ArtsMode::in &&
      !isFineGrainedHint && acqOp.getPartitionIndices().empty()) {
    DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
    const_cast<DbAcquireNode *>(this)->collectAccessOperations(dbRefToMemOps);

    bool stencilFound = false;
    EdtOp edtUser = const_cast<DbAcquireNode *>(this)->getEdtUser();
    if (edtUser) {
      edtUser.walk([&](ForOp forOp) {
        if (stencilFound || forOp.getLowerBound().empty())
          return;
        Value loopIV = forOp.getBody()->getArgument(0);

        DenseMap<Value, std::pair<int64_t, int64_t>> baseOffsetRange;

        for (auto &[dbRef, memOps] : dbRefToMemOps) {
          if (stencilFound)
            break;
          for (Operation *memOp : memOps) {
            if (stencilFound)
              break;
            SmallVector<Value> chain =
                DbUtils::collectFullIndexChain(dbRef, memOp);
            if (chain.empty())
              continue;

            Value idxForBounds;
            for (Value idx : chain) {
              int64_t constVal = 0;
              if (!ValueUtils::getConstantIndex(idx, constVal)) {
                idxForBounds = idx;
                break;
              }
            }
            if (!idxForBounds)
              continue;

            int64_t constOffset = 0;
            Value base =
                ValueUtils::stripConstantOffset(idxForBounds, &constOffset);
            if (!base)
              continue;
            base = ValueUtils::stripNumericCasts(base);
            if (!ValueUtils::dependsOn(base, loopIV))
              continue;

            auto it = baseOffsetRange.find(base);
            if (it == baseOffsetRange.end()) {
              baseOffsetRange.try_emplace(base, constOffset, constOffset);
            } else {
              it->second.first = std::min(it->second.first, constOffset);
              it->second.second = std::max(it->second.second, constOffset);
              if (it->second.first != it->second.second) {
                ARTS_DEBUG("  Coarse stencil detected: base="
                           << base << " min=" << it->second.first
                           << " max=" << it->second.second);
                stencilFound = true;
              }
            }
          }
        }
      });
    }

    if (stencilFound) {
      ARTS_DEBUG("  -> Returning Stencil (coarse acquire IR walk)");
      accessPattern = AccessPattern::Stencil;
      return *accessPattern;
    }
  }

  ARTS_DEBUG("  -> Returning Uniform (fallback)");
  accessPattern = AccessPattern::Uniform;
  return *accessPattern;
}

///===----------------------------------------------------------------------===///
/// Remaining methods (print, worker-indexed, disjoint, validation)
///===----------------------------------------------------------------------===///

void DbAcquireNode::print(llvm::raw_ostream &os) const {
  os << "DbAcquireNode (" << getHierId() << ")";
  os << " estimatedBytes=" << estimatedBytes;
  os << "\n";
}

bool DbAcquireNode::isWorkerIndexedAccess() const {
  if (!op)
    return false;

  DbAcquireOp mutableOp(op);

  Value partitionIdx =
      !mutableOp.getIndices().empty()
          ? mutableOp.getIndices().front()
          : (!mutableOp.getOffsets().empty() ? mutableOp.getOffsets().front()
                                             : nullptr);
  Value partitionSz =
      !mutableOp.getSizes().empty() ? mutableOp.getSizes().front() : nullptr;

  if (!partitionIdx || !partitionSz)
    return false;

  int64_t sz = 0;
  if (!ValueUtils::getConstantIndex(partitionSz, sz) || sz != 1)
    return false;

  return isa<BlockArgument>(partitionIdx);
}

bool DbAcquireNode::hasDisjointPartitionWith(const DbAcquireNode *other) const {
  if (!other)
    return false;

  DbAcquireOp acqA = dbAcquireOp;
  DbAcquireOp acqB = other->getDbAcquireOp();
  if (!acqA || !acqB)
    return false;

  auto getPartitionIdx = [](DbAcquireOp acq) -> Value {
    if (!acq.getIndices().empty())
      return acq.getIndices().front();
    if (!acq.getOffsets().empty())
      return acq.getOffsets().front();
    return nullptr;
  };
  auto getPartitionSize = [](DbAcquireOp acq) -> Value {
    if (!acq.getSizes().empty())
      return acq.getSizes().front();
    return nullptr;
  };

  Value partIdxA = getPartitionIdx(acqA);
  Value partSizeA = getPartitionSize(acqA);
  Value partIdxB = getPartitionIdx(acqB);
  Value partSizeB = getPartitionSize(acqB);

  if (!partIdxA || !partSizeA || !partIdxB || !partSizeB)
    return false;

  int64_t idxAVal = 0, sizeAVal = 0, idxBVal = 0, sizeBVal = 0;
  if (ValueUtils::getConstantIndex(partIdxA, idxAVal) &&
      ValueUtils::getConstantIndex(partSizeA, sizeAVal) &&
      ValueUtils::getConstantIndex(partIdxB, idxBVal) &&
      ValueUtils::getConstantIndex(partSizeB, sizeBVal)) {
    if (sizeAVal == sizeBVal && idxAVal != idxBVal)
      return true;
    int64_t startA = idxAVal;
    int64_t endA = startA + sizeAVal;
    int64_t startB = idxBVal;
    int64_t endB = startB + sizeBVal;
    if (endA <= startB || endB <= startA)
      return true;
  }

  int64_t sizeAConst = 0, sizeBConst = 0;
  bool sizeAIsOne =
      ValueUtils::getConstantIndex(partSizeA, sizeAConst) && sizeAConst == 1;
  bool sizeBIsOne =
      ValueUtils::getConstantIndex(partSizeB, sizeBConst) && sizeBConst == 1;

  if (sizeAIsOne && sizeBIsOne) {
    auto blockArgA = dyn_cast<BlockArgument>(partIdxA);
    auto blockArgB = dyn_cast<BlockArgument>(partIdxB);
    if (blockArgA && blockArgB && blockArgA != blockArgB &&
        blockArgA.getOwner() != blockArgB.getOwner())
      return true;
  }

  return false;
}

bool DbAcquireNode::accessIndexDependsOn(Value idx) {
  if (!idx)
    return false;

  DenseMap<DbRefOp, SetVector<Operation *>> dbRefToMemOps;
  collectAccessOperations(dbRefToMemOps);

  for (auto &[dbRef, memOps] : dbRefToMemOps) {
    for (Operation *memOp : memOps) {
      SmallVector<Value> fullChain =
          DbUtils::collectFullIndexChain(dbRef, memOp);
      for (Value chainIdx : fullChain) {
        if (ValueUtils::dependsOn(chainIdx, idx))
          return true;
      }
    }
  }

  return false;
}

bool DbAcquireNode::validateElementWisePartitioning() {
  auto partitionIndices = dbAcquireOp.getPartitionIndices();
  if (partitionIndices.empty())
    return true;

  /// Check 1: Do partition indices come from a loop with step > 1?
  /// If so, this is a block-wise pattern (e.g., for k_block = 0 to N step 16)
  LoopAnalysis *loopAnalysis = analysis ? analysis->getLoopAnalysis() : nullptr;
  for (Value idx : partitionIndices) {
    if (!loopAnalysis)
      break;
    if (LoopNode *drivingLoop = loopAnalysis->findEnclosingLoopDrivenBy(
            dbAcquireOp.getOperation(), idx)) {
      auto step = drivingLoop->getStepConstant();
      if (step && *step > 1) {
        ARTS_DEBUG("  Block-wise pattern detected: loop step = "
                   << *step << " for index " << idx);
        return false;
      }
    }
  }

  /// Check 2: Do the partition indices appear in EDT body accesses?
  for (Value idx : partitionIndices) {
    if (!accessIndexDependsOn(idx)) {
      ARTS_DEBUG("  Partition index " << idx << " not found in access chain");
      return false;
    }
  }

  return true;
}
