///==========================================================================
/// File: PreprocessNestedAllocations.cpp
///
/// This pass preprocesses nested allocation patterns before DB creation.
/// It detects multi-dimensional arrays allocated as arrays-of-pointers and
/// converts them to single N-dimensional allocations to simplify DB handling.
///
/// Pass Overview:
/// 1. Detect nested allocation patterns (arrays of pointers to arrays)
/// 2. Convert nested patterns to single N-dimensional allocations
/// 3. Transform cascaded memory accesses into direct N-dimensional indexing
/// 4. Remove initialization loops and legacy allocations
///
/// This pass runs before CreateDbsPass to ensure all nested allocations are
/// converted to a canonical form that DB creation can handle uniformly.
///
///==========================================================================

#include "ArtsPassDetails.h"
#include "arts/Analysis/StringAnalysis.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
// Avoid depending on LLVM op headers directly; match by op name
#include "polygeist/Ops.h"
// #include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
// #include "polygeist/Ops.h" // no direct use after removal of subview/collapse
// path
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(preprocess_nested_allocations);

using namespace mlir;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//
namespace {

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

struct PreprocessNestedAllocationsPass
    : public arts::PreprocessNestedAllocationsBase<
          PreprocessNestedAllocationsPass> {

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

  /// Extract the logical (pre-malloc) size for a dynamically computed value
  Value getOriginalSizeValue(Value size, OpBuilder &builder, Location loc);
};

} // namespace

//===----------------------------------------------------------------------===//
// Pass Entry Point
//===----------------------------------------------------------------------===//
void PreprocessNestedAllocationsPass::runOnOperation() {
  module = getOperation();
  opsToRemove.clear();

  ARTS_INFO_HEADER(PreprocessNestedAllocationsPass);
  ARTS_DEBUG_REGION(module.dump(););

  OpBuilder builder(module.getContext());

  /// Phase 1: Detect and convert nested allocation patterns
  ARTS_INFO("Phase 1: Detecting nested allocation patterns");
  SetVector<Value> nestedAllocs;

  /// Multi-dimensional arrays allocated as arrays-of-pointers are detected
  /// and grouped by their initialization loop for later conversion to N-D
  /// allocs
  DenseMap<scf::ForOp, SmallVector<NestedAllocPattern>> nestedPatternsByLoop;

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

  if (nestedPatternsByLoop.empty()) {
    ARTS_DEBUG(" - No nested allocation patterns found");
    ARTS_INFO_FOOTER(PreprocessNestedAllocationsPass);
    return;
  }

  /// Convert nested patterns to a single N-D stack allocation and rewrite
  /// accesses to index directly into that allocation.
  ARTS_DEBUG(
      " - Converting nested patterns to N-D alloca and rewriting accesses");
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
      Value ndAlloc =
          builder.create<memref::AllocaOp>(loc, ndType, originalSizes);

      /// Rewrite cascaded memory accesses to direct N-D indexing
      transformNestedAccesses(pattern.outerAlloc, ndAlloc);

      /// Schedule removal of the initialization loop and legacy outer/inner
      /// allocations for cleanup; all uses should be rewritten by now.
      if (pattern.initLoop)
        opsToRemove.insert(pattern.initLoop.getOperation());
      auto legacyAllocs = pattern.getAllNestedAllocs();
      for (Value legacy : legacyAllocs) {
        if (!legacy)
          continue;
        if (Operation *def = legacy.getDefiningOp())
          opsToRemove.insert(def);
      }
    }
  }

  /// Cleanup legacy allocations
  ARTS_INFO(" - Cleaning up legacy nested allocations");
  removeOps(module, opsToRemove, true);

  ARTS_INFO_FOOTER(PreprocessNestedAllocationsPass);
  ARTS_DEBUG_REGION(module.dump(););
}

//===----------------------------------------------------------------------===//
// Size Value Extraction
/// Extract logical allocation size from sizeof-scaled expressions
//===----------------------------------------------------------------------===//
Value PreprocessNestedAllocationsPass::getOriginalSizeValue(Value size,
                                                            OpBuilder &builder,
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
// Nested Allocation Pattern Detection
/// Detect multi-dimensional arrays allocated as nested arrays-of-pointers
//===----------------------------------------------------------------------===//
std::optional<PreprocessNestedAllocationsPass::NestedAllocPattern>
PreprocessNestedAllocationsPass::detectNestedAllocPattern(Value alloc) {
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
                  for (Value operand : defOp->getOperands())
                    newOperands.push_back(hoistValue(operand));

                  Operation *cloned = hoistBuilder.clone(*defOp);
                  for (size_t i = 0; i < newOperands.size(); ++i)
                    cloned->setOperand(i, newOperands[i]);

                  return cloned->getResult(0);
                }
              }
              /// Already outside loop or is block argument
              return val;
            };

            innerSize = hoistValue(innerSize);
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
void PreprocessNestedAllocationsPass::transformNestedAccesses(Value outerAlloc,
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
    } else {
      builder.create<memref::StoreOp>(op->getLoc(), storedValue, ndPtr,
                                      fullIndices);
    }

    /// Remove the operation that we directly replaced.
    opsToRemove.insert(op);

    /// If intermediate chain loads became dead due to the rewrite, remove them.
    for (auto *load : chainOpt->loads) {
      if (load && load->getNumResults() > 0 && load->getResult(0).use_empty())
        opsToRemove.insert(load);
    }

    return WalkResult::advance();
  });

  // Rewrite pointer-paths that consume row pointers: replace
  //   row = memref.load A[i]
  //   p = memref2pointer(row)
  //   gep(p, j*elemBytes)
  // with base = memref2pointer(ndPtr); gep(base, ((i*dim1)+j)*elemBytes)
  module.walk([&](polygeist::Memref2PointerOp m2p) {
    auto rowLoad = m2p.getSource().getDefiningOp<memref::LoadOp>();
    if (!rowLoad)
      return;
    // Ensure this load comes from the outer array-of-pointers
    Value base = arts::getUnderlyingValue(rowLoad.getMemref());
    if (base != outerAlloc)
      return;

    // Expect exactly one index: the row index i
    auto loadIndices = rowLoad.getIndices();
    if (loadIndices.size() != 1)
      return;
    Value iIndex = loadIndices[0];

    // Find the GEP op that uses the pointer produced by memref2pointer
    Operation *gepUser = nullptr;
    for (auto &use : m2p.getResult().getUses()) {
      Operation *user = use.getOwner();
      if (user && user->getName().getStringRef() == "llvm.getelementptr") {
        gepUser = user;
        break;
      }
    }
    if (!gepUser)
      return; // Only rewrite when pattern matches

    // Extract j index from the bytes index pattern: bytes = (j * elemBytes)
    // Assume operand #1 holds the dynamic index in our patterns
    if (gepUser->getNumOperands() < 2)
      return;
    Value gepIdx = gepUser->getOperand(1);

    // In many cases gepIdx is i64; try to find the source index value j
    Value idxSource = gepIdx;
    if (auto ic2 = idxSource.getDefiningOp<arith::IndexCastOp>())
      idxSource = ic2.getIn();
    if (auto ic = idxSource.getDefiningOp<arith::IndexCastOp>())
      idxSource = ic.getIn();

    auto mulIdx = idxSource.getDefiningOp<arith::MulIOp>();
    if (!mulIdx)
      return;
    // Identify j operand (the non-constant-8 operand)
    auto isConst8 = [](Value v) {
      if (auto c = v.getDefiningOp<arith::ConstantIndexOp>())
        return c.value() == 8;
      if (auto ci = v.getDefiningOp<arith::ConstantIntOp>())
        return ci.value() == 8 && ci.getType().isInteger(64);
      return false;
    };
    Value jIndex =
        isConst8(mulIdx.getLhs())
            ? mulIdx.getRhs()
            : (isConst8(mulIdx.getRhs()) ? mulIdx.getLhs() : Value());
    if (!jIndex)
      return;

    OpBuilder builder(module.getContext());
    builder.setInsertionPoint(gepUser);
    Location loc = gepUser->getLoc();

    // Compute dim1 = dim(ndPtr, 1)
    Value dim1 = builder.create<memref::DimOp>(loc, ndPtr, 1);

    // Compute linear index: i*dim1 + j
    Value iTimesDim = builder.create<arith::MulIOp>(loc, iIndex, dim1);
    Value linIdx = builder.create<arith::AddIOp>(loc, iTimesDim, jIndex);

    // Convert to i64 and scale by element size in bytes
    uint64_t elemBytes = getElementTypeByteSize(
        ndPtr.getType().cast<MemRefType>().getElementType());
    Value linIdxI64 =
        builder.create<arith::IndexCastOp>(loc, builder.getI64Type(), linIdx);
    Value cElem = builder.create<arith::ConstantIntOp>(
        loc, static_cast<int64_t>(elemBytes), 64);
    Value byteOff = builder.create<arith::MulIOp>(loc, linIdxI64, cElem);

    // Create pointer to base of ndPtr and update GEP operands
    auto llvmPtrTy = LLVM::LLVMPointerType::get(builder.getContext());
    Value basePtr =
        builder.create<polygeist::Memref2PointerOp>(loc, llvmPtrTy, ndPtr)
            .getResult();

    // Replace operands in-place
    gepUser->setOperand(0, basePtr);
    // For GEPOp, the last operand is the index in our patterns
    if (gepUser->getNumOperands() >= 2) {
      // Operand 0: base pointer, Operand 1: index
      gepUser->setOperand(0, basePtr);
      gepUser->setOperand(1, byteOff);
    }

    // Clean up the old memref2pointer and row load if now unused
    if (m2p.getResult().use_empty())
      opsToRemove.insert(m2p.getOperation());
    if (rowLoad && rowLoad->use_empty())
      opsToRemove.insert(rowLoad.getOperation());
  });
}

//===----------------------------------------------------------------------===//
// Pass creation
//===----------------------------------------------------------------------===//
namespace mlir {
namespace arts {
std::unique_ptr<Pass> createPreprocessNestedAllocationsPass() {
  return std::make_unique<PreprocessNestedAllocationsPass>();
}
} // namespace arts
} // namespace mlir
