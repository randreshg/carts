# Convolution example analysis

Walk through these steps and fix any problem that you find in the way.

1. **Navigate to the convolution example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/convolution
   ```

2. **Build and generate the MLIR inputs:**

   ```bash
   carts build
   carts cgeist convolution.cpp -O0 --print-debug-info -S --raise-scf-to-affine > deps_seq.mlir
   carts run deps_seq.mlir --collect-metadata > deps_arts_metadata.mlir
   carts cgeist convolution.cpp -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine > convolution.mlir
   ```

3. **Create DBs with control DBs (expect coarse allocations + partition hints):**

   Use the canonicalized file to drive DB creation and dump debug info:

   ```bash
   carts run convolution.mlir --create-dbs --debug-only=create_dbs &> convolution_create_dbs.mlir
   ```

   Analyze the comments within the summarized output.

   ```mlir
   module attributes {...} {
      ...
      func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
         ...
         /// Allocate A
         %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
         %0 = llvm.mlir.undef : i32
         %1 = arts.db_ref %ptr[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
         memref.store %0, %1[%c0] : memref<?xi32>
         /// Allocate B
         %guid_0, %ptr_1 = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
         %2 = arts.db_ref %ptr_1[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
         memref.store %0, %2[%c0] : memref<?xi32>
         ...
         %12 = arts.epoch {
            ...
            /// Acquire A
            %guid_2, %ptr_3 = arts.db_acquire[<inout>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1]  -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_3) : memref<?xmemref<memref<?xi32>>> {
            ^bb0(%arg0: memref<?xmemref<memref<?xi32>>>):
            ...
            %34 = arts.db_ref %arg0[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            memref.store %33, %34[%c0] : memref<?xi32>
            ...
            /// Print A
            %37 = llvm.call @printf(%36, %33) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
            arts.db_release(%arg0) : memref<?xmemref<memref<?xi32>>>
            }
            /// Why are we acquiring the acquire? we shouldve acuired the db alloc... we only acquire another acquire if we are within an edt...
            /// We acquire a db that an edt acquired already
            /// This is an error
            %guid_4, %ptr_5 = arts.db_acquire[<inout>] ( %ptr_3 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1]  -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            /// Acquire B - This is perfect
            %guid_6, %ptr_7 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1]  -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_5, %ptr_7) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>> {
            ^bb0(%arg0: memref<?xmemref<memref<?xi32>>>, %arg1: memref<?xmemref<memref<?xi32>>>):
            ....
            arts.db_release(%arg0) : memref<?xmemref<memref<?xi32>>>
            arts.db_release(%arg1) : memref<?xmemref<memref<?xi32>>>
            }
            /// Same problem here... we shouldve acquired the db alloc...
            %guid_8, %ptr_9 = arts.db_acquire[<inout>] ( %ptr_7 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1]  -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_9) : memref<?xmemref<memref<?xi32>>> {
            ^bb0(%arg0: memref<?xmemref<memref<?xi32>>>):
            ...
            %36 = llvm.call @printf(%35, %33) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
            arts.db_release(%arg0) : memref<?xmemref<memref<?xi32>>>
            }
         } : i64
         ...
         arts.db_free(%guid_0) : memref<?xi64>
         arts.db_free(%ptr_1) : memref<?xmemref<memref<?xi32>>>
         arts.db_free(%guid) : memref<?xi64>
         arts.db_free(%ptr) : memref<?xmemref<memref<?xi32>>>
         return %28 : i32
      }
      ...
      }
   ```

4. **Continue or run end-to-end:**

   To check later stages, keep using the stop flags:

   ```bash
   carts run convolution.mlir --concurrency --debug-only=db &> convolution_concurrency.mlir
   carts run convolution.mlir --concurrency-opt --debug-only=db_partitioning &> convolution_concurrency_opt.mlir
   carts run convolution.mlir --arts-to-llvm &> convolution_llvm.mlir
   ```

   Or run the full program:

   ```bash
   carts execute -O3 convolution.cpp
   ./convolution_arts
   ```

   If execution fails, re-check `convolution_create_dbs.mlir` for missing partition hints and `convolution_concurrency_opt.mlir` for missing partitioning (CreateDbs outputs are expected to remain coarse).
