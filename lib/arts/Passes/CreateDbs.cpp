///==========================================================================
/// File: CreateDbs.cpp
///
/// This pass creates ARTS Datablocks (DB) operations to handle memory
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
/// ----------------------------------------------------------------
/// Special Case: Nested Array Allocations (Multi-Dimensional Arrays)
/// This pass handles a common C/C++ pattern where multi-dimensional arrays
/// are allocated as arrays of pointers, with each pointer allocated separately:
/// Example:
///   double **A = malloc(N * sizeof(double*));
///   for (int i = 0; i < N; i++)
///     A[i] = malloc(M * sizeof(double));
///
/// This pattern translates to MLIR as:
///   %outer = memref.alloc(%N) : memref<?xmemref<?xf64>>
///   scf.for %i = 0 to %N {
///     %inner = memref.alloc(%M) : memref<?xf64>
///     memref.store %inner, %outer[%i]
///   }
///
/// Instead of creating separate datablocks for each row, this pass:
/// 1. Detects the nested allocation pattern
/// 2. Creates a single N-dimensional datablock (e.g., memref<?x?xf64>)
/// 3. Transforms cascaded memory accesses (load(load(A[i])[j])) into
///    direct N-dimensional access (load(A_ND[i,j]))
/// 4. Removes the initialization loop that allocated individual rows
///
///
/// This pass assumes the worst-case scenario for access modes, always using
/// inout.
//==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/StringAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(create_dbs);

using namespace mlir;
using namespace mlir::arts;

/// Dependency information for Datablocks acquisition.
/// Contains the access mode and slice parameters for acquiring a memory region.
struct DepInfo {
  ArtsMode mode;
  SmallVector<Value> offsets;
  SmallVector<Value> sizes;
  MemRefType resultType;
};

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//
namespace {

/// Strip numeric cast operations to find the underlying value.
/// Traverses through index casts, sign/zero extensions, and truncations.
static Value stripNumericCasts(Value value) {
  while (true) {
    if (auto idxCast = value.getDefiningOp<arith::IndexCastOp>()) {
      value = idxCast.getIn();
      continue;
    }
    if (auto ext = value.getDefiningOp<arith::ExtSIOp>()) {
      value = ext.getIn();
      continue;
    }
    if (auto ext = value.getDefiningOp<arith::ExtUIOp>()) {
      value = ext.getIn();
      continue;
    }
    if (auto trunc = value.getDefiningOp<arith::TruncIOp>()) {
      value = trunc.getIn();
      continue;
    }
    break;
  }
  return value;
}

/// Check if two values represent equivalent scaling factors.
/// Used to recognize patterns like (N * sizeof(T)) / sizeof(T) -> N.
static bool scalesAreEquivalent(Value lhs, Value rhs) {
  Value a = stripNumericCasts(lhs);
  Value b = stripNumericCasts(rhs);
  if (a == b)
    return true;

  auto constantValue = [](Value v) -> std::optional<int64_t> {
    if (auto cIdx = v.getDefiningOp<arith::ConstantIndexOp>())
      return cIdx.value();
    if (auto cInt = v.getDefiningOp<arith::ConstantIntOp>())
      return cInt.value();
    return std::nullopt;
  };

  if (auto lhsConst = constantValue(a))
    if (auto rhsConst = constantValue(b))
      return lhsConst == rhsConst;

  if (auto lhsType = a.getDefiningOp<polygeist::TypeSizeOp>())
    if (auto rhsType = b.getDefiningOp<polygeist::TypeSizeOp>())
      return lhsType.getType() == rhsType.getType();

  return false;
}

/// Cast a value to index type if needed.
static Value castToIndex(Value value, OpBuilder &builder, Location loc) {
  if (!value)
    return value;
  if (value.getType().isIndex())
    return value;
  if (value.getType().isIntOrIndex())
    return builder.create<arith::IndexCastOp>(loc, builder.getIndexType(),
                                              value);
  return value;
}

/// Extract original size from (N * scale) / scale pattern.
/// Common in malloc size calculations: malloc(N * sizeof(T)) / sizeof(T) -> N.
static Value extractOriginalSize(Value numerator, Value denominator,
                                 OpBuilder &builder, Location loc) {
  Value stripped = stripNumericCasts(numerator);
  if (auto mul = stripped.getDefiningOp<arith::MulIOp>()) {
    Value lhs = mul.getLhs();
    Value rhs = mul.getRhs();
    if (scalesAreEquivalent(lhs, denominator))
      return castToIndex(stripNumericCasts(rhs), builder, loc);
    if (scalesAreEquivalent(rhs, denominator))
      return castToIndex(stripNumericCasts(lhs), builder, loc);
  }
  return Value();
}

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

struct CreateDbsPass : public arts::CreateDbsBase<CreateDbsPass> {
  CreateDbsPass(bool identifyDbs) { this->identifyDbs = identifyDbs; }

  void runOnOperation() override;

private:
  bool identifyDbs;
  ModuleOp module;
  DenseMap<Value, DbAllocOp> allocToDbAlloc;
  SmallVector<DbAllocOp, 8> createdDbAllocs;
  StringAnalysis *stringAnalysis;

  /// Core helper functions
  void cleanupExistingDbOps();
  SetVector<Value> collectUsedAllocations();
  void createDbAllocOps(const SetVector<Value> &allocs, OpBuilder &builder);
  SetVector<Value> getEdtExternalValues(EdtOp edt);
  void processEdtDeps(EdtOp edt, OpBuilder &builder);

  /// Infer allocation type from the defining operation
  DbAllocType inferAllocType(Value basePtr);

  /// Pattern recognition for nested allocations
  struct NestedAllocPattern {
    Value outerAlloc;      /// The outermost array allocation for this level
    Value dimSize;         /// Size for this dimension
    Value innerDimSize;    /// Inner size only for base case
    Value innerAllocValue; /// Inner alloc value only for base case
    Type elementType;      /// Final scalar element type (e.g., f64)
    scf::ForOp initLoop;   /// The initialization loop for this level
    std::unique_ptr<NestedAllocPattern> inner; /// Next nested level

    NestedAllocPattern() = default;
    NestedAllocPattern(NestedAllocPattern &&) = default;
    NestedAllocPattern &operator=(NestedAllocPattern &&) = default;
    NestedAllocPattern(const NestedAllocPattern &) = delete;
    NestedAllocPattern &operator=(const NestedAllocPattern &) = delete;

    int getDepth() const { return inner ? 1 + inner->getDepth() : 2; }

    SmallVector<Value> getAllSizes() const {
      SmallVector<Value> s = {dimSize};
      if (inner)
        s.append(inner->getAllSizes());
      else
        s.push_back(innerDimSize);

      return s;
    }

    SmallVector<scf::ForOp> getAllInitLoops() const {
      SmallVector<scf::ForOp> l = {initLoop};
      if (inner)
        l.append(inner->getAllInitLoops());
      return l;
    }

    SmallVector<Value> getAllNestedAllocs() const {
      SmallVector<Value> a = {outerAlloc};
      if (inner)
        a.append(inner->getAllNestedAllocs());
      else if (innerAllocValue)
        a.push_back(innerAllocValue);
      return a;
    }

    Type getFinalElementType() const { return elementType; }
  };

  std::optional<NestedAllocPattern> detectNestedAllocPattern(Value alloc);
  void eraseNestedAllocInits(NestedAllocPattern &pattern);
  void transformNestedAccesses(Value outerAlloc, DbAllocOp dbAlloc);

  /// Dependency analysis
  DepInfo computeDepInfo(OpBuilder &builder, EdtOp edt, Value originalValue,
                         Value source);

  /// Access adjustment
  void adjustAccesses(Region &region, Value originalValue, Value view,
                      ArrayRef<Value> offsets);

  /// Extract the logical (pre-malloc) size for a dynamically computed value
  Value getOriginalSizeValue(Value size, OpBuilder &builder, Location loc);

  /// DbAlloc cleanup
  void insertDbFreeOperations(DbAllocOp dbAlloc, OpBuilder &builder);

  /// Helper function to check if a value is related to the original allocation
  bool isRelatedToAllocation(Value value, DbAllocOp dbAlloc);
};
} // namespace

//===----------------------------------------------------------------------===//
// Pass Entry Point
//===----------------------------------------------------------------------===//
void CreateDbsPass::runOnOperation() {
  module = getOperation();
  ARTS_INFO_HEADER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););

  OpBuilder builder(module.getContext());

  /// Run StringAnalysis to detect string memrefs
  stringAnalysis = new StringAnalysis(module);
  stringAnalysis->run();
  assert(stringAnalysis && "StringAnalysis must be created");

  /// Phase 1: Cleanup existing DB operations from previous runs
  ARTS_DEBUG("Phase 1: Cleaning up existing DB operations");
  cleanupExistingDbOps();

  /// Phase 2: Detect nested allocation patterns
  /// Multi-dimensional arrays allocated as arrays-of-pointers are detected
  /// and grouped by their initialization loop for later conversion to N-D DBs
  ARTS_DEBUG("Phase 2: Detecting nested allocation patterns");
  DenseMap<scf::ForOp, SmallVector<NestedAllocPattern>> nestedPatternsByLoop;
  SetVector<Value> nestedAllocs;

  module.walk([&](memref::AllocOp allocOp) {
    Value alloc = allocOp.getResult();
    /// Skip allocations inside loops - these are inner allocations
    if (allocOp->getParentOfType<scf::ForOp>())
      return;
    if (auto pattern = detectNestedAllocPattern(alloc)) {
      nestedPatternsByLoop[pattern->initLoop].push_back(std::move(*pattern));
      nestedAllocs.insert(alloc);
    }
  });

  /// Phase 3: Collect allocations used in EDTs
  /// Separate regular allocations from nested ones for different handling
  ARTS_DEBUG("Phase 3: Collecting allocations used in EDTs");
  SetVector<Value> allUsedAllocs = collectUsedAllocations();
  SetVector<Value> regularAllocs;
  for (Value alloc : allUsedAllocs) {
    if (!nestedAllocs.contains(alloc))
      regularAllocs.insert(alloc);
  }

  ARTS_INFO("Found " << regularAllocs.size() << " regular allocations and "
                     << nestedAllocs.size()
                     << " nested allocations to convert to DB");

  /// Convert nested patterns to N-D datablocks
  /// For each nested allocation pattern (e.g., A[N][M]), create a single
  /// N-dimensional datablock instead of separate 1-D datablocks per row
  ARTS_DEBUG("Converting nested patterns to N-D datablocks");
  DenseMap<Value, DbAllocOp> nestedDbAllocs;
  for (auto &[loop, patterns] : nestedPatternsByLoop) {
    for (auto &pattern : patterns) {
      auto allocOp = pattern.outerAlloc.getDefiningOp<memref::AllocOp>();
      if (!allocOp)
        continue;

      Location loc = allocOp->getLoc();
      SmallVector<Value> allSizes = pattern.getAllSizes();
      if (allSizes.empty())
        continue;

      builder.setInsertionPoint(pattern.initLoop);

      /// Extract logical sizes (remove sizeof scaling)
      SmallVector<Value> originalSizes;
      originalSizes.reserve(allSizes.size());
      for (Value size : allSizes)
        originalSizes.push_back(getOriginalSizeValue(size, builder, loc));

      /// Create N-D datablock with stack allocation
      DbAllocType allocType = DbAllocType::stack;
      ArtsMode mode = ArtsMode::inout;
      DbMode dbMode = DbMode::write;

      SmallVector<Value> payloadSizes;
      auto zeroRoute = builder.create<arith::ConstantIntOp>(loc, 0, 32);

      SmallVector<int64_t> shape(pattern.getDepth(), ShapedType::kDynamic);
      MemRefType ndType = MemRefType::get(shape, pattern.getFinalElementType());
      auto ndAlloc =
          builder.create<memref::AllocaOp>(loc, ndType, originalSizes);

      auto dbAllocOp = builder.create<DbAllocOp>(
          loc, mode, zeroRoute, allocType, dbMode,
          pattern.getFinalElementType(), ndAlloc, originalSizes, payloadSizes);

      ARTS_DEBUG("Created rank-" << shape.size()
                                 << " DbAlloc for nested pattern, element type "
                                 << pattern.getFinalElementType());

      allocToDbAlloc[pattern.outerAlloc] = dbAllocOp;
      createdDbAllocs.push_back(dbAllocOp);
      nestedDbAllocs[pattern.outerAlloc] = dbAllocOp;
    }

    /// Clean up initialization loops now that we have N-D datablocks
    if (!patterns.empty())
      eraseNestedAllocInits(patterns.front());

    /// Rewrite cascaded memory accesses to direct N-D indexing
    for (auto &pattern : patterns) {
      transformNestedAccesses(pattern.outerAlloc,
                              nestedDbAllocs[pattern.outerAlloc]);
    }
  }

  /// Clean up obsolete outer allocations
  ARTS_DEBUG("Cleaning up obsolete nested allocations");
  unsigned nestedAllocsErased = 0;
  for (auto &[loop, patterns] : nestedPatternsByLoop) {
    for (auto &pattern : patterns) {
      Value outer = pattern.outerAlloc;
      auto defOp = outer.getDefiningOp();
      if (defOp && outer.use_empty()) {
        defOp->erase();
        ++nestedAllocsErased;
      }
    }
  }

  /// Remove deallocs for nested allocations
  SmallVector<Operation *> deallocsToErase;
  module.walk([&](memref::DeallocOp deallocOp) {
    Value mem = deallocOp.getMemref();
    Value root = arts::getUnderlyingValue(mem);
    if (nestedAllocs.contains(root)) {
      deallocsToErase.push_back(deallocOp);
    }
  });
  for (Operation *op : deallocsToErase)
    op->erase();

  if (nestedAllocsErased > 0)
    ARTS_DEBUG("Erased " << nestedAllocsErased << " nested allocations and "
                         << deallocsToErase.size() << " deallocs");

  /// Convert regular allocations to datablocks
  ARTS_DEBUG("Converting regular allocations to datablocks");
  createDbAllocOps(regularAllocs, builder);

  /// Phase 4: Process EDTs for dependencies
  SmallVector<EdtOp> allEdts;
  module.walk([&](EdtOp edt) { allEdts.push_back(edt); });
  ARTS_INFO("Processing " << allEdts.size() << " EDT operations");
  for (auto it = allEdts.rbegin(); it != allEdts.rend(); ++it)
    processEdtDeps(*it, builder);

  /// Phase 5: Insert DbFree operations
  ARTS_INFO("Inserting DbFree operations for " << createdDbAllocs.size()
                                               << " DB allocations");
  for (DbAllocOp dbAlloc : createdDbAllocs)
    insertDbFreeOperations(dbAlloc, builder);

  /// Phase 6: Simplify the module
  DominanceInfo domInfo(module);
  arts::simplifyIR(module, domInfo);
  ARTS_INFO_FOOTER(CreateDbsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Allocation Type Inference
//===----------------------------------------------------------------------===//
DbAllocType CreateDbsPass::inferAllocType(Value basePtr) {
  if (!basePtr)
    return DbAllocType::heap;

  if (auto definingOp = basePtr.getDefiningOp()) {
    if (isa<memref::AllocaOp>(definingOp))
      return DbAllocType::stack;
    if (isa<memref::AllocOp>(definingOp))
      return DbAllocType::heap;
    if (isa<memref::GetGlobalOp>(definingOp))
      return DbAllocType::global;
  }
  return DbAllocType::unknown;
}

//===----------------------------------------------------------------------===//
// Cleanup Existing DB Operations
//===----------------------------------------------------------------------===//
void CreateDbsPass::cleanupExistingDbOps() {
  /// Erase all db_control ops from previous passes, replacing uses with pointer
  module.walk([&](DbControlOp dbControl) {
    dbControl.getSubview().replaceAllUsesWith(dbControl.getPtr());
    dbControl.erase();
  });

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

//===----------------------------------------------------------------------===//
// Collect Allocations Used in EDTs
//===----------------------------------------------------------------------===//
SetVector<Value> CreateDbsPass::collectUsedAllocations() {
  SetVector<Value> allUsedAllocs;
  module.walk([&](EdtOp edt) {
    edt.walk([&](Operation *op) {
      Value mem;
      if (auto load = dyn_cast<memref::LoadOp>(op))
        mem = load.getMemref();
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        mem = store.getMemref();
      else
        return;

      Value base = arts::getUnderlyingValue(mem);
      if (base)
        allUsedAllocs.insert(base);
    });
  });
  return allUsedAllocs;
}

//===----------------------------------------------------------------------===//
// Create DB Allocation Operations
//===----------------------------------------------------------------------===//
void CreateDbsPass::createDbAllocOps(const SetVector<Value> &allocs,
                                     OpBuilder &builder) {
  ARTS_DEBUG("Creating DbAlloc operations for " << allocs.size()
                                                << " allocations");

  for (Value alloc : allocs) {
    Operation *allocOp = alloc.getDefiningOp();
    if (!allocOp)
      continue;

    auto memRefType = alloc.getType().cast<MemRefType>();

    Location loc = allocOp->getLoc();
    builder.setInsertionPointAfter(allocOp);

    /// Infer allocation type based on the defining operation
    DbAllocType allocType = inferAllocType(alloc);
    ArtsMode mode = ArtsMode::inout;
    DbMode dbMode = DbMode::write;
    Type elementType = memRefType.getElementType();

    /// Extract dimension sizes from the memref type
    SmallVector<Value> sizes, payloadSizes;
    const int rank = memRefType.getRank();
    sizes.reserve(rank);

    for (int i = 0; i < rank; ++i) {
      if (memRefType.isDynamicDim(i)) {
        sizes.push_back(builder.create<arts::DbDimOp>(loc, alloc, (int64_t)i));
      } else {
        sizes.push_back(builder.create<arith::ConstantIndexOp>(
            loc, memRefType.getDimSize(i)));
      }
    }

    auto zeroRoute = builder.create<arith::ConstantIntOp>(loc, 0, 32);
    auto dbAllocOp =
        builder.create<DbAllocOp>(loc, mode, zeroRoute, allocType, dbMode,
                                  elementType, alloc, sizes, payloadSizes);

    alloc.replaceUsesWithIf(dbAllocOp.getPtr(), [&](OpOperand &use) -> bool {
      Operation *user = use.getOwner();
      /// Skip operands to the db_alloc, db_dim ops and EDT regions.
      return !isa<DbAllocOp>(user) && !isa<DbDimOp>(user) &&
             !user->getParentOfType<EdtOp>();
    });

    /// Store mappings for later use
    allocToDbAlloc[alloc] = dbAllocOp;
    createdDbAllocs.push_back(dbAllocOp);
  }
}

//===----------------------------------------------------------------------===//
// Dependency Information Computation
//===----------------------------------------------------------------------===//
DepInfo CreateDbsPass::computeDepInfo(OpBuilder &builder, EdtOp edt,
                                      Value originalValue, Value source) {
  MemRefType sourceType = source.getType().dyn_cast<MemRefType>();
  assert(sourceType && "Source must be memref");

  /// For string memrefs, acquire the full region
  if (stringAnalysis->isStringMemRef(originalValue)) {
    int rank = sourceType.getRank();
    SmallVector<Value> offsets, sizes;
    for (int d = 0; d < rank; ++d) {
      offsets.push_back(
          builder.create<arith::ConstantIndexOp>(edt.getLoc(), 0));
      Value sizeVal;
      if (sourceType.isDynamicDim(d)) {
        auto dimIdx = builder.create<arith::ConstantIndexOp>(edt.getLoc(), d);
        sizeVal = builder.create<arts::DbDimOp>(edt.getLoc(), source, dimIdx);
      } else
        sizeVal = builder.create<arith::ConstantIndexOp>(
            edt.getLoc(), sourceType.getDimSize(d));
      sizes.push_back(sizeVal);
    }

    MemRefType resultType =
        MemRefType::get(sourceType.getShape(), sourceType.getElementType());
    return {ArtsMode::inout, offsets, sizes, resultType};
  }

  /// For other memrefs, defer slicing to downstream passes
  SmallVector<Value> offsets, sizes;
  MemRefType resultType = sourceType;
  return {ArtsMode::inout, offsets, sizes, resultType};
}

//===----------------------------------------------------------------------===//
// Extract External Values from EDT
//===----------------------------------------------------------------------===//
SetVector<Value> CreateDbsPass::getEdtExternalValues(EdtOp edt) {
  SetVector<Value> externalValues;
  auto &edtRegion = edt.getRegion();

  /// Collect external values used by this EDT (excluding nested EDTs)
  edt.walk([&](Operation *op) -> WalkResult {
    if (isa<EdtOp>(op) && op != edt.getOperation())
      return WalkResult::skip();

    Value mem;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      mem = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      mem = store.getMemref();
    else
      return WalkResult::advance();

    /// Mark as external if defined outside the EDT region
    Value underlyingValue = arts::getUnderlyingValue(mem);
    if (!underlyingValue)
      return WalkResult::advance();
    Operation *definingOp = underlyingValue.getDefiningOp();
    if (definingOp && !edtRegion.isAncestor(definingOp->getParentRegion()))
      externalValues.insert(underlyingValue);
    return WalkResult::advance();
  });

  return externalValues;
}

//===----------------------------------------------------------------------===//
// Process EDT Dependencies
/// For each EDT, analyze external memory dependencies and insert
/// acquire/release
//===----------------------------------------------------------------------===//
void CreateDbsPass::processEdtDeps(EdtOp edt, OpBuilder &builder) {
  SetVector<Value> externalValues = getEdtExternalValues(edt);

  if (externalValues.empty())
    return;

  ARTS_DEBUG("Processing EDT with " << externalValues.size()
                                    << " external dependencies");

  /// For each external memory value, create acquire and release operations
  for (Value value : externalValues) {
    /// Locate the source datablock (db_alloc or db_acquire)
    Operation *sourceOp = nullptr;
    if (allocToDbAlloc.count(value))
      sourceOp = allocToDbAlloc[value];
    else
      sourceOp = arts::getUnderlyingDb(value);

    /// Check that the source operation is a DbAllocOp or DbAcquireOp
    assert(sourceOp && "Source operation must be non-null");
    assert(sourceOp->getNumResults() == 2 &&
           "Source operation must have 2 results");
    auto sourceGuid = sourceOp->getResult(0);
    auto sourcePtr = sourceOp->getResult(1);
    assert((sourceGuid && sourcePtr) && "Source guid and ptr must be non-null");

    /// Create acquire operation (pass both source guid and ptr)
    builder.setInsertionPoint(edt);
    DepInfo info = computeDepInfo(builder, edt, value, sourcePtr);
    DbAcquireOp acquire = builder.create<DbAcquireOp>(
        edt.getLoc(), info.mode, sourceGuid, sourcePtr, SmallVector<Value>{},
        info.offsets, info.sizes);

    /// Add acquire pointer as EDT dependency
    SmallVector<Value> newOperands;
    if (Value route = edt.getRoute())
      newOperands.push_back(route);
    auto existingDeps = edt.getDependenciesAsVector();
    newOperands.append(existingDeps.begin(), existingDeps.end());
    newOperands.push_back(acquire.getPtr());
    edt->setOperands(newOperands);

    /// Add corresponding block argument for the acquired view
    BlockArgument arg =
        edt.getBody().front().addArgument(info.resultType, edt.getLoc());

    /// Rewrite memory accesses to use the acquired view
    adjustAccesses(edt.getRegion(), value, arg, info.offsets);

    /// Insert release before EDT terminator
    builder.setInsertionPoint(edt.getBody().front().getTerminator());
    builder.create<DbReleaseOp>(edt.getLoc(), arg);
  }
}

//===----------------------------------------------------------------------===//
// Adjust Memory Accesses
/// Rewrite load/store operations to use acquired datablock views
//===----------------------------------------------------------------------===//
void CreateDbsPass::adjustAccesses(Region &region, Value originalValue,
                                   Value view, ArrayRef<Value> offsets) {
  region.walk([&](Operation *op) -> WalkResult {
    /// Do not rewrite inside nested EDTs. Each EDT will be handled separately
    /// and should use its own acquired block arguments, not the parent's.
    if (isa<EdtOp>(op))
      return WalkResult::skip();
    Value mem;
    SmallVector<Value> oldIndices;
    bool isLoad = false;
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      mem = load.getMemref();
      oldIndices = load.getIndices();
      isLoad = true;
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      mem = store.getMemref();
      oldIndices = store.getIndices();
    } else if (auto gep = dyn_cast<DbGepOp>(op)) {
      mem = gep.getBasePtr();
      oldIndices.assign(gep.getIndices().begin(), gep.getIndices().end());
    } else {
      return WalkResult::advance();
    }
    if (arts::getUnderlyingValue(mem) != originalValue)
      return WalkResult::advance();

    /// Rewrite the memref to the acquired view.
    if (isLoad) {
      auto load = cast<memref::LoadOp>(op);
      load.getMemrefMutable().assign(view);
    } else if (auto st = dyn_cast<memref::StoreOp>(op)) {
      auto s = cast<memref::StoreOp>(op);
      s.getMemrefMutable().assign(view);
    }
    return WalkResult::advance();
  });
}

//===----------------------------------------------------------------------===//
// Size Value Extraction
/// Extract logical allocation size from sizeof-scaled expressions
//===----------------------------------------------------------------------===//
Value CreateDbsPass::getOriginalSizeValue(Value size, OpBuilder &builder,
                                          Location loc) {
  if (!size)
    return size;

  if (auto div = size.getDefiningOp<arith::DivUIOp>())
    if (Value original =
            extractOriginalSize(div.getLhs(), div.getRhs(), builder, loc))
      return original;

  if (auto div = size.getDefiningOp<arith::DivSIOp>())
    if (Value original =
            extractOriginalSize(div.getLhs(), div.getRhs(), builder, loc))
      return original;

  return castToIndex(stripNumericCasts(size), builder, loc);
}

//===----------------------------------------------------------------------===//
// Insert DB Free Operations
/// Insert db_free for each datablock, replacing existing dealloc operations
//===----------------------------------------------------------------------===//
void CreateDbsPass::insertDbFreeOperations(DbAllocOp dbAlloc,
                                           OpBuilder &builder) {
  Region *region = dbAlloc->getParentRegion();
  if (!region)
    return;

  Operation *regionOwner = region->getParentOp();
  if (!regionOwner)
    return;

  DominanceInfo dom(regionOwner);
  SmallVector<Operation *, 8> opsToErase;
  bool foundFreeCalls = false;
  auto replaceFreeCalls = [&](memref::DeallocOp deallocOp) {
    Value memref = deallocOp.getMemref();
    if (isRelatedToAllocation(memref, dbAlloc)) {
      builder.setInsertionPoint(deallocOp);
      /// Free both the datablock pointer and the GUID memref
      builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getPtr());
      builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getGuid());
      opsToErase.push_back(deallocOp);
      foundFreeCalls = true;
    }
  };

  /// Skip outlined EDT functions
  if (auto parentFunc = dyn_cast<func::FuncOp>(regionOwner))
    if (parentFunc.getName().startswith("__arts_edt_"))
      return;

  /// Find and replace existing deallocation operations
  region->walk(replaceFreeCalls);

  /// If no existing deallocations found, insert DbFree before terminators
  if (!foundFreeCalls) {
    for (Block &block : *region) {
      if (Operation *terminator = block.getTerminator()) {
        if (dom.dominates(dbAlloc.getOperation(), terminator)) {
          builder.setInsertionPoint(terminator);
          /// Free both the datablock pointer and the GUID memref
          builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getPtr());
          builder.create<DbFreeOp>(dbAlloc.getLoc(), dbAlloc.getGuid());
        }
      }
    }
  }

  /// Clean up replaced dealloc operations
  for (Operation *op : opsToErase)
    op->erase();
}

//===----------------------------------------------------------------------===//
// Allocation Relationship Check
/// Check if a value is related to a specific datablock allocation
//===----------------------------------------------------------------------===//
bool CreateDbsPass::isRelatedToAllocation(Value value, DbAllocOp dbAlloc) {
  /// First check if the value is directly from this DbAllocOp
  if (auto dbAllocOp = value.getDefiningOp<DbAllocOp>()) {
    if (dbAllocOp == dbAlloc)
      return true;
  }

  /// Use getUnderlyingValue to find the root allocation for both values
  Value valueRoot = arts::getUnderlyingValue(value);
  Value dbAllocRoot = arts::getUnderlyingValue(dbAlloc.getAddress());

  /// If either root is null, we can't determine the relationship
  if (!valueRoot || !dbAllocRoot)
    return false;

  /// Check if the root allocations are the same
  return valueRoot == dbAllocRoot;
}

//===----------------------------------------------------------------------===//
// Nested Allocation Pattern Detection
/// Detect multi-dimensional arrays allocated as nested arrays-of-pointers
//===----------------------------------------------------------------------===//
std::optional<CreateDbsPass::NestedAllocPattern>
CreateDbsPass::detectNestedAllocPattern(Value alloc) {
  auto currentType = alloc.getType().dyn_cast<MemRefType>();
  if (!currentType)
    return std::nullopt;

  auto elementType = currentType.getElementType();
  auto innerMemRefType = elementType.dyn_cast<MemRefType>();
  if (!innerMemRefType)
    return std::nullopt;

  ARTS_DEBUG("Analyzing potential nested allocation: " << currentType);

  /// Get this dimension size
  Value thisSize;
  auto allocOp = alloc.getDefiningOp<memref::AllocOp>();
  if (!allocOp)
    return std::nullopt;
  Location loc = allocOp->getLoc();
  if (currentType.isDynamicDim(0) && !allocOp.getDynamicSizes().empty()) {
    thisSize = allocOp.getDynamicSizes()[0];
  } else if (!currentType.isDynamicDim(0)) {
    OpBuilder builder(module.getContext());
    builder.setInsertionPointAfter(allocOp);
    thisSize =
        builder.create<arith::ConstantIndexOp>(loc, currentType.getDimSize(0));
  } else {
    return std::nullopt;
  }

  /// Find the initialization loop and inner allocations
  scf::ForOp initLoop = nullptr;
  Value storedValue;

  for (Operation *user : alloc.getUsers()) {
    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getMemref() != alloc)
        continue;

      storedValue = storeOp.getValue();
      auto innerAlloc = storedValue.getDefiningOp<memref::AllocOp>();
      if (!innerAlloc)
        continue;

      auto forOp = storeOp->getParentOfType<scf::ForOp>();
      if (!forOp)
        continue;

      initLoop = forOp;
      auto innerAllocType = innerAlloc.getResult().getType().cast<MemRefType>();

      /// Get inner dimension size and hoist it if needed
      Value innerSize;
      if (innerAllocType.isDynamicDim(0) &&
          !innerAlloc.getDynamicSizes().empty()) {
        innerSize = innerAlloc.getDynamicSizes()[0];

        /// Hoist the inner size computation before the loop if it's defined
        /// inside
        if (auto sizeDefOp = innerSize.getDefiningOp()) {
          if (forOp.getRegion().isAncestor(sizeDefOp->getParentRegion())) {
            /// Size is defined inside loop - need to hoist it
            OpBuilder hoistBuilder(forOp->getContext());
            hoistBuilder.setInsertionPoint(forOp);

            /// Recursively clone the operation and its operands
            std::function<Value(Value)> hoistValue = [&](Value val) -> Value {
              if (auto defOp = val.getDefiningOp()) {
                if (forOp.getRegion().isAncestor(defOp->getParentRegion())) {
                  /// Need to hoist this operation too
                  SmallVector<Value> newOperands;
                  for (Value operand : defOp->getOperands()) {
                    newOperands.push_back(hoistValue(operand));
                  }

                  Operation *cloned = hoistBuilder.clone(*defOp);
                  for (size_t i = 0; i < newOperands.size(); ++i) {
                    cloned->setOperand(i, newOperands[i]);
                  }
                  return cloned->getResult(0);
                }
              }
              return val; /// Already outside loop or is block argument
            };

            innerSize = hoistValue(innerSize);
            ARTS_DEBUG("Hoisted size computation out of initialization loop");
          }
        }
      } else if (!innerAllocType.isDynamicDim(0)) {
        OpBuilder builder(innerAlloc->getContext());
        builder.setInsertionPoint(innerAlloc);
        innerSize = builder.create<arith::ConstantIndexOp>(
            innerAlloc->getLoc(), innerAllocType.getDimSize(0));
      }

      if (!innerSize)
        continue;

      /// Recursively detect inner pattern
      auto innerPattern = detectNestedAllocPattern(innerAlloc.getResult());

      NestedAllocPattern pattern;
      pattern.outerAlloc = alloc;
      pattern.dimSize = thisSize;
      pattern.initLoop = initLoop;
      if (innerPattern) {
        pattern.inner =
            std::make_unique<NestedAllocPattern>(std::move(*innerPattern));
        pattern.elementType = innerPattern->getFinalElementType();
      } else {
        pattern.elementType = innerAllocType.getElementType();
        pattern.innerDimSize = innerSize;
        pattern.innerAllocValue = innerAlloc.getResult();
      }

      ARTS_DEBUG("Detected nested allocation pattern: depth="
                 << pattern.getDepth() << ", element_type="
                 << pattern.elementType << ", loc=" << loc);

      return pattern;
    }
  }

  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// Erase Nested Allocation Initialization Loops
/// Remove initialization loops after converting to N-D datablocks
//===----------------------------------------------------------------------===//
void CreateDbsPass::eraseNestedAllocInits(NestedAllocPattern &pattern) {
  /// Recurse to erase inner levels first
  if (pattern.inner) {
    eraseNestedAllocInits(*pattern.inner);
  }

  ARTS_DEBUG("Pruning dead operations in initialization loop at "
             << pattern.initLoop.getLoc());

  Block *loopBody = pattern.initLoop.getBody();
  bool changed = true;
  unsigned totalErased = 0;
  while (changed) {
    changed = false;
    SmallVector<Operation *, 8> opsToErase;
    for (Operation &op : *loopBody) {
      if (isa<scf::YieldOp>(op))
        continue;
      if (op.use_empty())
        opsToErase.push_back(&op);
    }
    if (opsToErase.empty())
      break;
    for (Operation *op : opsToErase) {
      op->erase();
      ++totalErased;
      changed = true;
    }
  }

  bool hasBodyOps = llvm::any_of(
      *loopBody, [](Operation &op) { return !isa<scf::YieldOp>(op); });

  if (!hasBodyOps) {
    Location loopLoc = pattern.initLoop.getLoc();
    // Capture bounds if they are constants for a more informative message
    Value lb = pattern.initLoop.getLowerBound();
    Value ub = pattern.initLoop.getUpperBound();
    int64_t lbConst = 0, ubConst = 0;
    bool hasLb = arts::getConstantIndex(lb, lbConst);
    bool hasUb = arts::getConstantIndex(ub, ubConst);
    pattern.initLoop.erase();
    if (hasLb && hasUb)
      ARTS_DEBUG("Erased empty initialization loop ["
                 << lbConst << ", " << ubConst << ") at " << loopLoc
                 << ", removed " << totalErased << " ops");
    else
      ARTS_DEBUG("Erased empty initialization loop at "
                 << loopLoc << ", removed " << totalErased << " ops");
  } else {
    ARTS_DEBUG("Initialization loop retained at " << pattern.initLoop.getLoc()
                                                  << ", removed " << totalErased
                                                  << " dead ops");
  }
}

//===----------------------------------------------------------------------===//
// Transform Nested Memory Accesses
/// Convert cascaded load/store chains into direct N-dimensional indexing
/// Pattern: load(load(A[i])[j]) -> load(A_2D[i,j])
//===----------------------------------------------------------------------===//
void CreateDbsPass::transformNestedAccesses(Value outerAlloc,
                                            DbAllocOp dbAlloc) {
  ARTS_DEBUG("Rewriting nested memory accesses to direct N-D indexing");

  Value ndPtr = dbAlloc.getPtr();
  MemRefType ndType = ndPtr.getType().cast<MemRefType>();
  int rank = ndType.getRank();
  int expectedChainLength = rank - 1;
  OpBuilder builder(dbAlloc->getContext());

  struct ChainInfo {
    SmallVector<Value> indices;
    SmallVector<Operation *> loads;
  };

  /// Recursive function to collect index chain
  auto collectChain = [&](Value mem, auto &&self) -> std::optional<ChainInfo> {
    Value base = arts::getUnderlyingValue(mem);
    if (base == outerAlloc) {
      return ChainInfo{{}, {}};
    }

    if (auto defOp = mem.getDefiningOp<memref::LoadOp>()) {
      auto indicesRange = defOp.getIndices();
      SmallVector<Value> thisIndices(indicesRange.begin(), indicesRange.end());
      if (thisIndices.size() != 1)
        return std::nullopt;

      auto prevOpt = self(defOp.getMemref(), self);
      if (!prevOpt)
        return std::nullopt;

      ChainInfo prev = *prevOpt;
      prev.indices.push_back(thisIndices[0]);
      prev.loads.push_back(defOp);
      return prev;
    }

    return std::nullopt;
  };

  SetVector<Operation *> toErase;

  module.walk([&](Operation *op) -> WalkResult {
    Value mem;
    SmallVector<Value> indices;
    bool isLoad = false;
    Value storedValue;
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      mem = loadOp.getMemref();
      indices.assign(loadOp.getIndices().begin(), loadOp.getIndices().end());
      isLoad = true;
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      mem = storeOp.getMemref();
      indices.assign(storeOp.getIndices().begin(), storeOp.getIndices().end());
      isLoad = false;
      storedValue = storeOp.getValue();
    } else {
      return WalkResult::advance();
    }

    auto chainOpt = collectChain(mem, collectChain);
    if (!chainOpt)
      return WalkResult::advance();

    SmallVector<Value> fullIndices = chainOpt->indices;
    if (static_cast<int>(fullIndices.size()) != expectedChainLength)
      return WalkResult::advance();

    if (indices.size() != 1)
      return WalkResult::advance();

    fullIndices.push_back(indices[0]);

    builder.setInsertionPoint(op);
    if (isLoad) {
      auto newLoad =
          builder.create<memref::LoadOp>(op->getLoc(), ndPtr, fullIndices);
      op->getResult(0).replaceAllUsesWith(newLoad);
      ARTS_DEBUG("Rewrote load to rank-" << ndType.getRank() << " access with "
                                         << fullIndices.size() << " indices at "
                                         << op->getLoc());
    } else {
      builder.create<memref::StoreOp>(op->getLoc(), storedValue, ndPtr,
                                      fullIndices);
      ARTS_DEBUG("Rewrote store to rank-" << ndType.getRank() << " access with "
                                          << fullIndices.size()
                                          << " indices at " << op->getLoc());
    }

    toErase.insert(op);
    for (auto *load : chainOpt->loads)
      toErase.insert(load);

    return WalkResult::advance();
  });

  /// Erase old operations
  unsigned erased = 0;
  for (Operation *op : toErase) {
    if (!op->use_empty()) {
      ARTS_DEBUG("Skipping erase of op with remaining uses");
      continue;
    }
    op->erase();
    ++erased;
  }

  ARTS_DEBUG("Completed nested access transformation, erased " << erased
                                                               << " ops");
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCreateDbsPass(bool identifyDbs) {
  return std::make_unique<CreateDbsPass>(identifyDbs);
}
} // namespace arts
} // namespace mlir
