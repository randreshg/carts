module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_vel4sg_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-8 = arith.constant -8 : index
    %c8 = arith.constant 8 : index
    %c54 = arith.constant 54 : index
    %c5 = arith.constant 5 : index
    %c6 = arith.constant 6 : index
    %c9 = arith.constant 9 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 5.000000e-04 : f32
    %cst_0 = arith.constant 5.000000e-01 : f32
    %c-1 = arith.constant -1 : index
    %cst_1 = arith.constant 1.000000e+00 : f32
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c46 = arith.constant 46 : index
    %cst_2 = arith.constant 0.000000e+00 : f32
    %cst_3 = arith.constant 2.500000e+03 : f32
    %c13_i32 = arith.constant 13 : i32
    %cst_4 = arith.constant 0.00999999977 : f32
    %c7_i32 = arith.constant 7 : i32
    %c19_i32 = arith.constant 19 : i32
    %c11_i32 = arith.constant 11 : i32
    %c23_i32 = arith.constant 23 : i32
    %c17_i32 = arith.constant 17 : i32
    %cst_5 = arith.constant 5.000000e-03 : f32
    %c5_i32 = arith.constant 5 : i32
    %c29_i32 = arith.constant 29 : i32
    %cst_6 = arith.constant 4.000000e-03 : f32
    %c3_i32 = arith.constant 3 : i32
    %c31_i32 = arith.constant 31 : i32
    %cst_7 = arith.constant 6.000000e-03 : f32
    %c2_i32 = arith.constant 2 : i32
    %c37_i32 = arith.constant 37 : i32
    %c1 = arith.constant 1 : index
    %c48 = arith.constant 48 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_8 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 475 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_9, %ptr_10 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 476 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_10 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_11, %ptr_12 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 477 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_12 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_13, %ptr_14 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 478 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_14 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_15, %ptr_16 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 479 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_16 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_17, %ptr_18 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 480 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_18 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_19, %ptr_20 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 481 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_20 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_21, %ptr_22 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 482 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_23, %ptr_24 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 483 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_25, %ptr_26 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c9] {arts.id = 484 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_26 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %6 = scf.for %arg0 = %c0 to %c48 step %c1 iter_args(%arg1 = %c0_i32) -> (i32) {
      %11 = scf.for %arg2 = %c0 to %c48 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %12 = arith.index_cast %arg3 : i32 to index
        %13 = arith.addi %12, %c48 : index
        %14 = arith.index_cast %13 : index to i32
        scf.for %arg4 = %c0 to %c6 step %c1 {
          %15 = arith.muli %arg4, %c9 : index
          %16 = arith.subi %c48, %15 : index
          %17 = arith.cmpi uge, %15, %c48 : index
          %18 = arith.select %17, %c0, %16 : index
          %19 = arith.minui %18, %c9 : index
          %20 = arts.db_ref %ptr[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %21 = arts.db_ref %ptr_10[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %22 = arts.db_ref %ptr_12[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %23 = arts.db_ref %ptr_14[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %24 = arts.db_ref %ptr_16[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %25 = arts.db_ref %ptr_18[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %26 = arts.db_ref %ptr_20[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %27 = arts.db_ref %ptr_22[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %28 = arts.db_ref %ptr_24[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %29 = arts.db_ref %ptr_26[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          scf.for %arg5 = %c0 to %19 step %c1 {
            %30 = arith.addi %15, %arg5 : index
            %31 = arith.addi %12, %30 : index
            %32 = arith.index_cast %31 : index to i32
            memref.store %cst_2, %20[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            memref.store %cst_2, %21[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            memref.store %cst_2, %22[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %33 = arith.remsi %32, %c13_i32 : i32
            %34 = arith.sitofp %33 : i32 to f32
            %35 = arith.addf %34, %cst_3 : f32
            memref.store %35, %23[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %36 = arith.muli %32, %c7_i32 : i32
            %37 = arith.remsi %36, %c19_i32 : i32
            %38 = arith.sitofp %37 : i32 to f32
            %39 = arith.mulf %38, %cst_4 : f32
            memref.store %39, %24[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %40 = arith.muli %32, %c11_i32 : i32
            %41 = arith.remsi %40, %c23_i32 : i32
            %42 = arith.sitofp %41 : i32 to f32
            %43 = arith.mulf %42, %cst_4 : f32
            memref.store %43, %25[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %44 = arith.muli %32, %c13_i32 : i32
            %45 = arith.remsi %44, %c17_i32 : i32
            %46 = arith.sitofp %45 : i32 to f32
            %47 = arith.mulf %46, %cst_4 : f32
            memref.store %47, %26[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %48 = arith.muli %32, %c5_i32 : i32
            %49 = arith.remsi %48, %c29_i32 : i32
            %50 = arith.sitofp %49 : i32 to f32
            %51 = arith.mulf %50, %cst_5 : f32
            memref.store %51, %27[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %52 = arith.muli %32, %c3_i32 : i32
            %53 = arith.remsi %52, %c31_i32 : i32
            %54 = arith.sitofp %53 : i32 to f32
            %55 = arith.mulf %54, %cst_6 : f32
            memref.store %55, %28[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %56 = arith.muli %32, %c2_i32 : i32
            %57 = arith.remsi %56, %c37_i32 : i32
            %58 = arith.sitofp %57 : i32 to f32
            %59 = arith.mulf %58, %cst_7 : f32
            memref.store %59, %29[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
          }
        }
        scf.yield %14 : i32
      } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:135:3">}
      scf.yield %11 : i32
    } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:135:3">}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %7 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c6 step %c1 {
        %11 = arith.cmpi ult, %arg0, %c6 : index
        %12 = arith.select %11, %c1, %c0 : index
        %13 = arith.minui %arg0, %c6 : index
        %14 = arith.muli %13, %c9 : index
        %15 = arith.muli %12, %c9 : index
        %16 = arith.cmpi uge, %14, %c46 : index
        %17 = arith.subi %c46, %14 : index
        %18 = arith.select %16, %c0, %17 : index
        %19 = arith.minui %15, %18 : index
        %20 = arith.cmpi ugt, %19, %c0 : index
        scf.if %20 {
          %21 = arith.divui %14, %c9 : index
          %22 = arith.addi %14, %15 : index
          %23 = arith.subi %22, %c1 : index
          %24 = arith.divui %23, %c9 : index
          %25 = arith.cmpi ugt, %21, %c5 : index
          %26 = arith.minui %24, %c5 : index
          %27 = arith.select %25, %24, %26 : index
          %28 = arith.subi %27, %21 : index
          %29 = arith.addi %28, %c1 : index
          %30 = arith.select %25, %c0, %21 : index
          %31 = arith.select %25, %c0, %29 : index
          %guid_27, %ptr_28 = arts.db_acquire[<in>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] element_offsets[%c0, %c0, %14] element_sizes[%c48, %c48, %15] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_28 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_29, %ptr_30 = arts.db_acquire[<in>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] element_offsets[%c0, %c0, %14] element_sizes[%c48, %c48, %15] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_30 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_31, %ptr_32 = arts.db_acquire[<in>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] element_offsets[%c0, %c0, %14] element_sizes[%c48, %c48, %15] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_32 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %32 = arith.addi %14, %19 : index
          %33 = arith.cmpi uge, %14, %c1 : index
          %34 = arith.subi %14, %c1 : index
          %35 = arith.select %33, %34, %c0 : index
          %36 = arith.addi %32, %c1 : index
          %37 = arith.minui %36, %c48 : index
          %38 = arith.cmpi uge, %37, %35 : index
          %39 = arith.subi %37, %35 : index
          %40 = arith.select %38, %39, %c0 : index
          %41 = arith.cmpi uge, %35, %c1 : index
          %42 = arith.subi %35, %c1 : index
          %43 = arith.select %41, %42, %c0 : index
          %44 = arith.addi %35, %40 : index
          %45 = arith.addi %44, %c1 : index
          %46 = arith.minui %45, %c54 : index
          %47 = arith.cmpi uge, %46, %43 : index
          %48 = arith.subi %46, %43 : index
          %49 = arith.select %47, %48, %c0 : index
          %50 = arith.divui %43, %c9 : index
          %51 = arith.addi %43, %49 : index
          %52 = arith.subi %51, %c1 : index
          %53 = arith.divui %52, %c9 : index
          %54 = arith.cmpi ugt, %50, %c5 : index
          %55 = arith.minui %53, %c5 : index
          %56 = arith.select %54, %53, %55 : index
          %57 = arith.subi %56, %50 : index
          %58 = arith.addi %57, %c1 : index
          %59 = arith.select %54, %c0, %50 : index
          %60 = arith.select %54, %c0, %58 : index
          %guid_33, %ptr_34 = arts.db_acquire[<in>] (%guid_23 : memref<?xi64>, %ptr_24 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%59], sizes[%60] element_offsets[%c0, %c0, %43] element_sizes[%c48, %c48, %49] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_34 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_35, %ptr_36 = arts.db_acquire[<in>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] element_offsets[%c0, %c0, %14] element_sizes[%c48, %c48, %15] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_36 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_37, %ptr_38 = arts.db_acquire[<in>] (%guid_25 : memref<?xi64>, %ptr_26 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%59], sizes[%60] element_offsets[%c0, %c0, %43] element_sizes[%c48, %c48, %49] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_38 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_39, %ptr_40 = arts.db_acquire[<in>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%59], sizes[%60] element_offsets[%c0, %c0, %43] element_sizes[%c48, %c48, %49] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_40 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_41, %ptr_42 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_42 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_43, %ptr_44 = arts.db_acquire[<inout>] (%guid_9 : memref<?xi64>, %ptr_10 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_44 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_45, %ptr_46 = arts.db_acquire[<inout>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_46 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_28, %ptr_30, %ptr_32, %ptr_34, %ptr_36, %ptr_38, %ptr_40, %ptr_42, %ptr_44, %ptr_46) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>> attributes {arts.id = 485 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf32>>, %arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?xf32>>, %arg5: memref<?xmemref<?x?x?xf32>>, %arg6: memref<?xmemref<?x?x?xf32>>, %arg7: memref<?xmemref<?x?x?xf32>>, %arg8: memref<?xmemref<?x?x?xf32>>, %arg9: memref<?xmemref<?x?x?xf32>>, %arg10: memref<?xmemref<?x?x?xf32>>):
            %61 = arith.subi %c2, %14 : index
            %62 = arith.cmpi slt, %61, %c0 : index
            %63 = arith.select %62, %c0, %61 : index
            %64 = arith.cmpi slt, %17, %c0 : index
            %65 = arith.select %64, %c0, %17 : index
            %66 = arith.minui %65, %19 : index
            %67 = arith.addi %63, %14 : index
            %68 = arith.addi %66, %14 : index
            %69 = arith.divui %67, %c9 : index
            %70 = arith.addi %68, %c8 : index
            %71 = arith.divui %70, %c9 : index
            %72 = arith.cmpi ugt, %68, %67 : index
            %73 = arith.select %72, %71, %69 : index
            scf.for %arg11 = %69 to %73 step %c1 {
              %74 = arith.muli %arg11, %c9 : index
              %75 = arith.cmpi uge, %74, %67 : index
              %76 = arith.subi %67, %74 : index
              %77 = arith.select %75, %c0, %76 : index
              %78 = arith.minui %77, %c9 : index
              %79 = arith.cmpi uge, %74, %68 : index
              %80 = arith.subi %68, %74 : index
              %81 = arith.select %79, %c0, %80 : index
              %82 = arith.minui %81, %c9 : index
              %83 = arith.minui %82, %c1 : index
              %84 = arith.addi %arg11, %c-1 : index
              %85 = arith.subi %arg11, %30 : index
              %86 = arith.subi %arg11, %59 : index
              %87 = arith.subi %84, %59 : index
              %88 = arts.db_ref %arg1[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %89 = arts.db_ref %arg2[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %90 = arts.db_ref %arg3[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %91 = arts.db_ref %arg4[%86] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %92 = arts.db_ref %arg5[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %93 = arts.db_ref %arg6[%86] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %94 = arts.db_ref %arg7[%86] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %95 = arts.db_ref %arg8[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %96 = arts.db_ref %arg9[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %97 = arts.db_ref %arg10[%85] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %98 = arts.db_ref %arg4[%87] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %99 = arts.db_ref %arg6[%87] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %100 = arts.db_ref %arg7[%87] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              scf.for %arg12 = %78 to %83 step %c1 {
                %110 = arith.addi %arg12, %c8 : index
                %111 = arith.addi %arg12, %c1 : index
                scf.for %arg13 = %c2 to %c46 step %c1 {
                  %112 = arith.addi %arg13, %c1 : index
                  %113 = arith.addi %arg13, %c-1 : index
                  scf.for %arg14 = %c2 to %c46 step %c1 {
                    %114 = memref.load %88[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %115 = arith.divf %cst_1, %114 : f32
                    %116 = arith.addi %arg14, %c1 : index
                    %117 = memref.load %89[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %118 = arith.addi %arg14, %c-1 : index
                    %119 = memref.load %89[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %120 = arith.subf %117, %119 : f32
                    %121 = arith.mulf %120, %cst_0 : f32
                    %122 = memref.load %90[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %123 = memref.load %90[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %124 = arith.subf %122, %123 : f32
                    %125 = arith.mulf %124, %cst_0 : f32
                    %126 = arith.addf %121, %125 : f32
                    %127 = memref.load %91[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %128 = memref.load %98[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %129 = arith.subf %127, %128 : f32
                    %130 = arith.mulf %129, %cst_0 : f32
                    %131 = arith.addf %126, %130 : f32
                    %132 = memref.load %90[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %133 = memref.load %90[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %134 = arith.subf %132, %133 : f32
                    %135 = arith.mulf %134, %cst_0 : f32
                    %136 = memref.load %92[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %137 = memref.load %92[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %138 = arith.subf %136, %137 : f32
                    %139 = arith.mulf %138, %cst_0 : f32
                    %140 = arith.addf %135, %139 : f32
                    %141 = memref.load %93[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %142 = memref.load %99[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %143 = arith.subf %141, %142 : f32
                    %144 = arith.mulf %143, %cst_0 : f32
                    %145 = arith.addf %140, %144 : f32
                    %146 = memref.load %91[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %147 = memref.load %91[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %148 = arith.subf %146, %147 : f32
                    %149 = arith.mulf %148, %cst_0 : f32
                    %150 = memref.load %93[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %151 = memref.load %93[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %152 = arith.subf %150, %151 : f32
                    %153 = arith.mulf %152, %cst_0 : f32
                    %154 = arith.addf %149, %153 : f32
                    %155 = memref.load %94[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %156 = memref.load %100[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %157 = arith.subf %155, %156 : f32
                    %158 = arith.mulf %157, %cst_0 : f32
                    %159 = arith.addf %154, %158 : f32
                    %160 = arith.mulf %115, %cst : f32
                    %161 = arith.mulf %160, %131 : f32
                    %162 = memref.load %95[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %163 = arith.addf %162, %161 : f32
                    memref.store %163, %95[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %164 = arith.mulf %160, %145 : f32
                    %165 = memref.load %96[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %166 = arith.addf %165, %164 : f32
                    memref.store %166, %96[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %167 = arith.mulf %160, %159 : f32
                    %168 = memref.load %97[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %169 = arith.addf %168, %167 : f32
                    memref.store %169, %97[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                  } {arts.id = 178 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
                } {arts.id = 180 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
              }
              %101 = arith.maxui %78, %c1 : index
              %102 = arith.minui %82, %c8 : index
              scf.for %arg12 = %101 to %102 step %c1 {
                %110 = arith.addi %arg12, %c-1 : index
                %111 = arith.addi %arg12, %c1 : index
                scf.for %arg13 = %c2 to %c46 step %c1 {
                  %112 = arith.addi %arg13, %c1 : index
                  %113 = arith.addi %arg13, %c-1 : index
                  scf.for %arg14 = %c2 to %c46 step %c1 {
                    %114 = memref.load %88[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %115 = arith.divf %cst_1, %114 : f32
                    %116 = arith.addi %arg14, %c1 : index
                    %117 = memref.load %89[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %118 = arith.addi %arg14, %c-1 : index
                    %119 = memref.load %89[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %120 = arith.subf %117, %119 : f32
                    %121 = arith.mulf %120, %cst_0 : f32
                    %122 = memref.load %90[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %123 = memref.load %90[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %124 = arith.subf %122, %123 : f32
                    %125 = arith.mulf %124, %cst_0 : f32
                    %126 = arith.addf %121, %125 : f32
                    %127 = memref.load %91[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %128 = memref.load %91[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %129 = arith.subf %127, %128 : f32
                    %130 = arith.mulf %129, %cst_0 : f32
                    %131 = arith.addf %126, %130 : f32
                    %132 = memref.load %90[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %133 = memref.load %90[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %134 = arith.subf %132, %133 : f32
                    %135 = arith.mulf %134, %cst_0 : f32
                    %136 = memref.load %92[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %137 = memref.load %92[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %138 = arith.subf %136, %137 : f32
                    %139 = arith.mulf %138, %cst_0 : f32
                    %140 = arith.addf %135, %139 : f32
                    %141 = memref.load %93[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %142 = memref.load %93[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %143 = arith.subf %141, %142 : f32
                    %144 = arith.mulf %143, %cst_0 : f32
                    %145 = arith.addf %140, %144 : f32
                    %146 = memref.load %91[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %147 = memref.load %91[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %148 = arith.subf %146, %147 : f32
                    %149 = arith.mulf %148, %cst_0 : f32
                    %150 = memref.load %93[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %151 = memref.load %93[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %152 = arith.subf %150, %151 : f32
                    %153 = arith.mulf %152, %cst_0 : f32
                    %154 = arith.addf %149, %153 : f32
                    %155 = memref.load %94[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %156 = memref.load %94[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %157 = arith.subf %155, %156 : f32
                    %158 = arith.mulf %157, %cst_0 : f32
                    %159 = arith.addf %154, %158 : f32
                    %160 = arith.mulf %115, %cst : f32
                    %161 = arith.mulf %160, %131 : f32
                    %162 = memref.load %95[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %163 = arith.addf %162, %161 : f32
                    memref.store %163, %95[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %164 = arith.mulf %160, %145 : f32
                    %165 = memref.load %96[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %166 = arith.addf %165, %164 : f32
                    memref.store %166, %96[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %167 = arith.mulf %160, %159 : f32
                    %168 = memref.load %97[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %169 = arith.addf %168, %167 : f32
                    memref.store %169, %97[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                  } {arts.id = 178 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
                } {arts.id = 180 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
              }
              %103 = arith.maxui %78, %c8 : index
              %104 = arith.minui %82, %c9 : index
              %105 = arith.addi %arg11, %c1 : index
              %106 = arith.subi %105, %59 : index
              %107 = arts.db_ref %arg4[%106] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %108 = arts.db_ref %arg6[%106] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %109 = arts.db_ref %arg7[%106] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              scf.for %arg12 = %103 to %104 step %c1 {
                %110 = arith.addi %arg12, %c-1 : index
                %111 = arith.addi %arg12, %c-8 : index
                scf.for %arg13 = %c2 to %c46 step %c1 {
                  %112 = arith.addi %arg13, %c1 : index
                  %113 = arith.addi %arg13, %c-1 : index
                  scf.for %arg14 = %c2 to %c46 step %c1 {
                    %114 = memref.load %88[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %115 = arith.divf %cst_1, %114 : f32
                    %116 = arith.addi %arg14, %c1 : index
                    %117 = memref.load %89[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %118 = arith.addi %arg14, %c-1 : index
                    %119 = memref.load %89[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %120 = arith.subf %117, %119 : f32
                    %121 = arith.mulf %120, %cst_0 : f32
                    %122 = memref.load %90[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %123 = memref.load %90[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %124 = arith.subf %122, %123 : f32
                    %125 = arith.mulf %124, %cst_0 : f32
                    %126 = arith.addf %121, %125 : f32
                    %127 = memref.load %107[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %128 = memref.load %91[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %129 = arith.subf %127, %128 : f32
                    %130 = arith.mulf %129, %cst_0 : f32
                    %131 = arith.addf %126, %130 : f32
                    %132 = memref.load %90[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %133 = memref.load %90[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %134 = arith.subf %132, %133 : f32
                    %135 = arith.mulf %134, %cst_0 : f32
                    %136 = memref.load %92[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %137 = memref.load %92[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %138 = arith.subf %136, %137 : f32
                    %139 = arith.mulf %138, %cst_0 : f32
                    %140 = arith.addf %135, %139 : f32
                    %141 = memref.load %108[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %142 = memref.load %93[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %143 = arith.subf %141, %142 : f32
                    %144 = arith.mulf %143, %cst_0 : f32
                    %145 = arith.addf %140, %144 : f32
                    %146 = memref.load %91[%116, %arg13, %arg12] : memref<?x?x?xf32>
                    %147 = memref.load %91[%118, %arg13, %arg12] : memref<?x?x?xf32>
                    %148 = arith.subf %146, %147 : f32
                    %149 = arith.mulf %148, %cst_0 : f32
                    %150 = memref.load %93[%arg14, %112, %arg12] : memref<?x?x?xf32>
                    %151 = memref.load %93[%arg14, %113, %arg12] : memref<?x?x?xf32>
                    %152 = arith.subf %150, %151 : f32
                    %153 = arith.mulf %152, %cst_0 : f32
                    %154 = arith.addf %149, %153 : f32
                    %155 = memref.load %109[%arg14, %arg13, %111] : memref<?x?x?xf32>
                    %156 = memref.load %94[%arg14, %arg13, %110] : memref<?x?x?xf32>
                    %157 = arith.subf %155, %156 : f32
                    %158 = arith.mulf %157, %cst_0 : f32
                    %159 = arith.addf %154, %158 : f32
                    %160 = arith.mulf %115, %cst : f32
                    %161 = arith.mulf %160, %131 : f32
                    %162 = memref.load %95[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %163 = arith.addf %162, %161 : f32
                    memref.store %163, %95[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %164 = arith.mulf %160, %145 : f32
                    %165 = memref.load %96[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %166 = arith.addf %165, %164 : f32
                    memref.store %166, %96[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %167 = arith.mulf %160, %159 : f32
                    %168 = memref.load %97[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                    %169 = arith.addf %168, %167 : f32
                    memref.store %169, %97[%arg14, %arg13, %arg12] : memref<?x?x?xf32>
                  } {arts.id = 178 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
                } {arts.id = 180 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
              }
            }
            arts.db_release(%arg1) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg2) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg3) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg6) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg7) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg8) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg9) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg10) : memref<?xmemref<?x?x?xf32>>
          }
        }
      } {arts.id = 182 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 44 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">}
    } : i64
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %9 = scf.for %arg0 = %c0 to %c6 step %c1 iter_args(%arg1 = %cst_8) -> (f64) {
      %11 = arith.muli %arg0, %c9 : index
      %12 = arith.subi %c48, %11 : index
      %13 = arith.cmpi uge, %11, %c48 : index
      %14 = arith.select %13, %c0, %12 : index
      %15 = arith.minui %14, %c9 : index
      %16 = arts.db_ref %ptr[%arg0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
      %17 = arts.db_ref %ptr_10[%arg0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
      %18 = arts.db_ref %ptr_12[%arg0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
      %19 = scf.for %arg2 = %c0 to %15 step %c1 iter_args(%arg3 = %arg1) -> (f64) {
        %20 = arith.addi %11, %arg2 : index
        %21 = memref.load %16[%20, %20, %arg2] : memref<?x?x?xf32>
        %22 = arith.extf %21 : f32 to f64
        %23 = math.absf %22 : f64
        %24 = memref.load %17[%20, %20, %arg2] : memref<?x?x?xf32>
        %25 = arith.extf %24 : f32 to f64
        %26 = math.absf %25 : f64
        %27 = arith.addf %23, %26 : f64
        %28 = memref.load %18[%20, %20, %arg2] : memref<?x?x?xf32>
        %29 = arith.extf %28 : f32 to f64
        %30 = math.absf %29 : f64
        %31 = arith.addf %27, %30 : f64
        %32 = arith.addf %arg3, %31 : f64
        scf.yield %32 : f64
      }
      scf.yield %19 : f64
    }
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_9) : memref<?xi64>
    arts.db_free(%ptr_10) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_23) : memref<?xi64>
    arts.db_free(%ptr_24) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_25) : memref<?xi64>
    arts.db_free(%ptr_26) : memref<?xmemref<?x?x?xf32>>
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
