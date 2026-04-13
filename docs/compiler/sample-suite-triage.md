# Sample Suite Triage — architecture/sde-restructuring

Snapshot: 2026-04-13, branch `architecture/sde-restructuring` at commit
`bab33916` (RFC step 3 gap D).

Running `dekk carts examples run --all` against the default config
(`samples/arts.cfg`, 8 workers, 1 node) produces:

- **9 PASS**
- **6 RUN-FAIL** (compile ok, runtime prints `FAIL — …`)
- **11 COMPILE-FAIL** (`carts-compile` crashes, binary never produced)

All 17 failures trace to a single pipeline feature: `DbPartitioning` (stage
11) on DBs whose storage type is `memref<?xmemref<?xT>>` — a pointer to a
heap array coming from `int *A = malloc(N*…)`. Compile failures are that
same code path going into infinite recursion (`copyArtsMetadataAttrs` calls
itself with identical arguments); run failures are the same path silently
downgrading `<fine_grained, indices[%i]>` to `<coarse>`, collapsing N
per-element acquires into one coarse acquire that serializes every task.

---

## 1. Passing (9)

| Sample | Shape |
|---|---|
| `convolution` | `omp parallel for` over dense grid |
| `deps` | scalar `a`, `b` with `task depend()` — no arrays |
| `jacobi/deps` | scalar reductions |
| `matrix` | simple matrix ops |
| `matrixmul` | dense matmul |
| `parallel` | plain `omp parallel` |
| `parallel_for/block` | `omp parallel for schedule(static)` |
| `parallel_for/static` | simple static schedule |
| `task` | scalar task |

Common property: none rely on per-element `depend(inout: A[i])` against a
heap-allocated `int *A`. Deps are either scalar (one DB per var) or
absent.

## 2. Compile failures (11) — `carts-compile` crash

All except three crash at **stage 11 / DbPartitioningPass**:

| Sample | Crash site |
|---|---|
| `concurrent/concurrent.cpp` | DbPartitioning |
| `jacobi/for/jacobi-for.c` | DbPartitioning |
| `mixed_access/mixed_access.c` | DbPartitioning |
| `mixed_orientation/poisson_mixed_orientation.c` | DbPartitioning |
| `parallel_for/loops/parallel_for.c` | DbPartitioning |
| `parallel_for/reduction/parallel_for.c` | DbPartitioning |
| `parallel_for/single/parallel_for.c` | DbPartitioning |
| `rows/chunks/rowchunk.c` | DbPartitioning |

The crash signature (from `samples/parallel_for/single/`):

```
#4  mlir::arts::copyArtsMetadataAttrs(mlir::Operation*, mlir::Operation*)   0x…
#5  mlir::arts::copyArtsMetadataAttrs(mlir::Operation*, mlir::Operation*)   0x…  (SAME addr)
#6  DbPartitioningPass::assembleAndApplyRewritePlan(…)
#7  DbPartitioningPass::partitionAlloc(…)
```

Frames 4 and 5 at identical addresses → **tight self-recursion**.
`copyArtsMetadataAttrs` is defined in
`lib/arts/dialect/core/Analysis/heuristics/PartitioningHeuristics.cpp:706`
and its body does not explicitly recurse — the infinite call most likely
comes from an attribute copy that re-triggers a rewriter hook which calls
back into `copyArtsMetadataAttrs` on the same op pair.

Three samples fail **earlier** (before DbPartitioning) with a nested-memref
diagnostic:

| Sample | Error |
|---|---|
| `dotproduct/dotproduct.c` | `un-normalizable nested memref pattern (element type is memref)` |
| `parallel_for/stencil/parallel_for.c` | same |
| `stencil/stencil.c` | `cannot trace memref operand to its underlying allocation` |

These errors surface from `CreateDbs` / `RaiseMemRefDimensionality` when
confronted with the same `memref<?xmemref<?xT>>` shape — the errors are
just diagnostic-friendly versions of what blows up DbPartitioning later.

## 3. Run failures (6) — binary produces wrong answer

| Sample | Task-dep pattern |
|---|---|
| `array/chunks/array.c` | `task depend(inout: A[i])` on `int *A` |
| `array/deps/array.c` | `task depend(in: A[i], in: B[i-1], inout: B[i])` |
| `cholesky/dynamic/cholesky.c` | per-block dep on tile `(i,j)` |
| `cholesky/static/cholesky.c` | static schedule per-tile |
| `rows/deps/rowdeps.c` | per-row deps on a 2D array |
| `smith-waterman/smith-waterman.c` | per-cell wavefront deps |

### Pipeline trace — `array/deps`

Dumped via `--all-pipelines` at
`samples/array/deps/pipelines/` (regenerable via `dekk carts compile
samples/array/deps/array.c --all-pipelines -o samples/array/deps/pipelines`).

| Stage | Per-element `[i]` preserved? |
|---|---|
| 01 raise-memref-dimensionality | yes — `arts.omp_dep<<inout>> %alloc[%i] size[%c1]` |
| 03 openmp-to-arts | yes — EDTs carry memref operands |
| 05 create-dbs | yes — `db_acquire … partitioning(<fine_grained>, indices[%i])` |
| 06–10 | yes (6 fine-grained acquires) |
| **11 db-partitioning** | **no — all 6 rewritten to `<coarse>`, indices dropped** |
| 14 epochs | only `<coarse>` remains; `partition_indices_segments` metadata lingers but the index SSA is gone |

Effect: every task acquires the same single-element DB with `<inout>`, so
the runtime serializes N tasks on one dep edge. The `[i]` distinction is
silently dropped and tasks over-overlap, producing wrong values.

### Compounding shape issue

The DB type at stage 05 is `memref<?xmemref<?xi32>>` — a *single-element*
DB **holding** the N-element pointer, not a true N-element DB of `i32`:

```mlir
%guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
    route(…) sizes[%c1] elementType(i32) elementSizes[%N]
    : (memref<?xi64>, memref<?xmemref<?xi32>>)
```

`sizes[%c1] elementType(i32) elementSizes[%N]` says "1 element, size N of
i32" — the partitioning model sees one element and therefore one dep edge
per DB, no matter the `indices[%i]` hint. This is the same memref-of-memref
shape that trips the compile-fail diagnostic in `dotproduct`/`stencil`.

## 4. Root cause — three symptoms, one feature

```
                     int *A = malloc(N*sizeof(int));
                     #pragma omp task depend(inout: A[i])

                              │
                     RaiseMemRefDimensionality
                              │ memref.alloc(N) : memref<?xi32>
                     ConvertOpenMPToSde + ConvertSdeToArts
                              │ arts.edt <task> (%alloc)
                     CreateDbs
                              │ arts.db_alloc … sizes[%c1] elementType(i32) elementSizes[%N]
                              │ type: memref<?xmemref<?xi32>>   ← memref-of-memref DB
                              │ arts.db_acquire … partitioning(<fine_grained>, indices[%i])
                              │
                              ▼
                    DbPartitioning stage 11
                              │
               ┌──────────────┼──────────────────────────┐
               ▼              ▼                          ▼
       copyArtsMetadataAttrs  downgrade to               emit "un-normalizable
       self-recursion          <coarse>, drop indices    nested memref" error
       (8 samples)             (6 run-fail samples)      (3 samples)
```

## 5. Open questions / next steps

Potential fix paths, ordered by scope:

1. **Tactical**: add a null-guard / recursion-depth assertion at
   `copyArtsMetadataAttrs` entry to convert the crash into a diagnostic, so
   we can see *which* attribute copy triggers the self-recursion. Small,
   makes the 8 Category-A samples report a real error instead of crashing.
2. **Targeted**: fix `CreateDbs` to flatten `int *A` malloc'd arrays into
   N-element DBs of T (instead of 1-element DBs holding the pointer). This
   removes the memref-of-memref shape entirely — all three symptoms
   disappear simultaneously.
3. **Architectural (RFC step 5)**: raise these patterns into the
   `!sde.token` dataflow form via `RaiseMemrefToTensor` with slice access
   contracts (the pass's v2 plan). The token threading then forces each
   task to see a distinct slice — partitioning can stay coarse because the
   dep graph is expressed at the SSA level, not in partitioning metadata.

My recommendation: (1) as a same-day diagnostic improvement, then (2) for
the actual fix. (3) is the correct long-term answer but larger.

## 6. Reproducing the analysis

### Batch run
```bash
dekk carts examples run --all --json /tmp/carts-samples.json
```

### Full pipeline dump for one sample
```bash
dekk carts compile samples/parallel_for/single/parallel_for.c \
    --all-pipelines --arts-config samples/arts.cfg \
    -o samples/parallel_for/single/pipelines
```
Produces the dialect-keyed tree documented in
[`docs/compiler/pipeline.md`](pipeline.md) and emitted by
`tools/scripts/compile.py::_compile_all_pipelines`:
```
pipelines/
  1_polygeist/   cgeist C→MLIR output
  2_sde/         stages 01..02 + stage 03 passes up to ConvertSdeToArts
  3_core/        stage 03 from ConvertSdeToArts onwards + stages 04..14
  4_rt/          stage 15 from ParallelEdtLowering onwards + stage 16
  5_llvm/        final .ll
  boundaries/    dialect-conversion slices (omp→sde, sde→arts, arts→rt)
  complete.mlir  full post-pipeline MLIR
```
The bucketing is driven by `_DIALECT_BOUNDARIES` + the live pipeline
manifest, so new phases / renamed stages update the layout automatically.
