# Cholesky Dynamic Example Analysis

## Overview

Block-wise Cholesky decomposition (A = L * L^T) with OpenMP task dependencies. Uses dynamically-sized matrices with configurable N (default 64), block size 16. Heap-allocated arrays using pointer-to-pointer pattern.

This example demonstrates **2D fine-grained partitioning** where each matrix element becomes its own datablock, with diagonal access patterns like `A[i][i]`.

## Key Parameters

| Parameter | Value |
|-----------|-------|
| Matrix size (N) | argv[1] or 64 (dynamic) |
| Block size | 16 |
| Total blocks | (N/16)^2 |
| Tolerance | 10e-6 |
| Allocation | Heap (`double **L` with malloc) |

## Pipeline Architecture

The datablock pipeline follows a two-phase approach:

1. **CreateDbs**: Creates coarse DB-space acquires with **partition hints** (2D indices from depend clauses)
2. **DbPartitioning**: Transforms coarse acquires to 2D fine-grained based on hints

## The Pattern in C Code

### Dynamic Allocation Pattern

```c
int main(int argc, char *argv[]) {
    const unsigned N = (argc >= 2) ? atoi(argv[1]) : 64;

    // Pointer-to-pointer allocation
    double **A = (double **)malloc(N * sizeof(double *));
    double **L_parallel = (double **)malloc(N * sizeof(double *));

    for (unsigned i = 0; i < N; i++) {
        A[i] = (double *)malloc(N * sizeof(double));
        L_parallel[i] = (double *)malloc(N * sizeof(double));
    }
    ...
}
```

### Block-wise Cholesky with 2D Dependencies

```c
#pragma omp parallel
#pragma omp single
{
    for (unsigned k_block = 0; k_block < N; k_block += BLOCK_SIZE) {
        // Task 1: Factorize diagonal block (POTRF-like)
        #pragma omp task depend(inout: L_parallel[k_block][k_block])
        { /* diagonal block factorization */ }

        // Task 2: Update panel below diagonal (TRSM-like)
        for (unsigned j_block = k_block + kb; j_block < N; j_block += BLOCK_SIZE) {
            #pragma omp task depend(in: L_parallel[k_block][k_block]) \
                             depend(inout: L_parallel[j_block][k_block])
            { /* triangular solve */ }
        }

        // Task 3: Update trailing submatrix (SYRK/GEMM-like)
        for (unsigned j_block_trail = k_block + kb; j_block_trail < N; ...) {
            for (unsigned i_block_trail = j_block_trail; i_block_trail < N; ...) {
                #pragma omp task depend(in: L_parallel[i_block_trail][k_block], \
                                            L_parallel[j_block_trail][k_block]) \
                                 depend(inout: L_parallel[i_block_trail][j_block_trail])
                { /* rank-k update */ }
            }
        }
    }
}
```

**Key observation:** The `depend(inout: L_parallel[i][j])` creates **2D indexed dependencies** - each element has unique indices.

## Step-by-Step Analysis

### 1. Navigate to the cholesky/dynamic directory

```bash
cd ~/Documents/carts/tests/examples/cholesky/dynamic
```

### 2. Build and generate the MLIR inputs

```bash
carts build
carts cgeist cholesky.c -O0 --print-debug-info -S --raise-scf-to-affine &> cholesky_seq.mlir
carts run cholesky_seq.mlir --collect-metadata &> cholesky_arts_metadata.mlir
carts cgeist cholesky.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> cholesky.mlir
```

### 3. CreateDbs: Coarse acquires with 2D partition hints

```bash
carts run cholesky.mlir --create-dbs &> cholesky_create_dbs.mlir
```

**What CreateDbs produces:**

```mlir
// DbAlloc - coarse allocation (sizes=[1], elementSizes=[N, N])
%guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
    route(%c0_i32) sizes[%c1] elementType(f64) elementSizes[%15, %18]

// DbAcquire - coarse DB-space with 2D fine-grained partition hints
%guid_6, %ptr_7 = arts.db_acquire[<inout>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%arg2, %arg2]),  // <-- 2D indices!
    offsets[%c0], sizes[%c1]
    {partition_indices_segments = array<i32: 2>, ...}

// Multi-entry acquire (depends on multiple elements)
%guid_8, %ptr_9 = arts.db_acquire[<inout>] (%guid, %ptr)
    partitioning(<fine_grained>, indices[%arg2, %arg2, %arg3, %arg2]),
    offsets[%c0], sizes[%c1]
    {partition_indices_segments = array<i32: 2, 2>, ...}  // Two 2D entries
```

**Key observations:**

- `indices[%arg2, %arg2]` - 2D diagonal access pattern
- `partition_indices_segments = array<i32: 2, 2>` - multiple 2D entries per acquire
- Multi-entry acquires are expanded into separate single-entry acquires by DbPartitioning

### 4. DbPartitioning: Transform to 2D fine-grained allocation

```bash
carts run cholesky.mlir --concurrency-opt &> cholesky_partitioned.mlir
```

**What DbPartitioning does:**

1. **Expands multi-entry acquires** into separate DbAcquireOps
2. **Transforms allocation** from coarse to 2D fine-grained
3. **Generates 2D db_ref indices** for element access

In `cholesky_partitioned.mlir`, verify:

```mlir
// DbAlloc - now 2D fine-grained (sizes=[N, N], elementSizes=[1])
%guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <fine_grained>]
    route(%c0_i32) sizes[%15, %18] elementType(f64) elementSizes[%c1]

// Inside EDT: 2D db_ref indices
%ref = arts.db_ref %arg1[%c0, %c0] : memref<?x?xmemref<?xf64>> -> memref<?xf64>
%val = memref.load %ref[%c0] : memref<?xf64>
```

**Transformation summary:**

| Field | Before (CreateDbs) | After (DbPartitioning) |
|-------|-------------------|------------------------|
| DbAlloc mode | coarse | fine_grained |
| DbAlloc sizes | `[1]` | `[N, N]` |
| DbAlloc elementSizes | `[N, N]` | `[1]` |
| DbAcquire indices | `[]` | `[%i, %j]` |
| db_ref indices | 1D | 2D `[%c0, %c0]` |

### 5. Execute and verify

```bash
carts execute cholesky.c -O3
./cholesky_arts 32      # Small size for testing
./cholesky_arts 64      # Default size
```

## Current Status (January 2026)

### Compilation Pipeline

| Stage | Status | Notes |
|-------|--------|-------|
| MLIR Generation | ✓ Pass | cholesky.mlir generated |
| CreateDbs | ✓ Pass | 2D partition hints preserved |
| DbPartitioning | ✓ Pass | **No longer hangs** (iterator bug fixed) |
| LLVM Generation | ✓ Pass | cholesky-arts.ll generated |
| Binary Compilation | ✓ Pass | cholesky_arts binary created |

### Runtime Verification

| Test | Status | Notes |
|------|--------|-------|
| N=16 | **PASS** | Verification succeeded |
| N=32 | **PASS** | Verification succeeded |
| N=64 | **PASS** | Verification succeeded |

### Bug Fixes Applied

1. **Multi-entry acquire expansion + matching** (DbPartitioning)
   - Preserves per-entry hints and maps each `db_ref` to the correct entry
2. **Correct N-D bounds checks** (ConvertArtsToLLVM)
   - Uses the product of all allocation dimensions, preventing NULL deps
3. **Depv handling fixes** (EdtLowering/ConvertArtsToLLVM)
   - DepGep access path and latch increments work for DepDbAcquire

## Comparison with Matrix Example

| Aspect | Matrix | Cholesky |
|--------|--------|----------|
| Partition type | 2D fine-grained | 2D fine-grained |
| Index pattern | `A[i][j]` (independent) | `A[i][i]` (diagonal) |
| Multi-entry acquires | No | Yes (complex deps) |
| Runtime result | **PASS** | **PASS** |

Cholesky now passes with the same 2D fine-grained strategy after the
multi-entry and bounds-check fixes.

## Files Modified for 2D Support

| File | Change |
|------|--------|
| `lib/arts/Passes/Optimizations/DbPartitioning.cpp` | Fixed iterator invalidation |
| `include/arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h` | Multi-dim offsets |
| `lib/arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.cpp` | Updated constructor |
| `lib/arts/Transforms/Datablock/ElementWise/DbElementWiseRewriter.cpp` | Pass all indices |
| `lib/arts/Transforms/Datablock/DbRewriter.cpp` | Updated factory method |

## Next Steps (Optional)

1. Add a larger-N stress run (e.g., N=128) for regression coverage.
