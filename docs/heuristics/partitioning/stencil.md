# CARTS Stencil Partitioning Analysis: The Jacobi Case Study

## 1. Problem Statement

The Jacobi iterative solver presents a challenging compilation scenario: a single array (`u`) is accessed with **two fundamentally different patterns** within the same iteration:

```c
// jacobi-for.c - The problematic code structure
for (it = 0; it < maxit; it++) {

    // LOOP 1: Copy operation - UNIFORM access pattern
    #pragma omp parallel for private(j)
    for (i = 0; i < nx; i++) {
        for (j = 0; j < ny; j++) {
            u[i][j] = unew[i][j];  // Each worker writes ONLY its assigned rows
        }
    }

    // LOOP 2: Stencil computation - NEIGHBOR access pattern
    #pragma omp parallel for private(j)
    for (i = 0; i < nx; i++) {
        for (j = 0; j < ny; j++) {
            if (i == 0 || j == 0 || i == nx-1 || j == ny-1) {
                unew[i][j] = f[i][j];  // Boundary condition
            } else {
                // 5-point stencil: reads u[i-1], u[i], u[i+1]
                unew[i][j] = 0.25 * (u[i-1][j] + u[i][j+1] +
                                     u[i][j-1] + u[i+1][j] +
                                     f[i][j] * dx * dy);
            }
        }
    }
}
```

**The Challenge**: How should we partition array `u` when:
- Loop 1 needs simple block distribution (rows 0-24 to Worker 0, 25-49 to Worker 1, etc.)
- Loop 2 needs each worker to access its rows PLUS neighboring rows (Worker 1 needs rows 24-50)

---

## 2. The Opportunity

This pattern is extremely common in scientific computing:
- Stencil computations (heat diffusion, image processing, CFD)
- Iterative solvers (Jacobi, Gauss-Seidel, SOR)
- Any algorithm with a "prepare data" phase followed by a "compute with neighbors" phase

Solving this efficiently enables CARTS to handle a large class of real-world applications.

---

## 3. Four Partitioning Strategies

### 3.1 Element-Wise (Fine-Grained) Partitioning

**Concept**: Each row of the array becomes its own datablock.

```
Array u[100][50] with 4 workers:

ALLOCATION:
  arts.db.alloc sizes=[100] element_sizes=[50]
                      ↑ 100 datablocks, each containing 1 row (50 elements)

  DB[0]  = u[0][*]   (row 0)
  DB[1]  = u[1][*]   (row 1)
  ...
  DB[99] = u[99][*]  (row 99)

LOOP 1 (Copy) - Worker 1 owns rows 25-49:
  ┌─────────────────────────────────────────────────────────────────┐
  │ Worker 1 acquires: DB[25], DB[26], ..., DB[49]                  │
  │ Mode: WRITE (exclusive ownership)                               │
  │ Action: Write u[25..49][*] = unew[25..49][*]                    │
  └─────────────────────────────────────────────────────────────────┘

LOOP 2 (Stencil) - Worker 1 computes rows 25-49:
  ┌─────────────────────────────────────────────────────────────────┐
  │ Worker 1 acquires:                                              │
  │   - DB[24] (READ) ← from Worker 0 (left neighbor row)           │
  │   - DB[25..49] (READ) ← own rows                                │
  │   - DB[50] (READ) ← from Worker 2 (right neighbor row)          │
  │                                                                 │
  │ Network transfer: Only 2 rows (DB[24] and DB[50])               │
  │ Data fetched: 2 × 50 × 8 bytes = 800 bytes                      │
  └─────────────────────────────────────────────────────────────────┘
```

**Advantages**:
- Precise data acquisition (fetch exactly what you need)
- Works for ANY access pattern
- Minimal network transfer for stencils
- Simple mental model

**Disadvantages**:
- Many datablocks (100 DBs for 100 rows)
- More acquire/release operations
- Higher metadata overhead in ARTS runtime
- Many persistent events and dependency edges (more signaling traffic)
- Harder to amortize runtime overhead when kernels are small

---

### 3.2 Chunk-Wise (Coarse-Grained) Partitioning

**Concept**: Multiple rows are grouped into chunks; each chunk is one datablock.

```
Array u[100][50] with 4 workers, chunkSize=25:

ALLOCATION:
  arts.db.alloc sizes=[4] element_sizes=[25, 50]
                      ↑ 4 chunks, each containing 25 rows (1250 elements)

  Chunk[0] = u[0..24][*]   (rows 0-24,  Worker 0)
  Chunk[1] = u[25..49][*]  (rows 25-49, Worker 1)
  Chunk[2] = u[50..74][*]  (rows 50-74, Worker 2)
  Chunk[3] = u[75..99][*]  (rows 75-99, Worker 3)

LOOP 1 (Copy) - Worker 1:
  ┌─────────────────────────────────────────────────────────────────┐
  │ Worker 1 acquires: Chunk[1]                                     │
  │ Mode: WRITE (exclusive ownership)                               │
  │ Action: Write u[25..49][*] = unew[25..49][*]                    │
  │ Perfect! Worker owns exactly what it needs.                     │
  └─────────────────────────────────────────────────────────────────┘

LOOP 2 (Stencil) - Worker 1:
  ┌─────────────────────────────────────────────────────────────────┐
  │ Worker 1 needs: u[24] (from Chunk[0]) and u[50] (from Chunk[2]) │
  │                                                                 │
  │ PROBLEM: To get u[24], must acquire ENTIRE Chunk[0]!            │
  │                                                                 │
  │ Worker 1 acquires:                                              │
  │   - Chunk[0] (READ) ← 25 rows, but only need 1 row!             │
  │   - Chunk[1] (READ) ← own chunk                                 │
  │   - Chunk[2] (READ) ← 25 rows, but only need 1 row!             │
  │                                                                 │
  │ Network transfer: 50 rows fetched, only 2 rows needed!          │
  │ Data fetched: 50 × 50 × 8 bytes = 20,000 bytes                  │
  │ WASTE: 96% of transferred data is unnecessary!                  │
  └─────────────────────────────────────────────────────────────────┘
```

**Advantages**:
- Fewer datablocks (4 DBs vs 100 DBs)
- Fewer acquire/release operations
- Lower metadata overhead
- Perfect for uniform access patterns

**Disadvantages**:
- Wastes network bandwidth for stencil patterns
- Must fetch entire chunks even when needing single rows
- Requires complex "halo" mechanism to avoid waste (currently broken)

---

### 3.3 Hybrid: Chunked Ownership + Halo Strips (Explicit Halo Exchange)

**Concept**: Keep **chunked** datablocks for ownership and bulk writes (good for Loop 1),
but represent the **stencil boundary data** as **separate, small datablocks** (“halo strips”).

For a 1D row-stencil with `haloLeft=1`, `haloRight=1`, each chunk publishes:
- `halo_left(chunk k)`: first row of chunk `k` (row `k*chunkSize`)
- `halo_right(chunk k)`: last row of chunk `k` (row `(k+1)*chunkSize - 1`)

```
Array u[100][50], 4 workers, chunkSize=25, halo=1 row:

OWNERSHIP DBs (chunked):
  Chunk[0] = u[0..24][*]   (rows 0-24,  Worker 0)
  Chunk[1] = u[25..49][*]  (rows 25-49, Worker 1)
  Chunk[2] = u[50..74][*]  (rows 50-74, Worker 2)
  Chunk[3] = u[75..99][*]  (rows 75-99, Worker 3)

HALO STRIP DBs (small):
  HaloRight[0] = u[24][*]   (row 24)
  HaloLeft[1]  = u[25][*]   (row 25)
  HaloRight[1] = u[49][*]   (row 49)
  HaloLeft[2]  = u[50][*]   (row 50)
  HaloRight[2] = u[74][*]   (row 74)
  HaloLeft[3]  = u[75][*]   (row 75)

Loop 1 (Copy) - Worker 1:
  - Writes Chunk[1] (rows 25..49)
  - Also updates HaloLeft[1] (row 25) and HaloRight[1] (row 49)

Loop 2 (Stencil) - Worker 1 computes rows 25..49:
  - Reads Chunk[1] (own rows)
  - Reads HaloRight[0] (row 24) and HaloLeft[2] (row 50)

Network transfer for Worker 1 (per iteration):
  - Only 2 rows (row 24 and 50), like fine-grained
  - But far fewer DBs than “one DB per row” (only 2 halo DBs per chunk)
```

**Advantages**:
- Preserves chunked ownership for bulk updates (Loop 1)
- Transfers only halo boundary data for stencils (Loop 2), not whole neighbor chunks
- Avoids creating “N datablocks” when only “O(chunks)” boundaries are needed
- Aligns with classic domain decomposition practice (MPI ghost cells / halo exchange)

**Disadvantages**:
- Requires explicit halo materialization:
  - either a pack/unpack step (copy boundary rows into halo DBs), or
  - a compiler/runtime mechanism to expose sub-DB slices as first-class DBs
- Adds extra DBs and tasks (though far fewer than fully fine-grained)
- Generalizes to 2D/3D stencils with faces/edges/corners (more bookkeeping)

---

### 3.4 Ephemeral Slice Dependencies (ESD): Chunked DBs + Slice Gets + Interior/Boundary Split

**Concept**: Keep chunked ownership DBs (like 3.2), but avoid whole-chunk transfers by
fetching *only the needed halo bytes* as **ephemeral pointer dependencies**.

This leverages an existing ARTS capability that classic DB-acquire does not provide:
`artsGetFromDbAt(edtGuid, dbGuid, slot, offsetBytes, sizeBytes, rank)` fetches a byte-range
and satisfies an EDT slot with a pointer (`ARTS_PTR` mode), rather than satisfying it with a DB GUID.

The kernel is split into two EDTs per worker:
1. **Interior EDT**: reads only the local chunk; no halos needed
2. **Boundary EDT**: reads the local chunk plus two halo slices (left/right)

```
Array u[100][50], 4 workers, chunkSize=25, halo=1 row:

Ownership DBs:
  Chunk[0], Chunk[1], Chunk[2], Chunk[3]

Worker 1, Loop 2:
  - Interior EDT (depc=1): depends only on Chunk[1]
  - Boundary EDT (depc=3): depends on:
      slot0: Chunk[1] (DB-guid dep, acquired normally)
      slot1: left halo row (ptr dep)  = GetFromDbAt(Chunk[0], offset=row24)
      slot2: right halo row (ptr dep) = GetFromDbAt(Chunk[2], offset=row50)

At boundaries (worker 0 / worker last), slot1 or slot2 is still required:
  - satisfy with artsSignalEdtNull(...) or a dummy ptr
  - otherwise the EDT will never become runnable (fixed depc!)
```

**Why this is “new” relative to classic halo DBs**:
- No additional persistent halo datablocks/events are created.
- Halo data is delivered as ephemeral `ARTS_PTR` dependencies (byte slices), which avoids
  “DB explosion” and avoids whole-chunk transfers.

**Advantages**:
- Stencil transfer volume is optimal (halo bytes only), like fine-grained
- Number of DBs remains small (chunked ownership only)
- Natural overlap: interior compute can run while slice-get halos are in flight

**Disadvantages / requirements**:
- Requires a barrier/versioning point between producer and consumer phases (Jacobi has this)
- Requires codegen support to emit `GetFromDbAt` slice gets and to split EDTs
- Still increases task count (interior+boundary), and requires careful slot accounting

---

### 3.5 Enhanced Hybrid Partitioning (EHP): A Policy Layer Over Hybrid + ESD (Recommended Default)

**Concept**: EHP is not “a fifth layout”; it is a **compiler policy** that chooses the
cheapest correct way to provide stencil halos *without*:
- creating `O(Nrows)` datablocks (fine-grained), or
- fetching `O(chunkBytes)` just to read `O(haloBytes)` (coarse-grained).

EHP keeps **chunked ownership** for phases like Loop 1, and then chooses one of:
1) **Hybrid halo strips** (push-style halo exchange): publish boundary rows/faces as small halo DBs.
2) **ESD slice gets** (pull-style halo exchange): fetch halo byte ranges via `artsGetFromDbAt(...)`
   and satisfy EDT slots with `ARTS_PTR` (no persistent halo DBs).

Why this matters in CARTS specifically:
- ForLowering already knows each worker’s contiguous `i`-range. For Jacobi, that means the only
  *cross-worker* data is a **constant-size boundary** (halo), even though the loop touches many rows.
- Fine-grained “one DB per row” is therefore usually **over-decomposition**: it creates metadata and
  dependency edges for interior rows that will never be communicated across ranks.

EHP selection rule-of-thumb (Jacobi-like stencils):
- If **slice gets are available** and the halo is a **small, statically known set of byte ranges**,
  prefer **ESD** (keeps DB count minimal and avoids whole-chunk transfers).
- Otherwise, prefer **Hybrid halo strips** (uses standard DB dependencies and persistent-event signaling).
- Only fall back to **Chunked** (whole-chunk fetch) when neither halo strips nor slice gets can be generated.

EHP also makes the “halo-in-chunk” pitfall explicit:
- Allocating halo *space* inside a chunk does not create halo *data*. Without materialization
  (pack/unpack or slice-get), those halo reads are undefined.

## 4. Trade-off Summary

| Aspect | Element-Wise (Row DBs) | Chunk-Wise (Chunk DBs) | Hybrid (Chunks + Halo Strips) | ESD (Chunks + Slice Gets) |
|--------|-------------------------|------------------------|------------------------------|--------------------------|
| **# of Datablocks** | `O(Nrows)` | `O(Nchunks)` | `O(Nchunks + Nchunks*haloFaces)` | `O(Nchunks)` |
| **Deps per worker (typical)** | High (many row DB deps) | Low (`1..3` chunk deps) | Medium (`1 chunk + halos`) | Medium (`1 chunk + 2 ptr halos`, plus extra EDT) |
| **Stencil transfer volume** | Optimal (halo rows only) | Worst-case (entire neighbor chunks) | Optimal (halo rows only) | Optimal (halo bytes only) |
| **Bytes moved (1D row-stencil)** | `haloBytes = (haloL+haloR)*rowBytes` | `~2*chunkBytes` (worst-case) | `haloBytes + pack/unpack` | `haloBytes` (direct slice get) |
| **Uniform loop fit (Loop 1)** | Correct but extra overhead | Best fit | Best fit | Best fit |
| **Runtime metadata & signaling** | High (many events/edges) | Low | Medium | Medium (extra EDTs + ptr deps, but no halo DB events) |
| **Compiler complexity** | Low | Low | Medium | Medium/High (task split + byte offset computation) |
| **Correctness risk** | Low | Low (but wasteful) | Low (if halo publish is correct) | Medium (requires phase ordering / version discipline) |
| **Best when** | Small N, irregular access | No halo / very wide halo | Narrow halo, regular stencil | Narrow halo + clear phase barrier + want to avoid halo DBs |

**Key Insight**: The “right” partitioning is **not just correctness**; it’s also whether ARTS can execute it efficiently:
- Fine-grained is correct, but may overproduce DBs/events and dependency edges.
- Chunked is fast for uniform loops, but wastes bandwidth on narrow-halo stencils.
- Hybrid is the standard stencil strategy (halo exchange), and fits ARTS well if we make halos first-class.

### 4.1 Three-Way Comparison (Fine vs Chunked vs EHP Default)

This section compares the **decision-relevant** choices:
- **Fine-grained** (row DBs): “always correct, minimal bytes, maximal metadata”
- **Chunked** (chunk DBs): “minimal metadata, maximal bytes”
- **EHP** (recommended default): “minimal bytes with bounded metadata”

Assumptions (mock scenario):
- `nx=4096`, `ny=4096`, `elemBytes=8` (double)
- `ranks=16`, `chunkSize=256 rows`, `haloLeft=haloRight=1 row`
- `rowBytes = ny*elemBytes = 32 KiB`
- `chunkBytes = chunkSize*rowBytes = 8 MiB`
- `haloBytes (per worker per iter) = 2*rowBytes = 64 KiB`

What the mock values represent:
- **Bytes moved**: approximate cross-rank payload for one worker for Loop 2.
- **Large transfers**: “big payload” moves (not counting control packets).
- **Deps/slots**: number of EDT dependency slots that must be satisfied before running.

| Metric (per worker, per iteration) | Fine-Grained (Row DBs) | Chunked (Chunk DBs) | EHP Default (Hybrid or ESD) |
|---|---:|---:|---:|
| Cross-rank bytes moved (Loop 2) | ~`64 KiB` | ~`16 MiB` (2 neighbor chunks) | ~`64 KiB` |
| Large transfers | 2 small halo rows | 2 large chunk pulls | 2 small halo rows/slices |
| DBs total (global) | `4096` | `16` | `16` (+ halo DBs if Hybrid) |
| Deps/slots for a worker’s stencil work | `~258` | `3` | `3` (Hybrid) or `3` (ESD boundary), plus optional interior EDT |
| Concurrency (what gates runnable EDTs) | slot-satisfaction overhead | bandwidth stalls | bounded slots + minimal stalls |

Key takeaway:
- The *communication requirement* for Jacobi is `O(haloBytes)`, not `O(chunkBytes)` and not `O(Nrows)`.
  EHP aims to match that requirement while respecting ARTS’s fixed-slot execution model.

### 4.2 Visual Comparison (Data Movement)

Same scenario (`chunkSize=256`, `halo=1`), worker `k` computing its chunk interior:

```
FINE-GRAINED (Row DBs)
  needs: many local row DBs + 2 remote row DBs (precise, but many deps)

  Rank k-1            Rank k               Rank k+1
   Row[end-1]  ───▶   Worker k   ◀───  Row[start]
     (32KiB)              ▲              (32KiB)
                           │
                       +256 local row DB deps

CHUNKED (Chunk DBs)
  needs: 2 remote chunk DBs (wasteful pulls)

  Rank k-1            Rank k               Rank k+1
   Chunk[k-1]  ───▶   Worker k   ◀───  Chunk[k+1]
     (8MiB)               ▲              (8MiB)
                           │
                       Chunk[k] local

EHP DEFAULT (Hybrid halo strips OR ESD slice gets)
  needs: 2 remote halo rows/slices + local chunk (minimal bytes, bounded deps)

  Rank k-1            Rank k               Rank k+1
   HaloRight[k-1] ─▶  Worker k  ◀─ HaloLeft[k+1]
      (32KiB)            ▲          (32KiB)
                          │
                     Chunk[k] local
```

---

## 5. Current CARTS Pipeline

### Step 1: C Code to MLIR (Polygeist)

```
Input: jacobi-for.c with #pragma omp parallel for

Output: MLIR with omp.parallel + omp.wsloop operations
```

### Step 2: OpenMP to ARTS Conversion

```
Pass: ConvertOpenMPToArts

omp.parallel { omp.wsloop { ... } }
    ↓
arts.edt<parallel> { arts.for { ... } }
```

### Step 3: CreateDbs - Coarse-Grained Allocation

```
Pass: CreateDbs

Analysis:
  - Array u[100][50] accessed in parallel loops
  - Decision: Create COARSE-GRAINED datablock

Output:
  arts.db.alloc [inout, heap, db_main] sizes=[1] element_sizes=[100, 50]
                                              ↑ Single DB for entire array

Note: At this stage, no partitioning yet - just one big datablock.
```

### Step 4: ForLowering - Add Partition Hints

```
Pass: ForLowering (in setupConcurrency)

Analysis:
  - Loop iterates i = 0 to 100
  - 4 workers available
  - Block distribution: each worker gets 25 rows

Output: Adds offset_hints and size_hints to acquires
  Worker 0: offset_hints=[0],  size_hints=[25]
  Worker 1: offset_hints=[25], size_hints=[25]
  Worker 2: offset_hints=[50], size_hints=[25]
  Worker 3: offset_hints=[75], size_hints=[25]

Also creates EpochOp wrappers for synchronization:
  arts.epoch {
    scf.for %worker = 0 to 4 {
      arts.edt<task> { /* loop body for this worker */ }
    }
  }
```

### Step 5: DbPartitioning - Current (Broken) Approach

```
Pass: DbPartitioning (in setupConcurrencyOpt)

CURRENT BEHAVIOR (PROBLEMATIC):

1. Detects stencil pattern in Loop 2:
   - Access u[i-1], u[i], u[i+1]
   - haloLeft = 1, haloRight = 1

2. Extends acquire hints to include halo:
   Worker 1: offset_hints=[24], size_hints=[27]  // rows 24-50 instead of 25-49

3. Creates halo-extended chunk allocation:
   arts.db.alloc sizes=[4] element_sizes=[27, 50]
                                         ↑ 27 rows per chunk (25 + 2 halo)

4. THE BUG: Allocates halo SPACE but never COPIES halo DATA!
   ┌─────────────────────────────────────────────────────────────────┐
   │ Worker 1's chunk layout:                                       │
   │                                                                 │
   │   local[0]  → should contain u[24] (LEFT HALO)  ← UNINITIALIZED│
   │   local[1]  → contains u[25]                                   │
   │   local[2]  → contains u[26]                                   │
   │   ...                                                          │
   │   local[25] → contains u[49]                                   │
   │   local[26] → should contain u[50] (RIGHT HALO) ← UNINITIALIZED│
   │                                                                 │
   │   Stencil reads local[0] and local[26] → GARBAGE VALUES!       │
   └─────────────────────────────────────────────────────────────────┘
```

---

## 6. What ARTS Excels At (And Why It Matters Here)

ARTS is strongest when a program can be expressed as:
1. **Datablocks** that represent data “ownership units”
2. **EDTs** (tasks) that consume datablocks
3. **Persistent events** that represent “this datablock is ready”

### ARTS EDT Execution Model (Fixed `depc`, Slot Satisfaction, and DB Acquire)

Two runtime constraints shape what the compiler can and cannot do:
1. **The number of dependencies is fixed at EDT creation time** (`depc`).
2. **An EDT cannot run until all `depc` slots are satisfied**.

ARTS implements this with a counter (`depcNeeded`) and a per-slot `depv[]` array:

```
EDT creation:
  edtGuid = artsEdtCreate(func, route, paramc, paramv, depc=N)
  edt->depcNeeded = N
  depv[0..N-1] exists (slots)

Satisfying a slot:
  internalSignalEdtWithMode(edtGuid, slot=i, dataGuid, mode, acquireMode, twinDiff)
    depv[i] = { guid=dataGuid, mode=..., acquireMode=..., ptr=... }
    edt->depcNeeded--
    if (depcNeeded==0) artsHandleReadyEdt(edt)

Becoming runnable (two-step):
  artsHandleReadyEdt(edt):
    acquireDbs(edt)             // fetch DB contents if a slot contains a DB guid
    ... when all DB pointers are ready ...
    enqueue EDT for execution
```

The “two-step” is important:
- Slot satisfaction makes an EDT **ready**.
- DB acquisition makes its DB pointers **usable**.

This has two direct consequences for stencil strategies:
- If a strategy creates an EDT with `depc=N`, **every one of those N slots must be satisfied**,
  even on boundaries; otherwise the EDT will never run.
- If you want a dependency that is **already a byte buffer** (not a DB),
  you can satisfy a slot with `ARTS_PTR` (e.g., via `artsSignalEdtPtr(...)` or `artsGetFromDbAt(...)`),
  and it will bypass DB acquisition for that slot.

Practical boundary rule:
- Prefer “fixed slots + null satisfaction” over “variable depc”.
- CARTS already uses this pattern for stencils: it can satisfy unused boundary slots with
  `artsSignalEdtNull(...)` (avoids deadlock).

#### Execution Model Diagram (Putting It Together)

This diagram shows the full lifecycle from “EDT created” to “EDT runs”:

```
      (A) create EDT with fixed depc=N
          edtGuid = artsEdtCreate(..., depc=N)
          edt->depcNeeded = N
          depv[0..N-1] allocated
                     │
                     │ (B) record dependencies (compiler-emitted)
                     │     artsRecordDep(dbGuid, edtGuid, slot, acquireMode, twinDiff)
                     │       -> adds (eventGuid -> edtGuid/slot) edge
                     ▼
      (C) producers update DBs and satisfy/decrement latches
          -> DB persistent event fires when latchCount reaches 0
                     │
                     │ (D) event firing signals slots
                     │     internalSignalEdtWithMode(edtGuid, slot, dbGuid, ...)
                     │       depv[slot].guid = dbGuid
                     │       edt->depcNeeded--
                     ▼
      (E) when edt->depcNeeded == 0
          artsHandleReadyEdt(edt)
            acquireDbs(edt)  // fetch remote DB contents if needed
            (when all required DB pointers are available)
            enqueue EDT
                     ▼
      (F) scheduler runs EDT
          prepDbs(...)
          func(paramc, paramv, depc, depv)
          releaseDbs(...)
```

Key takeaway: **slot satisfaction is the gate**. If `depc=N`, the runtime will never call
`artsHandleReadyEdt` unless it receives N signals (or null-signals) for slots `0..N-1`.

### Dependency Recording via Persistent Events

At runtime, each datablock has an associated **persistent event** (`eventGuid`).
When the compiler “records a dependency” from a datablock to an EDT slot, it is
effectively registering:

```
DB.eventGuid  ──(fires)──▶  EDT(slot i)
```

One way to visualize the cross-rank control flow is:

```
Rank(owner of DB)                                   Rank(running EDT)

  DB(eventGuid)
     │
     │ (compiler emits) artsRecordDep(dbGuid, edtGuid, slot, acquireMode, twinDiff)
     │
     ├─────────────── ARTS_REMOTE_DB_ADD_DEPENDENCE_MSG ───────────────▶
     │                     (if DB is remote / different rank)
     │
     │ (later, when DB is updated + latch reaches 0)
     │
     └─────────────── ARTS_REMOTE_EDT_SIGNAL_MSG (or local signal) ─────▶  EDT slot satisfied
                                                                       (EDT becomes runnable when all slots satisfied)
```

Conceptually:
- `artsRecordDep(dbGuid, edtGuid, slot, acquireMode, useTwinDiff)`
  - adds a dependence from the DB’s persistent event to the EDT slot
  - for WRITE-mode, also increments a latch so the event can “fire again” after updates

When ranks differ, ARTS sends a remote message to the datablock’s home rank:
- `ARTS_REMOTE_DB_ADD_DEPENDENCE_MSG` → `artsDbAddDependenceWithModeAndDiff(...)`
- later, when the event is satisfied/fires, the dependent EDT is signaled (possibly remotely)

Concrete call-path (runtime side, simplified):

```
compiler-emitted call:
  artsRecordDep(dbGuid, edtGuid, slot, acquireMode, useTwinDiff)
    external/arts/core/src/runtime/memory/DbFunctions.c
      -> artsDbAddDependenceWithModeAndDiff(dbGuid, edtGuid, slot, acquireMode, useTwinDiff)
           -> artsAddDependenceToPersistentEventWithModeAndDiff(db.eventGuid, edtGuid, slot, ...)
      -> (if acquireMode == WRITE) artsDbIncrementLatch(dbGuid)

remote case:
  artsRemoteDbAddDependenceWithHints(...) sends ARTS_REMOTE_DB_ADD_DEPENDENCE_MSG
    external/arts/core/src/runtime/network/RemoteFunctions.c
  receiver handles packet and executes artsDbAddDependenceWithModeAndDiff(...)
    external/arts/core/src/runtime/Server.c
```

**Why this shows up in partitioning**:
- Fine-grained partitioning increases the number of datablocks → increases persistent events → increases dependence registrations and signaling traffic.
- Chunked partitioning decreases this overhead, but increases bytes moved per dependency.

### How CARTS Generates “Record Dependencies” Code

At the MLIR level, CARTS builds a list of dependencies for an EDT and then lowers it to runtime calls:
1. `EdtLowering` inserts `arts.rec_dep` (record dependency edges) for the EDT’s dependencies.
2. `ConvertArtsToLLVM` lowers `arts.rec_dep` to runtime calls that register each slot dependency:
   - `artsRecordDep(...)` in the normal case
   - `artsSignalEdtNull(...)` for boundary/invalid cases (so the slot is still satisfied)

The key property is: **dependency slots exist because `depc` was fixed at `artsEdtCreate` time**.
Recording dependencies does not change `depc`; it only determines *which event will satisfy which slot*.

#### Record vs Signal vs Acquire (The Part That Matters for Stencils)

ARTS has a strict separation between:
- **recording** an edge (who should signal which slot later), and
- **signaling** a slot (actually satisfying the dependency), and
- **acquiring** DB contents (making DB pointers usable before running).

This separation is why stencil strategies must be designed around *fixed slots* and *explicit halo satisfaction*.

```
COMPILER / CODEGEN TIME (CARTS)
  (1) create EDT with fixed depc=N
        edtGuid = artsEdtCreate(..., depc=N)          [external/arts/.../compute/EdtFunctions.c]
        edt->depcNeeded = N                           // "slots remaining"

  (2) for each slot i:
        if boundsValid:
          artsRecordDep(dbGuid, edtGuid, slot=i, acquireMode, twinDiff)
            // registers: DB.persistentEvent -> EDT(slot i)
            // may send ARTS_REMOTE_DB_ADD_DEPENDENCE_MSG if dbGuid is remote
                                                   [external/arts/.../memory/DbFunctions.c]
        else:
          artsSignalEdtNull(edtGuid, slot=i)          // satisfy slot immediately
                                                   [external/arts/.../compute/EdtFunctions.c]

RUNTIME TIME (ARTS)
  (3) producer updates DB and decrements its latch
        -> persistent event reaches latchCount==0
        -> event fires its dependent list
        -> internalSignalEdtWithMode(edtGuid, slot=i, dbGuid, ...)
           (may send ARTS_REMOTE_EDT_SIGNAL_MSG if edtGuid is remote)
                                                   [external/arts/.../sync/EventFunctions.c]
                                                   [external/arts/.../compute/EdtFunctions.c]

  (4) slot satisfaction drives readiness
        internalSignalEdtWithMode:
          edt->depv[slot].guid = dbGuid / ptr / null
          edt->depcNeeded--                           // "slots remaining"
          if edt->depcNeeded == 0:
            artsHandleReadyEdt(edt)                   [external/arts/.../Runtime.c]

  (5) DB acquisition is a second gate before running
        artsHandleReadyEdt:
          acquireDbs(edt)                             [external/arts/.../memory/DbFunctions.c]
            edt->depcNeeded = edt->depc + 1           // reused as "DBs remaining + enqueue token"
            request/move/duplicate any DB deps whose depv[i].ptr == NULL
          if (edt->depcNeeded decrements to 0):
            enqueue EDT
            scheduler runs it
```

Stencil implications:
- `depc` must be known at EDT creation time, so stencils should prefer a **fixed slot template**
  (e.g., local chunk + left halo + right halo) and satisfy “unused” boundary slots with `artsSignalEdtNull`.
- `arts.db_acquire` is DB-granularity; “slice” semantics require either **halo DBs** (Hybrid) or **ptr deps**
  satisfied by `artsGetFromDbAt(...)` / `artsSignalEdtPtr(...)` (ESD).

### Implications for Stencil Partitioning

For stencils, ARTS does best when we can express “halo needs” as:
- a **small number of small dependencies**, rather than
- a **small number of huge datablocks**, or
- a **huge number of tiny datablocks**

That is exactly why “halo strips” are a natural fit for ARTS.

#### What ARTS Excels At (And What It Punishes)

ARTS tends to excel when:
- EDTs have **small, fixed `depc`** (few slots to satisfy before becoming runnable).
- Dependencies are expressed through **persistent events** (clean “publish then consume” phases).
- Boundary conditions are handled via **fixed slot templates + `artsSignalEdtNull`**, not variable `depc`.
- Communication can be expressed as **either small DBs** (halo strips) **or pointer deps** (slice gets).

ARTS tends to suffer when:
- we generate **many tiny DBs** (metadata + dependence registration + signaling scale up quickly), or
- we rely on **DB-granularity acquires** for narrow halos (bandwidth becomes volume-like, not surface-like).

---

## 7. Why This Problem is Interesting

### Two Loops with Different Access Patterns

This is the crux of the challenge:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ARRAY u ACCESS PATTERNS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  LOOP 1 (Copy): EMBARRASSINGLY PARALLEL                                    │
│  ─────────────────────────────────────                                      │
│    u[i][j] = unew[i][j]                                                     │
│                                                                             │
│    Worker 0: writes u[0..24]    (own chunk only)                            │
│    Worker 1: writes u[25..49]   (own chunk only)                            │
│    Worker 2: writes u[50..74]   (own chunk only)                            │
│    Worker 3: writes u[75..99]   (own chunk only)                            │
│                                                                             │
│    Pattern: UNIFORM - each worker touches only its assigned rows            │
│    Ideal partitioning: CHUNKED (minimal overhead)                           │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  LOOP 2 (Stencil): NEIGHBOR-DEPENDENT (but READ-ONLY on u!)                 │
│  ───────────────────────────────────────────────────────────                │
│    unew[i][j] = f(u[i-1][j], u[i][j], u[i+1][j], ...)                       │
│                                                                             │
│    Worker 0: reads u[-1..25]    (needs row 25 from Worker 1)                │
│    Worker 1: reads u[24..50]    (needs rows 24,50 from neighbors)           │
│    Worker 2: reads u[49..75]    (needs rows 49,75 from neighbors)           │
│    Worker 3: reads u[74..100]   (needs row 74 from Worker 2)                │
│                                                                             │
│    Pattern: STENCIL - each worker reads own rows + neighbor boundaries      │
│    Key insight: u is READ-ONLY in this loop! unew is written.               │
│    Ideal data movement: HALO-ONLY (EHP: halo strips or slice gets)          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### The Synchronization is Already There

The epoch mechanism in ForLowering ensures:
1. Loop 1 completes (all workers finish writing to u)
2. Epoch barrier
3. Loop 2 starts (workers can safely read u)

**The problem is NOT synchronization** - it's DATA ACCESS. Workers in Loop 2 need to acquire the correct data from neighbors.

### The Read-Only Nature of the Stencil

A crucial observation: Loop 2 only READS from `u` - it writes to `unew`.

```
Loop 2 access modes:
  u[i-1][j], u[i][j], u[i+1][j]  → READ (ArtsMode::in)
  unew[i][j]                     → WRITE (ArtsMode::out)
```

This means:
- No write conflicts on `u` during Loop 2
- Multiple workers CAN read the same rows of `u` simultaneously
- ARTS can cache/replicate row data across workers

### Why Fine-Grained Is Not Always the Best Answer

Fine-grained is a great **correctness and bandwidth** baseline, but it is not
always the most proper solution for ARTS execution efficiency:

1. **Too many datablocks**:
   - Each datablock carries runtime metadata (routing, versioning, events).
   - Each datablock update creates work for termination detection and scheduling.
2. **Too many dependencies to record**:
   - CARTS lowers dependencies to calls that attach EDT slots to DB persistent events.
   - Large numbers of DBs inflate dependence registration and remote traffic.
3. **Higher scheduling overhead per byte moved**:
   - A 1-row DB is “small”, but still has the full machinery of DB acquisition,
     dependence management, and (possibly) remote request/response.

**Crucially**: For Jacobi-like loops we already know the access structure from
ForLowering (regular chunk offsets and sizes). We can often avoid “N row DBs” by
creating **only the halo bytes we actually need** (as halo strips or slice gets).

---

## 8. Data Movement Diagrams (Per Iteration)

Assume row decomposition, 4 workers, chunkSize=25, halo=1 row.

### Three-Way Comparison (Mock Numbers + Concurrency Implications)

This section compares three approaches end-to-end from an **ARTS execution model**
perspective: how many slots must be satisfied before an EDT can run, and how many
bytes/messages move across ranks.

#### Mock problem configuration (illustrative)

Assume:
- `ranks = 4`
- `u[nx][ny]` with `nx = 4096`, `ny = 4096`
- element type: `f64` ⇒ `elemBytes = 8`
- `rowBytes = ny * elemBytes = 4096 * 8 = 32768 B = 32 KiB`
- `chunkRows = 256` ⇒ `chunkBytes = chunkRows * rowBytes = 256 * 32 KiB = 8 MiB`
- `haloWidth = 1 row` ⇒ per-worker halo volume ≈ `2 * rowBytes = 64 KiB` (interior workers)

#### Diagram key

```
solid arrow  = transfers whole chunk DB (DB-granularity)
dashed arrow = transfers slice/row only (byte-range)
```

#### A) Fine-grained (Row DBs)

Bandwidth is optimal, but readiness gating can be expensive because the EDT has many slots.

```
W0 ── - - row(DB) - - ─▶ W1 ◀─ - - row(DB) - - ── W2
      (row 24)                 (row 50)

W1 stencil EDT depc (typical): ~chunkRows + 2 = 258 slots
  - must satisfy all 258 slots before the EDT is runnable
Network (per iteration, interior worker): ~64 KiB (2 rows)
```

#### B) Chunked (Whole-chunk fetch)

Readiness gating is light (few slots), but bandwidth is worst-case because DB-acquire is DB-granularity.

```
W0 ───────── Chunk[0] (8 MiB) ─────────▶ W1 ◀──────── Chunk[2] (8 MiB) ───────── W2
             (need 1 row)                      (need 1 row)

W1 stencil EDT depc (typical): 3 slots (Chunk[0], Chunk[1], Chunk[2])
  - runnable quickly once 3 slots are satisfied
Network (per iteration, interior worker): ~16 MiB (2 neighbor chunks)
```

#### C) EHP default (Hybrid halo strips OR ESD slice gets)

EHP matches fine-grained bandwidth (`O(haloBytes)`), but avoids both extremes:
- it avoids `O(Nrows)` DB/dependence explosion (fine-grained), and
- it avoids `O(chunkBytes)` neighbor pulls (chunked).

EHP can take one of two equivalent “halo exchange” forms:
- **ESD path (pull)**: slice-get the halo bytes into `ARTS_PTR` deps (best when slice-get lowering exists).
- **Hybrid path (push)**: publish halo strips as small DBs (best when we want to stay purely DB-based).

```
Option 1 (ESD / slice gets):
  Phase 1: run interior EDT (no halos)
    W1 interior EDT depc: 1 slot (Chunk[1])

  Phase 2: boundary EDT waits on two slice-get pointer deps + local chunk
    W0 ── - - GetFromDbAt(slice row24, 32 KiB) - - ─▶ W1 ◀─ - - GetFromDbAt(slice row50, 32 KiB) - - ── W2

    W1 boundary EDT depc: 3 slots
      - slot0: Chunk[1] (DB guid)
      - slot1: left halo ptr (ARTS_PTR)
      - slot2: right halo ptr (ARTS_PTR)

  Network (per iteration, interior worker): ~64 KiB (2 slices)
  Concurrency: interior can execute while slice gets are in flight

Option 2 (Hybrid / halo strips):
  Single stencil EDT depends on: Chunk[1] + HaloRight[0] + HaloLeft[2]
    W0 ── - - HaloRight[0] (row24, 32 KiB) - - ─▶ W1 ◀─ - - HaloLeft[2] (row50, 32 KiB) - - ── W2

  Network (per iteration, interior worker): ~64 KiB (2 halo rows)
  Concurrency: no interior/boundary split, but requires halo publish/pack step between phases

Critical: depc is fixed
  - boundary slots must always be satisfied (use SignalEdtNull/dummy on global boundaries)
```

#### Mock comparison table (per *interior* worker, per iteration)

These are not measurements; they represent the quantities ARTS actually “pays for”.

| Metric | Fine-grained (Row DBs) | Chunked (Whole-chunk) | EHP default (Hybrid or ESD) |
|--------|-------------------------|-----------------------|------------------|
| **Bytes pulled from neighbors** | ~64 KiB | ~16 MiB | ~64 KiB |
| **Messages (neighbor pulls)** | ~2 | ~2 | ~2 |
| **ETDs per worker** | 1 | 1 | 1 (+ pack/publish) or 2 (interior + boundary) |
| **Slots that gate stencil start** | ~258 | 3 | 3 (Hybrid) or 1 then 3 (ESD) |
| **What gates progress** | “all rows arrived” | “2 big chunks arrived” | “2 halo rows arrived (plus optional interior overlap)” |
| **Primary risk** | dependence explosion | bandwidth explosion | implementation complexity (halo materialization or slice offsets) |

Note: EHP is exactly the “middle ground” between Chunked and Fine-grained:
- it keeps a small number of dependencies like Chunked, but
- it moves only halo bytes like Fine-grained.

### Element-Wise (Row DBs)

```
Worker 1 owns rows 25..49

Loop 2 needs halo rows: 24 and 50

W0 ── row24(DB[24]) ───────▶ W1
W2 ── row50(DB[50]) ───────▶ W1

Data moved: 2 rows total
Runtime work: many DBs, many dependencies (row DBs)
```

### Chunk-Wise (Chunk DBs)

```
Worker 1 owns Chunk[1] = rows 25..49
Loop 2 needs halo rows: 24 (in Chunk[0]) and 50 (in Chunk[2])

W0 ── Chunk[0] (rows 0..24) ─▶ W1   (needs only row24)
W2 ── Chunk[2] (rows 50..74) ─▶ W1  (needs only row50)

Data moved: 2 chunks = 50 rows
Runtime work: few DBs, few dependencies, but wasteful bandwidth
```

### Hybrid (Chunks + Halo Strips)

```
Each worker publishes boundary rows after Loop 1:
  HaloRight[0] = row24
  HaloLeft[2]  = row50

W0 ── HaloRight[0] (row24) ─▶ W1
W2 ── HaloLeft[2]  (row50) ─▶ W1

Worker 1 reads:
  - Chunk[1] for its interior rows
  - 2 halo strip DBs for neighbor boundaries

Data moved: 2 rows total
Runtime work: few chunk DBs + few halo strip DBs
```

### ESD (Chunks + Slice Gets + Interior/Boundary EDTs)

ESD changes the *kind* of dependencies:
- The local chunk is still a DB-guid dependency (normal ARTS acquire).
- The halos are pointer dependencies satisfied by slice gets (`artsGetFromDbAt`).

```
Worker 1, per iteration (simplified):

Create interior EDT: depc=1
  slot0 = Chunk[1] (DB-guid)  ── satisfied by signaling/recordDep ──▶ acquireDbs fetches if needed

Create boundary EDT: depc=3
  slot0 = Chunk[1] (DB-guid)
  slot1 = left halo row  (ARTS_PTR)
  slot2 = right halo row (ARTS_PTR)

Halo slice delivery:
  GetFromDbAt(boundaryEdt, Chunk[0], slot1, offsetBytes(row24), sizeBytes(rowBytes), rank=W0)
  GetFromDbAt(boundaryEdt, Chunk[2], slot2, offsetBytes(row50), sizeBytes(rowBytes), rank=W2)

Critical rule: depc is fixed
  - boundary EDT will not run until slot0, slot1, slot2 are all satisfied
  - on physical boundaries, satisfy missing halo slots with SignalEdtNull or dummy ptr
```

This provides the same “2 rows moved” bandwidth property as halo strips, but without creating extra halo DBs/events.

### A Full-Iteration “Timeline” View (Hybrid)

This shows how halo exchange fits between the two phases:

```
Iteration t:

  Loop 1 (Copy phase):        Halo publish:              Loop 2 (Stencil phase):

  Chunk[k] (WRITE)            HaloLeft[k]  (WRITE)       Chunk[k] (READ)
  Chunk[k] (WRITE)     +      HaloRight[k] (WRITE)  +    HaloRight[k-1] (READ)
                                                           HaloLeft[k+1]  (READ)
                     (then an epoch/barrier between phases)
```

---

## 9. Does Halo Exchange Make Sense Here?

Yes—this is essentially the Jacobi canonical optimization:
- The stencil uses only a **narrow boundary** from neighbors.
- The “prepare” loop overwrites `u` every iteration, so the halo must be refreshed each iteration anyway.

Halo exchange is attractive when:
- `haloWidth << chunkSize` (narrow stencil)
- the stencil reads are **read-only** (no write-sharing on `u` during the stencil loop)
- network costs dominate, so reducing bytes moved matters more than a few extra tasks/copies

Halo exchange is less attractive when:
- the halo is wide (halo width comparable to chunk size)
- access patterns are irregular or data-dependent (hard to precompute a compact halo)
- the cost to pack/unpack halos exceeds the savings (very small arrays / single node)

In ARTS terms, halo exchange “makes sense” when it converts:
- “depend on 2 large DBs” into
- “depend on 2 small DBs”, at the cost of a short halo-pack step.

This is especially relevant for CARTS today because `arts.db_acquire` is
fundamentally a **datablock-granularity** mechanism: if the neighbor data lives
inside a large chunk datablock, ARTS will move the whole chunk. Halo strips make
the “slice” first-class, which is exactly what stencil communication wants.

### Generalization: Why Halo Exchange Scales (2D/3D Intuition)

Halo exchange is a “surface area vs volume” optimization:
- Chunked DBs move **volume** (entire neighbor chunks).
- Halo exchange moves **surface** (faces/edges/corners needed by the stencil).

For a 2D tiling (blocks), the neighbor data is the tile perimeter:

```
Tile (Bx x By), halo radius r:

   ┌───────────────────────────┐
   │     top halo (r rows)     │
   ├── left ───────────── right┤
   │ halo │   interior   │ halo│   (each halo is width r)
   ├── left ───────────── right┤
   │   bottom halo (r rows)    │
   └───────────────────────────┘

Bytes per tile per iteration (roughly):
  haloBytes2D ≈ elemBytes * r * (2*Bx + 2*By)   (+ corners)
```

In 3D, the halo is mostly the six faces of a brick, and the “surface/volume”
gap grows even larger; that’s why explicit halos are the standard approach.

---

## 10. Related Techniques and How They Map to CARTS/ARTS

This problem has decades of established practice; CARTS can reuse these ideas:

1. **MPI ghost cells / halo exchange**
   - Classic: `sendrecv` boundary faces each iteration
   - ARTS mapping: halo strip DBs + EDTs that pack boundary rows and satisfy events

2. **Owner-computes rule (domain decomposition)**
   - Each chunk updates its interior; boundaries read neighbor halos
   - ARTS mapping: chunk DB ownership; halo DBs are read-only inputs to stencil EDTs

3. **Over-decomposition (task-based runtimes)**
   - More chunks than workers to improve load balance
   - ARTS mapping: smaller chunk DBs; reuse the same halo-strip idea (more halos, but still “surface area”)

4. **Polyhedral dependence analysis (compile-time halo width)**
   - Compute stencil radius statically (here: 1 row)
   - CARTS mapping: derive halo width from `DbAcquireNode` bounds; generate halo-strip plan

5. **Time tiling / communication avoiding stencils**
   - Reduce halo exchange frequency by computing multiple time steps locally (larger halo)
   - CARTS mapping: legal if dependencies allow; increases halo width but may reduce total exchanges

---

## 11. Decision Heuristics and Recommendation

The “best” strategy depends on both **data movement** and **ARTS overhead**. A
practical compiler heuristic can be built from three measurable quantities:

1. **Halo bytes per iteration** (what we *want* to move)
2. **Chunk bytes per iteration** (what we move if we fetch whole neighbor chunks)
3. **Dependency cardinality** (how many DB dependencies we record/signal)

For 1D row-stencils on a 2D array `u[nx][ny]`:

```
rowBytes      = ny * elemBytes
chunkBytes    = chunkSize * rowBytes
haloBytes     = (haloLeft + haloRight) * rowBytes

fineGrained deps/worker ≈ chunkSize + haloLeft + haloRight
chunked     deps/worker ≈ 1..3 chunks
hybrid      deps/worker ≈ 1 chunk + (haloLeft + haloRight) halo strips
```

### Recommended default (Jacobi-like: uniform write + narrow read-only stencil)

Use **EHP (Enhanced Hybrid Partitioning)** as the default for this pattern:
- Loop 1 writes are chunk-local (ownership-friendly)
- Loop 2 reads are chunk-local *plus* a narrow, read-only halo (communication-friendly)
- an epoch/barrier already separates the phases (versioning-friendly)

EHP selection inside the compiler:
- Prefer **ESD slice gets** when the halo is a small, statically known set of byte ranges and
  `artsGetFromDbAt(...)` lowering is available (minimal bytes, no extra halo DBs/events).
- Otherwise prefer **Hybrid halo strips** (explicit halo DBs published each iteration).
- Only fall back to **Chunked** (whole neighbor chunk acquisition) if neither ESD nor Hybrid can be generated.

Why fine-grained is not the default here (even though it’s “precise”):
- ForLowering already gives us a contiguous chunk assignment; the *only* cross-rank data for Jacobi is the
  `O(haloBytes)` boundary. Making every interior row its own DB adds `O(chunkSize)` extra dependencies per EDT
  without reducing communication below the halo.
- In ARTS, fixed-slot EDTs + persistent-event signaling means “many tiny DBs” amplifies:
  - `artsRecordDep(...)` calls,
  - dependence registrations to persistent events,
  - slot-satisfaction signals,
  - and scheduler pressure,
  even when most of those DBs are local.
  Fine-grained remains useful as a small-N or irregular-pattern fallback, not as the Jacobi default.

### Mixed access patterns on the same allocation (general heuristic)

If an allocation is used with *multiple* access patterns across phases:
- If at least one phase writes and another phase reads with a stencil halo, do **not** assume “halo space implies halo data”.
- Prefer **EHP**: keep chunked ownership, and provide halos either as halo strips (Hybrid) or as slice gets (ESD).
- If EHP cannot be generated, pick the least-bad correct option based on scale:
  - If `nx` (or `Nrows`) is small: fine-grained is acceptable.
  - If `nx` is large: keep chunked to avoid dependence explosion, and accept extra bytes moved.

---

## 12. Conclusion

The Jacobi stencil case exposes a fundamental trade-off:
- **Communication volume** (bytes moved per iteration)
- **Runtime overhead** (number of DBs and dependency edges)

The key observation is that the “best” stencil strategy for ARTS is usually **EHP**:
keep chunked ownership for bulk updates and exchange only boundary halos (via Hybrid halo strips or ESD slice gets).

Fine-grained (row DBs) remains a useful baseline for correctness and minimal
transfer, but it can become an execution bottleneck in ARTS due to DB/event and
dependency scaling. Chunked is the right default for uniform loops, but it
becomes wasteful for narrow-halo stencils unless halo exchange is made explicit.

This analysis applies to many scientific computing patterns beyond Jacobi - any code with a "prepare data" phase followed by "compute with neighbors" will benefit from this approach.
