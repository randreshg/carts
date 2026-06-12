# Partitioning

This guide describes the current DB partitioning shape after the ARTS
analysis/graph cleanup.

Partitioning has two independent concepts:

- Allocation layout: how many DB entries exist and what payload shape each DB
  entry has.
- Acquire range: which DB entries one `arts.db_acquire` requests.

`coarse` is an allocation layout: one DB entry backs the whole logical object.
A full-range acquire is different: it can request all entries of a block,
stencil, or fine-grained allocation without collapsing the allocation itself.

## Ownership

Distributed DB ownership is not selected by a DB graph or cached analysis layer.
SDE and ARTS author the layout and movement facts while structured information
is still available. ARTS then consumes those committed facts and either realizes
distributed ownership mechanically or rejects the allocation with a precise
reason.

The owner-route pass uses direct IR queries over `arts.db_alloc`,
`arts.db_acquire`, and their consuming `arts.edt` operations. It does not build
cached graph nodes, analysis managers, or heuristic decision caches.

## Pipeline

The relevant stages are:

- `sde-planning`: chooses legal source-level distribution, grain, movement, and
  loop shape.
- `sde-to-arts`: mechanically turns SDE MU/CU/SU structure into explicit ARTS
  deps, params, token-local views, and DB/acquire/EDT objects.
- `edt-dep-realization`: realizes ARTS EDT dependency distribution facts and
  checks the SDE/ARTS boundary.
- `create-dbs`: consumes ARTS storage facts and must reject tiled/block raw
  accesses that SDE did not rewrite.
- `db-opt`: tightens DB modes from actual uses.
- `post-db-refinement`: realizes distributed ownership, consolidates committed
  stencil halo windows, removes cleanup-only DB chains, and realizes ARTS-level
  reduction/matmul continuation structure from ARTS-authored facts.

## Current Utilities

Use narrow utilities instead of cached analysis graphs:

- `DbUtils`: DB tracing, sizes, access modes, cleanup-only chains, and DB-space
  slice conversion.
- `EdtUtils`: EDT dependency/block-argument queries.
- `StringUtils`: direct string-memref discovery.
- `DbDistributedEligibility`: direct distributed ownership eligibility query used by
  `DbDistributedOwnershipRealization`.

If a pass has enough information to transform, it should transform in its owning
layer. If it does not, it should fail closed rather than leave metadata for a
later layer to repair.
