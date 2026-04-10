module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("atax\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1000 = arith.constant 1000 : index
    %c-1000 = arith.constant -1000 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 3.1415926535897931 : f64
    %c2000 = arith.constant 2000 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 4.000000e+03 : f64
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c4000 = arith.constant 4000 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c4000, %c4000] {arts.id = 140 : i64} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %guid_2, %ptr_3 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f64) elementSizes[%c2000] {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:89:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32000 : i64, firstUseId = 34 : i64, lastUseId = 79 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 34 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_3 : memref<?xmemref<?xf64>>) block_shape[%c2000] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_4, %ptr_5 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f64) elementSizes[%c2000] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:90:20", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32000 : i64, firstUseId = 35 : i64, lastUseId = 80 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_5 : memref<?xmemref<?xf64>>) block_shape[%c2000] {owner_dims = array<i64: 0>, post_db_refined}
    %6 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %12 = arith.index_cast %arg2 : index to i32
      %13 = arith.sitofp %12 : i32 to f64
      scf.for %arg3 = %c0 to %c4000 step %c1 {
        %14 = arith.index_cast %arg3 : index to i32
        %15 = arith.addi %14, %c1_i32 : i32
        %16 = arith.sitofp %15 : i32 to f64
        %17 = arith.mulf %13, %16 : f64
        %18 = arith.divf %17, %cst_0 : f64
        memref.store %18, %6[%arg2, %arg3] : memref<?x?xf64>
      } {arts.id = 48 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 50 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %7 = arts.epoch attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %12 = arith.muli %arg2, %c2000 : index
        %13 = arith.cmpi uge, %12, %c4000 : index
        %14 = arith.subi %c4000, %12 : index
        %15 = arith.select %13, %c0, %14 : index
        %16 = arith.minui %15, %c2000 : index
        %17 = arith.cmpi ugt, %16, %c0 : index
        scf.if %17 {
          %18 = arith.divui %12, %c2000 : index
          %19 = arith.cmpi ugt, %18, %c1 : index
          %20 = arith.select %19, %c0, %18 : index
          %guid_10, %ptr_11 = arts.db_acquire[<out>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%12], sizes[%c2000]), offsets[%20], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_11 : memref<?xmemref<?xf64>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c2000] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.lowering_contract(%ptr_7 : memref<?xmemref<?x?xf64>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c2000] min_offsets[%c-1000] max_offsets[%c1000] {contract_kind = 1 : i64, critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_11, %ptr_7) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 142 : i64, arts.pattern_revision = 2 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>):
            %21 = arith.subi %c0, %12 : index
            %22 = arith.cmpi slt, %21, %c0 : index
            %23 = arith.select %22, %c0, %21 : index
            %24 = arith.cmpi slt, %14, %c0 : index
            %25 = arith.select %24, %c0, %14 : index
            %26 = arith.minui %25, %16 : index
            %27 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
            %28 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            scf.for %arg5 = %23 to %26 step %c1 {
              %29 = arith.addi %12, %arg5 : index
              memref.store %cst_1, %27[%arg5] : memref<?xf64>
              scf.for %arg6 = %c0 to %c4000 step %c1 {
                %30 = memref.load %27[%arg5] : memref<?xf64>
                %31 = memref.load %28[%29, %arg6] : memref<?x?xf64>
                %32 = arith.index_cast %arg6 : index to i32
                %33 = arith.sitofp %32 : i32 to f64
                %34 = arith.mulf %33, %cst : f64
                %35 = arith.mulf %31, %34 : f64
                %36 = arith.addf %30, %35 : f64
                memref.store %36, %27[%arg5] : memref<?xf64>
              } {arts.id = 61 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "recovered"}
            } {arts.id = 140 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
          }
        }
      }
    } : i64
    arts.db_release(%ptr_7) : memref<?xmemref<?x?xf64>>
    %guid_8, %ptr_9 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [4000, 2000], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %8 = arts.epoch attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %12 = arith.muli %arg2, %c2000 : index
        %13 = arith.cmpi uge, %12, %c4000 : index
        %14 = arith.subi %c4000, %12 : index
        %15 = arith.select %13, %c0, %14 : index
        %16 = arith.minui %15, %c2000 : index
        %17 = arith.cmpi ugt, %16, %c0 : index
        scf.if %17 {
          %guid_10, %ptr_11 = arts.db_acquire[<in>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%12], sizes[%c2000]), offsets[%c0], sizes[%c2] {arts.id = 145 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_11 : memref<?xmemref<?xf64>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c2000] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.lowering_contract(%ptr_9 : memref<?xmemref<?x?xf64>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c4000] min_offsets[%c-1000] max_offsets[%c1000] {contract_kind = 1 : i64, critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 1>, post_db_refined}
          %18 = arith.divui %12, %c2000 : index
          %19 = arith.cmpi ugt, %18, %c1 : index
          %20 = arith.select %19, %c0, %18 : index
          %guid_12, %ptr_13 = arts.db_acquire[<out>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%12], sizes[%c2000]), offsets[%20], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_13 : memref<?xmemref<?xf64>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c2000] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_11, %ptr_9, %ptr_13) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 143 : i64, arts.pattern_revision = 2 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %21 = arith.subi %c0, %12 : index
            %22 = arith.cmpi slt, %21, %c0 : index
            %23 = arith.select %22, %c0, %21 : index
            %24 = arith.cmpi slt, %14, %c0 : index
            %25 = arith.select %24, %c0, %14 : index
            %26 = arith.minui %25, %16 : index
            %27 = arts.db_ref %arg5[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
            %28 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            scf.for %arg6 = %23 to %26 step %c1 {
              %29 = arith.addi %12, %arg6 : index
              memref.store %cst_1, %27[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c2 step %c1 {
                %30 = arith.muli %arg7, %c2000 : index
                %31 = arith.subi %c4000, %30 : index
                %32 = arith.cmpi uge, %30, %c4000 : index
                %33 = arith.select %32, %c0, %31 : index
                %34 = arith.minui %33, %c2000 : index
                %35 = arts.db_ref %arg3[%arg7] : memref<?xmemref<?xf64>> -> memref<?xf64>
                scf.for %arg8 = %c0 to %34 step %c1 {
                  %36 = arith.addi %30, %arg8 : index
                  %37 = memref.load %27[%arg6] : memref<?xf64>
                  %38 = memref.load %28[%36, %29] : memref<?x?xf64>
                  %39 = memref.load %35[%arg8] : memref<?xf64>
                  %40 = arith.mulf %38, %39 : f64
                  %41 = arith.addf %37, %40 : f64
                  memref.store %41, %27[%arg6] : memref<?xf64>
                }
              }
            } {arts.id = 141 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?xf64>>
          }
        }
      }
    } : i64
    arts.db_release(%ptr_9) : memref<?xmemref<?x?xf64>>
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %10 = scf.for %arg2 = %c0 to %c2 step %c1 iter_args(%arg3 = %cst_1) -> (f64) {
      %12 = arith.muli %arg2, %c2000 : index
      %13 = arith.subi %c4000, %12 : index
      %14 = arith.cmpi uge, %12, %c4000 : index
      %15 = arith.select %14, %c0, %13 : index
      %16 = arith.minui %15, %c2000 : index
      %17 = arts.db_ref %ptr_3[%arg2] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %18 = scf.for %arg4 = %c0 to %16 step %c1 iter_args(%arg5 = %arg3) -> (f64) {
        %19 = memref.load %17[%arg4] : memref<?xf64>
        %20 = arith.addf %arg5, %19 : f64
        scf.yield %20 : f64
      }
      scf.yield %18 : f64
    }
    call @carts_bench_checksum_d(%10) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%11, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_4) : memref<?xi64>
    arts.db_free(%ptr_5) : memref<?xmemref<?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_2) : memref<?xi64>
    arts.db_free(%ptr_3) : memref<?xmemref<?xf64>>
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
}
