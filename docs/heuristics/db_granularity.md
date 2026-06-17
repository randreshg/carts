# Single-rank heuristics: DB granularity

This note explains runtime mechanics that drive EDT concurrency and turns them
into practical guidance for DB/MU granularity. The rules apply to single-node
and distributed runs unless a section says otherwise.

Primary audiences:
- CARTS/ARTS compiler-pass authors (MLIR passes)
- ARTS runtime authors (DB memory model / CDAG)
- performance engineers tuning single-node runs

## Key costs and where they come from

### Cost A: per-EDT acquire/release scales with number of dependencies

At runtime, each EDT acquires its dependencies by iterating its `depv` array:
- `external/arts/libs/src/core/memory/db.c`: dependency handling iterates per-EDT DB inputs.

So if we make DBs *too fine-grained*, we often increase:
- number of `db_acquire` ops per EDT
- `depc` per EDT
- total `acquireDbs()` work per EDT

### Cost B: per-DB metadata/event overhead scales with number of DB objects

Each DB carries persistent tracking structures:
- `external/arts/libs/src/core/memory/db.c`: DB creation initializes `db_list` (`arts_new_db_list()`).
- Under `USE_SMART_DB`, each DB also gets a **persistent event** (`artsPersistentEventCreate(...)`).

So if we split an array into many DBs, we also pay for many DB objects' metadata.

## ARTS DB frontiers: the mechanism behind serialization

Each DB maintains a `dbList` (a linked list) of **frontiers**:
- `external/arts/libs/src/core/memory/frontier.c`: DB frontier structures start with a
  single frontier and grows as needed.

A **frontier** is a phase that groups compatible consumers of a DB version:
- multiple READs can share a frontier
- at most one WRITE can join a frontier
- an exclusive acquisition blocks reads and writes

If a request cannot enter the current frontier, ARTS moves to the next frontier
(and allocates one if needed). Requests in later frontiers are logically queued
behind earlier frontiers. Frontier advancement happens via
`artsProgressFrontier(...)`, which signals the next frontier's waiters.

## What fine-grained DBs buy you: avoiding *write* serialization

The serialization question is about **multiple EDTs that want to write the same
DB GUID** (or the same coarse-grained representation of a larger structure).

Key distinction:
- **Multiple WRITE DBs can be acquired concurrently** *if they are different
  DB GUIDs*.

So the core goal of DB partitioning is to turn a single large write target into
multiple disjoint write targets that map to **distinct DBs**.

## Practical heuristics

1. **Prefer block partitioning when loops carry a clear partition offset.**
   This keeps DB count moderate while still allowing concurrent writes.

2. **Avoid full-range acquires in hot loops.**
   Full-range acquires negate the benefit of partitioning by forcing every EDT
   to touch all DBs.

3. **Do not cap DB/MU grain to the worker count.**
   SDE must expose every legal independent MU/CU block. ARTS cannot recover
   parallel writer waves from one coarse DB, and worker-count caps leave legal
   work unused on larger machines. If overhead is high, group compute/bridge/
   communication CUs over preserved per-block DBs instead of coarsening the DBs.

4. **Coarse can be correct but often serializes writes.**
   Use coarse only when partitioning is unsafe or when EDTs are read-only.

5. **Stencils benefit from block or stencil partitioning.**
   Use halo-view handling to preserve locality without full-range accesses.

6. **Keep DB grain separate from CU/bridge grain.**
   Per-block single-writer DBs are the concurrency substrate. CU, bridge, and
   communication grouping is an ARTS graph optimization over those DBs, not a
   reason to merge independent DBs or hide committed SDE block structure.

For partitioning mechanics and mode definitions, see
`docs/heuristics/partitioning.md`.
