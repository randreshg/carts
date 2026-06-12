---
name: carts-vision
description: Use when a CARTS compiler/runtime task mentions the vision, SDE/ARTS/ARTS-RT spine, real transformations instead of metadata, value optimization across state/dependency/effect/compute/memory/sync, hypergraph planning, DB/CU grain, distributed DBs, GASNet/distributed scaling, or asks where a fix belongs.
---

# CARTS Vision

Use this skill before implementation decisions that affect compiler shape,
distributed execution, optimization placement, or benchmark scaling.

## Spine

- **SDE commits real layout/tiling facts.** SDE owns OpenMP semantics,
  HPF-style `DISTRIBUTE`/`ALIGN`, per-array block layouts from affine access
  relations, abstract communication-volume cost, and real source/SU/CU/MU
  loop/layout transformations. It names no collectives, DBs, EDTs, owner
  routes, GUIDs, or runtime policy.
- **ARTS commits codelet graph structure.** ARTS owns isolated codelets,
  explicit deps/params, storage views, and graph optimizations. It consumes
  SDE-authored layout, access-window, and movement structure; it does not redo
  SDE layout or choose movement families.
- **ARTS realizes DB/EDT ownership and grouped execution.** ARTS owns
  per-block single-writer DBs, owner routes, DB/EDT graphs, placement, DB modes,
  and grouped compute/bridge/communication CUs over committed SDE facts.
- **ARTS-RT lowers mechanically.** ARTS-RT lowers chosen ARTS objects to
  runtime ABI and LLVM-facing shape. It must not infer scheduling, ownership,
  partition, storage grain, or collective policy.

## Transformation Rule

A layer that has enough information to transform must perform the real
transformation in its own IR, or fail closed with evidence. Metadata-only
promises that rely on a later layer to reinterpret or repair semantics are
bugs. Later layers may verify, consume, realize, or reject committed upstream
facts; they must not silently recompute owner dims, block shapes, movement
families, owner routes, DB grain, or runtime modes.

## Value Optimization Lanes

Optimize state, dependency, effect, compute, memory, and sync by changing real
IR shape in the owning layer:

| Lane | Optimizes | Real transformations |
|---|---|---|
| State | Memory shape and value movement | memref normalization, pointer-of-pointer flattening, scalar forwarding, MU/block realization, access windows, layout assignment, owner dims, in-place safety classification |
| Dependency | Communication and legality | explicit deps/params, storage views that consume committed owner dims, SDE movement representation, halo/contraction/reduction realization, control-token boundaries |
| Effect | Observable semantics and legal ordering | effect summaries, in-place legality, source dependency preservation, and rejection of transforms that would change visible ordering |
| Compute | Work shape and locality | loop tiling/interchange/fusion when legal, owner-local compute grouping, contraction tiling, and bridge/communication CU grouping without changing DB grain |
| Memory | Storage grain and movement | per-block DB realization, compact read-only bridge slices, DB modes, owner routes, cache behavior, and block-range realization from committed facts |
| Sync | Runtime frontier and epoch pressure | barrier/epoch reduction, dependency fanout reduction, grouped bridge launch, and runtime synchronization cleanup using explicit ARTS verdicts |

Do not use an effect, memory, sync, or ARTS-RT pass to compensate for wrong
state facts or missing SDE movement structure.

## Grain Rule

DB/MU grain and CU/bridge grain are distinct:

- Too-coarse DBs serialize independent writers and hide remote ownership.
- One CU/bridge per tiny DB drowns the runtime in dependency and copy EDTs.
- The target shape is a two-level graph: MU/DB blocks fine enough for
  single-writer concurrency, plus grouped compute/bridge/communication CUs over
  block ranges for read-only or copy-like edges.

## Hypergraph Rule

Use the CU/MU hypergraph to investigate grouping and partition quality over
committed layout facts:

- CU vertices model owner-local work candidates over committed MUs. Bridge and
  communication CU grouping is realized later, after ARTS represents SDE
  movement structure and ARTS realizes DB/EDT ownership.
- MU hyperedges model memory blocks and dependency fanout, weighted by abstract
  communication volume, byte traffic, halo/collective cost, or remote fanout.
- The hypergraph can guide CU grouping, bridge coalescing, and partition
  quality over committed MU facts.
- It must not become a benchmark-name shortcut for owner dims, owner routes,
  block shape, or storage grain. Owner dims come from SDE affine layout facts;
  movement families come from SDE structure, and owner routes belong to ARTS
  realization.

Inspect the SDE planning shape directly:

```bash
dekk carts compile <input> -O3 --pipeline=sde-planning
```

## CGO'15 Stencil Mapping

For locality-aware concurrent-start stencils, keep the mapping strict:

- SDE proves Jacobi-style legality, performs the real skew/diamond or
  time-band loop transform, widens halo by `radius * bandDepth`, and commits
  layout/time-band facts.
- ARTS consumes the transformed SDE movement structure as graph edges.
- ARTS realizes the same fine per-block DB/MU grain with grouped
  compute/bridge/communication CUs over block ranges.
- ARTS-RT lowers mechanically; it must not infer time bands, stencil legality,
  owner routes, movement families, or DB grain from runtime shape.

Thread grouping maps to CU/bridge grouping evidence over committed MUs, not to
coarse DBs or deferred metadata markers.

## Timestep/Epoch Rule

Barrier and epoch optimization must be a real graph transformation:

- SDE removes or amortizes timestep barriers only by rewriting loop/dataflow
  shape, such as time-band/skew/diamond tiling with widened halo or explicit
  per-block/range tokens.
- ARTS consumes the transformed SDE movement structure as graph edges.
- ARTS groups compute/bridge/communication CUs over committed block ranges while
  each lane keeps its own per-block DB acquire.
- ARTS-RT/runtime lowers and tunes the emitted epoch/frontier/route calls; it
  must not infer missing dataflow or suppress waits.

Never coarsen `logicalWorkerSlice`, `physicalBlockShape`, or DB/MU grain from a
barrier/epoch optimization.

## Layer Placement Checklist

1. What is the first wrong committed fact?
2. Which layer owns that fact?
3. What real IR transformation makes the fact true?
4. What verifier or focused lit test proves downstream cannot repair it by
   accident?
5. Does the change preserve DB/MU grain separately from CU/bridge grain?
6. Does any heuristic trigger only on affine structure, typed attrs, graph
   facts, layout mismatch, or runtime topology, never a benchmark name?

## Validation

- Inspect IR shape first: `sde-planning`, `sde-to-arts`, `sde-to-arts`,
  `post-db-refinement`, and `pre-lowering` for distributed work.
- Add focused lit tests at the owning dialect boundary.
- Run `dekk carts format`, focused lit tests, `dekk carts build`, and the
  affected benchmark.
- For distributed DB work, validate real 64-thread RDMA runs in order:
  1 node, then 2 nodes with plain `-O3` (default-on distribution). Use
  megalarge only after compiler shape is sane.
  Confirm checksums, distributed `arts.db_alloc` block counts, owner routes,
  rank logs, and non-owner allocation behavior.
