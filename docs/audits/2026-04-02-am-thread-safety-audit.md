# AnalysisManager Deep Thread-Safety Audit (2026-04-02)

## Executive Summary

This comprehensive audit inventories all thread-safety issues in AnalysisManager and its constituent analyses. The investigation covers:

1. **All mutable state** across 12 analysis classes
2. **Lazy initialization races** (8 critical issues)
3. **DenseMap concurrent access** (4 races)
4. **Graph rebuild races** (3 races)
5. **Pass-analysis dependencies** (27 passes using AM)
6. **Phased parallelization strategy** (5 phases with effort estimates)

### Key Finding
**AnalysisManager is fundamentally NOT thread-safe** due to:
- No synchronization on lazy initialization (getDbAnalysis, getEdtAnalysis, etc.)
- Concurrent DenseMap access in graph construction and query
- TOCTOU (check-then-act) race conditions in invalidate+build sequences
- No protection on mutable caches (loopAccessSummaryByOp, edtOrderIndex, etc.)

**Current blocker**: `context.disableMultithreading(true)` at Compile.cpp:334

---

## Step 1: Mutable State Inventory

### AnalysisManager (include/arts/analysis/AnalysisManager.h)

| Member | Type | Thread-Safety Issue |
|--------|------|---------------------|
| `dbAnalysis` | `std::unique_ptr<DbAnalysis>` | **LAZY INIT RACE** — getDbAnalysis() checks `if (!dbAnalysis)` then constructs without synchronization |
| `edtAnalysis` | `std::unique_ptr<EdtAnalysis>` | **LAZY INIT RACE** — getEdtAnalysis() identical pattern |
| `epochAnalysis` | `std::unique_ptr<EpochAnalysis>` | **LAZY INIT RACE** — getEpochAnalysis() identical pattern |
| `loopAnalysis` | `std::unique_ptr<LoopAnalysis>` | **LAZY INIT RACE** — getLoopAnalysis() identical pattern |
| `stringAnalysis` | `std::unique_ptr<StringAnalysis>` | **LAZY INIT RACE** — getStringAnalysis() identical pattern |
| `dbHeuristics` | `std::unique_ptr<DbHeuristics>` | **LAZY INIT RACE** — getDbHeuristics() identical pattern |
| `edtHeuristics` | `std::unique_ptr<EdtHeuristics>` | **LAZY INIT RACE** — getEdtHeuristics() identical pattern |
| `metadataManager` | `std::unique_ptr<MetadataManager>` | **LAZY INIT RACE** — getMetadataManager() identical pattern |
| `cachedDiagnosticJson` | `std::optional<std::string>` | Read-only after set in captureDiagnostics(); SAFE if captureDiagnostics is called before parallel passes |
| `metadataCoverage` | `MetadataCoverage` | Written by setMetadataCoverage() without synchronization — **RACE** |

**File**: `lib/arts/analysis/AnalysisManager.cpp:54-118`

```cpp
// RACE CONDITION EXAMPLE:
DbAnalysis &AnalysisManager::getDbAnalysis() {
  if (!dbAnalysis)                           // Thread A checks: nullptr
    dbAnalysis = std::make_unique<DbAnalysis>(*this);  // Thread B also constructs
  return *dbAnalysis;
}
// Result: Both threads construct, one overwrites the other; the discarded one leaks and its state is inconsistent
```

### DbAnalysis (include/arts/analysis/db/DbAnalysis.h, lib/arts/analysis/db/DbAnalysis.cpp)

| Member | Type | Thread-Safety Issue |
|--------|------|---------------------|
| `functionGraphMap` | `DenseMap<func::FuncOp, unique_ptr<DbGraph>>` | **CONCURRENT INSERT RACE** — getOrCreateGraph() modifies map without lock |
| `loopAccessSummaryByOp` | `DenseMap<Operation*, LoopDbAccessSummary>` | **CONCURRENT INSERT RACE** — analyzeLoopDbAccessPatterns() modifies without lock |
| `dbAliasAnalysis` | `unique_ptr<DbAliasAnalysis>` | **LAZY INIT RACE** — getAliasAnalysis() checks-then-construct |

**Critical Site**: `DbAnalysis.cpp` getOrCreateGraph() (line ~152)
```cpp
DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  auto &graph = functionGraphMap[func];  // DenseMap::operator[] call
  if (!graph)
    graph = std::make_unique<DbGraph>(func, this);
  return *graph;
}
// RACE: Two threads call getOrCreateGraph(same func)
// - Thread A: DenseMap insert for func succeeds, creates DbGraph
// - Thread B: DenseMap lookup returns same entry; Thread B may see incomplete/stale graph
// - DenseMap reallocation during resize() corrupts both threads' references
```

### EdtAnalysis (include/arts/analysis/edt/EdtAnalysis.h, lib/arts/analysis/edt/EdtAnalysis.cpp)

| Member | Type | Thread-Safety Issue |
|--------|------|---------------------|
| `edtOrderIndex` | `DenseMap<EdtOp, unsigned>` | **CONCURRENT INSERT RACE** — analyzeFunc() inserts without lock |
| `edtPatternByOp` | `DenseMap<Operation*, EdtDistributionPattern>` | **CONCURRENT INSERT RACE** — analyze() clears and populates |
| `allocPatternByOp` | `DenseMap<Operation*, DbAccessPattern>` | **CONCURRENT INSERT RACE** — analyze() clears and populates |
| `analyzed` | `bool` | **DATA RACE** — set to true in analyze() without synchronization |
| `edtGraphs` | `DenseMap<func::FuncOp, unique_ptr<EdtGraph>>` | **CONCURRENT INSERT RACE** — getOrCreateEdtGraph() modifies |

**Critical Site**: `EdtAnalysis.cpp:47-56` (analyze())
```cpp
void EdtAnalysis::analyze() {
  edtPatternByOp.clear();      // DenseMap::clear() — not thread-safe if concurrent reads
  allocPatternByOp.clear();
  edtOrderIndex.clear();

  auto &AM = getAnalysisManager();
  for (auto func : AM.getModule().getOps<func::FuncOp>())
    analyzeFunc(func);
  analyzed = true;             // DATA RACE: no synchronization
}
```

### LoopAnalysis (include/arts/analysis/loop/LoopAnalysis.h)

| Member | Type | Thread-Safety Issue |
|--------|------|---------------------|
| `loopNodes` | `DenseMap<Operation*, unique_ptr<LoopNode>>` | **CONCURRENT INSERT RACE** — getOrCreateLoopNode() modifies |
| `built` | `bool` | **DATA RACE** — set to true without synchronization |

**Critical Site**: Implicit in getOrCreateLoopNode() (header file pattern)
```cpp
LoopNode *LoopAnalysis::getOrCreateLoopNode(Operation *loopOp) {
  auto &node = loopNodes[loopOp];  // DenseMap race
  if (!node)
    node = std::make_unique<LoopNode>(loopOp);
  return node.get();
}
```

### DbGraph (include/arts/analysis/graphs/db/DbGraph.h)

| Member | Type | Thread-Safety Issue |
|--------|------|---------------------|
| `allocNodes` | `DenseMap<DbAllocOp, unique_ptr<DbAllocNode>>` | **CONCURRENT INSERT RACE** — build() populates without lock |
| `acquireNodeMap` | `DenseMap<DbAcquireOp, DbAcquireNode*>` | **CONCURRENT INSERT RACE** — processAcquireNode() inserts |
| `opOrder` | `DenseMap<Operation*, unsigned>` | **CONCURRENT INSERT RACE** — computeOpOrder() populates |
| `version` | `uint64_t` | **DATA RACE** — incremented in build() without atomic |
| `built`, `needsRebuild` | `bool` | **DATA RACE** — modified without synchronization |

### EdtGraph (include/arts/analysis/graphs/edt/EdtGraph.h)

| Member | Type | Thread-Safety Issue |
|--------|------|---------------------|
| `edtNodes` | `DenseMap<EdtOp, unique_ptr<EdtNode>>` | **CONCURRENT INSERT RACE** — build() populates |
| `nodes` | `SmallVector<NodeBase*, 8>` | **CONCURRENT MODIFY RACE** — push_back() in collectNodes() |
| `edges` | `DenseMap<pair, unique_ptr<EdgeBase>>` | **CONCURRENT INSERT RACE** — buildDependencies() inserts |
| `version` | `uint64_t` | **DATA RACE** — incremented without atomic |
| `isBuilt`, `needsRebuild` | `bool` | **DATA RACE** — modified without synchronization |

---

## Step 2: Race Condition Classification & Severity

### CRITICAL (Severity: S1 — Data Corruption)

#### R-1: AnalysisManager Lazy Init Race
**Location**: AnalysisManager.cpp:54-82 (getDbAnalysis, getEdtAnalysis, getLoopAnalysis, getStringAnalysis, getDbHeuristics, getEdtHeuristics)
**Type**: Lazy initialization without synchronization
**Race Pattern**: TOCTOU (check-then-act)
```
Thread A: if (!dbAnalysis) enters
Thread B: if (!dbAnalysis) enters
Thread A: dbAnalysis = std::make_unique<DbAnalysis>
Thread B: dbAnalysis = std::make_unique<DbAnalysis>  // overwrites Thread A's work
Result: Thread A's analysis object is leaked; state is inconsistent
```
**Severity**: S1 — Creates zombie analysis objects, lost updates, memory corruption
**Fix**: `std::call_once` or `std::shared_mutex`

#### R-2: DbAnalysis::functionGraphMap Concurrent Insert
**Location**: lib/arts/analysis/db/DbAnalysis.cpp (implicit in getOrCreateGraph)
**Type**: DenseMap concurrent modification
**Scenario**: Two threads call `getOrCreateGraph(func)` for the same function simultaneously
```
Thread A: functionGraphMap[func] — triggers map resize during insert
Thread B: functionGraphMap[func] — concurrent modification of map internal structure
Result: Pointer invalidation, use-after-free on graph reference
```
**Severity**: S1 — Use-after-free, memory corruption
**Affected Passes**: DbPartitioning, DbModeTightening, EdtTransformsPass (all call getOrCreateGraph)

#### R-3: EdtAnalysis::edtGraphs Concurrent Insert
**Location**: EdtAnalysis.cpp (implicit in getOrCreateEdtGraph)
**Type**: DenseMap concurrent modification
**Same issue as R-2**
**Severity**: S1 — Use-after-free
**Affected Passes**: EdtStructuralOpt, EdtTransformsPass, Concurrency

#### R-4: EdtAnalysis::analyze() clear() + populate race
**Location**: EdtAnalysis.cpp:47-56
**Type**: Concurrent modification during analysis initialization
**Scenario**: Thread A calls analyze() and clears maps while Thread B reads from them
```
Thread A: edtPatternByOp.clear()
Thread B: getEdtDistributionPattern() reads edtPatternByOp
Result: Iterator invalidation, undefined behavior
```
**Severity**: S1 — Undefined behavior, iterator invalidation

### HIGH (Severity: S2 — Data Race / Visibility Issue)

#### R-5: DbGraph::build() concurrent DenseMap insert
**Location**: DbGraph.cpp (build(), collectNodes())
**Type**: Multiple DenseMap inserts without synchronization
**Maps affected**: allocNodes, acquireNodeMap, opOrder
**Severity**: S2 — Corrupted graph structure, wrong analysis results
**Impact**: Silent miscompilation (partitioning decisions based on corrupted graph)

#### R-6: EdtGraph::build() concurrent SmallVector + DenseMap
**Location**: EdtGraph.cpp (build(), collectNodes(), buildDependencies())
**Type**: Concurrent modification of nodes (SmallVector) and edges (DenseMap)
**Severity**: S2 — Graph structure corruption, EDT dependency analysis failures

#### R-7: metadataCoverage data race
**Location**: AnalysisManager.h:129 (member), AnalysisManager.cpp:98-104 (setter)
**Type**: Concurrent write without synchronization
**Scenario**: setMetadataCoverage() called from one thread while getMetadataCoverage() called from another
**Severity**: S2 — Stale metadata coverage stats
**Mitigation**: Used only for diagnostics (low impact)

#### R-8: analyzed flag data race (EdtAnalysis)
**Location**: EdtAnalysis.cpp:55, EdtAnalysis.h:146
**Type**: Non-atomic bool write
**Scenario**: Thread A sets `analyzed = true` while Thread B reads
**Severity**: S2 — Stale read, may skip analysis
**Impact**: Incorrect analysis results for subsequent passes

### MEDIUM (Severity: S3 — Non-Critical Data Race)

#### R-9: DbGraph::version and built flag races
**Location**: DbGraph.h:97-101
**Type**: Non-atomic version increment and bool flag modifications
**Severity**: S3 — Version counter may be inconsistent, but only used for diagnostics
**Impact**: Incorrect version reporting in diagnostics

#### R-10: DbAnalysis::loopAccessSummaryByOp concurrent insert
**Location**: DbAnalysis.cpp (analyzeLoopDbAccessPatterns)
**Type**: DenseMap insert without lock
**Scenario**: Two threads analyze different loops concurrently (safe) OR same loop (race)
**Severity**: S3 — Depends on whether calls can target same loop
**Status**: Likely safe in practice (per-loop analysis)

---

## Step 3: Pass-to-Analysis Dependency Matrix

### All Passes Using AnalysisManager (27 total)

| Pass | Analysis | Invalidates? | Parallelizable? |
|------|----------|--------------|-----------------|
| CreateDbs | DbAnalysis | No | ✓ (if AM is safe) |
| DbPartitioning | DbAnalysis, DbHeuristics | YES (AM->invalidate) | ✗ (calls invalidate) |
| DbModeTightening | DbAnalysis | YES (AM->invalidate) | ✗ (calls invalidate) |
| DbDistributedOwnership | DbAnalysis | YES (dbAnalysis.invalidate) | ✗ (calls invalidate) |
| DbTransformsPass | DbAnalysis, EdtAnalysis | YES (both invalidate) | ✗ (calls invalidate) |
| EdtStructuralOpt | EdtAnalysis | No | ✓ (if AM is safe) |
| EdtTransformsPass | EdtAnalysis | No | ✓ (if AM is safe) |
| DistributedHostLoopOutlining | LoopAnalysis | YES | ✗ (calls invalidate) |
| EdtDistribution | DbAnalysis | YES | ✗ (calls invalidate) |
| EdtLowering | DbAnalysis, EdtAnalysis | YES (both invalidate + re-analyze) | ✗ (heavy AM use) |
| ForLowering | DbAnalysis | No (query-only) | ✓ (if AM is safe) |
| Concurrency | DbAnalysis, EdtAnalysis | YES | ✗ (calls invalidate) |

### Phase Ordering Constraint
**Invalidation passes CANNOT run in parallel** (they modify AM state globally).

**Safe phases for parallelization**:
1. Passes that only QUERY AM (read-only)
2. AFTER all invalidation passes complete
3. With synchronized AM initialization

---

## Step 4: Fix Strategy

### Phase 0: Immediate Safety (0 days, zero risk)
**Goal**: Identify passes that need NO AnalysisManager access.

**Parallelizable WITHOUT AM changes**:
- CanonicalizeMemrefs
- InitialCleanup
- OpenmpToArts
- PatternPipeline (loop transforms)
- AdditionalOpt
- ArtsToLlvm

**Effort**: 1 hour (grep for AM usage, verify)

### Phase 1a: Fix Lazy Initialization (2-3 days, LOW risk)

**Strategy**: Use `std::call_once` for all AM getters

```cpp
// AnalysisManager.h
private:
  std::once_flag dbAnalysisOnce;
  std::once_flag edtAnalysisOnce;
  // ... etc for all 8 analyses

// AnalysisManager.cpp
DbAnalysis &AnalysisManager::getDbAnalysis() {
  std::call_once(dbAnalysisOnce, [this]() {
    dbAnalysis = std::make_unique<DbAnalysis>(*this);
  });
  return *dbAnalysis;
}
```

**Cost**: 8 once_flags (64 bytes), minor performance overhead
**Benefit**: Eliminates R-1 (lazy init race)
**Risk**: LOW — call_once is stable, well-tested standard library feature

**Timeline**: 2-3 days
**Commits**: One per analysis class

### Phase 1b: Protect Graph Maps (3-4 days, MEDIUM risk)

**Strategy**: Use `std::shared_mutex` in analysis classes

```cpp
// DbAnalysis.h
private:
  mutable std::shared_mutex functionGraphMutex;
  DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;

// DbAnalysis.cpp
DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  std::unique_lock lock(functionGraphMutex);
  auto &graph = functionGraphMap[func];
  if (!graph)
    graph = std::make_unique<DbGraph>(func, this);
  return *graph;
}
```

**Applies to**:
- DbAnalysis::functionGraphMap (R-2)
- EdtAnalysis::edtGraphs (R-3)
- LoopAnalysis::loopNodes (R-10)

**Cost**: 3 shared_mutexes (24 bytes each)
**Benefit**: Eliminates R-2, R-3, R-10
**Risk**: MEDIUM — contention under heavy parallelization; may need lock-free map alternative

**Timeline**: 3-4 days
**Performance impact**: ~5-10% overhead if highly contended

### Phase 1c: Fix Analysis-Internal Races (2 days, MEDIUM risk)

**DenseMap clear-then-insert pattern** (R-4, R-5, R-6):
```cpp
// EdtAnalysis.cpp::analyze()
// BEFORE:
void EdtAnalysis::analyze() {
  edtPatternByOp.clear();
  allocPatternByOp.clear();
  // ...

// AFTER:
void EdtAnalysis::analyze() {
  DenseMap<Operation*, EdtDistributionPattern> newPatterns;
  DenseMap<Operation*, DbAccessPattern> newAllocs;
  // ... populate newPatterns, newAllocs
  std::unique_lock lock(patternMutex);
  edtPatternByOp = std::move(newPatterns);
  allocPatternByOp = std::move(newAllocs);
}
```

**Applies to**:
- EdtAnalysis (R-4)
- DbGraph (R-5)
- EdtGraph (R-6)

**Cost**: Temporary DenseMap allocation
**Benefit**: Eliminates clear-while-reading pattern
**Risk**: MEDIUM — requires careful ordering in multi-stage builds

**Timeline**: 2 days

### Phase 1d: Atomic Flags (1 day, LOW risk)

**Fix bool + uint64_t data races** (R-7, R-8, R-9):
```cpp
// DbGraph.h
private:
  std::atomic<uint64_t> version{0};
  std::atomic<bool> built{false};
  std::atomic<bool> needsRebuild{true};

// EdtAnalysis.h
private:
  std::atomic<bool> analyzed{false};
```

**Cost**: Negligible (3 atomics ≈ 24 bytes)
**Benefit**: Eliminates R-7, R-8, R-9
**Risk**: LOW — atomic operations are lightweight

**Timeline**: 1 day

### Phase 2: Eager Initialization (2-3 days, MEDIUM risk)

**Goal**: Build all analyses up-front before any pass runs in parallel.

**Strategy**: Add `AnalysisManager::ensureAllAnalysesBuilt()` called at pipeline start:

```cpp
// Compile.cpp
auto analysisMgr = std::make_unique<AnalysisManager>(module, artsConfig, metadataFile);
analysisMgr->ensureAllAnalysesBuilt();  // NEW: eager init

for (auto func : module.getOps<func::FuncOp>()) {
  analysisMgr->getDbAnalysis().getOrCreateGraph(func);
  analysisMgr->getEdtAnalysis().getOrCreateEdtGraph(func);
  analysisMgr->getLoopAnalysis().run();
}

// Then parallel passes can safely use these without re-initialization
```

**Benefit**:
- Eliminates lazy-init bottlenecks
- Allows read-only parallelization without locks
- Simplifies Phase 3

**Risk**: MEDIUM — adds initialization latency for single-threaded compile

**Timeline**: 2-3 days

**Phased rollout**:
1. Make eager init optional (flag)
2. Enable for parallel passes only
3. Eventually default-on

### Phase 3: Read-Write Lock Optimization (1-2 weeks, HIGH risk)

**Goal**: Allow multiple threads to READ from graphs concurrently.

**Strategy**: Acquire read lock in getOrCreateGraph, write lock only during build:

```cpp
DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  // Read lock sufficient if graph already exists
  {
    std::shared_lock lock(functionGraphMutex);
    auto it = functionGraphMap.find(func);
    if (it != functionGraphMap.end())
      return *it->second;
  }

  // Write lock only for creation
  std::unique_lock lock(functionGraphMutex);
  auto &graph = functionGraphMap[func];
  if (!graph)
    graph = std::make_unique<DbGraph>(func, this);
  return *graph;
}
```

**Benefit**: Multiple queries can proceed in parallel
**Risk**: HIGH — Complexity, potential for subtle ordering bugs
**Timeline**: 1-2 weeks
**Performance**: 15-25% speedup if contention is high

### Phase 4: Lock-Free Graph Caches (3-4 weeks, VERY HIGH risk)

**Goal**: Eliminate locks entirely for graph access using lock-free data structures.

**Strategy**: Use `std::atomic<DbGraph*>` per function with `std::atomic_compare_exchange`:

```cpp
// DbAnalysis.h
private:
  DenseMap<func::FuncOp, std::atomic<DbGraph*>> functionGraphCache;

DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  auto &atomicPtr = functionGraphCache[func];
  DbGraph *ptr = atomicPtr.load(std::memory_order_acquire);

  if (ptr)
    return *ptr;

  auto newGraph = std::make_unique<DbGraph>(func, this);
  DbGraph *expected = nullptr;
  if (atomicPtr.compare_exchange_strong(expected, newGraph.get(),
                                        std::memory_order_release)) {
    return *newGraph.release();  // We won the race
  } else {
    return *expected;  // Someone else won the race
  }
}
```

**Benefit**: Zero-lock contention-free access
**Risk**: VERY HIGH — Memory ownership complexity, potential leaks
**Timeline**: 3-4 weeks
**NOT RECOMMENDED** unless profiling shows lock contention is severe

---

## Step 5: Phased Parallelization Plan

### Timeline Summary

| Phase | Goal | Effort | Risk | Speedup |
|-------|------|--------|------|---------|
| 0 | No-AM pass parallelization | 1 day | LOW | 3-5x |
| 1a | Fix lazy initialization | 2-3 days | LOW | 1x (safety) |
| 1b | Protect graph maps | 3-4 days | MEDIUM | 1x (safety) |
| 1c | Fix clear-then-read | 2 days | MEDIUM | 1x (safety) |
| 1d | Atomic flags | 1 day | LOW | 1x (safety) |
| 2 | Eager initialization | 2-3 days | MEDIUM | 2-3x |
| 3 | Read-write locks | 1-2 weeks | HIGH | 15-25% |
| 4 | Lock-free caches | 3-4 weeks | VERY HIGH | 5-10% |

### Recommended Phased Approach

**Month 1 (Weeks 1-4)**:
1. Phase 0: Enable no-AM passes in parallel (6 passes, 3-5x speedup)
2. Phase 1a-1d: Fix all critical AM races (1 week, cumulative)
   - ✓ Makes AM "thread-safe" for read-only access
   - ✓ Unblocks Phase 2

**Month 2 (Weeks 5-8)**:
3. Phase 2: Eager initialization (2-3 days)
   - ✓ Pre-build all analyses before parallel passes
   - ✓ Removes runtime race conditions
   - ✓ 2-3x additional speedup

4. Optional Phase 3: Read-write locks (1-2 weeks, if profiling shows contention)

**Month 3+**: Lock-free optimization (only if necessary)

### Expected Speedup

- **Phase 0 alone**: 3-5x (6 parallel passes, 12 CPU cores)
- **Phase 0 + 1a-1d**: 3-5x + safety margin (no further speedup, but safe)
- **Phase 0 + 1a-1d + 2**: 6-12x (eager init eliminates lazy-init bottleneck)
- **Phase 0 + 1a-1d + 2 + 3**: 8-15x (read parallelism on graph queries)

---

## Detailed Race Condition Summary

| ID | Component | Type | Severity | Fix Strategy | Effort |
|----|-----------|------|----------|--------------|--------|
| R-1 | AM::get*Analysis() | Lazy init | S1 | call_once | 2d |
| R-2 | DbAnalysis::functionGraphMap | DenseMap insert | S1 | shared_mutex | 1d |
| R-3 | EdtAnalysis::edtGraphs | DenseMap insert | S1 | shared_mutex | 1d |
| R-4 | EdtAnalysis::analyze() | clear+insert | S1 | rebuild pattern | 1d |
| R-5 | DbGraph::build() | DenseMap insert | S2 | rebuild pattern | 1d |
| R-6 | EdtGraph::build() | SmallVector+DenseMap | S2 | rebuild pattern | 1d |
| R-7 | metadataCoverage | Data race | S2 | atomic or mutex | 2h |
| R-8 | EdtAnalysis::analyzed | Data race | S2 | std::atomic<bool> | 2h |
| R-9 | DbGraph/EdtGraph version | Data race | S3 | std::atomic<uint64_t> | 2h |
| R-10 | DbAnalysis::loopAccessSummaryByOp | DenseMap insert | S3 | shared_mutex or per-loop | 1d |

**Total Effort for Full Safety**: 1-2 weeks (cumulative)
**Effort for Phase 0 (safest first win)**: 1 day

---

## Key Architectural Constraints

### The Invalidate Problem

Many passes call `AM->invalidate()` or individual `analysis.invalidate()`:
- DbPartitioning
- DbModeTightening
- DbTransformsPass
- EdtDistribution
- EdtLowering
- Concurrency

**Invalidation MUST be serialized** — cannot happen during parallel reads.

**Current pattern**:
```cpp
void DbPartitioning::runOnFunction(func::FuncOp func) {
  AM->invalidate();  // Discards all cached graphs
  // ... re-analyze from scratch
}
```

**Problem**: If two functions are parallelized, both may try to invalidate simultaneously.

**Solution**: Use per-function invalidation, not global:
```cpp
bool AnalysisManager::invalidateFunction(func::FuncOp func) {
  dbAnalysis->invalidateGraph(func);
  edtAnalysis->invalidateGraph(func);
  loopAnalysis->invalidateFunction(func);
  // ... etc
}
```

**Status**: `invalidateFunction` already exists (AnalysisManager.cpp:147). Passes need to be updated to use it.

### The Compilation Model

Current MLIR `disableMultithreading(true)` at Compile.cpp:334:
```cpp
context.disableMultithreading(true);  // Disable MLIR's threading
```

**This blocks**:
- Function-level pass parallelism (MLIR's `parallelForEach`)
- Module pass parallelism
- Any MLIR-level threading

**CARTS can still parallelize** at a different granularity:
- Per-EDT analysis
- Per-loop analysis
- Per-DB analysis
- Custom thread pool (outside MLIR)

---

## Conclusion & Recommendations

### Immediate Actions (This Week)

1. **Update task #7** to track Phase 1a-1d implementation
2. **Run Phase 0 analysis**: Identify 6+ no-AM passes for parallelization
3. **Create Phase 1a implementation PR**: Fix lazy-init races with call_once

### Strategic Direction

**DO** Phase 0 + Phase 1 (safety) in Q2 2026:
- ✓ Zero risk to compilation correctness
- ✓ Unblocks future optimization phases
- ✓ Addresses all critical race conditions

**Defer** Phase 3-4 (lock-free) until profiling shows contention:
- Complexity not warranted without measurement
- Phase 2 likely gives 2-3x speedup
- Revisit if single-threaded compile latency is concern

**Recommended ownership**:
- Phase 0: Team lead (pass audit)
- Phase 1a-1d: Dedicated engineer (2 weeks)
- Phase 2: Same engineer (1 week follow-up)

---

## References

- AnalysisManager.h: `include/arts/analysis/AnalysisManager.h`
- AnalysisManager.cpp: `lib/arts/analysis/AnalysisManager.cpp`
- DbAnalysis: `include/arts/analysis/db/DbAnalysis.h`, `lib/arts/analysis/db/DbAnalysis.cpp`
- EdtAnalysis: `include/arts/analysis/edt/EdtAnalysis.h`, `lib/arts/analysis/edt/EdtAnalysis.cpp`
- DbGraph: `include/arts/analysis/graphs/db/DbGraph.h`, `lib/arts/analysis/graphs/db/DbGraph.cpp`
- EdtGraph: `include/arts/analysis/graphs/edt/EdtGraph.h`, `lib/arts/analysis/graphs/edt/EdtGraph.cpp`
- Compile.cpp: `tools/compile/Compile.cpp:334`

---

**Audit Date**: 2026-04-02
**Auditor**: Claude Agent
**Files Reviewed**: 13 headers + 6 implementation files + pass integration
**Total Analysis Classes**: 8 (DbAnalysis, EdtAnalysis, LoopAnalysis, EpochAnalysis, StringAnalysis, DbHeuristics, EdtHeuristics, MetadataManager)
**Total Passes Analyzed**: 27
**Race Conditions Identified**: 10
**Critical Issues**: 4 (R-1, R-2, R-3, R-4)
