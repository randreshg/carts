# Dotproduct example analysis

Walk through these steps and check any problem that you find in the way
1. **Navigate to the dotproduct example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/dotproduct
   ```

2. **Build carts if any changes were made:**

   ```bash
   carts build
   ```

   If there is no dotproduct.mlir run:

   ```bash
      carts cgeist dotproduct.c -O0 --print-debug-info -S --raise-scf-to-affine &> dotproduct_seq.mlir
      carts run dotproduct_seq.mlir --collect-metadata &> dotproduct_metadata.mlir
      carts cgeist dotproduct.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> dotproduct.mlir
   ```
   Check in here the carts metadata file: .carts-metadata.json

3. **Run the pipeline and stop after any stage**
    Remember that you can also debug any pass. For example to stop at the concurrency stage and debug the for_lowering pass
   ```bash
   carts run dotproduct.mlir --concurrency &> dotproduct_concurrency.mlir
   ```
   Lets analyze the output - Read the inline comments that I left in the code.
   This is an example of the for_lowering. Check the arts.for splitting pattern.
   ```mlir
   module attributes {...} {
    ...
    func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
        ...
        /// Allocate reduction result db.
        %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i64) elementSizes[%c1] {arts.id = 53 : i64} : (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
        /// Notice that since this is a coarse grained allocation we use a db_ref with index 0, and then we have a single element access (the 0th element). Remember that the db_ref iterates over the outer dimensions (the sizes).
        %2 = arts.db_ref %ptr[%c0] : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
        /// Store the initial value of the reduction result db
        memref.store %0, %2[%c0] : memref<?xi64>
        %3 = arith.cmpi slt, %arg0, %c2_i32 : i32
        %4 = arith.cmpi sge, %arg0, %c2_i32 : i32
        %5 = arith.select %3, %c1_i32, %1 : i32
        ...
        scf.if %6#1 {
        %9 = arith.extsi %6#0 : i32 to i64
        %10 = arith.muli %9, %c4_i64 : i64
        %11 = arith.index_cast %10 : i64 to index
        %12 = arith.divui %11, %c4 : index
        /// Create the input db - these are coarse grained allocations (sizes = [1] and element sizes = [12])
        /// This is expected, by design, in the createDbPass we create coarse grained allocations when used in a parallel region.
        %guid_0, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%12] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
        %guid_2, %ptr_3 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%12] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
        /// Store the initial value of the reduction result db
        memref.store %c0_i64, %2[%c0] : memref<?xi64>
        /// This is the initialization of the input dbs... Noticed that since they are coarse grained allocations we use a db_ref with index 0, and then we iterate over the inner dimension (arg2). Remember that the db_ref iterates over the outer dimensions (the sizes).
        scf.for %arg2 = %c0 to %7 step %c1 {
            %33 = arith.index_cast %arg2 : index to i32
            %34 = arts.db_ref %ptr_1[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            memref.store %33, %34[%arg2] : memref<?xi32>
            %35 = arith.muli %33, %c2_i32 : i32
            %36 = arts.db_ref %ptr_3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            memref.store %35, %36[%arg2] : memref<?xi32>
        } {...}
        /// This is the allocation of the partial accumulatores. Check that this is fine grained because we create one for each worker.
        %guid_4, %ptr_5 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%c8] elementType(i64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
        /// We initialize the partial accumulators to the identity value (0 for addition)
        scf.for %arg2 = %c0 to %c8 step %c1 {
            %33 = arts.db_ref %ptr_5[%arg2] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
            memref.store %c0_i64, %33[%c0] : memref<?xi64>
        } {...}
        /// This is the parallel EDT that performs the work before the arts.for - This is the pessimistic case, and this is created by the arts.for lowering pass. This will be removed later on by the edt pass in the concurrency-opt pipeline.
        %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
        %guid_8, %ptr_9 = arts.db_acquire[<in>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
        %guid_10, %ptr_11 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi64>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
        arts.edt <parallel> <intranode> route(%c0_i32) (%ptr_7, %ptr_9, %ptr_11) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi64>>> attributes {workers = #arts.workers<8>} {
        ^bb0(%arg2: memref<?xmemref<memref<?xi32>>>, %arg3: memref<?xmemref<memref<?xi32>>>, %arg4: memref<?xmemref<memref<?xi64>>>):
            arts.db_release(%arg2) : memref<?xmemref<memref<?xi32>>>
            arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
            arts.db_release(%arg4) : memref<?xmemref<memref<?xi64>>>
        }
        /// This is the epoch that perform the actual arts.for work. We create an epoch so we can dispatch the work to the workers and synchronize them.
        %17 = arts.epoch {
            /// This is the worker loop that performs the work. We dispatch the work to the workers and synchronize them.
            scf.for %arg2 = %c0 to %c8 step %c1 {
                /// This is the work that each worker performs. We compute the block size and the chunk index.
                %33 = arith.addi %7, %c7 : index
                %34 = arith.divui %33, %c8 : index
                %35 = arith.muli %arg2, %34 : index
                %36 = arith.cmpi uge, %35, %7 : index
                %37 = arith.subi %7, %35 : index
                %38 = arith.select %36, %c0, %37 : index
                %39 = arith.minui %34, %38 : index
                %40 = arith.minui %39, %38 : index
                %41 = arith.cmpi ugt, %40, %c0 : index
                /// If the current worker has work to do, we acquire the partial accumulator dbs and the result db.
                scf.if %41 {
                    /// This is the acquire of the partial accumulator. This is fine grained (we know we will just write to the %arg2th element of the db)
                    %guid_16, %ptr_17 = arts.db_acquire[<inout>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<memref<?xi64>>>) offsets[%arg2] sizes[%c1] offset_hints[%arg2] size_hints[%c1] -> (memref<?xi64>, memref<memref<?xi64>>)
                    /// These are the input dbs, we acquire them in a coarse grained way (sizes = [1] and offsets = [0])
                    /// Why? because we can not guarantee so far that the accesses are disjoint across the workers, however we provided offsets and sizes hints that we will be using to prove disjointness later on in the concurrency-opt pipeline, specifically in the DbPass when partitioning is enabled
                    %guid_18, %ptr_19 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] offset_hints[%35] size_hints[%40] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                    %guid_20, %ptr_21 = arts.db_acquire[<in>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<memref<?xi32>>>) offsets[%c0] sizes[%c1] offset_hints[%35] size_hints[%40] -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                    /// This is the EDT that computes the work for each partial accumulator.
                    arts.edt <task> <intranode> route(%c0_i32) (%ptr_19, %ptr_21, %ptr_17) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>>, memref<memref<?xi64>> {
                    ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>, %arg4: memref<?xmemref<memref<?xi32>>>, %arg5: memref<memref<?xi64>>):
                    /// This is the inner loop that performs the work for each partial accumulator.
                    scf.for %arg6 = %c0 to %40 step %c1 {
                        /// We load the result db... since this is size 1, we use a db_ref with index 0.
                        %42 = arts.db_ref %arg5[%c0] {operandSegmentSizes = array<i32: 1, 1>} : memref<memref<?xi64>> -> memref<?xi64>
                        /// We compute the index of the input db that we are accessing.
                        %43 = arith.addi %35, %arg6 : index
                        /// We load the input db... since this is size 1, we use a db_ref with index 0.
                        %44 = arts.db_ref %arg3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                        /// Then we load the value from the input db (inner dimension)
                        %45 = memref.load %44[%43] : memref<?xi32>
                        %46 = arith.extsi %45 : i32 to i64
                        %47 = arts.db_ref %arg4[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                        %48 = memref.load %47[%43] : memref<?xi32>
                        %49 = arith.extsi %48 : i32 to i64
                        %50 = arith.muli %46, %49 : i64
                        /// We then load the result and update it with the sum of the two input values.
                        %51 = memref.load %42[%c0] : memref<?xi64>
                        %52 = arith.addi %51, %50 : i64
                        memref.store %52, %42[%c0] : memref<?xi64>
                    } {...}
                    arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
                    arts.db_release(%arg4) : memref<?xmemref<memref<?xi32>>>
                    arts.db_release(%arg5) : memref<memref<?xi64>>
                    }
                }
            }
            /// This is the EDT that performs the final reduction of the partial accumulators into the result db.
            /// We acquire all the partial accumulators, since we need to aggregate them into the result db.
            %guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<memref<?xi64>>>) offsets[%c0] sizes[%c8] -> (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
            /// This is the acquire of the result db. This is a fine grained acquire (sizes = [1] and offsets = [0])
            %guid_14, %ptr_15 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi64>>>) offsets[%c0] sizes[%c1] -> (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
            /// This is the EDT that performs the final reduction of the partial accumulators into the result db.
            arts.edt <task> <intranode> route(%c0_i32) (%ptr_13, %ptr_15) : memref<?xmemref<memref<?xi64>>>, memref<?xmemref<memref<?xi64>>> {
            ^bb0(%arg2: memref<?xmemref<memref<?xi64>>>, %arg3: memref<?xmemref<memref<?xi64>>>):
            /// We load the result db... since this is size 1, we use a db_ref with index 0.
            %33 = arts.db_ref %arg3[%c0] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
            /// We then iterate over the partial accumulators and aggregate them into the result db.
            %34 = scf.for %arg4 = %c0 to %c8 step %c1 iter_args(%arg5 = %c0_i64) -> (i64) {
                /// We load the partial accumulator... since this is size 1, we use a db_ref with index 0.
                %35 = arts.db_ref %arg2[%arg4] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
                /// We then load the value from the partial accumulator (inner dimension)
                %36 = memref.load %35[%c0] : memref<?xi64>
                %37 = arith.addi %arg5, %36 : i64
                scf.yield %37 : i64
            } {...}
            memref.store %34, %33[%c0] : memref<?xi64>
            arts.db_release(%arg2) : memref<?xmemref<memref<?xi64>>>
            arts.db_release(%arg3) : memref<?xmemref<memref<?xi64>>>
            }
        } : i64
        arts.db_free(%guid_4) : memref<?xi64>
        arts.db_free(%ptr_5) : memref<?xmemref<memref<?xi64>>>
        arts.db_free(%guid) : memref<?xi64>
        arts.db_free(%ptr) : memref<?xmemref<memref<?xi64>>>
        ...
        /// This is a sequiential verificaiton of the result. We iterate over the input dbs and compute the dot product.
        /// Notice the dbref pattern here, we use a db_ref with index 0 to iterate over the outer dimension of the input dbs.
        %30 = scf.for %arg2 = %c0 to %7 step %c1 iter_args(%arg3 = %c0_i64) -> (i64) {
            %33 = arts.db_ref %ptr_1[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            %34 = memref.load %33[%arg2] : memref<?xi32>
            %35 = arith.extsi %34 : i32 to i64
            %36 = arts.db_ref %ptr_3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            %37 = memref.load %36[%arg2] : memref<?xi32>
            %38 = arith.extsi %37 : i32 to i64
            %39 = arith.muli %35, %38 : i64
            %40 = arith.addi %arg3, %39 : i64
            scf.yield %40 : i64
        } {...}
        %31 = memref.load %2[%c0] : memref<?xi64>
        ...
        arts.db_free(%guid_2) : memref<?xi64>
        arts.db_free(%ptr_3) : memref<?xmemref<memref<?xi32>>>
        arts.db_free(%guid_0) : memref<?xi64>
        arts.db_free(%ptr_1) : memref<?xmemref<memref<?xi32>>>
        }
        arts.db_free(%guid) : memref<?xi64>
        arts.db_free(%ptr) : memref<?xmemref<memref<?xi64>>>
        return %8 : i32
    }
    }

4. **Lets now analyze the concurrency opt pass. In this pass we have the second run of the dbpass with partioning enabled**
    Remember that you can also debug any pass. For example to stop at create_db pass
   ```bash
   carts run dotproduct.mlir --concurrency-opt --debug-only=db &> dotproduct_concurrency_opt.mlir
   ```
   Analyze the inline comments that I left in the code.
   I will provide a summarized version of the module after all finished. 
   ```mlir
     module attributes {...} {
        ...
        func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
            ...
            /// Result db allocation.
            %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>] route(%c0_i32 : i32) sizes[%c1] elementType(i64) elementSizes[%c1] {arts.id = 53 : i64} : (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
            %2 = arts.db_ref %ptr[%c0] : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
            memref.store %0, %2[%c0] : memref<?xi64>
            ...
            scf.if %6#1 {
            %9 = arith.extsi %6#0 : i32 to i64
            %10 = arith.muli %9, %c4_i64 : i64
            %11 = arith.index_cast %10 : i64 to index
            %12 = arith.divui %11, %c4 : index
            /// Notice that now we create fine grained allocations for the input dbs! The dbpass was able to promote the element sizes to sizes.
            %guid_0, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%12] elementType(i32) elementSizes[%c1] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            %guid_2, %ptr_3 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%12] elementType(i32) elementSizes[%c1] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            memref.store %c0_i64, %2[%c0] : memref<?xi64>
            scf.for %arg2 = %c0 to %7 step %c1 {
                %33 = arith.index_cast %arg2 : index to i32
                /// Notice that now we use a fine grained db_ref to iterate over the outer dimension of the input dbs.
                %34 = arts.db_ref %ptr_1[%arg2] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                memref.store %33, %34[%c0] : memref<?xi32>
                %35 = arith.muli %33, %c2_i32 : i32
                /// Notice that now we use a fine grained db_ref to iterate over the outer dimension of the input dbs.
                %36 = arts.db_ref %ptr_3[%arg2] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                memref.store %35, %36[%c0] : memref<?xi32>
            } {...}
            ...
            /// This is the allocation of the partial accumulatores - this remained unchanged from the first run of the dbpass.
            %guid_4, %ptr_5 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%c8] elementType(i64) elementSizes[%c1] {arts.id = 37 : i64} : (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
            scf.for %arg2 = %c0 to %c8 step %c1 {
                %33 = arts.db_ref %ptr_5[%arg2] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
                memref.store %c0_i64, %33[%c0] : memref<?xi64>
            } {...}
            /// Notice that we dont longer have the pesessimistic case of the parallel edt that perform the work before the arts.for - the edt pass in the concurrency-opt pipeline successfully removed it.

            /// This is the epoch that performs the work before the arts.for
            %17 = arts.epoch {
                scf.for %arg2 = %c0 to %c8 step %c1 {
                %33 = arith.addi %7, %c7 : index
                %34 = arith.divui %33, %c8 : index
                %35 = arith.muli %arg2, %34 : index
                %36 = arith.cmpi uge, %35, %7 : index
                %37 = arith.subi %7, %35 : index
                %38 = arith.select %36, %c0, %37 : index
                %39 = arith.minui %34, %38 : index
                %40 = arith.minui %39, %38 : index
                %41 = arith.cmpi ugt, %40, %c0 : index
                scf.if %41 {
                    /// Notice that we now use a fine grained db_acquire to acquire the partial accumulator dbs.
                    /// The hints were propagated correctly to the acquires.
                    /// Also notice all twindiff are set to false, this is because we now use fine grained acquires and we can prove disjointness.
                    %guid_10, %ptr_11 = arts.db_acquire[<inout>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<memref<?xi64>>>) offsets[%arg2] sizes[%c1] offset_hints[%arg2] size_hints[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<memref<?xi64>>)
                    %guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) offsets[%35] sizes[%40] offset_hints[%35] size_hints[%40] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                    %guid_14, %ptr_15 = arts.db_acquire[<in>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<memref<?xi32>>>) offsets[%35] sizes[%40] offset_hints[%35] size_hints[%40] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                    /// This is the EDT that computes the work for each partial accumulator.
                    arts.edt <task> <intranode> route(%c0_i32) (%ptr_13, %ptr_15, %ptr_11) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>>, memref<memref<?xi64>> attributes {arts.id = 37 : i64} {
                    ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>, %arg4: memref<?xmemref<memref<?xi32>>>, %arg5: memref<memref<?xi64>>):
                    scf.for %arg6 = %c0 to %40 step %c1 {
                        /// This remained unchanged from the first run of the concurrency pass... this is expected sice the result db is still size 1.
                        %42 = arts.db_ref %arg5[%c0] {operandSegmentSizes = array<i32: 1, 1>} : memref<memref<?xi64>> -> memref<?xi64>
                        /// Notice that the input Dbs are now relative to zero, this happenes because the acqurie make sure we send only what we need...
                        %43 = arts.db_ref %arg3[%arg6] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                        %44 = memref.load %43[%c0] : memref<?xi32>
                        %45 = arith.extsi %44 : i32 to i64
                        %46 = arts.db_ref %arg4[%arg6] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                        %47 = memref.load %46[%c0] : memref<?xi32>
                        %48 = arith.extsi %47 : i32 to i64
                        %49 = arith.muli %45, %48 : i64
                        %50 = memref.load %42[%c0] : memref<?xi64>
                        %51 = arith.addi %50, %49 : i64
                        memref.store %51, %42[%c0] : memref<?xi64>
                    } {...}
                    arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
                    arts.db_release(%arg4) : memref<?xmemref<memref<?xi32>>>
                    arts.db_release(%arg5) : memref<memref<?xi64>>
                    }
                }
                } {...}
                /// This is the acquire of the partial accumulator dbs. These remained unchanged from the first run of the concurrency pass. This is correct since the result db is still size 1.
                %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<memref<?xi64>>>) offsets[%c0] sizes[%c8] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
                %guid_8, %ptr_9 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi64>>>) offsets[%c0] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi64>>>)
                arts.edt <task> <intranode> route(%c0_i32) (%ptr_7, %ptr_9) : memref<?xmemref<memref<?xi64>>>, memref<?xmemref<memref<?xi64>>> attributes {arts.id = 37 : i64} {
                ^bb0(%arg2: memref<?xmemref<memref<?xi64>>>, %arg3: memref<?xmemref<memref<?xi64>>>):
                %33 = arts.db_ref %arg3[%c0] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
                %34 = scf.for %arg4 = %c0 to %c8 step %c1 iter_args(%arg5 = %c0_i64) -> (i64) {
                    %35 = arts.db_ref %arg2[%arg4] {operandSegmentSizes = array<i32: 1, 1>} : memref<?xmemref<memref<?xi64>>> -> memref<?xi64>
                    %36 = memref.load %35[%c0] : memref<?xi64>
                    %37 = arith.addi %arg5, %36 : i64
                    scf.yield %37 : i64
                } {...}
                memref.store %34, %33[%c0] : memref<?xi64>
                arts.db_release(%arg2) : memref<?xmemref<memref<?xi64>>>
                arts.db_release(%arg3) : memref<?xmemref<memref<?xi64>>>
                }
            } : i64
            arts.db_free(%guid_4) : memref<?xi64>
            arts.db_free(%ptr_5) : memref<?xmemref<memref<?xi64>>>
            arts.db_free(%guid) : memref<?xi64>
            arts.db_free(%ptr) : memref<?xmemref<memref<?xi64>>>
            %18 = func.call @omp_get_wtime() : () -> f64
            %19 = llvm.mlir.addressof @str3 : !llvm.ptr
            %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
            %21 = arith.subf %18, %16 : f64
            %22 = llvm.call @printf(%20, %21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
            %23 = llvm.mlir.addressof @str4 : !llvm.ptr
            %24 = llvm.getelementptr %23[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<32 x i8>
            %25 = llvm.call @printf(%24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
            %26 = llvm.mlir.addressof @str5 : !llvm.ptr
            %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<19 x i8>
            %28 = memref.load %2[%c0] : memref<?xi64>
            %29 = llvm.call @printf(%27, %28) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i64) -> i32
            /// This is a sequiential verificaiton of the result. We iterate over the input dbs and compute the dot product.
            /// Notice the dbref pattern here, we use a db_ref with arg2 as the index to iterate over the outer dimension of the input dbs.
            %30 = scf.for %arg2 = %c0 to %7 step %c1 iter_args(%arg3 = %c0_i64) -> (i64) {
                %33 = arts.db_ref %ptr_1[%arg2] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                %34 = memref.load %33[%c0] : memref<?xi32>
                %35 = arith.extsi %34 : i32 to i64
                %36 = arts.db_ref %ptr_3[%arg2] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                %37 = memref.load %36[%c0] : memref<?xi32>
                %38 = arith.extsi %37 : i32 to i64
                %39 = arith.muli %35, %38 : i64
                %40 = arith.addi %arg3, %39 : i64
                scf.yield %40 : i64
            } {...}
            %31 = memref.load %2[%c0] : memref<?xi64>
            %32 = arith.cmpi ne, %31, %30 : i64
            scf.if %32 {
                %33 = llvm.mlir.addressof @str6 : !llvm.ptr
                %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
                %35 = memref.load %2[%c0] : memref<?xi64>
                %36 = llvm.call @printf(%34, %35, %30) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i64, i64) -> i32
            } else {
                %33 = llvm.mlir.addressof @str7 : !llvm.ptr
                %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<17 x i8>
                %35 = llvm.call @printf(%34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
            }
            arts.db_free(%guid_0) : memref<?xi64>
            arts.db_free(%ptr_1) : memref<?xmemref<memref<?xi32>>>
            arts.db_free(%guid_2) : memref<?xi64>
            arts.db_free(%ptr_3) : memref<?xmemref<memref<?xi32>>>
            }
            arts.db_free(%guid) : memref<?xi64>
            arts.db_free(%ptr) : memref<?xmemref<memref<?xi64>>>
            return %8 : i32
        }
        func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
        func.func private @omp_get_wtime() -> f64 attributes {llvm.linkage = #llvm.linkage<external>}
        }
   ```

4. **Lets now analyze the prelowering... I want you to analyze both the ParallelEDTLowering and EpochLowering**
   ```bash
   carts run dotproduct.mlir --pre-lowering --debug-only=parallel_edt_lowering &> dotproduct_lowering.mlir
   ```
   Lets analyze here the output of the prelowering pass.

   ```bash
   carts run dotproduct.mlir --pre-lowering &> dotproduct_lowering.mlir
   ```

5. **Lets now check the llvm lowering**
   ```bash
   carts run dotproduct.mlir --arts-to-llvm --debug-only=convert_arts_to_llvm &> dotproduct_llvm.mlir 
   ```

6. Finally lets carts execute,.

```bash
carts execute dotproduct.c -O3
./dotproduct 3 &> output
```

Remember that we want to fix the code so it works when optimizations are enabled
