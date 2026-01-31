# Partitioning (Technical Guide)

This document explains CARTS datablock partitioning as a technical reference. It focuses on what each partitioning mode means, how the data layout changes, and how indices map in the rewriters and indexers.

---

## Core Distinction: Allocation Layout vs Acquire Range

Partitioning has two orthogonal concepts:

1) Allocation layout (how the datablock is shaped)
2) Acquire range (how many blocks a particular acquire requests)

Coarse is a layout choice (outer sizes = 1). Full-range is an acquire choice (request all blocks), and can happen under block or fine-grained layouts.

Coarse layout (single DB)
- Outer sizes = [1]
- Inner sizes = original shape
- There is exactly one datablock

Full-range acquire (many DBs)
- Outer sizes = many blocks
- Acquire offsets = [0]
- Acquire sizes = [numBlocks]
- You still get the multi-block layout, but the acquire spans all blocks

This distinction is critical for correctness and performance:
- Coarse: no partitioning, simplest correctness, no parallel locality.
- Full-range: keep the block layout, but allow an access pattern that is not tied to the partition offset.

---

## Terminology

- Outer sizes: number of blocks per partitioned dimension
- Inner sizes: block size for partitioned dimensions; original sizes for non-partitioned dims
- partitionedDims: which original dimensions are partitioned (supports non-leading)
- startBlock: acquire-specific block start used for localizing indices

---

## Responsibilities and Shared Data

Partitioning is split into clear roles. The shared data structure is PartitionInfo
and its context/decision wrappers.

Flow of responsibility

  DbAcquireNode  ->  DbAllocNode  ->  Heuristics  ->  DbPartitioning  ->  Rewriters/Indexers
  (analysis)         (summary)        (decision)      (coordination)     (execution)

What each layer owns

- DbAcquireNode: access pattern classification, bounds, partition dims.
- DbAllocNode: aggregation of acquire patterns and alloc shape summary.
- Heuristics: chooses PartitioningDecision based on context.
- DbPartitioning: coordinates the decision, applies it, preserves hints, and
  legalizes invalid choices.
- Rewriters/Indexers: perform layout changes and index localization from
  PartitionInfo + plan.

Rules of engagement

- Analysis stays in Acquire/Alloc nodes.
- Decisions stay in Heuristics.
- DbPartitioning only coordinates and legalizes.
- Rewriters/Indexers only execute; they do not re-decide.

---

## Mode 1 - Coarse (Single DB)

Meaning: one datablock holds the entire array.

Theorem (Coarse Layout)
If the allocation is coarse, then all indices map directly to the inner memref; the db_ref index is always 0.

Layout (2D example A[4][8])

  Single datablock
  +-------------------------+
  | [0,0]..[0,7]            |
  | [1,0]..[1,7]            |
  | [2,0]..[2,7]            |
  | [3,0]..[3,7]            |
  +-------------------------+
  db_ref[0]

Index mapping
- db_ref index = 0
- memref indices = original indices

---

## Mode 2 - Fine-Grained (Element-wise)

Meaning: one datablock per element of the outer dimensions. (In practice, "element" can be a row for 2D or a plane for 3D depending on outerRank.)

Theorem (Element-wise Layout)
If outerRank = r, then each unique outer index tuple selects a distinct datablock; remaining indices address inside that block.

Layout (2D example A[4][8], outerRank = 1)

  4 datablocks, each holds one row
  +-------+ +-------+ +-------+ +-------+
  | Row 0 | | Row 1 | | Row 2 | | Row 3 |
  +-------+ +-------+ +-------+ +-------+
   db_ref[0] db_ref[1] db_ref[2] db_ref[3]

Index mapping
- db_ref index = outer indices
- memref indices = remaining indices

Full-range acquire in fine-grained
- Outer layout is still many DBs
- Acquire requests all element blocks
- Used when access does not depend on the partition offset

---

## Mode 3 - Block (Blocked)

Meaning: each datablock contains a contiguous block along one or more dimensions.

Theorem (Block Layout)
If a dimension d is partitioned with block size B, then:
- block index = floor(global_d / B)
- local index = global_d mod B
The db_ref index is the block index (relative to the acquire start), and the memref index is the local index.

Layout (2D example A[4][8], blockSize=2 on dim 0)

  +---------------+  +---------------+
  | Block 0       |  | Block 1       |
  | rows 0-1      |  | rows 2-3      |
  +---------------+  +---------------+
   db_ref[0]         db_ref[1]

Index mapping (dim 0 blocked)
- db_ref index = i / B
- memref indices = [i % B, j]

Full-range acquire in block mode
- Allocation has many blocks
- Acquire offsets = [0]
- Acquire sizes = [numBlocks]
- Used when the access pattern does not depend on the partition offset

---

## Mode 4 - Stencil (Block + Halo)

Meaning: block layout plus halos for neighbor access. Uses ESD (ephemeral slice dependencies) to fetch halo data.

Theorem (Stencil Layout)
Let haloLeft and haloRight be the stencil halos. A worker with block start S can access a local region extended by halos without accessing other workers' owned blocks directly.

Layout (1D example, blockSize=4, halo=1)

  Block 1 (owned) with halos
  +--+-------------+--+
  |H | owned rows  |H |
  +--+-------------+--+
   left halo         right halo

Notes
- Stencil mode assumes leading partitioned dims.
- Non-leading partitioning with stencil is downgraded to block mode.

---

## Non-Leading Partitioning (partitionedDims)

CARTS supports partitioning non-leading dimensions. The selected dimensions are recorded in partitionedDims and used by the block indexer.

Theorem (Non-Leading Block Layout)
Let partitionedDims = [p0, p1, ...] and blockSizes = [B0, B1, ...]. Then:
- outerSizes[k] = ceil(dimSize[p_k] / B_k)
- innerSizes preserves original dimension order, but replaces each partitioned dim size with its block size
- db_ref indices correspond to partitionedDims order
- memref indices keep original order, with partitioned dims localized by modulo

Example A[i][j][k], partition only k (partitionedDims=[2], B=4)

  Layout:
  - outerSizes = [ceil(K/4)]
  - innerSizes = [I, J, 4]

  Mapping:
  - db_ref index = k / 4
  - memref indices = [i, j, k % 4]

Example A[i][j][k], partition i and k (partitionedDims=[0,2])

  Layout:
  - outerSizes = [ceil(I/Bi), ceil(K/Bk)]
  - innerSizes = [Bi, J, Bk]

  Mapping:
  - db_ref indices = [i / Bi, k / Bk]
  - memref indices = [i % Bi, j, k % Bk]

---

## Coarse vs Full-Range Summary (Quick Table)

| Concept | What changes | When used |
|--------|--------------|-----------|
| Coarse layout | Outer sizes = 1; single DB | No partitioning possible or beneficial |
| Full-range acquire | Acquire spans all blocks | Access does not depend on partition offset |

---

## Checklist for Correctness

- Use coarse only when partitioning is unsafe or pointless.
- Use full-range acquire when access does not depend on the partition offset.
- Ensure partitionedDims matches the actual access dimension(s).
- For stencil: use only leading partitioned dimensions.

---

## Where the Meaning Is Implemented

- Allocation layout is created in DbPartitioning.
- Acquire range is enforced per-acquire (full-range vs single block).
- Index mapping is handled by DbBlockIndexer / DbElementWiseIndexer.

This keeps semantics explicit: heuristics decide the layout, rewriters apply it, indexers localize indices.
