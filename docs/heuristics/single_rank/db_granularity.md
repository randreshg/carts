# Single-rank heuristics: DB granularity

This note is scoped to **single-rank / single-node** runs (i.e.,
`artsGlobalRankCount == 1`). It explains runtime mechanics that drive EDT
concurrency and turns them into practical guidance for DB granularity.

Primary audiences:
- CARTS/ARTS compiler-pass authors (MLIR passes)
- ARTS runtime authors (DB memory model / CDAG)
- performance engineers tuning single-node runs

## Key costs and where they come from

### Cost A: per-EDT acquire/release scales with number of dependencies

At runtime, each EDT acquires its dependencies by iterating its `depv` array:
- `external/arts/core/src/runtime/memory/DbFunctions.c`: `acquireDbs(struct artsEdt *edt)` is a `for (i < depc)` loop.

So if we make DBs *too fine-grained*, we often increase:
- number of `db_acquire` ops per EDT
- `depc` per EDT
- total `acquireDbs()` work per EDT

### Cost B: per-DB metadata/event overhead scales with number of DB objects

Each DB carries persistent tracking structures:
- `external/arts/core/src/runtime/memory/DbFunctions.c`: DB creation initializes `dbList` (`artsNewDbList()`).
- Under `USE_SMART_DB`, each DB also gets a **persistent event** (`artsPersistentEventCreate(...)`).

So if we split an array into many DBs, we also pay for many DB objects' metadata.

## ARTS DB frontiers: the mechanism behind serialization

Each DB maintains a `dbList` (a linked list) of **frontiers**:
- `external/arts/core/src/runtime/memory/DbList.c`: `artsDbList` starts with a
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

## Practical heuristics (single rank)

1. **Prefer block partitioning when loops carry a clear partition offset.**
   This keeps DB count moderate while still allowing concurrent writes.

2. **Avoid full-range acquires in hot loops.**
   Full-range acquires negate the benefit of partitioning by forcing every EDT
   to touch all DBs.

3. **Keep DB count proportional to parallelism.**
   If the DB count is far larger than the number of workers, overhead dominates.

4. **Coarse can be correct but often serializes writes.**
   Use coarse only when partitioning is unsafe or when EDTs are read-only.

5. **Stencils benefit from block or stencil partitioning.**
   Use ESD/halo handling to preserve locality without full-range accesses.

For partitioning mechanics and mode definitions, see
`docs/heuristics/partitioning/partitioning.md`.
