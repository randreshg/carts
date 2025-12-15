# Parallel example analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the parallel example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/parallel
   ```

2. **Build and generate the MLIR inputs:**

   ```bash
   carts build
   carts cgeist parallel.c -O0 --print-debug-info -S --raise-scf-to-affine &> parallel_seq.mlir
   carts run parallel_seq.mlir --collect-metadata &> parallel_arts_metadata.mlir
   carts cgeist parallel.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> parallel.mlir
   ```

   `parallel_seq.mlir` shows the sequential shape, `parallel_arts_metadata.mlir` keeps the collected metadata, and `parallel.mlir` is the OpenMP input for the pipeline.

3. **Check the concurrency pipeline:**
   ```bash
   carts run parallel.mlir --concurrency &> parallel_concurrency.mlir
   carts run parallel.mlir --concurrency-opt &> parallel_concurrency_opt.mlir
   carts run parallel.mlir --arts-to-llvm &> parallel_llvm.mlir
   ```

   Or run the full program:

   ```bash
   carts execute parallel.c
   ./parallel
   ```
