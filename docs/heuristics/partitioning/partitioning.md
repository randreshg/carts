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
