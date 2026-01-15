# Partitioning Decision Guide

This document describes how CARTS decides the datablock partitioning mode for each allocation. The decision is made once per DbAlloc and applies to all acquires of that allocation.

For rank-aware and multi-node specifics, see:
- /Users/randreshg/Documents/carts/docs/heuristics/single_rank
- /Users/randreshg/Documents/carts/docs/heuristics/multi_rank/db_granularity_and_twin_diff.md

## Modes at a Glance

+--------------+-------------------------------+---------------------------+----------------------------------+
| Mode         | What It Means                  | Best Fit                  | Tradeoffs                        |
+--------------+-------------------------------+---------------------------+----------------------------------+
| Coarse       | One datablock for full array   | Irregular or mixed access | Lowest overhead, least locality  |
| Element-wise | One datablock per element      | Precise indexed access    | Highest overhead                 |
| Chunked      | One datablock per block        | Blocked/tiling patterns   | Good locality, moderate overhead |
+--------------+-------------------------------+---------------------------+----------------------------------+

## Decision Flow (High-Level)

A compact reasoning flow that captures how the system decides without naming specific heuristics.

+-----------------------------------------------------------------------+
|                               START                                   |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| 1) Collect access evidence across all acquires for the allocation      |
|    - indices, offsets, sizes, read/write modes                         |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| 2) Normalize structure and run uniformity checks                       |
|    - Dimensional shape matches across acquires                         |
|    - Chunk rank and per-dim sizes are consistent                       |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| 3) Build intent signals                                                |
|    - Element-wise intent: indices are present everywhere               |
|    - Chunked intent: offsets/sizes are present and consistent          |
|    - Read-only: no writes across all acquires                          |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| 4) Evaluate environment and risk                                       |
|    - Single-node vs multi-node                                         |
|    - Safety of partitioning with observed access structure             |
|    - Expected overhead vs locality gain                                |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| 5) Choose a mode                                                       |
|    - Coarse if partitioning is unsafe or not beneficial                |
|    - Element-wise if precise indexing intent dominates                 |
|    - Chunked if block intent dominates and overhead is acceptable      |
+-----------------------------------------------------------------------+

## Partitioning Pipeline (End-to-End)

This flow includes the canonical chunk size step and shows where heuristics
consume the inputs.

+-----------------------------------------------------------------------+
| ForLowering (parallel loops)                                           |
| - create worker_id placeholder                                         |
| - compute offset/size hints per worker                                 |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| DbPartitioning: per-acquire analysis                                   |
| - access pattern summary                                               |
| - partition offset/size extraction                                     |
| - base chunk size candidate per acquire                                |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| Canonical chunk size per allocation                                    |
| - pick offset-independent base sizes                                  |
| - trace/hoist to dominate the alloc                                    |
| - combine candidates (min) into a single size                          |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| Build PartitioningContext                                              |
| - ctx.canChunked/ctx.canElementWise                                    |
| - ctx.chunkSize (canonical)                                            |
| - per-acquire voting                                                   |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| Heuristics (H1.x) -> decision                                          |
| - chooses Chunked/ElementWise/Coarse                                   |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| DbRewriter                                                             |
| - rewrites alloc and acquires to new grid                              |
+-----------------------------------------------------------------------+
            |
            v
+-----------------------------------------------------------------------+
| Attributes and diagnostics                                             |
| - arts.partition                                                       |
| - heuristics decisions recorded via HeuristicsConfig                   |
+-----------------------------------------------------------------------+

## What Signals Are Used

+------------------------------+------------------------------------------+---------------------------+
| Signal                       | How It Is Derived                        | Why It Matters            |
+------------------------------+------------------------------------------+---------------------------+
| Uniform access structure     | Uniformity checks pass for all acquires  | Enables safe partitioning |
| Element-wise intent          | Indices present for all acquires         | Enables element-wise mode |
| Chunked intent               | Offsets/sizes present and consistent     | Enables chunked mode      |
| Read-only allocation         | No write acquires across the allocation  | Reduces need to split DBs |
| Environment (single/multi)   | Execution context                        | Changes transfer tradeoff |
+------------------------------+------------------------------------------+---------------------------+

## Canonical Chunk Size Per Allocation

Chunked partitioning needs a single chunk size per allocation, even when
individual acquires have remainder-aware sizes (e.g., the last worker gets a
smaller slice). The canonicalization step derives a stable base size and makes
it available to the heuristics interface.

Algorithm (per allocation):

1) Collect offset/size hints from all chunked acquires.
2) For each acquire, try to strip remainder-aware sizes:
   - If size depends on offset, peel min/select to pick the non-offset branch.
   - If size does not depend on offset, use it directly.
3) Trace/hoist candidates to dominate the alloc site.
4) Canonicalize:
   - If candidates match, use that.
   - If candidates differ, use min(candidate_i) to stay safe.
5) Set ctx.chunkSize to the smallest static candidate (if any) and use the
   canonical dynamic size for rewriting.

This ensures all acquires are rewritten against the same allocation grid while
keeping per-worker offsets/sizes intact.

## Offset Validation (Chunked Safety)

Offset validation ensures that the partition offset is tied to the memref
indices used by the task. The current rule is:

- Prefer memref indices (load/store) for validation.
- If no memref indices exist, fall back to db_ref indices.
- The first dynamic index in that chain must depend on the partition offset.

Pseudocode:

```
for each memref access:
  chain = memref.indices if present else db_ref.indices
  first_dyn = first non-constant index in chain
  if first_dyn exists and !dependsOn(first_dyn, offset):
    fail
```

When twin_diff is unavailable, partitioning proceeds only if this rule holds
for all acquires, ensuring disjoint chunk ownership for writers.

## Why Uniformity Is Critical

Uniformity ensures that each EDT observes the same partitioning contract.

+------------------------------+----------------------+----------------------------+
| Pattern Consistency          | Partition Safety     | Expected Outcome           |
+------------------------------+----------------------+----------------------------+
| Same dimensional structure   | Safe                | Fine-grain can be used     |
| Mixed dimensional structure  | Unsafe              | Coarse required            |
| Inconsistent chunk sizes     | Unsafe              | Coarse required            |
+------------------------------+----------------------+----------------------------+

## Per-Acquire Voting

A single allocation may have multiple acquires with different characteristics. The system collects per-acquire information and aggregates it to make allocation-level decisions.

### Why Per-Acquire Voting

Consider an allocation with two acquires:
- Acquire #1: READ mode (in) - could use coarse
- Acquire #2: WRITE mode (out) - benefits from fine-grained

Without per-acquire voting, the system might choose coarse based on the first acquire alone. With voting, the write acquire's need for fine-grained partitioning takes priority.

### AcquireInfo Structure

Each acquire contributes:

+---------------------+-----------------------------+----------------------------------------+
| Field               | Type                        | Description                            |
+---------------------+-----------------------------+----------------------------------------+
| accessMode          | in/out/inout                | Read, write, or read-write access      |
| canElementWise      | bool                        | Has indexed dependencies               |
| canChunked          | bool                        | Has chunk/offset dependencies          |
| pinnedDimCount      | unsigned                    | Dimensions with indexed access         |
+---------------------+-----------------------------+----------------------------------------+

### Aggregation Helpers

+----------------------+--------------------------------------------+--------------------------------+
| Helper               | Logic                                      | Used By                        |
+----------------------+--------------------------------------------+--------------------------------+
| hasWriteAccess()     | Any acquire is out or inout                | H1.2 (optimistic path)         |
| allReadOnly()        | ALL acquires are in mode                   | H1.1 (read-only check)         |
| anyCanElementWise()  | Any acquire can use element-wise           | H1.4 (multi-node)              |
| anyCanChunked()      | Any acquire can use chunked                | H1.4 (multi-node)              |
| maxPinnedDimCount()  | Maximum pinnedDimCount across acquires     | H1.2 (outerRank decision)      |
+----------------------+--------------------------------------------+--------------------------------+

### Write-Priority Rule

Write modes have priority over read modes because:
- Concurrent writes benefit from fine-grained partitioning
- Read-only access can safely share coarse datablocks
- If ANY acquire is write, fine-grained is preferred

## Heuristic Per-Acquire Behavior

+------------+----------------------------+---------------------------------------------+
| Heuristic  | Per-Acquire Check          | Behavior                                    |
+------------+----------------------------+---------------------------------------------+
| H1.1       | allReadOnly()              | Only applies if ALL acquires are read-only  |
| H1.2       | hasWriteAccess()           | Allows element-wise even without indices    |
| H1.4       | anyCanChunked/ElementWise  | Uses any-acquire capability for multi-node  |
| H1.5       | accessPatterns summary     | Already aggregates stencil/mixed patterns   |
+------------+----------------------------+---------------------------------------------+

## Heuristic Evaluation Flowchart

Heuristics are evaluated in priority order (highest first). The first heuristic
that returns a decision wins.

                            +------------------+
                            |      START       |
                            +------------------+
                                     |
                                     v
                   +----------------------------------+
                   | Build PartitioningContext        |
                   | - Collect per-acquire info       |
                   | - Summarize access patterns      |
                   +----------------------------------+
                                     |
                                     v
    +----------------------------------------------------------------+
    | H1.1 (priority=100): Read-Only Single-Node                     |
    | Condition: allReadOnly() && nodeCount==1 && !canFineGrained    |
    +----------------------------------------------------------------+
                  |                              |
             [applies]                      [skip]
                  |                              |
                  v                              v
           +----------+        +------------------------------------------+
           | COARSE   |        | H1.5 (priority=95): Stencil Pattern     |
           +----------+        | Condition: hasStencil || isMixed        |
                               +------------------------------------------+
                                        |                    |
                                   [applies]            [skip]
                                        |                    |
                                        v                    v
                              +---------------+   +----------------------------------+
                              | Mixed->ElemWise|   | H1.4 (priority=90): Multi-Node  |
                              | Stencil->Stencil|   | Condition: nodeCount > 1        |
                              +---------------+   +----------------------------------+
                                                          |                |
                                                     [applies]         [skip]
                                                          |                |
                                                          v                v
                                              +----------------+  +---------------------------+
                                              | chunked or     |  | H1.3 (priority=80):      |
                                              | element-wise   |  | Access Uniformity        |
                                              +----------------+  | Condition: !isUniform &&  |
                                                                  |   !canFineGrained        |
                                                                  +---------------------------+
                                                                        |              |
                                                                   [applies]       [skip]
                                                                        |              |
                                                                        v              v
                                                                  +----------+  +-----------------+
                                                                  | COARSE   |  | H1.2 (priority=50)
                                                                  +----------+  | Cost Model      |
                                                                                +-----------------+
                                                                                       |
                                                                                       v
                                                            +-------------------------------------+
                                                            | Evaluate chunked vs element-wise   |
                                                            | - If canChunked && costOK: Chunked |
                                                            | - If canElemWise: ElementWise      |
                                                            | - If hasWriteAccess: ElementWise   |
                                                            +-------------------------------------+
                                                                               |
                                                                               v
                                                                        +-----------+
                                                                        | DECISION  |
                                                                        +-----------+

## Decision Equation

  Decision =
    if !canBePartitioned              -> Coarse
    else if H1.1 (read-only + single) -> Coarse
    else if H1.5 (stencil/mixed)      -> Stencil or ElementWise
    else if H1.4 (multi-node)         -> Chunked or ElementWise
    else if H1.3 (!uniform)           -> Coarse
    else if H1.2 (cost model)         -> Chunked or ElementWise
    else                              -> Coarse (fallback)

## When Chunked Beats Element-wise

Chunked mode is favored when the access pattern is contiguous and block-shaped, and the system expects locality benefits to outweigh overhead.

+------------------------------+-------------------------+----------------------------+
| Observation                  | Favor                   | Reason                     |
+------------------------------+-------------------------+----------------------------+
| Offsets/sizes present        | Chunked                 | Explicit block structure   |
| Only indices present         | Element-wise            | Fine-grain intent          |
| Dynamic chunk size           | Chunked (optimistic)    | Intent is clear            |
| Very small chunk capacity    | Element-wise or Coarse  | Overhead outweighs benefit |
+------------------------------+-------------------------+----------------------------+

## Mixed Mode Partitioning

Mixed mode enables chunked partitioning when an allocation has both chunked acquires
(in parallel regions) and coarse acquires (in non-parallel regions). Without mixed
mode, the presence of any coarse acquire would force the entire allocation to use
coarse-grained partitioning, serializing all parallel work.

### The Problem: Serialization Due to Mixed Acquires

Consider an array used in both initialization (sequential) and computation (parallel):

```
Allocation: sizes=[1], elementSizes=[N]  (COARSE - single block)

                    ┌─────────────────────────────────────────┐
                    │        Single Coarse Datablock          │
                    │   A[0] A[1] A[2] ... A[N-1]              │
                    └─────────────────────────────────────────┘
                                      │
        ┌─────────────────────────────┼─────────────────────────────┐
        │                             │                             │
   ┌────▼────┐                   ┌────▼────┐                   ┌────▼────┐
   │ Init    │ ───────────────▶  │ Worker0 │ ───────────────▶  │ Worker1 │ ...
   │ (full)  │    SERIALIZED!    │ (full)  │    SERIALIZED!    │ (full)  │
   └─────────┘                   └─────────┘                   └─────────┘

Problem: All workers must wait for exclusive access to the single coarse block.
```

### The Solution: Mixed Mode with Full-Range Acquires

Mixed mode uses chunked partitioning but allows coarse acquires to access all chunks:

```
Allocation: sizes=[numChunks], elementSizes=[chunkSize]  (CHUNKED)

        ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
        │ Chunk 0  │  │ Chunk 1  │  │ Chunk 2  │  │ Chunk 3  │ ...
        │ A[0..C]  │  │ A[C..2C] │  │ A[2C..3C]│  │ A[3C..4C]│
        └──────────┘  └──────────┘  └──────────┘  └──────────┘
              │             │             │             │
              ▼             ▼             ▼             ▼
   ┌─────────────────────────────────────────────────────────────┐
   │ Init EDT (FULL-RANGE: offset=0, size=numChunks)             │
   │ - Acquires ALL chunks                                        │
   │ - Uses div/mod indexing: dbRef[i/C], memref[i%C]            │
   └─────────────────────────────────────────────────────────────┘
              │
              ▼ (workers run in PARALLEL after init completes)
        ┌─────┴─────┬─────┴─────┬─────┴─────┐
        │           │           │           │
   ┌────▼────┐ ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
   │ Worker0 │ │ Worker1 │ │ Worker2 │ │ Worker3 │
   │ off=[0] │ │ off=[1] │ │ off=[2] │ │ off=[3] │
   │ size=1  │ │ size=1  │ │ size=1  │ │ size=1  │
   └─────────┘ └─────────┘ └─────────┘ └─────────┘

Benefit: Workers own disjoint chunks and can execute concurrently.
```

### How Index Localization Works

The DbChunkedRewriter uses div/mod localization for both single-chunk and full-range
acquires. The same arithmetic handles both cases correctly.

**Localization Formula:**
```
dbRefIdx  = (globalRow / chunkSize) - startChunk
memrefIdx = globalRow % chunkSize
```

**Single-Chunk Acquire (Parallel Worker):**

Worker owns chunk 2, accessing element at globalIdx=55 with chunkSize=25:

```
startChunk = 2
physChunk  = 55 / 25 = 2        (worker accesses its owned chunk)
dbRefIdx   = 2 - 2 = 0          (local view: chunk appears at index 0)
memrefIdx  = 55 % 25 = 5        (offset within chunk)

Result: dbRef[0], memref[5]  ← worker sees single chunk at index 0
```

**Full-Range Acquire (Non-Parallel Code):**

Init code accessing element at globalIdx=55 with chunkSize=25:

```
startChunk = 0
physChunk  = 55 / 25 = 2        (absolute chunk index)
dbRefIdx   = 2 - 0 = 2          (global view: chunk at absolute index)
memrefIdx  = 55 % 25 = 5        (offset within chunk)

Result: dbRef[2], memref[5]  ← init sees all chunks at their absolute indices
```

### Detection and Decision Rule

Mixed mode is detected when:
- `anyCanChunked()` returns true (at least one acquire has offset/size hints)
- Some acquires have `mode == PartitionMode::Coarse` (no hints)

Decision:
- Use chunked partitioning for the allocation
- Mark coarse acquires as `needsFullRange = true`
- Rewrite full-range acquires with `offset=0, size=numChunks`

### IR Transformation Example

**Before (Coarse - Serialized):**
```mlir
// Single coarse block - all acquires compete for exclusive access
%alloc = arts.db_alloc sizes[%c1] elementSizes[%N]
    {arts.partition = #arts.promotion_mode<none>}

%acq_init = arts.db_acquire %alloc offsets[%c0] sizes[%c1]
%acq_worker = arts.db_acquire %alloc offsets[%c0] sizes[%c1]
    offset_hints[%worker_off] size_hints[%chunk]  // Hints ignored!
```

**After (Mixed Mode - Parallel):**
```mlir
// Chunked allocation - workers can run in parallel
%alloc = arts.db_alloc sizes[%numChunks] elementSizes[%chunkSize]
    {arts.partition = #arts.promotion_mode<chunked>}

// Init: full-range access (all chunks)
%acq_init = arts.db_acquire %alloc offsets[%c0] sizes[%numChunks]

// Worker: single-chunk access (disjoint)
%acq_worker = arts.db_acquire %alloc offsets[%workerIdx] sizes[%c1]
```

### When Mixed Mode Applies

+----------------------------------+------------------+--------------------------------+
| Acquire Pattern                  | Mixed Mode?      | Resulting Behavior             |
+----------------------------------+------------------+--------------------------------+
| All chunked                      | No               | Standard chunked partitioning  |
| All coarse                       | No               | Coarse (no partitioning)       |
| Chunked + coarse (same alloc)    | Yes              | Chunked with full-range coarse |
| Chunked + element-wise           | No               | Falls back based on heuristics |
+----------------------------------+------------------+--------------------------------+

### Implementation Notes

1. **No New Rewriter Needed:** The existing DbChunkedRewriter handles full-range
   correctly because `startChunk=0` makes the subtraction a no-op.

2. **Allocation Grid:** Determined by chunked acquires (chunk count and size).
   Coarse acquires adapt to this grid by requesting all chunks.

3. **Dataflow Safety:** The ARTS runtime still enforces proper dependencies.
   Init completes before workers start because of the acquire ordering.

4. **Validation Relaxed:** Full-range acquires bypass offset validation since
   they access the entire allocation (no partition offset to validate).

## Summary

- The decision is per allocation, not per acquire.
- Partitioning is only enabled when it is safe and consistent across all acquires.
- Canonical chunk size is derived per allocation from acquire hints before heuristics run.
- Explicit intent signals (indices or chunk bounds) are honored when possible.
- Environment constraints (single vs multi-node) affect whether fine-grain is worth it.
- Per-acquire voting enables fine-grained decisions when acquires have different access modes.
- Write modes get priority: if any acquire writes, fine-grained partitioning is preferred.
- Mixed mode allows chunked partitioning even when some acquires are coarse, enabling
  parallel execution while preserving correctness for sequential initialization code.
