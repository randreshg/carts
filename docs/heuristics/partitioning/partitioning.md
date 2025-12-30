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

## Summary

- The decision is per allocation, not per acquire.
- Partitioning is only enabled when it is safe and consistent across all acquires.
- Explicit intent signals (indices or chunk bounds) are honored when possible.
- Environment constraints (single vs multi-node) affect whether fine-grain is worth it.
- Per-acquire voting enables fine-grained decisions when acquires have different access modes.
- Write modes get priority: if any acquire writes, fine-grained partitioning is preferred.
