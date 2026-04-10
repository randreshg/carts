module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("poisson-for\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-12 = arith.constant -12 : index
    %c-14 = arith.constant -14 : index
    %c12 = arith.constant 12 : index
    %c7 = arith.constant 7 : index
    %0 = llvm.mlir.zero : !llvm.ptr
    %c8 = arith.constant 8 : index
    %cst = arith.constant 2.500000e-01 : f64
    %cst_0 = arith.constant 0.010101010101010102 : f64
    %c-1 = arith.constant -1 : index
    %c13 = arith.constant 13 : index
    %cst_1 = arith.constant -9.869604401089358 : f64
    %cst_2 = arith.constant 3.1415926535897931 : f64
    %cst_3 = arith.constant 9.900000e+01 : f64
    %c2 = arith.constant 2 : index
    %c11 = arith.constant 11 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c100 = arith.constant 100 : index
    %c0 = arith.constant 0 : index
    %c99_i32 = arith.constant 99 : i32
    %1 = llvm.mlir.addressof @str3 : !llvm.ptr
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %3 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %4 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c8] elementType(f64) elementSizes[%c13, %c100] {arts.id = 275 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%c13] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_5, %ptr_6 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c8] elementType(f64) elementSizes[%c13, %c100] {arts.id = 276 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_6 : memref<?xmemref<?x?xf64>>) block_shape[%c13] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_7, %ptr_8 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c8] elementType(f64) elementSizes[%c13, %c100] {arts.id = 277 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_8 : memref<?xmemref<?x?xf64>>) block_shape[%c13] {owner_dims = array<i64: 0>, post_db_refined}
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %11 = arith.divui %arg0, %c13 : index
      %12 = arith.remui %arg0, %c13 : index
      %13 = arts.db_ref %ptr[%11] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      %14 = arts.db_ref %ptr_6[%11] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      %15 = arts.db_ref %ptr_8[%11] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      scf.for %arg1 = %c0 to %c100 step %c1 {
        memref.store %cst_4, %13[%12, %arg1] : memref<?x?xf64>
        memref.store %cst_4, %14[%12, %arg1] : memref<?x?xf64>
        memref.store %cst_4, %15[%12, %arg1] : memref<?x?xf64>
      } {arts.id = 46 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:106:23">}
    } {arts.id = 45 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:105:21">}
    %7 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %11 = arith.muli %arg0, %c13 : index
        %12 = arith.cmpi uge, %11, %c100 : index
        %13 = arith.subi %c100, %11 : index
        %14 = arith.select %12, %c0, %13 : index
        %15 = arith.minui %14, %c13 : index
        %16 = arith.cmpi ugt, %15, %c0 : index
        scf.if %16 {
          %guid_9, %ptr_10 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%c0], sizes[%c8] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [100, 13], stencil_center_offset = 1 : i64, stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_10 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c100] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 1>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_10) : memref<?xmemref<?x?xf64>> attributes {arts.id = 279 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [100, 13]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>):
            %17 = arith.subi %c0, %11 : index
            %18 = arith.cmpi slt, %17, %c0 : index
            %19 = arith.select %18, %c0, %17 : index
            %20 = arith.cmpi slt, %13, %c0 : index
            %21 = arith.select %20, %c0, %13 : index
            %22 = arith.minui %21, %15 : index
            scf.for %arg2 = %19 to %22 step %c1 {
              %23 = arith.addi %11, %arg2 : index
              %24 = arith.index_cast %23 : index to i32
              %25 = arith.sitofp %24 : i32 to f64
              %26 = arith.divf %25, %cst_3 : f64
              %27 = arith.cmpi eq, %24, %c0_i32 : i32
              %28 = arith.cmpi eq, %24, %c99_i32 : i32
              scf.for %arg3 = %c0 to %c8 step %c1 {
                %29 = arith.muli %arg3, %c13 : index
                %30 = arith.subi %c100, %29 : index
                %31 = arith.cmpi uge, %29, %c100 : index
                %32 = arith.select %31, %c0, %30 : index
                %33 = arith.minui %32, %c13 : index
                scf.for %arg4 = %c0 to %33 step %c1 {
                  %34 = arith.addi %29, %arg4 : index
                  %35 = arith.index_cast %34 : index to i32
                  %36 = arith.sitofp %35 : i32 to f64
                  %37 = arith.divf %36, %cst_3 : f64
                  %38 = arith.cmpi eq, %34, %c0 : index
                  %39 = scf.if %38 -> (i1) {
                    scf.yield %true : i1
                  } else {
                    %42 = arith.cmpi eq, %35, %c99_i32 : i32
                    scf.yield %42 : i1
                  }
                  %40 = arith.select %39, %true, %27 : i1
                  %41 = arith.select %40, %true, %28 : i1
                  scf.if %41 {
                    %42 = arith.mulf %37, %cst_2 : f64
                    %43 = arith.mulf %42, %26 : f64
                    %44 = math.sin %43 : f64
                    %45 = arith.divui %34, %c13 : index
                    %46 = arith.remui %34, %c13 : index
                    %47 = arts.db_ref %arg1[%45] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    memref.store %44, %47[%46, %23] : memref<?x?xf64>
                  } else {
                    %42 = arith.mulf %37, %37 : f64
                    %43 = arith.mulf %26, %26 : f64
                    %44 = arith.addf %42, %43 : f64
                    %45 = arith.mulf %44, %cst_1 : f64
                    %46 = arith.mulf %37, %cst_2 : f64
                    %47 = arith.mulf %46, %26 : f64
                    %48 = math.sin %47 : f64
                    %49 = arith.mulf %45, %48 : f64
                    %50 = arith.negf %49 : f64
                    %51 = arith.divui %34, %c13 : index
                    %52 = arith.remui %34, %c13 : index
                    %53 = arts.db_ref %arg1[%51] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    memref.store %50, %53[%52, %23] : memref<?x?xf64>
                  }
                }
              }
            } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:114:3">}
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
          }
        }
      } {arts.id = 84 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:114:3">}
    } : i64
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %11 = arith.index_cast %arg0 : index to i32
      %12 = arith.cmpi eq, %arg0, %c0 : index
      %13 = scf.if %12 -> (i1) {
        scf.yield %true : i1
      } else {
        %14 = arith.cmpi eq, %11, %c99_i32 : i32
        scf.yield %14 : i1
      }
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %14 = arith.index_cast %arg1 : index to i32
        %15 = scf.if %13 -> (i1) {
          scf.yield %true : i1
        } else {
          %17 = arith.cmpi eq, %14, %c0_i32 : i32
          scf.yield %17 : i1
        }
        %16 = scf.if %15 -> (i1) {
          scf.yield %true : i1
        } else {
          %17 = arith.cmpi eq, %14, %c99_i32 : i32
          scf.yield %17 : i1
        }
        scf.if %16 {
          %17 = arith.divui %arg0, %c13 : index
          %18 = arith.remui %arg0, %c13 : index
          %19 = arts.db_ref %ptr[%17] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          %20 = memref.load %19[%18, %arg1] : memref<?x?xf64>
          %21 = arts.db_ref %ptr_8[%17] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          memref.store %20, %21[%18, %arg1] : memref<?x?xf64>
        } else {
          %17 = arith.divui %arg0, %c13 : index
          %18 = arith.remui %arg0, %c13 : index
          %19 = arts.db_ref %ptr_8[%17] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          memref.store %cst_4, %19[%18, %arg1] : memref<?x?xf64>
        }
      } {arts.id = 88 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:119:23">}
    } {arts.id = 86 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:118:21">}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    scf.for %arg0 = %c1 to %c11 step %c1 {
      %11 = arith.subi %arg0, %c1 : index
      %12 = arith.remui %11, %c2 : index
      %13 = arith.cmpi eq, %12, %c0 : index
      scf.if %13 {
        %14 = arts.epoch attributes {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} {
          scf.for %arg1 = %c0 to %c8 step %c1 {
            %15 = arith.muli %arg1, %c13 : index
            %16 = arith.cmpi uge, %15, %c100 : index
            %17 = arith.subi %c100, %15 : index
            %18 = arith.select %16, %c0, %17 : index
            %19 = arith.minui %18, %c13 : index
            %20 = arith.cmpi ugt, %19, %c0 : index
            scf.if %20 {
              %21 = arith.cmpi uge, %15, %c1 : index
              %22 = arith.subi %15, %c1 : index
              %23 = arith.select %21, %22, %c0 : index
              %24 = arith.addi %23, %c1 : index
              %25 = arith.divui %24, %c13 : index
              %26 = arith.subi %25, %c1 : index
              %27 = arith.cmpi ne, %25, %c0 : index
              %guid_9, %ptr_10 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%26], sizes[%c1] bounds_valid(%27) element_offsets[%c12, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_10 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %28 = arith.addi %25, %c1 : index
              %29 = arith.cmpi ne, %25, %c7 : index
              %guid_11, %ptr_12 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%28], sizes[%c1] bounds_valid(%29) element_offsets[%c0, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_12 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %guid_13, %ptr_14 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%25], sizes[%c1] element_offsets[%c0, %c0] element_sizes[%c13, %c100] {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, left_halo_arg_idx = 5 : index, right_halo_arg_idx = 6 : index, stencil_block_shape = [13, 100], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_14 : memref<?xmemref<?x?xf64>>) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c13] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              %30 = arith.divui %15, %c13 : index
              %31 = arith.addi %15, %c12 : index
              %32 = arith.divui %31, %c13 : index
              %33 = arith.cmpi ugt, %30, %c7 : index
              %34 = arith.minui %32, %c7 : index
              %35 = arith.select %33, %32, %34 : index
              %36 = arith.subi %35, %30 : index
              %37 = arith.addi %36, %c1 : index
              %38 = arith.select %33, %c0, %30 : index
              %39 = arith.select %33, %c0, %37 : index
              %guid_15, %ptr_16 = arts.db_acquire[<out>] (%guid_5 : memref<?xi64>, %ptr_6 : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%38], sizes[%39] {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [13, 100], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_16 : memref<?xmemref<?x?xf64>>) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c13] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              %guid_17, %ptr_18 = arts.db_acquire[<in>] (%guid_7 : memref<?xi64>, %ptr_8 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%26], sizes[%c1] bounds_valid(%27) element_offsets[%c12, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_18 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %guid_19, %ptr_20 = arts.db_acquire[<in>] (%guid_7 : memref<?xi64>, %ptr_8 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%28], sizes[%c1] bounds_valid(%29) element_offsets[%c0, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_20 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %guid_21, %ptr_22 = arts.db_acquire[<in>] (%guid_7 : memref<?xi64>, %ptr_8 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%25], sizes[%c1] element_offsets[%c0, %c0] element_sizes[%c13, %c100] {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, left_halo_arg_idx = 3 : index, right_halo_arg_idx = 4 : index, stencil_block_shape = [13, 100], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?xf64>>) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c13] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              arts.edt <task> <intranode> route(%c-1_i32) (%ptr_14, %ptr_16, %ptr_22, %ptr_18, %ptr_20, %ptr_10, %ptr_12) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 66 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [13, 100], stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} {
              ^bb0(%arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?x?xf64>>, %arg6: memref<?xmemref<?x?xf64>>, %arg7: memref<?xmemref<?x?xf64>>, %arg8: memref<?xmemref<?x?xf64>>):
                %40 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %41 = arts.db_ref %arg7[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %42 = arts.db_ref %arg8[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %43 = polygeist.memref2pointer %41 : memref<?x?xf64> to !llvm.ptr
                %44 = llvm.icmp "ne" %43, %0 : !llvm.ptr
                %45 = polygeist.memref2pointer %42 : memref<?x?xf64> to !llvm.ptr
                %46 = llvm.icmp "ne" %45, %0 : !llvm.ptr
                %47 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %48 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %49 = arts.db_ref %arg6[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %50 = polygeist.memref2pointer %48 : memref<?x?xf64> to !llvm.ptr
                %51 = llvm.icmp "ne" %50, %0 : !llvm.ptr
                %52 = polygeist.memref2pointer %49 : memref<?x?xf64> to !llvm.ptr
                %53 = llvm.icmp "ne" %52, %0 : !llvm.ptr
                %54 = arith.subi %c0, %15 : index
                %55 = arith.cmpi slt, %54, %c0 : index
                %56 = arith.select %55, %c0, %54 : index
                %57 = arith.cmpi slt, %17, %c0 : index
                %58 = arith.select %57, %c0, %17 : index
                %59 = arith.minui %58, %19 : index
                scf.for %arg9 = %56 to %59 step %c1 {
                  %60 = arith.cmpi slt, %arg9, %c0 : index
                  %61 = arith.cmpi sge, %arg9, %c13 : index
                  %62 = arith.addi %arg9, %c1 : index
                  %63 = arith.maxsi %arg9, %c0 : index
                  %64 = arith.minsi %63, %c12 : index
                  %65 = arith.subi %arg9, %c13 : index
                  %66 = arith.maxsi %64, %c0 : index
                  %67 = arith.minsi %66, %c12 : index
                  %68 = arith.maxsi %62, %c0 : index
                  %69 = arith.minsi %68, %c0 : index
                  %70 = arith.maxsi %65, %c0 : index
                  %71 = arith.minsi %70, %c0 : index
                  %72 = arith.select %44, %69, %67 : index
                  %73 = arith.select %60, %72, %67 : index
                  %74 = arith.select %46, %71, %67 : index
                  %75 = arith.select %61, %74, %73 : index
                  %76 = arith.addi %arg9, %c-1 : index
                  %77 = arith.cmpi slt, %76, %c0 : index
                  %78 = arith.cmpi sge, %76, %c13 : index
                  %79 = arith.maxsi %76, %c0 : index
                  %80 = arith.minsi %79, %c12 : index
                  %81 = arith.addi %arg9, %c-14 : index
                  %82 = arith.maxsi %80, %c0 : index
                  %83 = arith.minsi %82, %c12 : index
                  %84 = arith.minsi %63, %c0 : index
                  %85 = arith.maxsi %81, %c0 : index
                  %86 = arith.minsi %85, %c0 : index
                  %87 = arith.select %51, %84, %83 : index
                  %88 = arith.select %77, %87, %83 : index
                  %89 = arith.select %53, %86, %83 : index
                  %90 = arith.select %78, %89, %88 : index
                  %91 = arith.select %51, %69, %67 : index
                  %92 = arith.select %60, %91, %67 : index
                  %93 = arith.select %53, %71, %67 : index
                  %94 = arith.select %61, %93, %92 : index
                  %95 = arith.cmpi slt, %62, %c0 : index
                  %96 = arith.cmpi sge, %62, %c13 : index
                  %97 = arith.addi %arg9, %c2 : index
                  %98 = arith.minsi %68, %c12 : index
                  %99 = arith.addi %arg9, %c-12 : index
                  %100 = arith.maxsi %98, %c0 : index
                  %101 = arith.minsi %100, %c12 : index
                  %102 = arith.maxsi %97, %c0 : index
                  %103 = arith.minsi %102, %c0 : index
                  %104 = arith.maxsi %99, %c0 : index
                  %105 = arith.minsi %104, %c0 : index
                  %106 = arith.select %51, %103, %101 : index
                  %107 = arith.select %95, %106, %101 : index
                  %108 = arith.select %53, %105, %101 : index
                  %109 = arith.select %96, %108, %107 : index
                  %110 = arith.addi %15, %arg9 : index
                  %111 = arith.index_cast %110 : index to i32
                  %112 = arith.cmpi eq, %111, %c0_i32 : i32
                  %113 = arith.cmpi eq, %111, %c99_i32 : i32
                  scf.for %arg10 = %c0 to %c100 step %c1 {
                    %114 = arith.index_cast %arg10 : index to i32
                    %115 = scf.if %112 -> (i1) {
                      scf.yield %true : i1
                    } else {
                      %118 = arith.cmpi eq, %114, %c0_i32 : i32
                      scf.yield %118 : i1
                    }
                    %116 = arith.select %115, %true, %113 : i1
                    %117 = scf.if %116 -> (i1) {
                      scf.yield %true : i1
                    } else {
                      %118 = arith.cmpi eq, %114, %c99_i32 : i32
                      scf.yield %118 : i1
                    }
                    scf.if %117 {
                      %118 = scf.if %61 -> (f64) {
                        %123 = scf.if %46 -> (f64) {
                          %124 = memref.load %42[%75, %arg10] : memref<?x?xf64>
                          scf.yield %124 : f64
                        } else {
                          %124 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %124 : f64
                        }
                        scf.yield %123 : f64
                      } else {
                        %123 = scf.if %60 -> (f64) {
                          %124 = scf.if %44 -> (f64) {
                            %125 = memref.load %41[%75, %arg10] : memref<?x?xf64>
                            scf.yield %125 : f64
                          } else {
                            %125 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                            scf.yield %125 : f64
                          }
                          scf.yield %124 : f64
                        } else {
                          %124 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %124 : f64
                        }
                        scf.yield %123 : f64
                      }
                      %119 = arith.addi %15, %56 : index
                      %120 = arith.divui %119, %c13 : index
                      %121 = arith.subi %120, %38 : index
                      %122 = arts.db_ref %arg3[%121] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      memref.store %118, %122[%arg9, %arg10] : memref<?x?xf64>
                    } else {
                      %118 = scf.if %78 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%90, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%90, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %77 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%90, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%90, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%90, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %119 = arith.addi %arg10, %c1 : index
                      %120 = scf.if %61 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%94, %119] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %119] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %60 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%94, %119] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%94, %119] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %119] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %121 = arith.addf %118, %120 : f64
                      %122 = arith.addi %arg10, %c-1 : index
                      %123 = scf.if %61 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%94, %122] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %122] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %60 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%94, %122] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%94, %122] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %122] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %124 = arith.addf %121, %123 : f64
                      %125 = scf.if %96 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%109, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%109, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %95 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%109, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%109, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%109, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %126 = arith.addf %124, %125 : f64
                      %127 = scf.if %61 -> (f64) {
                        %136 = scf.if %46 -> (f64) {
                          %137 = memref.load %42[%75, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %60 -> (f64) {
                          %137 = scf.if %44 -> (f64) {
                            %138 = memref.load %41[%75, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %128 = arith.mulf %127, %cst_0 : f64
                      %129 = arith.mulf %128, %cst_0 : f64
                      %130 = arith.addf %126, %129 : f64
                      %131 = arith.mulf %130, %cst : f64
                      %132 = arith.addi %15, %56 : index
                      %133 = arith.divui %132, %c13 : index
                      %134 = arith.subi %133, %38 : index
                      %135 = arts.db_ref %arg3[%134] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      memref.store %131, %135[%arg9, %arg10] : memref<?x?xf64>
                    }
                  } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
                } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
                arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg5) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg6) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg7) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg8) : memref<?xmemref<?x?xf64>>
              }
            }
          } {arts.id = 111 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
        } : i64
      } else {
        %14 = arts.epoch attributes {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} {
          scf.for %arg1 = %c0 to %c8 step %c1 {
            %15 = arith.muli %arg1, %c13 : index
            %16 = arith.cmpi uge, %15, %c100 : index
            %17 = arith.subi %c100, %15 : index
            %18 = arith.select %16, %c0, %17 : index
            %19 = arith.minui %18, %c13 : index
            %20 = arith.cmpi ugt, %19, %c0 : index
            scf.if %20 {
              %21 = arith.cmpi uge, %15, %c1 : index
              %22 = arith.subi %15, %c1 : index
              %23 = arith.select %21, %22, %c0 : index
              %24 = arith.addi %23, %c1 : index
              %25 = arith.divui %24, %c13 : index
              %26 = arith.subi %25, %c1 : index
              %27 = arith.cmpi ne, %25, %c0 : index
              %guid_9, %ptr_10 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%26], sizes[%c1] bounds_valid(%27) element_offsets[%c12, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_10 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %28 = arith.addi %25, %c1 : index
              %29 = arith.cmpi ne, %25, %c7 : index
              %guid_11, %ptr_12 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%28], sizes[%c1] bounds_valid(%29) element_offsets[%c0, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_12 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %guid_13, %ptr_14 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%25], sizes[%c1] element_offsets[%c0, %c0] element_sizes[%c13, %c100] {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, left_halo_arg_idx = 5 : index, right_halo_arg_idx = 6 : index, stencil_block_shape = [13, 100], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_14 : memref<?xmemref<?x?xf64>>) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c13] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              %30 = arith.divui %15, %c13 : index
              %31 = arith.addi %15, %c12 : index
              %32 = arith.divui %31, %c13 : index
              %33 = arith.cmpi ugt, %30, %c7 : index
              %34 = arith.minui %32, %c7 : index
              %35 = arith.select %33, %32, %34 : index
              %36 = arith.subi %35, %30 : index
              %37 = arith.addi %36, %c1 : index
              %38 = arith.select %33, %c0, %30 : index
              %39 = arith.select %33, %c0, %37 : index
              %guid_15, %ptr_16 = arts.db_acquire[<out>] (%guid_7 : memref<?xi64>, %ptr_8 : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%38], sizes[%39] {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [13, 100], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_16 : memref<?xmemref<?x?xf64>>) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c13] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              %guid_17, %ptr_18 = arts.db_acquire[<in>] (%guid_5 : memref<?xi64>, %ptr_6 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%26], sizes[%c1] bounds_valid(%27) element_offsets[%c12, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_18 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %guid_19, %ptr_20 = arts.db_acquire[<in>] (%guid_5 : memref<?xi64>, %ptr_6 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%28], sizes[%c1] bounds_valid(%29) element_offsets[%c0, %c0] element_sizes[%c1, %c100] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_20 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
              %guid_21, %ptr_22 = arts.db_acquire[<in>] (%guid_5 : memref<?xi64>, %ptr_6 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%25], sizes[%c1] element_offsets[%c0, %c0] element_sizes[%c13, %c100] {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, left_halo_arg_idx = 3 : index, right_halo_arg_idx = 4 : index, stencil_block_shape = [13, 100], stencil_center_offset = 1 : i64, stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?xf64>>) dep_pattern(<jacobi_alternating_buffers>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c13] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined, spatial_dims = array<i64: 0, 1>, supported_block_halo}
              arts.edt <task> <intranode> route(%c-1_i32) (%ptr_14, %ptr_16, %ptr_22, %ptr_18, %ptr_20, %ptr_10, %ptr_12) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 68 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [13, 100], stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} {
              ^bb0(%arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?x?xf64>>, %arg6: memref<?xmemref<?x?xf64>>, %arg7: memref<?xmemref<?x?xf64>>, %arg8: memref<?xmemref<?x?xf64>>):
                %40 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %41 = arts.db_ref %arg7[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %42 = arts.db_ref %arg8[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %43 = polygeist.memref2pointer %41 : memref<?x?xf64> to !llvm.ptr
                %44 = llvm.icmp "ne" %43, %0 : !llvm.ptr
                %45 = polygeist.memref2pointer %42 : memref<?x?xf64> to !llvm.ptr
                %46 = llvm.icmp "ne" %45, %0 : !llvm.ptr
                %47 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %48 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %49 = arts.db_ref %arg6[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %50 = polygeist.memref2pointer %48 : memref<?x?xf64> to !llvm.ptr
                %51 = llvm.icmp "ne" %50, %0 : !llvm.ptr
                %52 = polygeist.memref2pointer %49 : memref<?x?xf64> to !llvm.ptr
                %53 = llvm.icmp "ne" %52, %0 : !llvm.ptr
                %54 = arith.subi %c0, %15 : index
                %55 = arith.cmpi slt, %54, %c0 : index
                %56 = arith.select %55, %c0, %54 : index
                %57 = arith.cmpi slt, %17, %c0 : index
                %58 = arith.select %57, %c0, %17 : index
                %59 = arith.minui %58, %19 : index
                scf.for %arg9 = %56 to %59 step %c1 {
                  %60 = arith.cmpi slt, %arg9, %c0 : index
                  %61 = arith.cmpi sge, %arg9, %c13 : index
                  %62 = arith.addi %arg9, %c1 : index
                  %63 = arith.maxsi %arg9, %c0 : index
                  %64 = arith.minsi %63, %c12 : index
                  %65 = arith.subi %arg9, %c13 : index
                  %66 = arith.maxsi %64, %c0 : index
                  %67 = arith.minsi %66, %c12 : index
                  %68 = arith.maxsi %62, %c0 : index
                  %69 = arith.minsi %68, %c0 : index
                  %70 = arith.maxsi %65, %c0 : index
                  %71 = arith.minsi %70, %c0 : index
                  %72 = arith.select %44, %69, %67 : index
                  %73 = arith.select %60, %72, %67 : index
                  %74 = arith.select %46, %71, %67 : index
                  %75 = arith.select %61, %74, %73 : index
                  %76 = arith.addi %arg9, %c-1 : index
                  %77 = arith.cmpi slt, %76, %c0 : index
                  %78 = arith.cmpi sge, %76, %c13 : index
                  %79 = arith.maxsi %76, %c0 : index
                  %80 = arith.minsi %79, %c12 : index
                  %81 = arith.addi %arg9, %c-14 : index
                  %82 = arith.maxsi %80, %c0 : index
                  %83 = arith.minsi %82, %c12 : index
                  %84 = arith.minsi %63, %c0 : index
                  %85 = arith.maxsi %81, %c0 : index
                  %86 = arith.minsi %85, %c0 : index
                  %87 = arith.select %51, %84, %83 : index
                  %88 = arith.select %77, %87, %83 : index
                  %89 = arith.select %53, %86, %83 : index
                  %90 = arith.select %78, %89, %88 : index
                  %91 = arith.select %51, %69, %67 : index
                  %92 = arith.select %60, %91, %67 : index
                  %93 = arith.select %53, %71, %67 : index
                  %94 = arith.select %61, %93, %92 : index
                  %95 = arith.cmpi slt, %62, %c0 : index
                  %96 = arith.cmpi sge, %62, %c13 : index
                  %97 = arith.addi %arg9, %c2 : index
                  %98 = arith.minsi %68, %c12 : index
                  %99 = arith.addi %arg9, %c-12 : index
                  %100 = arith.maxsi %98, %c0 : index
                  %101 = arith.minsi %100, %c12 : index
                  %102 = arith.maxsi %97, %c0 : index
                  %103 = arith.minsi %102, %c0 : index
                  %104 = arith.maxsi %99, %c0 : index
                  %105 = arith.minsi %104, %c0 : index
                  %106 = arith.select %51, %103, %101 : index
                  %107 = arith.select %95, %106, %101 : index
                  %108 = arith.select %53, %105, %101 : index
                  %109 = arith.select %96, %108, %107 : index
                  %110 = arith.addi %15, %arg9 : index
                  %111 = arith.index_cast %110 : index to i32
                  %112 = arith.cmpi eq, %111, %c0_i32 : i32
                  %113 = arith.cmpi eq, %111, %c99_i32 : i32
                  scf.for %arg10 = %c0 to %c100 step %c1 {
                    %114 = arith.index_cast %arg10 : index to i32
                    %115 = scf.if %112 -> (i1) {
                      scf.yield %true : i1
                    } else {
                      %118 = arith.cmpi eq, %114, %c0_i32 : i32
                      scf.yield %118 : i1
                    }
                    %116 = arith.select %115, %true, %113 : i1
                    %117 = scf.if %116 -> (i1) {
                      scf.yield %true : i1
                    } else {
                      %118 = arith.cmpi eq, %114, %c99_i32 : i32
                      scf.yield %118 : i1
                    }
                    scf.if %117 {
                      %118 = scf.if %61 -> (f64) {
                        %123 = scf.if %46 -> (f64) {
                          %124 = memref.load %42[%75, %arg10] : memref<?x?xf64>
                          scf.yield %124 : f64
                        } else {
                          %124 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %124 : f64
                        }
                        scf.yield %123 : f64
                      } else {
                        %123 = scf.if %60 -> (f64) {
                          %124 = scf.if %44 -> (f64) {
                            %125 = memref.load %41[%75, %arg10] : memref<?x?xf64>
                            scf.yield %125 : f64
                          } else {
                            %125 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                            scf.yield %125 : f64
                          }
                          scf.yield %124 : f64
                        } else {
                          %124 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %124 : f64
                        }
                        scf.yield %123 : f64
                      }
                      %119 = arith.addi %15, %56 : index
                      %120 = arith.divui %119, %c13 : index
                      %121 = arith.subi %120, %38 : index
                      %122 = arts.db_ref %arg3[%121] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      memref.store %118, %122[%arg9, %arg10] : memref<?x?xf64>
                    } else {
                      %118 = scf.if %78 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%90, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%90, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %77 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%90, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%90, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%90, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %119 = arith.addi %arg10, %c1 : index
                      %120 = scf.if %61 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%94, %119] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %119] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %60 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%94, %119] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%94, %119] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %119] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %121 = arith.addf %118, %120 : f64
                      %122 = arith.addi %arg10, %c-1 : index
                      %123 = scf.if %61 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%94, %122] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %122] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %60 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%94, %122] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%94, %122] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%94, %122] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %124 = arith.addf %121, %123 : f64
                      %125 = scf.if %96 -> (f64) {
                        %136 = scf.if %53 -> (f64) {
                          %137 = memref.load %49[%109, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%109, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %95 -> (f64) {
                          %137 = scf.if %51 -> (f64) {
                            %138 = memref.load %48[%109, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %47[%109, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %47[%109, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %126 = arith.addf %124, %125 : f64
                      %127 = scf.if %61 -> (f64) {
                        %136 = scf.if %46 -> (f64) {
                          %137 = memref.load %42[%75, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      } else {
                        %136 = scf.if %60 -> (f64) {
                          %137 = scf.if %44 -> (f64) {
                            %138 = memref.load %41[%75, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          } else {
                            %138 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                            scf.yield %138 : f64
                          }
                          scf.yield %137 : f64
                        } else {
                          %137 = memref.load %40[%75, %arg10] : memref<?x?xf64>
                          scf.yield %137 : f64
                        }
                        scf.yield %136 : f64
                      }
                      %128 = arith.mulf %127, %cst_0 : f64
                      %129 = arith.mulf %128, %cst_0 : f64
                      %130 = arith.addf %126, %129 : f64
                      %131 = arith.mulf %130, %cst : f64
                      %132 = arith.addi %15, %56 : index
                      %133 = arith.divui %132, %c13 : index
                      %134 = arith.subi %133, %38 : index
                      %135 = arts.db_ref %arg3[%134] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      memref.store %131, %135[%arg9, %arg10] : memref<?x?xf64>
                    }
                  } {arts.id = 143 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
                } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
                arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg5) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg6) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg7) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg8) : memref<?xmemref<?x?xf64>>
              }
            }
          } {arts.id = 145 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
        } : i64
      } {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]}
    } {arts.id = 147 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %9 = scf.for %arg0 = %c0 to %c8 step %c1 iter_args(%arg1 = %cst_4) -> (f64) {
      %11 = arith.muli %arg0, %c13 : index
      %12 = arith.subi %c100, %11 : index
      %13 = arith.cmpi uge, %11, %c100 : index
      %14 = arith.select %13, %c0, %12 : index
      %15 = arith.minui %14, %c13 : index
      %16 = arts.db_ref %ptr_8[%arg0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      %17 = scf.for %arg2 = %c0 to %15 step %c1 iter_args(%arg3 = %arg1) -> (f64) {
        %18 = arith.addi %11, %arg2 : index
        %19 = memref.load %16[%arg2, %18] : memref<?x?xf64>
        %20 = arith.addf %arg3, %19 : f64
        scf.yield %20 : f64
      }
      scf.yield %17 : f64
    }
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_5) : memref<?xi64>
    arts.db_free(%ptr_6) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_7) : memref<?xi64>
    arts.db_free(%ptr_8) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
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
