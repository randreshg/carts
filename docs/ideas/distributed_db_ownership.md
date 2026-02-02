# Distributed DataBlock Ownership: Multi-Node Allocation in CARTS

## Executive Summary

CARTS currently tends to allocate ARTS DataBlocks (DBs) on a single node (often rank 0) even when the computation is distributed across nodes. This creates two related limitations:

1. **Memory capacity doesn’t scale with node count** for large arrays allocated outside parallel regions (everything is physically allocated on one node).
2. **Data locality is poor**: other nodes frequently perform remote reads/writes to node 0’s memory.

ARTS already supports a pattern where **GUIDs are pre-reserved with embedded owner-rank information**, and then **each rank allocates only the DBs it owns** (local allocation on multiple nodes). This document proposes how CARTS can adopt that pattern and how to identify cases where it is safe and beneficial.

---

## Research Findings (ARTS)

### 1) GUIDs embed the owner rank

ARTS GUIDs contain `{type, rank, key}`:

- `rank` is stored in the GUID itself (`uint16_t rank : 16`).
- The runtime uses `artsGuidGetRank(guid)` / `artsIsGuidLocal(guid)` to decide locality.

Code pointers:
- `external/arts/core/inc/arts/gas/Guid.h` (GUID bitfields)
- `external/arts/core/src/gas/Guid.c` (GUID creation + rank assignment)

### 2) `artsDbCreateWithGuid*` is local-only

The runtime only allocates a DB locally if the GUID is local:

- `artsDbCreateWithGuid(...)` checks `artsIsGuidLocal(guid)` and returns `NULL` on non-owner ranks.
- Same for `artsDbCreateWithGuidAndArtsId(...)` and `artsDbCreateWithGuidAndData(...)`.

Code pointer:
- `external/arts/core/src/runtime/memory/DbFunctions.c`

This is exactly what enables “same code on all ranks, but each rank allocates only what it owns”.

### 3) Deterministic “global” GUID reservation before parallel start

Before calling user `initPerNode(...)`, ARTS enables deterministic GUID generation:

- `setGlobalGuidOn()` happens before `initPerNode(...)`.
- This allows each rank to run the *same* GUID reservation loops and get identical GUID sequences, even in a distributed run.

Code pointer:
- `external/arts/core/src/runtime/Runtime.c` (`artsThreadZeroNodeStart`)

### 4) Concrete ARTS example: distributed allocation using pre-reserved GUIDs

`external/arts/test/testRouteTable.c` demonstrates the canonical pattern:

1. Every rank reserves one GUID per node:
   - `guids[i] = artsReserveGuidRoute(ARTS_DB_READ, i);`
2. During `initPerWorker`, each rank allocates only its local GUID:
   - `if (artsIsGuidLocal(guids[i])) artsDbCreateWithGuid(guids[i], ...)`

This yields DB allocation distributed across nodes without requiring remote allocation APIs.

---

## Current State (CARTS)

### What CARTS generates today

At the MLIR level, `arts.db_alloc` carries a single `route` operand. In CreateDbs, CARTS currently sets that route to `0` for allocations it introduces:

- `lib/arts/Passes/Transformations/CreateDbs.cpp` (currently uses `route = 0`)

In LLVM lowering:

- `lib/arts/Passes/Transformations/ConvertArtsToLLVM.cpp` lowers each DB element by calling:
  - `artsReserveGuidRoute(dbMode, route)`
  - then `artsDbCreateWithGuid*` (local allocation)

If `route` is always 0, all GUIDs embed rank 0, so all DBs allocate on node 0.

---

## Proposed Design

### Goal

Enable CARTS to **distribute DB ownership and physical allocation across nodes** when it is safe and profitable, by:

1. Assigning different owner ranks to different DB partitions/elements.
2. Ensuring allocation (and optional initialization) occurs on the owning rank.

### Core idea: “pre-reserve GUIDs everywhere; allocate only when local”

Use the existing ARTS mechanism:

- Reserve GUIDs with an intended owner rank.
- Run the same reservation code on all ranks (deterministic ordering).
- Allocate the DB only on the owner rank via `artsDbCreateWithGuid*`.

This matches ARTS’s tested pattern and avoids inventing new runtime mechanisms.

### Ownership policies (compiler choice)

| Policy | Route rule | When to use |
|--------|------------|-------------|
| **Single-node (status quo)** | `route = 0` | Shared read-mostly arrays, host-initialized data, or unknown patterns |
| **Round-robin** | `route = linearIndex % numNodes` | Simple memory spreading; good first prototype |
| **Owner-compute (preferred)** | `route = node executing first writer EDT` | Best locality; aligns data placement with distributed work |

### Identification: which allocations are candidates?

Start conservative; expand over time.

Good initial candidates:

- Allocations converted into DBs by CreateDbs that are **accessed inside `internode` tasks** and are **partitioned (blocked/fine)**.
- Access patterns that suggest **disjoint per-task/per-chunk writes**:
  - `arts.db_acquire` uses `partitioning(<blocked/...>)` or `partitioning(<fine_grained/...>)`
  - metadata frequently shows `accessedInParallelLoop=true` and `hasLoopCarriedDeps=false`

Avoid initially:

- DB pointers dereferenced in host code before any EDT runs (host needs a local pointer).
- Global memrefs with static initializers that CARTS currently copies after DB creation (needs distributed initialization).
- Truly shared-write patterns (reductions without proper partitioning, loop-carried deps, etc.).

---

## Lowering Strategy Options

### Option A (recommended first): generate `initPerNode` for distributed DB allocation

Generate an `initPerNode(nodeId, argc, argv)` wrapper that:

1. Replays the GUID reservation loops for candidate DBs on every rank (deterministic).
2. Allocates only the locally-owned DBs:
   - `if (artsGuidGetRank(guid) == nodeId) artsDbCreateWithGuid*(guid, size, ...)`

This mirrors `external/arts/test/testRouteTable.c` and naturally spreads physical allocation.

#### Option A: Detailed Plan (what we need to build)

**Why `initPerNode` is the right hook**

- In ARTS, `artsMain(...)` only runs on rank 0, but `initPerNode(...)` runs on *every* rank.
- ARTS enables deterministic GUID generation right before calling `initPerNode` (`setGlobalGuidOn()`), which is exactly what we need to “pre-reserve everywhere” safely.

**What the compiler must guarantee**

- The reservation loops in generated `initPerNode` must execute in the *same order* on all ranks.
  - No control-flow divergence based on `nodeId` except for the “allocate if local” check.
  - All loop bounds must be derived from identical inputs (typically `argc/argv` parsing).
- Any DB whose pointer is dereferenced in host code (rank 0) must *not* be moved to Option A until we also move its initialization into owner-routed EDTs (Option B).

**Eligibility rules for the first implementation**

Start with the most conservative set:

- The `DbAllocOp` is outside any `arts.edt` (i.e., “host” allocation).
- The allocation’s uses are only for:
  - creating dependencies (guid-based), and/or
  - passing DB handles into EDT creation,
  - but not dereferencing DB payload on rank 0 before EDTs execute.
- The allocation is partitioned (fine/blocked/stencil), and the distributed schedule is `internode` so memory spreading matters.

**What the generated program should look like (conceptual)**

For each candidate allocation, generate:

1. **Global storage** for the handle arrays (GUIDs and optionally pointers).
2. A module-level `initPerNode` function that:
   - allocates handle arrays,
   - reserves GUIDs for all elements/partitions (deterministic),
   - allocates DB payload only for locally-owned GUIDs,
   - stores local pointers for owned DBs and `NULL` otherwise.

Pseudo-code sketch:

```c
void initPerNode(unsigned nodeId, int argc, char **argv) {
  // parse sizes (must be identical on all ranks)
  int N = ...;
  long totalElems = (long)N * (long)N;

  A_guids = malloc(totalElems * sizeof(artsGuid_t));
  A_ptrs  = malloc(totalElems * sizeof(void*));

  unsigned numNodes = artsGetTotalNodes();
  for (long i = 0; i < totalElems; i++) {
    unsigned route = (unsigned)(i % numNodes);          // round-robin (Phase 0)
    artsGuid_t g = artsReserveGuidRoute(ARTS_DB_WRITE, route);
    A_guids[i] = g;
    if (route == nodeId) {
      A_ptrs[i] = artsDbCreateWithGuid(g, elemBytes);   // or WithArtsId(...)
    } else {
      A_ptrs[i] = NULL;
    }
  }
}
```

**Important note about `A_ptrs`**

- The pointer array is *not* expected to be meaningful on non-owner ranks.
- Correctness must rely on ARTS dependency delivery (`depv[i].ptr` inside EDTs), not on globally dereferencing `A_ptrs[i]` from host code.

**Where to implement in CARTS**

This is easiest to stage as an MLIR transformation before `ConvertArtsToLLVM`:

- Create a new pass (e.g., `distributed-db-ownership-init`) that:
  1. Finds eligible host `DbAllocOp`s.
  2. Materializes a `func.func @initPerNode(...)` with the reservation+allocation loops.
  3. Rewrites the original `DbAllocOp` sites to load the handle arrays from globals instead of creating them in `artsMain`.
  4. Leaves non-eligible allocations on the current path (single-node route).

This keeps `ConvertArtsToLLVM` mostly unchanged (it still lowers runtime calls), and makes the feature easier to reason about.

### Option B: “init EDTs” routed to owner ranks

If initialization must write data (not just allocate), emit initialization EDTs routed to the owning rank, and move the init loops into those EDTs. This avoids requiring a host pointer on rank 0.

### Option C: remote DB creation APIs (use sparingly)

ARTS has `artsDbCreateRemote(route, size, mode)` which creates DB metadata on a remote rank, but it does not directly give the initiating rank a usable local pointer for initialization. It can be useful for special cases (metadata-only, or when payload initialization is also remote).

---

## Implementation Plan (Incremental)

### Phase 0: Proof-of-concept (round-robin ownership, allocation-only)

**0.1 Add a feature flag + wiring**

- Add a compiler flag (e.g., `--distributed-db-ownership=init-per-node`) to gate the feature.
- Plumb it through:
  - `tools/run/carts-run.cpp` (pipeline option)
  - `tools/carts_cli.py` (CLI exposure)

**0.2 Identify and mark eligible allocations**

- Implement a conservative analysis that marks a `DbAllocOp` as “Option A eligible” when:
  - it is outside `arts.edt`, and
  - it is only used to feed dependency/EDT creation (no host payload deref), and
  - its users are `internode` (or it is used by `internode` tasks).
- Record a diagnostic decision per alloc (so we can explain why something wasn’t moved).

**0.3 Generate global handle storage**

- For each eligible allocation, create globals for:
  - GUID array storage (`i64[]` in lowered form),
  - pointer array storage (`ptr[]` in lowered form, optional but usually needed to preserve existing IR structure).
- Use a stable naming scheme (include `arts.id` / allocationId) for debugging.

**0.4 Synthesize `initPerNode`**

- Create `func.func @initPerNode(%nodeId, %argc, %argv)` that:
  - computes dynamic sizes exactly once (from argv or from already-materialized size SSA),
  - allocates the handle arrays (via `malloc` or `artsMalloc`, consistent with current runtime choices),
  - runs a deterministic nested loop over all elements/partitions:
    - computes `route = linearIndex % arts.get_total_nodes` (round-robin),
    - calls `artsReserveGuidRoute(dbMode, route)`,
    - stores GUID into the global GUID array,
    - if `route == nodeId`, calls `artsDbCreateWithGuid*` and stores pointer; else store `NULL`.

**0.5 Rewrite original `DbAllocOp` sites**

- Replace the original `DbAllocOp` results in host code with loads of the global handle arrays.
- Remove the original reservation+allocation loops from `artsMain` for those allocations.
- Keep non-eligible `DbAllocOp`s unchanged (still route 0).

**0.6 Validate (2 nodes minimum)**

- Add one focused end-to-end check:
  - Run a small example where the DB is large enough to observe per-rank memory changes.
  - Confirm correctness remains unchanged.
- Add a debug/trace mode that prints:
  - a few `(linearIndex, guid, guidRank)` tuples per allocation to prove rank distribution.

### Phase 1: Initialization support

- Identify and move initialization code into owner-routed EDTs (Option B) when host code currently initializes DB contents.
- Keep fallbacks to single-node route for cases CARTS cannot safely rewrite.

### Phase 2: Owner-compute ownership policy

- Use ForLowering’s internode routing strategy to align DB ownership with the executing node of the first writer chunk/task.
- Prefer placing write-most DBs with the writer EDT; consider replicating read-only DBs.

### Phase 3: Heuristics + broader coverage

- Expand identification using CreateDbs/DbPartitioning metadata and dependence analysis.
- Add diagnostics (per-alloc “ownership decision” reports) to confirm why a given allocation was/wasn’t distributed.

---

## Risks & Open Questions

- **Deterministic GUID reservation ordering**: all ranks must execute reservations in the same order before parallel start; conditional control flow in generated `initPerNode` must be identical across ranks.
- **Host pointer expectations**: any code path that expects a usable pointer on rank 0 must be rewritten (or forced to single-node ownership).
- **Global initializers**: distributing global constants requires per-owner copying or replication.
- **Interaction with partial deps**: correctness depends on disjointness assumptions; start conservative.

---

## Key Code References

- ARTS GUID encoding + locality: `external/arts/core/inc/arts/gas/Guid.h`, `external/arts/core/src/gas/Guid.c`
- Deterministic GUIDs before `initPerNode`: `external/arts/core/src/runtime/Runtime.c`
- Local-only DB allocation: `external/arts/core/src/runtime/memory/DbFunctions.c`
- ARTS distributed allocation example: `external/arts/test/testRouteTable.c`
- CARTS route defaulting: `lib/arts/Passes/Transformations/CreateDbs.cpp`, `lib/arts/Passes/Transformations/ConvertArtsToLLVM.cpp`
