# Array chunks example analysis

This example demonstrates **chunked partitioning** where tasks operate on fixed-size blocks of an array.

## Pipeline Architecture

The datablock pipeline follows a two-phase approach:

1. **CreateDbs**: Creates coarse DB-space acquires with **partition hints** (offsets/sizes from depend clauses)
2. **DbPartitioning**: Transforms coarse acquires to chunked based on hints

## The Pattern in C Code

```c
#define BLOCK_SIZE 4

#pragma omp task depend(in: A[i:BLOCK_SIZE]) depend(out: B[i:BLOCK_SIZE])
{
    for (int j = 0; j < BLOCK_SIZE; j++) {
        B[i + j] = A[i + j] * 2.0;
    }
}
```

**Key observation:** The `A[i:BLOCK_SIZE]` syntax creates a **range dependency** - the task depends on `BLOCK_SIZE` consecutive elements starting at index `i`.

## Step-by-Step Analysis

### 1. Navigate to the array example directory

```bash
cd ~/Documents/carts/tests/examples/array/chunks
```

### 2. Build and generate the MLIR inputs

```bash
carts build
carts cgeist array.c -O0 --print-debug-info -S --raise-scf-to-affine &> array_seq.mlir
carts run array_seq.mlir --collect-metadata &> array_arts_metadata.mlir
carts cgeist array.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> array.mlir
```

### 3. Canonicalize memrefs (Omp deps canonicalization)

```bash
carts run array.mlir --canonicalize-memrefs &> array_canonicalize_memrefs.mlir
```

Notice we generated `arts.omp_dep` operations for the OpenMP dependencies.
Sizes different from 1 indicate chunked dimensions:

```mlir
%15 = arts.omp_dep<<in>> %alloc[%arg2] size[%c4] : memref<?xf64> -> memref<?xf64>
%16 = arts.omp_dep<<out>> %alloc_0[%arg2] size[%c4] : memref<?xf64> -> memref<?xf64>
```

### 4. CreateDbs: Coarse acquires with chunked hints

```bash
carts run array.mlir --create-dbs &> array_create_dbs.mlir
```

**What CreateDbs does:**

- Wraps `memref.alloc` in `arts.db_alloc` operations with coarse mode
- Creates **chunked partition hints** from OpenMP depend clauses

In `array_create_dbs.mlir`, verify:

```mlir
// DbAlloc - coarse allocation (sizes=[1], elementSizes=[N])
%guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>]
    route(%c0_i32) sizes[%c1] elementType(f64) elementSizes[%9]

// DbAcquire - coarse DB-space with chunked partition hints
%guid_2, %ptr_3 = arts.db_acquire[<inout>] (%guid, %ptr)
    partitioning(<block>, offsets[%arg2], sizes[%c4]),  // <-- chunked hints!
    offsets[%c0], sizes[%c1]                               // <-- coarse DB-space
    {arts.twin_diff = false, partition_entry_modes = array<i32: 1>, ...}
```

**Key observations:**

- `partitioning(<block>, offsets[%arg2], sizes[%c4])` - hints from `depend(in: A[i:BLOCK_SIZE])`
- `offsets[%arg2]` - chunk start position (loop IV)
- `sizes[%c4]` - block size (BLOCK_SIZE = 4)

### 5. DbPartitioning: Transform to chunked allocation

```bash
carts run array.mlir --concurrency-opt &> array_partitioned.mlir
```

**What DbPartitioning does:**

- Reads partition hints from acquires
- Computes `numChunks = ceil(totalSize / blockSize)`
- Transforms DbAlloc from coarse to chunked
- Generates div/mod operations for index localization

In `array_partitioned.mlir`, verify:

```mlir
// Compute numChunks = ceil(N / 4)
%10 = arith.index_cast %9 : index to i64
%11 = arith.addi %10, %c3_i64 : i64        // N + 3
%12 = arith.divui %11, %c4_i64 : i64       // (N + 3) / 4 = ceil(N/4)
%13 = arith.index_cast %12 : i64 to index

// DbAlloc - now chunked (sizes=[numChunks], elementSizes=[BLOCK_SIZE])
%guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>]
    route(%c0_i32) sizes[%13] elementType(f64) elementSizes[%c4]

// DbAcquire - now has DB-space offsets from hints
%18 = arith.divui %arg2, %c4 : index       // chunkIdx = loopIV / BLOCK_SIZE
...
%guid_2, %ptr_3 = arts.db_acquire[<in>] (%guid, %ptr)
    partitioning(<block>, offsets[%arg2], sizes[%c4]),
    offsets[%18], sizes[%24]               // <-- actual DB-space (chunkIdx, numChunks)

// Inside EDT: index localization
%25 = arith.remui %arg5, %c4 : index       // localIdx = globalIdx % BLOCK_SIZE
%26 = arts.db_ref %arg3[%c0] : ... -> memref<?xf64>
%27 = memref.load %26[%25] : memref<?xf64>  // Access using local index
```

**Transformation summary:**

| Field | Before (CreateDbs) | After (DbPartitioning) |
|-------|-------------------|------------------------|
| DbAlloc mode | coarse | chunked |
| DbAlloc sizes | `[1]` | `[numChunks]` |
| DbAlloc elementSizes | `[N]` | `[BLOCK_SIZE]` |
| DbAcquire offsets | `[0]` | `[chunkIdx]` |
| Inner loop index | `%i + %j` | `%j % BLOCK_SIZE` |

### 6. Execute and verify

```bash
carts execute array.c -O3
./array_arts 64
```

**Expected output:** `[CARTS] array.c: PASS`

## Chunked Partitioning Semantics

| Attribute | Meaning |
|-----------|---------|
| `partition_offsets[%i]` | Global element offset (start of chunk) |
| `partition_sizes[%c4]` | Block size in elements |
| `offsets[%chunkIdx]` | DB-space chunk index (computed: `offset / blockSize`) |
| `sizes[%numChunks]` | Number of chunks to acquire |

## Index Localization

The DbChunkedRewriter transforms global indices to local chunk indices:

```
globalIdx = loopIV + innerOffset
chunkIdx = globalIdx / BLOCK_SIZE    (selects which chunk)
localIdx = globalIdx % BLOCK_SIZE    (offset within chunk)
```

This allows the EDT to access `memref[localIdx]` instead of `memref[globalIdx]`, which works correctly with the chunked datablock layout.
