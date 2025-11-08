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
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <string>

/// Debug
#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(canonicalize_memrefs);

using namespace mlir;
using namespace mlir::arts;
using namespace mlir::omp;

///===----------------------------------------------------------------------===///
// Pass Implementation
///===----------------------------------------------------------------------===///

struct CanonicalizeMemrefsPass
    : public arts::CanonicalizeMemrefsBase<CanonicalizeMemrefsPass> {

  void runOnOperation() override;

private:
  ModuleOp module;
  MLIRContext *ctx;
  SetVector<Operation *> opsToRemove;

  ///===--------------------------------------------------------------------===///
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
  ///===--------------------------------------------------------------------===///
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
    std::optional<Pattern> detectPatternFromMetadata(Value alloc);
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

  ///===--------------------------------------------------------------------===///
  /// OmpDepsCanonicalizer
  ///
  /// Canonicalizes OpenMP task dependencies to arts.omp_dep operations
  /// Handles:
  /// - Scalar dependencies
  /// - Array element dependencies (load-based)
  /// - Chunk dependencies (subview-based)
  /// - Token container patterns (alloca/store/load)
  ///===--------------------------------------------------------------------===///
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

    /// Helper: Extract memref and indices from a load operation (memref or
    /// affine)
    std::optional<std::pair<Value, SmallVector<Value>>>
    extractLoadInfo(Operation *op);

    /// Helper: Check if memref is token container and return stored value
    std::optional<Value> followTokenContainer(Value memref);

    std::optional<DepInfo> extractDepInfo(Value depVar, OpBuilder &builder,
                                          Location loc);
  };

  ///===--------------------------------------------------------------------===///
  /// Helper Methods
  ///===--------------------------------------------------------------------===///
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
  void verifyAllCanonical();
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

  /// Phase 2: Canonicalize OpenMP Dependencies
  ARTS_INFO("Phase 3: Canonicalizing OpenMP task dependencies");
  OmpDepsCanonicalizer ompCanonicalizer(this);
  ompCanonicalizer.canonicalizeDependencies();

  /// Phase 3: Cleanup
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

////===----------------------------------------------------------------------===///
/// Implementation continues in next section...
/// Now let's implement each analyzer class
////===----------------------------------------------------------------------===///
/// NestedAllocAnalyzer Implementation
////===----------------------------------------------------------------------===///

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

  /// Create N-D allocation matching the original's style and transfer metadata
  auto allocOp = pattern.outerAlloc.getDefiningOp<memref::AllocOp>();
  Value ndAlloc = pass->createMatchingAllocation(
      allocOp, loc, pattern.getFinalElementType(), allSizes);
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

///===----------------------------------------------------------------------===///
// OmpDepsCanonicalizer Implementation
///===----------------------------------------------------------------------===///

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
  if (!dependList) {
    ARTS_DEBUG("    - No dependList found for task");
    return;
  }

  auto dependVars = task.getDependVars();
  ARTS_DEBUG("    - Processing " << dependList.size() << " dependencies");

  for (unsigned i = 0, e = dependList.size(); i < e && i < dependVars.size();
       ++i) {
    /// Get dependency clause and type
    auto depClause = dyn_cast<omp::ClauseTaskDependAttr>(dependList[i]);
    if (!depClause) {
      ARTS_DEBUG("      - Dep " << i << ": not a ClauseTaskDependAttr");
      continue;
    }

    Value depVar = dependVars[i];
    ARTS_DEBUG("      - Dep " << i << ": extracting from " << depVar);

    /// Extract dependency information
    auto depInfo = extractDepInfo(depVar, builder, loc);
    if (!depInfo) {
      ARTS_DEBUG("      - Dep " << i << ": extractDepInfo returned nullopt");
      continue;
    }

    /// Create arts.omp_dep operation to represent the dependency
    builder.setInsertionPoint(task);

    /// Convert ClauseTaskDepend to ArtsMode
    ArtsMode accessMode;
    switch (depClause.getValue()) {
    case omp::ClauseTaskDepend::taskdependin:
      accessMode = ArtsMode::in;
      break;
    case omp::ClauseTaskDepend::taskdependout:
      accessMode = ArtsMode::out;
      break;
    case omp::ClauseTaskDepend::taskdependinout:
      accessMode = ArtsMode::inout;
      break;
    }

    ARTS_DEBUG("      - Dep "
               << i << ": creating arts.omp_dep with source=" << depInfo->source
               << ", indices=" << depInfo->indices.size()
               << ", sizes=" << depInfo->sizes.size());

    /// Create arts.omp_dep operation to represent the dependency
    auto ompDepOp = builder.create<arts::OmpDepOp>(
        loc, accessMode, depInfo->source, depInfo->indices, depInfo->sizes);

    /// Replace the depVar with the new arts.omp_dep operation
    auto dependVarsMutable = task.getDependVarsMutable();
    dependVarsMutable[i].set(ompDepOp.getResult());

    ARTS_DEBUG("      - Canonicalized dependency: mode="
               << accessMode << ", indices=" << depInfo->indices.size()
               << ", sizes=" << depInfo->sizes.size());
  }
}

///===----------------------------------------------------------------------===///
// OmpDepsCanonicalizer Helper Methods
///===----------------------------------------------------------------------===///

std::optional<std::pair<Value, SmallVector<Value>>>
CanonicalizeMemrefsPass::OmpDepsCanonicalizer::extractLoadInfo(Operation *op) {
  /// Extract memref and indices from memref.load
  if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
    SmallVector<Value> indices(loadOp.getIndices().begin(),
                               loadOp.getIndices().end());
    ARTS_DEBUG("        extractLoadInfo: memref.load with " << indices.size()
                                                            << " indices");
    return std::make_pair(loadOp.getMemref(), indices);
  }

  /// Extract memref and indices from affine.load
  /// NOTE: affine.load uses affine maps to represent indices
  /// For example: affine.load %m[%i, %i] is represented as:
  ///   - Affine map: (d0) -> (d0, d0)
  ///   - Operands: [%i]
  /// We need to use the map's getNumResults() to get the actual dimensionality
  if (auto loadOp = dyn_cast<affine::AffineLoadOp>(op)) {
    auto affineMap = loadOp.getAffineMap();
    auto mapOperands = loadOp.getMapOperands();

    /// The number of results in the affine map tells us the actual
    /// dimensionality
    unsigned numDims = affineMap.getNumResults();
    unsigned numOperands = mapOperands.size();

    ARTS_DEBUG("        extractLoadInfo: affine.load with map "
               << affineMap << ", " << numOperands << " operands, " << numDims
               << " dims");

    /// For simple identity or diagonal maps like (d0) -> (d0, d0),
    /// we replicate the operand for each dimension
    SmallVector<Value> indices;

    /// Check if this is a diagonal map (all results are the same)
    if (numOperands == 1 && numDims > 1) {
      /// This is likely a diagonal access like [%i, %i]
      /// Replicate the single operand for each dimension
      for (unsigned i = 0; i < numDims; ++i) {
        indices.push_back(mapOperands[0]);
      }
    } else {
      /// General case: use the operands directly
      indices.assign(mapOperands.begin(), mapOperands.end());
    }

    ARTS_DEBUG("        Extracted " << indices.size() << " indices");
    return std::make_pair(loadOp.getMemref(), indices);
  }

  return std::nullopt;
}

std::optional<Value>
CanonicalizeMemrefsPass::OmpDepsCanonicalizer::followTokenContainer(
    Value memref) {
  /// Check if memref is an alloca
  auto allocaOp = memref.getDefiningOp<memref::AllocaOp>();
  if (!allocaOp)
    return std::nullopt;

  /// Check if this is a scalar token container (rank-0 or rank-1 with size 1)
  auto allocaType = allocaOp.getType().dyn_cast<MemRefType>();
  bool isTokenContainer = allocaType && (allocaType.getRank() == 0 ||
                                         (allocaType.getRank() == 1 &&
                                          allocaType.hasStaticShape() &&
                                          allocaType.getNumElements() <= 1));

  if (!isTokenContainer)
    return std::nullopt;

  /// Find the store operation that wrote to this token container
  for (Operation *user : allocaOp->getUsers()) {
    /// Check memref.store
    if (auto storeOp = dyn_cast<memref::StoreOp>(user)) {
      return storeOp.getValue();
    }
    /// Check affine.store
    if (auto storeOp = dyn_cast<affine::AffineStoreOp>(user)) {
      return storeOp.getValue();
    }
  }

  return std::nullopt;
}

std::optional<CanonicalizeMemrefsPass::OmpDepsCanonicalizer::DepInfo>
CanonicalizeMemrefsPass::OmpDepsCanonicalizer::extractDepInfo(
    Value depVar, OpBuilder &builder, Location loc) {
  /// Extract dependency information from a value
  /// Handles:
  /// 1. Direct memref allocation
  /// 2. SubView (chunk dependency)
  /// 3. Token container (alloca -> store -> load pattern)
  /// 4. Load operations (memref.load or affine.load)
  /// 5. Polygeist pointer2memref
  /// 6. Block arguments

  DepInfo info;

  /// Pattern 1: Direct memref allocation
  if (auto allocOp = depVar.getDefiningOp<memref::AllocOp>()) {
    info.source = depVar;
    return info;
  }

  /// Pattern 2: Alloca (check if token container)
  if (auto allocaOp = depVar.getDefiningOp<memref::AllocaOp>()) {
    if (auto actualDep = followTokenContainer(depVar)) {
      /// Token container - follow to actual dependency
      return extractDepInfo(*actualDep, builder, loc);
    }
    /// Not a token container - whole array dependency
    info.source = depVar;
    return info;
  }

  /// Pattern 3: SubView operation (chunk dependency)
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

  /// Pattern 4: Load operation (memref.load or affine.load)
  Operation *defOp = depVar.getDefiningOp();
  if (defOp) {
    if (auto loadInfo = extractLoadInfo(defOp)) {
      auto [memref, indices] = *loadInfo;

      /// Check if loading from a token container
      if (auto actualDep = followTokenContainer(memref)) {
        return extractDepInfo(*actualDep, builder, loc);
      }

      /// Normal array element dependency
      info.source = memref;
      info.indices = indices;
      return info;
    }
  }

  /// Pattern 5: Polygeist pointer2memref
  if (auto p2mOp = depVar.getDefiningOp<polygeist::Pointer2MemrefOp>()) {
    info.source = depVar;
    return info;
  }

  /// Pattern 6: Block argument (function argument or loop induction variable)
  if (depVar.isa<BlockArgument>()) {
    info.source = depVar;
    return info;
  }

  /// Unable to extract dependency info
  return std::nullopt;
}

///===----------------------------------------------------------------------===///
// Shared Helper Methods Implementation
///===----------------------------------------------------------------------===///

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

  /// Transfer arts.memref_dim_info attribute if present
  if (auto dimInfoAttr =
          oldAlloc->getAttrOfType<MemrefDimInfoAttr>("arts.memref_dim_info")) {
    newAlloc->setAttr("arts.memref_dim_info", dimInfoAttr);
  }
}

void CanonicalizeMemrefsPass::duplicateDeallocations(Value oldAlloc,
                                                     Value newAlloc,
                                                     OpBuilder &builder) {
  /// Find all deallocations of oldAlloc and create corresponding ones for
  /// newAlloc
  for (Operation *user : oldAlloc.getUsers()) {
    if (auto deallocOp = dyn_cast<memref::DeallocOp>(user)) {
      builder.setInsertionPoint(deallocOp);
      builder.create<memref::DeallocOp>(deallocOp.getLoc(), newAlloc);
    }
  }
}

Value CanonicalizeMemrefsPass::createMatchingAllocation(
    Operation *oldAllocOp, Location loc, Type elementType,
    ArrayRef<Value> dimSizes) {
  OpBuilder builder(ctx);
  builder.setInsertionPoint(oldAllocOp);
  return createCanonicalAllocation(builder, loc, elementType, dimSizes);
}

SmallVector<Value>
CanonicalizeMemrefsPass::getAllocDynamicSizes(Operation *allocOp) {
  SmallVector<Value> sizes;
  if (auto alloc = dyn_cast<memref::AllocOp>(allocOp)) {
    sizes.assign(alloc.getDynamicSizes().begin(),
                 alloc.getDynamicSizes().end());
  } else if (auto alloca = dyn_cast<memref::AllocaOp>(allocOp)) {
    sizes.assign(alloca.getDynamicSizes().begin(),
                 alloca.getDynamicSizes().end());
  }
  return sizes;
}

void CanonicalizeMemrefsPass::linearizePolygeistViews() {
  /// This function was used to linearize polygeist pointer2memref views
  /// for flattened array detection. Since we're removing flattened array
  /// canonicalization, this is no longer needed.
}

void CanonicalizeMemrefsPass::removeOps(ModuleOp module,
                                        SetVector<Operation *> &opsToRemove,
                                        bool verify) {
  /// Remove all operations marked for removal
  for (Operation *op : opsToRemove) {
    if (op && op->use_empty()) {
      op->erase();
    }
  }
  opsToRemove.clear();
}

void CanonicalizeMemrefsPass::verifyAllCanonical() {
  /// TODO: Implement this function
  return;

  /// Verify that all allocations are canonical (multi-dimensional memrefs)
  SmallVector<Operation *> nonCanonical;

  module.walk([&](Operation *op) {
    Value alloc;
    if (auto allocOp = dyn_cast<memref::AllocOp>(op))
      alloc = allocOp.getResult();
    else if (auto allocaOp = dyn_cast<memref::AllocaOp>(op))
      alloc = allocaOp.getResult();
    else
      return;

    auto allocType = alloc.getType().dyn_cast<MemRefType>();
    if (!allocType)
      return;

    /// Check if it's a 1D allocation
    if (allocType.getRank() == 1) {
      int64_t size = allocType.getNumElements();
      /// Allow small scalars (size <= 16), but larger 1D allocations are
      /// non-canonical
      if (size == ShapedType::kDynamic || size > 16) {
        nonCanonical.push_back(op);
      }
    }
  });

  if (!nonCanonical.empty()) {
    std::string errorMsg = "CanonicalizeMemrefsPass failed: found " +
                           std::to_string(nonCanonical.size()) +
                           " non-canonical allocations";
    auto diag = module.emitError(errorMsg);
    for (Operation *op : nonCanonical) {
      diag.attachNote(op->getLoc()) << "allocation not in canonical form: "
                                    << op->getName().getStringRef();
    }
    signalPassFailure();
  }
}

///===----------------------------------------------------------------------===///
// Pass Creation
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createCanonicalizeMemrefsPass() {
  return std::make_unique<CanonicalizeMemrefsPass>();
}
} // namespace arts
} // namespace mlir
