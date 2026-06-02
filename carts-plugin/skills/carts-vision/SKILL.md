---
name: carts-vision
description: Use when a CARTS compiler/runtime task mentions the vision, SDE/CODIR/ARTS/ARTS-RT spine, real transformations instead of metadata, state/dependency/effect value optimization, hypergraph planning, DB/CU grain, distributed DBs, RDMA scaling, or asks where a fix belongs.
---

# CARTS Vision

Use this skill before implementation decisions that affect compiler shape,
distributed execution, optimization placement, or benchmark scaling.

## Spine

- **SDE commits real layout/tiling facts.** SDE owns OpenMP semantics,
  HPF-style `DISTRIBUTE`/`ALIGN`, per-array block layouts from affine access
  relations, abstract communication-volume cost, and real source/SU/CU/MU
  loop/layout transformations. It names no collectives, DBs, EDTs, routes,
  GUIDs, or runtime policy.
- **CODIR commits collectives/bridges.** CODIR owns isolated codelets, explicit
  deps/params, first-class MPI distribution patterns
  (`all_gather`, `all_to_all`, `reduce_scatter`, `allreduce`, `broadcast`,
  `halo`), storage-view transitions, contraction tiling, redistribution, and
  halo bridge structure. It consumes SDE facts; it does not redo SDE layout.
- **ARTS realizes DB/EDT ownership and grouped execution.** ARTS owns
  per-block single-writer DBs, owner maps, DB/EDT graphs, placement,
  distributed ownership, DB modes, and grouped compute/bridge/communication
  CUs over committed SDE/CODIR facts.
- **ARTS-RT lowers mechanically.** ARTS-RT lowers chosen ARTS objects to
  runtime ABI and LLVM-facing shape. It must not infer scheduling, ownership,
  partition, storage grain, or collective policy.

## Transformation Rule

A layer that has enough information to transform must perform the real
transformation in its own IR, or fail closed with evidence. Metadata-only
promises that rely on a later layer to reinterpret or repair semantics are
bugs. Later layers may verify, consume, realize, or reject committed upstream
facts; they must not silently recompute owner dims, block shapes, collective
families, owner maps, DB grain, or runtime modes.

## Value Optimization Lanes

Use the SDE state/dependency/effect split to decide where value is created:

| Lane | Optimizes | Real transformations |
|---|---|---|
| State | Memory shape and value movement | memref normalization, pointer-of-pointer flattening, scalar forwarding, MU/block materialization, access windows, layout assignment, owner dims, in-place safety classification |
| Dependency | Communication and legality | explicit deps/params, storage views that consume committed owner dims, collective/bridge selection, halo/contraction/reduction materialization, control-token boundaries |
| Effect | Compute and sync | scheduling plans, distribution plans, loop tiling, barrier elimination, CU grouping, bridge grouping, epoch/sync reduction |

Do not use an effect pass to compensate for wrong state facts, or an ARTS pass
to compensate for missing CODIR collective intent.

## Grain Rule

DB/MU grain and CU/bridge grain are distinct:

- Too-coarse DBs serialize independent writers and hide remote ownership.
- One CU/bridge per tiny DB drowns the runtime in dependency and copy EDTs.
- The target shape is a two-level graph: MU/DB blocks fine enough for
  single-writer concurrency, plus grouped compute/bridge/communication CUs over
  block ranges for read-only or copy-like edges.

## Hypergraph Rule

Use the CU/MU hypergraph to investigate grouping and partition quality:

- CU vertices model compute/bridge/communication work.
- MU hyperedges model memory blocks and dependency fanout, weighted by abstract
  communication volume, byte traffic, halo/collective cost, or remote fanout.
- The hypergraph can guide CU grouping, bridge coalescing, and partition
  quality over committed MU facts.
- It must not become a benchmark-name shortcut for owner dims, block shape, or
  storage grain. Owner dims come from SDE affine layout facts; collective
  families come from CODIR layout mismatch plus compute pattern.

Useful diagnostic hook:

```bash
CARTS_DIAG_HYPERGRAPH=1 dekk carts compile <input> -O3 --pipeline=sde-planning
```

## Layer Placement Checklist

1. What is the first wrong committed fact?
2. Which layer owns that fact?
3. What real IR transformation makes the fact true?
4. What verifier or focused lit test proves downstream cannot repair it by
   accident?
5. Does the change preserve DB/MU grain separately from CU/bridge grain?
6. Does any heuristic trigger only on affine structure, typed attrs, graph
   facts, contracts, layout mismatch, or runtime topology, never a benchmark
   name?

## Validation

- Inspect IR shape first: `sde-planning`, `sde-to-codir`, `codir-to-arts`,
  `post-db-refinement`, and `pre-lowering` for distributed work.
- Add focused lit tests at the owning dialect boundary.
- Run `dekk carts format`, focused lit tests, `dekk carts build`, and the
  affected benchmark.
- For distributed DB work, validate real 64-thread RDMA runs in order:
  1 node, 2 nodes without `--distributed-db`, then 2 nodes with
  `--distributed-db`. Use megalarge only after compiler shape is sane.
  Confirm checksums, distributed `arts.db_alloc` block counts, owner maps,
  rank logs, and non-owner allocation behavior.
