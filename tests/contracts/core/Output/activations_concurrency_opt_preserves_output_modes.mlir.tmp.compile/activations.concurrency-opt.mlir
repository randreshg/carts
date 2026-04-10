module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for single-worker lowering checks.\0Aworker_threads=1\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 1 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant -1.702000e+00 : f32
    %cst_0 = arith.constant 1.000000e+00 : f32
    %cst_1 = arith.constant 5.000000e-01 : f32
    %cst_2 = arith.constant 0.797884583 : f32
    %cst_3 = arith.constant 4.471500e-02 : f32
    %cst_4 = arith.constant 1.000000e-01 : f32
    %cst_5 = arith.constant 0x49800000 : f32
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %cst_6 = arith.constant 1.000000e+02 : f32
    %c1048576 = arith.constant 1048576 : index
    %cst_7 = arith.constant 0.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %cst_8 = arith.constant -3.000000e+00 : f32
    %cst_9 = arith.constant 6.000000e+00 : f32
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 296 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_11, %ptr_12 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 297 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_13, %ptr_14 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 298 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_15, %ptr_16 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:174:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 33 : i64, lastUseId = 189 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 33 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_17, %ptr_18 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 299 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_19, %ptr_20 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 300 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_21, %ptr_22 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:177:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 34 : i64, lastUseId = 190 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 34 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %alloc = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:179:27", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 400 : i64, firstUseId = 35 : i64, lastUseId = 244 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "recovered"} : memref<100xf32>
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %6 = arts.epoch attributes {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      %guid_23, %ptr_24 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_24 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %guid_25, %ptr_26 = arts.db_acquire[<out>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_26 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %guid_27, %ptr_28 = arts.db_acquire[<out>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_28 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %33 = arts.db_ref %ptr_24[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %34 = arts.db_ref %ptr_26[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %35 = arts.db_ref %ptr_28[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %40 = arith.index_cast %arg2 : index to i32
        %41 = arith.sitofp %40 : i32 to f32
        %42 = arith.divf %41, %cst_5 : f32
        %43 = arith.mulf %42, %cst_9 : f32
        %44 = arith.addf %43, %cst_8 : f32
        %45 = arith.cmpf ogt, %44, %cst_7 : f32
        %46 = arith.select %45, %44, %cst_7 : f32
        memref.store %46, %33[%arg2] : memref<?xf32>
        %47 = scf.if %45 -> (f32) {
          scf.yield %44 : f32
        } else {
          %50 = arith.mulf %44, %cst_4 : f32
          scf.yield %50 : f32
        }
        memref.store %47, %34[%arg2] : memref<?xf32>
        %48 = arith.cmpf olt, %46, %cst_9 : f32
        %49 = arith.select %48, %46, %cst_9 : f32
        memref.store %49, %35[%arg2] : memref<?xf32>
      } {arts.id = 179 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:190:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_24) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_26) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_28) : memref<?xmemref<?xf32>>
      %guid_29, %ptr_30 = arts.db_acquire[<out>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_30 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %36 = arts.db_ref %ptr_30[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %40 = arith.index_cast %arg2 : index to i32
        %41 = arith.sitofp %40 : i32 to f32
        %42 = arith.divf %41, %cst_5 : f32
        %43 = arith.mulf %42, %cst_9 : f32
        %44 = arith.addf %43, %cst_8 : f32
        %45 = arith.mulf %44, %44 : f32
        %46 = arith.mulf %45, %44 : f32
        %47 = arith.mulf %46, %cst_3 : f32
        %48 = arith.addf %44, %47 : f32
        %49 = arith.mulf %48, %cst_2 : f32
        %50 = arith.mulf %44, %cst_1 : f32
        %51 = func.call @tanhf(%49) : (f32) -> f32
        %52 = arith.addf %51, %cst_0 : f32
        %53 = arith.mulf %50, %52 : f32
        memref.store %53, %36[%arg2] : memref<?xf32>
      } {arts.id = 180 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:193:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_30) : memref<?xmemref<?xf32>>
      %guid_31, %ptr_32 = arts.db_acquire[<out>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_32 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %guid_33, %ptr_34 = arts.db_acquire[<out>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_34 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %37 = arts.db_ref %ptr_32[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %38 = arts.db_ref %ptr_34[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %40 = arith.index_cast %arg2 : index to i32
        %41 = arith.sitofp %40 : i32 to f32
        %42 = arith.divf %41, %cst_5 : f32
        %43 = arith.mulf %42, %cst_9 : f32
        %44 = arith.addf %43, %cst_8 : f32
        %45 = arith.mulf %44, %cst : f32
        %46 = math.exp %45 : f32
        %47 = arith.addf %46, %cst_0 : f32
        %48 = arith.divf %cst_0, %47 : f32
        %49 = arith.mulf %44, %48 : f32
        memref.store %49, %37[%arg2] : memref<?xf32>
        %50 = arith.negf %44 : f32
        %51 = math.exp %50 : f32
        %52 = arith.addf %51, %cst_0 : f32
        %53 = arith.divf %cst_0, %52 : f32
        memref.store %53, %38[%arg2] : memref<?xf32>
      } {arts.id = 296 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:194:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_32) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_34) : memref<?xmemref<?xf32>>
      %guid_35, %ptr_36 = arts.db_acquire[<out>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_36 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c1048576] {distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
      %39 = arts.db_ref %ptr_36[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %40 = arith.index_cast %arg2 : index to i32
        %41 = arith.sitofp %40 : i32 to f32
        %42 = arith.divf %41, %cst_5 : f32
        %43 = arith.mulf %42, %cst_9 : f32
        %44 = arith.addf %43, %cst_8 : f32
        %45 = func.call @tanhf(%44) : (f32) -> f32
        memref.store %45, %39[%arg2] : memref<?xf32>
      } {arts.id = 297 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:196:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_36) : memref<?xmemref<?xf32>>
    } : i64
    %7 = scf.for %arg2 = %c1 to %c100 step %c1 iter_args(%arg3 = %cst_8) -> (f32) {
      %33 = arith.index_cast %arg2 : index to i32
      %34 = arith.sitofp %33 : i32 to f32
      %35 = arith.divf %34, %cst_6 : f32
      %36 = arith.mulf %35, %cst_9 : f32
      %37 = arith.addf %36, %cst_8 : f32
      %38 = arith.cmpf ogt, %37, %arg3 : f32
      %39 = arith.select %38, %37, %arg3 : f32
      scf.yield %39 : f32
    } {arts.id = 127 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 99 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    %8 = scf.for %arg2 = %c0 to %c100 step %c1 iter_args(%arg3 = %cst_7) -> (f32) {
      %33 = arith.index_cast %arg2 : index to i32
      %34 = arith.sitofp %33 : i32 to f32
      %35 = arith.divf %34, %cst_6 : f32
      %36 = arith.mulf %35, %cst_9 : f32
      %37 = arith.addf %36, %cst_8 : f32
      %38 = arith.subf %37, %7 : f32
      %39 = math.exp %38 : f32
      memref.store %39, %alloc[%arg2] : memref<100xf32>
      %40 = arith.addf %arg3, %39 : f32
      scf.yield %40 : f32
    } {arts.id = 133 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %33 = memref.load %alloc[%arg2] : memref<100xf32>
      %34 = arith.divf %33, %8 : f32
      memref.store %34, %alloc[%arg2] : memref<100xf32>
    } {arts.id = 138 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %10 = arts.db_ref %ptr[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %11 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %10[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 140 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:206:21">, arts.metadata_provenance = "recovered"}
    %12 = arts.db_ref %ptr_12[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %13 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %12[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 145 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:209:21">, arts.metadata_provenance = "recovered"}
    %14 = arts.db_ref %ptr_14[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %15 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %14[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 150 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:212:21">, arts.metadata_provenance = "recovered"}
    %16 = arts.db_ref %ptr_16[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %17 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %16[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 155 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:215:21">, arts.metadata_provenance = "recovered"}
    %18 = arts.db_ref %ptr_18[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %19 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %18[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 161 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:218:21">, arts.metadata_provenance = "recovered"}
    %20 = arts.db_ref %ptr_20[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %21 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %20[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 166 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:221:21">, arts.metadata_provenance = "recovered"}
    %22 = arts.db_ref %ptr_22[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %23 = scf.for %arg2 = %c0 to %c1048576 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %22[%arg2] : memref<?xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 171 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:224:21">, arts.metadata_provenance = "recovered"}
    %24 = scf.for %arg2 = %c0 to %c100 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %33 = memref.load %alloc[%arg2] : memref<100xf32>
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = arith.addf %arg3, %35 : f64
      scf.yield %36 : f64
    } {arts.id = 177 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:227:21">, arts.metadata_provenance = "recovered"}
    %25 = arith.addf %11, %13 : f64
    %26 = arith.addf %25, %15 : f64
    %27 = arith.addf %26, %17 : f64
    %28 = arith.addf %27, %19 : f64
    %29 = arith.addf %28, %21 : f64
    %30 = arith.addf %29, %23 : f64
    %31 = arith.addf %30, %24 : f64
    call @carts_bench_checksum_d(%31) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %32 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%32, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc {arts.id = 244 : i64} : memref<100xf32>
    call @carts_phase_timer_stop(%32) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?xmemref<?xf32>>
    return %c0_i32 : i32
  }
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @tanhf(f32) -> f32 attributes {llvm.linkage = #llvm.linkage<external>}
}
