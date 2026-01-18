# Matrix deps example analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the matrix example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/matrix
   ```

2. **Build and generate the MLIR inputs:**

   ```bash
   carts build
   carts cgeist matrix.c -O0 --print-debug-info -S --raise-scf-to-affine &> matrix_seq.mlir
   carts run matrix_seq.mlir --collect-metadata &> matrix_arts_metadata.mlir
   carts cgeist matrix.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> matrix.mlir
   ```

   `matrix_seq.mlir` shows the sequential shape, `matrix_arts_metadata.mlir` keeps the collected metadata, and `matrix.mlir` is the OpenMP input for the pipeline.

3. **Canonicalize memrefs (array-of-pointers → shaped memrefs):**

   Run the pipeline up to canonicalization and capture the result:

   ```bash
   carts run matrix.mlir --canonicalize-memrefs &> matrix_canon.mlir
   ```

   Inspect `matrix_canon.mlir` to confirm the array-of-pointers form has been rewritten into a memref with explicit dimensions. For example, the OpenMP input carries `memref<?xmemref<?xf64>>` to represent `double**`; after canonicalization you should see a contiguous shape such as `memref<?x?xf64>` (or equivalent casts) and accesses of the form:

   ```
   /// Before: pointer-chasing per row/col
   %row = memref.load %A[%i] : memref<?xmemref<?xf64>>
   %elt = memref.load %row[%j] : memref<?xf64>

   /// After: direct 2D memref with clear sizes
   %elt = memref.load %A_flat[%i, %j] : memref<?x?xf64>
   ```

   Make sure loop bounds (`%i`, `%j`) match the memref extents, that casts collapse/expand shapes consistently, and that there are no remaining `memref<?xmemref<?xf64>>` users before continuing.

4. **Create DBs with control DBs (expect fine-grained allocations):**

   Use the canonicalized file to drive DB creation and dump debug info:

   ```bash
   carts run matrix.mlir --create-dbs --debug-only=db &> matrix_create_dbs.mlir
   ```

   In `matrix_create_dbs.mlir`, verify:
   - `arts.db_alloc` for matrices A and B are fine grained (sizes correspond to the 2D shape, e.g., `sizes[%N, %N]` with `elementSizes[%c1]`), not coarse `[1]` allocations.
   - `arts.db_acquire` uses indices/offsets aligned with the task dependencies: tasks that write one element should acquire with `indices[%i, %j]` (or offsets + sizes of 1 on each dimension) and have `arts.twin_diff = false` when control DBs prove disjointness.
   - `arts.db_ref`/`memref.load`/`memref.store` patterns operate on the inner dimension (element) while the acquire/ref index walks the outer dimensions.
   - Control DB analysis should give offset/size hints matching each task slice; if you still see coarse acquires with `[0]`/`[1]`, investigate missing canonicalization or control-db metadata.

5. **Continue or run end-to-end:**

   To check later stages, keep using the stop flags:

   ```bash
   carts run matrix.mlir --concurrency --debug-only=db &> matrix_concurrency.mlir
   carts run matrix.mlir --concurrency-opt --debug-only=db &> matrix_concurrency_opt.mlir
   carts run matrix.mlir --arts-to-llvm &> matrix_llvm.mlir
   ```

   Or run the full program:

   ```bash
   carts execute matrix.c
   ./matrix 8
   ```

   If execution fails, re-check `matrix_canon.mlir` for any remaining array-of-pointer accesses and `matrix_create_dbs.mlir` for coarse-grained DBs that should have been refined by control DBs.
