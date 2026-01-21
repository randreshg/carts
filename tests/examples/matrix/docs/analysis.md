# Matrix deps example analysis

Walk through these steps and fix any problem that you find in the way

## Pipeline Architecture

The datablock pipeline follows a two-phase approach:

1. **CreateDbs**: Creates coarse DB-space acquires with **partition hints**
2. **DbPartitioning**: Transforms coarse acquires to fine-grained based on hints

This separation keeps CreateDbs simple (just wrap allocations) while DbPartitioning handles all partitioning optimization decisions.

## Step-by-Step Analysis

### 1. Navigate to the matrix example directory

```bash
cd ~/Documents/carts/tests/examples/matrix
```

### 2. Build and generate the MLIR inputs

```bash
carts build
carts cgeist matrix.c -O0 --print-debug-info -S --raise-scf-to-affine &> matrix_seq.mlir
carts run matrix_seq.mlir --collect-metadata &> matrix_arts_metadata.mlir
carts cgeist matrix.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> matrix.mlir
```

- `matrix_seq.mlir` - Sequential shape for reference
- `matrix_arts_metadata.mlir` - Collected metadata
- `matrix.mlir` - OpenMP input for the pipeline

### 3. Canonicalize memrefs (array-of-pointers → shaped memrefs)

```bash
carts run matrix.mlir --canonicalize-memrefs &> matrix_canon.mlir
```

Inspect `matrix_canon.mlir` to confirm the array-of-pointers form has been rewritten into a memref with explicit dimensions:

```mlir
// Before: pointer-chasing per row/col
%row = memref.load %A[%i] : memref<?xmemref<?xf64>>
%elt = memref.load %row[%j] : memref<?xf64>

// After: direct 2D memref with clear sizes
%elt = memref.load %A_flat[%i, %j] : memref<?x?xf64>
```

Verify: loop bounds match memref extents, casts are consistent, no remaining `memref<?xmemref<?xf64>>` users.

### 4. CreateDbs: Coarse acquires with partition hints

```bash
carts run matrix.mlir --create-dbs --debug-only=db &> matrix_create_dbs.mlir
```

**What CreateDbs does:**

- Wraps `memref.alloc` in `arts.db_alloc` operations
- Creates **coarse DB-space acquires**: `offsets=[0], sizes=[allocSizes]`
- Attaches **partition hints** from OpenMP depend clauses for DbPartitioning

In `matrix_create_dbs.mlir`, verify:

```mlir
// DbAlloc - coarse allocation (partition_mode will be set by DbPartitioning)
%guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
    route(%c0_i32) sizes[%c1] elementType(f64) elementSizes[%N, %N]

// DbAcquire - coarse DB-space with partition hints
%guid_2, %ptr_3 = arts.db_acquire[<out>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%i, %j]),  // <-- partition hints
    offsets[%c0], sizes[%c1]                        // <-- coarse DB-space
    {arts.twin_diff = false}

// DbRef uses [0] index for coarse 1D allocation
%ref = arts.db_ref %arg[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
```

**Key observations:**

- `partitioning(<fine_grained>, indices[%i, %j])` - hints from `depend(inout: A[i][j])`
- `offsets[%c0], sizes[%c1]` - coarse DB-space (acquire entire block)
- `arts.twin_diff = false` - indexed hints prove non-overlap
- DbRef uses `[%c0]` because DB-space is 1D coarse

### 5. DbPartitioning: Transform to fine-grained

```bash
carts run matrix.mlir --concurrency-opt --debug-only=db &> matrix_partitioned.mlir
```

**What DbPartitioning does:**

- Reads partition hints from acquires
- Transforms DbAlloc from coarse to fine-grained based on hints
- Updates DbAcquire to use actual DB-space indices/offsets

In `matrix_partitioned.mlir`, verify:

```mlir
// DbAlloc - now fine-grained (transformed by DbPartitioning)
%guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <fine_grained>]
    route(%c0_i32) sizes[%N] elementType(f64) elementSizes[%N]

// DbAcquire - now has DB-space indices from hints
%guid_2, %ptr_3 = arts.db_acquire[<out>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%i, %j]),  // <-- hints preserved
    indices[%i], offsets[%c0], sizes[%c1]           // <-- actual DB-space access
    {arts.twin_diff = false}
```

**Transformation summary:**

| Field | Before (CreateDbs) | After (DbPartitioning) |
|-------|-------------------|------------------------|
| DbAlloc sizes | `[1]` coarse | `[N]` fine-grained |
| DbAcquire indices | empty | `[%i]` from hints |
| DbAcquire offsets | `[0]` | `[0]` |
| DbAcquire sizes | `[1]` | `[1]` |

### 6. Continue or run end-to-end

```bash
# Later stages
carts run matrix.mlir --concurrency --debug-only=db &> matrix_concurrency.mlir
carts run matrix.mlir --arts-to-llvm &> matrix_llvm.mlir

# Full execution
carts execute matrix.c
./matrix 8
```

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Coarse acquires after DbPartitioning | Missing partition hints | Check DbControlOp generation from depend clauses |
| `twin_diff = true` unexpectedly | No indexed hints | Verify depend clauses have indices `A[i][j]` |
| DbRef index mismatch | DB-space rank mismatch | Check acquire sizes match DbRef indices |
| Assertion in verifier | Partition hints in DB-space fields | Ensure CreateDbs uses separate partition hint fields |

## Multi-Entry Acquire Expansion

This example demonstrates how CARTS handles tasks with **multiple dependencies on the same array** (e.g., `A[i][j]` and `A[i-1][j]`).

### The Pattern in C Code

```c
// Task depends on A[i][j] AND A[i-1][j]
#pragma omp task depend(in: A[i][j], A[i-1][j]) depend(out: B[i][j])
{
    B[i][j] = A[i][j] + A[i-1][j];
}
```

### CreateDbs: Multi-Entry Partition Hints

CreateDbs creates a **single acquire with multiple entries** encoded in segment arrays:

```mlir
%guid_2, %ptr_3 = arts.db_acquire[<inout>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%arg2, %arg3, %26, %arg3]),  // <-- 4 values!
    offsets[%c0], sizes[%c1]
    {partition_indices_segments = array<i32: 2, 2>,    // Two entries, each with 2 indices
     partition_entry_modes = array<i32: 2, 2>}         // Both are fine_grained (mode=2)
```

- `indices[%arg2, %arg3, %26, %arg3]` contains two 2D indices: `[i,j]` and `[i-1,j]`
- `partition_indices_segments = [2, 2]` means: first entry has 2 indices, second has 2

### DbPartitioning: Expand to Separate Acquires

DbPartitioning expands the multi-entry acquire into **separate DbAcquireOps**:

```mlir
// Entry 1: A[i][j]
%guid_2, %ptr_3 = arts.db_acquire[<in>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%arg2, %arg3]),
    indices[%arg2], offsets[%c0], sizes[%c1]

// Entry 2: A[i-1][j]
%guid_4, %ptr_5 = arts.db_acquire[<in>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%26, %arg3]),
    indices[%26], offsets[%c0], sizes[%c1]

// Output: B[i][j]
%guid_6, %ptr_7 = arts.db_acquire[<out>] (%guid_0, %ptr_1)
    partitioning(<fine_grained>, indices[%arg2, %arg3]),
    indices[%arg2], offsets[%c0], sizes[%c1]

// EDT now has 3 block arguments (one per acquire)
arts.edt <task> route(%c0_i32) (%ptr_3, %ptr_5, %ptr_7) : ... {
^bb0(%arg4: ..., %arg5: ..., %arg6: ...):
    %27 = arts.db_ref %arg4[%c0] : ... -> memref<?xf64>   // A[i][j]
    %28 = memref.load %27[%arg3] : memref<?xf64>
    %29 = arts.db_ref %arg5[%c0] : ... -> memref<?xf64>   // A[i-1][j]
    %30 = memref.load %29[%arg3] : memref<?xf64>
    ...
}
```

### Verification Results (January 2026)

| Test | Status | Details |
|------|--------|---------|
| CreateDbs hints | ✅ PASS | `indices[%arg2, %arg3, %26, %arg3]` with segments `[2, 2]` |
| DbPartitioning expansion | ✅ PASS | 3 separate acquires, 3 block args |
| Row-wise partitioning | ✅ PASS | `sizes[%N]` rows, `elementSizes[%N]` cols |
| Runtime execution | ✅ PASS | `matrix_arts 8` produces correct output |
