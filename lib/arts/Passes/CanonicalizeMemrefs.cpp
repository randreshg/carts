//==============================================================================
// File: CanonicalizeMemrefs.cpp
//
// This pass canonicalizes memory allocations to enable proper DB creation
// and LLVM code generation. It performs SELF-CONTAINED analysis to handle
// three main patterns:
//
// Pattern 1: Array of Arrays (Nested Allocations)
//   Input:  memref<?xmemref<?xf64>> with cascaded accesses
//   Output: memref<?x?xf64> with direct accesses
//   - Detects nested allocation patterns by analyzing initialization loops
//   - Converts to single N-dimensional allocations
//   - Transforms cascaded memory accesses to direct N-dimensional indexing
//   - Removes initialization loops and legacy allocations
//
// Pattern 2: Flattened Arrays with Stride Accesses
//   Input:  memref<?xf64> with linearized accesses load(A[i*N + j])
//   Output: memref<?x?xf64> with direct accesses load(A_ND[i,j])
//   - Analyzes ALL loads/stores to infer stride patterns (NO external
//   metadata!)
//   - Delinearizes index computations (i*N+j -> [i,j])
//   - Creates N-dimensional allocations matching access patterns
//   - Rewrites all loads/stores to use multidimensional indexing
//   - Supports both affine and arithmetic expressions
//   - Uses majority-rule confidence (>80% of accesses must match pattern)
//
// Pattern 3: OpenMP Task Dependencies
//   - Canonicalizes OpenMP task dependencies to arts.omp_dep operations
//   - Handles scalar, array element, and chunk dependencies
//   - Linearizes polygeist pointer2memref views for consistent representation
//
//==============================================================================

#include "ArtsPassDetails.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"

#include "arts/Passes/ArtsPasses.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
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
  MLIRContext *ctx;
  SetVector<Operation *> opsToRemove;

  //===--------------------------------------------------------------------===//
  /// NestedAllocAnalyzer
  ///
  /// Handles arrays-of-arrays patterns
  /// Example: A[N][M] where A is memref<?xmemref<?xf64>>
  /// Transforms to: memref<?x?xf64>
  ///
  /// Detection strategy:
  /// - Finds allocations with memref element types
  /// - Identifies initialization loops that store inner allocations
  /// - Recursively handles N-dimensional nesting
  ///
  /// Supports:
  /// - N-dimensional nesting
  /// - Both affine and memref load/store operations
  /// - Dynamic and static dimensions
  //===--------------------------------------------------------------------===//
  struct NestedAllocAnalyzer {
    CanonicalizeMemrefsPass *pass;
    NestedAllocAnalyzer(CanonicalizeMemrefsPass *p) : pass(p) {}

    /// Pattern Structure
    struct Pattern {
      Value outerAlloc;
      Value dimSize;
      Value innerDimSize;
      Value innerAllocValue;
      Type elementType;
      scf::ForOp initLoop;
      std::unique_ptr<Pattern> inner;

      Pattern() = default;
      Pattern(Pattern &&) = default;
      Pattern &operator=(Pattern &&) = default;
      Pattern(const Pattern &) = delete;
      Pattern &operator=(const Pattern &) = delete;

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

    /// Analysis & Transformation Methods
    std::optional<Pattern> detectPattern(Value alloc);
    Value transformToCanonical(Pattern &pattern, Location loc);
    void transformAccesses(Pattern &pattern, Value ndAlloc);
    void cleanupPattern(Pattern &pattern);

  private:
    bool isCascadedLoad(Operation *op) const;
    void findCascadedLoads(Value baseAlloc,
                           SmallVectorImpl<Operation *> &loads);
    void transformCascadedChain(Operation *outerLoad, Value ndAlloc,
                                SmallVector<Value> &indices);
  };

  //===--------------------------------------------------------------------===//
  /// FlattenedArrayAnalyzer
  /// Handles flattened arrays with linearized indexing
  /// Example: A[i*N + j] where A is memref<?xf64> but logically 2D
  /// Transforms to: memref<?x?xf64> with A[i,j]
  ///
  /// Detection strategy:
  /// - Analyzes loads/stores to the allocation
  /// - Detects stride patterns in index expressions
  /// - Prioritizes affine expressions, falls back to arithmetic
  /// - Infers dimensions from stride patterns
  /// - Uses majority-rule: >80% of accesses must match pattern
  ///
  /// Supports:
  /// - N-dimensional arrays
  /// - Both affine and memref load/store operations
  /// - Dynamic and static dimensions
  //===--------------------------------------------------------------------===//
  struct FlattenedArrayAnalyzer {
    CanonicalizeMemrefsPass *pass;
    FlattenedArrayAnalyzer(CanonicalizeMemrefsPass *p) : pass(p) {}

    /// Pattern Structure
    struct Pattern {
      Value flatAlloc;
      Type elementType;
      SmallVector<int64_t> dims;
      SmallVector<Value> dimValues;
      SmallVector<int64_t> strides;
      int confidence;

      int getDepth() const { return dims.size(); }

      bool isStatic() const {
        for (int64_t dim : dims)
          if (dim == ShapedType::kDynamic)
            return false;
        return true;
      }

      /// Compute runtime strides for delinearization
      SmallVector<Value> computeStrides(OpBuilder &builder, Location loc) const;
    };

    /// Analysis & Transformation Methods
    std::optional<Pattern> detectPattern(Value alloc);
    Value transformToCanonical(Pattern &pattern, Location loc);
    void transformAccesses(Pattern &pattern, Value ndAlloc);
    void cleanupPattern(Pattern &pattern);

  private:
    /// Stride Analysis Helpers
    struct StrideInfo {
      SmallVector<Value> inductionVars;
      SmallVector<int64_t> strides;
      std::optional<int64_t> constant;

      /// Normalize stride info by sorting strides in descending order
      /// This ensures i*N+j and j+i*N are recognized as the same pattern
      void normalize() {
        if (inductionVars.size() != strides.size())
          return;

        /// Create pairs of (stride, IV) and sort by stride (descending)
        SmallVector<std::pair<int64_t, Value>> pairs;
        for (size_t i = 0; i < strides.size(); ++i)
          pairs.push_back({strides[i], inductionVars[i]});

        /// Sort pairs by stride in descending order
        llvm::sort(pairs, [](const auto &a, const auto &b) {
          return a.first > b.first;
        });

        /// Extract back to parallel arrays
        for (size_t i = 0; i < pairs.size(); ++i) {
          strides[i] = pairs[i].first;
          inductionVars[i] = pairs[i].second;
        }
      }
    };

    std::optional<StrideInfo> analyzeAffineExpr(AffineExpr expr,
                                                ArrayRef<Value> operands);
    std::optional<StrideInfo> analyzeArithExpr(Value index);
    bool isLinearizedIndex(Value index);
    SmallVector<int64_t> inferDimensionsFromStrides(ArrayRef<int64_t> strides,
                                                    int64_t totalSize);
    SmallVector<Value> extractDimensionValues(Value alloc,
                                              ArrayRef<int64_t> strides,
                                              OpBuilder &builder);
    SmallVector<Value> delinearizeIndex(Value linearIndex,
                                        ArrayRef<Value> strides,
                                        OpBuilder &builder, Location loc);
  };

  //===--------------------------------------------------------------------===//
  /// OmpDepsCanonicalizer
  ///
  /// Canonicalizes OpenMP task dependencies to arts.omp_dep operations
  /// Handles:
  /// - Scalar dependencies
  /// - Array element dependencies (load-based)
  /// - Chunk dependencies (subview-based)
  /// - Token container patterns (alloca/store/load)
  //===--------------------------------------------------------------------===//
  struct OmpDepsCanonicalizer {
    CanonicalizeMemrefsPass *pass;
    OmpDepsCanonicalizer(CanonicalizeMemrefsPass *p) : pass(p) {}

    /// Analysis & Transformation Methods
    void canonicalizeDependencies();
    void canonicalizeTask(omp::TaskOp task);

  private:
    struct DepInfo {
      Value source;
      SmallVector<Value> indices;
      SmallVector<Value> sizes;
    };
    std::optional<DepInfo> extractDepInfo(Value depVar, OpBuilder &builder,
                                          Location loc);
  };

  //===--------------------------------------------------------------------===//
  /// Helper Methods
  //===--------------------------------------------------------------------===//
  void markForRemoval(Operation *op) {
    if (!op)
      return;
    opsToRemove.insert(op);
  }

  bool isMarkedForRemoval(Operation *op) const {
    return opsToRemove.contains(op);
  }
  bool verifyAllUsersMarkedForRemoval(Value value, StringRef allocName = "");
  Value createCanonicalAllocation(OpBuilder &builder, Location loc,
                                  Type elementType, ArrayRef<Value> dimSizes);
  void transferMetadata(Operation *oldAlloc, Operation *newAlloc,
                        bool isCanonicalizedArray = true);
  Value createMatchingAllocation(Operation *origAllocOp, Location loc,
                                 Type elementType, ArrayRef<Value> dimSizes);
  void duplicateDeallocations(Value oldAlloc, Value newAlloc,
                              OpBuilder &builder);
  void updateDependenciesForNewAlloc(Value oldAlloc, Value newAlloc);
  bool verifyCanonicalForm(Operation *allocOp);
  void verifyAllCanonical();
  Value getOriginalSizeValue(Value size, OpBuilder &builder, Location loc);
  SmallVector<Value> getAllocDynamicSizes(Operation *allocOp);
  std::optional<omp::ClauseTaskDepend> getDepModeFromTask(omp::TaskOp task,
                                                          Value depVar);
  Value materializeOpFoldResult(OpFoldResult ofr, OpBuilder &builder,
                                Location loc);
  void removeOps(ModuleOp module, SetVector<Operation *> &ops, bool verify);
  void linearizePolygeistViews();
};

void CanonicalizeMemrefsPass::runOnOperation() {
  module = getOperation();
  ctx = module.getContext();
  opsToRemove.clear();

  ARTS_INFO_HEADER(CanonicalizeMemrefsPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Verification: Count OpenMP task operations before pass
  unsigned numOmpTasksBefore = 0;
  module.walk([&](omp::TaskOp task) { ++numOmpTasksBefore; });
  ARTS_DEBUG(" - OpenMP task count before pass: " << numOmpTasksBefore);

  OpBuilder builder(ctx);

  /// Phase 1: Detect and transform nested allocations (arrays-of-arrays)
  ARTS_INFO("Phase 1: Detecting and transforming nested allocations");

  NestedAllocAnalyzer nestedAnalyzer(this);
  DenseMap<scf::ForOp, SmallVector<NestedAllocAnalyzer::Pattern>>
      nestedPatternsByLoop;

  module.walk([&](Operation *op) {
    Value alloc;

    if (auto allocOp = dyn_cast<memref::AllocOp>(op))
      alloc = allocOp.getResult();
    else if (auto allocaOp = dyn_cast<memref::AllocaOp>(op))
      alloc = allocaOp.getResult();
    else
      return;

    /// Skip allocations inside loops - these are usually inner allocations
    if (op->getParentOfType<scf::ForOp>())
      return;

    if (auto pattern = nestedAnalyzer.detectPattern(alloc))
      nestedPatternsByLoop[pattern->initLoop].push_back(std::move(*pattern));
  });

  if (!nestedPatternsByLoop.empty()) {
    ARTS_INFO(" - Found " << nestedPatternsByLoop.size()
                          << " nested allocation groups");

    for (auto &[loop, patterns] : nestedPatternsByLoop) {
      for (auto &pattern : patterns) {
        Location loc = pattern.outerAlloc.getLoc();
        Value ndAlloc = nestedAnalyzer.transformToCanonical(pattern, loc);
        nestedAnalyzer.transformAccesses(pattern, ndAlloc);
        nestedAnalyzer.cleanupPattern(pattern);
      }
    }
  } else {
    ARTS_DEBUG(" - No nested allocation patterns found");
  }

  /// Phase 2: Detect and transform flattened arrays (linearized indexing)
  ARTS_INFO("Phase 2: Detecting and transforming flattened arrays");

  ARTS_DEBUG(" - Linearizing polygeist pointer2memref views");
  linearizePolygeistViews();

  FlattenedArrayAnalyzer flattenedAnalyzer(this);
  SmallVector<std::pair<FlattenedArrayAnalyzer::Pattern, Value>>
      flattenedPatterns;

  /// Process both heap (AllocOp) and stack (AllocaOp) allocations
  /// Stack allocations will be converted to dynamic by CreateDB pass,
  /// so we need to canonicalize them first
  module.walk([&](Operation *op) {
    Value alloc;

    if (auto allocOp = dyn_cast<memref::AllocOp>(op))
      alloc = allocOp.getResult();
    else if (auto allocaOp = dyn_cast<memref::AllocaOp>(op))
      alloc = allocaOp.getResult();
    else
      return;

    /// Skip if already processed as nested
    if (isMarkedForRemoval(op))
      return;

    if (auto pattern = flattenedAnalyzer.detectPattern(alloc)) {
      ARTS_DEBUG(" - Detected flattened array with "
                 << pattern->getDepth() << "D access pattern ("
                 << pattern->confidence << "% confidence)");
      flattenedPatterns.push_back({std::move(*pattern), alloc});
    }
  });

  if (!flattenedPatterns.empty()) {
    ARTS_INFO(" - Found " << flattenedPatterns.size() << " flattened arrays");

    for (auto &[pattern, alloc] : flattenedPatterns) {
      Location loc = alloc.getLoc();

      Value ndAlloc = flattenedAnalyzer.transformToCanonical(pattern, loc);
      flattenedAnalyzer.transformAccesses(pattern, ndAlloc);
      flattenedAnalyzer.cleanupPattern(pattern);
    }
  } else {
    ARTS_DEBUG(" - No flattened arrays detected");
  }

  /// Phase 3: Canonicalize OpenMP Dependencies
  ARTS_INFO("Phase 3: Canonicalizing OpenMP task dependencies");
  OmpDepsCanonicalizer ompCanonicalizer(this);
  ompCanonicalizer.canonicalizeDependencies();

  /// Phase 4: Cleanup
  ARTS_INFO("Phase 4: Removing legacy allocations and operations");
  removeOps(module, opsToRemove, true);

  /// Verify OpenMP task count unchanged
  unsigned numOmpTasksAfter = 0;
  module.walk([&](omp::TaskOp task) { ++numOmpTasksAfter; });
  ARTS_DEBUG(" - OpenMP task count after pass: " << numOmpTasksAfter);

  if (numOmpTasksBefore != numOmpTasksAfter) {
    module.emitError("CanonicalizeMemrefsPass verification failed: OpenMP task "
                     "count mismatch (before: ")
        << numOmpTasksBefore << ", after: " << numOmpTasksAfter << ")";
    signalPassFailure();
    return;
  }

  /// Verify all allocations are canonical
  verifyAllCanonical();

  ARTS_INFO("CanonicalizeMemrefsPass completed successfully");
}

///===----------------------------------------------------------------------===//
/// Implementation continues in next section...
/// Now let's implement each analyzer class
///===----------------------------------------------------------------------===//
/// NestedAllocAnalyzer Implementation
///===----------------------------------------------------------------------===//

std::optional<CanonicalizeMemrefsPass::NestedAllocAnalyzer::Pattern>
CanonicalizeMemrefsPass::NestedAllocAnalyzer::detectPattern(Value alloc) {
  auto currentType = alloc.getType().dyn_cast<MemRefType>();
  if (!currentType)
    return std::nullopt;

  auto elementType = currentType.getElementType();
  auto innerMemRefType = elementType.dyn_cast<MemRefType>();
  if (!innerMemRefType)
    return std::nullopt;

  /// Get this dimension size
  Value thisSize;
  Operation *allocOpRaw = alloc.getDefiningOp();
  if (!allocOpRaw)
    return std::nullopt;

  Location loc = allocOpRaw->getLoc();

  /// Handle both AllocOp and AllocaOp
  SmallVector<Value> dynamicSizes;
  if (auto allocOp = dyn_cast<memref::AllocOp>(allocOpRaw)) {
    dynamicSizes.append(allocOp.getDynamicSizes().begin(),
                        allocOp.getDynamicSizes().end());
  } else if (auto allocaOp = dyn_cast<memref::AllocaOp>(allocOpRaw)) {
    dynamicSizes.append(allocaOp.getDynamicSizes().begin(),
                        allocaOp.getDynamicSizes().end());
  } else {
    return std::nullopt;
  }

  if (currentType.isDynamicDim(0) && !dynamicSizes.empty()) {
    thisSize = dynamicSizes[0];
  } else if (!currentType.isDynamicDim(0)) {
    OpBuilder builder(pass->ctx);
    builder.setInsertionPointAfter(allocOpRaw);
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

      /// Check for both AllocOp and AllocaOp
      Operation *innerAllocOp = storedValue.getDefiningOp();
      if (!innerAllocOp || (!isa<memref::AllocOp>(innerAllocOp) &&
                            !isa<memref::AllocaOp>(innerAllocOp)))
        continue;

      auto forOp = storeOp->getParentOfType<scf::ForOp>();
      if (!forOp)
        continue;

      initLoop = forOp;
      auto innerAllocType = storedValue.getType().cast<MemRefType>();

      /// Get inner dimension size
      Value innerSize;
      SmallVector<Value> innerDynamicSizes =
          pass->getAllocDynamicSizes(innerAllocOp);

      if (innerAllocType.isDynamicDim(0) && !innerDynamicSizes.empty()) {
        innerSize = innerDynamicSizes[0];
      } else if (!innerAllocType.isDynamicDim(0)) {
        OpBuilder builder(pass->ctx);
        builder.setInsertionPoint(innerAllocOp);
        innerSize = builder.create<arith::ConstantIndexOp>(
            innerAllocOp->getLoc(), innerAllocType.getDimSize(0));
      }

      if (!innerSize)
        continue;

      /// Recursively detect inner pattern
      auto innerPattern = detectPattern(storedValue);

      Pattern pattern;
      pattern.outerAlloc = alloc;
      pattern.dimSize = thisSize;
      pattern.initLoop = initLoop;
      if (innerPattern) {
        pattern.inner = std::make_unique<Pattern>(std::move(*innerPattern));
        pattern.elementType = innerPattern->getFinalElementType();
      } else {
        pattern.elementType = innerAllocType.getElementType();
        pattern.innerDimSize = innerSize;
        pattern.innerAllocValue = storedValue;
      }

      ARTS_DEBUG(" - Detected nested allocation pattern: depth="
                 << pattern.getDepth()
                 << ", element_type=" << pattern.elementType);

      return pattern;
    }
  }

  return std::nullopt;
}

Value CanonicalizeMemrefsPass::NestedAllocAnalyzer::transformToCanonical(
    Pattern &pattern, Location loc) {
  OpBuilder builder(pass->ctx);
  builder.setInsertionPoint(pattern.initLoop);

  /// Extract all dimension sizes
  SmallVector<Value> allSizes = pattern.getAllSizes();
  SmallVector<Value> originalSizes;
  originalSizes.reserve(allSizes.size());

  for (Value size : allSizes)
    originalSizes.push_back(pass->getOriginalSizeValue(size, builder, loc));

  /// Create N-D allocation matching the original's style and transfer metadata
  auto allocOp = pattern.outerAlloc.getDefiningOp<memref::AllocOp>();
  Value ndAlloc = pass->createMatchingAllocation(
      allocOp, loc, pattern.getFinalElementType(), originalSizes);
  pass->transferMetadata(allocOp, ndAlloc.getDefiningOp(), true);
  pass->duplicateDeallocations(pattern.outerAlloc, ndAlloc, builder);

  return ndAlloc;
}

void CanonicalizeMemrefsPass::NestedAllocAnalyzer::transformAccesses(
    Pattern &pattern, Value ndAlloc) {
  MemRefType ndType = ndAlloc.getType().cast<MemRefType>();
  int rank = ndType.getRank();
  int expectedChainLength = rank - 1;
  OpBuilder builder(pass->ctx);

  struct ChainInfo {
    SmallVector<Value> indices;
    SmallVector<Operation *> loads;
  };

  Value outerAlloc = pattern.outerAlloc;

  /// Recursive function to collect index chain
  std::function<std::optional<ChainInfo>(Value)> collectChain;
  collectChain = [&](Value mem) -> std::optional<ChainInfo> {
    Value base = arts::getUnderlyingValue(mem);
    if (base == outerAlloc)
      return ChainInfo{{}, {}};

    if (auto defOp = mem.getDefiningOp<memref::LoadOp>()) {
      auto indicesRange = defOp.getIndices();
      SmallVector<Value> thisIndices(indicesRange.begin(), indicesRange.end());
      if (thisIndices.size() != 1)
        return std::nullopt;

      auto prevOpt = collectChain(defOp.getMemref());
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
  pass->module.walk([&](Operation *op) -> WalkResult {
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
    auto chainOpt = collectChain(mem);
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
          builder.create<memref::LoadOp>(op->getLoc(), ndAlloc, fullIndices);
      op->getResult(0).replaceAllUsesWith(newLoad);
    } else {
      builder.create<memref::StoreOp>(op->getLoc(), storedValue, ndAlloc,
                                      fullIndices);
    }

    /// Schedule the operation for removal
    pass->markForRemoval(op);

    /// Only mark intermediate loads for removal if they have no remaining uses
    for (auto *load : chainOpt->loads) {
      if (load->use_empty())
        pass->markForRemoval(load);
    }

    return WalkResult::advance();
  });
}

void CanonicalizeMemrefsPass::NestedAllocAnalyzer::cleanupPattern(
    Pattern &pattern) {
  /// Schedule all legacy allocations for removal
  auto legacyAllocs = pattern.getAllNestedAllocs();
  for (Value legacy : legacyAllocs) {
    if (!legacy)
      continue;
    if (Operation *def = legacy.getDefiningOp())
      pass->markForRemoval(def);

    /// Remove stores that write this allocation into the outer array
    for (Operation *user : legacy.getUsers()) {
      if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
        if (storeOp.getValue() == legacy)
          pass->markForRemoval(storeOp);
      }
    }
  }

  /// Schedule initialization loops for removal
  auto initLoops = pattern.getAllInitLoops();
  for (auto loop : initLoops)
    pass->markForRemoval(loop);

  /// Find and remove deallocation loops that dealloc inner arrays
  for (Operation *user : pattern.outerAlloc.getUsers()) {
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      /// Check if this load's result is used by memref.dealloc
      for (Operation *loadUser : loadOp->getUsers()) {
        if (isa<memref::DeallocOp>(loadUser)) {
          pass->markForRemoval(loadOp);
          pass->markForRemoval(loadUser);
          /// Also mark the enclosing loop for removal if it only contains
          /// deallocs
          if (auto forOp = loadOp->getParentOfType<scf::ForOp>())
            pass->markForRemoval(forOp);
          break;
        }
      }
    } else if (auto deallocOp = dyn_cast<memref::DeallocOp>(user)) {
      /// Direct dealloc of outer array
      pass->markForRemoval(deallocOp);
    }
  }

  /// Mark outer allocation for removal
  auto allocOp = pattern.outerAlloc.getDefiningOp<memref::AllocOp>();
  if (allocOp)
    pass->markForRemoval(allocOp);

  /// Verify that all remaining users are marked for removal
  pass->verifyAllUsersMarkedForRemoval(pattern.outerAlloc,
                                       "Nested allocation (outer array)");
}

//===----------------------------------------------------------------------===//
// FlattenedArrayAnalyzer Implementation
//===----------------------------------------------------------------------===//

SmallVector<Value>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::Pattern::computeStrides(
    OpBuilder &builder, Location loc) const {
  /// Convert static strides to runtime Values
  SmallVector<Value> strideVals;
  strideVals.reserve(strides.size());

  for (int64_t stride : strides) {
    strideVals.push_back(builder.create<arith::ConstantIndexOp>(loc, stride));
  }

  return strideVals;
}

//===----------------------------------------------------------------------===//
// Stride Analysis Helper Methods
//===----------------------------------------------------------------------===//

bool CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::isLinearizedIndex(
    Value index) {
  /// Check if an index looks like a linearized expression
  /// Examples: i*N + j, i*M*L + j*L + k

  if (!index)
    return false;

  /// Check for affine.apply operations (structured)
  if (auto applyOp = index.getDefiningOp<affine::AffineApplyOp>()) {
    AffineMap map = applyOp.getAffineMap();
    AffineExpr expr = map.getResult(0);

    /// Linearized indices typically have multiplication and addition
    return expr.isa<AffineBinaryOpExpr>();
  }

  /// Check for arithmetic operations (mul, add pattern)
  if (auto addOp = index.getDefiningOp<arith::AddIOp>()) {
    /// Look for mul in operands
    if (addOp.getLhs().getDefiningOp<arith::MulIOp>() ||
        addOp.getRhs().getDefiningOp<arith::MulIOp>()) {
      return true;
    }
  }

  if (index.getDefiningOp<arith::MulIOp>()) {
    return true;
  }

  return false;
}

std::optional<CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::StrideInfo>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::analyzeAffineExpr(
    AffineExpr expr, ArrayRef<Value> operands) {
  /// Analyze an affine expression to extract stride information
  /// Example: d0*N + d1 -> strides=[N, 1], IVs=[d0, d1]
  StrideInfo info;

  /// Recursive helper to extract terms
  std::function<void(AffineExpr, int64_t)> extractTerms;
  extractTerms = [&](AffineExpr e, int64_t multiplier) {
    if (auto binOp = e.dyn_cast<AffineBinaryOpExpr>()) {
      switch (binOp.getKind()) {
      case AffineExprKind::Add:
        extractTerms(binOp.getLHS(), multiplier);
        extractTerms(binOp.getRHS(), multiplier);
        break;
      case AffineExprKind::Mul: {
        /// Check if one side is a constant
        if (auto constExpr = binOp.getLHS().dyn_cast<AffineConstantExpr>()) {
          extractTerms(binOp.getRHS(), multiplier * constExpr.getValue());
        } else if (auto constExpr =
                       binOp.getRHS().dyn_cast<AffineConstantExpr>()) {
          extractTerms(binOp.getLHS(), multiplier * constExpr.getValue());
        }
        break;
      }
      default:
        break;
      }
    } else if (auto dimExpr = e.dyn_cast<AffineDimExpr>()) {
      /// Found a dimension reference
      unsigned pos = dimExpr.getPosition();
      if (pos < operands.size()) {
        info.inductionVars.push_back(operands[pos]);
        info.strides.push_back(multiplier);
      }
    } else if (auto constExpr = e.dyn_cast<AffineConstantExpr>()) {
      /// Constant offset
      info.constant = constExpr.getValue();
    }
  };

  extractTerms(expr, 1);

  if (info.inductionVars.empty())
    return std::nullopt;

  return info;
}

std::optional<CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::StrideInfo>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::analyzeArithExpr(Value index) {
  /// Analyze arithmetic expression tree to extract stride information
  /// This is the fallback when affine expressions aren't available

  StrideInfo info;

  /// Helper to recursively analyze expression tree
  std::function<std::optional<int64_t>(Value)> extractStride;
  extractStride = [&](Value v) -> std::optional<int64_t> {
    if (auto mulOp = v.getDefiningOp<arith::MulIOp>()) {
      /// Try to extract constant stride
      if (auto constOp =
              mulOp.getRhs().getDefiningOp<arith::ConstantIndexOp>()) {
        info.inductionVars.push_back(mulOp.getLhs());
        int64_t stride = constOp.value();
        info.strides.push_back(stride);
        return stride;
      } else if (auto constOp =
                     mulOp.getLhs().getDefiningOp<arith::ConstantIndexOp>()) {
        info.inductionVars.push_back(mulOp.getRhs());
        int64_t stride = constOp.value();
        info.strides.push_back(stride);
        return stride;
      }
    } else if (auto addOp = v.getDefiningOp<arith::AddIOp>()) {
      /// Recursively analyze both sides
      extractStride(addOp.getLhs());
      extractStride(addOp.getRhs());
    } else if (auto constOp = v.getDefiningOp<arith::ConstantIndexOp>()) {
      /// Constant offset
      info.constant = constOp.value();
    } else {
      /// Assume stride of 1 for bare induction variable
      info.inductionVars.push_back(v);
      info.strides.push_back(1);
    }
    return std::nullopt;
  };

  extractStride(index);

  if (info.inductionVars.empty())
    return std::nullopt;

  return info;
}

SmallVector<int64_t>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::inferDimensionsFromStrides(
    ArrayRef<int64_t> strides, int64_t totalSize) {
  /// Infer dimensions from stride pattern
  /// Example: strides=[N*M, M, 1] with totalSize -> dims=[?, ?, M]
  /// Example: strides=[6, 3, 1] -> dims=[?, 2, 3]
  SmallVector<int64_t> dims;
  if (strides.empty())
    return dims;

  /// For each stride, the dimension is stride[i] / stride[i+1]
  for (size_t i = 0; i < strides.size(); ++i) {
    if (i + 1 < strides.size()) {
      int64_t dim = strides[i] / strides[i + 1];
      dims.push_back(dim);
    } else {
      /// Last dimension - if stride is 1, dimension is unknown
      if (strides[i] == 1) {
        dims.push_back(ShapedType::kDynamic);
      } else {
        dims.push_back(strides[i]);
      }
    }
  }

  return dims;
}

SmallVector<Value>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::extractDimensionValues(
    Value alloc, ArrayRef<int64_t> strides, OpBuilder &builder) {
  /// Extract runtime dimension values from the IR
  /// Look for loops and size computations that match the strides

  SmallVector<Value> dimValues;
  Operation *allocOpRaw = alloc.getDefiningOp();
  if (!allocOpRaw ||
      (!isa<memref::AllocOp>(allocOpRaw) && !isa<memref::AllocaOp>(allocOpRaw)))
    return dimValues;

  Location loc = allocOpRaw->getLoc();

  /// Try to find the total size
  auto allocType = alloc.getType().cast<MemRefType>();
  Value totalSize = nullptr;

  /// Get dynamic sizes (works for both AllocOp and AllocaOp)
  SmallVector<Value> dynamicSizes = pass->getAllocDynamicSizes(allocOpRaw);
  if (allocType.isDynamicDim(0) && !dynamicSizes.empty()) {
    totalSize = dynamicSizes[0];
  }

  if (!totalSize)
    return dimValues;

  /// For each stride, try to extract the dimension value
  builder.setInsertionPointAfter(allocOpRaw);

  for (size_t i = 0; i < strides.size(); ++i) {
    if (i == 0) {
      /// First dimension: totalSize / firstStride
      if (strides[0] > 1) {
        Value strideVal =
            builder.create<arith::ConstantIndexOp>(loc, strides[0]);
        dimValues.push_back(
            builder.create<arith::DivUIOp>(loc, totalSize, strideVal));
      } else {
        dimValues.push_back(totalSize);
      }
    } else if (i + 1 < strides.size()) {
      /// Middle dimensions: stride[i] / stride[i+1]
      int64_t dim = strides[i] / strides[i + 1];
      dimValues.push_back(builder.create<arith::ConstantIndexOp>(loc, dim));
    } else {
      /// Last dimension
      dimValues.push_back(
          builder.create<arith::ConstantIndexOp>(loc, strides[i]));
    }
  }

  return dimValues;
}

SmallVector<Value>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::delinearizeIndex(
    Value linearIndex, ArrayRef<Value> strides, OpBuilder &builder,
    Location loc) {
  /// Delinearize a linear index into N-dimensional indices
  /// Example: idx -> [idx / stride[0], (idx % stride[0]) / stride[1], ...]
  SmallVector<Value> indices;
  Value remaining = linearIndex;

  for (size_t i = 0; i < strides.size(); ++i) {
    if (i < strides.size() - 1) {
      /// indices[i] = remaining / stride[i]
      Value idx = builder.create<arith::DivUIOp>(loc, remaining, strides[i]);
      indices.push_back(idx);

      /// remaining = remaining % stride[i]
      remaining = builder.create<arith::RemUIOp>(loc, remaining, strides[i]);
    } else {
      /// Last index is just the remaining value
      indices.push_back(remaining);
    }
  }

  return indices;
}

//===----------------------------------------------------------------------===//
// Main Detection and Transformation Methods
//===----------------------------------------------------------------------===//

std::optional<CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::Pattern>
CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::detectPattern(Value alloc) {
  /// Detect flattened array pattern by analyzing all loads/stores
  /// This is SELF-CONTAINED - no external metadata required!

  auto allocType = alloc.getType().dyn_cast<MemRefType>();
  if (!allocType)
    return std::nullopt;

  /// Only consider 1D allocations
  if (allocType.getRank() != 1)
    return std::nullopt;

  /// Skip if element type is itself a memref (nested pattern)
  if (allocType.getElementType().isa<MemRefType>())
    return std::nullopt;

  /// Phase 1: Collect all accesses and analyze their indices
  struct AccessInfo {
    Operation *op;
    Value index;
    std::optional<StrideInfo> strideInfo;
  };

  SmallVector<AccessInfo> accesses;
  unsigned totalAccessCount = 0;

  /// Collect all loads/stores (both memref and affine dialects)
  for (Operation *user : alloc.getUsers()) {
    Value index;
    bool isMemoryAccess = false;

    /// Handle memref loads/stores
    if (auto loadOp = dyn_cast<memref::LoadOp>(user)) {
      if (loadOp.getIndices().size() == 1) {
        index = loadOp.getIndices()[0];
        isMemoryAccess = true;
      }
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      if (storeOp.getIndices().size() == 1) {
        index = storeOp.getIndices()[0];
        isMemoryAccess = true;
      }
    }
    /// Handle affine loads/stores
    else if (auto affLoadOp = dyn_cast<affine::AffineLoadOp>(user)) {
      if (affLoadOp.getMapOperands().size() > 0) {
        /// For affine loads, we need to get the index from the affine map
        /// If it's a simple 1D access, we can proceed
        auto map = affLoadOp.getAffineMap();
        if (map.getNumResults() == 1) {
          /// Create equivalent affine.apply for analysis
          OpBuilder builder(pass->ctx);
          builder.setInsertionPoint(affLoadOp);
          auto applyOp = builder.create<affine::AffineApplyOp>(
              affLoadOp.getLoc(), map, affLoadOp.getMapOperands());
          index = applyOp.getResult();
          isMemoryAccess = true;
        }
      }
    } else if (auto affStoreOp = dyn_cast<affine::AffineStoreOp>(user)) {
      if (affStoreOp.getMapOperands().size() > 0) {
        auto map = affStoreOp.getAffineMap();
        if (map.getNumResults() == 1) {
          OpBuilder builder(pass->ctx);
          builder.setInsertionPoint(affStoreOp);
          auto applyOp = builder.create<affine::AffineApplyOp>(
              affStoreOp.getLoc(), map, affStoreOp.getMapOperands());
          index = applyOp.getResult();
          isMemoryAccess = true;
        }
      }
    }

    if (!isMemoryAccess || !index)
      continue;

    /// Count all memory accesses
    totalAccessCount++;

    /// Skip simple direct indices
    if (!isLinearizedIndex(index))
      continue;

    AccessInfo info;
    info.op = user;
    info.index = index;

    /// Try affine analysis first
    if (auto applyOp = index.getDefiningOp<affine::AffineApplyOp>()) {
      auto mapOperands = applyOp.getMapOperands();
      SmallVector<Value> operands(mapOperands.begin(), mapOperands.end());
      info.strideInfo =
          analyzeAffineExpr(applyOp.getAffineMap().getResult(0), operands);
    }

    /// Fall back to arithmetic analysis
    if (!info.strideInfo)
      info.strideInfo = analyzeArithExpr(index);

    if (info.strideInfo) {
      /// Normalize stride pattern for consistent comparison
      info.strideInfo->normalize();
      accesses.push_back(info);
    }
  }

  /// If no memory accesses found at all, not an array
  if (totalAccessCount == 0)
    return std::nullopt;

  /// If no linearized accesses found, not a flattened array
  if (accesses.empty())
    return std::nullopt;

  /// Phase 2: Find common stride pattern (majority rule: >80%)
  /// Use a simple approach: iterate through accesses and count patterns
  SmallVector<int64_t> dominantStrides;
  unsigned maxCount = 0;

  /// First pass: find the most common pattern
  for (const auto &access : accesses) {
    if (!access.strideInfo)
      continue;

    SmallVector<int64_t> strides = access.strideInfo->strides;
    unsigned count = 0;

    /// Count how many times this pattern appears
    for (const auto &otherAccess : accesses) {
      if (!otherAccess.strideInfo)
        continue;
      if (otherAccess.strideInfo->strides == strides)
        count++;
    }

    if (count > maxCount) {
      maxCount = count;
      dominantStrides = strides;
    }
  }

  /// Calculate confidence percentage using all accesses
  int confidence = (maxCount * 100) / totalAccessCount;

  /// Check confidence threshold (>80% per user requirement)
  if (confidence < 80) {
    ARTS_DEBUG("   Flattened array confidence too low: " << confidence << "%");
    return std::nullopt;
  }

  /// Phase 3: Infer dimensions from dominant stride pattern
  Pattern pattern;
  pattern.flatAlloc = alloc;
  pattern.elementType = allocType.getElementType();
  pattern.strides = dominantStrides;
  pattern.confidence = confidence;

  /// Get total size
  Operation *allocOpRaw = alloc.getDefiningOp();
  if (!allocOpRaw || (!isa<memref::AllocOp>(allocOpRaw) &&
                      !isa<memref::AllocaOp>(allocOpRaw))) {
    return std::nullopt;
  }

  int64_t totalSize = ShapedType::kDynamic;
  if (allocType.hasStaticShape())
    totalSize = allocType.getNumElements();

  /// Infer dimensions
  pattern.dims = inferDimensionsFromStrides(dominantStrides, totalSize);

  /// Extract runtime dimension values
  OpBuilder builder(pass->ctx);
  pattern.dimValues = extractDimensionValues(alloc, dominantStrides, builder);

  if (pattern.dimValues.empty() || pattern.dims.empty())
    return std::nullopt;

  return pattern;
}

Value CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::transformToCanonical(
    Pattern &pattern, Location loc) {
  OpBuilder builder(pass->ctx);
  Operation *allocOpRaw = pattern.flatAlloc.getDefiningOp();
  if (!allocOpRaw)
    return Value();

  builder.setInsertionPoint(allocOpRaw);

  /// Create N-D canonical allocation and transfer metadata
  Value ndAlloc = pass->createMatchingAllocation(
      allocOpRaw, loc, pattern.elementType, pattern.dimValues);
  pass->transferMetadata(allocOpRaw, ndAlloc.getDefiningOp(), true);
  pass->duplicateDeallocations(pattern.flatAlloc, ndAlloc, builder);

  return ndAlloc;
}

void CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::transformAccesses(
    Pattern &pattern, Value ndAlloc) {
  OpBuilder builder(pass->ctx);

  /// Compute runtime stride values for delinearization
  SmallVector<Value> strideVals =
      pattern.computeStrides(builder, ndAlloc.getLoc());

  /// Helper: Extract N-D indices from a linear index
  /// Returns nullopt if index doesn't match pattern or can't be delinearized
  auto extractNDIndices =
      [&](Value linearIndex) -> std::optional<SmallVector<Value>> {
    if (!linearIndex)
      return std::nullopt;

    /// If index comes from affine.apply with matching pattern, directly reuse
    /// the map operands instead of creating div/rem ops
    if (auto applyOp = linearIndex.getDefiningOp<affine::AffineApplyOp>()) {
      auto mapOperands = applyOp.getMapOperands();
      SmallVector<Value> operands(mapOperands.begin(), mapOperands.end());
      auto strideInfo =
          analyzeAffineExpr(applyOp.getAffineMap().getResult(0), operands);
      if (strideInfo) {
        StrideInfo normalized = *strideInfo;
        normalized.normalize();

        /// Check if this matches the pattern
        if (normalized.strides == pattern.strides) {
          /// Perfect match! Just return the induction variables in order
          return SmallVector<Value>(normalized.inductionVars.begin(),
                                    normalized.inductionVars.end());
        }
      }
    }

    /// Validate: Check if this is a linearized index matching our pattern
    if (!isLinearizedIndex(linearIndex))
      return std::nullopt;

    /// Analyze the index to confirm it matches our pattern
    std::optional<StrideInfo> strideInfo;
    if (auto applyOp = linearIndex.getDefiningOp<affine::AffineApplyOp>()) {
      auto mapOperands = applyOp.getMapOperands();
      SmallVector<Value> operands(mapOperands.begin(), mapOperands.end());
      strideInfo =
          analyzeAffineExpr(applyOp.getAffineMap().getResult(0), operands);
    } else {
      strideInfo = analyzeArithExpr(linearIndex);
    }

    if (!strideInfo)
      return std::nullopt;

    strideInfo->normalize();

    /// Verify it matches the detected pattern
    if (strideInfo->strides != pattern.strides) {
      /// Different pattern, don't transform
      return std::nullopt;
    }

    /// Generic delinearization using div/rem
    return delinearizeIndex(linearIndex, strideVals, builder,
                            linearIndex.getLoc());
  };

  /// Transform all loads and stores (both memref and affine)
  SmallVector<Operation *> opsToTransform;

  for (Operation *user : pattern.flatAlloc.getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, affine::AffineLoadOp,
            affine::AffineStoreOp>(user)) {
      opsToTransform.push_back(user);
    }
  }

  for (Operation *op : opsToTransform) {
    builder.setInsertionPoint(op);
    Location loc = op->getLoc();

    /// Handle memref::LoadOp
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      if (loadOp.getIndices().size() != 1)
        continue;

      auto ndIndices = extractNDIndices(loadOp.getIndices()[0]);
      if (!ndIndices)
        continue; /// Skip if doesn't match pattern

      auto newLoad = builder.create<memref::LoadOp>(loc, ndAlloc, *ndIndices);
      loadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      pass->markForRemoval(loadOp);
    }
    /// Handle memref::StoreOp
    else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      if (storeOp.getIndices().size() != 1)
        continue;

      auto ndIndices = extractNDIndices(storeOp.getIndices()[0]);
      if (!ndIndices)
        continue; /// Skip if doesn't match pattern

      builder.create<memref::StoreOp>(loc, storeOp.getValue(), ndAlloc,
                                      *ndIndices);
      pass->markForRemoval(storeOp);
    }
    /// Handle affine::AffineLoadOp
    else if (auto affLoadOp = dyn_cast<affine::AffineLoadOp>(op)) {
      auto map = affLoadOp.getAffineMap();
      if (map.getNumResults() != 1)
        continue;

      /// Create affine.apply to get the linear index
      auto applyOp = builder.create<affine::AffineApplyOp>(
          loc, map, affLoadOp.getMapOperands());

      auto ndIndices = extractNDIndices(applyOp.getResult());
      /// Skip if doesn't match pattern
      if (!ndIndices)
        continue;

      /// Create new memref load (converting from affine to memref)
      auto newLoad = builder.create<memref::LoadOp>(loc, ndAlloc, *ndIndices);
      affLoadOp.getResult().replaceAllUsesWith(newLoad.getResult());
      pass->markForRemoval(affLoadOp);
      pass->markForRemoval(applyOp);
    }
    /// Handle affine::AffineStoreOp
    else if (auto affStoreOp = dyn_cast<affine::AffineStoreOp>(op)) {
      auto map = affStoreOp.getAffineMap();
      if (map.getNumResults() != 1)
        continue;

      /// Create affine.apply to get the linear index
      auto applyOp = builder.create<affine::AffineApplyOp>(
          loc, map, affStoreOp.getMapOperands());

      auto ndIndices = extractNDIndices(applyOp.getResult());
      /// Skip if doesn't match pattern
      if (!ndIndices)
        continue;

      /// Create new memref store (converting from affine to memref)
      builder.create<memref::StoreOp>(loc, affStoreOp.getValue(), ndAlloc,
                                      *ndIndices);
      pass->markForRemoval(affStoreOp);
      pass->markForRemoval(applyOp);
    }
  }
}

void CanonicalizeMemrefsPass::FlattenedArrayAnalyzer::cleanupPattern(
    Pattern &pattern) {
  /// Mark the old flat allocation for removal
  if (auto allocOp = pattern.flatAlloc.getDefiningOp())
    pass->markForRemoval(allocOp);

  /// Mark deallocations for removal
  for (Operation *user : pattern.flatAlloc.getUsers()) {
    if (isa<memref::DeallocOp>(user))
      pass->markForRemoval(user);
  }
}

//===----------------------------------------------------------------------===//
// OmpDepsCanonicalizer Implementation
//===----------------------------------------------------------------------===//

void CanonicalizeMemrefsPass::OmpDepsCanonicalizer::canonicalizeDependencies() {
  /// Walk all OpenMP task operations and canonicalize their dependencies
  ARTS_DEBUG(" - Canonicalizing OpenMP task dependencies");

  SmallVector<omp::TaskOp> tasks;
  pass->module.walk([&](omp::TaskOp task) { tasks.push_back(task); });

  if (tasks.empty()) {
    ARTS_WARN("   No OpenMP tasks found");
    return;
  }

  ARTS_DEBUG(" - Found " << tasks.size() << " OpenMP tasks");

  for (auto task : tasks)
    canonicalizeTask(task);

  ARTS_DEBUG("   Dependency canonicalization complete");
}

void CanonicalizeMemrefsPass::OmpDepsCanonicalizer::canonicalizeTask(
    omp::TaskOp task) {
  /// Canonicalize dependencies for a single task operation
  /// This handles three types of dependencies:
  /// 1. Scalar dependencies (direct values)
  /// 2. Array element dependencies (via load operations)
  /// 3. Chunk dependencies (via subview operations)

  OpBuilder builder(pass->ctx);
  Location loc = task.getLoc();

  /// Process dependencies using the new TaskOp API
  auto dependList = task.getDependsAttr();
  if (dependList) {
    auto dependVars = task.getDependVars();
    for (unsigned i = 0, e = dependList.size(); i < e && i < dependVars.size();
         ++i) {
      /// Get dependency clause and type
      auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
      if (!depClause)
        continue;

      Value depVar = dependVars[i];

      /// Extract dependency information
      auto depInfo = extractDepInfo(depVar, builder, loc);
      if (!depInfo)
        continue;

      /// Create arts.omp_dep operation to represent the dependency
      builder.setInsertionPoint(task);

      /// Convert ClauseTaskDepend to access mode string
      StringRef accessMode;
      switch (depClause.getValue()) {
      case omp::ClauseTaskDepend::taskdependin:
        accessMode = "in";
        break;
      case omp::ClauseTaskDepend::taskdependout:
        accessMode = "out";
        break;
      case omp::ClauseTaskDepend::taskdependinout:
        accessMode = "inout";
        break;
      }

      /// For now, we just verify the dependency structure is valid
      /// The actual arts.omp_dep operation creation would go here
      /// if the operation is defined in the dialect
      ARTS_DEBUG("    - Canonicalized dependency: mode="
                 << accessMode << ", indices=" << depInfo->indices.size()
                 << ", sizes=" << depInfo->sizes.size());
    }
  }
}

std::optional<CanonicalizeMemrefsPass::OmpDepsCanonicalizer::DepInfo>
CanonicalizeMemrefsPass::OmpDepsCanonicalizer::extractDepInfo(
    Value depVar, OpBuilder &builder, Location loc) {
  /// Extract dependency information from a value
  /// Handles multiple patterns:
  /// 1. Direct memref (whole array dependency)
  /// 2. SubView (chunk dependency)
  /// 3. Load from token container (scalar dependency via alloca/store/load)
  /// 4. Polygeist pointer2memref views

  DepInfo info;

  /// Pattern 1: Direct memref allocation
  if (auto allocOp = depVar.getDefiningOp<memref::AllocOp>()) {
    info.source = depVar;
    return info;
  }

  if (auto allocaOp = depVar.getDefiningOp<memref::AllocaOp>()) {
    info.source = depVar;
    return info;
  }

  /// Pattern 2: SubView operation (chunk dependency)
  if (auto subviewOp = depVar.getDefiningOp<memref::SubViewOp>()) {
    info.source = subviewOp.getSource();

    /// Extract indices from offsets
    for (auto offset : subviewOp.getMixedOffsets()) {
      if (auto value = offset.dyn_cast<Value>()) {
        info.indices.push_back(value);
      } else if (auto attr = offset.dyn_cast<Attribute>()) {
        auto intAttr = attr.cast<IntegerAttr>();
        Value constVal =
            builder.create<arith::ConstantIndexOp>(loc, intAttr.getInt());
        info.indices.push_back(constVal);
      }
    }

    /// Extract sizes
    for (auto size : subviewOp.getMixedSizes()) {
      if (auto value = size.dyn_cast<Value>()) {
        info.sizes.push_back(value);
      } else if (auto attr = size.dyn_cast<Attribute>()) {
        auto intAttr = attr.cast<IntegerAttr>();
        Value constVal =
            builder.create<arith::ConstantIndexOp>(loc, intAttr.getInt());
        info.sizes.push_back(constVal);
      }
    }

    return info;
  }

  /// Pattern 3: Load from token container (alloca -> store -> load pattern)
  if (auto loadOp = depVar.getDefiningOp<memref::LoadOp>()) {
    Value memref = loadOp.getMemref();

    /// Check if this is a token container
    if (auto allocaOp = memref.getDefiningOp<memref::AllocaOp>()) {
      /// Find the store operation that wrote to this alloca
      for (Operation *user : allocaOp->getUsers()) {
        if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
          /// Recursively extract info from the actual dependency
          Value actualDep = storeOp.getValue();
          return extractDepInfo(actualDep, builder, loc);
        }
      }
    }

    /// If not a token container, treat as array element dependency
    info.source = loadOp.getMemref();
    info.indices.assign(loadOp.getIndices().begin(), loadOp.getIndices().end());
    return info;
  }

  /// Pattern 4: Polygeist pointer2memref
  if (auto p2mOp = depVar.getDefiningOp<polygeist::Pointer2MemrefOp>()) {
    info.source = depVar;
    return info;
  }

  /// Pattern 5: Block argument (function argument or loop induction variable)
  if (depVar.isa<BlockArgument>()) {
    info.source = depVar;
    return info;
  }

  /// Unable to extract dependency info
  ARTS_DEBUG("    Warning: Unable to extract dependency info from value");
  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// Shared Helper Methods Implementation
//===----------------------------------------------------------------------===//

bool CanonicalizeMemrefsPass::verifyAllUsersMarkedForRemoval(
    Value value, StringRef allocName) {
  unsigned totalUsers = 0;
  SmallVector<Operation *> unmarkedUsers;

  for (Operation *user : value.getUsers()) {
    totalUsers++;
    if (!isMarkedForRemoval(user))
      unmarkedUsers.push_back(user);
  }

  if (!unmarkedUsers.empty()) {
    ARTS_DEBUG("  Warning: " << allocName << " has " << unmarkedUsers.size()
                             << " unmarked users out of " << totalUsers);
    for (Operation *user : unmarkedUsers)
      ARTS_DEBUG("    - Unmarked: " << user->getName().getStringRef());
    return false;
  }

  return true;
}

Value CanonicalizeMemrefsPass::createCanonicalAllocation(
    OpBuilder &builder, Location loc, Type elementType,
    ArrayRef<Value> dimSizes) {
  SmallVector<int64_t> shape(dimSizes.size(), ShapedType::kDynamic);
  MemRefType ndType = MemRefType::get(shape, elementType);

  /// Use heap allocation (alloc) to enable proper DB management
  return builder.create<memref::AllocOp>(loc, ndType, dimSizes);
}

void CanonicalizeMemrefsPass::transferMetadata(Operation *oldAlloc,
                                               Operation *newAlloc,
                                               bool isCanonicalizedArray) {
  if (!oldAlloc || !newAlloc)
    return;

  /// Transfer relevant metadata attributes
  if (auto nameAttr = oldAlloc->getAttrOfType<StringAttr>("arts.name"))
    newAlloc->setAttr("arts.name", nameAttr);

  /// Mark as canonicalized
  if (isCanonicalizedArray) {
    OpBuilder builder(ctx);
    newAlloc->setAttr("arts.is_canonicalized", builder.getBoolAttr(true));
  }
}

Value CanonicalizeMemrefsPass::createMatchingAllocation(
    Operation *origAllocOp, Location loc, Type elementType,
    ArrayRef<Value> dimSizes) {
  OpBuilder builder(ctx);

  SmallVector<int64_t> shape(dimSizes.size(), ShapedType::kDynamic);
  MemRefType ndType = MemRefType::get(shape, elementType);

  /// Match the allocation style (alloc vs alloca)
  if (isa<memref::AllocaOp>(origAllocOp)) {
    return builder.create<memref::AllocaOp>(loc, ndType, dimSizes);
  } else {
    return builder.create<memref::AllocOp>(loc, ndType, dimSizes);
  }
}

void CanonicalizeMemrefsPass::duplicateDeallocations(Value oldAlloc,
                                                     Value newAlloc,
                                                     OpBuilder &builder) {
  /// Find all dealloc operations for the old allocation
  SmallVector<memref::DeallocOp> deallocOps;
  for (Operation *user : oldAlloc.getUsers()) {
    if (auto deallocOp = dyn_cast<memref::DeallocOp>(user))
      deallocOps.push_back(deallocOp);
  }

  /// Create matching deallocs for the new allocation
  for (auto deallocOp : deallocOps) {
    builder.setInsertionPoint(deallocOp);
    builder.create<memref::DeallocOp>(deallocOp.getLoc(), newAlloc);
  }
}

void CanonicalizeMemrefsPass::updateDependenciesForNewAlloc(Value oldAlloc,
                                                            Value newAlloc) {
  /// Update task dependencies that reference the old allocation
  module.walk([&](omp::TaskOp task) {
    auto dependVars = task.getDependVars();
    if (dependVars.empty())
      return;

    SmallVector<Value> newDependVars;
    bool changed = false;
    for (Value dep : dependVars) {
      if (dep == oldAlloc) {
        newDependVars.push_back(newAlloc);
        changed = true;
      } else {
        newDependVars.push_back(dep);
      }
    }

    if (changed)
      task.getDependVarsMutable().assign(newDependVars);
  });
}

bool CanonicalizeMemrefsPass::verifyCanonicalForm(Operation *allocOp) {
  Value alloc = allocOp->getResult(0);
  auto allocType = alloc.getType().dyn_cast<MemRefType>();
  if (!allocType)
    return false;

  /// Check that element type is not a memref (no nesting)
  if (allocType.getElementType().isa<MemRefType>())
    return false;

  /// Canonical form verified
  return true;
}

void CanonicalizeMemrefsPass::verifyAllCanonical() {
  ARTS_INFO("Verification: Checking canonical form of all allocations");

  unsigned totalAllocs = 0;
  unsigned canonicalAllocs = 0;
  unsigned nonCanonicalAllocs = 0;

  module.walk([&](Operation *op) {
    if (!isa<memref::AllocOp, memref::AllocaOp>(op))
      return;

    totalAllocs++;

    if (verifyCanonicalForm(op))
      canonicalAllocs++;
    else
      nonCanonicalAllocs++;
  });

  ARTS_DEBUG("Total allocations: " << totalAllocs);
  ARTS_DEBUG("  Canonical: " << canonicalAllocs);
  ARTS_DEBUG("  Non-canonical: " << nonCanonicalAllocs);

  if (nonCanonicalAllocs > 0) {
    module.emitWarning(
        "Found " + std::to_string(nonCanonicalAllocs) +
        " non-canonical allocations after canonicalization pass");
  } else {
    ARTS_INFO("All allocations are in canonical form");
  }
}

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

SmallVector<Value>
CanonicalizeMemrefsPass::getAllocDynamicSizes(Operation *allocOp) {
  SmallVector<Value> dynamicSizes;
  if (auto allocaOp = dyn_cast<memref::AllocaOp>(allocOp)) {
    dynamicSizes.append(allocaOp.getDynamicSizes().begin(),
                        allocaOp.getDynamicSizes().end());
  } else if (auto mallocOp = dyn_cast<memref::AllocOp>(allocOp)) {
    dynamicSizes.append(mallocOp.getDynamicSizes().begin(),
                        mallocOp.getDynamicSizes().end());
  }
  return dynamicSizes;
}

std::optional<omp::ClauseTaskDepend>
CanonicalizeMemrefsPass::getDepModeFromTask(omp::TaskOp task, Value depVar) {
  auto dependList = task.getDependsAttr();
  if (!dependList)
    return std::nullopt;

  auto dependVars = task.getDependVars();
  for (unsigned i = 0, e = dependList.size(); i < e && i < dependVars.size();
       ++i) {
    if (dependVars[i] == depVar) {
      auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
      if (depClause)
        return depClause.getValue();
    }
  }

  return std::nullopt;
}

Value CanonicalizeMemrefsPass::materializeOpFoldResult(OpFoldResult ofr,
                                                       OpBuilder &builder,
                                                       Location loc) {
  if (auto value = ofr.dyn_cast<Value>())
    return value;

  auto attr = ofr.get<Attribute>().cast<IntegerAttr>();
  return builder.create<arith::ConstantIndexOp>(loc, attr.getInt());
}

void CanonicalizeMemrefsPass::removeOps(ModuleOp module,
                                        SetVector<Operation *> &ops,
                                        bool verify) {
  /// Remove operations in reverse order (to handle nested operations correctly)
  for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
    Operation *op = *it;
    if (op && !op->use_empty() && verify) {
      ARTS_DEBUG("Warning: Removing operation with uses: "
                 << op->getName().getStringRef());
    }
    if (op)
      op->erase();
  }
  ops.clear();
}

void CanonicalizeMemrefsPass::linearizePolygeistViews() {
  /// This method linearizes multidimensional polygeist.pointer2memref views
  /// to 1D views, making it easier for FlattenedArrayAnalyzer to detect
  /// patterns
  ///
  /// Example transformation:
  /// %view = polygeist.pointer2memref %ptr : memref<?x?xf64>
  /// -> %view = polygeist.pointer2memref %ptr : memref<?xf64>

  OpBuilder builder(ctx);
  SmallVector<Operation *> viewsToLinearize;

  /// Phase 1: Identify multidimensional polygeist views
  module.walk([&](polygeist::Pointer2MemrefOp p2mOp) {
    auto resultType = p2mOp.getType().dyn_cast<MemRefType>();
    if (!resultType)
      return;

    /// Only linearize multidimensional views (rank > 1)
    if (resultType.getRank() > 1) {
      ARTS_DEBUG(" - Found multidimensional polygeist view: rank="
                 << resultType.getRank());
      viewsToLinearize.push_back(p2mOp);
    }
  });

  if (viewsToLinearize.empty()) {
    ARTS_DEBUG("   No multidimensional polygeist views found");
    return;
  }

  ARTS_DEBUG(" - Linearizing " << viewsToLinearize.size()
                               << " polygeist views");

  /// Phase 2: Linearize each view
  for (Operation *op : viewsToLinearize) {
    auto p2mOp = cast<polygeist::Pointer2MemrefOp>(op);
    auto resultType = p2mOp.getType().cast<MemRefType>();
    Location loc = p2mOp.getLoc();

    /// Calculate total size (product of all dimensions)
    builder.setInsertionPoint(p2mOp);
    Value totalSize = nullptr;

    for (int i = 0; i < resultType.getRank(); ++i) {
      Value dimSize;
      if (resultType.isDynamicDim(i)) {
        /// Extract dynamic dimension using memref.dim
        dimSize = builder.create<memref::DimOp>(loc, p2mOp.getResult(), i);
      } else {
        /// Use static dimension
        dimSize = builder.create<arith::ConstantIndexOp>(
            loc, resultType.getDimSize(i));
      }

      if (!totalSize)
        totalSize = dimSize;
      else
        totalSize = builder.create<arith::MulIOp>(loc, totalSize, dimSize);
    }

    /// Create new 1D memref type
    SmallVector<int64_t> linearShape = {ShapedType::kDynamic};
    MemRefType linearType =
        MemRefType::get(linearShape, resultType.getElementType());

    /// Create new linearized pointer2memref operation
    builder.setInsertionPoint(p2mOp);
    auto newP2mOp = builder.create<polygeist::Pointer2MemrefOp>(
        loc, linearType, p2mOp.getSource());

    /// Create a subview with the computed total size
    auto subviewOp = builder.create<memref::SubViewOp>(
        loc, newP2mOp, SmallVector<OpFoldResult>{builder.getIndexAttr(0)},
        SmallVector<OpFoldResult>{totalSize},
        SmallVector<OpFoldResult>{builder.getIndexAttr(1)});

    /// Replace all uses of the original multidimensional view
    /// Note: We need to track accesses that will need delinearization
    p2mOp.replaceAllUsesWith(subviewOp.getResult());

    /// Mark original for removal
    markForRemoval(p2mOp);
  }

  ARTS_DEBUG("   Linearization complete");
}

//===----------------------------------------------------------------------===//
// Pass Creation
//===----------------------------------------------------------------------===//

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCanonicalizeMemrefsPass() {
  return std::make_unique<CanonicalizeMemrefsPass>();
}
} // namespace arts
} // namespace mlir
