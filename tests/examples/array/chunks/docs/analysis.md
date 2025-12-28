# Array chunks example analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the array example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/array/chunks
   ```

2. **Build carts if any changes were made:**

   ```bash
   carts build
   ```

   If there is no array.mlir run:

   ```bash
      carts cgeist array.c -O0 --print-debug-info -S --raise-scf-to-affine &> array_seq.mlir
      carts run array_seq.mlir --collect-metadata &> array_arts_metadata.mlir
      carts cgeist array.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> array.mlir
   ```
2. **Canonicalize memrefs (Omp deps canonicalization):**
    ```bash
      carts run array.mlir --canonicalize-memrefs &> array_canonicalize_memrefs.mlir
    ```
    Notice we generated arts.omp_dep operations for the OpenMP dependencies.
    Sizes different from 1 are the chunked dimensions.
    For example:
    ```mlir
    %15 = arts.omp_dep<<in>> %alloc[%arg2] size[%c4] : memref<?xf64> -> memref<?xf64>
    %16 = arts.omp_dep<<out>> %alloc_0[%arg2] size[%c4] : memref<?xf64> -> memref<?xf64>
    ```

3. **Run the pipeline and stop after any stage**
    Remember that you can also debug any pass. For example to stop at create_db pass

   Check for example the create db pass
   ```bash
      carts run array.mlir --create-dbs --debug-only=create_dbs &> array_create_dbs.mlir
   ```
   Notice how we allocated the DBs in a chunked manner.

4. **Carts execute the example:**
    ```bash
      carts execute array.c -O3
    ```
    Notice how we allocated the DBs in a chunked manner.
