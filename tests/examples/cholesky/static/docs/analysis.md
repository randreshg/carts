# Cholesky Static Example Analysis

## Overview

Block-wise Cholesky decomposition (A = L * L^T) with OpenMP task dependencies. Uses a fixed 128x128 matrix with block size 16, resulting in 8x8 blocks. Stack-allocated arrays.

## Key Parameters

| Parameter | Value |
|-----------|-------|
| Matrix size (N) | 128 (fixed) |
| Block size | 16 |
| Total blocks | 8x8 = 64 |
| Tolerance | 1e-6 |
| Allocation | Stack (`double L[N][N]`) |

## Pipeline Architecture

The datablock pipeline follows a two-phase approach:

1. **CreateDbs**: Creates coarse DB-space acquires with **partition hints**
2. **DbPartitioning**: Transforms coarse acquires to fine-grained based on hints

This separation keeps CreateDbs simple (just wrap allocations) while DbPartitioning handles all partitioning optimization decisions.

## The Pattern in C Code

Block-wise Cholesky with three task types (POTRF/TRSM/SYRK):

```c
#pragma omp parallel
#pragma omp single
{
    for (int k_block = 0; k_block < N; k_block += BLOCK_SIZE) {
        int kb = (k_block + BLOCK_SIZE > N) ? N - k_block : BLOCK_SIZE;

        // Task 1: Factorize diagonal block (POTRF-like)
        #pragma omp task depend(inout: L_parallel[k_block][k_block])
        { /* diagonal block factorization */ }

        // Task 2: Update panel below diagonal (TRSM-like)
        for (int j_block = k_block + kb; j_block < N; j_block += BLOCK_SIZE) {
            #pragma omp task depend(in: L_parallel[k_block][k_block]) \
                             depend(inout: L_parallel[j_block][k_block])
            { /* triangular solve */ }
        }

        // Task 3: Update trailing submatrix (SYRK/GEMM-like)
        for (int j_block_trail = k_block + kb; j_block_trail < N; ...) {
            for (int i_block_trail = j_block_trail; i_block_trail < N; ...) {
                #pragma omp task depend(in: L_parallel[i_block_trail][k_block], \
                                            L_parallel[j_block_trail][k_block]) \
                                 depend(inout: L_parallel[i_block_trail][j_block_trail])
                { /* rank-k update */ }
            }
        }
    }
}
```

### Task Dependency Structure

```
k_block=0:
  POTRF[0,0] ─────────────────────────────────────┐
       │                                          │
       ├──> TRSM[1,0] ──> SYRK[1,1]              │
       │         │              │                 │
       ├──> TRSM[2,0] ──> SYRK[2,1] ──> SYRK[2,2]│
       │         │              │         │       │
       ...      ...            ...       ...     ...
       │                                          │
k_block=1:                                        │
  POTRF[1,1] <────────────────────────────────────┘
       │
       ...
```

## Step-by-Step Analysis

### 1. Navigate to the cholesky/static directory

```bash
cd ~/Documents/carts/tests/examples/cholesky/static
```

### 2. Build and generate the MLIR inputs

```bash
carts build
carts cgeist cholesky.c -O0 --print-debug-info -S --raise-scf-to-affine &> cholesky_seq.mlir
carts run cholesky_seq.mlir --collect-metadata &> cholesky_arts_metadata.mlir
carts cgeist cholesky.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> cholesky.mlir
```

### 3. Examine the generated MLIR for dependency handling

```bash
grep -E "alloca.*memref<f64>|omp.task|depend" cholesky.mlir
```

Expected pattern showing **dependency aliasing issue**:

```mlir
// Allocas created OUTSIDE the k_block loop
%alloca_7 = memref.alloca() : memref<f64>   // Used for ALL k_block iterations!
%alloca_8 = memref.alloca() : memref<f64>
%alloca_9 = memref.alloca() : memref<f64>

affine.for %arg0 = 0 to 128 step 16 {
    // Store diagonal element value into scalar alloca
    %21 = affine.load %alloca_5[%arg0, %arg0] : memref<128x128xf64>
    affine.store %21, %alloca_7[] : memref<f64>

    // Task depends on SAME alloca_7 for ALL iterations!
    omp.task depend(taskdependinout -> %alloca_7 : memref<f64>) {
        ...
    }
}
```

### 4. CreateDbs: Coarse acquires with partition hints

```bash
carts run cholesky.mlir --create-dbs --debug-only=db &> cholesky_create_dbs.mlir
```

### 5. DbPartitioning: Transform to fine-grained

```bash
carts run cholesky.mlir --concurrency-opt --debug-only=db &> cholesky_partitioned.mlir
```

### 6. Full execution

```bash
carts execute cholesky.c -O3
./cholesky_arts
```

## Critical Issue: Dependency Aliasing

### Root Cause

The generated MLIR shows dependencies mapped to **scalar memref<f64> allocas** that are **reused across loop iterations**:

```mlir
%alloca_7 = memref.alloca() : memref<f64>    // Reused for ALL k_block iterations!

affine.for %arg0 = 0 to 128 step 16 {
    %21 = affine.load %alloca_5[%arg0, %arg0] : memref<128x128xf64>
    affine.store %21, %alloca_7[] : memref<f64>
    omp.task depend(taskdependinout -> %alloca_7 : memref<f64>) {...}
}
```

**Problem**: The SSA value `%alloca_7` is the same for all iterations, but it represents different blocks `L[0][0]`, `L[16][16]`, `L[32][32]`, etc.

### Why This Causes Issues

1. **Same SSA = Same Dependency**: Runtime sees all POTRF tasks as depending on the same memory location
2. **Incorrect Serialization**: Tasks that should run concurrently are serialized
3. **Potential Deadlock**: Circular false dependencies can cause hanging

## Current Status (January 2026)

| Test | Status | Details |
|------|--------|---------|
| Compilation | Pass | MLIR and binary generated |
| CreateDbs hints | Pass | Coarse allocs with partition hints preserved |
| DbPartitioning | Pass | Block/fine-grained rewrites applied as needed |
| Runtime execution | Pass | Verified with current example inputs |

### Comparison with Working Examples

| Example | Index Pattern | Works? |
|---------|---------------|--------|
| array/deps | `depend(inout: A[i])` - 1D | Yes |
| matrix | `depend(out: A[i][j])` - 2D element | Yes |
| cholesky | `depend(inout: L[k_block][k_block])` - 2D block | Yes |

### Historical Root Cause (Fixed)

Earlier builds could lower `depend(inout: L[k_block][k_block])` into a
loop-invariant scalar alloca, collapsing distinct block dependencies into one
SSA value. The current pipeline preserves partition hints on `db_acquire` and
rewrites them correctly during DbPartitioning, so dependencies remain distinct.

## Cholesky-Specific Patterns

### Lower Triangular Access

The algorithm only operates on the lower triangle of the matrix:

```c
// Task 3: i_block_trail >= j_block_trail (lower triangle only)
for (int j_block_trail = k_block + kb; j_block_trail < N; ...) {
    for (int i_block_trail = j_block_trail; i_block_trail < N; ...) {
        // i_block_trail >= j_block_trail enforced by loop bounds
    }
}
```

### Block Size Boundary Handling

Edge blocks may be smaller than BLOCK_SIZE:

```c
int kb = (k_block + BLOCK_SIZE > N) ? N - k_block : BLOCK_SIZE;
```

For N=128 and BLOCK_SIZE=16, all blocks are full size (128 % 16 == 0).
