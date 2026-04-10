module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2 = arith.constant 2 : index
    %cst = arith.constant -1.702000e+00 : f32
    %cst_0 = arith.constant 1.000000e+00 : f32
    %cst_1 = arith.constant 5.000000e-01 : f32
    %cst_2 = arith.constant 0.797884583 : f32
    %cst_3 = arith.constant 4.471500e-02 : f32
    %cst_4 = arith.constant 1.000000e-01 : f32
    %cst_5 = arith.constant 0x49800000 : f32
    %c524288 = arith.constant 524288 : index
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
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 296 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_11, %ptr_12 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 297 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_12 : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_13, %ptr_14 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 298 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_14 : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_15, %ptr_16 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:174:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 33 : i64, lastUseId = 189 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 33 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_16 : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_17, %ptr_18 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 299 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_18 : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_19, %ptr_20 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 300 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_20 : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_21, %ptr_22 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c524288] {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:177:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 34 : i64, lastUseId = 190 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 34 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_22 : memref<?xmemref<?xf32>>) block_shape[%c524288] {owner_dims = array<i64: 0>, post_db_refined}
    %alloc = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:179:27", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 400 : i64, firstUseId = 35 : i64, lastUseId = 244 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "recovered"} : memref<100xf32>
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %6 = arts.epoch attributes {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %26 = arith.muli %arg2, %c524288 : index
        %27 = arith.cmpi uge, %26, %c1048576 : index
        %28 = arith.subi %c1048576, %26 : index
        %29 = arith.select %27, %c0, %28 : index
        %30 = arith.minui %29, %c524288 : index
        %31 = arith.cmpi ugt, %30, %c0 : index
        scf.if %31 {
          %32 = arith.divui %26, %c524288 : index
          %33 = arith.cmpi ugt, %32, %c1 : index
          %34 = arith.select %33, %c0, %32 : index
          %guid_23, %ptr_24 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          %guid_25, %ptr_26 = arts.db_acquire[<out>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_26 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          %guid_27, %ptr_28 = arts.db_acquire[<out>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_28 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_26, %ptr_28) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 298 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>, %arg4: memref<?xmemref<?xf32>>, %arg5: memref<?xmemref<?xf32>>):
            %35 = arith.subi %c0, %26 : index
            %36 = arith.cmpi slt, %35, %c0 : index
            %37 = arith.select %36, %c0, %35 : index
            %38 = arith.cmpi slt, %28, %c0 : index
            %39 = arith.select %38, %c0, %28 : index
            %40 = arith.minui %39, %30 : index
            %41 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            %42 = arts.db_ref %arg4[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            %43 = arts.db_ref %arg5[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            scf.for %arg6 = %37 to %40 step %c1 {
              %44 = arith.addi %26, %arg6 : index
              %45 = arith.index_cast %44 : index to i32
              %46 = arith.sitofp %45 : i32 to f32
              %47 = arith.divf %46, %cst_5 : f32
              %48 = arith.mulf %47, %cst_9 : f32
              %49 = arith.addf %48, %cst_8 : f32
              %50 = arith.cmpf ogt, %49, %cst_7 : f32
              %51 = arith.select %50, %49, %cst_7 : f32
              memref.store %51, %41[%arg6] : memref<?xf32>
              %52 = scf.if %50 -> (f32) {
                scf.yield %49 : f32
              } else {
                %55 = arith.mulf %49, %cst_4 : f32
                scf.yield %55 : f32
              }
              memref.store %52, %42[%arg6] : memref<?xf32>
              %53 = arith.cmpf olt, %51, %cst_9 : f32
              %54 = arith.select %53, %51, %cst_9 : f32
              memref.store %54, %43[%arg6] : memref<?xf32>
            } {arts.id = 179 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:190:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?xf32>>
          }
        }
      }
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %26 = arith.muli %arg2, %c524288 : index
        %27 = arith.cmpi uge, %26, %c1048576 : index
        %28 = arith.subi %c1048576, %26 : index
        %29 = arith.select %27, %c0, %28 : index
        %30 = arith.minui %29, %c524288 : index
        %31 = arith.cmpi ugt, %30, %c0 : index
        scf.if %31 {
          %32 = arith.divui %26, %c524288 : index
          %33 = arith.cmpi ugt, %32, %c1 : index
          %34 = arith.select %33, %c0, %32 : index
          %guid_23, %ptr_24 = arts.db_acquire[<out>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 1 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24) : memref<?xmemref<?xf32>> attributes {arts.id = 299 : i64, arts.pattern_revision = 2 : i64, critical_path_distance = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>):
            %35 = arith.subi %c0, %26 : index
            %36 = arith.cmpi slt, %35, %c0 : index
            %37 = arith.select %36, %c0, %35 : index
            %38 = arith.cmpi slt, %28, %c0 : index
            %39 = arith.select %38, %c0, %28 : index
            %40 = arith.minui %39, %30 : index
            %41 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            scf.for %arg4 = %37 to %40 step %c1 {
              %42 = arith.addi %26, %arg4 : index
              %43 = arith.index_cast %42 : index to i32
              %44 = arith.sitofp %43 : i32 to f32
              %45 = arith.divf %44, %cst_5 : f32
              %46 = arith.mulf %45, %cst_9 : f32
              %47 = arith.addf %46, %cst_8 : f32
              %48 = arith.mulf %47, %47 : f32
              %49 = arith.mulf %48, %47 : f32
              %50 = arith.mulf %49, %cst_3 : f32
              %51 = arith.addf %47, %50 : f32
              %52 = arith.mulf %51, %cst_2 : f32
              %53 = arith.mulf %47, %cst_1 : f32
              %54 = func.call @tanhf(%52) : (f32) -> f32
              %55 = arith.addf %54, %cst_0 : f32
              %56 = arith.mulf %53, %55 : f32
              memref.store %56, %41[%arg4] : memref<?xf32>
            } {arts.id = 180 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:193:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
          }
        }
      }
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %26 = arith.muli %arg2, %c524288 : index
        %27 = arith.cmpi uge, %26, %c1048576 : index
        %28 = arith.subi %c1048576, %26 : index
        %29 = arith.select %27, %c0, %28 : index
        %30 = arith.minui %29, %c524288 : index
        %31 = arith.cmpi ugt, %30, %c0 : index
        scf.if %31 {
          %32 = arith.divui %26, %c524288 : index
          %33 = arith.cmpi ugt, %32, %c1 : index
          %34 = arith.select %33, %c0, %32 : index
          %guid_23, %ptr_24 = arts.db_acquire[<out>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 2 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          %guid_25, %ptr_26 = arts.db_acquire[<out>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_26 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 2 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_26) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 300 : i64, critical_path_distance = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>, %arg4: memref<?xmemref<?xf32>>):
            %35 = arith.subi %c0, %26 : index
            %36 = arith.cmpi slt, %35, %c0 : index
            %37 = arith.select %36, %c0, %35 : index
            %38 = arith.cmpi slt, %28, %c0 : index
            %39 = arith.select %38, %c0, %28 : index
            %40 = arith.minui %39, %30 : index
            %41 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            %42 = arts.db_ref %arg4[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            scf.for %arg5 = %37 to %40 step %c1 {
              %43 = arith.addi %26, %arg5 : index
              %44 = arith.index_cast %43 : index to i32
              %45 = arith.sitofp %44 : i32 to f32
              %46 = arith.divf %45, %cst_5 : f32
              %47 = arith.mulf %46, %cst_9 : f32
              %48 = arith.addf %47, %cst_8 : f32
              %49 = arith.mulf %48, %cst : f32
              %50 = math.exp %49 : f32
              %51 = arith.addf %50, %cst_0 : f32
              %52 = arith.divf %cst_0, %51 : f32
              %53 = arith.mulf %48, %52 : f32
              memref.store %53, %41[%arg5] : memref<?xf32>
              %54 = arith.negf %48 : f32
              %55 = math.exp %54 : f32
              %56 = arith.addf %55, %cst_0 : f32
              %57 = arith.divf %cst_0, %56 : f32
              memref.store %57, %42[%arg5] : memref<?xf32>
            } {arts.id = 296 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:194:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?xf32>>
          }
        }
      }
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %26 = arith.muli %arg2, %c524288 : index
        %27 = arith.cmpi uge, %26, %c1048576 : index
        %28 = arith.subi %c1048576, %26 : index
        %29 = arith.select %27, %c0, %28 : index
        %30 = arith.minui %29, %c524288 : index
        %31 = arith.cmpi ugt, %30, %c0 : index
        scf.if %31 {
          %32 = arith.divui %26, %c524288 : index
          %33 = arith.cmpi ugt, %32, %c1 : index
          %34 = arith.select %33, %c0, %32 : index
          %guid_23, %ptr_24 = arts.db_acquire[<out>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%26], sizes[%c524288]), offsets[%34], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?xf32>>) dep_pattern(<elementwise_pipeline>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c524288] {critical_path_distance = 3 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24) : memref<?xmemref<?xf32>> attributes {arts.id = 301 : i64, arts.pattern_revision = 2 : i64, critical_path_distance = 3 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>):
            %35 = arith.subi %c0, %26 : index
            %36 = arith.cmpi slt, %35, %c0 : index
            %37 = arith.select %36, %c0, %35 : index
            %38 = arith.cmpi slt, %28, %c0 : index
            %39 = arith.select %38, %c0, %28 : index
            %40 = arith.minui %39, %30 : index
            %41 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
            scf.for %arg4 = %37 to %40 step %c1 {
              %42 = arith.addi %26, %arg4 : index
              %43 = arith.index_cast %42 : index to i32
              %44 = arith.sitofp %43 : i32 to f32
              %45 = arith.divf %44, %cst_5 : f32
              %46 = arith.mulf %45, %cst_9 : f32
              %47 = arith.addf %46, %cst_8 : f32
              %48 = func.call @tanhf(%47) : (f32) -> f32
              memref.store %48, %41[%arg4] : memref<?xf32>
            } {arts.id = 297 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:196:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
          }
        }
      }
    } : i64
    %7 = scf.for %arg2 = %c1 to %c100 step %c1 iter_args(%arg3 = %cst_8) -> (f32) {
      %26 = arith.index_cast %arg2 : index to i32
      %27 = arith.sitofp %26 : i32 to f32
      %28 = arith.divf %27, %cst_6 : f32
      %29 = arith.mulf %28, %cst_9 : f32
      %30 = arith.addf %29, %cst_8 : f32
      %31 = arith.cmpf ogt, %30, %arg3 : f32
      %32 = arith.select %31, %30, %arg3 : f32
      scf.yield %32 : f32
    } {arts.id = 127 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 99 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 0 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    %8 = scf.for %arg2 = %c0 to %c100 step %c1 iter_args(%arg3 = %cst_7) -> (f32) {
      %26 = arith.index_cast %arg2 : index to i32
      %27 = arith.sitofp %26 : i32 to f32
      %28 = arith.divf %27, %cst_6 : f32
      %29 = arith.mulf %28, %cst_9 : f32
      %30 = arith.addf %29, %cst_8 : f32
      %31 = arith.subf %30, %7 : f32
      %32 = math.exp %31 : f32
      memref.store %32, %alloc[%arg2] : memref<100xf32>
      %33 = arith.addf %arg3, %32 : f32
      scf.yield %33 : f32
    } {arts.id = 133 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %26 = memref.load %alloc[%arg2] : memref<100xf32>
      %27 = arith.divf %26, %8 : f32
      memref.store %27, %alloc[%arg2] : memref<100xf32>
    } {arts.id = 138 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %10 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %11 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr_12[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %12 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr_14[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %13 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr_16[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %14 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr_18[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %15 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr_20[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %16 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = arith.muli %arg2, %c524288 : index
      %27 = arith.subi %c1048576, %26 : index
      %28 = arith.cmpi uge, %26, %c1048576 : index
      %29 = arith.select %28, %c0, %27 : index
      %30 = arith.minui %29, %c524288 : index
      %31 = arts.db_ref %ptr_22[%arg2] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %32 = scf.for %arg4 = %c0 to %30 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %33 = memref.load %31[%arg4] : memref<?xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = math.absf %34 : f64
        %36 = arith.addf %arg5, %35 : f64
        scf.yield %36 : f64
      }
      scf.yield %32 : f64
    }
    %17 = scf.for %arg2 = %c0 to %c100 step %c1 iter_args(%arg3 = %cst_10) -> (f64) {
      %26 = memref.load %alloc[%arg2] : memref<100xf32>
      %27 = arith.extf %26 : f32 to f64
      %28 = math.absf %27 : f64
      %29 = arith.addf %arg3, %28 : f64
      scf.yield %29 : f64
    } {arts.id = 177 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:227:21">, arts.metadata_provenance = "recovered"}
    %18 = arith.addf %10, %11 : f64
    %19 = arith.addf %18, %12 : f64
    %20 = arith.addf %19, %13 : f64
    %21 = arith.addf %20, %14 : f64
    %22 = arith.addf %21, %15 : f64
    %23 = arith.addf %22, %16 : f64
    %24 = arith.addf %23, %17 : f64
    call @carts_bench_checksum_d(%24) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %25 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%25, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc {arts.id = 244 : i64} : memref<100xf32>
    call @carts_phase_timer_stop(%25) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?xmemref<?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?xf32>>
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
