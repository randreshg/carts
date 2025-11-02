///==========================================================================
/// File: CanonicalizeMemrefs.cpp
///
/// This pass preprocesses nested allocation patterns by converting them to
/// single N-dimensional allocations. This is essential for proper DB creation
/// and LLVM code generation.
///
/// Pass Overview:
/// 1. Detect nested allocation patterns (arrays of pointers)
/// 2. Convert them to single N-dimensional allocations
/// 3. Rewrite memory accesses to use direct N-dimensional indexing
/// 4. Remove the initialization loops and legacy allocations
///
///
/// Instead of creating separate DBs for each row, this pass:
/// 1. Detects the nested allocation pattern
/// 2. Creates a single N-dimensional datablock (e.g., memref<?x?xf64>)
/// 3. Transforms cascaded memory accesses (load(load(A[i])[j])) into
///    direct N-dimensional access (load(A_ND[i,j]))
/// 4. Removes the initialization loop that allocated individual rows
//==========================================================================

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
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
ARTS_DEBUG_SETUP(canonicalize_memrefs);

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::omp;

//===----------------------------------------------------------------------===//
// Pass Implementation
//===----------------------------------------------------------------------===//

struct CanonicalizeMemrefsPass
    : public arts::CanonicalizeMemrefsBase<CanonicalizeMemrefsPass> {

  void runOnOperation() override;

private:
  ModuleOp module;
  SetVector<Operation *> opsToRemove;

  /// Helper: mark an operation for deferred removal
  void markForRemoval(Operation *op) {
    if (!op)
      return;
    opsToRemove.insert(op);
  }

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
  void transformNestedAccesses(Value outerAlloc, Value ndPtr);
  void linearizePolygeistViews();

  /// Extract the logical (pre-malloc) size for a dynamically computed value
  Value getOriginalSizeValue(Value size, OpBuilder &builder, Location loc);
};

//===----------------------------------------------------------------------===//
// Pass Entry Point
//===----------------------------------------------------------------------===//
void CanonicalizeMemrefsPass::runOnOperation() {
  module = getOperation();
  opsToRemove.clear();

  ARTS_INFO_HEADER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Verification: Count OpenMP task operations before pass execution
  unsigned numOmpTasksBefore = 0;
  module.walk([&](omp::TaskOp task) { ++numOmpTasksBefore; });
  ARTS_DEBUG(" - OpenMP task count before pass: " << numOmpTasksBefore);

  OpBuilder builder(module.getContext());

  /// Phase 1: Detect and convert nested allocation patterns
  ARTS_INFO("Phase 1: Detecting nested allocation patterns");
  DenseMap<scf::ForOp, SmallVector<NestedAllocPattern>> nestedPatternsByLoop;

  module.walk([&](memref::AllocOp allocOp) {
    Value alloc = allocOp.getResult();
    /// Skip allocations inside loops - these are inner allocations
    if (allocOp->getParentOfType<scf::ForOp>())
      return;
    if (auto pattern = detectNestedAllocPattern(alloc)) {
      nestedPatternsByLoop[pattern->initLoop].push_back(std::move(*pattern));
    }
  });

  if (nestedPatternsByLoop.empty()) {
    ARTS_DEBUG(" - No nested allocation patterns found");
    ARTS_INFO_FOOTER(CanonicalizeMemrefsPass);
    return;
  }

  /// Convert nested patterns to a single N-D stack allocation and rewrite
  /// accesses to index directly into that allocation.
  ARTS_INFO("Phase 2: Converting nested patterns to N-D alloca and rewriting "
            "accesses");
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

      /// Extract logical sizes
      SmallVector<Value> originalSizes;
      originalSizes.reserve(allSizes.size());
      for (Value size : allSizes)
        originalSizes.push_back(getOriginalSizeValue(size, builder, loc));

      SmallVector<int64_t> shape(pattern.getDepth(), ShapedType::kDynamic);
      MemRefType ndType = MemRefType::get(shape, pattern.getFinalElementType());

      /// Use heap allocation (alloc) instead of stack (alloca) to enable
      /// proper DB management and give flexibility for memory allocation
      Value ndAlloc =
          builder.create<memref::AllocOp>(loc, ndType, originalSizes);

      /// Rewrite cascaded memory accesses to direct N-D indexing
      transformNestedAccesses(pattern.outerAlloc, ndAlloc);

      /// Replace loads from outer array with subviews ONLY if they're used by
      /// omp.task depend clauses. All other loads should be part of cascaded
      /// chains and will be handled by transformNestedAccesses.
      Value innerSize =
          originalSizes.back(); // Last size is the column dimension
      SmallVector<scf::ForOp> initLoops = pattern.getAllInitLoops();
      SmallVector<memref::LoadOp> loadsForDepends;

      for (Operation *user : pattern.outerAlloc.getUsers()) {
        if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
          /// Skip loads inside initialization loops
          bool insideInitLoop = false;
          for (auto loop : initLoops) {
            if (loop->isAncestor(loadOp)) {
              insideInitLoop = true;
              break;
            }
          }
          if (insideInitLoop)
            continue;

          /// Check if this load is used by an omp.task depend clause
          bool usedByTaskDepend = false;
          for (Operation *loadUser : loadOp->getUsers()) {
            if (isa<omp::TaskOp>(loadUser)) {
              usedByTaskDepend = true;
              break;
            }
          }

          if (usedByTaskDepend && loadOp.getIndices().size() == 1)
            loadsForDepends.push_back(loadOp);
        }
      }

      /// Create subviews only for depend clauses
      for (auto loadOp : loadsForDepends) {
        Value rowIndex = loadOp.getIndices()[0];
        Location loc = loadOp->getLoc();
        builder.setInsertionPoint(loadOp);

        /// Create a subview that extracts row[rowIndex] as memref<?xf64>
        SmallVector<OpFoldResult> offsets = {rowIndex, builder.getIndexAttr(0)};
        SmallVector<OpFoldResult> sizes = {builder.getIndexAttr(1), innerSize};
        SmallVector<OpFoldResult> strides = {builder.getIndexAttr(1),
                                             builder.getIndexAttr(1)};

        auto subviewOp = builder.create<memref::SubViewOp>(
            loc, ndAlloc, offsets, sizes, strides);

        /// Replace all uses of the load with the subview
        loadOp.getResult().replaceAllUsesWith(subviewOp.getResult());
        opsToRemove.insert(loadOp);
      }

      /// Schedule all legacy allocations and stores for removal
      auto legacyAllocs = pattern.getAllNestedAllocs();
      for (Value legacy : legacyAllocs) {
        if (!legacy)
          continue;
        if (Operation *def = legacy.getDefiningOp())
          opsToRemove.insert(def);
        /// Remove stores that write this allocation into the outer array
        for (Operation *user : legacy.getUsers()) {
          if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
            if (storeOp.getValue() == legacy)
              opsToRemove.insert(storeOp);
          }
        }
      }

      /// Schedule initialization loops for removal since they're now obsolete
      for (auto loop : initLoops)
        opsToRemove.insert(loop);

      /// Find and remove deallocation loops that dealloc inner arrays
      /// These are loops that load from outer array and dealloc the result
      for (Operation *user : pattern.outerAlloc.getUsers()) {
        if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
          /// Check if this load's result is used by memref.dealloc
          for (Operation *loadUser : loadOp->getUsers()) {
            if (isa<memref::DeallocOp>(loadUser)) {
              /// This is a dealloc of an inner array - mark load and dealloc
              opsToRemove.insert(loadOp);
              opsToRemove.insert(loadUser);
              /// Also mark the enclosing loop for removal if it only contains
              /// deallocs
              if (auto forOp = loadOp->getParentOfType<scf::ForOp>())
                opsToRemove.insert(forOp);
              break;
            }
          }
        } else if (auto deallocOp = dyn_cast<memref::DeallocOp>(user)) {
          /// Direct dealloc of outer array
          opsToRemove.insert(deallocOp);
        }
      }
    }
  }

  ARTS_INFO("Phase 3: Linearizing polygeist pointer2memref views");
  linearizePolygeistViews();

  ARTS_INFO("Phase 4: Removing legacy allocations");
  removeOps(module, opsToRemove, true);

  /// Verification: Count OpenMP task operations after pass execution
  unsigned numOmpTasksAfter = 0;
  module.walk([&](omp::TaskOp task) { ++numOmpTasksAfter; });
  ARTS_DEBUG(" - OpenMP task count after pass: " << numOmpTasksAfter);

  /// Fail if OpenMP task count changed
  if (numOmpTasksBefore != numOmpTasksAfter) {
    module.emitError("CanonicalizeMemrefsPass verification failed: OpenMP task "
                     "count mismatch (before: ")
        << numOmpTasksBefore << ", after: " << numOmpTasksAfter << ")";
    signalPassFailure();
    return;
  }

  ARTS_INFO_FOOTER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Size Value Extraction
/// Extract logical allocation size from sizeof-scaled expressions
//===----------------------------------------------------------------------===//
Value CanonicalizeMemrefsPass::getOriginalSizeValue(Value size,
                                                    OpBuilder &builder,
                                                    Location loc) {
  if (!size)
    return size;

  if (auto div = size.getDefiningOp<arith::DivUIOp>())
    if (Value original =
            arts::extractOriginalSize(div.getLhs(), div.getRhs(), builder, loc))
      return original;

  if (auto div = size.getDefiningOp<arith::DivSIOp>())
    if (Value original =
            arts::extractOriginalSize(div.getLhs(), div.getRhs(), builder, loc))
      return original;

  return size;
}

//===----------------------------------------------------------------------===//
// Nested Allocation Pattern Detection
/// Detect multi-dimensional arrays allocated as nested arrays-of-pointers
//===----------------------------------------------------------------------===//
std::optional<CanonicalizeMemrefsPass::NestedAllocPattern>
CanonicalizeMemrefsPass::detectNestedAllocPattern(Value alloc) {
  auto currentType = alloc.getType().dyn_cast<MemRefType>();
  if (!currentType)
    return std::nullopt;

  auto elementType = currentType.getElementType();
  auto innerMemRefType = elementType.dyn_cast<MemRefType>();
  if (!innerMemRefType)
    return std::nullopt;

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

      ARTS_DEBUG(" - Detected nested allocation pattern: depth="
                 << pattern.getDepth()
                 << ", element_type=" << pattern.elementType);

      return pattern;
    }
  }

  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// Transform Nested Memory Accesses
/// Convert cascaded load/store chains into direct N-dimensional indexing
/// Pattern: load(load(A[i])[j]) -> load(A_2D[i,j])
//===----------------------------------------------------------------------===//
void CanonicalizeMemrefsPass::transformNestedAccesses(Value outerAlloc,
                                                      Value ndPtr) {

  MemRefType ndType = ndPtr.getType().cast<MemRefType>();
  int rank = ndType.getRank();
  int expectedChainLength = rank - 1;
  OpBuilder builder(module.getContext());

  struct ChainInfo {
    SmallVector<Value> indices;
    SmallVector<Operation *> loads;
  };

  /// Recursive function to collect index chain
  auto collectChain = [&](Value mem, auto &&self) -> std::optional<ChainInfo> {
    Value base = arts::getUnderlyingValue(mem);
    if (base == outerAlloc)
      return ChainInfo{{}, {}};

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

  /// Transform nested memory accesses to direct N-dimensional indexing
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

    /// Collect the index chain
    auto chainOpt = collectChain(mem, collectChain);
    if (!chainOpt)
      return WalkResult::advance();

    SmallVector<Value> fullIndices = chainOpt->indices;
    if (static_cast<int>(fullIndices.size()) != expectedChainLength)
      return WalkResult::advance();

    /// Check if the current indices are a single index
    if (indices.size() != 1)
      return WalkResult::advance();

    /// Combine the index chain with the current indices
    fullIndices.push_back(indices[0]);

    builder.setInsertionPoint(op);
    if (isLoad) {
      auto newLoad =
          builder.create<memref::LoadOp>(op->getLoc(), ndPtr, fullIndices);
      op->getResult(0).replaceAllUsesWith(newLoad);
    } else {
      builder.create<memref::StoreOp>(op->getLoc(), storedValue, ndPtr,
                                      fullIndices);
    }

    /// Schedule the operation for removal
    opsToRemove.insert(op);
    /// Only mark intermediate loads for removal if they have no remaining uses
    /// (they may be used by omp.task depend clauses, for example)
    for (auto *load : chainOpt->loads) {
      if (load->use_empty())
        opsToRemove.insert(load);
    }

    return WalkResult::advance();
  });
}

//===----------------------------------------------------------------------===//
// Linearize Polygeist Pointer2Memref Views
/// When polygeist creates multidimensional views from 1D allocations,
/// we need to linearize the indices to match the underlying 1D memory layout.
/// Pattern: memref.load %2d_view[%i, %j] -> memref.load %1d_alloc[%i * dim +
/// %j]
//===----------------------------------------------------------------------===//
void CanonicalizeMemrefsPass::linearizePolygeistViews() {
  OpBuilder builder(module.getContext());

  /// Find all pointer2memref operations that create multidimensional views
  SmallVector<polygeist::Pointer2MemrefOp> viewsToLinearize;

  module.walk([&](polygeist::Pointer2MemrefOp p2mOp) {
    auto resultType = p2mOp.getResult().getType().dyn_cast<MemRefType>();
    if (!resultType || resultType.getRank() <= 1)
      return;

    /// Get the source - should trace back to a 1D allocation
    Value source = p2mOp.getSource();
    if (auto m2pOp = source.getDefiningOp<polygeist::Memref2PointerOp>()) {
      Value baseAlloc = m2pOp.getSource();
      auto baseType = baseAlloc.getType().dyn_cast<MemRefType>();

      /// Only linearize if base is 1D and view is multidimensional
      if (baseType && baseType.getRank() == 1 && resultType.getRank() > 1) {
        viewsToLinearize.push_back(p2mOp);
      }
    }
  });

  /// Process each multidimensional view
  for (auto p2mOp : viewsToLinearize) {
    auto viewType = p2mOp.getResult().getType().cast<MemRefType>();
    Value view = p2mOp.getResult();

    /// Get the underlying 1D allocation
    auto m2pOp = p2mOp.getSource().getDefiningOp<polygeist::Memref2PointerOp>();
    if (!m2pOp)
      continue;
    Value baseAlloc = m2pOp.getSource();

    /// Handle 2D -> 1D case
    if (viewType.getRank() == 2) {
      ARTS_DEBUG(" - Linearizing 2D polygeist view to 1D");

      /// Find all load/store operations using this view
      SmallVector<Operation *> usersToUpdate;
      for (Operation *user : view.getUsers()) {
        if (isa<memref::LoadOp, memref::StoreOp>(user))
          usersToUpdate.push_back(user);
      }

      /// Extract dimension info for linearization: index = i * dim1 + j
      for (Operation *op : usersToUpdate) {
        Location loc = op->getLoc();

        if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
          auto indices = loadOp.getIndices();
          if (indices.size() != 2)
            continue;

          Value i = indices[0];
          Value j = indices[1];

          builder.setInsertionPoint(op);

          /// Extract dimension from the memref type or context
          int64_t dim1 = viewType.getDimSize(1);
          Value dim1Value;

          if (dim1 != ShapedType::kDynamic) {
            /// Static dimension - use it directly
            dim1Value = builder.create<arith::ConstantIndexOp>(loc, dim1);
          } else {
            /// Dynamic dimension - extract from enclosing loop
            scf::ForOp innermostLoop = op->getParentOfType<scf::ForOp>();
            if (innermostLoop) {
              dim1Value = innermostLoop.getUpperBound();
            } else {
              /// Cannot determine dimension - skip this operation
              continue;
            }
          }

          /// Linearize: linearIndex = i * dim1 + j
          Value offset = builder.create<arith::MulIOp>(loc, i, dim1Value);
          Value linearIndex = builder.create<arith::AddIOp>(loc, offset, j);

          /// Create new load with 1D index
          auto newLoad =
              builder.create<memref::LoadOp>(loc, baseAlloc, linearIndex);
          loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
          markForRemoval(loadOp);

        } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
          auto indices = storeOp.getIndices();
          if (indices.size() != 2)
            continue;

          Value i = indices[0];
          Value j = indices[1];
          Value valueToStore = storeOp.getValue();

          builder.setInsertionPoint(op);

          /// Extract dimension from the memref type or context
          int64_t dim1 = viewType.getDimSize(1);
          Value dim1Value;

          if (dim1 != ShapedType::kDynamic) {
            /// Static dimension - use it directly
            dim1Value = builder.create<arith::ConstantIndexOp>(loc, dim1);
          } else {
            /// Dynamic dimension - extract from enclosing loop
            scf::ForOp innermostLoop = op->getParentOfType<scf::ForOp>();
            if (innermostLoop) {
              dim1Value = innermostLoop.getUpperBound();
            } else {
              /// Cannot determine dimension - skip this operation
              continue;
            }
          }

          /// Linearize: linearIndex = i * dim1 + j
          Value offset = builder.create<arith::MulIOp>(loc, i, dim1Value);
          Value linearIndex = builder.create<arith::AddIOp>(loc, offset, j);

          /// Create new store with 1D index
          builder.create<memref::StoreOp>(loc, valueToStore, baseAlloc,
                                          linearIndex);
          markForRemoval(storeOp);
        }
      }

      /// Mark polygeist operations for removal
      markForRemoval(p2mOp);
      markForRemoval(m2pOp);
    }
    /// TODO: Add support for 3D -> 1D and other rank transformations
  }
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCanonicalizeMemrefsPass() {
  return std::make_unique<CanonicalizeMemrefsPass>();
}
} // namespace arts
} // namespace mlir