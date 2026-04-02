# CARTS Pipeline Phase Ordering Semantics (G-5 Investigation)

**Status**: Comprehensive investigation completed
**Date**: 2026-04-02
**Investigation lead**: Multi-agent deep analysis

## Executive Summary

CARTS pipeline ordering is enforced through a combination of:
1. **IR structure invariants** (hard preconditions)
2. **Verification barriers** (stage transitions)
3. **AnalysisManager invalidation patterns** (analysis dependencies)
4. **Stage registry order** (implicit, not formally declared)

The pipeline has **4 hard preconditions** that are impossible to reorder and **8 additional analysis-dependency constraints** managed through selective invalidation. However, ordering is **implicit in the StageDescriptor registry** with **no formal dependency declarations**, creating a reordering risk.

## 1. Pipeline Structure (20 Stages)

### 18 Core Stages (in canonical order)

```
1. raise-memref-dimensionality    (normalize memrefs)
2. collect-metadata               (extract loop/array metadata)
3. initial-cleanup                (simplify IR)
4. openmp-to-arts                 (convert OpenMP → EDTs)
5. pattern-pipeline               (semantic pattern discovery)
6. edt-transforms                 (EDT structure optimization)
7. create-dbs                      (allocate DataBlocks) ← DB structure created
8. db-opt                          (tighten DB modes)
9. edt-opt                         (fuse EDTs)
10. concurrency                    (build dependency graph)
11. edt-distribution               (select distribution; lower for-loops)
12. post-distribution-cleanup      (restructure after loop lowering)
13. db-partitioning                (block/stencil partitioning) ← DB graph rebuilt
14. post-db-refinement             (mode refinement; validation)
15. late-concurrency-cleanup       (loop optimization)
16. epochs                         (create synchronization groups)
17. pre-lowering                   (lower ARTS ops to runtime calls) ← All ARTS ops lowered
18. arts-to-llvm                   (convert to LLVM IR)
```

### 2 Epilogue Stages (optional, conditional)

```
19. post-o3-opt                    (optional: classical optimizations when -O3)
20. llvm-ir-emission               (optional: emit LLVM IR when --emit-llvm)
```

## 2. Hard Preconditions (Impossible to Reorder)

These 4 preconditions are enforced by **IR structure itself** — reordering causes silent miscompilation:

### 2.1 openmp-to-arts (stage 4) must run before create-dbs (stage 7)

**Reason**: CreateDbs expects arts.edt operations to exist; it wraps them with DataBlock acquires.

**Precondition**: `arts.edt` operations present
**Postcondition**: `arts.db_acquire` operations created

**What breaks if reordered**:
- If CreateDbs runs before openmp-to-arts: No EDTs to wrap; all data remains in scalars/memrefs
- Result: Silent miscompilation; data aliases undetected; cache coherence failures at runtime

**Implementation**:
- `ConvertOpenMPToArts.cpp`: Creates `arts.edt` for each omp.parallel
- `CreateDbs.cpp`: Iterates EDTs; creates `arts.db_acquire` for each data access
- No explicit check; relies on builder function order

### 2.2 create-dbs (stage 7) must run before db-opt, db-partitioning, etc. (stages 8+)

**Reason**: DB analysis is **built for the first time in stage 7**. All downstream passes assume it exists.

**Precondition**: `arts.db_acquire` operations present
**Postcondition**: DbAnalysis initialized; graph built

**What breaks if reordered**:
- If DbPartitioning (stage 13) runs without DB creation: No DB allocations to partition; crash on graph access
- Result: Immediate failure; AM->getDbAnalysis() returns empty graph

**Implementation**:
- `CreateDbs.cpp`: First pass to create arts.db_acquire
- `AnalysisManager::getDbAnalysis()`: Lazily builds DbGraph on first call
- Subsequent passes read the graph

### 2.3 edt-distribution + for-lowering (stage 11) must run before pre-lowering (stage 17)

**Reason**: ForLowering converts `arts.for` loops to runtime control flow. EdtLowering assumes no `arts.for` remain.

**Precondition**: `arts.for` loops lowered to `arith` + `cf` operations
**Postcondition**: No `arts.for` operations in IR

**What breaks if reordered**:
- If EdtLowering (stage 17) runs before ForLowering: EDTs still contain arts.for; lowering crashes
- Result: Immediate failure; lowering code expects all loops already converted

**Implementation**:
- `ForLowering.cpp`: Transforms arts.for → arith/cf
- `EdtLowering.cpp`: Assumes no arts.for; walks EBB structure
- `VerifyForLowered` (stage 11): Enforces "no arts.for remain" precondition

### 2.4 pre-lowering (stage 17) must run before arts-to-llvm (stage 18)

**Reason**: ConvertArtsToLLVM expects all `arts.edt` and `arts.db_acquire` to be lowered to ops.

**Precondition**: All ARTS ops lowered (no `arts.edt`, `arts.db_acquire`, `arts.epoch` remain)
**Postcondition**: Only LLVM-compatible ops remain

**What breaks if reordered**:
- If ConvertArtsToLLVM runs before EdtLowering: arts.edt ops still present; LLVM lowering crashes
- Result: Immediate failure; no pattern match for arts.edt in LLVM conversion

**Implementation**:
- `EdtLowering.cpp`, `DbLowering.cpp`, `EpochLowering.cpp`: Lower all ARTS ops
- `VerifyPreLowered` (stage 17): Enforces no ARTS ops remain
- `ConvertArtsToLLVM.cpp`: Walks function; lowers remaining ops

## 3. Analysis Dependencies (8 Constraint Patterns)

These are **soft ordering constraints** enforced through AnalysisManager invalidation:

### 3.1 DbModeTightening invalidates all analyses (stages 8, 14)

**Reason**: DB mode changes affect downstream analysis (partitioning heuristics, affinity decisions).

**Invalidation type**: `AM->invalidate()` (full global invalidate)
**Cost**: Expensive; blocks pass parallelization

**Stages using this pattern**:
- Stage 8 (db-opt): DbModeTightening → AM->invalidate()
- Stage 14 (post-db-refinement): DbModeTightening → AM->invalidate()

**Why both?**:
1. Stage 8: Early tightening before major graph-building passes (concurrency, edt-dist)
2. Stage 14: Late tightening after partitioning when new modes discovered (e.g., stencil dimension)

### 3.2 DbPartitioning rebuilds DB graph (stage 13)

**Reason**: Block partitioning creates **new DB allocations** (distinct GUIDs). Old graph is invalid.

**Invalidation type**: `AM->invalidateAndRebuildGraphs()` (rebuild graphs eagerly)
**Cost**: High; required because structure changed

**Why invalidateAndRebuildGraphs()?**:
- DB structure changed (single → multiple blocks)
- EDT graph still valid (EDT operations unchanged)
- But eager rebuild allows downstream passes to assume current graph

### 3.3 Concurrency invalidates DB analysis only (stage 10)

**Reason**: Concurrency builds EDT dependency graph; DB structure **unchanged**.

**Invalidation type**: `AM->getDbAnalysis().invalidate()` (selective invalidation)
**Cost**: Low; only recomputes DB access analysis

**Why selective?**:
- EDT structure changes (deps added)
- DB structure unchanged (acquires, modes, allocations same)
- ForOpt reads edtHeuristics (expects current EDT graph)

**Implementation**:
```cpp
// In Concurrency pass:
AM->getDbAnalysis().invalidate();  // DB graph invalid; will rebuild on next access
```

### 3.4 EdtDistribution invalidates DB analysis (stage 11)

**Reason**: ForLowering transforms `arts.for` to control flow; DB structure **unchanged**.

**Invalidation type**: `AM->getDbAnalysis().invalidate()` (selective)
**Cost**: Low; only recomputes DB access analysis

**Why selective?**:
- Loop structure changes (arts.for → arith/cf)
- DB allocations unchanged
- But DB access analysis must recompute (loop bounds changed)

**Implementation**:
```cpp
// In EdtDistribution pass:
AM->getDbAnalysis().invalidate();  // Will rebuild post-ForLowering
```

### 3.5 DbModeTightening mode changes affect downstream heuristics (stages 8, 14)

**Reason**: PartitioningHeuristics and AffinityHeuristics depend on DB modes.

**Modes changed by DbModeTightening**:
- COARSE → FINE (over-conservative modes tightened based on access patterns)
- Affects: Stencil dimension selection, block-size decisions

**Example dependency**:
```
Stage 8: DbModeTightening → AM->invalidate()
         ↓
Stage 9: EdtOpt (LoopFusion reads LoopAnalysis — safe, unchanged)
         ↓
Stage 10: Concurrency (builds dependency graph)
         ↓
Stage 11: EdtDistribution (reads EdtHeuristics — uses updated modes)
```

### 3.6 DbTransforms affects DB graph structure (stages 13, 14)

**Reason**: DbTransforms implements data flow optimizations (ESD, halo consolidation, GUID range).

**Transformations**:
- DT-1: Contract persistence (metadata not structure)
- DT-2: Halo consolidation (combines related acquires)
- DT-3: ESD (extracts common subexpressions in bounds)
- DT-4: GUID range optimization
- DT-7: Block window extraction

**When does it invalidate?** Only when structure changes (DT-2 halo consolidation):
```cpp
// In DbTransformsPass:
if (consolidatedHalos) {
  AM->invalidateFunction(func);  // Function-level invalidation
}
```

### 3.7 LoopFusion reads LoopAnalysis; loop structure **must be stable** (stage 9)

**Reason**: LoopAnalysis caches loop nest structure; LoopFusion fuses adjacent loops.

**Precondition**: Loop nests finalized (no major restructuring between stages)
**Postcondition**: Some loops fused; others unchanged

**Why safe**?**:
- LoopFusion reads LoopAnalysis (doesn't invalidate)
- Loop structure doesn't change between stages 9 and 15 (LateConcurrencyCleanup)
- Analysis remains valid

### 3.8 EdtStructuralOpt with/without runAnalysis affects analysis state (stages 6, 9, 12)

**Pattern**:
- Stage 6: `EdtStructuralOpt(runAnalysis=false)` — reads cached EDT graph
- Stage 9: `EdtStructuralOpt(runAnalysis=true)` — runs new EDT analysis
- Stage 12: `EdtStructuralOpt(runAnalysis=false)` — reads cached graph

**Why?**:
- Stage 6: EDT graph not yet built (DB creation not started); safe to skip analysis
- Stage 9: EDT graph now valid (after DB creation); analysis needed for fusion
- Stage 12: EDT graph valid; cleanup doesn't require fresh analysis

## 4. Verification Barriers

8 verification passes enforce preconditions at critical IR transitions:

| Stage | Pass | Enforces | Blocks |
|-------|------|----------|--------|
| 2 | VerifyMetadata | Metadata consistency | stage 3+ (diagnose mode) |
| 7 | VerifyDbCreated | All arts.db_acquire created | stage 8+ |
| 11 | VerifyForLowered | No arts.for remain | stage 12+ |
| 17 | VerifyEdtLowered | No arts.edt remain | stage 18 |
| 17 | VerifyEpochLowered | No arts.epoch remain | stage 18 |
| 17 | VerifyPreLowered | No ARTS ops remain | stage 18 |
| 18 | VerifyDbLowered | All DB calls present | (final check) |
| 18 | VerifyLowered | Complete lowering | (final check) |

**Missing verification passes** (identified during investigation):
- VerifyForCreated (stage 5): arts.for present before distribution
- VerifyEdtCreated (stage 4): arts.edt present after openmp conversion
- VerifyDbPartitioned (stage 13): Block partitioning completed
- VerifyEpochCreated (stage 16): arts.epoch present after CreateEpochs

## 5. Implicit vs. Explicit Ordering

### Current Approach (Implicit)

**Storage**: Stage builder functions in StageDescriptor array (line 952-1102 in Compile.cpp)

```cpp
static const std::array<StageDescriptor, 20> kStageRegistry = {{
  {StageId::RaiseMemRefDimensionality, "raise-memref-dimensionality", ...},
  {StageId::CollectMetadata, "collect-metadata", ...},
  // ... 18 more stages in order
}};
```

**Ordering enforced by**: Array iteration order in buildPassManager() (line 1244)

```cpp
for (const auto &stage : getStageRegistry()) {
  if (stage.kind != StageKind::Core) continue;
  int currentIndex = stageIndex(stage.id, StageKind::Core);
  if (currentIndex < startIndex) continue;
  if (!stage.enabled(...)) continue;
  if (failed(runStage(stage, stopHere))) return failure();
  if (stopHere) { ... }
}
```

**Risk**: Accidental reordering in registry array silently breaks compilation

### Explicit Constraints (Verification + AM invalidation)

**Storage**: Hardcoded checks in individual passes

```cpp
// In DbPartitioning.cpp:
AM->invalidateAndRebuildGraphs(module);  // Enforces: "DBs are new allocations"

// In Concurrency.cpp:
AM->getDbAnalysis().invalidate();  // Enforces: "DB structure unchanged"
```

**Advantage**: Selective verification catches some violations
**Disadvantage**: No formal DAG; constraints scattered across codebase

## 6. Stage Registry Semantics

### allowStartFrom / allowPipelineStop Fields

StageDescriptor controls which stages can be entry/exit points:

```cpp
struct StageDescriptor {
  bool allowPipelineStop;   // Can use --pipeline=<stage>?
  bool allowStartFrom;      // Can use --start-from=<stage>?
  ...
};
```

**All 18 core stages**: `allowPipelineStop=true, allowStartFrom=true`

**Epilogue stages** (post-o3-opt, llvm-ir-emission): `allowPipelineStop=false, allowStartFrom=false`

**Validation in buildPassManager()**:

```cpp
int startIndex = stageIndex(startFrom, StageKind::Core);
int stopIndex = stopAt ? stageIndex(*stopAt, StageKind::Core)
                       : stageIndex(StageId::ArtsToLLVM, StageKind::Core);
if (startIndex > stopIndex) {
  return failure();  // Reject invalid ranges
}
```

### --start-from Entry Point Validation

**Current validation**: Checks that startIndex ≤ stopIndex
**Missing validation**: Checks that preconditions are met for chosen startIndex

**Example vulnerability**:
```bash
carts compile file.mlir --start-from=db-partitioning
```

This is **allowed by allowStartFrom=true**, but **preconditions are not met**:
- DB structure must exist (created in stage 7)
- EDT concurrency graph must exist (created in stage 10)
- For-loops must be lowered (lowered in stage 11)

**Risk**: Stage 13 runs on uninitialized AM; crashes or silently produces wrong result

**Fix needed**: Add precondition validation for each allowStartFrom stage

## 7. AnalysisManager Interface

### Analyses Available

5 core analyses managed by AnalysisManager:

1. **DbAnalysis**: Graph of allocations and acquires
   - Methods: `getOrCreateGraph(func)`, `invalidate()`
   - Used by: CreateDbs, DbPartitioning, DbModeTightening, DbTransforms, etc.

2. **EdtAnalysis**: Graph of tasks and dependencies
   - Methods: `getOrCreateEdtGraph(func)`, `getEdtNode(op)`, `invalidate()`
   - Used by: EdtStructuralOpt, EdtTransforms, Concurrency, etc.

3. **LoopAnalysis**: Loop nest structure and bounds
   - Methods: Used by LoopFusion, DbPartitioning, etc.
   - Invalidation: Implicit (no explicit invalidate() calls)

4. **DbHeuristics**: Decision tracking for partitioning
   - Methods: `recordDecision()`, used by DbPartitioning
   - Invalidation: Implicit (no explicit invalidate())

5. **EdtHeuristics**: Decision tracking for distribution
   - Methods: `resolveLoweringStrategy()`, used by ForLowering
   - Invalidation: Implicit (no explicit invalidate())

### Invalidation Methods

1. **`AM->invalidate()`** (expensive, blocking)
   - Invalidates: All analyses (DbAnalysis, EdtAnalysis, LoopAnalysis, heuristics)
   - Used by: DbModeTightening (stages 8, 14)
   - Cost: Blocks parallelization; requires sequential pass execution

2. **`AM->invalidateAndRebuildGraphs()`** (very expensive, eager rebuild)
   - Invalidates: All graphs; rebuilds immediately
   - Used by: DbPartitioning (stage 13)
   - Cost: Highest; eager rebuild blocks parallelization
   - Benefit: Downstream passes assume current graphs

3. **`AM->invalidateFunction(func)`** (medium cost, function-level)
   - Invalidates: Analyses for one function (parallel-safe)
   - Used by: EdtLowering (stage 17), DbTransforms (stage 14 if consolidating halos)
   - Benefit: Finer granularity; allows function-level parallelism

4. **`getXxxAnalysis().invalidate()`** (low cost, selective)
   - Invalidates: One analysis class only
   - Used by: Concurrency (DB analysis), EdtDistribution (DB analysis)
   - Benefit: Low cost; other analyses remain valid

## 8. Comparison with Production Compilers

### LLVM PassBuilder (since LLVM 10)

**Approach**:
- Passes declare **required passes** and **preservation constraints**
- PassBuilder automatically inserts required passes before dependent stages
- No explicit ordering DAG; implicit via dependencies

**Example**:
```cpp
// SimplifyCFG requires LowerExpectIntrinsics to run first
class SimplifyCFGPass : public PassInfoMixin<SimplifyCFGPass> {
  PreservedAnalyses run(...) {
    // Run LowerExpectIntrinsics internally if needed
  }
};
```

**Advantage**: Flexible; passes are composable
**Disadvantage**: Hard to reason about total pipeline order

### IREE Phase-Based Pipeline

**Approach**:
- Define phases with explicit preconditions/postconditions
- Register verification passes at phase boundaries
- PipelineHooks for before/after phase logging

**Example**:
```cpp
struct Phase {
  std::string name;
  std::vector<Pass> passes;
  std::vector<Verification> preconditions;
  std::vector<Verification> postconditions;
};

phases.push_back({
  "lowering-phase",
  {createXxxPass(), createYyyPass()},
  {verifyHighLevelOps()},
  {verifyNoArtsOps()}
});
```

**Advantage**: Clear invariants; explicit verification
**Disadvantage**: Verbose; requires maintenance

### CARTS Current Approach

**Hybrid**: Implicit order + selective verification

**Strengths**:
- Concise; pipeline fits in one file
- Clear stage separation via builders

**Weaknesses**:
- No formal dependency DAG
- Verification coverage sparse
- Reordering risk high
- Hard to parallelize (invalidation blocks)

## 9. Enforcement Gaps & Risks

### Gap 1: No Formal Dependency DAG

**Current**: Stage order is implicit in registry array order

**Risk**: Accidental reordering silently breaks compilation

**Example**:
```cpp
// Current (correct):
{StageId::CreateDbs, "create-dbs", ...},
{StageId::DbOpt, "db-opt", ...},

// Accidental swap (wrong):
{StageId::DbOpt, "db-opt", ...},
{StageId::CreateDbs, "create-dbs", ...},
// → DbOpt runs before CreateDbs; silent miscompilation
```

**Recommendation**: Add `dependsOn: ArrayRef<StageId>` field to StageDescriptor

```cpp
struct StageDescriptor {
  StageId id;
  std::vector<StageId> dependsOn;  // New field
  ...
};
```

### Gap 2: --start-from Precondition Validation Missing

**Current**: Only checks allowStartFrom field

**Risk**: Users can start from stages that require prior initialization

**Example**:
```bash
carts compile file.mlir --start-from=db-partitioning
```

**Preconditions not checked**:
- DB structure created (stage 7)
- EDT concurrency graph built (stage 10)
- For-loops lowered (stage 11)

**Recommendation**: Add precondition validation

```cpp
static bool validateStartFromPreconditions(StageId startFrom, ModuleOp module) {
  switch (startFrom) {
    case StageId::DbPartitioning:
      if (!hasArtsDbAcquire(module)) return false;  // DB not created
      if (!hasEdtDependencies(module)) return false;  // Deps not built
      return true;
    // ... check other stages
  }
  return true;
}
```

### Gap 3: Sparse Verification Coverage

**Current**: 8 verification passes

**Missing**:
- VerifyForCreated (stage 5): arts.for present before distribution
- VerifyEdtCreated (stage 4): arts.edt present after openmp conversion
- VerifyDbPartitioned (stage 13): Block partitioning completed
- VerifyEpochCreated (stage 16): arts.epoch present after CreateEpochs

**Recommendation**: Implement 4 additional verification passes (IREE approach)

### Gap 4: Analysis Invalidation Blocks Parallelization

**Current**: `AM->invalidate()` is global and blocking

**Risk**: Prevents pass parallelization (identified in multithreading audit)

**Recommendation** (future, Phase 2+ priority):
1. Eager AM initialization at pipeline start
2. Pre-build all graphs before stage 1 runs
3. Replace global invalidate() with fine-grained invalidation
4. Enable --mlir-threading for safe stages

## 10. Summary Table: Ordering Maturity

| Aspect | Current Maturity | Coverage | Risk Level |
|--------|------------------|----------|-----------|
| Hard preconditions | High | 4/4 enforced by IR structure | Low (enforced by nature) |
| Verification barriers | Medium | 8/12 critical transitions | Medium (sparse) |
| Analysis dependencies | Medium-High | 8 patterns via invalidation | Medium (implicit) |
| Stage registry order | **Low** | Implicit only | **High** (reordering risk) |
| --start-from validation | Low | Syntax check only | Medium (precondition risk) |
| --pipeline stop points | Medium | All stages allowed; sensible default | Low |
| Documentation | Medium | pipeline.md + comments | Medium (could be better) |

## 11. Recommendations for Enforcement Maturity

### Phase 1 (High Impact, Low Effort)

1. **Add formal dependency DAG to StageDescriptor**
   - Add `dependsOn: ArrayRef<StageId>` field
   - Populate for all stages
   - Validate at pipeline construction time
   - Effort: 1-2 days

2. **Implement --start-from precondition validation**
   - Check required analyses/structures exist
   - Emit clear error messages
   - Effort: 2-3 days

3. **Document implicit ordering constraints**
   - Create `docs/compiler/phase-ordering-semantics.md` (this document)
   - Add precondition/postcondition table for each stage
   - Effort: 1 day (already done as part of investigation)

### Phase 2 (Medium Impact, Medium Effort)

4. **Implement 4 missing verification passes**
   - VerifyForCreated, VerifyEdtCreated, VerifyDbPartitioned, VerifyEpochCreated
   - Effort: 3-4 days

5. **Add --mlir-timing to Compile.cpp**
   - Instrument all 18 stages
   - Enable detailed performance analysis
   - Effort: 1-2 days

### Phase 3 (High Impact, High Effort)

6. **Eager AnalysisManager initialization**
   - Pre-build all graphs at pipeline start
   - Replace global invalidate() with fine-grained invalidation
   - Effort: 2-3 weeks
   - Benefit: Enables pass parallelization

## 12. Investigation Artifacts

**Produced during this investigation**:
- tools/compile/Compile.cpp: Full analysis of 20 stages, 18 builders, invalidation patterns
- include/arts/analysis/AnalysisManager.h: Interface documentation
- lib/arts/passes/*/: 9 passes analyzed for invalidation usage
- docs/compiler/pipeline.md: Existing documentation (baseline)
- This document: Comprehensive phase ordering guide

**Commands for verification**:
```bash
# List all stages in pipeline order
carts pipeline

# Check pass order for specific stage
carts pipeline --pipeline=db-partitioning

# Print full manifest (source of truth)
carts pipeline --json
```

## References

- **Compile.cpp** (tools/compile/Compile.cpp): Stage definitions, builders, execution loop
- **pipeline.md** (docs/compiler/pipeline.md): Existing pipeline reference
- **AGENTS.md** (AGENTS.md): Agent-accessible pipeline guide
- **AnalysisManager.h** (include/arts/analysis/AnalysisManager.h): Analysis interface
- **MLIR PassBuilder** (upstream LLVM): Comparison reference
- **IREE Phase-Based Pipeline**: Comparison reference (not in repo)

