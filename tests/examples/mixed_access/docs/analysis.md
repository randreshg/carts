# Mixed Access Pattern Example Analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the mixed_access example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/mixed_access
   ```

2. **Build and generate the MLIR inputs:**

   ```bash
    carts build
    carts cgeist mixed_access.c -O0 --print-debug-info -S --raise-scf-to-affine &> mixed_access_seq.mlir
    carts run mixed_access_seq.mlir --collect-metadata &> mixed_access_arts_metadata.mlir
    carts cgeist mixed_access.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> mixed_access.mlir
   ```

   You should see the following in the output:

  ```mlir
    omp.parallel {
      omp.wsloop for (%e) : index = (%c0) to (%numElems) step (%c1) {
        // sum = 0; for j: sum += nodeData[nodelist[e][j]]; elemData[e] = sum/4
        omp.yield
      }
      omp.terminator
    }
```

1. **Canonicalize memrefs (array-of-pointers → shaped memrefs):**

   ```bash
   carts run mixed_access.mlir --canonicalize-memrefs &> mixed_access_canon.mlir
   ```

  Notice that all memrefes are canonicalized.

  ```mlir
    %alloc_7 = memref.alloc(%13, %17) : memref<?x?xi32>
  ```

1. **Create DBs with control DBs**

  ```bash
    carts run mixed_access.mlir --create-dbs > create_dbs.mlir
    grep -c "arts.edt" create_dbs.mlir      # Expected: 2 (one per omp.parallel)
  ```

1. **ForLowering - Worker Chunk Hints**

ForLowering lowers `omp.wsloop` into worker-chunked `scf.for` loops and marks
the worker acquires as `partitioning(<block>)` with offsets/sizes derived from
the loop IVs. Indirect reads may still carry chunked acquires here; DbPartitioning
decides which ones become full-range.

```bash
  carts run mixed_access.mlir --concurrency > for_lowering.mlir
```

If you grep for `partitioning(<block>)` in `for_lowering.mlir`, you should see
chunked acquires for the worker loops.

1. **DbPartitioning**
This example is a mixed access pattern: one parallel loop suggests chunked
partitioning, while the other needs full-range for the indirect reads. We rely
on H1.2 to choose block partitioning for the allocation and mark indirect
read-only acquires as full-range.

In `mixed_access_partitioned.mlir`, verify:

- First loop (node writes): block partitioning + per-chunk offsets/sizes.
- Second loop (indirect reads of `nodeData`): block partitioning but **full-range**
  offsets/sizes for the read acquire.
- Second loop (direct writes to `elemData`): block partitioning + per-chunk
  offsets/sizes.

Example (trimmed):

```mlir
%guid_8, %ptr_9 = arts.db_acquire[<out>] ... partitioning(<block>, offsets[%36], sizes[%c163]),
  offsets[%42], sizes[%47]
%guid_10, %ptr_11 = arts.db_acquire[<in>] ... partitioning(<block>, offsets[%36], sizes[%c157]),
  offsets[%c0], sizes[%c17]        // full-range for indirect reads
%guid_12, %ptr_13 = arts.db_acquire[<out>] ... partitioning(<block>, offsets[%36], sizes[%c157]),
  offsets[%42], sizes[%51]
```

Concretely, after `--concurrency-opt`:

- `nodeData` in the **first** parallel loop is chunked (`offsets[%chunk]`).
- `nodeData` in the **second** parallel loop is full-range (`offsets[%c0]`,
  `sizes[%numChunks]`) because of indirect indexing.
- `elemData` stays chunked in the second loop (direct access).

```bash
carts run mixed_access.mlir --concurrency-opt > mixed_access_partitioned.mlir
```

Verify by inspecting the acquires in `mixed_access_partitioned.mlir`.

1. **Execute and Verify Correctness**

```bash
# Compile and execute
carts execute mixed_access.c -O3

# Run the executable
./mixed_access_arts

# Expected output:
# Mixed Access Test: 2601 nodes, 2500 elements
# Pattern: chunked node writes + full-range indirect reads
# Running sequential version...
# Running parallel version...
# RMS error: 0.000000e+00
# Max error: 0.000000e+00
# Test PASSED
```

If something doesn't work, investigate why and fix it.
If necessary run lldb to debug.
