# matrixmul example analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the matrixmul example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/matrixmul
   ```

2. **Build and generate the MLIR inputs:**

   ```bash
   carts build
   carts cgeist matrixmul.c -O0 --print-debug-info -S --raise-scf-to-affine &> matrix_seq.mlir
   carts run matrix_seq.mlir --collect-metadata &> matrix_arts_metadata.mlir
   carts cgeist matrixmul.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> matrixmul.mlir
   ```

   `matrix_seq.mlir` shows the sequential shape, `matrix_arts_metadata.mlir` keeps the collected metadata, and `matrixmul.mlir` is the OpenMP input for the pipeline.

3. **Canonicalize memrefs (array-of-pointers → shaped memrefs):**

   Run the pipeline up to canonicalization and capture the result:

   ```bash
   carts run matrixmul.mlir --canonicalize-memrefs &> matrix_canon.mlir
   ```

   Check the code for memref allocs like:

   ```mlir
   %alloc = memref.alloc(%15, %18) : memref<?x?xf32>
   %alloc_1 = memref.alloc(%15, %18) : memref<?x?xf32>
   %alloc_2 = memref.alloc(%15, %18) : memref<?x?xf32>
   %alloc_3 = memref.alloc(%15, %18) : memref<?x?xf32>
   ```

   The sizes should be the dimensions of the matrix.

4. **Create DBs with control DBs (expect coarse allocations + partition hints):**

   Use the canonicalized file to drive DB creation and dump debug info:

   ```bash
   carts run matrixmul.mlir --create-dbs --debug-only=create_dbs &> matrix_create_dbs.mlir
   ```

   At this stage, `db_alloc` stays coarse (`sizes=[1]`) and partition hints
   live in `db_acquire` (`partition_indices/offsets/sizes`).

5. **Continue or run end-to-end:**

   To check later stages, keep using the stop flags:

   ```bash
   carts run matrixmul.mlir --concurrency &> matrix_concurrency.mlir
   carts run matrixmul.mlir --concurrency-opt &> matrix_concurrency_opt.mlir
   carts run matrixmul.mlir --arts-to-llvm &> matrix_llvm.mlir
   ```

   Or run the full program:

   ```bash
   carts execute matrixmul.c
   ./matrixmul 8
   ```

   If execution fails, re-check `matrix_canon.mlir` for any remaining array-of-pointer accesses and `matrix_concurrency_opt.mlir` for missing partitioning (CreateDbs outputs are expected to remain coarse).
