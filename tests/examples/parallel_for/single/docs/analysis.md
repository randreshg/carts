# Single example analysis

Walk through these steps and stop as soon as you spot something odd.

1. **Navigate to the single example:**

   ```bash
   cd /opt/carts/tests/examples/parallel_for/single
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

4. **Concurrency stage:**
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
5. **Concurrency-opt checkpoint (DbAllocPromotion - Chunked Mode):**

   The concurrency-opt pass applies DbAllocPromotion to transform coarse-grained allocations
   to chunked allocations for better parallelism. This is where coordinate localization happens.

   ```bash
   carts run parallel_for.mlir --concurrency-opt &> single_concurrency_opt.mlir
   ```

   **Key transformation: Coarse-grained → Chunked allocation**

   The array `data[100]` is transformed from a single datablock to 6 chunks of ~17 elements each:
   - Before: `sizes[%c1] elementSizes[%c100]` (1 outer × 100 elements)
   - After: `sizes[%c6] elementSizes[%c17]` (6 outer × 17 elements)

   ```mlir
   module attributes {...} {
   func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
      ...
      /// Chunked allocation: 6 chunks of 17 elements each (covers 100 elements total)
      /// DbAllocPromotion transforms sizes[1] elementSizes[100] → sizes[6] elementSizes[17]
      %guid, %ptr = arts.db_alloc[...] sizes[%c6] elementType(i32) elementSizes[%c17] {...}

      /// Single task for the "single" region (unchanged from before)
      %guid_2, %ptr_3 = arts.db_acquire[<inout>] (%guid_0 : ...) offsets[%c0] sizes[%c1] {...}
      arts.edt <task> <intranode> route(%c0_i32) (%ptr_3) : memref<?xmemref<?xi32>> {
      ^bb0(%arg0: memref<?xmemref<?xi32>>):
         /// db_ref[%c0] returns the single chunk (not transformed - already local)
         %30 = arts.db_ref %arg0[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
         %31 = memref.load %30[%c0] : memref<?xi32>
         %32 = arith.addi %31, %c1000_i32 : i32
         memref.store %32, %30[%c0] : memref<?xi32>
         arts.db_release(%arg0) : memref<?xmemref<?xi32>>
      }

      /// Parallel for loop - 6 iterations, one per chunk
      %7 = arts.epoch {
         scf.for %arg0 = %c0 to %c6 step %c1 {
         /// %26 = element offset for this chunk (0, 17, 34, 51, 68, 85)
         %26 = arith.muli %arg0, %c17 : index
         /// Compute actual work size for this chunk (handles last partial chunk)
         %27 = arith.cmpi uge, %26, %c100 : index
         %28 = arith.subi %c100, %26 : index
         %29 = arith.select %27, %c0, %28 : index
         %30 = arith.minui %29, %c17 : index
         %31 = arith.minui %30, %29 : index
         %32 = arith.cmpi ugt, %31, %c0 : index
         scf.if %32 {
            /// Acquire uses offset_hints[%26] = element-space offset for memref localization
            /// %33 = chunk index (0, 1, 2, 3, 4, 5) - used for db_ref
            %33 = arith.divui %26, %c17 : index
            %34 = arith.addi %31, %c16 : index
            %35 = arith.divui %34, %c17 : index
            /// offsets[%33] sizes[%35] = chunk-space coordinates for outer datablock access
            /// offset_hints[%26] = element-space coordinates for memref index localization
            %guid_6, %ptr_7 = arts.db_acquire[<inout>] (%guid : ...) offsets[%33] sizes[%35] offset_hints[%26] size_hints[%31] {...}
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_5, %ptr_7) : ... {
            ^bb0(%arg1: memref<?xmemref<?xi32>>, %arg2: memref<?xmemref<?xi32>>):
               /// Inside EDT: %arg2 is the acquired view (already localized to this chunk)
               scf.for %arg3 = %c0 to %31 step %c1 {
                  /// db_ref[%c0] gives local chunk view (chunk index is 0 within the acquired view)
                  %36 = arts.db_ref %arg2[%c0] : memref<?xmemref<?xi32>> -> memref<?xi32>
                  /// CRITICAL: memref indices use LOCAL coordinates (%arg3), NOT global (%26+%arg3)
                  /// DbAllocPromotion::localizeElementIndices() transforms global → local
                  /// Pattern: global = %26 + %arg3, offset_hint = %26, so local = %arg3
                  %37 = memref.load %36[%arg3] : memref<?xi32>
                  %38 = arith.muli %37, %c2_i32 : i32
                  memref.store %38, %36[%arg3] : memref<?xi32>
               } {...}
               arts.db_release(%arg1) : memref<?xmemref<?xi32>>
               arts.db_release(%arg2) : memref<?xmemref<?xi32>>
            }
         }
         }
      } : i64
      ...
      /// Access verification uses chunked coordinates
      %17 = arts.db_ref %ptr[%c0] : ... -> memref<?xi32>  /// chunk 0, element 0 → global 0
      %18 = memref.load %17[%c0] : memref<?xi32>
      %19 = arts.db_ref %ptr[%c5] : ... -> memref<?xi32>  /// chunk 5, element 14 → global 99
      %20 = memref.load %19[%c14] : memref<?xi32>
      ...
      return %25 : i32
   }
   }
   ```

   **Coordinate Localization Summary (DbAllocPromotion class):**

   | Coordinate Space | Example (chunk 1) | Used For |
   |------------------|-------------------|----------|
   | Global element   | 17, 18, 19, ...   | Original source code indices |
   | Chunk index      | 1                 | db_ref outer index (offsets[]) |
   | Local element    | 0, 1, 2, ...      | memref.load/store inside EDT |

   The `offset_hints` attribute preserves element-space offsets so that
   `localizeElementIndices()` can transform global memref indices to local ones
   inside EDT bodies. This is handled by `DbAllocPromotion::rebaseEdtUsers()`.

6. **Pre-lowering (ParallelEDTLowering / EpochLowering):**

   ```bash
   carts run parallel_for.mlir --pre-lowering --debug-only=parallel_edt_lowering &> single_pre_lowering.mlir
   ```

7. **Execute to run the code:**

   ```bash
   carts execute parallel_for.c -O3
   ./parallel_for 8 &> output
   ```

8. **Test with carts examples:**
   ```bash
   carts examples run parallel_for/single --trace
   ```

---

## Bug Fix History

### Chunked Mode Coordinate Localization Bug (Fixed)

**Problem:** Runtime crash (SIGSEGV) when accessing chunked datablocks inside EDT bodies.

**Root Cause:** After DbAllocPromotion transformed coarse-grained allocations to chunked form,
memref.load/store operations inside EDT bodies still used global element indices instead of
local indices relative to the acquired chunk.

**Example of the bug:**
```mlir
/// Chunk 1 starts at global element 17, contains elements 0-16 locally
scf.for %arg3 = %c0 to %31 step %c1 {
  %36 = arith.addi %26, %arg3 : index     /// %26=17, so %36 = 17, 18, 19, ...
  %37 = arts.db_ref %arg2[%c0] : ...      /// Returns local chunk view (0-16)
  %38 = memref.load %37[%36] : ...        /// BUG: accessing index 17+ on chunk with only 0-16!
}
```

**Fix:** Added `localizeElementIndices()` method to `DbAllocPromotion` class that transforms
global memref indices to local coordinates using the `offset_hints` attribute:

```mlir
/// After fix - memref indices are properly localized
scf.for %arg3 = %c0 to %31 step %c1 {
  %36 = arts.db_ref %arg2[%c0] : ...
  %37 = memref.load %36[%arg3] : ...      /// FIXED: uses local index directly (0, 1, 2, ...)
}
```

**Files Modified:**
- `/opt/carts/include/arts/Transforms/DbTransforms.h` - Added `elementOffsets` to AcquireViewMap, added `localizeElementIndices()` declaration
- `/opt/carts/lib/arts/Transforms/DbTransforms.cpp` - Implemented `localizeElementIndices()`, extended `rebaseEdtUsers()` to handle memref ops

**Key insight:** The `offset_hints` attribute must preserve element-space offsets (not chunk-space)
so that memref indices can be correctly localized inside EDT bodies.
