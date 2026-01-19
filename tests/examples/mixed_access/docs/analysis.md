# Mixed Access Pattern Example Analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the matrixmul example directory:**

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

3. **Canonicalize memrefs (array-of-pointers → shaped memrefs):**

   ```bash
   carts run mixed_access.mlir --canonicalize-memrefs &> mixed_access_canon.mlir
   ```

  Notice that all memrefes are canonicalized.

  ```mlir
    %alloc_7 = memref.alloc(%13, %17) : memref<?x?xi32>
  ```

4. **Create DBs with control DBs**

  ```bash
    carts run mixed_access.mlir --create-dbs > create_dbs.mlir
    grep -c "arts.edt" create_dbs.mlir      # Expected: 2 (one per omp.parallel)
  ```

5. **ForLowering - Chunk Hints from Loop IVs**

ForLowering adds `chunk_hint(chunk_index, chunk_size)` when it can derive the
index from loop IVs. Indirect reads may still carry a hint, but are handled
later in DbPartitioning.

```bash
  carts run mixed_access.mlir --concurrency > for_lowering.mlir
```

if yoy $grep "chunk_hint" for_lowering.mlir$ you will see we generate chunk hints.

6. **DbPartitioning**
This example is a mixed access pattern, one parallel for suggest chunked partitioning, and one parallel for suggest full-range partitioning.
So we should apply the H1.2 heuristic to decide the partitioning mode.
In this partitioning, the dballoc is rewritten with the chunk info of the first parallel and send only the chunk index to the first parallel, and for the second parallel, we send the full range.
Lets make sure THIS IS THE CASE.
Remember that we should stay with the chunk info of the first parallel.


7. **Execute and Verify Correctness**

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
IF something does work, investigate why, and fix it.....
If necessary run lldb to debug.