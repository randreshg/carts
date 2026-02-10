# Partitioning (Technical Guide)

This document explains CARTS datablock partitioning as a technical reference.
It focuses on:
- What each partitioning mode means
- How the data layout changes (ASCII diagrams)
- How index localization works (db_ref vs local indices)
- Where decisions are made (analysis vs heuristics vs rewriting)

---

## 0) The Core Distinction: Allocation Layout vs Acquire Range

Partitioning has two orthogonal concepts:

1) Allocation layout (how the datablock is shaped and how many DBs exist)
2) Acquire range (how many DB entries a particular acquire requests)

Coarse is a *layout* choice (outer sizes = 1). Full-range is an *acquire* choice
(request all blocks), and can happen under block, stencil, or fine-grained
layouts.

Coarse layout (single DB)
- outerSizes = [1]
- innerSizes = original shape
- there is exactly one datablock

Full-range acquire (many DBs)
- outerSizes = many blocks/elements
- acquire offsets = [0]
- acquire sizes = [numBlocks]
- you keep the multi-block layout, but this acquire spans all blocks

Why this matters:
- Coarse: simplest correctness, but forces shared DB access (poor locality and
  less concurrency).
- Full-range: keep the block layout (good concurrency), but allow an access
  pattern that is not tied to the per-task partition offset.

---

## 1) Terminology and Notation

For a logical array A[d0][d1]...[dR-1]:

- partitionedDims: which original dimensions are partitioned (supports
  non-leading for Block / Fine-grained).
- outerSizes: number of DB entries per partitioned dimension, in
  partitionedDims order.
- innerSizes: per-DB shape, in original dimension order (partitioned dims are
  replaced with block sizes or 1 depending on mode).
- startBlock: acquire-specific outer offset (block index) used for localizing
  indices.
- blockSize: the per-chunk size along the partitioned dimension (Block / Stencil).

Index mapping always has two levels:
- db_ref indices: choose which DB entry (outer index tuple).
- local indices: address inside the inner memref for that DB.

---

## 2) Responsibility Split (Who Does What)

Partitioning is intentionally split into roles. The rule is:

- Acquire/Alloc nodes analyze
- Heuristics decide
- DbPartitioning coordinates + applies
- Rewriters/Indexers execute

Flow of responsibility:

  DbAcquireNode  ->  DbAllocNode  ->  Heuristics  ->  DbPartitioning  ->  Rewriters/Indexers
  (analysis)         (summary)        (decision)      (coordination)     (execution)

What each layer owns:

- DbAcquireNode: per-acquire access classification (uniform/indexed/stencil),
  bounds validity, indirect access flags, and partition dim candidates.
- DbAllocNode: aggregation across acquires for one allocation.
- Heuristics: chooses allocation-level PartitionMode (H1.1-H1.6) and
  per-acquire full-range decisions (H1.7).
- DbPartitioning: builds a rewrite plan, legalizes invalid cases, and invokes
  the correct rewriter/indexer.
- Rewriters/Indexers: implement the chosen layout and localize indices using
  PartitionInfo + plan.

DB mode note (in/out/inout):
- Db access mode is adjusted by the Db pass based on actual loads/stores.
  "Force inout" style policies should not be needed; if something is read and
  written, it becomes inout automatically.

---

## 3) How Decisions Are Taken (Heuristics Overview)

Partitioning decisions happen at two levels:

Allocation-level (one decision for the DbAlloc):
- coarse / fine_grained / block / stencil

Acquire-level (can differ per acquire):
- full-range vs single-block acquire, driven by whether the access depends on
  the partition offset (or is indirect).

### 3.1 Allocation-Level Heuristics (H1.1-H1.6)

This is the current intent (see `lib/arts/Analysis/ArtsHeuristics.cpp`):

- H1.1: Read-only + single-node + no partition capability -> Coarse
- H1.1b: Read-only + single-node + all block-capable acquires are full-range -> Coarse
- H1.2: Direct writes + indirect reads -> Block (indirect uses full-range)
- H1.2b: Direct + indirect writes -> Block (indirect uses full-range)
- H1.3: Stencil patterns -> Stencil (ESD) when block-capable, else fallback
  - Special-case: read-only stencil on single-node -> Coarse (avoid halo overhead)
- H1.3 (indexed): Indexed patterns -> Block when possible, else element-wise
- H1.4: Uniform direct access -> Block
- H1.5: Multi-node -> Prefer Block (else fine-grained) for network efficiency
- H1.6: Non-uniform + no capability -> Coarse

Important naming detail:
- In the implementation, "block" is a family: PartitioningDecision::isBlock()
  returns true for both Block and Stencil (Stencil is "Block + ESD halos").

### 3.2 Per-Acquire Full-Range (H1.7)

Some acquires cannot be safely localized to a single block:
- indirect access (data-dependent indices)
- partition offset not present in the access expression

In those cases, the allocation can still be Block/Stencil, but that acquire is
rewritten as full-range (offset=0, size=numBlocks).

Stencil exception:
- Stencil patterns are allowed to omit the partition offset from the access
  expression, because halo handling provides the missing neighbor rows without
  requiring full-range acquires.

---

## 4) Mode 1 - Coarse (Single DB)

Meaning: one datablock holds the entire array.

Theorem (Coarse Layout):
If the allocation is coarse, then all indices map directly to the inner memref;
the db_ref index is always 0.

Layout (2D example A[4][8]):

```
Single datablock (outerSizes=[1])

  +-------------------------+
  | [0,0]..[0,7]            |
  | [1,0]..[1,7]            |
  | [2,0]..[2,7]            |
  | [3,0]..[3,7]            |
  +-------------------------+
   db_ref[0]
```

Concrete example (2D, A[4][8]):

```
Global A[i,j] (single DB):

  j=0  1  2  3  4  5  6  7
i=0  [0,0][0,1] ...       [0,7]
i=1  [1,0][1,1] ...       [1,7]
i=2  [2,0][2,1] ...       [2,7]
i=3  [3,0][3,1] ...       [3,7]

All accesses go through db_ref[0], and local indices are the original indices.
```

Index mapping:
- db_ref indices = [0]
- local indices = original indices

Transformation (Coarse):

```
  A : memref<[D0 x D1 x ...] x T>

        +------------------------------+
        | DbAlloc (mode=coarse)        |
        | outerSizes = [1]             |
        | innerSizes = [D0, D1, ...]   |
        +------------------------------+
                     |
                     v
        +------------------------------+
        | DbAcquire                    |
        | offsets=[0], sizes=[1]       |
        +------------------------------+
                     |
                     v
        +------------------------------+
        | DbRef[0] -> view             |
        | view : memref<[D0 x D1 ...]> |
        +------------------------------+
                     |
                     v
        memref.load/store view[...original indices...]
```

---

## 5) Mode 2 - Fine-Grained (Element-wise)

Meaning: one datablock per element of the outer dimensions (outerRank > 0).

Theorem (Element-wise Layout):
If outerRank = r, then each unique outer index tuple selects a distinct
datablock; remaining indices address inside that block.

Layout (2D example A[4][8], outerRank=1, partition dim 0):

```
outerSizes=[4]  (one DB per row)

  +-------+ +-------+ +-------+ +-------+
  | Row 0 | | Row 1 | | Row 2 | | Row 3 |
  +-------+ +-------+ +-------+ +-------+
   db_ref[0] db_ref[1] db_ref[2] db_ref[3]
```

Concrete example (2D, A[4][8], outerRank=1 on rows):

```
DB[0] (row 0) holds: [0,0] [0,1] ... [0,7]
DB[1] (row 1) holds: [1,0] [1,1] ... [1,7]
DB[2] (row 2) holds: [2,0] [2,1] ... [2,7]
DB[3] (row 3) holds: [3,0] [3,1] ... [3,7]

Access A[i,j]:
  db_ref index  = i
  local index   = j
```

Index mapping:
- db_ref index = outer indices (e.g., i)
- local indices = remaining indices (e.g., j)

Concrete example (2D, A[4][4], outerRank=2 on [i,j]):

```
outerSizes=[4,4]
innerSizes=[]  (each DB holds a single scalar)

DB grid (one DB per element):
  DB[0,0] holds A[0,0]   ...   DB[0,3] holds A[0,3]
  ...
  DB[3,0] holds A[3,0]   ...   DB[3,3] holds A[3,3]

Access A[i,j]:
  db_ref indices = [i, j]
  local indices  = []   (scalar)
```

Full-range acquire in fine-grained:
- allocation is still many DBs
- acquire offset = 0, size = numElements (all rows / tiles)
- used when access does not depend on the partition offset (or is indirect)

Transformation (Fine-Grained, outerRank=1 example):

```
  A : memref<[D0 x D1] x T>

        +-----------------------------------+
        | DbAlloc (mode=fine_grained)       |
        | partitionedDims = [0]             |
        | outerSizes = [D0]                 |
        | innerSizes = [D1]                 |
        +-----------------------------------+
                     |
                     v
        +-----------------------------------+
        | DbAcquire                          |
        | indices = [i]  (element coordinate)|
        +-----------------------------------+
                     |
                     v
        +-----------------------------------+
        | DbRef[i] -> view                  |
        | view : memref<[D1] x T> (row)     |
        +-----------------------------------+
                     |
                     v
        memref.load/store view[j]

  Full-range acquire variant (same allocation, different acquire):

        DbAcquire indices=[0] sizes=[D0]   (conceptual)
            |
            v
        DbRef[k] selects any row k (used for indirect / non-offset-tied access)
```

---

## 6) Mode 3 - Block (Blocked)

Meaning: each datablock contains a contiguous block along one or more
dimensions.

Theorem (Block Layout):
If a dimension d is partitioned with block size B:
- block index = floor(global_d / B)
- local index = global_d mod B
db_ref chooses the block (relative to startBlock), local indices address inside
that block.

Layout (2D example A[4][8], blockSize=2 on dim 0):

```
outerSizes=[2]  (two chunks)
innerSizes=[2,8] (each DB holds 2 rows)

  +---------------+  +---------------+
  | Block 0       |  | Block 1       |
  | rows 0-1      |  | rows 2-3      |
  +---------------+  +---------------+
   db_ref[0]         db_ref[1]
```

Concrete example (2D, A[6][8], blockSize=2 on rows):

```
outerSizes=[3] blocks, each inner block is [2 x 8]

DB[0] holds rows 0..1:
  [0,0]...[0,7]
  [1,0]...[1,7]

DB[1] holds rows 2..3:
  [2,0]...[2,7]
  [3,0]...[3,7]

DB[2] holds rows 4..5:
  [4,0]...[4,7]
  [5,0]...[5,7]

Access A[i,j]:
  blockIdx   = floor(i / 2)
  localRow   = i % 2
  db_ref     = DB[blockIdx]
  local      = [localRow, j]
```

Index mapping (partition dim 0):
- db_ref index = i / B
- local indices = [i % B, j]

Concrete example (2D tiling, A[4][4], partitionedDims=[0,1], blockSizes=[2,2]):

```
outerSizes=[2,2] (db_ref indices order: [iBlock, jBlock])
innerSizes=[2,2]

Outer grid of DB tiles:

           j=0..1       j=2..3
        +-----------+-----------+
i=0..1  | DB[0,0]   | DB[0,1]   |
        | 2x2 tile  | 2x2 tile  |
        +-----------+-----------+
i=2..3  | DB[1,0]   | DB[1,1]   |
        | 2x2 tile  | 2x2 tile  |
        +-----------+-----------+

Example access A[i,j]:
  iBlock=floor(i/2), localI=i%2
  jBlock=floor(j/2), localJ=j%2
  db_ref indices = [iBlock, jBlock]
  local indices  = [localI, localJ]
```

Full-range acquire in block mode:
- allocation has many blocks
- acquire offset = 0, size = numBlocks
- used when access does not depend on the per-task partition offset (or is indirect)

Transformation (Block, 1D-blocked-on-rows example):

```
  A : memref<[D0 x D1] x T>
  blockSize = B  (partition dim 0)

        +--------------------------------------+
        | DbAlloc (mode=block)                 |
        | partitionedDims = [0]                |
        | outerSizes = [ceil(D0 / B)]          |
        | innerSizes = [B, D1]                 |
        +--------------------------------------+
                       |
                       v
        +--------------------------------------+
        | DbAcquire (per worker task)          |
        | offsets = [startBlock]               |
        | sizes   = [1]                        |
        +--------------------------------------+
                       |
                       v
        +--------------------------------------+
        | Index localization                   |
        | blockIdx  = floor(i / B)             |
        | localBlk  = blockIdx - startBlock    |
        | localRow  = i % B                    |
        +--------------------------------------+
                       |
                       v
        +--------------------------------------+
        | DbRef[localBlk] -> view              |
        | view : memref<[B x D1] x T>          |
        +--------------------------------------+
                       |
                       v
        memref.load/store view[localRow, j]

  Full-range acquire variant (same allocation, different acquire):

        DbAcquire offsets=[0], sizes=[numBlocks]
            |
            v
        DbRef[blockIdx] allows access to any block (used for indirect patterns)
```

---

## 7) Mode 4 - Stencil (Block + Halo / ESD)

Meaning: Block layout plus halos for neighbor access, implemented using ESD
(Ephemeral Slice Dependencies).

### 7.1 What ESD Is (Runtime Semantics)

ESD is a runtime capability: an EDT can depend on a *byte slice* of a DB.
When the DB becomes ready, the runtime signals the EDT with a pointer to:

  dbData + byteOffset

and a payload size. On multi-node, the runtime sends only that byte slice.

This is the right primitive for distributed ghost-cell exchange: minimal halo
traffic while preserving blocked compute.

### 7.2 Stencil Layout (Conceptual View)

Stencil keeps the same *allocation layout* as Block (outerSizes/innerSizes).
What changes is the *dependency delivery* to the EDT:

- owned: the worker's chunk (normal block acquire)
- leftHalo: a partial slice from the previous chunk (ESD)
- rightHalo: a partial slice from the next chunk (ESD)

Transformation (Stencil / ESD, row-partitioned example):

```
  A : memref<[D0 x D1] x T>
  blockSize=B, haloLeft=HL, haloRight=HR, partition dim 0

        +--------------------------------------+
        | DbAlloc (mode=stencil)               |
        | (same physical layout as Block)      |
        | outerSizes = [ceil(D0 / B)]          |
        | innerSizes = [B, D1]                 |
        +--------------------------------------+
                       |
                       v
        +--------------------------------------------------+
        | Per worker EDT dependencies (3-buffer delivery)  |
        +--------------------------------------------------+
           |                       |                       |
           v                       v                       v
  +-------------------+   +-------------------+   +-------------------+
  | owned acquire     |   | left halo acquire |   | right halo acquire|
  | offsets=[k],      |   | offsets=[k-1],    |   | offsets=[k+1],    |
  | sizes=[1]         |   | sizes=[1]         |   | sizes=[1]         |
  | full block        |   | ESD slice of DB   |   | ESD slice of DB   |
  +-------------------+   +-------------------+   +-------------------+
           |                       |                       |
           v                       v                       v
        owned view              leftHalo view           rightHalo view

  Multi-node note:
    halo acquires use ESD byteOffset/size, so the runtime transfers ONLY halo
    bytes between nodes (ghost-cell exchange), not the full neighbor block.
```

Example (1D rows, blockSize=4, haloLeft=1, haloRight=1, worker owns rows 4-7):

```
Global rows:
  ... | 3 | 4 5 6 7 | 8 | ...
        ^   owned    ^
     leftHalo      rightHalo

Delivered buffers (conceptual):
  leftHalo[0]  -> global row 3   (slice of previous chunk)
  owned[0..3]  -> global rows 4..7
  rightHalo[0] -> global row 8   (slice of next chunk)
```

Concrete example (2D, A[10][8], blockSize=4, haloLeft=1, haloRight=1):

Assume worker k=1 owns rows 4..7 (baseOffset=4, blockSize=4).

```
Row mapping (for any column j):
  A[3,j]  -> leftHalo[0, j]
  A[4,j]  -> owned[0, j]
  A[5,j]  -> owned[1, j]
  A[6,j]  -> owned[2, j]
  A[7,j]  -> owned[3, j]
  A[8,j]  -> rightHalo[0, j]

Correctness rule:
  stores are allowed only to owned[r,j]; halos are read-only inputs.
```

Index mapping (baseOffset semantics, partition dim is rows):

Let:
- baseOffset = global start row for owned chunk
- blockSize = number of owned rows
- haloLeft / haloRight = halo widths (rows)

Then for an access at globalRow:

- if globalRow < baseOffset:
  - use leftHalo
  - haloRow = (globalRow - baseOffset) + haloLeft
- else if globalRow < baseOffset + blockSize:
  - use owned
  - localRow = globalRow - baseOffset
- else:
  - use rightHalo
  - haloRow = globalRow - (baseOffset + blockSize)

Correctness rule:
- stores must target owned only; halos are read-only.

### 7.3 Plan: "ESD Per Row" Lowering (Avoid Per-Element Branching)

Problem:
- A naive stencil indexer that selects (owned vs halo) per load generates
  branch/select overhead in the innermost loop and blocks vectorization.

Goal:
- Pay halo logic per *row* (surface) rather than per *element* (volume).

Strategy (row-partitioned stencils like Jacobi2D):

1) Identify the loop nest where the partitioned dimension is the outer loop
   (row loop), and the inner loops iterate other dims (e.g., columns).
2) Split the row loop into three regions:
   - left boundary rows: [0, haloLeft)
   - interior rows: [haloLeft, blockSize - haloRight)
   - right boundary rows: [blockSize - haloRight, blockSize)
3) For the interior region, rewrite all stencil loads to use owned only:
   - no halo selection, no bounds checks
   - enables vectorization and reduces control overhead
4) For boundary regions, use specialized code paths that load the missing
   neighbor rows from the halo buffers.

For halo=1 (common 5-point / 7-point stencils), this reduces to:

```
interior rows: r = 1 .. blockSize-2   (owned-only)
first row:     r = 0                 (uses leftHalo for r-1)
last row:      r = blockSize-1        (uses rightHalo for r+1)
```

Theorem (Per-row ESD Overhead):
If halo widths are O(1) and blockSize is large, then:
- compute overhead from halo selection is O(haloLeft + haloRight) per chunk
- the innermost loop is branch-free in the interior

Per-row compute structure (conceptual, local row r in [0..B-1]):

```
  +--------------------+--------------------------------------------+
  | Region             | Neighbor row sources                        |
  +--------------------+--------------------------------------------+
  | left boundary      | (r-1) from leftHalo,  r and (r+1) from owned |
  | interior           | (r-1), r, (r+1) all from owned               |
  | right boundary     | (r+1) from rightHalo, r and (r-1) from owned |
  +--------------------+--------------------------------------------+
```

This preserves multi-node benefits (minimal halo transfers) and restores
single-node performance by avoiding per-element halo selection.

### 7.4 Chunk Alignment at ForLowering (Stencil Safety)

For stencil loops, worker chunking must stay consistent with DB block
boundaries, but it must not expand the logical iteration domain.

Current policy:

1) Explicit compile-time block hints (`PartitioningHint blockSize > 1`)
   - If loop lower bound is a known constant and misaligned, ForLowering may
     align the chunking base downward.
   - The generated inner loop is then clamped back to the original
     `[lowerBound, upperBound)` domain, so no extra iterations execute.

2) Runtime DB block-size hints (`runtimeBlockSizeHint`)
   - Use the runtime block size for chunk granularity.
   - Do not realign `lowerBound` at runtime.
   - This avoids domain expansion and tail/halo mismatches in stencil kernels.

Why runtime lower-bound alignment is disabled:
- With internode stencil chunking, expanding `lowerBound` (for example `1 -> 0`)
  can create acquire/iteration mismatches near the upper edge on tail chunks.
- In Jacobi-style loops (`i-1`, `i`, `i+1`), the final interior row requires
  stable ownership of the `i+1` neighbor row.

Example A (compile-time block alignment, safe):

```
Loop domain:      i in [5, 17), step=1
Explicit block:   B = 4
Aligned base:     4

Chunking domain:  [4, 17)   (for partition math)
Execution domain: [5, 17)   (after loopLower/loopUpper clamp)
```

Example B (runtime block hint, no runtime lower align):

```
Loop domain:        i in [1, 255), step=1     (254 interior rows)
numNodes = 6
runtime block hint: B = ceil(256 / 6) = 43

Chunk starts (global):
  node0: [1, 44)
  node1: [44, 87)
  node2: [87, 130)
  node3: [130, 173)
  node4: [173, 216)
  node5: [216, 255)

No runtime realignment to 0 is applied.
```

### 7.5 Stencil Limitations (Current)

- Stencil ESD is restricted to leading partitioned dimensions.
- If partitionedDims is non-leading, stencil is downgraded to Block.

---

## 8) Non-Leading Partitioning (partitionedDims)

CARTS supports partitioning non-leading dimensions for Block and Fine-grained.
The selected dimensions are recorded in partitionedDims and used by the block
indexer for correct localization.

Theorem (Non-Leading Block Layout):
Let partitionedDims = [p0, p1, ...] and blockSizes = [B0, B1, ...]. Then:
- outerSizes[k] = ceil(dimSize[p_k] / B_k)
- innerSizes keeps original dimension order, but replaces each partitioned dim
  size with its block size
- db_ref indices correspond to partitionedDims order
- local indices keep original order, with partitioned dims localized by modulo

Example A[i][j][k], partition only k (partitionedDims=[2], B=4):

```
outerSizes = [ceil(K/4)]
innerSizes = [I, J, 4]

db_ref index = k / 4
local indices = [i, j, k % 4]
```

Concrete example (3D, A[2][3][8], partitionedDims=[2], blockSize=4):

```
outerSizes=[2] (k in blocks of 4)
innerSizes=[2,3,4]

DB[0] contains k=0..3 (localK=0..3):
  A[0,0,0..3]  A[0,1,0..3]  A[0,2,0..3]
  A[1,0,0..3]  A[1,1,0..3]  A[1,2,0..3]

DB[1] contains k=4..7 (localK=0..3):
  A[0,0,4..7]  A[0,1,4..7]  A[0,2,4..7]
  A[1,0,4..7]  A[1,1,4..7]  A[1,2,4..7]

Access A[i,j,k]:
  blockIdx = floor(k / 4)
  localK   = k % 4
  db_ref   = DB[blockIdx]
  local    = [i, j, localK]
```

Transformation (Non-leading Block, partition k example):

```
  A : memref<[I x J x K] x T>
  partitionedDims=[2], blockSize=B

        +------------------------------------------+
        | DbAlloc (mode=block)                     |
        | partitionedDims = [2]                    |
        | outerSizes = [ceil(K / B)]               |
        | innerSizes = [I, J, B]                   |
        +------------------------------------------+
                       |
                       v
        +------------------------------------------+
        | DbAcquire (per worker task)              |
        | offsets = [startBlock]                   |
        | sizes   = [1]                            |
        +------------------------------------------+
                       |
                       v
        +------------------------------------------+
        | Index localization                        |
        | blockIdx = floor(k / B)                   |
        | localBlk = blockIdx - startBlock          |
        | localK   = k % B                          |
        +------------------------------------------+
                       |
                       v
        +------------------------------------------+
        | DbRef[localBlk] -> view                  |
        | view : memref<[I x J x B] x T>           |
        +------------------------------------------+
                       |
                       v
        memref.load/store view[i, j, localK]
```

Example A[i][j][k], partition i and k (partitionedDims=[0,2]):

```
outerSizes = [ceil(I/Bi), ceil(K/Bk)]
innerSizes = [Bi, J, Bk]

db_ref indices = [i / Bi, k / Bk]
local indices = [i % Bi, j, k % Bk]
```

Concrete example (3D, A[4][2][6], partitionedDims=[0,2], Bi=2, Bk=3):

```
outerSizes = [ceil(4/2)=2, ceil(6/3)=2]  (db_ref indices order: [iBlock, kBlock])
innerSizes = [2, 2, 3]

Outer grid of DBs (by ranges in original coordinates):

              k=0..2       k=3..5
           +-----------+-----------+
i=0..1     | DB[0,0]   | DB[0,1]   |
           +-----------+-----------+
i=2..3     | DB[1,0]   | DB[1,1]   |
           +-----------+-----------+

Example access A[i,j,k]:
  iBlock = floor(i / 2), localI = i % 2
  kBlock = floor(k / 3), localK = k % 3
  db_ref indices = [iBlock, kBlock]
  local indices  = [localI, j, localK]
```

---

## 9) Coarse vs Full-Range Summary (Quick Table)

| Concept | What changes | When used |
|--------|--------------|-----------|
| Coarse layout | outerSizes = [1]; single DB | No safe/profitable partitioning |
| Full-range acquire | acquire spans all blocks | Access is indirect or not tied to partition offset |

---

## 10) Where the Meaning Is Implemented

- Analysis (patterns, bounds, partition dims): DbAcquireNode / DbAllocNode
- Decisions (H1.*): `evaluatePartitioningHeuristics` + H1.7 per-acquire voting
- Coordination + plan: DbPartitioning
- Layout rewrite: DbRewriter (Block / Fine-grained / Stencil)
- Index localization: DbBlockIndexer / DbElementWiseIndexer / DbStencilIndexer
- ESD runtime semantics: byte-slice deps (`artsRecordDepAt`) + pointer signaling

This keeps semantics explicit: heuristics decide the layout, rewriters apply it,
indexers localize indices, and the runtime implements the ESD transport.
