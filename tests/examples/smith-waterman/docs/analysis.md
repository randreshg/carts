# Smith Waterman example analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the smith waterman example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/smith-waterman
   ```

2. **Build and generate the MLIR inputs:**

   ```bash
   carts build
   carts cgeist smith-waterman.c -O0 --print-debug-info -S --raise-scf-to-affine &> smith_waterman_seq.mlir
   carts run smith-waterman_seq.mlir --collect-metadata &> smith_waterman_arts_metadata.mlir
   carts cgeist smith-waterman.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> smith-waterman.mlir
   ```
3. **Create DBs with control DBs:**

   Use the canonicalized file to drive DB creation and dump debug info:

   ```bash
   carts run smith-waterman.mlir --create-dbs --debug-only=create_dbs &> smith-waterman_create_dbs.mlir
   ```

   In `smith-waterman_create_dbs.mlir`, verify:
   - `arts.db_alloc` for the score matrices are fine grained (sizes correspond to the 2D shape, e.g., `sizes[%len1, %len2]` with `elementSizes[%c1]`), not coarse `[1]` allocations.
   - Something important here is that the string should remained coarse-grained.

   Or run the full program:

   ```bash
   carts execute matrix.c
   ./matrix 8
   ```

   If execution fails, analyze the code..
