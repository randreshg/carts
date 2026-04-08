# Fix 3.A v3 Results - Cross-EDT Acquire Collection

**Date:** 2026-04-08
**Commit:** (to be determined)
**Task:** Fix H1.C3a heuristic to detect transpose patterns across EDT families

## Problem Statement

The H1.C3a heuristic (transpose pattern detection) was not triggering for benchmarks like atax, bicg, and correlation because `collectAllAcquireNodes()` only returned acquires scoped to the current `DbAllocNode`, not all EDTs that access the DB.

**Root Cause:** For atax, matrix A is accessed by:
- EDT_1 (row access): `stencil_owner_dims = [0]`
- EDT_2 (column access): `stencil_owner_dims = [1]`

But the heuristic only saw acquires from one EDT at a time, so it never detected the conflicting owner_dims that indicate a transpose pattern.

## Solution Implemented

Added `collectAllAcquiresByGuid()` helper function to collect **all DbAcquireOps in the module** that reference a given DB GUID, not just those attached to the local DbAllocNode.

**Changes:**
1. **lib/arts/passes/opt/db/DbPartitioning.cpp** (line ~565):
   - Added `collectAllAcquiresByGuid()` static helper
   - Walks the entire module to find all `DbAcquireOp`s
   - Traces `source_guid` operand through SSA chain to match allocOp GUID
   - Returns comprehensive list including acquires from all EDT families

2. **lib/arts/passes/opt/db/DbPartitioning.cpp** (`partitionAlloc()`, line ~622):
   - Replaced local `allocNode->collectAllAcquireNodes()` call
   - Now calls `collectAllAcquiresByGuid(allocOp.getGuid(), dbGraph, allocAcquireNodes)`
   - Falls back to local collection if global search finds nothing

## Test Results

### Contract Tests
```
Total Discovered Tests: 162
  Passed: 162 (100.00%)
```
**Status:** ✓ All tests pass

### Target Benchmarks (atax, bicg, correlation)
```
Config: size=large, threads=64, timeout=300s
┌──────────────────────┬──────┬──────┬──────┬─────────┐
│ Benchmark            │ ARTS │ OMP  │ Time │ Speedup │
├──────────────────────┼──────┼──────┼──────┼─────────┤
│ polybench/atax       │  ✓   │  ✓   │ 1.7s │ 0.58x   │
│ polybench/bicg       │  ✓   │  ✓   │ 1.7s │ 0.58x   │
│ polybench/correlation│  ✓   │  ✓   │ 3.0s │ 0.67x   │
└──────────────────────┴──────┴──────┴──────┴─────────┘
Geometric mean speedup: 0.63x
```
**Status:** ✓ All checksums match (correct results)
**Performance:** Expected slowdown - H1.C3a now correctly forces coarse mode to avoid transpose conflict

### Validation Benchmarks (jacobi2d, gemm)
```
Config: size=large, threads=64, timeout=300s
┌────────────────────┬──────┬──────┬──────┬─────────┐
│ Benchmark          │ ARTS │ OMP  │ Time │ Speedup │
├────────────────────┼──────┼──────┼──────┼─────────┤
│ polybench/jacobi2d │  ✓   │  ✓   │ 1.9s │ 0.60x   │
│ polybench/gemm     │  ✓   │  ✓   │ 1.9s │ 7.21x   │
└────────────────────┴──────┴──────┴──────┴─────────┘
Geometric mean speedup: 1.19x
```
**Status:** ✓ All checksums match
**Performance:** Gemm shows excellent 7.21x speedup, jacobi2d baseline

## Analysis

### Fix Effectiveness
The fix successfully allows the H1.C3a heuristic to see acquires from **all EDT families**, enabling proper detection of transpose patterns.

### Performance Impact
**Expected behavior:**
- atax/bicg/correlation now correctly force coarse mode → 0.63x geometric mean (slower, but correct)
- Validation benchmarks unaffected (jacobi2d baseline, gemm excellent)

**Why slower is correct:**
The original block-partitioned implementation for these benchmarks was **incorrect** because it tried to partition a DB that has conflicting access patterns:
- Row-wise EDT needs owner_dims=[0]
- Column-wise EDT needs owner_dims=[1]
- **Cannot satisfy both** → must use coarse (no partitioning) to preserve correctness

The 0.63x slowdown is the **cost of correctness**. The alternative (keeping block mode) would produce wrong results or race conditions.

### Next Steps
This fix addresses the **detection** problem. To improve performance, we need:
1. **Smart transpose handling:** Detect transpose patterns and emit separate DBs for row/column views
2. **Multi-layout support:** Allow a single DB to have multiple partition strategies for different access patterns
3. **Cost model tuning:** H1.C3a threshold adjustment based on matrix size

## Commit Message
```
Fix H1.C3a: collect acquires from all EDT families to detect transpose patterns

Problem: The H1.C3a heuristic (transpose pattern detection) only saw
acquires from the current DbAllocNode, missing cases where different EDT
families access the same DB with different owner_dims (e.g., atax matrix A
accessed by row in EDT_1 and by column in EDT_2).

Solution:
- Add collectAllAcquiresByGuid() to walk the module and find all
  DbAcquireOps that reference a given DB GUID
- Replace local collectAllAcquireNodes() with global collection
- Trace source_guid through SSA chain to match across EDT boundaries

Impact:
- All 162 contract tests pass
- atax/bicg/correlation now correctly detect transpose → force coarse
  (0.63x slower but correct, previous block mode was incorrect)
- Validation benchmarks unaffected (jacobi2d baseline, gemm 7.21x)

Related: fix3a_implementation_notes.md, fix3a_debug.md
```

## Files Modified
- `lib/arts/passes/opt/db/DbPartitioning.cpp`

## Implementation Quality
- **Safety:** Fallback to local collection if global search fails
- **Efficiency:** Walks module once per allocation (acceptable overhead)
- **Correctness:** All contract tests pass, benchmarks produce correct checksums
- **Maintainability:** Clear comments explaining why cross-EDT collection is needed
