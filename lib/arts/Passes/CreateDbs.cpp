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
//==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/ArtsAnalysisManager.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Transforms/DbTransforms.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include <optional>

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
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

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Helper Functions
///===----------------------------------------------------------------------===///
namespace {

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///

struct CreateDbsPass : public arts::CreateDbsBase<CreateDbsPass> {
  CreateDbsPass(ArtsAnalysisManager *AM) : AM(AM) {
    assert(AM && "ArtsAnalysisManager must be provided externally");
  }

  void runOnOperation() override;

private:
  ModuleOp module;
  ArtsAnalysisManager *AM = nullptr;
  DenseMap<Operation *, Operation *> dbPtrToOriginalAlloc;
  SetVector<Operation *> opsToRemove;

  /// Consolidated structure to track all usage information for a memref
  struct MemrefInfo {
    Operation *alloc = nullptr;
    EdtOp parentEdt = nullptr;
    ArtsMode accessMode = ArtsMode::uninitialized;
    bool usedFineGrained = false;
    DbAllocOp dbAllocOp = nullptr;

    /// Structure to track a specific DB access (mode + indices + chunks)
    struct DbDep {
      ArtsMode mode;
      SmallVector<Value, 4> indices, offsets, sizes;
      SmallVector<Operation *, 4> operations;
    };

    /// Map from EDT operation to all DB accesses for that EDT
    DenseMap<EdtOp, SmallVector<DbDep>> edtToDeps;

    /// Add an access for this memref and update combined mode
    void addAccess(EdtOp edtOp, ArtsMode mode, SmallVector<Value, 4> indices,
                   SmallVector<Value, 4> offsets = {},
                   SmallVector<Value, 4> sizes = {},
                   SmallVector<Operation *, 4> operations = {}) {
      accessMode = combineAccessModes(accessMode, mode);
      edtToDeps[edtOp].push_back({mode, indices, offsets, sizes, operations});
    }

    /// Compute index pattern information from stored accesses
    struct AccessPatternInfo {
      /// Whether all accesses have the same number of indices
      bool isConsistent = true;
      /// Whether all accesses have indices
      bool allAccessesHaveIndices = true;
      /// Minimum number of pinned dimensions across all accesses
      unsigned pinnedDimCount = 0;
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
        }
      }

      return info;
    }
  };

  /// Map from memref value to its consolidated usage information
  DenseMap<Operation *, MemrefInfo> memrefInfo;

  /// Map an edtOp to its external values
  DenseMap<EdtOp, SetVector<Value>> edtExternalValues;

  /// Helper functions
  void collectMemrefs();
  void collectControlDbOps();
  void createDbAllocOps();

  /// Helper functions for collectControlDbOps
  void createDbAcquireOps(EdtOp edt, SetVector<Value> &externalDeps);

  /// Checks if the parent EDT already has an acquired handle for the given
  /// DbAllocOp.
  Value findParentAcquireSource(EdtOp edt, Operation *dbAllocOp,
                                bool requiresIndexedAccess = false);

  /// Infer allocation type from the defining operation
  DbAllocType inferAllocType(Operation *alloc);

  /// Insert DbFreeOp for a DbAllocOp at the appropriate location
  void insertDbFreeForDbAlloc(DbAllocOp dbAlloc, Operation *alloc,
                              OpBuilder &builder);

  /// Rewrite uses of datablocks
  void rewriteUsesInEdt(EdtOp edt, SmallVector<Operation *> &operations,
                        Operation *dbOp, SmallVector<Value> &acquireIndices,
                        SmallVector<Value> &acquireOffsets,
                        BlockArgument dbAcquireArg);
  void rewriteUsesInParentEdt(MemrefInfo &memrefInfo);
  void rewriteUsesEverywhere(Operation *alloc, DbAllocOp dbAlloc);
};
} // namespace

///===----------------------------------------------------------------------===///
// Pass Entry Point
///===----------------------------------------------------------------------===///
void CreateDbsPass::runOnOperation() {
  module = getOperation();
  opsToRemove.clear();
  memrefInfo.clear();

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
      Operation *underlyingOp = arts::getUnderlyingOperation(externalDep);
      if (!underlyingOp)
        continue;

      MemrefInfo &info = memrefInfo[underlyingOp];

      /// Check if this EDT has any DbControlOps for this memref
      bool hasControlDb = !info.edtToDeps[edt].empty();
      if (!hasControlDb) {
        /// No DbControlOps for this memref in this EDT - set to inout
        ARTS_DEBUG(
            " - Memref "
            << *underlyingOp
            << " used in EDT without DbControlOps, setting to inout mode");
        info.accessMode = combineAccessModes(info.accessMode, ArtsMode::inout);
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
  OpRemovalManager removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked(module, /*recursive=*/true);

  /// Phase 6: Enabling verification for EDTs
  module.walk([](EdtOp edt) { edt.removeNoVerifyAttr(); });

  /// Phase 7: Simplify the module
  DominanceInfo domInfo(module);
  arts::simplifyIR(module, domInfo);

  ARTS_INFO_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
// Allocation Type Inference
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
// Collect Allocations Used in EDTs
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
        Operation *underlyingOp = arts::getUnderlyingOperation(operand);
        if (!underlyingOp) {
          if (auto load = operand.getDefiningOp<memref::LoadOp>())
            underlyingOp = arts::getUnderlyingOperation(load.getMemref());
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
        if (parentEdt != edt)
          edtExternalValues[edt].insert(operand);
      }
    });
  });
}

///===----------------------------------------------------------------------===///
// Collect Control DB Operations
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
    Operation *underlyingOp = arts::getUnderlyingOperation(ptr);
    if (!underlyingOp)
      return;

    ARTS_DEBUG(" - Found DbControlOp for "
               << *underlyingOp << " with mode: " << mode << ", "
               << indices.size() << " indices, " << offsets.size()
               << " offsets, " << sizes.size() << " sizes");

    /// Get user EDT
    EdtOp userEdt = nullptr;
    for (Operation *user : dbControl.getResult().getUsers()) {
      if (auto edt = dyn_cast<EdtOp>(user)) {
        userEdt = edt;
        break;
      }
    }

    SmallVector<Value, 4> indexValues(indices.begin(), indices.end());
    SmallVector<Value, 4> offsetValues(offsets.begin(), offsets.end());
    SmallVector<Value, 4> sizeValues(sizes.begin(), sizes.end());

    /// Track all operations in this EDT that use this memref AND match the
    /// indices
    SmallVector<Operation *, 4> opsToRewrite;
    if (userEdt) {
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

      /// TODO: Implement affine/symbolic analysis for better index matching
      /// Current limitation: SSA value equality fails when semantically
      /// equivalent but syntactically different values are used (e.g., loop IVs
      /// vs. external values). Future approaches:
      /// 1. Affine expression analysis to determine symbolic equivalence
      /// 2. Value tracking through block arguments and control flow
      /// 3. Polyhedral analysis for complex access patterns
      /// For now, we rely on coarse-grained fallback when strict matching
      /// fails.

      /// Walk the user EDT and collect operations that match the indices
      userEdt.walk([&](Operation *op) {
        if (auto load = dyn_cast<memref::LoadOp>(op)) {
          if (arts::getUnderlyingOperation(load.getMemref()) == underlyingOp) {
            if (indicesMatchPrefix(load.getIndices()))
              opsToRewrite.push_back(op);
          }
        } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
          if (arts::getUnderlyingOperation(store.getMemref()) == underlyingOp) {
            if (indicesMatchPrefix(store.getIndices()))
              opsToRewrite.push_back(op);
          }
        }
      });
      ARTS_DEBUG("    - Found "
                 << opsToRewrite.size()
                 << " operations to rewrite for this specific dependency");
    }

    memrefInfo[underlyingOp].addAccess(userEdt, mode, indexValues, offsetValues,
                                       sizeValues, opsToRewrite);
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

///===----------------------------------------------------------------------===///
// Create DB Allocation Operations
///===----------------------------------------------------------------------===///
void CreateDbsPass::createDbAllocOps() {
  OpBuilder builder(module.getContext());
  /// All memrefs in memrefInfo are converted to DBs
  for (auto &entry : memrefInfo) {
    Operation *alloc = entry.first;
    ARTS_DEBUG("Creating DB Alloc Op for memref " << *alloc);
    MemrefInfo &info = entry.second;
    Location loc = alloc->getLoc();
    builder.setInsertionPointAfter(alloc);

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

    /// Determine allocation granularity based on index access pattern
    /// - If all accesses use a consistent dimension, allocate fine-grained DBs
    /// - Otherwise, use coarse-grained allocation (single DB with full memref)
    SmallVector<Value> sizes, elementSizes;
    const bool isRankZero = memRefType.getRank() == 0;
    const unsigned rank = std::max<unsigned>(1, memRefType.getRank());
    const auto accessPatternInfo = info.getAccessPatternInfo();
    bool useFineGrainedAllocation = !isRankZero &&
                                    accessPatternInfo.isConsistent &&
                                    accessPatternInfo.pinnedDimCount > 0 &&
                                    accessPatternInfo.allAccessesHaveIndices;

    if (useFineGrainedAllocation) {
      info.usedFineGrained = true;
      const auto pinnedDimCount = accessPatternInfo.pinnedDimCount;

      /// All indexed dimensions go into sizes (outer array
      /// dimensions) Non-indexed dimensions go into elementSizes (inner
      /// datablock dimensions)
      ///
      /// Example: memref<?x?x?xf64> with indices [i,j] (indexing dims 0,1)
      ///   sizes=[N,M], elementSizes=[K] -> memref<?x?xmemref<?xf64>>
      ///   Acquire: indices=[i,j] -> selects one memref<?xf64> datablock
      ///
      /// This allows fine-grained element-level distribution based on all
      /// indexed dimensions, with db_acquire using indices to directly select
      /// datablocks from the N-dimensional outer array.

      ARTS_DEBUG(" - Using fine-grained allocation with "
                 << pinnedDimCount << " pinned dimensions");

      /// Add all pinned dimensions to sizes
      for (unsigned dim = 0; dim < pinnedDimCount; ++dim) {
        if (memRefType.isDynamicDim(dim)) {
          sizes.push_back(
              builder.create<arts::DbDimOp>(loc, allocValue, (int64_t)dim));
        } else {
          sizes.push_back(builder.create<arith::ConstantIndexOp>(
              loc, memRefType.getDimSize(dim)));
        }
      }

      /// All non-pinned dimensions go into elementSizes
      for (unsigned i = pinnedDimCount; i < rank; ++i) {
        if (memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              builder.create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          elementSizes.push_back(builder.create<arith::ConstantIndexOp>(
              loc, memRefType.getDimSize(i)));
        }
      }

      ARTS_DEBUG("   - Created fine-grained DB: Outer rank="
                 << sizes.size() << ", Inner rank=" << elementSizes.size());
    } else {
      /// Coarse-grained: all dimensions go to elementSizes, single DB. For
      /// rank-0 memrefs, model them as a 1-D memref of size 1.
      ARTS_DEBUG(" - Falling back to coarse-grained allocation");
      elementSizes.reserve(rank);
      for (unsigned i = 0; i < rank; ++i) {
        if (!isRankZero && memRefType.isDynamicDim(i)) {
          elementSizes.push_back(
              builder.create<arts::DbDimOp>(loc, allocValue, (int64_t)i));
        } else {
          int64_t dimSize =
              isRankZero ? 1 : static_cast<int64_t>(memRefType.getDimSize(i));
          elementSizes.push_back(
              builder.create<arith::ConstantIndexOp>(loc, dimSize));
        }
      }

      /// Single datablock
      sizes.push_back(builder.create<arith::ConstantIndexOp>(loc, 1));
    }

    /// Create the db_alloc operation
    auto route = builder.create<arith::ConstantIntOp>(loc, 0, 32);
    auto dbAllocOp = builder.create<DbAllocOp>(
        loc, mode, route, allocType, dbMode, elementType, sizes, elementSizes);

    /// Transfer metadata from original allocation to DbAllocOp
    if (AM->getMetadataManager().transferMetadata(alloc, dbAllocOp)) {
      ARTS_DEBUG(
          "  Transferred metadata from original allocation to DbAllocOp");
    } else {
      ARTS_DEBUG("  No metadata found for original allocation");
    }

    /// Store mappings for later use
    info.dbAllocOp = dbAllocOp;
    dbPtrToOriginalAlloc[dbAllocOp] = alloc;

    /// Insert DbFreeOp and remove associated dealloc operations
    insertDbFreeForDbAlloc(dbAllocOp, alloc, builder);

    /// Rewire uses of the original allocation to use the db_alloc operation.
    /// If the allocation has a defining EDT, rewrite within that EDT. If it
    /// lives outside any EDT (host/global), rewrite all uses so host code sees
    /// the datablock contents as well.
    if (info.parentEdt)
      rewriteUsesInParentEdt(info);
    else
      rewriteUsesEverywhere(alloc, dbAllocOp);
  }
}

/// Find parent scope's acquired handle for a datablock
/// Checks if the parent EDT already has an acquired handle for the given
/// DbAllocOp.
Value CreateDbsPass::findParentAcquireSource(EdtOp edt, Operation *dbAllocOp,
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

    Operation *db = arts::getUnderlyingDbAlloc(v);
    if (!db || db != dbAllocOp)
      return false;

    /// If the caller intends to index into this handle, avoid reusing a
    /// single-element datablock view.
    if (requiresIndexedAccess) {
      if (Operation *dbOp = arts::getUnderlyingDb(v)) {
        if (dbHasSingleSize(dbOp))
          return false;
      }
    }

    /// Avoid treating the DbAllocOp results themselves as reusable handles
    if (auto *defOp = v.getDefiningOp())
      if (isa<DbAllocOp>(defOp))
        return false;

    return true;
  };

  /// Check parent EDT's block arguments
  for (BlockArgument arg : parentBlock->getArguments()) {
    if (matchesDb(arg)) {
      ARTS_DEBUG("   - Found matching handle in parent block arguments");
      return arg;
    }
  }

  ARTS_DEBUG("   - No existing handle found in parent scope");
  return nullptr;
}

///===----------------------------------------------------------------------===///
// Process EDT Dependencies
/// For each EDT, analyze external memory dependencies and insert
/// acquire/release
///===----------------------------------------------------------------------===///
void CreateDbsPass::createDbAcquireOps(EdtOp edt,
                                       SetVector<Value> &externalDeps) {
  OpBuilder builder(module.getContext());
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
    Operation *underlyingOp = arts::getUnderlyingOperation(externalDep);
    if (!underlyingOp)
      if (auto load = externalDep.getDefiningOp<memref::LoadOp>())
        underlyingOp = arts::getUnderlyingOperation(load.getMemref());
    assert(underlyingOp && "Underlying operation not found");

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
    auto sourceSizes = dbAllocOp.getSizes();
    assert((sourceGuid && sourcePtr) && "Source guid and ptr must be non-null");

    /// Create acquire operation (pass both source guid and ptr)
    builder.setInsertionPoint(edt);

    /// We use fine-grained acquire if the allocation was also fine-grained
    /// AND this EDT has dependencies for this memref
    const auto &deps = info.edtToDeps[edt];
    if (info.usedFineGrained && !deps.empty()) {
      /// Fine-grained acquire: Multiple separate acquires for stencil patterns
      /// Example: A stencil accessing u[i-1], u[i], u[i+1] creates 3 acquires
      ARTS_DEBUG(" - Using fine-grained acquire with " << deps.size()
                                                       << " dependencies");

      for (const auto &dep : deps) {
        const auto &depIndices = dep.indices;
        const auto &depOffsets = dep.offsets;

        SmallVector<Value> acquireIndices(depIndices.begin(), depIndices.end());
        SmallVector<Value> acquireOffsets(depOffsets.begin(), depOffsets.end());
        assert(sourceSizes.size() >= depIndices.size() &&
               "Sizes must be greater than or equal to indices");
        uint64_t acquireRank = sourceSizes.size() - depIndices.size();

        /// Source guid and ptr are the source of the acquire
        Value acqGuid = sourceGuid;
        Value acqPtr = sourcePtr;

        /// Check if there is a parent acquired handle for this memref
        if (Value availableHandle =
                findParentAcquireSource(edt, dbAllocOp, !depIndices.empty())) {
          ARTS_DEBUG("   - Reusing existing datablock handle in parent scope");
          acqGuid = Value();
          acqPtr = availableHandle;
        }

        SmallVector<Value> acquireSizes;
        for (size_t i = 0; i < acquireRank; ++i) {
          acquireSizes.push_back(
              builder.create<arith::ConstantIndexOp>(edt.getLoc(), 1));
        }
        auto acquireOp = builder.create<DbAcquireOp>(
            edt.getLoc(), dep.mode, acqGuid, acqPtr, acquireIndices,
            acquireOffsets, acquireSizes, /*offsetHints=*/SmallVector<Value>{},
            /*sizeHints=*/SmallVector<Value>{}, /*boundsValid=*/Value());

        /// Twin-diff decision for fine-grained acquires:
        /// When DbControlOps provide indexed access patterns (from OpenMP
        /// depend clauses like `depend(inout: A[i])`), each EDT acquires a
        /// DISTINCT element of the datablock. This guarantees isolation
        /// because:
        ///   1. The indices come directly from task dependencies in the source
        ///   2. OpenMP semantics ensure no two concurrent tasks share the same
        ///      depend index
        ///   3. Each EDT has exclusive read/write access to its indexed element
        /// Therefore, twin-diff is NOT needed - we can safely disable it.
        acquireOp.setTwinDiff(false);
        ARTS_DEBUG("   - Fine-grained acquire: twin_diff=false");

        /// Add corresponding block argument for each acquired/reused view
        Value acquirePtr = acquireOp.getPtr();
        auto sourceType = acquirePtr.getType().dyn_cast<MemRefType>();
        BlockArgument dbAcquireArg =
            edt.getBody().front().addArgument(sourceType, edt.getLoc());
        dependencyOperands.push_back(acquirePtr);

        /// Rewrite uses in edt - pass tracked operations and offsets for chunk
        /// handling
        SmallVector<Operation *> opsToRewrite(dep.operations.begin(),
                                              dep.operations.end());
        rewriteUsesInEdt(edt, opsToRewrite, acquireOp, acquireIndices,
                         acquireOffsets, dbAcquireArg);

        /// Insert release for each acquire before EDT terminator
        OpBuilder::InsertionGuard IG(builder);
        builder.setInsertionPoint(edt.getBody().front().getTerminator());
        builder.create<DbReleaseOp>(edt.getLoc(), dbAcquireArg);
      }
      /// We've handled everything for this memref, continue to next
      continue;
    } else {
      /// Coarse-grained acquire: Acquire entire DB as a single block
      ARTS_DEBUG(" - Using coarse-grained acquire");

      /// For coarse-grained acquires, all acquires for the same DbAllocOp have
      /// identical parameters (offsets=[0], sizes=[1]).
      if (!processedCoarseAllocs.insert(dbAllocOp.getOperation()).second) {
        ARTS_DEBUG("   - Skipping duplicate coarse-grained acquire for "
                   "already-processed DbAllocOp");
        continue;
      }

      SmallVector<Value> acquireIndices, acquireOffsets, acquireSizes;
      acquireOffsets.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0));
      acquireSizes.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 1));

      /// Source guid and ptr are the source of the acquire
      Value acqGuid = sourceGuid;
      Value acqPtr = sourcePtr;

      /// Check if there is a parent acquired handle for this memref
      if (Value availableHandle = findParentAcquireSource(edt, dbAllocOp)) {
        ARTS_DEBUG(" - Reusing existing datablock handle in parent scope");
        acqGuid = Value();
        acqPtr = availableHandle;
      }

      /// Use the dependency mode from DbControlOp if available
      ArtsMode acquireMode = ArtsMode::inout;
      auto acquireOp = builder.create<DbAcquireOp>(
          edt.getLoc(), acquireMode, acqGuid, acqPtr, acquireIndices,
          acquireOffsets, acquireSizes);

      /// Twin-diff decision for coarse-grained acquires:
      /// When we fall back to coarse-grained allocation, we CANNOT guarantee
      /// that EDTs access disjoint regions of the datablock. This happens when:
      ///   1. No DbControlOps were provided (no OpenMP depend clauses)
      ///   2. Access patterns are inconsistent across EDTs
      ///   3. Some EDTs need full array access without specific indices
      ///
      /// In these cases, we MUST enable twin-diff because:
      ///   - Multiple workers may write to overlapping regions
      ///   - Without twin-diff, concurrent writes cause DATA CORRUPTION
      ///   - The runtime twin-diff mechanism safely merges partial updates
      ///
      /// The Db.cpp pass may later disable twin-diff if it can prove
      /// non-overlap through successful partitioning or alias analysis.
      acquireOp.setTwinDiff(true);
      ARTS_DEBUG("   - Coarse-grained acquire: twin_diff=true (conservative, "
                 "potential overlap)");

      /// Add corresponding block argument for the acquired/reused view
      Value acquirePtr = acquireOp.getPtr();
      auto sourceType = acquirePtr.getType().dyn_cast<MemRefType>();
      BlockArgument dbAcquireArg =
          edt.getBody().front().addArgument(sourceType, edt.getLoc());
      dependencyOperands.push_back(acquirePtr);

      /// Collect all operations from all deps for coarse-grained rewrite
      SmallVector<Operation *> opsToRewrite;
      llvm::SmallPtrSet<Operation *, 16> seenOps;
      auto addOpIfNew = [&](Operation *op) {
        if (!op)
          return;
        if (seenOps.insert(op).second)
          opsToRewrite.push_back(op);
      };

      bool missingOpsForDeps = false;
      const auto &deps = info.edtToDeps[edt];
      for (const auto &dep : deps) {
        if (dep.operations.empty())
          missingOpsForDeps = true;
        for (Operation *op : dep.operations)
          addOpIfNew(op);
      }

      /// If we failed to match operations for any dependency (SSA mismatch)
      /// or if none were found, conservatively rewrite all uses of the memref
      /// within this EDT.
      if (opsToRewrite.empty() || missingOpsForDeps) {
        ARTS_DEBUG(" - Falling back: collecting all operations using this "
                   "memref in the EDT");
        edt.walk([&](Operation *op) {
          /// Skip operations that belong to nested EDTs - they will be
          /// processed when their containing EDT is handled
          if (op->getParentOfType<EdtOp>() != edt)
            return;

          for (Value operand : op->getOperands()) {
            Operation *operandUnderlyingOp =
                arts::getUnderlyingOperation(operand);
            if (operandUnderlyingOp == underlyingOp) {
              addOpIfNew(op);
              break;
            }
          }
        });
        ARTS_DEBUG(" - Found " << opsToRewrite.size()
                               << " operations to rewrite");
      }

      if (!opsToRewrite.empty()) {
        rewriteUsesInEdt(edt, opsToRewrite, acquireOp, acquireIndices,
                         acquireOffsets, dbAcquireArg);
      }

      /// Insert release for this acquire before EDT terminator
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(edt.getBody().front().getTerminator());
      builder.create<DbReleaseOp>(edt.getLoc(), dbAcquireArg);
    }
  }

  /// After processing all memrefs, set EDT dependencies
  edt.setDependencies(dependencyOperands);
}

///===----------------------------------------------------------------------===///
// Insert DB Free Operations
/// Insert db_free for DbAllocOp at the appropriate location and remove
/// associated dealloc operations
///===----------------------------------------------------------------------===///
void CreateDbsPass::insertDbFreeForDbAlloc(DbAllocOp dbAlloc, Operation *alloc,
                                           OpBuilder &builder) {
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
  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(insertionPoint);
  builder.create<DbFreeOp>(loc, dbAlloc.getGuid());
  builder.create<DbFreeOp>(loc, dbAlloc.getPtr());
}

///===----------------------------------------------------------------------===///
/// Rewrite Alloc uses
/// Adjust load/store indices based on DB allocation granularity.
///===----------------------------------------------------------------------===///
void CreateDbsPass::rewriteUsesInParentEdt(MemrefInfo &memrefInfo) {
  assert(memrefInfo.dbAllocOp && "No DbAllocOp found");

  auto dbAlloc = memrefInfo.dbAllocOp;
  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  /// Collect users in the parent edt
  SmallVector<Operation *, 8> users;
  for (auto &use : memrefInfo.alloc->getUses()) {
    Operation *user = use.getOwner();
    if (user->getParentOfType<EdtOp>() == memrefInfo.parentEdt)
      users.push_back(user);
  }

  ARTS_DEBUG(" - Rewriting " << users.size() << " operations in parent EDT");

  OpBuilder builder(module.getContext());
  DbTransforms transforms;

  /// Coarse-grained: db_ref[0] + load/store[indices]
  /// Fine-grained: db_ref[indices] + load/store[0]
  bool isCoarse = DbTransforms::isCoarseGrained(dbAlloc);
  unsigned outerCount = isCoarse ? 0 : dbAlloc.getSizes().size();
  for (Operation *user : users)
    transforms.rewriteAccessWithDbPattern(user, dbAlloc.getPtr(),
                                          elementMemRefType, outerCount,
                                          builder, opsToRemove);
}

/// Rewrite all uses of an allocation to point to the new db_alloc pointer. This
/// is used when the allocation is not inside an EDT, but is still shared with
/// EDTs and the host needs to see the updated data.
void CreateDbsPass::rewriteUsesEverywhere(Operation *alloc, DbAllocOp dbAlloc) {
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

  OpBuilder builder(module.getContext());
  DbTransforms transforms;

  /// Coarse-grained: db_ref[0] + load/store[indices]
  /// Fine-grained: db_ref[indices] + load/store[0]
  bool isCoarse = DbTransforms::isCoarseGrained(dbAlloc);
  unsigned outerCount = isCoarse ? 0 : dbAlloc.getSizes().size();
  for (Operation *user : users)
    transforms.rewriteAccessWithDbPattern(user, dbAlloc.getPtr(),
                                          elementMemRefType, outerCount,
                                          builder, opsToRemove);
}

///===----------------------------------------------------------------------===///
// Rewrite Uses in EDT
/// Rewrite loads/stores in an EDT to use acquired datablocks
/// Finds all operations that use external values and match the acquire indices
///===----------------------------------------------------------------------===///
void CreateDbsPass::rewriteUsesInEdt(EdtOp edt,
                                     SmallVector<Operation *> &operations,
                                     Operation *dbOp,
                                     SmallVector<Value> &acquireIndices,
                                     SmallVector<Value> &acquireOffsets,
                                     BlockArgument dbAcquireArg) {
  ARTS_DEBUG(" - Rewriting " << operations.size() << " operations in EDT");

  /// Get the element type from the acquired datablock
  /// For fine-grained: memref<?xmemref<?xT>> -> extract memref<?xT>
  /// The outer dimension is the datablock index, inner is the element memref
  Type elementMemRefType = dbAcquireArg.getType();
  if (auto outerMemrefType = elementMemRefType.dyn_cast<MemRefType>()) {
    if (auto innerMemrefType =
            outerMemrefType.getElementType().dyn_cast<MemRefType>()) {
      /// Fine-grained: outer type is memref<?xmemref<?xT>>
      /// Extract the element memref type: memref<?xT>
      elementMemRefType = innerMemrefType;
    }
  }

  /// Build value mapping from parent EDT block args/acquires to this EDT's args
  /// This ensures operations inside this EDT use the correct scoped values
  IRMapping scopeMapping;
  Block &edtBlock = edt.getBody().front();
  auto deps = edt.getDependenciesAsVector();

  for (auto [idx, blockArg] : llvm::enumerate(edtBlock.getArguments())) {
    if (idx < deps.size()) {
      Value externalDep = deps[idx];

      /// Map parent block args -> this EDT's block args
      if (auto parentArg = externalDep.dyn_cast<BlockArgument>()) {
        scopeMapping.map(parentArg, blockArg);
        ARTS_DEBUG("   - Mapping parent block arg to local: "
                   << parentArg << " -> " << blockArg);
      }

      /// Map acquire ptr results -> this EDT's block args
      if (auto acquireOp = externalDep.getDefiningOp<DbAcquireOp>()) {
        scopeMapping.map(acquireOp.getPtr(), blockArg);
        /// Also map the source ptr of the acquire for chained acquires
        if (Value srcPtr = acquireOp.getSourcePtr())
          scopeMapping.map(srcPtr, blockArg);
        ARTS_DEBUG("   - Mapping acquire result to local block arg");
      }
    }
  }

  /// Rewrite each tracked operation with DbRefOp pattern
  OpBuilder builder(edt.getContext());
  DbTransforms transforms;

  for (Operation *op : operations) {
    /// Apply scope mapping to operands before rewriting
    for (OpOperand &operand : op->getOpOperands()) {
      Value mappedVal = scopeMapping.lookupOrDefault(operand.get());
      if (mappedVal != operand.get()) {
        operand.set(mappedVal);
        ARTS_DEBUG("   - Remapped operand from outer scope to inner scope");
      }
    }

    if (acquireOffsets.empty() && acquireIndices.empty()) {
      unsigned outerCount = 0;
      if (auto load = dyn_cast<memref::LoadOp>(op))
        outerCount = load.getIndices().size();
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        outerCount = store.getIndices().size();
      transforms.rewriteAccessWithDbPattern(op, dbAcquireArg, elementMemRefType,
                                            outerCount, builder, opsToRemove);
    } else if (acquireIndices.empty()) {
      transforms.rewriteAccessWithDbPattern(op, dbAcquireArg, elementMemRefType,
                                            0, builder, opsToRemove);
    } else if (auto acquireOp = dyn_cast<DbAcquireOp>(dbOp)) {
      transforms.rebaseToAcquireView(op, acquireOp, dbAcquireArg,
                                     elementMemRefType, builder, opsToRemove);
    }
  }
}

///===----------------------------------------------------------------------===///
// Pass creation
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDbsPass(ArtsAnalysisManager *AM) {
  return std::make_unique<CreateDbsPass>(AM);
}
} // namespace arts
} // namespace mlir
