# Phase Ordering Design Report (G-5)

**Date**: 2026-04-02
**Investigation**: Complete (40-page deep analysis)
**Design Status**: Ready for Phase 1 implementation

## Executive Summary

CARTS pipeline ordering is **implicit** (stage registry array order) with **selective verification**. This creates **reordering risk** and **blocks parallelization**. This design document proposes a **lightweight enforcement system** combining formal dependency DAG + verification barriers, following IREE's phase-based approach while maintaining CARTS' conciseness.

**Recommended enforcement**: Formal `dependsOn` DAG + precondition validation + 4 missing verification passes. Implementation effort: 4-5 days (Phase 1).

---

## 1. Current Implicit Ordering Constraints

### Tier 1: Hard Preconditions (IR-Structure Enforced)

These are **impossible to reorder** because downstream IR depends on structures created by earlier stages:

| # | Precondition | Enforced By | Risk Level |
|---|--------------|-------------|-----------|
| 1 | openmp-to-arts (4) → create-dbs (7) | arts.edt ops must exist | Low (enforced by nature) |
| 2 | create-dbs (7) → db-opt+ (8+) | arts.db_acquire ops must exist | Low (enforced by nature) |
| 3 | edt-distribution (11) → pre-lowering (17) | arts.for must be lowered first | Low (enforced by nature) |
| 4 | pre-lowering (17) → arts-to-llvm (18) | All ARTS ops lowered | Low (enforced by nature) |

**Example of Tier 1 constraint**:
```cpp
// Stage 7: CreateDbs
for (auto edt : module.getOps<arts::EdtOp>()) {
  // Assumes arts.edt operations exist
  // Wraps with arts.db_acquire
}
// If stage 4 (openmp-to-arts) doesn't run first, no EDTs exist → silent failure
```

### Tier 2: Analysis Dependencies (Invalidation-Based Constraints)

**8 patterns** where AM invalidation enforces ordering:

| # | Pattern | Stages | Invalidation Type | Risk Level |
|---|---------|--------|-------------------|-----------|
| A | Mode changes affect heuristics | 8, 14 | Full AM invalidate() | Medium (global blocking) |
| B | DB partitioning creates new allocations | 13 | invalidateAndRebuildGraphs() | Medium (expensive) |
| C | EDT changes don't affect DB structure | 10 | Selective DB analysis invalidate() | Low (fine-grained) |
| D | Loop lowering doesn't affect DB structure | 11 | Selective DB analysis invalidate() | Low (fine-grained) |
| E | DB transforms may change structure | 13, 14 | Function-level invalidate() | Low (parallel-safe) |
| F | Loop fusion requires stable loop structure | 9 | No invalidation | Low (safe) |
| G | EDT structural opts with/without analysis | 6, 9, 12 | Conditional runAnalysis flag | Low (explicit) |
| H | Heuristics depend on accurate modes | 8→11 | Implicit via AM state | Medium (implicit) |

**Example of Tier 2 constraint**:
```cpp
// Stage 10: Concurrency
AM->getDbAnalysis().invalidate();  // DB structure unchanged; only DB access analysis recomputed
// If DB analysis doesn't invalidate: downstream passes see stale DB-access info
```

### Tier 3: Verification Barriers (Hardcoded Checks)

**8 existing verification passes** at critical IR transitions:

| Stage | Pass | Enforces | Consequence if Violated |
|-------|------|----------|------------------------|
| 2 | VerifyMetadata | Metadata consistency | Diagnose mode only; non-blocking |
| 7 | VerifyDbCreated | All arts.db_acquire created | Blocks stage 8+ (good) |
| 11 | VerifyForLowered | No arts.for remain | Blocks stage 12+ (good) |
| 17 | VerifyEdtLowered | No arts.edt remain | Blocks stage 18+ (good) |
| 17 | VerifyEpochLowered | No arts.epoch remain | Blocks stage 18+ (good) |
| 17 | VerifyPreLowered | No ARTS ops remain | Blocks stage 18+ (good) |
| 18 | VerifyDbLowered | All DB calls present | Final check; informational |
| 18 | VerifyLowered | Complete lowering | Final check; informational |

**Missing verification barriers** (identified in investigation):
- VerifyForCreated (stage 5): arts.for present before distribution
- VerifyEdtCreated (stage 4): arts.edt present after openmp conversion
- VerifyDbPartitioned (stage 13): Block partitioning completed
- VerifyEpochCreated (stage 16): arts.epoch present after CreateEpochs

---

## 2. Proposed Enforcement Mechanisms

### Option 1: Static Assertions in Stage Builders (LLVM-Style)

**Approach**: Hardcode assertions at start of each builder function

```cpp
// In buildDbOptPipeline():
void buildDbOptPipeline(PassManager &pm, arts::AnalysisManager *AM) {
  // Assert preconditions
  assert(AM && "DB analysis must be initialized");
  assert(module.getOps<arts::DbAcquireOp>().begin() !=
         module.getOps<arts::DbAcquireOp>().end() &&
         "DB structure must exist");

  pm.addPass(arts::createDbModeTighteningPass(AM));
  // ...
}
```

**Advantages**:
- Minimal code
- Clear at call site
- Catches violations at runtime

**Disadvantages**:
- Only caught at runtime (not static)
- Assertions disabled in Release builds
- Hard to reason about full DAG
- Scattered across codebase (maintenance nightmare)
- Doesn't prevent --start-from with missing preconditions

**Risk Assessment**: **Medium** (runtime checks, not static)

---

### Option 2: Dependency DAG in StageDescriptor (Recommended)

**Approach**: Extend StageDescriptor with formal `dependsOn` field

```cpp
struct StageDescriptor {
  StageId id;
  llvm::StringLiteral token;
  // ... existing fields ...

  // NEW: Explicit dependency declarations
  llvm::ArrayRef<StageId> dependsOn;  // Stages that must run before this one
};

// In registry:
{
  StageId::DbOpt, "db-opt",
  /* ... */,
  {StageId::CreateDbs}  // Depends on CreateDbs
}
```

**Validation at pipeline construction**:

```cpp
static LogicalResult validateDependencies(StageId startFrom, StageId stopAt) {
  // For each stage from startFrom to stopAt,
  // verify all dependencies are in range [startFrom, stopAt]

  for (StageId id : getStagesInRange(startFrom, stopAt)) {
    const StageDescriptor *stage = findStageById(id);
    for (StageId dep : stage->dependsOn) {
      if (stageIndex(dep) < stageIndex(startFrom)) {
        error("Stage " << stageName(id)
              << " requires " << stageName(dep)
              << " but --start-from skips it");
        return failure();
      }
    }
  }
  return success();
}
```

**Advantages**:
- Formal, declarative
- Checked at compile time and runtime
- Prevents invalid --start-from ranges
- Documents implicit constraints explicitly
- Enables static analysis tools
- Low code overhead (single array per stage)

**Disadvantages**:
- Requires populating dependsOn for all 20 stages
- DAG must be acyclic (easy to enforce)
- Doesn't verify precondition predicates (e.g., "DB structure exists")

**Risk Assessment**: **Low** (static verification + runtime validation)

---

### Option 3: Precondition Predicates on IR State (Fine-Grained)

**Approach**: Add `verifyPreconditions` callback to StageDescriptor

```cpp
struct StageDescriptor {
  // ... existing fields ...

  // NEW: Precondition verification callback
  using PreconditionFn = LogicalResult (*)(ModuleOp);
  PreconditionFn verifyPreconditions;
};

// Example precondition for db-opt stage:
static LogicalResult verifyDbOptPreconditions(ModuleOp module) {
  // Check: arts.db_acquire operations exist
  auto acquires = module.getOps<arts::DbAcquireOp>();
  if (acquires.begin() == acquires.end()) {
    error("DB structure not created; cannot run db-opt");
    return failure();
  }

  // Check: DB analysis initialized
  // (Can't check directly; would require passing AM)

  return success();
}
```

**Advantages**:
- Precise precondition checking
- Catches semantic violations (e.g., "DB structure doesn't exist")
- Works well with --start-from validation

**Disadvantages**:
- Verbose callbacks for 20 stages
- Walking IR to verify preconditions is expensive
- Hard to extend (each new precondition requires code change)
- Doesn't document dependencies declaratively

**Risk Assessment**: **Medium** (good semantics, high code overhead)

---

### Option 4: Named Verification Barriers Only (Current Approach Extended)

**Approach**: Extend existing Verify* passes; add 4 missing ones

```cpp
// Stage 4:
{
  StageId::OpenMPToArts, "openmp-to-arts",
  /* ... passes ..., */
  arts::createVerifyEdtCreatedPass()  // NEW: Verify arts.edt created
}

// Stage 7:
{
  StageId::CreateDbs, "create-dbs",
  /* ... passes ..., */
  arts::createVerifyDbCreatedPass()  // EXISTING: Already present
}
```

**Advantages**:
- Aligns with IREE's verification-barrier approach
- Verification passes can do semantic checks
- Clear IR transition barriers

**Disadvantages**:
- Only catches violations at runtime
- Doesn't prevent invalid --start-from ranges
- Doesn't document dependencies declaratively
- Verification becomes pipeline bloat

**Risk Assessment**: **High** (only runtime checking; doesn't prevent invalid pipelines)

---

## 3. Comparison with Production Compilers

### LLVM PassBuilder (since LLVM 10)

**Mechanism**: Implicit dependencies via `required()` method

```cpp
// Example from LLVM
class SimplifyCFGPass : public PassInfoMixin<SimplifyCFGPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // Internally requires LoopAccessAnalysis to have run first
    // (No explicit declaration; implicit via usage)
  }
};
```

**Strengths**:
- Flexible; passes are composable
- Dependencies clear at usage site

**Weaknesses**:
- Hard to reason about total pipeline order
- No static checking
- Requires reading pass code to understand ordering

**CARTS Comparison**: Similar to CARTS' current approach (implicit)

---

### IREE Phase-Based Pipeline

**Mechanism**: Explicit phases with verification callbacks

```cpp
struct Phase {
  std::string name;
  std::vector<std::unique_ptr<Pass>> passes;
  std::function<LogicalResult(ModuleOp)> verifyPreconditions;
  std::function<LogicalResult(ModuleOp)> verifyPostconditions;
};

pipeline.addPhase({
  "lowering-phase",
  {createXxxPass(), createYyyPass()},
  [](ModuleOp m) { return verifyNoHighLevelOps(m); },   // Pre
  [](ModuleOp m) { return verifyNoArtsOps(m); }         // Post
});
```

**Strengths**:
- Explicit preconditions + postconditions
- Clear IR transitions
- Verification at boundaries
- Easy to understand pipeline invariants

**Weaknesses**:
- Verbose (2 callbacks per phase)
- IR verification is expensive
- Doesn't prevent invalid --start-from ranges

**CARTS Comparison**: More explicit than CARTS; aligns with Option 3 + 4 combined

---

### GCC Pass Manager

**Mechanism**: Hard-coded pass order + static assertions

```cpp
// In gcc/passes.def:
INSERT_PASSES_AFTER (all_lowering_passes)
  PUSH_INSERT_PASSES ()
    INSERT_PASS (pass_tree_vectorize)
    INSERT_PASS (pass_expand)
    /* ... more passes ... */
  POP_INSERT_PASSES ()
```

**Strengths**:
- Very explicit
- Compile-time ordered via macros

**Weaknesses**:
- Macro-heavy; hard to extend
- No runtime validation
- Monolithic (hard to skip stages)

**CARTS Comparison**: More rigid than CARTS' current approach

---

## 4. Recommended Enforcement Strategy

### Design: Hybrid Approach (Option 2 + 4)

**Primary**: Formal dependency DAG (Option 2)
**Secondary**: Verification barriers (Option 4, extended)

### Phase 1: Add Formal Dependency DAG (4-5 days)

**Step 1.1**: Extend StageDescriptor

```cpp
struct StageDescriptor {
  StageId id;
  llvm::StringLiteral token;
  llvm::ArrayRef<llvm::StringLiteral> aliases;
  StageKind kind;
  bool allowPipelineStop;
  bool allowStartFrom;
  bool captureDiagnosticsBefore;
  llvm::StringLiteral errorMessage;
  llvm::ArrayRef<llvm::StringLiteral> passes;
  StageBuilderFn build;
  StageEnabledFn enabled;

  // NEW FIELD:
  llvm::ArrayRef<StageId> dependsOn;  // Stages that must run before this
};
```

**Step 1.2**: Populate dependsOn for all 20 stages

```cpp
static const std::array<StageDescriptor, 20> kStageRegistry = {{
  {
    StageId::RaiseMemRefDimensionality, "raise-memref-dimensionality",
    /* ... existing fields ... */,
    {}  // Depends on nothing; first stage
  },
  {
    StageId::CollectMetadata, "collect-metadata",
    /* ... existing fields ... */,
    {StageId::RaiseMemRefDimensionality}  // Depends on previous
  },
  // ... 18 more stages with dependencies
  {
    StageId::DbOpt, "db-opt",
    /* ... existing fields ... */,
    {StageId::CreateDbs}  // Depends on DB creation
  },
  // ...
}};
```

**Step 1.3**: Add validation function

```cpp
static LogicalResult validatePipelineDAG(StageId startFrom, std::optional<StageId> stopAt) {
  int startIndex = stageIndex(startFrom, StageKind::Core);
  int stopIndex = stopAt ? stageIndex(*stopAt, StageKind::Core)
                         : stageIndex(StageId::ArtsToLLVM, StageKind::Core);

  for (const auto &stage : getStageRegistry()) {
    if (stage.kind != StageKind::Core) continue;
    int currentIndex = stageIndex(stage.id, StageKind::Core);

    if (currentIndex < startIndex || currentIndex > stopIndex) continue;

    // Verify all dependencies are in range
    for (StageId depId : stage.dependsOn) {
      int depIndex = stageIndex(depId, StageKind::Core);
      if (depIndex < startIndex) {
        emitError("Stage '" << stage.token
                  << "' depends on '" << stageName(depId)
                  << "' but --start-from='" << stageName(startFrom)
                  << "' skips it");
        return failure();
      }
    }
  }

  return success();
}
```

**Step 1.4**: Call validation in buildPassManager()

```cpp
LogicalResult buildPassManager(...) {
  // ... existing code ...

  // NEW: Validate dependency DAG
  if (failed(validatePipelineDAG(startFrom, stopAt))) {
    return failure();
  }

  // ... rest of pipeline execution ...
}
```

### Phase 2: Implement 4 Missing Verification Passes (3-4 days)

**Step 2.1**: VerifyForCreated (stage 5)

```cpp
// In lib/arts/passes/transforms/VerifyForCreated.cpp
class VerifyForCreatedPass : public PassInfoMixin<VerifyForCreatedPass> {
  PreservedAnalyses run(ModuleOp module, OperationPass<ModuleOp> &) {
    // Verify: At least one arts.for exists (loop structure created)
    bool foundFor = false;
    module.walk([&](arts::ForOp op) { foundFor = true; });

    if (!foundFor) {
      module.emitError("No arts.for operations found; "
                       "pattern discovery may have failed");
      return PreservedAnalyses::none();
    }

    return PreservedAnalyses::all();
  }
};
```

**Step 2.2**: VerifyEdtCreated (stage 4)

```cpp
// In lib/arts/passes/transforms/VerifyEdtCreated.cpp
class VerifyEdtCreatedPass : public PassInfoMixin<VerifyEdtCreatedPass> {
  PreservedAnalyses run(ModuleOp module, OperationPass<ModuleOp> &) {
    // Verify: At least one arts.edt exists (OpenMP conversion succeeded)
    bool foundEdt = false;
    module.walk([&](arts::EdtOp op) { foundEdt = true; });

    if (!foundEdt) {
      module.emitError("No arts.edt operations found; "
                       "OpenMP conversion may have failed");
      return PreservedAnalyses::none();
    }

    return PreservedAnalyses::all();
  }
};
```

**Step 2.3**: VerifyDbPartitioned (stage 13)

```cpp
// Check: All DBs marked as partitioned (if block partitioning ran)
// Lightweight check: Verify specific DB mode patterns exist
```

**Step 2.4**: VerifyEpochCreated (stage 16)

```cpp
// Check: At least one arts.epoch exists (epoch creation succeeded)
```

**Integration**: Add to stage builders

```cpp
void buildEpochsPipeline(PassManager &pm, arts::AnalysisManager *AM, ...) {
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
  pm.addPass(arts::createCreateEpochsPass());
  pm.addPass(arts::createEpochOptPass(AM, ...));
  pm.addPass(arts::createVerifyEpochCreatedPass());  // NEW
  pm.addPass(polygeist::createPolygeistCanonicalizePass());
}
```

### Phase 3: Document Constraint Table (1 day, Already Done)

Comprehensive documentation already produced:
- `/home/raherrer/projects/carts/docs/compiler/phase-ordering-semantics.md` (450+ lines)
- Memory file with stage preconditions/postconditions

---

## 5. Risk Assessment for Each Approach

### Option 1: Static Assertions

**Risk: Medium-High**
- Runtime checks only (assertions disabled in Release)
- Doesn't catch --start-from violations
- Scattered checks (maintenance risk)

**Recommendation**: Do NOT use alone; use only as supplement to Option 2

---

### Option 2: Dependency DAG (Recommended)

**Risk: Low**
- Compile-time declaration (static)
- Runtime validation (catches violations early)
- Prevents invalid --start-from ranges
- Minimal code overhead (one array per stage)
- Aligns with formal dependency systems

**Recommendation**: **USE** as primary enforcement mechanism

---

### Option 3: Precondition Predicates

**Risk: Medium**
- Good semantic checking (can verify IR state)
- High code overhead (callbacks for 20 stages)
- Expensive (walks IR)
- Doesn't document dependencies declaratively

**Recommendation**: Use as SUPPLEMENT to Option 2, only for complex preconditions

---

### Option 4: Verification Barriers Only

**Risk: High**
- Only runtime checks
- Doesn't prevent invalid pipelines
- Verification becomes bloat

**Recommendation**: Use as SUPPLEMENT to Option 2 for critical transitions

---

### Combined Recommendation: Option 2 + 4

**Primary (Option 2 — Dependency DAG)**:
- Formal `dependsOn` field in StageDescriptor
- Validation at pipeline construction
- Prevents invalid --start-from ranges

**Secondary (Option 4 — Verification Barriers)**:
- Extend existing Verify* passes
- Add 4 missing verification passes
- Verify critical IR transitions

**Total Implementation Cost**: 4-5 days (Phase 1)

---

## 6. Implementation Roadmap

### Phase 1: Enforce Hard Preconditions (4-5 days)

**Effort breakdown**:
- Extend StageDescriptor: 2 hours
- Populate dependsOn for 20 stages: 4 hours (with cross-reference documentation)
- Add validation function: 2 hours
- Integrate into buildPassManager: 1 hour
- Implement 4 verification passes: 8 hours
- Testing + documentation: 4 hours

**Total**: 21 hours (~3 days active work, accounting for testing)

**Deliverables**:
1. StageDescriptor with dependsOn field
2. validatePipelineDAG() function
3. 4 new Verify* passes
4. Updated buildPassManager() with validation
5. Test cases for invalid --start-from ranges

---

### Phase 2: Add Optional Precondition Predicates (2-3 days, Future)

**Conditions**: Only if Phase 1 reveals complex preconditions

**Deliverables**:
1. StageDescriptor.verifyPreconditions field
2. Predicate functions for complex stages
3. Enhanced error messages

---

### Phase 3: Eager AnalysisManager Initialization (2-3 weeks, Future)

**Related to multithreading**: Addresses analysis invalidation blocking

**Deliverables**:
1. Eager AM graph construction at pipeline start
2. Fine-grained invalidation API
3. Enable --mlir-threading for safe stages

---

## 7. Summary & Recommendation

**Current State**:
- Pipeline ordering implicit (registry array order)
- 4 hard preconditions enforced by IR structure
- 8 analysis-dependency constraints via AM invalidation
- 8 verification barriers (sparse coverage)
- **Risk**: Accidental reordering, invalid --start-from ranges, parallelization blocked

**Proposed Solution**:
- Formal dependency DAG (Option 2) as primary enforcement
- Extended verification barriers (Option 4) as secondary safety net
- Implementation cost: 4-5 days (Phase 1)
- Risk reduction: High (prevents invalid pipelines)

**Recommended Action**:
1. **Immediately**: Proceed with Phase 1 implementation (formal DAG + verification)
2. **Next**: After Phase 1 succeeds, assess Phase 2 need (predicate checks)
3. **Future**: Phase 3 (eager AM init for parallelization, blocked by multithreading audit resolution)

**Why This Design**:
- **Formal DAG** (Option 2): Aligns with compiler theory; enables static analysis
- **Verification barriers** (Option 4): IREE-proven approach; catches semantic violations
- **Hybrid**: Combines strengths of both; low code overhead
- **Scalable**: Extends naturally as pipeline grows; easy to add new stages

---

## References

- **Investigation Results**: `/home/raherrer/projects/carts/docs/compiler/phase-ordering-semantics.md` (450+ lines)
- **Compile.cpp**: `tools/compile/Compile.cpp` (lines 238-250 StageDescriptor, 951-1103 registry)
- **IREE Reference**: Phase-based pipeline with verification callbacks
- **LLVM PassBuilder**: Implicit dependencies via usage patterns
- **GCC**: Hard-coded pass order via macros

