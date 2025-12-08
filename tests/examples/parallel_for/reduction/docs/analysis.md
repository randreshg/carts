# Reduction example analysis

Walk through these steps and stop as soon as you spot something odd.

1. **Navigate to the reduction example:**

   ```bash
   cd ~/Documents/carts/tests/examples/parallel_for/reduction
   ```

2. **Rebuild and regenerate the IR artifacts (fresh inputs avoid stale crashes):**

   ```bash
   carts build
   carts cgeist parallel_for.c -O0 --print-debug-info -S --raise-scf-to-affine &> parallel_for_seq.mlir
   carts run parallel_for_seq.mlir --collect-metadata &> parallel_for_seq_metadata.mlir
   carts cgeist parallel_for.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> parallel_for.mlir
   ```

   Peek at `.carts-metadata.json` to keep the loop/memref ids handy while debugging. For reference: loop `27` (init a/b), loop `34` (parallel add into c), loop `42` (the reduction), memrefs `17/20/21` (a/b/c).

3. **Baseline sanity (sequential IR):** skim `parallel_for_seq_metadata.mlir` and confirm the reduction loop is tagged with `hasReductions = true` and `reductionKinds = ["add"]`, and that the identity is zero. This gives you the expected shape of the computation before any arts transforms.

4. **OpenMP -> Arts lowering checkpoint:** stop early to make sure the OpenMP reduction is converted sensibly before the db pass.

   ```bash
   carts run parallel_for.mlir --openmp-to-arts &> reduction_openmp_to_arts.mlir
   ```

   Check that:
   - The worksharing loop for `c[i] = a[i] + b[i]` becomes an `arts.for` with the same chunking hints as the original `schedule(static,4)`.

   Example (what you should see around the reduction combine right after OpenMP->Arts):
   ```mlir
   %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>] ... elementType(i32)
   %acc = arts.db_ref %ptr[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
   memref.store %c0_i32, %acc[%c0] : memref<?xi32>   /// identity = 0
   ...
   arts.for(%c0) to(%N) step(%c1) reduction(%alloca : memref<1xi32>) {{
   ^bb0(%iv: index):
     %val = memref.load %c_buf[%iv] : memref<?xi32>
     %old = memref.load %acc[%c0] : memref<?xi32>
     /// combines into result
     memref.store (addi %old, %val), %acc[%c0] : memref<?xi32> 
   }}
   ```

5. **Concurrency stage:**
   Lets analyze the output - Read the inline comments that I left in the code.
   ```bash
   carts run parallel_for.mlir --concurrency &> reduction_concurrency.mlir
   ```

   I will provide a summarized version of the module after all finished. 
   ```mlir
      module attributes {...} {
      ...
      func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
         ...
         /// Allocate reduction result db
         %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%c1] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
         %1 = arts.db_ref %ptr[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
         memref.store %0, %1[%c0] : memref<?xi32>
         ...
         %5:2 = scf.if %3 -> (i1, i32) {
            ...
            /// Allocate input dbs - coarse grained allocations
            %guid_0, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%12] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            ...
            /// Acquire input dbs... coarse grained acquires - In next stages we will use fine grained acquires.
            %guid_2, %ptr_3 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] {arts.twin_diff = true} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            %guid_4, %ptr_5 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            /// Parallel work before the arts.for loop. This will me removed later on by the edt pass in the concurrency-opt pipeline.
            arts.edt <parallel> <intranode> route(%c0_i32) (%ptr_5) : memref<?xmemref<memref<?xi32>>> attributes {workers = #arts.workers<8>} {
            ^bb0(%arg2: memref<?xmemref<memref<?xi32>>>):
            arts.db_release(%arg2) : memref<?xmemref<memref<?xi32>>>
            }
            /// Epochs are used to create the tasks and the parallel_for
            %19 = arts.epoch {
            scf.for %arg2 = %c0 to %c8 step %c1 {
               /// Work partitioning logic.
               %41 = arith.addi %15, %c3 : index
               ...
               %56 = arith.cmpi ugt, %55, %c0 : index
               /// If the current worker has work to do, we acquire the input dbs and create a task.
               scf.if %56 {
                  /// Acquire input dbs - fine grained acquires. The dbMode is wrong here, but it should be fixed in the next stages.
                  %guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] offset_hints[%50] size_hints[%55] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  arts.edt <task> <intranode> route(%c0_i32) (%ptr_13) : memref<?xmemref<memref<?xi32>>> {
                  ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>):
                  scf.for %arg4 = %c0 to %55 step %c1 {
                     %57 = arith.addi %50, %arg4 : index
                     %58 = arith.index_cast %57 : index to i32
                     %59 = arith.muli %58, %c2_i32 : i32
                     %60 = arith.addi %59, %14 : i32
                     %61 = arith.addi %58, %60 : i32
                     %62 = arts.db_ref %arg3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                     memref.store %61, %62[%57] : memref<?xi32>
                  } {...}
                  arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
                  }
               }
            }
            } : i64
            ...
            memref.store %c0_i32, %1[%c0] : memref<?xi32>
            /// Parallel region for the arts.for
            /// Create partial reduction dbs - fine grained allocations - one for each worker.
            %guid_6, %ptr_7 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%c8] elementType(i32) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            scf.for %arg2 = %c0 to %c8 step %c1 {
            %41 = arts.db_ref %ptr_7[%arg2] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            memref.store %c0_i32, %41[%c0] : memref<?xi32>
            } {...}
            /// Work before the reduction loop - this will be removed later on by the edt pass in the concurrency-opt pipeline.
            %guid_8, %ptr_9 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            %guid_10, %ptr_11 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            arts.edt <parallel> <intranode> route(%c0_i32) (%ptr_9, %ptr_11) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>> attributes {workers = #arts.workers<8>} {
            ^bb0(%arg2: memref<?xmemref<memref<?xi32>>>, %arg3: memref<?xmemref<memref<?xi32>>>):
            arts.db_release(%arg2) : memref<?xmemref<memref<?xi32>>>
            arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
            }
            /// Epochs are used to create the tasks and the reduction loop.
            %23 = arts.epoch {
            scf.for %arg2 = %c0 to %c8 step %c1 {
               /// Work partitioning logic.
               %41 = arith.addi %15, %c7 : index
               ...
               %49 = arith.cmpi ugt, %48, %c0 : index
               /// If the current worker has work to do, we acquire the partial reduction dbs and create a task.
               scf.if %49 {
                  /// Acquire partial reduction dbs - fine grained acquire...
                  %guid_16, %ptr_17 = arts.db_acquire[<inout>] (%guid_6 : memref<?xi64>, %ptr_7 : memref<?xmemref<memref<?xi32>>>) offsets[%arg2] sizes[%c1] offset_hints[%arg2] size_hints[%c1] -> (memref<?xi64>, memref<memref<?xi32>>)
                  /// Acquire input Dbs... 
                  %guid_18, %ptr_19 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] offset_hints[%43] size_hints[%48] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  arts.edt <task> <intranode> route(%c0_i32) (%ptr_19, %ptr_17) : memref<?xmemref<memref<?xi32>>>, memref<memref<?xi32>> {
                  ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>, %arg4: memref<memref<?xi32>>):
                  scf.for %arg5 = %c0 to %48 step %c1 {
                     %50 = arts.db_ref %arg4[%c0] {operandSegmentSizes = array<i32: 1, 1>} : memref<memref<?xi32>> -> memref<?xi32>
                     %51 = arith.addi %43, %arg5 : index
                     %52 = arts.db_ref %arg3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                     %53 = memref.load %52[%51] : memref<?xi32>
                     %54 = memref.load %50[%c0] : memref<?xi32>
                     %55 = arith.addi %54, %53 : i32
                     memref.store %55, %50[%c0] : memref<?xi32>
                  } {...}
                  arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
                  arts.db_release(%arg4) : memref<memref<?xi32>>
                  }
               }
            }
            /// Acquire the reduction result db - we acquire ALL the partial reduction dbs and create a task.
            %guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_6 : memref<?xi64>, %ptr_7 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c8] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            /// Acquire the reduction result db
            %guid_14, %ptr_15 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_13, %ptr_15) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>> {
            ^bb0(%arg2: memref<?xmemref<memref<?xi32>>>, %arg3: memref<?xmemref<memref<?xi32>>>):
               %41 = arts.db_ref %arg3[%c0] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
               %42 = scf.for %arg4 = %c0 to %c8 step %c1 iter_args(%arg5 = %c0_i32) -> (i32) {
                  %43 = arts.db_ref %arg2[%arg4] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  %44 = memref.load %43[%c0] : memref<?xi32>
                  %45 = arith.addi %arg5, %44 : i32
                  scf.yield %45 : i32
               } {...}
               memref.store %42, %41[%c0] : memref<?xi32>
               arts.db_release(%arg2) : memref<?xmemref<memref<?xi32>>>
               arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
            }
            } : i64
            arts.db_free(%guid_6) : memref<?xi64>
            arts.db_free(%ptr_7) : memref<?xmemref<memref<?xi32>>>
            arts.db_free(%guid) : memref<?xi64>
            arts.db_free(%ptr) : memref<?xmemref<memref<?xi32>>>
            ...
            ...
            arts.db_free(%guid_0) : memref<?xi64>
            arts.db_free(%ptr_1) : memref<?xmemref<memref<?xi32>>>
            scf.yield %39, %40 : i1, i32
         } else {
            scf.yield %false, %4 : i1, i32
         }
         %6 = arith.select %5#0, %c0_i32, %5#1 : i32
         scf.if %5#0 {
            ...
         }
         arts.db_free(%guid) : memref<?xi64>
         arts.db_free(%ptr) : memref<?xmemref<memref<?xi32>>>
         return %6 : i32
      }
      }
   ```

6. **Concurrency-opt checkpoint:**

   ```bash
   carts run parallel_for.mlir --concurrency-opt --debug-only=db &> reduction_concurrency_opt.mlir
   ```
   Lets investigate what is happening in the db pass. Remember that you need to check the second run of the dbpass, the one that is after the concurrency-opt pass.
   Right now we are having some problems with the dbref ioperations
   loc("/Users/randreshg/Documents/carts/tests/examples/parallel_for/reduction/parallel_for.c":44:14): error: 'arts.db_ref' op Coarse-grained datablock expects db_ref indices to be constant zero
   %75 = "arts.db_ref"(%arg3, %72) : (memref<?xmemref<memref<?xi32>>>, index) -> memref<?xi32>

   After promotion I'd expect the dballoc, acquires and refs to be fine grained, so it seems fine, the problem seems that our dbref vericiation is too strict - we need to substract the offset but what because some SSA reasons we can not set it to zero? even if at runtime it becomes zero, at compile time it might not be zero, then our verification fails. no ? 

   Check that in DbTransforms::applyOffsets if the SSA values are equal to the offsets, we set them to zero. 

7. **Pre-lowering (ParallelEDTLowering / EpochLowering):**

   ```bash
   carts run parallel_for.mlir --pre-lowering --debug-only=parallel_edt_lowering &> reduction_pre_lowering.mlir
   ```


8. **Execute to reproduce the failure (after any change):**

   ```bash
   carts execute parallel_for.c -O3
   ./parallel_for 8 &> output
   ```

   If the optimized run still diverges, cross-check the failing IR stage above to see where the accumulator loses updates or where an acquire is dropped.

---

## Pipeline Flow for Reduction Handling

### Expected Flow

1. **CreateDbs**: Creates coarse-grained datablocks by default

2. **ForLowering**:
   - Acquires coarse-grained DBs but adds `offset_hints/size_hints`
   - Partial accumulators are already fine-grained (one per worker)
   - Reductions are handled specially via partial accumulators + final combine
   - `hasReductions`/`hasInterIterationDeps` on the loop should NOT block
     partitioning of INPUT arrays (they're read-only in the reduction)

3. **DbPass (concurrency-opt)**:
   - Analyzes if partitioning is possible using metadata + analysis
   - **If YES**: Promotes alloc, updates acquire offsets/sizes, adjusts db_ref indices
   - **If NO**: Must keep coarse-grained (constant 0 db_ref indices)

### Key Invariant

**If a DB can't be partitioned, its db_refs MUST use constant 0 indices.**

This invariant ensures verification passes. If DbPass skips partitioning for an acquire
(e.g., due to detected loop-carried dependencies), the db_ref operations must remain
coarse-grained with constant index 0. Dynamic indices are only valid after the DbPass
has promoted the acquire to fine-grained and adjusted the indices accordingly.

### Per-Memref Dependency Analysis

The `MemrefAnalyzer::hasLoopCarriedDependencies()` function uses per-memref dependency
analysis (via `DependenceAnalyzer::hasLoopCarriedDeps()`) instead of loop-level metadata.
This correctly handles reductions where:
- The loop has `hasInterIterationDeps = true` (due to the reduction variable)
- But the input arrays are read-only and have NO loop-carried dependencies

For example, in `sum += c[i]`, the scalar `sum` has loop-carried deps, but array `c`
is read-only and can be safely partitioned across workers.
