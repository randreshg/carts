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

**File**: `tools/run/carts-run.cpp`

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

**File**: `tools/run/carts-run.cpp`

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
| `tools/run/carts-run.cpp` | Add `--partition-fallback` CLI option |
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
carts run lulesh.mlir --concurrency-opt 2>&1 | grep "arts.partition" | sort | uniq -c

# Fine-grained
carts run lulesh.mlir --concurrency-opt --partition-fallback=fine 2>&1 | grep "arts.partition" | sort | uniq -c
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

# Part 2: DB Copy/Sync for Mixed Access Patterns

## Motivation

The fine-grained fallback (Part 1) helps but may have high overhead for LULESH due to:

- 8 acquires per element (one per node in hexahedron)
- High sync overhead for many small DBs

A better approach for mixed access patterns: **create separate DB versions with different partitioning, sync when needed**.

## The LULESH Pattern

```
x[] array lifecycle in LULESH iteration:

1. CalcPositionForNodes (node-parallel, WRITE)
   x[i] += xd[i] * dt    // Direct access, stride-1
   → Can use CHUNKED partitioning

2. CalcKinematicsForElems (element-parallel, READ)
   x_local[j] = x[nodelist[k][j]]  // Indirect access
   → Needs ELEMENT-WISE or full copy

3. Other element-parallel loops (READ)
   Same pattern as #2
```

## Proposed Solution: DB Versioning with Sync

```mlir
// Original allocation
%x_orig = arts.db_alloc memref<numNodes x f64>  // Source of truth

// Create read-optimized copy for indirect access
%x_copy = arts.db_copy(%x_orig) partition=element_wise
// OR: %x_copy = arts.db_copy(%x_orig) partition=none  // full replica

// Node-parallel loop: write to original (chunked)
arts.for (%i = 0 to %numNodes) {
  %x_chunk = arts.db_acquire(%x_orig) offsets[%start] sizes[%chunk_size]
  // x[i] += xd[i] * dt
}

// Sync copy with original before element-parallel reads
arts.db_sync(%x_copy, %x_orig)  // Copy updated data

// Element-parallel loop: read from copy
arts.for (%k = 0 to %numElem) {
  %n0 = load nodelist[%k, 0]
  %x_n0 = arts.db_acquire(%x_copy) indices[%n0] sizes[1]
  // Use x_n0...
}
```

## Benefits

1. **Write path optimized**: Chunked partitioning for direct writes (CalcPositionForNodes)
2. **Read path optimized**: Element-wise or replicated for indirect reads
3. **Explicit sync points**: Only sync when needed (between write and read phases)
4. **Reduced contention**: Writers and readers use different DB instances

## Implementation Plan (Part 2)

### Phase 2.1: Add `arts.db_copy` Operation

**File**: `include/arts/ArtsOps.td`

```tablegen
def DbCopyOp : Arts_Op<"db_copy", [...]> {
  let summary = "Create a copy of a datablock with different partitioning";
  let arguments = (ins
    AnyType:$sourceGuid,
    AnyType:$sourcePtr,
    OptionalAttr<PromotionModeAttr>:$partition
  );
  let results = (outs AnyType:$guid, AnyType:$ptr);
}
```

### Phase 2.2: Add `arts.db_sync` Operation

**File**: `include/arts/ArtsOps.td`

```tablegen
def DbSyncOp : Arts_Op<"db_sync", [...]> {
  let summary = "Synchronize datablock copy with source";
  let arguments = (ins
    AnyType:$destGuid,    // Copy to update
    AnyType:$sourceGuid,  // Source of truth
    DefaultValuedAttr<BoolAttr, "true">:$fullSync  // vs incremental
  );
}
```

### Phase 2.3: Analysis Pass for Copy Insertion

**File**: `lib/arts/Passes/Optimizations/DbVersioning.cpp` (NEW)

Analyze access patterns and insert db_copy/db_sync:

1. Identify DBs with mixed access patterns (write-direct + read-indirect)
2. Insert `db_copy` after allocation
3. Redirect indirect reads to copy
4. Insert `db_sync` at write→read boundaries

### Phase 2.4: Lower to Runtime

**File**: `lib/arts/Passes/Transformations/DbLowering.cpp`

- `db_copy` → `artsDbCopy()` runtime call
- `db_sync` → `artsDbSync()` runtime call (or memcpy for single-node)

### Phase 2.5: Runtime Support

**File**: ARTS runtime (external)

Need to ensure runtime supports:

- `artsDbCopy(srcGuid, dstGuid, partitionMode)`
- `artsDbSync(dstGuid, srcGuid)`

---

## Files to Add/Modify (Part 2)

| File | Change |
|------|--------|
| `include/arts/ArtsOps.td` | Add `DbCopyOp` and `DbSyncOp` |
| `lib/arts/Passes/Optimizations/DbVersioning.cpp` | **NEW** - analyze and insert copy/sync |
| `lib/arts/Passes/Optimizations/CMakeLists.txt` | Add DbVersioning.cpp |
| `include/arts/Passes/ArtsPasses.td` | Add DbVersioning pass |
| `lib/arts/Passes/Transformations/DbLowering.cpp` | Lower copy/sync ops |
| ARTS runtime | Add `artsDbCopy`, `artsDbSync` functions |

---

## Verification Plan (Part 2)

### Step 1: Test with Manual Annotation

First, manually insert db_copy/db_sync in LULESH MLIR to verify concept.

### Step 2: Automated Analysis

Run DbVersioning pass on LULESH, verify correct copy/sync insertion.

### Step 3: Performance Comparison

Compare three modes:

1. `--partition-fallback=coarse` (baseline)
2. `--partition-fallback=fine` (Part 1)
3. `--partition-fallback=versioned` (Part 2)

---

## Implementation Order

| Part | Description | Complexity | This PR? |
|------|-------------|------------|----------|
| **Part 1** | Fine-grained fallback option | Low | ✓ Yes |
| **Part 2** | DB versioning with copy/sync | High | Future |

**Part 1** enables quick experimentation. **Part 2** builds on learnings.

---

## Summary

| Approach | Parallelism | Overhead | Complexity |
|----------|-------------|----------|------------|
| Coarse (current) | None | Low | Simple |
| Fine-grained (Part 1) | High | High (many acquires) | Low |
| Versioned (Part 2) | High | Medium (sync cost) | High |
