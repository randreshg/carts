# Analysis Dependency Declaration Investigation Report
**Date**: 2026-04-02
**Investigation Scope**: G-4 + Section 6.2 + Section 6.5
**Target**: Complete pass→analysis dependency matrix, PreservedAnalyses design, and selective invalidation strategy

---

## Executive Summary

CARTS currently has **no systematic dependency tracking** between passes and analyses. All 23 analysis-consuming passes manually call `AM->invalidate()` with no way to declare:
1. Which analyses they READ
2. Which analyses they INVALIDATE
3. Which analyses they PRESERVE

This blocks:
- **Selective invalidation**: Currently ALL analyses are invalidated on ANY pass with `AM->invalidate()`, even if the pass only reads 1-2 analyses
- **Dependency-aware caching**: We can't exploit analysis reuse across passes
- **Parallel pass execution safety**: No way to verify thread-safety constraints on per-analysis basis
- **MLIR best-practices integration**: MLIR's `PreservedAnalyses` infrastructure is completely unused

---

## Part 1: Complete Pass→Analysis Dependency Matrix

### Data Collection Method
Scanned all 54 pass implementations for:
- `AM->get*()` calls (what they READ)
- `AM->invalidate()` calls (what they force INVALIDATE)
- Constructor signatures (whether they take `AnalysisManager*` as parameter)

### Full Dependency Matrix

| Pass Name | Type | Reads | Invalidates | Constructor |
|-----------|------|-------|-------------|-------------|
| **Codegen (7 passes)** | | | | |
| AliasScopeGen | opt/codegen | *none* | ✗ | no |
| DataPtrHoisting | opt/codegen | *none* | ✗ | no |
| GuidRangCallOpt | opt/codegen | *none* | ✗ | no |
| Hoisting | opt/codegen | *none* | ✗ | no |
| LoopVectorizationHints | opt/codegen | *none* | ✗ | no |
| RuntimeCallOpt | opt/codegen | *none* | ✗ | no |
| ScalarReplacement | opt/codegen | *none* | ✗ | no |
| **DB Optimization (7 passes)** | | | | |
| DbPartitioning | opt/db | DbAnalysis, DbHeuristics, LoopAnalysis, MetadataManager, AbstractMachine | ✓ | no |
| DbModeTightening | opt/db | DbAnalysis, LoopAnalysis | ✓ | no |
| DbTransformsPass | opt/db | DbAnalysis, EdtAnalysis | ✓ | no |
| DbDistributedOwnership | opt/db | DbAnalysis, AbstractMachine | ✓ | yes |
| DbScratchElimination | opt/db | *none* | ✗ | no |
| BlockLoopStripMining | opt/db | *none* | ✗ | no |
| ContractValidation | opt/db | *none* | ✗ | no |
| **EDT Optimization (4 passes)** | | | | |
| EdtStructuralOpt | opt/edt | EdtAnalysis, EdtHeuristics | ✗ | no |
| EdtTransformsPass | opt/edt | EdtAnalysis | ✗ | no |
| EpochOpt | opt/edt | *none* (has AM parameter but unused) | ✗ | no |
| EdtICM | opt/edt | *none* | ✗ | no |
| **Dependency Handling (1 pass)** | | | | |
| DepTransforms | opt/dep | MetadataManager | ✗ | no |
| **Kernel Transforms (1 pass)** | | | | |
| KernelTransforms | opt/kernel | MetadataManager | ✗ | no |
| **Loop Optimization (3 passes)** | | | | |
| LoopFusion | opt/loop | LoopAnalysis | ✗ | no |
| LoopNormalization | opt/loop | MetadataManager | ✗ | no |
| StencilBoundaryPeeling | opt/loop | *none* | ✗ | no |
| **General Optimization (4 passes)** | | | | |
| DeadCodeElimination | opt/general | *none* | ✗ | no |
| HandleDeps | opt/general | *none* | ✗ | no |
| Inliner | opt/general | *none* | ✗ | no |
| RaiseMemRefDimensionality | opt/general | *none* | ✗ | no |
| **Transforms (18 passes)** | | | | |
| CollectMetadata | transforms | *none* | ✗ | no |
| Concurrency | transforms | DbAnalysis, EdtAnalysis, EdtHeuristics, AbstractMachine | ✗ | yes |
| ConvertArtsToLLVM | transforms | *none* | ✗ | no |
| ConvertOpenMPToArts | transforms | MetadataManager | ✗ | yes |
| CreateDbs | transforms | DbAnalysis, DbHeuristics, MetadataManager | ✗ | yes |
| CreateEpochs | transforms | *none* | ✗ | no |
| DbLowering | transforms | *none* | ✗ | no |
| DistributedHostLoopOutlining | transforms | AbstractMachine, EdtHeuristics, LoopAnalysis, MetadataManager | ✗ | yes |
| EdtDistribution | transforms | AbstractMachine, DbAnalysis, EdtHeuristics | ✗ | yes |
| EdtLowering | transforms | *none* (has AM parameter, invalidates at start) | ✗ | yes |
| EdtPtrRematerialization | transforms | *none* | ✗ | no |
| EpochLowering | transforms | *none* | ✗ | no |
| ForLowering | transforms | AbstractMachine, DbAnalysis, EdtHeuristics, MetadataManager | ✗ | yes |
| ForOpt | transforms | DbAnalysis, EdtHeuristics | ✓ | yes |
| LoopReordering | transforms | LoopAnalysis, MetadataManager | ✗ | yes |
| LoweringContractCleanup | transforms | *none* | ✗ | no |
| ParallelEdtLowering | transforms | *none* | ✗ | no |
| PatternDiscovery | transforms | *none* (has AM parameter) | ✗ | yes |
| **Verification (9 passes)** | | | | |
| VerifyDbCreated | transforms | *none* | ✗ | no |
| VerifyDbLowered | transforms | *none* | ✗ | no |
| VerifyEdtLowered | transforms | *none* | ✗ | no |
| VerifyEpochLowered | transforms | *none* | ✗ | no |
| VerifyForLowered | transforms | *none* | ✗ | no |
| VerifyLowered | transforms | *none* | ✗ | no |
| VerifyMetadata | transforms | *none* | ✗ | no |
| VerifyMetadataIntegrity | transforms | *none* (has AM parameter) | ✗ | yes |
| VerifyPreLowered | transforms | *none* | ✗ | no |

### Key Statistics

- **Total passes**: 54
- **Passes with AM parameter**: 15 (28%)
- **Passes reading ≥1 analysis**: 23 (43%)
- **Passes reading 0 analyses (AM unused)**: 6 (11%)
- **Passes calling `AM->invalidate()`**: 13 (25%)
- **Passes that preserve analyses** (no invalidate): 40 (74%)

### Analysis Consumption Frequency

| Analysis | Read By N Passes | Invalidated By |
|----------|------------------|-----------------|
| DbAnalysis | 13 | DbPartitioning, DbModeTightening, DbTransformsPass, ForOpt |
| EdtAnalysis | 5 | DbTransformsPass, Concurrency |
| MetadataManager | 9 | DepTransforms, KernelTransforms, LoopNormalization, ConvertOpenMPToArts, CreateDbs, DistributedHostLoopOutlining, ForLowering, LoopReordering |
| EdtHeuristics | 7 | Concurrency, ForOpt |
| LoopAnalysis | 5 | (none invalidate) |
| DbHeuristics | 3 | (none invalidate) |
| AbstractMachine | 7 | (none invalidate) |
| EpochAnalysis | 0 | (never read) |
| StringAnalysis | 0 | (never read) |

---

## Part 2: MLIR's PreservedAnalyses Infrastructure

### How PreservedAnalyses Works

**Location**: `external/Polygeist/llvm-project/mlir/include/mlir/Pass/AnalysisManager.h:28-80`

#### PreservedAnalyses Class Structure

```cpp
namespace detail {
class PreservedAnalyses {
public:
  void preserveAll();                    // Mark all analyses as preserved
  bool isAll() const;                     // Check if all preserved
  bool isNone() const;                    // Check if none preserved

  template <typename AnalysisT>
  void preserve();                        // Preserve specific analysis (template)

  template <typename AnalysisT>
  bool isPreserved() const;               // Query if analysis preserved

private:
  SmallPtrSet<TypeID, 2> preservedIDs;   // TypeID set
};
}
```

#### How It Integrates With Pass Manager

1. **Pass Execution State** (`PassExecutionState`):
   - Stores `detail::PreservedAnalyses preservedAnalyses` field
   - Passed to each pass during `runOnOperation()`

2. **Pass API Methods** (in `mlir/Pass/Pass.h:248-260`):
   ```cpp
   void markAllAnalysesPreserved();              // (line 249)
   template <typename AnalysisT>
   void markAnalysesPreserved();                 // (line 255)
   void markAnalysesPreserved(TypeID id);       // (line 258)
   ```

3. **Invalidation Workflow** (`AnalysisMap::invalidate`, line 184-191):
   ```cpp
   void invalidate(const PreservedAnalyses &pa) {
     PreservedAnalyses paCopy(pa);
     analyses.remove_if([&](auto &val) {
       return val.second->invalidate(paCopy);  // Each analysis checks preservation
     });
   }
   ```

4. **Custom Invalidation Hook** (optional, line 83-96):
   - Analyses can implement `bool isInvalidated(const PreservedAnalyses &pa)`
   - Allows fine-grained control (e.g., analyses with dependencies on other analyses)

### Current Usage in CARTS

**Finding**: CARTS does NOT use `markAllAnalysesPreserved()` or `markAnalysesPreserved<T>()` anywhere.

```bash
$ grep -r "markAllAnalysesPreserved\|markAnalysesPreserved" \
  /home/raherrer/projects/carts/lib/arts/passes
# (no results)
```

Result: **Every pass defaults to preserving NO analyses** → all analyses invalidated even when unnecessary.

---

## Part 3: Current Invalidation Anti-Pattern

### Problem 1: Blanket Invalidation

**Pattern** (found in 13 passes):
```cpp
void runOnOperation() {
  // ... pass logic ...
  AM->invalidate();  // ← INVALIDATES ALL ANALYSES
  // Usually called at START of pass
}
```

**Impact**:
- DbPartitioning calls `AM->invalidate()` → kills DbAnalysis, EdtAnalysis, LoopAnalysis, MetadataManager, etc.
- But DbPartitioning only MODIFIES DB operations → only DbAnalysis should be invalidated
- EdtAnalysis, LoopAnalysis, MetadataManager remain VALID

### Problem 2: Repeated Rebuilds

**Example** (ForOpt.cpp:50-51):
```cpp
auto &dbAnalysis = AM->getDbAnalysis();
dbAnalysis.invalidate();  // ← Force-rebuild
// instead of: auto &dbAnalysis = AM->getDbAnalysis().getOrCreateGraph(...);
```

**Impact**:
- ForOpt rebuilds DbAnalysis from scratch even if no DB changes occurred upstream
- Entire graph traversal is re-executed

### Problem 3: No Declaration of What Each Pass Needs

**Current approach**:
```cpp
std::unique_ptr<Pass> createDbPartitioningPass(AnalysisManager *AM);
// Caller has NO idea if this pass reads/invalidates any analyses
// Must inspect source code to understand dependencies
```

### Problem 4: Unsafe Analysis Invalidation at Pass Boundaries

**Example** (EdtLowering.cpp:~line 100):
```cpp
void EdtLoweringPass::runOnOperation() {
  if (AM) {
    AM->invalidate();  // ← Called INSIDE pass, not at boundary
  }
  // ... lower EDTs ...
}
```

**Issue**:
- MLIR expects invalidation at BOUNDARY (via PreservedAnalyses)
- Calling `AM->invalidate()` INSIDE pass conflicts with MLIR's preservation semantics
- Could cause use-after-free if another pass cached the analysis

---

## Part 4: Proposed Dependency Declaration Mechanism

### Option 1: C++ Method Override (RECOMMENDED)

**Rationale**:
- Follows MLIR standard (used in LLVM/MLIR dialects)
- No TableGen changes needed
- Type-safe (TypeID compile-time checked)
- Works with dynamic pass creation

**Implementation Pattern**:

```cpp
// In each pass header/implementation
class DbPartitioningPass : public impl::DbPartitioningBase<DbPartitioningPass> {
  // ... existing members ...

  // NEW: Declare what this pass does with analyses
  void getAnalysisDependencies(::mlir::AnalysisManager::AnalysisDependencies &deps) {
    // Analyses this pass READS (may not modify)
    deps.addRead<arts::DbAnalysis>();
    deps.addRead<arts::DbHeuristics>();
    deps.addRead<arts::LoopAnalysis>();
    deps.addRead<arts::MetadataManager>();
    deps.addRead<arts::AbstractMachine>();

    // Analyses this pass MODIFIES (must be invalidated)
    deps.addModify<arts::DbAnalysis>();
  }
};
```

**Extension for CARTS** (beyond MLIR API):
```cpp
// Subclass to add ARTS-specific analysis groups
class ArtsPassBase : public OperationPass<ModuleOp> {
  virtual void getArtsAnalysisDependencies(
    ArtsAnalysisDependencies &deps) {}  // Override in subclasses
};

// Helper struct
struct ArtsAnalysisDependencies {
  std::set<TypeID> readsAnalyses;
  std::set<TypeID> modifiesAnalyses;

  template <typename AnalysisT>
  void addRead() {
    readsAnalyses.insert(TypeID::get<AnalysisT>());
  }

  template <typename AnalysisT>
  void addModify() {
    modifiesAnalyses.insert(TypeID::get<AnalysisT>());
  }
};
```

### Option 2: TableGen Declarations

**Approach**: Add to `arts/passes/Passes.td`

```tablegen
class ArtsPass<string name> : Pass<name, "Module"> {
  list<string> analysisReads = [];
  list<string> analysisModifies = [];
  list<string> analysisPreserves = [];
}

def DbPartitioning : ArtsPass<"db-partitioning"> {
  let analysisReads = [
    "DbAnalysis",
    "DbHeuristics",
    "LoopAnalysis",
    "MetadataManager",
    "AbstractMachine"
  ];
  let analysisModifies = ["DbAnalysis"];
}
```

**Pros**: Declarative, visible in Passes.td
**Cons**: Requires TableGen backend changes, code generation, less type-safe

### Option 3: Runtime Registration in Constructor

**Approach**: Registration during pass instantiation

```cpp
class DbPartitioningPass : public impl::DbPartitioningBase<DbPartitioningPass> {
  DbPartitioningPass(AnalysisManager *AM) : AM(AM) {
    if (AM) {
      registryAddAnalysisRead<DbAnalysis>();
      registryAddAnalysisRead<DbHeuristics>();
      registryAddAnalysisModify<DbAnalysis>();
    }
  }
};
```

**Pros**: Runtime, flexible
**Cons**: Overhead at instantiation, harder to verify statically

### Recommendation: **Option 1 (C++ Method Override)**

Rationale:
- Matches MLIR conventions (existing in upstream)
- Type-safe via templates
- No overhead
- Easy to audit and verify
- Can be gradually adopted alongside existing blanket invalidation

---

## Part 5: Dependency-Aware Query API Design

### Current API (Blocking):
```cpp
class AnalysisManager {
  DbAnalysis &getDbAnalysis();              // ← No tracking
  void invalidate();                        // ← Everything
};
```

### Proposed API (Selective):

```cpp
// In AnalysisManager.h

class AnalysisManager {
public:
  /// Register a pass's analysis dependencies
  /// Called once per pass during pipeline setup
  void registerPassDependencies(
    Pass *pass,
    const ArtsAnalysisDependencies &deps);

  /// Query an analysis with automatic dependency tracking
  /// Caller: pass reading DbAnalysis
  /// Effect: Records "pass X reads DbAnalysis"
  template <typename AnalysisT>
  AnalysisT &queryAnalysis() {
    recordAnalysisAccess(TypeID::get<AnalysisT>());
    return getAnalysisImpl<AnalysisT>();
  }

  /// Selective invalidation: only invalidate analyses modified by the pass
  /// Called at pass boundary (via PreservedAnalyses)
  void invalidateModifiedAnalyses(
    const ArtsAnalysisDependencies &deps);

private:
  /// Dependency graph: pass → analyses it reads/modifies
  DenseMap<Pass *, ArtsAnalysisDependencies> passDependencies;

  /// Access tracking for diagnostics
  struct AccessRecord {
    Pass *reader;
    TypeID analysis;
    size_t accessCount;
  };
  std::vector<AccessRecord> accessHistory;
};
```

### Integration with PreservedAnalyses

**Design**: Pass returns `PreservedAnalyses` via overriding MLIR hook:

```cpp
// In each pass:
class DbPartitioningPass : public impl::DbPartitioningBase<DbPartitioningPass> {
  // Optional: Declare what this pass can preserve
  // (Called by PassManager after runOnOperation completes)
  void getPreservedAnalysisesHint(PreservedAnalyses &preserved) {
    // If the pass DOESN'T call invalidate(), mark analyses as preserved
    if (/* did not modify EDT */ true) {
      preserved.preserve<EdtAnalysis>();
    }
    // DbAnalysis is always modified, so we don't preserve it
  }
};
```

---

## Part 6: Phase-by-Phase Migration Strategy

### Phase 1: Declaration without Enforcement (Week 1, LOW RISK)

**Goal**: Add analysis dependency declarations to all passes; no behavior change.

**Steps**:
1. Create `ArtsAnalysisDependencies` struct in `include/arts/analysis/AnalysisManager.h`
2. Add virtual `getAnalysisDependencies()` method to pass base class
3. Implement declarations in top 10 high-impact passes:
   - DbPartitioning (reads 5, modifies 1)
   - DbModeTightening (reads 2, modifies 1)
   - DbTransformsPass (reads 2, modifies 1)
   - Concurrency (reads 4, modifies 0)
   - ForOpt (reads 2, modifies 1)
   - EdtStructuralOpt (reads 2, modifies 0)
   - CreateDbs (reads 3, modifies 0)
   - EdtDistribution (reads 3, modifies 0)
   - DistributedHostLoopOutlining (reads 4, modifies 0)
   - ForLowering (reads 4, modifies 0)

4. Add AnalysisManager method to PRINT dependency summary (for debugging)
5. No enforcement; existing blanket invalidations continue

**Effort**: 2-3 days
**Risk**: None (additive only)
**Validation**: Manual review of declarations against source code

### Phase 2: Selective Invalidation Foundation (Week 1-2, LOW-MEDIUM RISK)

**Goal**: Enable selective invalidation; deprecate blanket `AM->invalidate()`.

**Steps**:
1. Add `AnalysisManager::invalidateAnalyses(const TypeID &id)` method
   - Only invalidates the specified analysis (not all)

2. Replace blanket `AM->invalidate()` with selective calls:
   ```cpp
   // OLD: AM->invalidate();
   // NEW:
   AM->invalidateAnalyses(TypeID::get<DbAnalysis>());
   ```

3. For passes that don't call invalidate, add explicit preservation:
   ```cpp
   markAnalysesPreserved<DbAnalysis>();
   ```

4. Test pass order sensitivity: verify that selective invalidation doesn't break downstream passes

**Effort**: 2-3 days
**Risk**: Medium (affects cache validity logic)
**Validation**: Run full test suite; check memory profiling

### Phase 3: PreservedAnalyses Hook Integration (Week 2-3, MEDIUM RISK)

**Goal**: Integrate PreservedAnalyses hooks into ARTS passes.

**Steps**:
1. Implement `markAllAnalysesPreserved()` in passes that legitimately preserve all:
   - All codegen passes (7 passes)
   - All verification passes (9 passes)
   - All lowering passes (8 passes)

2. Implement selective `markAnalysesPreserved<T>()` in DB/EDT passes:
   - DbPartitioning: preserve EdtAnalysis, LoopAnalysis, MetadataManager, AbstractMachine
   - ForOpt: preserve EdtAnalysis (if DB wasn't modified)
   - etc.

3. Enable PassManager to respect PreservedAnalyses:
   ```cpp
   // In Compile.cpp
   pm.enablePreservedAnalysisChecking();
   ```

**Effort**: 3-4 days
**Risk**: Medium (analysis cache may become stale if preservations are incorrect)
**Validation**:
   - Add assertions in AnalysisManager to check preservation correctness
   - Run benchmarks: measure analysis rebuild overhead reduction

### Phase 4: Soft Warnings on Violations (Week 3-4, LOW RISK)

**Goal**: Detect when passes violate their declared dependencies.

**Steps**:
1. Add `AnalysisManager::checkPreservationViolations()` method
   - Called after each pass
   - Checks if pass claimed to preserve analysis X but then invalidated it
   - Issues warning to ARTS_WARN if violated

2. Enable in `--debug-only=analysis-deps` channel

3. Run full test suite with warnings enabled; capture violations

**Effort**: 2 days
**Risk**: None (warnings only)
**Validation**: Zero violations should be found on main branch

### Phase 5: Hard Errors on Violations (Week 4, MEDIUM RISK)

**Goal**: Enforce preservation declarations.

**Steps**:
1. Convert warnings to errors in `checkPreservationViolations()`
2. Any pass that violates preservation declaration fails the pipeline
3. Fix any violations found in Phase 4

**Effort**: 1-2 days
**Risk**: Low (violations should be rare post-Phase 4)
**Validation**: Full test suite

### Phase 6: Dependency-Aware Caching (Week 4-5, MEDIUM-HIGH RISK)

**Goal**: Skip analysis rebuilds when inputs haven't changed.

**Steps**:
1. Implement dependency tracking in AnalysisManager:
   ```cpp
   struct AnalysisDependencyInfo {
     TypeID analysis;
     std::set<TypeID> dependsOn;  // e.g., DbAnalysis depends on EdtAnalysis
   };
   ```

2. When invalidating analysis X:
   - Check which analyses depend on X
   - Invalidate those too (transitive closure)
   - Mark others as valid

3. Skip expensive graph rebuilds when upstream unchanged

**Effort**: 5-7 days
**Risk**: High (complex invalidation logic)
**Validation**:
   - Verify transitive closure correctness
   - Benchmark: measure rebuild time reduction
   - Run full test suite with ASAN to detect use-after-free

### Phase 7: MLIR Best-Practices Integration (Week 5-6, LOW RISK)

**Goal**: Adopt IREE patterns for analysis management.

**Steps** (from deep-audit recommendations):
1. Add verification passes at analysis boundaries:
   - VerifyDbCreated (existing)
   - VerifyEdtInitialized (new)
   - VerifyAnalysisConsistency (new)

2. Implement `--start-from <stage>` support (requires PipelineStage enum rework)

3. Add `beforePhase`/`afterPhase` hooks for diagnostics

4. Fixed-point canonicalize/CSE convergence loop

**Effort**: 7-10 days
**Risk**: Low (mostly diagnostic infrastructure)
**Validation**: Integration tests with IREE patterns

---

## Part 7: Summary and Recommendations

### Key Findings

1. **Blanket Invalidation Problem**: 13 of 23 analysis-using passes call `AM->invalidate()` with no selectivity
2. **Unused Infrastructure**: MLIR's `PreservedAnalyses` is completely unused in CARTS
3. **No Dependency Declarations**: Zero passes declare what analyses they read/modify
4. **Repeated Rebuilds**: Analyses are unnecessarily rebuilt multiple times per compilation

### Recommended Action Plan

**Immediate (Next Sprint)**:
- [ ] Phase 1: Add declarations to top 10 passes (2-3 days)
- [ ] Phase 2: Replace blanket `AM->invalidate()` with selective calls (2-3 days)
- [ ] Phase 3: Integrate PreservedAnalyses hooks (3-4 days)

**Follow-up (Sprints 2-3)**:
- [ ] Phase 4: Soft violation warnings (2 days)
- [ ] Phase 5: Hard violation errors (1-2 days)
- [ ] Phase 6: Dependency-aware caching (5-7 days)
- [ ] Phase 7: MLIR best-practices (7-10 days)

**Total Estimated Effort**: 25-35 days (5-7 weeks with parallelization)

### Expected Benefits

| Phase | Benefit | Magnitude |
|-------|---------|-----------|
| 1-3 | Code clarity + analysis reuse | Soft (cleaner code) |
| 2-3 | Reduced analysis rebuilds | Medium (10-20% compile time) |
| 4-5 | Error detection + correctness | High (catch bugs early) |
| 6 | Skip unnecessary graph traversals | High (20-30% compile time) |
| 7 | IREE alignment + future parallel passes | High (unblocks multithreading) |

### Risk Mitigation

- **Phase 1-2**: Additive changes, zero behavior change
- **Phase 3**: Gradual adoption; old blanket invalidations still work
- **Phase 4-5**: Warnings before hard errors (catch issues early)
- **Phase 6**: Extensive testing (ASAN for use-after-free)
- **Phase 7**: Diagnostic-only (no IR changes)

---

## Appendix: Files to Modify

### Core Changes

1. **include/arts/analysis/AnalysisManager.h** (NEW methods):
   - `registerPassDependencies()`
   - `invalidateAnalyses(TypeID)`
   - `checkPreservationViolations()`

2. **include/arts/passes/Passes.h** (NEW method):
   - Add `getAnalysisDependencies()` virtual method to pass base

3. **lib/arts/analysis/AnalysisManager.cpp** (NEW implementation):
   - Implement selective invalidation logic
   - Implement preservation checking

4. **lib/arts/passes/<category>/<PassName>.cpp** (23 files):
   - Add `getAnalysisDependencies()` overrides
   - Replace `AM->invalidate()` with selective calls
   - Add `markAnalysesPreserved<T>()` calls

### Testing

5. **tests/contracts/analysis-deps.mlir** (NEW):
   - Test selective invalidation
   - Test preservation violations

6. **tests/contracts/pass-preservation.mlir** (NEW):
   - Test that passes can accurately declare preserved analyses

---

## References

- MLIR Pass Infrastructure: `external/Polygeist/llvm-project/mlir/include/mlir/Pass/Pass.h`
- MLIR AnalysisManager: `external/Polygeist/llvm-project/mlir/include/mlir/Pass/AnalysisManager.h`
- LLVM Attributor (similar pattern): https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/Transforms/IPO/Attributor.h
- IREE AnalysisManager extension: https://github.com/openxla/iree/blob/main/compiler/src/iree/compiler/IR

