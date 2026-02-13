# LULESH Performance Fix: Configurable Partition Fallback

## Problem Summary

**250x slowdown** in LULESH: ARTS E2E = 11.28s vs OMP E2E = 0.045s

**Root Cause**: `DbAllocNode::canBePartitioned()` returns `false` early when `hasNonAffineAccesses = true`, blocking partitioning BEFORE heuristics can choose element-wise mode.

```
Flow: hasNonAffineAccesses = true
      → canBePartitioned() returns false (line 127)
      → partitionDb() skips this allocation
      → Allocation stays COARSE
      → All EDTs serialize on acquire
```

---

## Solution: CLI Option for Partition Fallback

Add `--partition-fallback={coarse,fine}` option to control behavior:

- `coarse` (default): Current behavior - block non-affine, use coarse allocation
- `fine`: New behavior - allow non-affine through, use element-wise allocation

**Benefits**:

1. Safe rollout - default preserves existing behavior
2. Easy A/B testing for performance comparison
3. User control for different workload characteristics

---

## Implementation Plan

### Phase 1: Add CLI Option

**File**: `tools/run/carts-compile.cpp`

Add command-line option:

```cpp
enum class PartitionFallback { Coarse, Fine };

static cl::opt<PartitionFallback> PartitionFallbackMode(
    "partition-fallback",
    cl::desc("Fallback strategy for non-partitionable allocations:"),
    cl::values(
        clEnumValN(PartitionFallback::Coarse, "coarse",
                   "Use coarse allocation (serialize access, default)"),
        clEnumValN(PartitionFallback::Fine, "fine",
                   "Use fine-grained (element-wise) allocation")),
    cl::init(PartitionFallback::Coarse));
```

### Phase 2: Add Pass Option

**File**: `include/arts/Passes/ArtsPasses.td`

Add option to DbPartitioning pass:

```tablegen
def DbPartitioning : Pass<"db-partitioning", "mlir::ModuleOp"> {
  // ... existing ...
  let options = [
    Option<"useFineGrainedFallback", "fine-grained-fallback", "bool", "false",
           "Use fine-grained (element-wise) fallback for non-affine accesses">
  ];
}
```

### Phase 3: Pass Option Through Pipeline

**File**: `tools/run/carts-compile.cpp`

Update `setupConcurrencyOpt()` to pass option:

```cpp
void setupConcurrencyOpt(PassManager &pm, arts::ArtsAnalysisManager *AM) {
  // ...
  bool useFineGrained = (PartitionFallbackMode == PartitionFallback::Fine);
  pm.addPass(arts::createDbPartitioningPass(AM, useFineGrained));
  // ...
}
```

### Phase 4: Update Pass Constructor

**File**: `include/arts/Passes/ArtsPasses.h`

```cpp
std::unique_ptr<Pass> createDbPartitioningPass(
    ArtsAnalysisManager *AM,
    bool useFineGrainedFallback = false);
```

**File**: `lib/arts/Passes/Optimizations/DbPartitioning.cpp`

Store option in pass and forward to HeuristicsConfig or use directly.

### Phase 5: Use Option in canBePartitioned()

**File**: `lib/arts/Analysis/Graphs/Db/DbAllocNode.cpp`

```cpp
bool DbAllocNode::canBePartitioned() {
  // ...
  if (hasNonAffineAccesses && *hasNonAffineAccesses) {
    bool hasAcquireWithHints = false;
    // ... check for hints ...
    if (!hasAcquireWithHints) {
      // Check if fine-grained fallback is enabled
      if (useFineGrainedFallback_) {
        ARTS_DEBUG("  Non-affine access - using fine-grained fallback");
        // Continue to allow partitioning (H1.3 will choose element-wise)
      } else {
        return skip("memref has non-affine accesses (no hints to override)");
      }
    }
  }
  // ...
}
```

### Phase 6: Ensure AccessPattern::Indexed is Set

**File**: `lib/arts/Analysis/Graphs/Db/DbAllocNode.cpp`

In `summarizeAcquirePatterns()`, when fine-grained fallback is enabled:

```cpp
AcquirePatternSummary DbAllocNode::summarizeAcquirePatterns() const {
  AcquirePatternSummary summary;

  for (const auto &acqNode : acquireNodes) {
    // ... existing switch ...
  }

  // NEW: If allocation has non-affine accesses AND fine-grained fallback enabled
  // mark as indexed so H1.3 triggers element-wise
  if (useFineGrainedFallback_ && hasNonAffineAccesses && *hasNonAffineAccesses) {
    summary.hasIndexed = true;
  }

  return summary;
}
```

---

## Files to Modify

| File | Change |
|------|--------|
| `tools/run/carts-compile.cpp` | Add `--partition-fallback` CLI option |
| `include/arts/Passes/ArtsPasses.td` | Add `fine-grained-fallback` pass option |
| `include/arts/Passes/ArtsPasses.h` | Update `createDbPartitioningPass()` signature |
| `lib/arts/Passes/Optimizations/DbPartitioning.cpp` | Accept and store fallback option |
| `lib/arts/Analysis/Graphs/Db/DbAllocNode.cpp` | Use option in `canBePartitioned()` and `summarizeAcquirePatterns()` |
| `include/arts/Analysis/Graphs/Db/DbAllocNode.h` | Add `useFineGrainedFallback_` member |

---

## Verification Plan

### Step 1: Build

```bash
cd /Users/randreshg/Documents/carts
carts build
```

### Step 2: Test Default Behavior (unchanged)

```bash
carts benchmarks run lulesh --size small
# Should be same as before (~11.28s)
```

### Step 3: Test Fine-Grained Fallback

```bash
carts benchmarks run lulesh --size small -- --partition-fallback=fine
# Target: ARTS E2E < 1s
```

### Step 4: Verify Correctness

```bash
carts benchmarks run lulesh --size small --verify -- --partition-fallback=fine
# Must show Correct: ✓ YES
```

### Step 5: Compare Partitioning Stats

```bash
# Coarse (default)
carts compile lulesh.mlir --concurrency-opt 2>&1 | grep "arts.partition" | sort | uniq -c

# Fine-grained
carts compile lulesh.mlir --concurrency-opt --partition-fallback=fine 2>&1 | grep "arts.partition" | sort | uniq -c
```

### Step 6: Run Full Benchmark Suite (both modes)

```bash
# Default - ensure no regressions
carts benchmarks run --all --size small

# Fine-grained - check improvements
carts benchmarks run --all --size small -- --partition-fallback=fine
```

---

## Success Metrics

| Metric | Coarse (default) | Fine-grained | Target |
|--------|-----------------|--------------|--------|
| LULESH ARTS E2E | 11.28s | TBD | <1s |
| Coarse allocations | 66 | TBD | <30 |
| Element-wise allocations | 32 | TBD | >60 |
| Speedup vs OMP | 0.00x | TBD | >0.5x |
| Correctness | ✓ | ✓ | ✓ |

---

## Risk Assessment (Part 1)

**Low Risk**:

- Default behavior is unchanged
- Changes are localized and testable
- Existing H1.3 heuristic and element-wise rewriter are already tested

---

# Part 2: Memory EDT Path for Mixed Access Patterns

## Motivation

The fine-grained fallback (Part 1) helps, but LULESH-style indirect access can still create high overhead:

- 8 acquires per element (one per node in hexahedron)
- high control overhead when each element triggers many tiny accesses

For mixed access patterns, the active direction is to insert explicit transformation tasks between producer and consumer phases.

## The LULESH Pattern

```
x[] array lifecycle in one iteration:

1. CalcPositionForNodes (node-parallel, WRITE)
   x[i] += xd[i] * dt
   -> blocked/chunked layout is best

2. CalcKinematicsForElems (element-parallel, READ)
   x_local[j] = x[nodelist[k][j]]
   -> indirect indexed reads favor element-local layout

3. Additional element-parallel reads
   -> same indirect pattern
```

## Proposed Solution: Memory EDT Recode/Gather

```mlir
// Source allocation
%x_guid, %x_ptr = arts.db_alloc ...

// Producer phase
arts.for (%i = 0 to %numNodes) {
  %x_chunk = arts.db_acquire(%x_guid, %x_ptr) offsets[%start] sizes[%block_size]
  // x[i] += xd[i] * dt
}

// Transform phase as explicit task
%x_local_guid, %x_local_ptr = arts.memory_edt recode
    source(%x_ptr) [source_layout = blocked]
    -> [dest_layout = fine_grained]
    dependencies(%x_ptr)

// Consumer phase reads transformed layout
arts.for (%k = 0 to %numElem) {
  %n0 = load nodelist[%k, 0]
  %x_n0 = arts.db_acquire(%x_local_guid, %x_local_ptr) indices[%n0] sizes[1]
  // Use x_n0...
}
```

## Benefits

1. Write path remains optimized for direct updates.
2. Read path is optimized for indirect element access.
3. Data-layout transforms are explicit tasks in the EDT graph.
4. Policy stays compiler-driven through analysis + heuristics, not ad-hoc pass logic.

## Current Status and Next Milestones

1. Part 1 (`--partition-fallback`) is implemented and validated.
2. Next step for this track is Memory EDT insertion backed by DB/EDT analysis interfaces.
3. Distributed DB ownership is now available behind `--distributed-db-ownership` for eligible allocations.

## Summary

| Approach | Parallelism | Overhead | Complexity |
|----------|-------------|----------|------------|
| Coarse (current) | None | Low | Simple |
| Fine-grained fallback | High | High (many acquires) | Low |
| Memory EDT transform path | High | Medium (amortized transform) | Medium |
