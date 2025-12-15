# Single example analysis

Walk through these steps and stop as soon as you spot something odd.

1. **Navigate to the single example:**

   ```bash
   cd ~/Documents/carts/tests/examples/parallel_for/single
   ```

2. **Rebuild and regenerate the IR artifacts (fresh inputs avoid stale crashes):**

   ```bash
   carts build
   carts cgeist parallel_for.c -O0 --print-debug-info -S --raise-scf-to-affine &> parallel_for_seq.mlir
   carts run parallel_for_seq.mlir --collect-metadata &> parallel_for_arts_metadata.mlir
   carts cgeist parallel_for.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> parallel_for.mlir
   ```

3. **OpenMP -> Arts lowering checkpoint:** stop early to make sure the OpenMP single is converted sensibly before the db pass.

   ```bash
   carts run parallel_for.mlir --openmp-to-arts &> single_openmp_to_arts.mlir
   ```
   Check how the code is converted to ARTS ops.
   ```mlir
   module attributes {..} {
   ...
   func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
      ...
      /// Parallel region
      arts.edt <parallel> <internode> route(%c0_i32) attributes {no_verify = #arts.no_verify} {
         arts.barrier
         /// Single region
         arts.edt <single> <intranode> route(%c0_i32) attributes {no_verify = #arts.no_verify} {
         ...
         }
         arts.barrier
         arts.for(%c0) to(%c100) step(%c1) {{
         ^bb0(%arg0: index):
         ...
         }}
      }
      ...
      return ... : i32
   }
   ...
  }
  ```

5. **Concurrency stage:**
   Lets analyze the output - Read the inline comments that I left in the code.
   ```bash
   carts run parallel_for.mlir --concurrency &> single_concurrency.mlir
   ```
   Check the inine comments that I left in the code.

   ```mlir
   module attributes {...} {
   ...
   func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
      ...
      /// data[100] - coarse grained allocation [outer is one and inner is 100]
      %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%c100] {...} : (memref<?xi64>, memref<?xmemref<?xi32>>)
      /// sum - single db [outer and inner dims is one]
      %guid_0, %ptr_1 = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%c1] {...} : (memref<?xi64>, memref<?xmemref<?xi32>>)
      /// Initialize sum - notice that we use a db_ref with index 0, and then we have a single element access (the 0th element). Remember that the db_ref iterates over the outer dimensions (the sizes).
      %1 = arts.db_ref %ptr_1[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
      memref.store %0, %1[%c0] : memref<?xi32>
      %2 = memref.get_global @_carts_timer_start : memref<1xf64>
      %3 = call @omp_get_wtime() : () -> f64
      ....
      /// Parallel region - Noticed that we splitted it - check the arts for lowering logic
      
      /// This is the work before the arts.for.... It contains the single region work 
      /// This will be optimized by the edt pass into a single edt during the concurrency-opt pipeline.
      %guid_2, %ptr_3 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
      %guid_4, %ptr_5 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
      arts.edt <parallel> <internode> route(%c0_i32) (%ptr_3, %ptr_5) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>> attributes {workers = #arts.workers<6>} {
      ^bb0(%arg0: memref<?xmemref<?xi32>>, %arg1: memref<?xmemref<?xi32>>):
         %guid_10, %ptr_11 = arts.db_acquire[<inout>] ( %arg0 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
         arts.edt <single> <intranode> route(%c0_i32) (%ptr_11) : memref<?xmemref<?xi32>> {
         ^bb0(%arg2: memref<?xmemref<?xi32>>):
         %25 = arts.get_current_node -> i32
         %26 = llvm.mlir.addressof @str1 : !llvm.ptr
         %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
         %28 = llvm.call @printf(%27, %25) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
         %29 = arts.db_ref %arg2[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
         %30 = memref.load %29[%c0] : memref<?xi32>
         %31 = arith.addi %30, %c1000_i32 : i32
         memref.store %31, %29[%c0] : memref<?xi32>
         arts.db_release(%arg2) : memref<?xmemref<?xi32>>
         }
      }
      /// Although we have here an empty parallel region, it will be optimized by the edt pass into a single edt during the concurrency-opt pipeline.
      /// Lets investigate WHY we created 2 parallel regions for the pre arts.for work in the ForLowering pass... 
      /// I believe this didnt have to be created....
      %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xi32>>)
      %guid_8, %ptr_9 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xi32>>)
      arts.edt <parallel> <internode> route(%c0_i32) (%ptr_7, %ptr_9) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>> attributes {workers = #arts.workers<6>} {
      ^bb0(%arg0: memref<?xmemref<?xi32>>, %arg1: memref<?xmemref<?xi32>>):
         arts.db_release(%arg0) : memref<?xmemref<?xi32>>
         arts.db_release(%arg1) : memref<?xmemref<?xi32>>
      }

      /// Epochs are used to create the tasks and the parallel_for
      %7 = arts.epoch {
         scf.for %arg0 = %c0 to %c6 step %c1 {
         ...
         /// If we have work to do, we acquire the input and output dbs and create a task.
         scf.if %31 {
            %guid_10, %ptr_11 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] offset_hints[%25] size_hints[%30] -> (memref<?xi64>, memref<?xmemref<?xi32>>)
            %guid_12, %ptr_13 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] offset_hints[%25] size_hints[%30] -> (memref<?xi64>, memref<?xmemref<?xi32>>)
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_11, %ptr_13) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>> {
            ^bb0(%arg1: memref<?xmemref<?xi32>>, %arg2: memref<?xmemref<?xi32>>):
               scf.for %arg3 = %c0 to %30 step %c1 {
               %32 = arith.addi %25, %arg3 : index
               %33 = arts.db_ref %arg2[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
               %34 = memref.load %33[%32] : memref<?xi32>
               %35 = arith.muli %34, %c2_i32 : i32
               memref.store %35, %33[%32] : memref<?xi32>
               } {...}
               arts.db_release(%arg1) : memref<?xmemref<?xi32>>
               arts.db_release(%arg2) : memref<?xmemref<?xi32>>
            }
         }
         }
      } : i64
      ...
      arts.db_free(%guid) : memref<?xi64>
      arts.db_free(%ptr) : memref<?xmemref<?xi32>>
      arts.db_free(%guid_0) : memref<?xi64>
      arts.db_free(%ptr_1) : memref<?xmemref<?xi32>>
      return %24 : i32
   }
   }
   ```
6. **Concurrency-opt checkpoint:**

   ```bash
   carts run parallel_for.mlir --concurrency-opt &> single_concurrency_opt.mlir
   ```
   Check and handle the inline comments that I left in the code.
   ```mlir
   module attributes {...} {
   func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
      ...
      /// This shouldve been optimized by the edt pass into a single edt during the concurrency-opt pipeline.
      /// Why didt it happen?
      %guid_2, %ptr_3 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
      %guid_4, %ptr_5 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
      arts.edt <parallel> <internode> route(%c0_i32) (%ptr_3, %ptr_5) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>> attributes {workers = #arts.workers<6>} {
      ^bb0(%arg0: memref<?xmemref<?xi32>>, %arg1: memref<?xmemref<?xi32>>):
         %guid_6, %ptr_7 = arts.db_acquire[<inout>] ( %arg0 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
         arts.edt <single> <intranode> route(%c0_i32) (%ptr_7) : memref<?xmemref<?xi32>> {
         ^bb0(%arg2: memref<?xmemref<?xi32>>):
         %25 = arts.get_current_node -> i32
         %26 = llvm.mlir.addressof @str1 : !llvm.ptr
         %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
         %28 = llvm.call @printf(%27, %25) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
         %29 = arts.db_ref %arg2[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
         %30 = memref.load %29[%c0] : memref<?xi32>
         %31 = arith.addi %30, %c1000_i32 : i32
         memref.store %31, %29[%c0] : memref<?xi32>
         arts.db_release(%arg2) : memref<?xmemref<?xi32>>
         }
      }
      /// This seems correct
      %7 = arts.epoch {
         scf.for %arg0 = %c0 to %c6 step %c1 {
         %25 = arith.muli %arg0, %c17 : index
         %26 = arith.cmpi uge, %25, %c100 : index
         %27 = arith.subi %c100, %25 : index
         %28 = arith.select %26, %c0, %27 : index
         %29 = arith.minui %28, %c17 : index
         %30 = arith.minui %29, %28 : index
         %31 = arith.cmpi ugt, %30, %c0 : index
         scf.if %31 {
            %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] offset_hints[%25] size_hints[%30] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
            %guid_8, %ptr_9 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) offsets[%c0] sizes[%c1] offset_hints[%25] size_hints[%30] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<?xi32>>)
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_7, %ptr_9) : memref<?xmemref<?xi32>>, memref<?xmemref<?xi32>> {
            ^bb0(%arg1: memref<?xmemref<?xi32>>, %arg2: memref<?xmemref<?xi32>>):
               scf.for %arg3 = %c0 to %30 step %c1 {
               %32 = arith.addi %25, %arg3 : index
               %33 = arts.db_ref %arg2[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
               %34 = memref.load %33[%32] : memref<?xi32>
               %35 = arith.muli %34, %c2_i32 : i32
               memref.store %35, %33[%32] : memref<?xi32>
               } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, readCount = 1 : i64, writeCount = 1 : i64, tripCount = 100 : i64, nestingLevel = 0 : i64, hasUniformStride = true, hasGatherScatter = false, dataMovementPattern = 4 : i64, suggestedPartitioning = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "parallel_for.c:27:23">}
               arts.db_release(%arg1) : memref<?xmemref<?xi32>>
               arts.db_release(%arg2) : memref<?xmemref<?xi32>>
            }
         }
         }
      } : i64
      ...
      arts.db_free(%guid) : memref<?xi64>
      arts.db_free(%ptr) : memref<?xmemref<?xi32>>
      arts.db_free(%guid_0) : memref<?xi64>
      arts.db_free(%ptr_1) : memref<?xmemref<?xi32>>
      return %24 : i32
   }
   ...
   }
   ```

7. **Pre-lowering (ParallelEDTLowering / EpochLowering):**

   ```bash
   carts run parallel_for.mlir --pre-lowering --debug-only=parallel_edt_lowering &> single_pre_lowering.mlir
   ```


8. **Execute to run the code:**

   ```bash
   carts execute parallel_for.c -O3
   ./parallel_for 8 &> output
   ```

---
