///==========================================================================///
/// File: CreateDbs.cpp
///
/// This pass creates ARTS Dbs (DB) operations to handle memory
/// dependencies for EDTs (Event-Driven Tasks). Each EDT may execute in a
/// separate memory environment, so external memory references must be properly
/// managed.
///
/// Pass Overview:
/// 1. Identify external memory allocations used by EDTs
/// 2. Create arts.db_alloc operations for each allocation
/// 3. Analyze memory access patterns within each EDT
/// 4. Insert arts.db_acquire operations before EDTs with computed dependencies
/// 5. Update load/store operations to use acquired memory views
/// 6. Insert arts.db_release operations before EDT terminators
/// 7. Insert arts.db_free operations for db_alloc operations
///
/// Example:
///   Before:
///     memref.load/store inside arts.edt body on captured buffers
///
///   After:
///     arts.db_alloc + arts.db_acquire/arts.db_release around EDT body access
///     with loads/stores redirected through acquired DB views
///
/// Allocation Strategy:
/// This pass analyzes memory access patterns from task dependencies
/// (DbControlOps) to determine the optimal datablock allocation granularity:
///
/// 1. Fine-grained allocation (preferred when possible):
///    - Detects consistent single-dimension indexing patterns (e.g., A[i])
///    - Verifies ALL db_acquires have indices for fine-grained access
///    - Creates multiple datablocks: sizes=[N], elementSizes=[remaining dims]
///
/// 2. Coarse-grained allocation (fallback):
///    - Used when no task dependencies are provided (no DbControlOps)
///    - Used when access patterns are inconsistent across EDTs
///    - Used when some db_acquires lack indices (full array access needed)
///
/// Access Mode Strategy:
/// Access modes are extracted from DbControlOps (from OpenMP depend clauses).
/// If no control DB is found, conservatively defaults to inout mode.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/DbAnalysis.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/passes/PassDetails.h"
#include "arts/transforms/db/DbRewriter.h"
#include "arts/transforms/db/DbTransforms.h"
#include "arts/transforms/db/block/DbBlockIndexer.h"
#include "arts/transforms/db/elementwise/DbElementWiseIndexer.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/RemovalUtils.h"
#include "arts/utils/Utils.h"
#include "arts/utils/metadata/MemrefMetadata.h"
#include "arts/utils/metadata/Metadata.h"
#include <optional>

#include "arts/passes/Passes.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <memory>

/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Helper Functions
///===----------------------------------------------------------------------===///
namespace {

static ArtsMode getMemrefUserAccessMode(Operation *op,
                                        Operation *underlyingOp) {
  if (!op || !underlyingOp)
    return ArtsMode::uninitialized;

  bool hasRead = false;
  bool hasWrite = false;

  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    hasRead =
        ValueAnalysis::getUnderlyingOperation(load.getMemref()) == underlyingOp;
  } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
    hasWrite = ValueAnalysis::getUnderlyingOperation(store.getMemref()) ==
               underlyingOp;
  } else if (auto load = dyn_cast<affine::AffineLoadOp>(op)) {
    hasRead =
        ValueAnalysis::getUnderlyingOperation(load.getMemRef()) == underlyingOp;
  } else if (auto store = dyn_cast<affine::AffineStoreOp>(op)) {
    hasWrite = ValueAnalysis::getUnderlyingOperation(store.getMemRef()) ==
               underlyingOp;
  } else if (auto copy = dyn_cast<memref::CopyOp>(op)) {
    hasRead =
        ValueAnalysis::getUnderlyingOperation(copy.getSource()) == underlyingOp;
    hasWrite =
        ValueAnalysis::getUnderlyingOperation(copy.getTarget()) == underlyingOp;
  }

  if (hasRead && hasWrite)
    return ArtsMode::inout;
  if (hasWrite)
    return ArtsMode::out;
  if (hasRead)
    return ArtsMode::in;
  return ArtsMode::uninitialized;
}

static bool opMatchesAccessMode(Operation *op, Operation *underlyingOp,
                                ArtsMode requestedMode) {
  ArtsMode actualMode = getMemrefUserAccessMode(op, underlyingOp);
  if (actualMode == ArtsMode::uninitialized)
    return false;
  if (requestedMode == ArtsMode::inout)
    return true;
  return actualMode == requestedMode;
}

static bool accessModeCanSeedNestedAcquire(ArtsMode availableMode,
                                           ArtsMode requestedMode) {
  if (requestedMode == ArtsMode::uninitialized)
    return true;
  if (availableMode == ArtsMode::inout)
    return true;
  return availableMode == requestedMode;
}

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///

struct CreateDbsPass : public arts::CreateDbsBase<CreateDbsPass> {
  CreateDbsPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  ModuleOp module;
  mlir::arts::AnalysisManager *AM = nullptr;
  ArtsCodegen *AC = nullptr;
  DenseMap<Operation *, Operation *> dbPtrToOriginalAlloc;
  SetVector<Operation *> opsToRemove;

  /// Structure to track all usage information for a memref
  struct MemrefInfo {
    Operation *alloc = nullptr;
    EdtOp parentEdt = nullptr;
    /// True if this allocation is used by an EDT other than its defining EDT.
    bool usedByOtherEdts = false;
    ArtsMode accessMode = ArtsMode::uninitialized;
    bool usedFineGrained = false;
    bool usedBlock = false;
    SmallVector<Value> blockSizes;
    DbAllocOp dbAllocOp = nullptr;

    /// The computed allocation strategy plan
    std::optional<DbRewritePlan> rewritePlan;

    /// Structure to track a specific DB access (mode + indices + chunks)
    struct DbDep {
      ArtsMode mode;
      SmallVector<Value, 4> indices, offsets, sizes;
      SmallVector<Operation *, 4> operations;
      bool preserveDepEdge = true;
    };

    /// Map from EDT operation to all DB accesses for that EDT
    DenseMap<EdtOp, SmallVector<DbDep>> edtToDeps;

    /// Add an access for this memref and update combined mode
    void addAccess(EdtOp edtOp, ArtsMode mode, SmallVector<Value, 4> indices,
                   SmallVector<Value, 4> offsets = {},
                   SmallVector<Value, 4> sizes = {},
                   SmallVector<Operation *, 4> operations = {},
                   bool preserveDepEdge = true) {
      accessMode = combineAccessModes(accessMode, mode);
      edtToDeps[edtOp].push_back(
          {mode, indices, offsets, sizes, operations, preserveDepEdge});
    }

    /// Compute index pattern information from stored accesses
    struct AccessPatternInfo {
      /// Whether all accesses have the same number of indices
      bool isConsistent = true;
      /// Whether all accesses have indices
      bool allAccessesHaveIndices = true;
      /// Minimum number of pinned dimensions across all accesses
      unsigned pinnedDimCount = 0;

      /// Any deps have offsets/sizes?
      bool hasChunkDeps = false;
      /// Number of chunk dimensions
      unsigned chunkDimCount = 0;
      /// SSA values for block sizes
      SmallVector<Value> blockSizes;
      /// All deps use same block size count?
      bool blockSizesAreConsistent = true;
    };

    AccessPatternInfo getAccessPatternInfo() const {
      AccessPatternInfo info;
      std::optional<unsigned> expectedDimCount;
      bool hasPinnedAccess = false;

      for (const auto &[edtOp, accesses] : edtToDeps) {
        for (const auto &access : accesses) {
          /// If no operations matched this dependency (SSA value mismatch),
          /// fallback to coarse-grained allocation
          if (access.operations.empty()) {
            info.isConsistent = false;
            info.allAccessesHaveIndices = false;
            continue;
          }

          if (access.indices.empty()) {
            info.allAccessesHaveIndices = false;
          } else {
            unsigned dimCount = access.indices.size();

            /// Track minimum pinned dimensions
            if (!hasPinnedAccess) {
              info.pinnedDimCount = dimCount;
              hasPinnedAccess = true;
            } else {
              info.pinnedDimCount = std::min(info.pinnedDimCount, dimCount);
            }

            /// Check consistency
            if (!expectedDimCount) {
              expectedDimCount = dimCount;
              info.pinnedDimCount = dimCount;
            } else if (*expectedDimCount != dimCount) {
              info.isConsistent = false;
            }
          }

          /// Track chunk dependencies from DbControlOp.offsets/sizes
          /// Chunk deps have: indices=[], offsets=[offset], sizes=[blockSize]
          if (!access.offsets.empty() && !access.sizes.empty()) {
            info.hasChunkDeps = true;

            if (info.blockSizes.empty()) {
              /// First chunk dep - record the sizes
              info.blockSizes.assign(access.sizes.begin(), access.sizes.end());
              info.chunkDimCount = access.sizes.size();
            } else {
              /// Subsequent chunk deps - check consistency (dimension count)
              /// We can't compare SSA values statically, so we just check count
              if (access.sizes.size() != info.chunkDimCount) {
                info.blockSizesAreConsistent = false;
              }
            }
          }
        }
      }

      return info;
    }
  };

  DenseMap<Operation *, MemrefInfo> memrefInfo;
  DenseMap<EdtOp, SetVector<Value>> edtExternalValues;

  void collectMemrefs();
  void collectControlDbOps();
  void createDbAllocOps();
  void createDbAcquireOps(EdtOp edt, SetVector<Value> &externalDeps);
  ArtsMode inferEdtAccessMode(Operation *underlyingOp, EdtOp edt) const;
  Value findLocalAcquireView(EdtOp edt, Operation *dbAllocOp,
                             bool requiresIndexedAccess = false);
  Value findParentAcquireSource(EdtOp edt, Operation *dbAllocOp,
                                ArtsMode requestedMode,
                                bool requiresIndexedAccess = false);
  void initializeGlobalDbIfNeeded(Operation *alloc, DbAllocOp dbAllocOp,
                                  ArrayRef<Value> sizes, DbAllocType allocType);

  DbAllocType inferAllocType(Operation *alloc);
  void insertDbFreeForDbAlloc(DbAllocOp dbAlloc, Operation *alloc);
  void rewriteOpsToUseDbAcquire(EdtOp edt, SmallVector<Operation *> &operations,
                                Operation *dbOp,
                                SmallVector<Value> &acquireIndices,
                                SmallVector<Value> &acquireOffsets,
                                Value localAcquireView,
                                const DbRewritePlan &plan);
  void rewriteUsesInParentEdt(MemrefInfo &memrefInfo);
  void rewriteUsesEverywhereCoarse(Operation *alloc, DbAllocOp dbAlloc);

  ///===----------------------------------------------------------------------===///
  //// Helper Functions for Heuristic Decisions
  ///===----------------------------------------------------------------------===///

  /// Computes the number of outer DBs for fine-grained allocation.
  /// Returns the product of pinned dimensions, or kMaxOuterDBs+1 if dynamic.
  static int64_t computeOuterDBs(MemRefType memRefType,
                                 unsigned pinnedDimCount) {
    int64_t outerDBs = 1;
    for (unsigned d = 0; d < pinnedDimCount; ++d) {
      /// Conservative
      if (memRefType.isDynamicDim(d))
        return DbHeuristics::kMaxOuterDBs + 1;
      outerDBs *= memRefType.getDimSize(d);
    }
    return outerDBs;
  }

  /// Computes maximum dependencies per EDT for cost model evaluation.
  static int64_t computeMaxDepsPerEDT(const MemrefInfo &info) {
    int64_t maxDeps = 0;
    for (const auto &[edt, deps] : info.edtToDeps)
      maxDeps = std::max(maxDeps, static_cast<int64_t>(deps.size()));
    return maxDeps;
  }

  /// Computes total inner bytes (product of unpinned dimensions * element
  /// size).
  static int64_t computeInnerBytes(MemRefType memRefType,
                                   unsigned pinnedDimCount) {
    int64_t innerBytes = 1;
    Type elementType = memRefType.getElementType();
    while (auto nested = elementType.dyn_cast<MemRefType>())
      elementType = nested.getElementType();
    int64_t elemSize = elementType.isIntOrFloat()
                           ? (elementType.getIntOrFloatBitWidth() + 7) / 8
                           : 8;

    for (unsigned d = pinnedDimCount; d < memRefType.getRank(); ++d) {
      if (memRefType.isDynamicDim(d))
        return DbHeuristics::kMinInnerBytes + 1; /// Assume large enough
      innerBytes *= memRefType.getDimSize(d);
    }
    return innerBytes * elemSize;
  }
};
} // namespace

///===----------------------------------------------------------------------===///
/// Pass Entry Point
///===----------------------------------------------------------------------===///
ArtsMode CreateDbsPass::inferEdtAccessMode(Operation *underlyingOp,
                                           EdtOp edt) const {
  if (!underlyingOp || !edt)
    return ArtsMode::uninitialized;

  bool hasRead = false;
  bool hasWrite = false;

  edt.walk([&](Operation *op) {
    if (op->getParentOfType<EdtOp>() != edt)
      return;

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (ValueAnalysis::getUnderlyingOperation(load.getMemref()) ==
          underlyingOp)
        hasRead = true;
      return;
    }

    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (ValueAnalysis::getUnderlyingOperation(store.getMemref()) ==
          underlyingOp)
        hasWrite = true;
      return;
    }

    if (auto copy = dyn_cast<memref::CopyOp>(op)) {
      if (ValueAnalysis::getUnderlyingOperation(copy.getSource()) ==
          underlyingOp)
        hasRead = true;
      if (ValueAnalysis::getUnderlyingOperation(copy.getTarget()) ==
          underlyingOp)
        hasWrite = true;
    }
  });

  if (hasRead && hasWrite)
    return ArtsMode::inout;
  if (hasWrite)
    return ArtsMode::out;
  if (hasRead)
    return ArtsMode::in;
  return ArtsMode::uninitialized;
}

void CreateDbsPass::runOnOperation() {
  module = getOperation();
  opsToRemove.clear();
  memrefInfo.clear();

  /// Initialize ArtsCodegen
  auto ownedAC = std::make_unique<ArtsCodegen>(module, false);
  AC = ownedAC.get();

  ARTS_INFO_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Phase 1: Collect memrefs used in EDTs
  ARTS_INFO("Phase 1: Collecting memrefs used in EDTs");
  collectMemrefs();
  ARTS_DEBUG(" - Found " << memrefInfo.size() << " memrefs used in EDTs");

  /// Phase 2: collect and clean up existing Control DB operations
  ARTS_INFO("Phase 2: collect and clean up existing Control DB operations");
  collectControlDbOps();

  /// Phase 3.5: Check for memrefs used without DbControlOps
  /// For each EDT with external dependencies, check if any memref is used
  /// without a corresponding DbControlOp. If so, set that memref to inout.
  for (auto &edtEntry : edtExternalValues) {
    EdtOp edt = edtEntry.first;
    SetVector<Value> &externalDeps = edtEntry.second;

    for (Value externalDep : externalDeps) {
      Operation *underlyingOp =
          ValueAnalysis::getUnderlyingOperation(externalDep);
      if (!underlyingOp)
        continue;

      MemrefInfo &info = memrefInfo[underlyingOp];

      /// Check if this EDT has any DbControlOps for this memref
      bool hasControlDb = !info.edtToDeps[edt].empty();
      if (!hasControlDb) {
        ArtsMode inferredMode = inferEdtAccessMode(underlyingOp, edt);
        if (inferredMode == ArtsMode::uninitialized)
          inferredMode = ArtsMode::inout;

        ARTS_DEBUG(" - Memref "
                   << *underlyingOp
                   << " used in EDT without DbControlOps, inferred mode="
                   << inferredMode);
        info.accessMode = combineAccessModes(info.accessMode, inferredMode);
      }
    }
  }

  /// Also handle any memrefs that remain completely uninitialized
  for (auto &entry : memrefInfo) {
    MemrefInfo &info = entry.second;
    if (info.accessMode == ArtsMode::uninitialized) {
      ARTS_DEBUG(" - Memref "
                 << *entry.first
                 << " has no DbControlOps at all, defaulting to inout mode");
      info.accessMode = ArtsMode::inout;
    }
  }

  /// Phase 3: Create DbAlloc operations
  ARTS_INFO("Phase 3: Creating DbAlloc operations for " << memrefInfo.size()
                                                        << " memrefs");
  createDbAllocOps();

  /// Phase 4: Process EDTs for dependencies
  /// Use pre-order walk to naturally visit parents before children.
  ARTS_INFO("Phase 4: Creating DbAcquire operations for "
            << edtExternalValues.size() << " EDTs");

  module.walk<WalkOrder::PreOrder>([&](EdtOp edt) {
    auto it = edtExternalValues.find(edt);
    if (it != edtExternalValues.end())
      createDbAcquireOps(edt, it->second);
  });

  /// Phase 5: Clean up legacy allocations
  ARTS_INFO("Phase 5: Cleaning up legacy allocations");
  RemovalUtils removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked(module, /*recursive=*/true);

  /// Phase 6: Enabling verification for EDTs
  module.walk([](EdtOp edt) { edt.removeNoVerifyAttr(); });

  /// Phase 7: Simplify the module
  DominanceInfo domInfo(module);
  arts::simplifyIR(module, domInfo);

  AC = nullptr;
  ARTS_INFO_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Allocation Type Inference
///===----------------------------------------------------------------------===///
DbAllocType CreateDbsPass::inferAllocType(Operation *alloc) {
  assert(alloc && "Allocation operation not found");

  if (isa<memref::AllocaOp>(alloc))
    return DbAllocType::stack;
  if (isa<memref::AllocOp>(alloc))
    return DbAllocType::heap;
  if (isa<memref::GetGlobalOp>(alloc))
    return DbAllocType::global;

  /// Default to heap for unknown allocation types
  assert(false && "Unknown allocation type");
  return DbAllocType::heap;
}

///===----------------------------------------------------------------------===///
/// Collect Allocations Used in EDTs
///===----------------------------------------------------------------------===///
void CreateDbsPass::collectMemrefs() {
  memrefInfo.clear();
  module.walk([&](EdtOp edt) {
    edt.walk([&](Operation *op) {
      /// Skip self-references
      if (auto edtOp = dyn_cast<EdtOp>(op)) {
        if (edtOp == edt)
          return;
      }

      /// Check all operands of all operations for memory references
      for (Value operand : op->getOperands()) {
        if (!operand.getType().isa<MemRefType>())
          continue;

        /// Get the underlying value of the memory reference
        Operation *underlyingOp =
            ValueAnalysis::getUnderlyingOperation(operand);
        if (!underlyingOp) {
          if (auto load = operand.getDefiningOp<memref::LoadOp>())
            underlyingOp =
                ValueAnalysis::getUnderlyingOperation(load.getMemref());
        }
        if (!underlyingOp) {
          ARTS_WARN("Skipping value with no underlying operation: " << operand);
          continue;
        }

        /// If it is found, check the parent edt
        EdtOp parentEdt = underlyingOp->getParentOfType<EdtOp>();

        /// Create memref info for the underlying operation
        MemrefInfo &info = memrefInfo[underlyingOp];
        info.alloc = underlyingOp;
        info.parentEdt = parentEdt;

        /// If the parent edt is different from the current edt, it means
        /// it is an external dependency
        if (parentEdt != edt) {
          edtExternalValues[edt].insert(operand);
          info.usedByOtherEdts = true;
        }
      }
    });
  });
  ARTS_DEBUG("collectMemrefs: Total " << memrefInfo.size()
                                      << " unique memrefs");
}

///===----------------------------------------------------------------------===///
/// Collect Control DB Operations
///===----------------------------------------------------------------------===///
void CreateDbsPass::collectControlDbOps() {
  /// First, extract dependency mode and index information from all db_control
  /// ops before erasing them
  SmallVector<DbControlOp, 4> dbControlOps;
  module.walk([&](DbControlOp dbControl) {
    dbControlOps.push_back(dbControl);
    Value ptr = dbControl.getPtr();
    ArtsMode mode = dbControl.getMode();
    auto indices = dbControl.getIndices();
    auto offsets = dbControl.getOffsets();
    auto sizes = dbControl.getSizes();

    /// Get the underlying operation for this pointer
    Operation *underlyingOp = ValueAnalysis::getUnderlyingOperation(ptr);
    if (!underlyingOp)
      return;

    ARTS_DEBUG(" - Found DbControlOp for "
               << *underlyingOp << " with mode: " << mode << ", "
               << indices.size() << " indices, " << offsets.size()
               << " offsets, " << sizes.size() << " sizes");

    /// Get user EDT
    EdtOp userEdt = dyn_cast_or_null<EdtOp>(DbAnalysis::findUserEdt(dbControl));

    SmallVector<Value, 4> indexValues(indices.begin(), indices.end());
    SmallVector<Value, 4> offsetValues(offsets.begin(), offsets.end());
    SmallVector<Value, 4> sizeValues(sizes.begin(), sizes.end());

    /// Track all operations in this EDT that use this memref AND match the
    /// indices
    SmallVector<Operation *, 4> opsToRewrite;
    if (userEdt) {
      auto controlAccessOffsets =
          getStencilAccessOffsets(dbControl.getOperation());

      /// Helper to check if operation indices match DbControlOp using SSA value
      /// equality This is strict but correct for distinguishing multiple chunks
      /// (e.g., A[i-1], A[i], A[i+1])
      auto indicesMatchPrefix = [&](ValueRange opIndices) -> bool {
        /// If operation has fewer indices than pinned dimensions, can't match
        if (opIndices.size() < indexValues.size())
          return false;

        /// Check if the first N indices match the pinned indices (N =
        /// indexValues.size()) using SSA value equality
        return std::equal(indexValues.begin(), indexValues.end(),
                          opIndices.begin());
      };

      auto matchesExplicitStencilAccess = [&](Operation *op) -> bool {
        if (!controlAccessOffsets)
          return false;
        auto opAccessOffsets = getStencilAccessOffsets(op);
        return opAccessOffsets && *opAccessOffsets == *controlAccessOffsets;
      };

      /// Walk the user EDT and collect operations that match the indices
      userEdt.walk([&](Operation *op) {
        if (auto load = dyn_cast<memref::LoadOp>(op)) {
          if (ValueAnalysis::getUnderlyingOperation(load.getMemref()) ==
              underlyingOp) {
            bool matchesControl = controlAccessOffsets
                                      ? matchesExplicitStencilAccess(op)
                                      : indicesMatchPrefix(load.getIndices());
            if (opMatchesAccessMode(op, underlyingOp, mode) && matchesControl)
              opsToRewrite.push_back(op);
          }
        } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
          if (ValueAnalysis::getUnderlyingOperation(store.getMemref()) ==
              underlyingOp) {
            bool matchesControl = controlAccessOffsets
                                      ? matchesExplicitStencilAccess(op)
                                      : indicesMatchPrefix(store.getIndices());
            if (opMatchesAccessMode(op, underlyingOp, mode) && matchesControl)
              opsToRewrite.push_back(op);
          }
        }
      });
      ARTS_DEBUG("    - Found "
                 << opsToRewrite.size()
                 << " operations to rewrite for this specific dependency");
    }

    memrefInfo[underlyingOp].addAccess(
        userEdt, mode, indexValues, offsetValues, sizeValues, opsToRewrite,
        /*preserveDepEdge=*/!isLocalityOnly(dbControl.getOperation()));
  });

  /// Now erase all db_control ops, replacing uses with pointer
  for (DbControlOp dbControl : dbControlOps) {
    dbControl.getSubview().replaceAllUsesWith(dbControl.getPtr());
    dbControl.erase();
  }

  /// Clear all existing EDT dependencies and block args that are not used
  /// within the block
  module.walk([&](EdtOp edt) {
    Block &block = edt.getBody().front();

    /// Only erase block arguments that are not used within the block
    block.eraseArguments([&](BlockArgument arg) {
      /// Check if this argument is used anywhere in the block
      bool isUsed = false;
      block.walk([&](Operation *op) {
        for (Value operand : op->getOperands()) {
          if (operand == arg) {
            isUsed = true;
            return WalkResult::interrupt();
          }
        }
        return WalkResult::advance();
      });

      /// Only erase if the argument is not used
      return !isUsed;
    });

    /// Preserve the route operand when clearing dependencies
    SmallVector<Value> preservedOperands;
    if (Value route = edt.getRoute())
      preservedOperands.push_back(route);
    edt->setOperands(preservedOperands);
  });
}

/// Create DB Allocation Operations
void CreateDbsPass::createDbAllocOps() {
  /// All memrefs in memrefInfo are converted to DBs
  for (auto &entry : memrefInfo) {
    Operation *alloc = entry.first;
    ARTS_DEBUG("Creating DB Alloc Op for memref " << *alloc);
    MemrefInfo &info = entry.second;
    Location loc = alloc->getLoc();
    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPointAfter(alloc);

    /// Skip local allocations that do not escape their defining EDT.
    /// These buffers are private to a single EDT and do not need datablocks.
    if (info.parentEdt && !info.usedByOtherEdts) {
      bool escapesEdt = false;
      for (auto &use : alloc->getUses()) {
        Operation *user = use.getOwner();
        if (!info.parentEdt->isAncestor(user)) {
          escapesEdt = true;
          break;
        }
      }
      if (!escapesEdt) {
        ARTS_DEBUG(" - Skipping local EDT allocation (no external uses)");
        continue;
      }
    }

    /// Infer allocation type based on the defining operation
    DbAllocType allocType = inferAllocType(alloc);

    /// Use the dependency mode from DbControlOp
    ArtsMode mode = info.accessMode;
    ARTS_DEBUG(" - Using access mode " << mode);

    /// Set DbMode based on the access mode
    DbMode dbMode = DbMode::write;
    if (mode == ArtsMode::in)
      dbMode = DbMode::read;
    else if (mode == ArtsMode::out || mode == ArtsMode::inout)
      dbMode = DbMode::write;

    /// Get the element type of the memref
    Value allocValue = alloc->getResult(0);
    MemRefType memRefType = allocValue.getType().cast<MemRefType>();
    Type elementType = memRefType.getElementType();
    while (auto nestedMemRef = elementType.dyn_cast<MemRefType>())
      elementType = nestedMemRef.getElementType();

    /// Determine allocation granularity using unified heuristics
    SmallVector<Value> sizes, elementSizes;
    const bool isRankZero = memRefType.getRank() == 0;
    const unsigned rank = std::max<unsigned>(1, memRefType.getRank());
    const auto accessPatternInfo = info.getAccessPatternInfo();

    /// CreateDbs ALWAYS creates COARSE allocations.
    /// The actual partitioning is deferred to DbPartitioning pass.
    /// Partition info is carried by DbAcquireOp's offsets/indices attributes.

    /// Store block sizes for later use in createDbAcquireOps
    if (accessPatternInfo.hasChunkDeps) {
      info.blockSizes.assign(accessPatternInfo.blockSizes.begin(),
                             accessPatternInfo.blockSizes.end());
    }

    /// ALWAYS create COARSE allocation:
    /// sizes = [1], elementSizes = [all original dimensions]
    /// DbPartitioning will transform this based on the hint
    ARTS_DEBUG(" - Using coarse-grained allocation (deferred partitioning)");
    elementSizes.reserve(rank);
    for (unsigned i = 0; i < rank; ++i) {
      if (!isRankZero && memRefType.isDynamicDim(i)) {
        elementSizes.push_back(
            AC->create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
      } else {
        int64_t dimSize =
            isRankZero ? 1 : static_cast<int64_t>(memRefType.getDimSize(i));
        elementSizes.push_back(AC->createIndexConstant(dimSize, loc));
      }
    }
    sizes.push_back(AC->createIndexConstant(1, loc));

    /// Always coarse partition mode - DbPartitioning will change if needed
    PartitionMode partitionMode = PartitionMode::coarse;
    ARTS_DEBUG(" - Partition mode: coarse (to be refined by DbPartitioning)");

    /// Create the db_alloc operation
    auto route = AC->createIntConstant(0, AC->Int32, loc);
    auto dbAllocOp =
        AC->create<DbAllocOp>(loc, mode, route, allocType, dbMode, elementType,
                              sizes, elementSizes, partitionMode);

    {
      OpBuilder::InsertionGuard contractGuard(AC->getBuilder());
      AC->setInsertionPointAfter(dbAllocOp);
      if (auto contractInfo = getLoweringContract(alloc, AC->getBuilder(), loc))
        upsertLoweringContract(AC->getBuilder(), loc, dbAllocOp.getPtr(),
                               *contractInfo);
    }

    /// Initialize global DBs
    initializeGlobalDbIfNeeded(alloc, dbAllocOp, sizes, allocType);

    /// Transfer metadata from original allocation to DbAllocOp
    if (AM->getMetadataManager().transferMetadata(alloc, dbAllocOp)) {
      ARTS_DEBUG(
          "  Transferred metadata from original allocation to DbAllocOp");
    } else {
      ARTS_DEBUG("  No metadata found for original allocation");
    }

    /// Create coarse rewrite plan - DbPartitioning will refine based on hint
    DbRewritePlan plan(PartitionMode::coarse);
    info.rewritePlan = plan;

    /// Record allocation strategy decision for diagnostics
    AM->getDbHeuristics().recordDecision(
        "AllocationStrategy", true, "Coarse allocation", alloc,
        {{"outerRank", 0}, {"innerRank", static_cast<int64_t>(rank)}});

    /// Store mappings for later use
    info.dbAllocOp = dbAllocOp;
    dbPtrToOriginalAlloc[dbAllocOp] = alloc;

    /// Insert DbFreeOp and remove associated dealloc operations
    insertDbFreeForDbAlloc(dbAllocOp, alloc);

    /// Rewire uses of the original allocation to use the db_alloc operation.
    /// If the allocation has a defining EDT, rewrite within that EDT. If it
    /// lives outside any EDT (host/global), rewrite all uses so host code sees
    /// the datablock contents as well.
    if (info.parentEdt) {
      rewriteUsesInParentEdt(info);
    } else {
      /// Use element-wise indexer with outerRank=0
      /// Coarse allocation: keep indices empty so db_ref uses constant zero
      PartitionInfo partInfo;
      partInfo.mode = PartitionMode::coarse;

      DbElementWiseIndexer indexer(partInfo, 0, rank, {});

      /// Step 1: Rewrite parent region uses (skips EDTs by design)
      indexer.transformUsesInParentRegion(alloc, dbAllocOp, *AC, opsToRemove);

      /// Step 2: Rewrite EDT uses without explicit deps
      Type elementMemRefType = dbAllocOp.getAllocatedElementType();
      SmallVector<Operation *> edtUsesToRewrite;
      for (auto &use : alloc->getUses()) {
        Operation *user = use.getOwner();
        if (user == dbAllocOp.getOperation())
          continue;
        if (auto parentEdt = user->getParentOfType<EdtOp>()) {
          if (info.edtToDeps.count(parentEdt) == 0)
            edtUsesToRewrite.push_back(user);
        }
      }
      if (!edtUsesToRewrite.empty()) {
        indexer.transformOps(edtUsesToRewrite, dbAllocOp.getPtr(),
                             elementMemRefType, *AC, opsToRemove);
      }
    }
  }
}

/// Initialize global constants for single coarse-grained DBs.
/// Since CreateDbs now always creates coarse allocations, this is always safe.
void CreateDbsPass::initializeGlobalDbIfNeeded(Operation *alloc,
                                               DbAllocOp dbAllocOp,
                                               ArrayRef<Value> sizes,
                                               DbAllocType allocType) {
  if (allocType != DbAllocType::global)
    return;

  auto getGlobal = dyn_cast<memref::GetGlobalOp>(alloc);
  if (!getGlobal)
    return;

  auto globalOp = SymbolTable::lookupNearestSymbolFrom<memref::GlobalOp>(
      getGlobal, getGlobal.getNameAttr());
  if (!globalOp || !globalOp.getInitialValue().has_value())
    return;

  Location loc = alloc->getLoc();
  OpBuilder::InsertionGuard initGuard(AC->getBuilder());
  AC->setInsertionPointAfter(dbAllocOp);
  Value globalMemref = AC->create<memref::GetGlobalOp>(loc, globalOp.getType(),
                                                       getGlobal.getNameAttr());

  SmallVector<Value> zeroIndices;
  zeroIndices.reserve(sizes.size());
  for (size_t i = 0; i < sizes.size(); ++i)
    zeroIndices.push_back(AC->createIndexConstant(0, loc));
  Value dbRef = AC->create<DbRefOp>(loc, dbAllocOp.getPtr(), zeroIndices);

  auto memRefType = globalOp.getType().cast<MemRefType>();
  unsigned rank = memRefType.getRank();
  if (rank == 0) {
    Value initVal = AC->create<memref::LoadOp>(loc, globalMemref);
    AC->create<memref::StoreOp>(loc, initVal, dbRef);
    return;
  }

  SmallVector<Value> indices;
  indices.reserve(rank);
  auto emitLoopCopy = [&](auto &self, unsigned dim) -> void {
    if (dim == rank) {
      Value initVal = AC->create<memref::LoadOp>(loc, globalMemref, indices);
      AC->create<memref::StoreOp>(loc, initVal, dbRef, indices);
      return;
    }

    Value lower = AC->createIndexConstant(0, loc);
    Value upper;
    if (memRefType.isDynamicDim(dim)) {
      upper = AC->create<memref::DimOp>(loc, globalMemref, dim);
    } else {
      upper = AC->createIndexConstant(memRefType.getDimSize(dim), loc);
    }
    Value step = AC->createIndexConstant(1, loc);

    auto loop = AC->create<scf::ForOp>(loc, lower, upper, step);
    OpBuilder::InsertionGuard loopGuard(AC->getBuilder());
    AC->setInsertionPointToStart(loop.getBody());
    indices.push_back(loop.getInductionVar());
    self(self, dim + 1);
    indices.pop_back();
  };
  emitLoopCopy(emitLoopCopy, 0);
}

/// Find parent scope's acquired handle for a datablock
/// Checks if the parent EDT already has an acquired handle for the given
/// DbAllocOp.
Value CreateDbsPass::findLocalAcquireView(EdtOp edt, Operation *dbAllocOp,
                                          bool requiresIndexedAccess) {
  auto dbAlloc = dyn_cast_or_null<DbAllocOp>(dbAllocOp);
  if (!dbAlloc || !edt)
    return nullptr;

  Type targetType = dbAlloc.getPtr().getType();
  Block &body = edt.getBody().front();
  auto deps = edt.getDependenciesAsVector();

  auto matchesDb = [&](Value v) -> bool {
    if (!v || v.getType() != targetType)
      return false;

    Operation *db = DbUtils::getUnderlyingDbAlloc(v);
    if (!db || db != dbAllocOp)
      return false;

    if (requiresIndexedAccess) {
      if (Operation *dbOp = DbUtils::getUnderlyingDb(v)) {
        if (DbAnalysis::hasSingleSize(dbOp))
          return false;
      }
    }
    return true;
  };

  for (auto [idx, dep] : llvm::enumerate(deps)) {
    if (idx >= body.getNumArguments())
      break;
    if (matchesDb(dep))
      return body.getArgument(idx);
  }

  return nullptr;
}

Value CreateDbsPass::findParentAcquireSource(EdtOp edt, Operation *dbAllocOp,
                                             ArtsMode requestedMode,
                                             bool requiresIndexedAccess) {
  auto dbAlloc = dyn_cast_or_null<DbAllocOp>(dbAllocOp);
  if (!dbAlloc)
    return nullptr;

  ARTS_DEBUG("   - Checking if parent has acquired handle for this memref");

  Block *parentBlock = edt->getBlock();
  if (!parentBlock)
    return nullptr;

  Type targetType = dbAlloc.getPtr().getType();

  auto matchesDb = [&](Value v) -> bool {
    if (!v || v.getType() != targetType)
      return false;

    Operation *db = DbUtils::getUnderlyingDbAlloc(v);
    if (!db || db != dbAllocOp)
      return false;

    /// If the caller intends to index into this handle, avoid reusing a
    /// single-element datablock view.
    if (requiresIndexedAccess) {
      if (Operation *dbOp = DbUtils::getUnderlyingDb(v)) {
        if (DbAnalysis::hasSingleSize(dbOp))
          return false;
      }
    }

    /// Avoid treating the DbAllocOp results themselves as reusable handles
    if (auto *defOp = v.getDefiningOp())
      if (isa<DbAllocOp>(defOp))
        return false;

    if (Operation *dbOp = DbUtils::getUnderlyingDb(v)) {
      if (auto acquire = dyn_cast<DbAcquireOp>(dbOp)) {
        if (!accessModeCanSeedNestedAcquire(acquire.getMode(), requestedMode))
          return false;
      }
    }

    return true;
  };

  /// Walk outward through enclosing block arguments and look for a handle that
  /// is already available in scope at the current insertion point.
  /// This handles nested EDTs inside arts.for/scf.for regions where the
  /// immediate block has only loop IV arguments.
  Block *scanBlock = parentBlock;
  Operation *anchorOp = edt.getOperation();

  while (scanBlock && anchorOp) {
    for (BlockArgument arg : scanBlock->getArguments()) {
      if (matchesDb(arg)) {
        ARTS_DEBUG("   - Found matching handle in enclosing block arguments");
        return arg;
      }
    }

    Operation *ownerOp = scanBlock->getParentOp();
    if (!ownerOp)
      break;
    scanBlock = ownerOp->getBlock();
    anchorOp = ownerOp;
  }

  ARTS_DEBUG("   - No existing handle found in parent scope");
  return nullptr;
}

/// Create DB Acquire Operations for EDT Dependencies
void CreateDbsPass::createDbAcquireOps(EdtOp edt,
                                       SetVector<Value> &externalDeps) {
  ARTS_DEBUG(" - Creating DbAcquire operations for "
             << externalDeps.size() << " external dependencies");

  /// Accumulate dependency operands to set on the EDT
  SmallVector<Value> dependencyOperands;

  /// Track which DbAllocOps have already been processed for coarse-grained
  /// acquires. For fine-grained acquires, we may need multiple acquires per
  /// DbAllocOp with different indices/offsets, so we don't skip those.
  DenseSet<Operation *> processedCoarseAllocs;

  /// For each external value, create acquire and release operations
  for (Value externalDep : externalDeps) {
    /// Locate the underlying operation
    Operation *underlyingOp =
        ValueAnalysis::getUnderlyingOperation(externalDep);
    if (!underlyingOp)
      if (auto load = externalDep.getDefiningOp<memref::LoadOp>())
        underlyingOp = ValueAnalysis::getUnderlyingOperation(load.getMemref());
    if (!underlyingOp) {
      ARTS_DEBUG("Skipping external dep with no underlying allocation: "
                 << externalDep);
      continue;
    }

    /// Get the memref info for the underlying operation
    MemrefInfo &info = memrefInfo[underlyingOp];
    DbAllocOp dbAllocOp = info.dbAllocOp;
    ARTS_DEBUG("DbAllocOp " << dbAllocOp);
    if (!dbAllocOp) {
      ARTS_ERROR("DbAllocOp not found for: " << *underlyingOp);
      assert(false && "DbAllocOp not found for underlying operation");
      continue;
    }

    /// Get the source guid and ptr
    auto sourceGuid = dbAllocOp.getGuid();
    auto sourcePtr = dbAllocOp.getPtr();
    assert((sourceGuid && sourcePtr) && "Source guid and ptr must be non-null");

    /// Create acquire operation (pass both source guid and ptr)
    OpBuilder::InsertionGuard IG(AC->getBuilder());
    AC->setInsertionPoint(edt);

    /// Create acquires based on DbControlOp dependencies
    const auto &deps = info.edtToDeps[edt];
    const auto &plan = info.rewritePlan.value();

    /// Deduplicate: one acquire per allocation per EDT
    if (!processedCoarseAllocs.insert(dbAllocOp.getOperation()).second) {
      ARTS_DEBUG("   - Skipping duplicate acquire for already-processed "
                 "DbAllocOp");
      continue;
    }

    /// Build coarse DB-space: offsets=[0], sizes=[allocSizes]
    SmallVector<Value> dbOffsets, dbSizes;
    dbOffsets.push_back(AC->createIndexConstant(0, edt.getLoc()));

    SmallVector<Value> allocSizes(dbAllocOp.getSizes().begin(),
                                  dbAllocOp.getSizes().end());
    if (allocSizes.empty()) {
      dbSizes.push_back(AC->createIndexConstant(1, edt.getLoc()));
    } else {
      for (Value s : allocSizes)
        dbSizes.push_back(s);
    }

    SmallVector<SmallVector<const MemrefInfo::DbDep *, 4>, 4> depGroups;
    SmallVector<ArtsMode, 4> depGroupModes;
    SmallVector<bool, 4> depGroupPreserveDepEdges;
    SmallVector<PartitionMode, 4> depGroupPartitionModes;
    if (deps.empty()) {
      depGroups.emplace_back();
      depGroupModes.push_back(ArtsMode::uninitialized);
      depGroupPreserveDepEdges.push_back(true);
      depGroupPartitionModes.push_back(PartitionMode::coarse);
    } else {
      for (const auto &dep : deps) {
        PartitionMode depPartitionMode = PartitionMode::coarse;
        if (!dep.indices.empty()) {
          depPartitionMode = PartitionMode::fine_grained;
        } else if (!dep.offsets.empty() && !dep.sizes.empty()) {
          depPartitionMode = PartitionMode::block;
        }
        int groupIndex = -1;
        for (auto [idx, groupMode] : llvm::enumerate(depGroupModes)) {
          if (groupMode == dep.mode &&
              depGroupPreserveDepEdges[idx] == dep.preserveDepEdge &&
              depGroupPartitionModes[idx] == depPartitionMode) {
            groupIndex = static_cast<int>(idx);
            break;
          }
        }
        if (groupIndex < 0) {
          depGroupModes.push_back(dep.mode);
          depGroupPreserveDepEdges.push_back(dep.preserveDepEdge);
          depGroupPartitionModes.push_back(depPartitionMode);
          depGroups.emplace_back();
          depGroups.back().push_back(&dep);
          continue;
        }
        depGroups[groupIndex].push_back(&dep);
      }
    }

    /// Track the first dependency-backed local view materialized for this DB
    /// so locality-only acquires inside the EDT can derive nested views from
    /// an in-scope handle instead of the outer db_alloc result.
    Value inScopeAcquireView;

    for (auto [groupIdx, depGroup] : llvm::enumerate(depGroups)) {
      /// CreateDbs always creates coarse-grained DB-space acquires. The
      /// partition hints (from DbControlOp depend clauses) tell DbPartitioning
      /// how to optimize later. Keep different access modes in separate
      /// acquires so downstream rec_dep lowering preserves read vs write slots.
      const bool preserveDepEdge = depGroupPreserveDepEdges[groupIdx];
      ARTS_DEBUG(" - Creating coarse-grained acquire group "
                 << groupIdx << " for mode " << depGroupModes[groupIdx]
                 << ", preserveDepEdge=" << preserveDepEdge);

      /// Collect partition hints from this group's deps.
      SmallVector<Value> partIndices, partOffsets, partSizes;
      SmallVector<int32_t> indicesSegments, offsetsSegments, sizesSegments;
      SmallVector<int32_t> entryModes;
      ArtsMode acquireMode = depGroupModes[groupIdx];

      for (const MemrefInfo::DbDep *dep : depGroup) {
        PartitionMode entryMode = PartitionMode::coarse;
        if (!dep->indices.empty()) {
          entryMode = PartitionMode::fine_grained;
        } else if (!dep->offsets.empty() && !dep->sizes.empty()) {
          entryMode = PartitionMode::block;
        }
        entryModes.push_back(static_cast<int32_t>(entryMode));

        indicesSegments.push_back(static_cast<int32_t>(dep->indices.size()));
        for (Value v : dep->indices)
          partIndices.push_back(v);

        offsetsSegments.push_back(static_cast<int32_t>(dep->offsets.size()));
        for (Value v : dep->offsets)
          partOffsets.push_back(v);

        sizesSegments.push_back(static_cast<int32_t>(dep->sizes.size()));
        for (Value v : dep->sizes)
          partSizes.push_back(v);
      }

      /// If this EDT has no explicit db_control hints for the memref, infer
      /// per-EDT access from actual loads/stores before falling back.
      if (acquireMode == ArtsMode::uninitialized) {
        acquireMode = inferEdtAccessMode(underlyingOp, edt);
        if (acquireMode == ArtsMode::uninitialized) {
          acquireMode = (info.accessMode == ArtsMode::uninitialized)
                            ? ArtsMode::inout
                            : info.accessMode;
        }
      }

      PartitionMode partMode = PartitionMode::coarse;
      if (!entryModes.empty())
        partMode = static_cast<PartitionMode>(entryModes[0]);

      Value acqGuid = sourceGuid;
      Value acqPtr = sourcePtr;
      if (!preserveDepEdge && inScopeAcquireView) {
        ARTS_DEBUG("   - Reusing existing datablock handle inside EDT");
        acqGuid = Value();
        acqPtr = inScopeAcquireView;
      }
      if (acqPtr == sourcePtr)
        if (Value availableHandle =
                findParentAcquireSource(edt, dbAllocOp, acquireMode)) {
          ARTS_DEBUG("   - Reusing existing datablock handle in parent scope");
          acqGuid = Value();
          acqPtr = availableHandle;
        }

      auto createAcquire = [&](Location acquireLoc) {
        Type ptrType = acqPtr ? acqPtr.getType() : Type();
        if (DbUtils::getUnderlyingDbAlloc(acqPtr)) {
          return AC->create<DbAcquireOp>(
              acquireLoc, acquireMode, acqGuid, acqPtr, partMode,
              /*indices=*/SmallVector<Value>{}, /*offsets=*/dbOffsets,
              /*sizes=*/dbSizes,
              /*partition_indices=*/partIndices,
              /*partition_offsets=*/partOffsets,
              /*partition_sizes=*/partSizes);
        }

        return AC->create<DbAcquireOp>(
            acquireLoc, acquireMode, acqGuid, acqPtr, ptrType, partMode,
            /*indices=*/SmallVector<Value>{}, /*offsets=*/dbOffsets,
            /*sizes=*/dbSizes,
            /*partition_indices=*/partIndices,
            /*partition_offsets=*/partOffsets,
            /*partition_sizes=*/partSizes);
      };

      if (!preserveDepEdge)
        AC->setInsertionPointToStart(&edt.getBody().front());
      auto acquireOp = createAcquire(edt.getLoc());

      if (auto depPattern = getEffectiveDepPattern(edt.getOperation()))
        acquireOp.setDepPattern(*depPattern);
      copyStencilContractAttrs(edt.getOperation(), acquireOp.getOperation());
      {
        OpBuilder::InsertionGuard contractGuard(AC->getBuilder());
        AC->setInsertionPointAfter(acquireOp);
        Value contractTarget = acquireOp.getPtr();
        if (auto contractInfo = getLoweringContract(
                edt.getOperation(), AC->getBuilder(), edt.getLoc()))
          upsertLoweringContract(AC->getBuilder(), edt.getLoc(), contractTarget,
                                 *contractInfo);
      }

      if (!depGroup.empty()) {
        /// Explicit DbControlOp-derived acquires always preserve their access
        /// mode contract. Only true predecessor controls also preserve the EDT
        /// dependency edge.
        acquireOp.setPreserveAccessMode();
        if (preserveDepEdge)
          acquireOp.setPreserveDepEdge();
      }

      acquireOp.setPartitionSegments(indicesSegments, offsetsSegments,
                                     sizesSegments, entryModes);

      ARTS_DEBUG(" - Created acquire group with "
                 << depGroup.size() << " partition entries, primary mode="
                 << static_cast<int>(partMode) << ": " << acquireOp);

      Value localAcquireView = acquireOp.getPtr();
      if (preserveDepEdge) {
        auto sourceType = localAcquireView.getType().dyn_cast<MemRefType>();
        BlockArgument dbAcquireArg =
            edt.getBody().front().addArgument(sourceType, edt.getLoc());
        dependencyOperands.push_back(localAcquireView);
        localAcquireView = dbAcquireArg;
        if (!inScopeAcquireView)
          inScopeAcquireView = localAcquireView;
      }

      SmallVector<Operation *> opsToRewrite;
      llvm::SmallPtrSet<Operation *, 16> seenOps;
      auto addOpIfNew = [&](Operation *op) {
        if (!op)
          return;
        if (seenOps.insert(op).second)
          opsToRewrite.push_back(op);
      };

      bool missingOpsForDeps = deps.empty();
      for (const MemrefInfo::DbDep *dep : depGroup) {
        for (Operation *op : dep->operations)
          addOpIfNew(op);
      }

      /// Fallback: collect all operations using this memref in EDT when there
      /// are no explicit dependency entries for the memref.
      if (opsToRewrite.empty() || missingOpsForDeps) {
        ARTS_DEBUG(
            " - Fallback: collecting all operations using this memref in EDT");
        edt.walk([&](Operation *op) {
          if (op->getParentOfType<EdtOp>() != edt)
            return;
          if (!opMatchesAccessMode(op, underlyingOp, acquireMode))
            return;
          addOpIfNew(op);
        });
        ARTS_DEBUG(" - Found " << opsToRewrite.size()
                               << " operations to rewrite");
      }

      if (!opsToRewrite.empty()) {
        SmallVector<Value> dbRefIndices;
        dbRefIndices.push_back(AC->createIndexConstant(0, edt.getLoc()));
        rewriteOpsToUseDbAcquire(edt, opsToRewrite, acquireOp, dbRefIndices,
                                 dbOffsets, localAcquireView, plan);
      }

      OpBuilder::InsertionGuard releaseGuard(AC->getBuilder());
      AC->setInsertionPoint(edt.getBody().front().getTerminator());
      AC->create<DbReleaseOp>(edt.getLoc(), localAcquireView);
    }
  }

  /// After processing all memrefs, set EDT dependencies
  edt.setDependencies(dependencyOperands);
}

/// Insert db_free for DbAllocOp at the appropriate location and remove
/// associated dealloc operations
void CreateDbsPass::insertDbFreeForDbAlloc(DbAllocOp dbAlloc,
                                           Operation *alloc) {
  Location loc = dbAlloc.getLoc();
  Value allocResult = alloc->getResult(0);

  /// Find associated dealloc operation
  std::optional<Operation *> deallocOp = memref::findDealloc(allocResult);
  if (deallocOp.has_value() && *deallocOp)
    opsToRemove.insert(*deallocOp);

  /// Determine where to insert DbFreeOp based on where dbAlloc is located
  /// Insert at the end of the block containing dbAlloc
  Block *allocBlock = dbAlloc->getBlock();
  Operation *insertionPoint = allocBlock->getTerminator();

  /// Insert DbFreeOp if we found a valid insertion point
  assert(insertionPoint && "Could not find insertion point for DbFreeOp");
  OpBuilder::InsertionGuard IG(AC->getBuilder());
  AC->setInsertionPoint(insertionPoint);
  AC->create<DbFreeOp>(loc, dbAlloc.getGuid());
  AC->create<DbFreeOp>(loc, dbAlloc.getPtr());
}

/// Rewrite uses in parent EDT based on DB allocation granularity.
void CreateDbsPass::rewriteUsesInParentEdt(MemrefInfo &memrefInfo) {
  assert(memrefInfo.dbAllocOp && "No DbAllocOp found");

  auto dbAlloc = memrefInfo.dbAllocOp;
  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  /// Collect users in the parent edt
  SmallVector<Operation *, 8> users;
  unsigned totalUses = 0;
  unsigned skippedUses = 0;
  for (auto &use : memrefInfo.alloc->getUses()) {
    totalUses++;
    Operation *user = use.getOwner();
    EdtOp userParentEdt = user->getParentOfType<EdtOp>();
    if (userParentEdt == memrefInfo.parentEdt) {
      users.push_back(user);
      ARTS_DEBUG("   Collected user: " << user->getName() << " at "
                                       << user->getLoc());
    } else {
      skippedUses++;
      ARTS_DEBUG("   Skipped user (different EDT): " << user->getName());
    }
  }

  ARTS_DEBUG(" - Alloc has " << totalUses << " total uses, collected "
                             << users.size() << " in parent EDT, skipped "
                             << skippedUses);

  /// Coarse-grained: db_ref[0] + load/store[indices]
  /// Fine-grained: db_ref[indices] + load/store[0]
  unsigned outerRank =
      memrefInfo.usedFineGrained ? dbAlloc.getSizes().size() : 0;
  unsigned innerRank = elementMemRefType.cast<MemRefType>().getRank();
  /// Coarse allocation: keep indices empty so db_ref uses constant zero
  PartitionInfo info;
  info.mode = PartitionMode::coarse;
  DbElementWiseIndexer indexer(info, outerRank, innerRank, {});
  for (Operation *user : users) {
    size_t sizeBefore = opsToRemove.size();
    indexer.transformAccess(user, dbAlloc.getPtr(), elementMemRefType, *AC,
                            opsToRemove);
    if (opsToRemove.size() > sizeBefore) {
      ARTS_DEBUG("   Transformed: " << user->getName()
                                    << " -> added to remove");
    } else {
      ARTS_DEBUG("   NOT transformed: " << user->getName() << " at "
                                        << user->getLoc());
    }
  }
}

/// Rewrite uses of a coarse-grained allocation in the parent region.
/// This is used when the allocation is not inside an EDT, but is still shared
/// with EDTs and the host needs to see the updated data.
/// For coarse-grained: all indices go to inner load/store, db_ref gets [0].
void CreateDbsPass::rewriteUsesEverywhereCoarse(Operation *alloc,
                                                DbAllocOp dbAlloc) {
  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  SmallVector<Operation *> users;
  for (auto &use : alloc->getUses()) {
    Operation *user = use.getOwner();
    /// Skip the DbAlloc itself
    if (user == dbAlloc.getOperation())
      continue;
    /// Skip uses inside EDTs; those are rewritten via acquires separately.
    if (user->getParentOfType<EdtOp>())
      continue;
    users.push_back(user);
  }

  if (users.empty())
    return;

  /// Coarse-grained: outerCount=0, all indices go to inner load/store
  /// Keep indices empty so db_ref uses constant zero
  PartitionInfo info;
  info.mode = PartitionMode::coarse;
  DbElementWiseIndexer indexer(
      info, 0, elementMemRefType.cast<MemRefType>().getRank(), {});
  for (Operation *user : users)
    indexer.transformAccess(user, dbAlloc.getPtr(), elementMemRefType, *AC,
                            opsToRemove);
}

/// Rewrite operations to use DbAcquire
void CreateDbsPass::rewriteOpsToUseDbAcquire(
    EdtOp edt, SmallVector<Operation *> &operations, Operation *dbOp,
    SmallVector<Value> &acquireIndices, SmallVector<Value> &acquireOffsets,
    Value localAcquireView, const DbRewritePlan &plan) {
  ARTS_DEBUG(" - Rewriting " << operations.size() << " operations in EDT");

  /// Get the element type from the acquired datablock
  /// For fine-grained: memref<?xmemref<?xT>> -> extract memref<?xT>
  Type elementMemRefType = localAcquireView.getType();
  if (auto outerMemrefType = elementMemRefType.dyn_cast<MemRefType>()) {
    if (auto innerMemrefType =
            outerMemrefType.getElementType().dyn_cast<MemRefType>()) {
      elementMemRefType = innerMemrefType;
    }
  }
  ARTS_DEBUG(" - Element memref type: " << elementMemRefType);

  /// Use stored ranks from plan, with fallback for innerRank
  unsigned innerRank = plan.innerRank();
  if (innerRank == 0)
    if (auto memrefType = elementMemRefType.dyn_cast<MemRefType>())
      innerRank = memrefType.getRank();

  /// Build value mapping from parent EDT block args/acquires to this EDT's args
  IRMapping scopeMapping;
  Block &edtBlock = edt.getBody().front();
  auto deps = edt.getDependenciesAsVector();

  for (auto [idx, blockArg] : llvm::enumerate(edtBlock.getArguments())) {
    if (idx < deps.size()) {
      Value externalDep = deps[idx];
      if (auto parentArg = externalDep.dyn_cast<BlockArgument>()) {
        scopeMapping.map(parentArg, blockArg);
        ARTS_DEBUG("   - Mapping parent block arg to local");
      }
      if (auto acquireOp = externalDep.getDefiningOp<DbAcquireOp>()) {
        scopeMapping.map(acquireOp.getPtr(), blockArg);
        if (Value srcPtr = acquireOp.getSourcePtr())
          scopeMapping.map(srcPtr, blockArg);
        ARTS_DEBUG("   - Mapping acquire result to local block arg");
      }
    }
  }

  /// Rewrite each tracked operation with DbRefOp pattern
  for (Operation *op : operations) {
    ARTS_DEBUG(" - Rewriting operation: " << *op);
    OpBuilder::InsertionGuard ig(AC->getBuilder());
    AC->setInsertionPoint(op);

    /// Apply scope mapping to operands before rewriting
    for (OpOperand &operand : op->getOpOperands()) {
      Value mappedVal = scopeMapping.lookupOrDefault(operand.get());
      if (mappedVal != operand.get()) {
        operand.set(mappedVal);
        ARTS_DEBUG("   - Remapped operand from outer scope to inner scope");
      }
    }

    if (plan.mode == PartitionMode::coarse) {
      /// Coarse mode: db_ref index must be constant zero.
      PartitionInfo coarseInfo;
      coarseInfo.mode = PartitionMode::coarse;
      DbElementWiseIndexer indexer(coarseInfo, /*outerRank=*/0, innerRank, {});
      llvm::SetVector<Operation *> localOpsToRemove;
      indexer.transformOps({op}, localAcquireView, elementMemRefType, *AC,
                           localOpsToRemove);
      for (Operation *toRemove : localOpsToRemove)
        opsToRemove.insert(toRemove);
      continue;
    }

    if (plan.mode == PartitionMode::block) {
      /// Block mode: Use DbBlockIndexer with stored block size
      ARTS_DEBUG(" - Using DbBlockIndexer with stored plan");
      Location loc = op->getLoc();
      unsigned chunkOuterRank = plan.outerRank();
      if (chunkOuterRank == 0)
        if (auto acquireOp = dyn_cast<DbAcquireOp>(dbOp))
          chunkOuterRank = acquireOp.getSizes().size();

      SmallVector<Value> blockSizes;
      SmallVector<Value> startBlocks;
      blockSizes.reserve(plan.blockSizes.size());
      startBlocks.reserve(plan.blockSizes.size());

      Value zero = AC->createIndexConstant(0, loc);
      for (auto [dimIdx, blockSize] : llvm::enumerate(plan.blockSizes)) {
        if (!blockSize)
          continue;
        blockSizes.push_back(blockSize);
        Value startOffset =
            dimIdx < acquireOffsets.size() ? acquireOffsets[dimIdx] : zero;
        startBlocks.push_back(
            AC->create<arith::DivUIOp>(loc, startOffset, blockSize));
      }

      if (blockSizes.empty()) {
        Value fallbackBlockSize = plan.getBlockSize(0);
        if (fallbackBlockSize) {
          blockSizes.push_back(fallbackBlockSize);
          Value startOffset = acquireOffsets.empty() ? zero : acquireOffsets[0];
          startBlocks.push_back(
              AC->create<arith::DivUIOp>(loc, startOffset, fallbackBlockSize));
        }
      }

      /// Build PartitionInfo from stored N-D block plan.
      PartitionInfo blockInfo;
      blockInfo.mode = PartitionMode::block;
      blockInfo.sizes.assign(blockSizes.begin(), blockSizes.end());
      blockInfo.partitionedDims.assign(plan.partitionedDims.begin(),
                                       plan.partitionedDims.end());
      DbBlockIndexer indexer(blockInfo, startBlocks, chunkOuterRank, innerRank);
      llvm::SetVector<Operation *> localOpsToRemove;
      indexer.transformOps({op}, localAcquireView, elementMemRefType, *AC,
                           localOpsToRemove);
      for (Operation *toRemove : localOpsToRemove)
        opsToRemove.insert(toRemove);
      continue;
    }

    /// ElementWise mode
    Value elemOffset = AC->createIndexConstant(0, op->getLoc());

    /// Build PartitionInfo from elemOffset
    PartitionInfo ewInfo;
    ewInfo.mode = PartitionMode::fine_grained;
    ewInfo.indices.push_back(elemOffset);
    DbElementWiseIndexer indexer(ewInfo, plan.outerRank(), innerRank, {});

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      auto localized = indexer.localizeForFineGrained(
          load.getIndices(), acquireIndices, acquireOffsets, AC->getBuilder(),
          op->getLoc());
      auto dbRef =
          AC->create<DbRefOp>(op->getLoc(), elementMemRefType, localAcquireView,
                              localized.dbRefIndices);
      auto newLoad = AC->create<memref::LoadOp>(op->getLoc(), dbRef,
                                                localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(op);
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      auto localized = indexer.localizeForFineGrained(
          store.getIndices(), acquireIndices, acquireOffsets, AC->getBuilder(),
          op->getLoc());
      auto dbRef =
          AC->create<DbRefOp>(op->getLoc(), elementMemRefType, localAcquireView,
                              localized.dbRefIndices);
      AC->create<memref::StoreOp>(op->getLoc(), store.getValue(), dbRef,
                                  localized.memrefIndices);
      opsToRemove.insert(op);
    } else {
      llvm::SetVector<Operation *> localOpsToRemove;
      indexer.transformOps({op}, localAcquireView, elementMemRefType, *AC,
                           localOpsToRemove);
      for (Operation *toRemove : localOpsToRemove)
        opsToRemove.insert(toRemove);
    }
  }
}

///===----------------------------------------------------------------------===///
/// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDbsPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<CreateDbsPass>(AM);
}
} // namespace arts
} // namespace mlir
