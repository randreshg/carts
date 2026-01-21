# Rowchunk Example Analysis

This example demonstrates **row-chunking optimization** for `#pragma omp parallel for` without explicit dependencies. Unlike the `matrix` example (which uses `omp task depend` for element-wise allocation), rowchunk shows how CARTS automatically partitions arrays into row chunks based on the number of workers.

## Key Differences from Matrix Example

| Aspect | Matrix Example | Rowchunk Example |
|--------|----------------|------------------|
| OpenMP construct | `#pragma omp task depend(...)` | `#pragma omp parallel for` |
| DbControlOps | Present (from depend clause) | **None** |
| Partitioning | Element-wise (fine-grained) | **Row-chunking** |
| Decision driver | User-provided dependencies | H2 heuristic + worker count |

## Pipeline Flow (Two-Pass Db)

The key insight is that the Db pass runs **twice**:

1. **First pass** (`--create-dbs`): Creates coarse allocations with `sizes=[1]`
2. **ForLowering**: Converts `arts.for` → chunked `scf.for`, populates `offset_hints`/`size_hints`
3. **Second pass** (`--concurrency-opt`): Promotes allocations using chunk info from hints

## Walkthrough Steps

### 1. Navigate to the rowchunk example directory

```bash
cd ~/Documents/carts/tests/examples/rows/chunks
```

### 2. Build and generate the MLIR inputs

```bash
carts build
carts cgeist rowchunk.c -O0 --print-debug-info -S --raise-scf-to-affine &> rowchunk_seq.mlir
carts run rowchunk_seq.mlir --collect-metadata &> rowchunk_arts_metadata.mlir
carts cgeist rowchunk.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> rowchunk.mlir
```

### 3. Canonicalize memrefs (array-of-pointers → shaped memrefs)

```bash
carts run rowchunk.mlir --canonicalize-memrefs &> rowchunk_canon.mlir
```

Verify that `float **A` becomes `memref<?x?xf32>`:
```mlir
// Before: array-of-pointers
%row = affine.load %A[%i] : memref<?xmemref<?xf32>>
%elt = affine.load %row[%j] : memref<?xf32>

// After: contiguous 2D memref
%elt = memref.load %A_flat[%i, %j] : memref<?x?xf32>
```

### 4. Create DBs (first pass - coarse allocation)

```bash
carts run rowchunk.mlir --create-dbs &> rowchunk_create_dbs.mlir
```

**Expected behavior**: Initial allocations are **coarse** (`sizes=[1]`) because ForLowering hasn't run yet:
```mlir
%guid, %ptr = arts.db_alloc ... sizes[%c1] elementType(f32) elementSizes[%runtime, %c1000]
```

The EDT contains `arts.for` which will be lowered next:
```mlir
arts.for(%c0) to(%c1000) step(%c1) {{
  /// parallel loop body
}}
```

### 5. Run concurrency passes (ForLowering populates hints)

```bash
carts run rowchunk.mlir --concurrency &> rowchunk_concurrency.mlir
```

**Expected behavior**: ForLowering converts `arts.for` to worker distribution and adds
chunked acquires with `partitioning(<block>)` plus DB-space `offsets`/`sizes`:
```mlir
scf.for %worker = %c0 to %c16 step %c1 {
  %offset = arith.muli %worker, %c63 : index  /// 1000/16 ≈ 63 rows per worker
  %size = ...                                 /// min(63, remaining)

  %guid_acq, %ptr_acq = arts.db_acquire ... partitioning(<block>), offsets[%offset], sizes[%size]
}
```

### 6. Run concurrency-opt (second Db pass - promotion)

```bash
carts run rowchunk.mlir --concurrency-opt &> rowchunk_concurrency_opt.mlir
```

**Expected behavior**: The second Db pass uses chunk info from hints to promote allocations:

```mlir
// BEFORE (create-dbs):
%guid, %ptr = arts.db_alloc ... sizes[%c1] elementSizes[%runtime, %c1000]

// AFTER (concurrency-opt):
%numChunks = arith.ceildivui %runtime, %c63 : index
%guid, %ptr = arts.db_alloc ... sizes[%numChunks] elementSizes[%c63, %c1000]
```

**Heuristics applied**:
- **H1 (Read-only on single-node)**: Array A (read-only `<in>`) stays coarse - no benefit from chunking
- **H2 with ParallelFor source**: Array B (write `<out>`) gets row-chunked - prioritize parallelism

### 7. Verify final LLVM IR

```bash
carts run rowchunk.mlir --arts-to-llvm &> rowchunk_llvm.mlir
```

### 8. Execute the program

```bash
carts execute rowchunk.c
./rowchunk_arts
```

**Expected output**:
```
[CARTS] rowchunk.c: PASS (X.XXXXs)
```

The runtime should create 16 task EDTs, each processing ~63 rows of the 1000-row matrix.

## Verification Checklist

| Stage | What to Check |
|-------|---------------|
| `rowchunk_canon.mlir` | `memref<?x?xf32>` (not `memref<?xmemref<?xf32>>`) |
| `rowchunk_create_dbs.mlir` | `sizes[%c1]` (coarse), `arts.for` present |
| `rowchunk_concurrency.mlir` | `offset_hints[...]` and `size_hints[...]` on acquires |
| `rowchunk_concurrency_opt.mlir` | `sizes[%numChunks]`, `elementSizes[%c63, %c1000]` (row-chunked) |
| Execution | 16 task EDTs, `PASS` result |

## Troubleshooting

**Problem**: Allocation stays at `sizes=[1]` after concurrency-opt
- **Check**: Are `offset_hints`/`size_hints` present on acquires after concurrency?
- **Check**: Does `canBePartitioned()` return true? (metadata: `accessedInParallelLoop=true`, `hasLoopCarriedDeps=false`)

**Problem**: Read-only array gets chunked (unexpected)
- **Check**: H1 heuristic should keep read-only arrays coarse on single-node
- **Check**: `arts.cfg` has `nodeCount=1`

**Problem**: Wrong block size
- **Check**: `arts.cfg` has `threads=16` (determines worker count)
- **Expected**: blockSize = ceil(1000/16) = 63 rows
