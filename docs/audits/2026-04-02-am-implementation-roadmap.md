# AnalysisManager Thread-Safety Fix Implementation Roadmap

## Quick Reference: What to Fix

### Phase 1 Implementation Checklist (1-2 weeks, CRITICAL)

- [ ] **Phase 1a: Lazy Initialization** (2-3 days)
  - [ ] AnalysisManager::getDbAnalysis() → add std::call_once
  - [ ] AnalysisManager::getEdtAnalysis() → add std::call_once
  - [ ] AnalysisManager::getEpochAnalysis() → add std::call_once
  - [ ] AnalysisManager::getLoopAnalysis() → add std::call_once
  - [ ] AnalysisManager::getStringAnalysis() → add std::call_once
  - [ ] AnalysisManager::getDbHeuristics() → add std::call_once
  - [ ] AnalysisManager::getEdtHeuristics() → add std::call_once
  - [ ] AnalysisManager::getMetadataManager() → add std::call_once

- [ ] **Phase 1b: Graph Map Protection** (3-4 days)
  - [ ] DbAnalysis: Add std::shared_mutex to functionGraphMap
  - [ ] EdtAnalysis: Add std::shared_mutex to edtGraphs
  - [ ] LoopAnalysis: Add std::shared_mutex to loopNodes
  - [ ] DbAnalysis::getOrCreateGraph() → use unique_lock
  - [ ] EdtAnalysis::getOrCreateEdtGraph() → use unique_lock
  - [ ] LoopAnalysis::getOrCreateLoopNode() → use unique_lock

- [ ] **Phase 1c: Clear-then-Read Pattern** (2 days)
  - [ ] EdtAnalysis::analyze() → build-then-swap pattern
  - [ ] DbGraph::build() → build-then-swap pattern
  - [ ] EdtGraph::build() → build-then-swap pattern

- [ ] **Phase 1d: Atomic Flags** (1 day)
  - [ ] Replace bool with std::atomic<bool>: DbGraph, EdtGraph, EdtAnalysis
  - [ ] Replace uint64_t with std::atomic<uint64_t>: version counters
  - [ ] AnalysisManager::setMetadataCoverage() → use atomics or mutex

### Phase 0 Implementation Checklist (1 day, FASTEST WIN)

- [ ] Audit passes for AM usage
  - [ ] CanonicalizeMemrefs — likely no AM
  - [ ] InitialCleanup — likely no AM
  - [ ] ArtsToLlvm — likely no AM
  - [ ] Others: OpenmpToArts, PatternPipeline, AdditionalOpt
- [ ] Validate 6+ passes that don't call AM
- [ ] Register those passes for parallelization in Compile.cpp

---

## Detailed Fix Instructions

### Phase 1a: Fix Lazy Initialization

#### File: include/arts/analysis/AnalysisManager.h

**Add private members for synchronization**:
```cpp
private:
  std::once_flag dbAnalysisOnce;
  std::once_flag edtAnalysisOnce;
  std::once_flag epochAnalysisOnce;
  std::once_flag loopAnalysisOnce;
  std::once_flag stringAnalysisOnce;
  std::once_flag dbHeuristicsOnce;
  std::once_flag edtHeuristicsOnce;
  std::once_flag metadataManagerOnce;
```

#### File: lib/arts/analysis/AnalysisManager.cpp

**Replace each getter** (8 total):

Before:
```cpp
DbAnalysis &AnalysisManager::getDbAnalysis() {
  if (!dbAnalysis)
    dbAnalysis = std::make_unique<DbAnalysis>(*this);
  return *dbAnalysis;
}
```

After:
```cpp
DbAnalysis &AnalysisManager::getDbAnalysis() {
  std::call_once(dbAnalysisOnce, [this]() {
    dbAnalysis = std::make_unique<DbAnalysis>(*this);
  });
  return *dbAnalysis;
}
```

**Repeat for**: getEdtAnalysis, getEpochAnalysis, getLoopAnalysis, getStringAnalysis, getDbHeuristics, getEdtHeuristics, getMetadataManager

**Add include**: `#include <mutex>`

**Effort**: 30 minutes (8 nearly-identical changes)
**Testing**: Existing unit tests should pass (no behavior change)

---

### Phase 1b: Protect Graph Maps

#### File: include/arts/analysis/db/DbAnalysis.h

Add member:
```cpp
private:
  mutable std::shared_mutex functionGraphMutex;
```

#### File: lib/arts/analysis/db/DbAnalysis.cpp

Wrap getOrCreateGraph (find location ~line 152):
```cpp
DbGraph &DbAnalysis::getOrCreateGraph(func::FuncOp func) {
  std::unique_lock lock(functionGraphMutex);
  auto &graph = functionGraphMap[func];
  if (!graph)
    graph = std::make_unique<DbGraph>(func, this);
  return *graph;
}
```

Also protect invalidateGraph (similar pattern):
```cpp
bool DbAnalysis::invalidateGraph(func::FuncOp func) {
  std::unique_lock lock(functionGraphMutex);
  return functionGraphMap.erase(func) > 0;
}
```

#### File: include/arts/analysis/edt/EdtAnalysis.h

Add member:
```cpp
private:
  mutable std::shared_mutex edtGraphsMutex;
```

#### File: lib/arts/analysis/edt/EdtAnalysis.cpp

Wrap getOrCreateEdtGraph (find location):
```cpp
EdtGraph &EdtAnalysis::getOrCreateEdtGraph(func::FuncOp func) {
  std::unique_lock lock(edtGraphsMutex);
  auto &graph = edtGraphs[func];
  if (!graph) {
    graph = std::make_unique<EdtGraph>(func, dbGraph, this);
    graph->build();
  }
  return *graph;
}
```

Also protect invalidateGraph:
```cpp
bool EdtAnalysis::invalidateGraph(func::FuncOp func) {
  std::unique_lock lock(edtGraphsMutex);
  return edtGraphs.erase(func) > 0;
}
```

#### File: include/arts/analysis/loop/LoopAnalysis.h

Add member:
```cpp
private:
  mutable std::shared_mutex loopNodesMutex;
```

#### File: lib/arts/analysis/loop/LoopAnalysis.cpp

Wrap getOrCreateLoopNode:
```cpp
LoopNode *LoopAnalysis::getOrCreateLoopNode(Operation *loopOp) {
  std::unique_lock lock(loopNodesMutex);
  auto &node = loopNodes[loopOp];
  if (!node)
    node = std::make_unique<LoopNode>(loopOp);
  return node.get();
}
```

**Add includes**: `#include <shared_mutex>`

**Effort**: 2-3 hours
**Testing**: Existing tests validate correctness; benchmark to measure lock contention

---

### Phase 1c: Fix Clear-then-Read Pattern

This is the most complex fix. The issue: `clear()` while other threads read.

#### File: lib/arts/analysis/edt/EdtAnalysis.cpp

**Current code** (lines ~47-56):
```cpp
void EdtAnalysis::analyze() {
  edtPatternByOp.clear();
  allocPatternByOp.clear();
  edtOrderIndex.clear();
  // ... populate maps
}
```

**Fixed code**:
```cpp
void EdtAnalysis::analyze() {
  DenseMap<Operation *, EdtDistributionPattern> newPatterns;
  DenseMap<Operation *, DbAccessPattern> newAllocs;
  DenseMap<EdtOp, unsigned> newOrderIndex;

  auto &AM = getAnalysisManager();
  for (auto func : AM.getModule().getOps<func::FuncOp>()) {
    analyzeFunc(func);  // populates newPatterns, newAllocs, newOrderIndex
  }

  {
    std::unique_lock lock(patternMutex);  // NEW: add mutex
    edtPatternByOp = std::move(newPatterns);
    allocPatternByOp = std::move(newAllocs);
    edtOrderIndex = std::move(newOrderIndex);
  }

  analyzed = true;
}
```

**Add to EdtAnalysis.h**:
```cpp
private:
  mutable std::shared_mutex patternMutex;  // Protects edtPatternByOp, allocPatternByOp, edtOrderIndex
```

#### File: include/arts/analysis/graphs/db/DbGraph.h

**Add member**:
```cpp
private:
  mutable std::shared_mutex buildMutex;
```

#### File: lib/arts/analysis/graphs/db/DbGraph.cpp

**Find build() function and refactor**:

Current:
```cpp
void DbGraph::build() {
  allocNodes.clear();
  acquireNodeMap.clear();
  opOrder.clear();
  collectNodes();
  computeOpOrder();
  computeMetrics();
  built = true;
}
```

Fixed:
```cpp
void DbGraph::build() {
  DenseMap<DbAllocOp, std::unique_ptr<DbAllocNode>> newAllocNodes;
  DenseMap<DbAcquireOp, DbAcquireNode*> newAcquireNodeMap;
  DenseMap<Operation*, unsigned> newOpOrder;

  // Build into temporary maps
  // ... (copy collectNodes, computeOpOrder logic but use new* maps)

  {
    std::unique_lock lock(buildMutex);
    allocNodes = std::move(newAllocNodes);
    acquireNodeMap = std::move(newAcquireNodeMap);
    opOrder = std::move(newOpOrder);
    built = true;
    version++;
  }
}
```

**Same pattern for EdtGraph::build()** in lib/arts/analysis/graphs/edt/EdtGraph.cpp

**Effort**: 4-5 hours (requires careful refactoring)
**Testing**: Critical — test that concurrent reads don't crash during build()

---

### Phase 1d: Atomic Flags

#### File: include/arts/analysis/graphs/db/DbGraph.h

Replace:
```cpp
private:
  bool built = false;
  bool needsRebuild = true;
  uint64_t version = 0;
```

With:
```cpp
private:
  std::atomic<bool> built{false};
  std::atomic<bool> needsRebuild{true};
  std::atomic<uint64_t> version{0};
```

#### File: include/arts/analysis/graphs/edt/EdtGraph.h

Same changes for EdtGraph.

#### File: include/arts/analysis/edt/EdtAnalysis.h

Replace:
```cpp
private:
  bool analyzed = false;
```

With:
```cpp
private:
  std::atomic<bool> analyzed{false};
```

#### File: include/arts/analysis/AnalysisManager.h

For metadataCoverage, either:
- Option A: Wrap in std::shared_mutex (more conservative)
- Option B: Use std::atomic<int64_t> for each field (less safe but simpler)

Recommend Option A:
```cpp
private:
  mutable std::shared_mutex metadataCoverageMutex;
  MetadataCoverage metadataCoverage;
```

Then in setMetadataCoverage():
```cpp
void AnalysisManager::setMetadataCoverage(...) {
  std::unique_lock lock(metadataCoverageMutex);
  metadataCoverage.loopsAnalyzed = loopsAnalyzed;
  // ...
}
```

And in getMetadataCoverage():
```cpp
const MetadataCoverage &AnalysisManager::getMetadataCoverage() const {
  std::shared_lock lock(metadataCoverageMutex);
  return metadataCoverage;
}
```

**Effort**: 30 minutes
**Testing**: Existing tests

---

## Phase 0 Implementation

### Step 1: Audit Passes for AM Usage

Run grep to find which passes DON'T use AnalysisManager:

```bash
grep -r "AnalysisManager\|->invalidate\|getDbAnalysis\|getEdtAnalysis" \
  lib/arts/passes/opt/general \
  lib/arts/passes/opt/edt/EdtStructuralOpt.cpp \
  lib/arts/passes/transforms/EdtLowering.cpp
```

Passes likely to be AM-free:
- CanonicalizeMemrefs
- InitialCleanup
- OpenmpToArts (likely, needs verification)
- PatternPipeline (likely, needs verification)
- AdditionalOpt
- ArtsToLlvm

### Step 2: Register for Parallel Execution

In Compile.cpp, find where passes are added to PM:

Current (likely):
```cpp
pm.addPass(createCanonicalizeMemrefsPass());
pm.addPass(createInitialCleanupPass());
```

Change to use parallel execution:
```cpp
// TODO: Only if pass doesn't use AM
pm.addPass(createCanonicalizeMemrefsPass());
pm.addPass(createInitialCleanupPass());
pm.enableMultithreading(true);  // Enable MLIR threading for these passes
pm.addPass(createArtsToLlvmPass());
pm.enableMultithreading(false);  // Disable before AM-using passes
```

OR use function-level parallelism:
```cpp
auto canPass = createCanonicalizeMemrefsPass();
canPass->setThreadingMode(PassThreadingMode::PerModule);
pm.addPass(std::move(canPass));
```

**Effort**: 4-6 hours (depends on current pass structure)

---

## Validation & Testing

### After Phase 1a (Lazy Init):
```bash
carts test
carts lit tests/contracts/
```
Should pass without changes.

### After Phase 1b (Graph Maps):
```bash
# Compile with -fsanitize=thread to detect races
clang++ -fsanitize=thread ...
carts build
```

### After Phase 1c (Clear-then-Read):
```bash
# Stress test with high parallelism
export OMP_NUM_THREADS=16
carts compile example.mlir --O3
```

### After Phase 1d (Atomic Flags):
```bash
carts test
carts benchmark jacobi2d gemm specfem3d
```

---

## Estimated Timeline

| Phase | Duration | Dependency | Owner |
|-------|----------|-----------|-------|
| 0 | 1 day | None | Team lead (pass audit) |
| 1a | 2-3 days | None | Dedicated engineer |
| 1b | 3-4 days | 1a | Same engineer |
| 1c | 2 days | 1b | Same engineer |
| 1d | 1 day | 1c | Same engineer |
| **Total Phase 1** | **8-12 days** | Sequential | **1 engineer** |
| 2 | 2-3 days | Phase 1 complete | Same engineer |
| **Total (Phase 0+1+2)** | **11-16 days** | Sequential | **2 engineers** |

### Parallel Schedule (Optimal)

**Week 1**:
- Engineer A: Phase 0 (pass audit) + 1a (lazy init)
- Engineer B: Document and plan

**Week 2**:
- Engineer A: Phase 1b-1d (graph protection, clear-then-read, atomics)
- Engineer B: Code review + testing

**Week 3**:
- Engineer A: Phase 2 (eager init)
- Engineer B: Benchmarking

**Completion**: 2.5-3 weeks with 2 engineers

---

## Rollout Strategy

### Stage 1: Feature Flag (Hidden)
- Add `--enable-am-threading` CLI flag
- Enable only when explicitly set
- Default OFF (safe)

### Stage 2: Alpha Testing
- Enable for internal benchmarks
- Monitor for any issues
- Gather performance data

### Stage 3: Beta (Default Opt-In)
- Set default ON for parallel builds
- Provide --disable-am-threading escape hatch

### Stage 4: Stable (Default)
- Remove flag, always thread-safe

---

## Risk Mitigation

### High-Risk Areas

1. **DenseMap reallocation during concurrent insert**
   - **Mitigation**: shared_mutex ensures only one writer
   - **Test**: Run AddressSanitizer, ThreadSanitizer

2. **Version/flag consistency**
   - **Mitigation**: Use std::atomic
   - **Test**: Verify version increments correctly under load

3. **Graph build invalidation**
   - **Mitigation**: Per-function invalidation, not global
   - **Test**: Unit tests for invalidateFunction()

### Validation Checklist
- [ ] All existing tests pass
- [ ] ThreadSanitizer reports no new warnings
- [ ] AddressSanitizer reports no new issues
- [ ] Benchmarks show expected speedup (or at least no regression)
- [ ] Code review by 2+ domain experts
- [ ] 1 week real-world testing before release

---

## References

- Main audit report: `docs/audits/2026-04-02-am-thread-safety-audit.md`
- AnalysisManager code: `lib/arts/analysis/AnalysisManager.cpp`
- C++ threading reference: https://en.cppreference.com/w/cpp/thread/call_once
- MLIR threading: https://mlir.llvm.org/docs/PassManagement/#threading

