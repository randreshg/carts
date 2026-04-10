module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem3d_update_stress\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-8 = arith.constant -8 : index
    %c8 = arith.constant 8 : index
    %c45 = arith.constant 45 : index
    %c4 = arith.constant 4 : index
    %c5 = arith.constant 5 : index
    %c9 = arith.constant 9 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 2.000000e+00 : f64
    %cst_0 = arith.constant 5.000000e-01 : f64
    %c-1 = arith.constant -1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c38 = arith.constant 38 : index
    %cst_1 = arith.constant 1.000000e-03 : f64
    %c17_i32 = arith.constant 17 : i32
    %cst_2 = arith.constant 1.500000e-03 : f64
    %c3_i32 = arith.constant 3 : i32
    %c19_i32 = arith.constant 19 : i32
    %cst_3 = arith.constant 8.000000e-04 : f64
    %c5_i32 = arith.constant 5 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_4 = arith.constant 3.000000e+01 : f64
    %cst_5 = arith.constant 5.000000e-02 : f64
    %c11_i32 = arith.constant 11 : i32
    %cst_6 = arith.constant 2.000000e+01 : f64
    %cst_7 = arith.constant 4.000000e-02 : f64
    %c13_i32 = arith.constant 13 : i32
    %c1 = arith.constant 1 : index
    %c40 = arith.constant 40 : index
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
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 557 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_9, %ptr_10 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 558 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_10 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_11, %ptr_12 = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 559 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_12 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_13, %ptr_14 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 560 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_14 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_15, %ptr_16 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 561 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_16 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_17, %ptr_18 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 562 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_18 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_19, %ptr_20 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 563 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_20 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_21, %ptr_22 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 564 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_23, %ptr_24 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 565 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_25, %ptr_26 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 566 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_26 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_27, %ptr_28 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c5] elementType(f64) elementSizes[%c40, %c40, %c9] {arts.id = 567 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    arts.lowering_contract(%ptr_28 : memref<?xmemref<?x?x?xf64>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %6 = scf.for %arg0 = %c0 to %c40 step %c1 iter_args(%arg1 = %c0_i32) -> (i32) {
      %11 = scf.for %arg2 = %c0 to %c40 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %12 = arith.index_cast %arg3 : i32 to index
        %13 = arith.addi %12, %c40 : index
        %14 = arith.index_cast %13 : index to i32
        scf.for %arg4 = %c0 to %c5 step %c1 {
          %15 = arith.muli %arg4, %c9 : index
          %16 = arith.subi %c40, %15 : index
          %17 = arith.cmpi uge, %15, %c40 : index
          %18 = arith.select %17, %c0, %16 : index
          %19 = arith.minui %18, %c9 : index
          %20 = arts.db_ref %ptr[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %21 = arts.db_ref %ptr_10[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %22 = arts.db_ref %ptr_12[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %23 = arts.db_ref %ptr_14[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %24 = arts.db_ref %ptr_16[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %25 = arts.db_ref %ptr_28[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %26 = arts.db_ref %ptr_26[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %27 = arts.db_ref %ptr_24[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %28 = arts.db_ref %ptr_22[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %29 = arts.db_ref %ptr_20[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          %30 = arts.db_ref %ptr_18[%arg4] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          scf.for %arg5 = %c0 to %19 step %c1 {
            %31 = arith.addi %15, %arg5 : index
            %32 = arith.addi %12, %31 : index
            %33 = arith.index_cast %32 : index to i32
            %34 = arith.remsi %33, %c17_i32 : i32
            %35 = arith.sitofp %34 : i32 to f64
            %36 = arith.mulf %35, %cst_1 : f64
            memref.store %36, %20[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            %37 = arith.muli %33, %c3_i32 : i32
            %38 = arith.remsi %37, %c19_i32 : i32
            %39 = arith.sitofp %38 : i32 to f64
            %40 = arith.mulf %39, %cst_2 : f64
            memref.store %40, %21[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            %41 = arith.muli %33, %c5_i32 : i32
            %42 = arith.remsi %41, %c23_i32 : i32
            %43 = arith.sitofp %42 : i32 to f64
            %44 = arith.mulf %43, %cst_3 : f64
            memref.store %44, %22[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            %45 = arith.remsi %33, %c11_i32 : i32
            %46 = arith.sitofp %45 : i32 to f64
            %47 = arith.mulf %46, %cst_5 : f64
            %48 = arith.addf %47, %cst_4 : f64
            memref.store %48, %23[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            %49 = arith.remsi %33, %c13_i32 : i32
            %50 = arith.sitofp %49 : i32 to f64
            %51 = arith.mulf %50, %cst_7 : f64
            %52 = arith.addf %51, %cst_6 : f64
            memref.store %52, %24[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            memref.store %cst_8, %25[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            memref.store %cst_8, %26[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            memref.store %cst_8, %27[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            memref.store %cst_8, %28[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            memref.store %cst_8, %29[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
            memref.store %cst_8, %30[%arg0, %arg2, %arg5] : memref<?x?x?xf64>
          }
        }
        scf.yield %14 : i32
      } {arts.id = 113 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:139:3">}
      scf.yield %11 : i32
    } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:139:3">}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %7 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c5 step %c1 {
        %11 = arith.cmpi ult, %arg0, %c5 : index
        %12 = arith.select %11, %c1, %c0 : index
        %13 = arith.minui %arg0, %c5 : index
        %14 = arith.muli %13, %c9 : index
        %15 = arith.muli %12, %c9 : index
        %16 = arith.cmpi uge, %14, %c38 : index
        %17 = arith.subi %c38, %14 : index
        %18 = arith.select %16, %c0, %17 : index
        %19 = arith.minui %15, %18 : index
        %20 = arith.cmpi ugt, %19, %c0 : index
        scf.if %20 {
          %21 = arith.divui %14, %c9 : index
          %22 = arith.addi %14, %15 : index
          %23 = arith.subi %22, %c1 : index
          %24 = arith.divui %23, %c9 : index
          %25 = arith.cmpi ugt, %21, %c4 : index
          %26 = arith.minui %24, %c4 : index
          %27 = arith.select %25, %24, %26 : index
          %28 = arith.subi %27, %21 : index
          %29 = arith.addi %28, %c1 : index
          %30 = arith.select %25, %c0, %21 : index
          %31 = arith.select %25, %c0, %29 : index
          %guid_29, %ptr_30 = arts.db_acquire[<in>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] element_offsets[%c0, %c0, %14] element_sizes[%c40, %c40, %15] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_30 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_31, %ptr_32 = arts.db_acquire[<in>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] element_offsets[%c0, %c0, %14] element_sizes[%c40, %c40, %15] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_32 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %32 = arith.addi %14, %19 : index
          %33 = arith.cmpi uge, %14, %c1 : index
          %34 = arith.subi %14, %c1 : index
          %35 = arith.select %33, %34, %c0 : index
          %36 = arith.addi %32, %c1 : index
          %37 = arith.minui %36, %c40 : index
          %38 = arith.cmpi uge, %37, %35 : index
          %39 = arith.subi %37, %35 : index
          %40 = arith.select %38, %39, %c0 : index
          %41 = arith.cmpi uge, %35, %c1 : index
          %42 = arith.subi %35, %c1 : index
          %43 = arith.select %41, %42, %c0 : index
          %44 = arith.addi %35, %40 : index
          %45 = arith.addi %44, %c1 : index
          %46 = arith.minui %45, %c45 : index
          %47 = arith.cmpi uge, %46, %43 : index
          %48 = arith.subi %46, %43 : index
          %49 = arith.select %47, %48, %c0 : index
          %50 = arith.divui %43, %c9 : index
          %51 = arith.addi %43, %49 : index
          %52 = arith.subi %51, %c1 : index
          %53 = arith.divui %52, %c9 : index
          %54 = arith.cmpi ugt, %50, %c4 : index
          %55 = arith.minui %53, %c4 : index
          %56 = arith.select %54, %53, %55 : index
          %57 = arith.subi %56, %50 : index
          %58 = arith.addi %57, %c1 : index
          %59 = arith.select %54, %c0, %50 : index
          %60 = arith.select %54, %c0, %58 : index
          %guid_33, %ptr_34 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%59], sizes[%60] element_offsets[%c0, %c0, %43] element_sizes[%c40, %c40, %49] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_34 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_35, %ptr_36 = arts.db_acquire[<in>] (%guid_9 : memref<?xi64>, %ptr_10 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%59], sizes[%60] element_offsets[%c0, %c0, %43] element_sizes[%c40, %c40, %49] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_36 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_37, %ptr_38 = arts.db_acquire[<in>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%59], sizes[%60] element_offsets[%c0, %c0, %43] element_sizes[%c40, %c40, %49] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_38 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_39, %ptr_40 = arts.db_acquire[<inout>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_40 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_41, %ptr_42 = arts.db_acquire[<inout>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_42 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_43, %ptr_44 = arts.db_acquire[<inout>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_44 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_45, %ptr_46 = arts.db_acquire[<inout>] (%guid_23 : memref<?xi64>, %ptr_24 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_46 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_47, %ptr_48 = arts.db_acquire[<inout>] (%guid_25 : memref<?xi64>, %ptr_26 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_48 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_49, %ptr_50 = arts.db_acquire[<inout>] (%guid_27 : memref<?xi64>, %ptr_28 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%14], sizes[%15]), offsets[%30], sizes[%31] {arts.partition_hint = {blockSize = 9 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_50 : memref<?xmemref<?x?x?xf64>>) {critical_path_distance = 0 : i64}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_30, %ptr_32, %ptr_34, %ptr_36, %ptr_38, %ptr_40, %ptr_42, %ptr_44, %ptr_46, %ptr_48, %ptr_50) : memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>> attributes {arts.id = 568 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf64>>, %arg2: memref<?xmemref<?x?x?xf64>>, %arg3: memref<?xmemref<?x?x?xf64>>, %arg4: memref<?xmemref<?x?x?xf64>>, %arg5: memref<?xmemref<?x?x?xf64>>, %arg6: memref<?xmemref<?x?x?xf64>>, %arg7: memref<?xmemref<?x?x?xf64>>, %arg8: memref<?xmemref<?x?x?xf64>>, %arg9: memref<?xmemref<?x?x?xf64>>, %arg10: memref<?xmemref<?x?x?xf64>>, %arg11: memref<?xmemref<?x?x?xf64>>):
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
            scf.for %arg12 = %69 to %73 step %c1 {
              %74 = arith.muli %arg12, %c9 : index
              %75 = arith.cmpi uge, %74, %67 : index
              %76 = arith.subi %67, %74 : index
              %77 = arith.select %75, %c0, %76 : index
              %78 = arith.minui %77, %c9 : index
              %79 = arith.cmpi uge, %74, %68 : index
              %80 = arith.subi %68, %74 : index
              %81 = arith.select %79, %c0, %80 : index
              %82 = arith.minui %81, %c9 : index
              %83 = arith.minui %82, %c1 : index
              %84 = arith.addi %arg12, %c-1 : index
              %85 = arith.subi %arg12, %30 : index
              %86 = arith.subi %arg12, %59 : index
              %87 = arith.subi %84, %59 : index
              %88 = arts.db_ref %arg1[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %89 = arts.db_ref %arg2[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %90 = arts.db_ref %arg3[%86] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %91 = arts.db_ref %arg4[%86] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %92 = arts.db_ref %arg5[%86] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %93 = arts.db_ref %arg6[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %94 = arts.db_ref %arg7[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %95 = arts.db_ref %arg8[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %96 = arts.db_ref %arg9[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %97 = arts.db_ref %arg10[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %98 = arts.db_ref %arg11[%85] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %99 = arts.db_ref %arg5[%87] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %100 = arts.db_ref %arg3[%87] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %101 = arts.db_ref %arg4[%87] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              scf.for %arg13 = %78 to %83 step %c1 {
                %111 = arith.addi %arg13, %c8 : index
                %112 = arith.addi %arg13, %c1 : index
                scf.for %arg14 = %c2 to %c38 step %c1 {
                  %113 = arith.addi %arg14, %c1 : index
                  %114 = arith.addi %arg14, %c-1 : index
                  scf.for %arg15 = %c2 to %c38 step %c1 {
                    %115 = memref.load %88[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %116 = memref.load %89[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %117 = arith.addi %arg15, %c1 : index
                    %118 = memref.load %90[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %119 = arith.addi %arg15, %c-1 : index
                    %120 = memref.load %90[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %121 = arith.subf %118, %120 : f64
                    %122 = arith.mulf %121, %cst_0 : f64
                    %123 = memref.load %91[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %124 = memref.load %91[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %125 = arith.subf %123, %124 : f64
                    %126 = arith.mulf %125, %cst_0 : f64
                    %127 = memref.load %92[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %128 = memref.load %99[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %129 = arith.subf %127, %128 : f64
                    %130 = arith.mulf %129, %cst_0 : f64
                    %131 = arith.addf %122, %126 : f64
                    %132 = arith.addf %131, %130 : f64
                    %133 = arith.mulf %115, %cst : f64
                    %134 = arith.mulf %133, %122 : f64
                    %135 = arith.mulf %116, %132 : f64
                    %136 = arith.addf %134, %135 : f64
                    %137 = arith.mulf %136, %cst_1 : f64
                    %138 = memref.load %93[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %139 = arith.addf %138, %137 : f64
                    memref.store %139, %93[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %140 = arith.mulf %133, %126 : f64
                    %141 = arith.addf %140, %135 : f64
                    %142 = arith.mulf %141, %cst_1 : f64
                    %143 = memref.load %94[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %144 = arith.addf %143, %142 : f64
                    memref.store %144, %94[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %145 = arith.mulf %133, %130 : f64
                    %146 = arith.addf %145, %135 : f64
                    %147 = arith.mulf %146, %cst_1 : f64
                    %148 = memref.load %95[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %149 = arith.addf %148, %147 : f64
                    memref.store %149, %95[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %150 = arith.mulf %115, %cst_1 : f64
                    %151 = memref.load %90[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %152 = memref.load %90[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %153 = arith.subf %151, %152 : f64
                    %154 = arith.mulf %153, %cst_0 : f64
                    %155 = memref.load %91[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %156 = memref.load %91[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %157 = arith.subf %155, %156 : f64
                    %158 = arith.mulf %157, %cst_0 : f64
                    %159 = arith.addf %154, %158 : f64
                    %160 = arith.mulf %150, %159 : f64
                    %161 = memref.load %96[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %162 = arith.addf %161, %160 : f64
                    memref.store %162, %96[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %163 = memref.load %90[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %164 = memref.load %100[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %165 = arith.subf %163, %164 : f64
                    %166 = arith.mulf %165, %cst_0 : f64
                    %167 = memref.load %92[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %168 = memref.load %92[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %169 = arith.subf %167, %168 : f64
                    %170 = arith.mulf %169, %cst_0 : f64
                    %171 = arith.addf %166, %170 : f64
                    %172 = arith.mulf %150, %171 : f64
                    %173 = memref.load %97[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %174 = arith.addf %173, %172 : f64
                    memref.store %174, %97[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %175 = memref.load %91[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %176 = memref.load %101[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %177 = arith.subf %175, %176 : f64
                    %178 = arith.mulf %177, %cst_0 : f64
                    %179 = memref.load %92[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %180 = memref.load %92[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %181 = arith.subf %179, %180 : f64
                    %182 = arith.mulf %181, %cst_0 : f64
                    %183 = arith.addf %178, %182 : f64
                    %184 = arith.mulf %150, %183 : f64
                    %185 = memref.load %98[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %186 = arith.addf %185, %184 : f64
                    memref.store %186, %98[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                  } {arts.id = 194 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
                } {arts.id = 196 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
              }
              %102 = arith.maxui %78, %c1 : index
              %103 = arith.minui %82, %c8 : index
              scf.for %arg13 = %102 to %103 step %c1 {
                %111 = arith.addi %arg13, %c-1 : index
                %112 = arith.addi %arg13, %c1 : index
                scf.for %arg14 = %c2 to %c38 step %c1 {
                  %113 = arith.addi %arg14, %c1 : index
                  %114 = arith.addi %arg14, %c-1 : index
                  scf.for %arg15 = %c2 to %c38 step %c1 {
                    %115 = memref.load %88[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %116 = memref.load %89[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %117 = arith.addi %arg15, %c1 : index
                    %118 = memref.load %90[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %119 = arith.addi %arg15, %c-1 : index
                    %120 = memref.load %90[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %121 = arith.subf %118, %120 : f64
                    %122 = arith.mulf %121, %cst_0 : f64
                    %123 = memref.load %91[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %124 = memref.load %91[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %125 = arith.subf %123, %124 : f64
                    %126 = arith.mulf %125, %cst_0 : f64
                    %127 = memref.load %92[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %128 = memref.load %92[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %129 = arith.subf %127, %128 : f64
                    %130 = arith.mulf %129, %cst_0 : f64
                    %131 = arith.addf %122, %126 : f64
                    %132 = arith.addf %131, %130 : f64
                    %133 = arith.mulf %115, %cst : f64
                    %134 = arith.mulf %133, %122 : f64
                    %135 = arith.mulf %116, %132 : f64
                    %136 = arith.addf %134, %135 : f64
                    %137 = arith.mulf %136, %cst_1 : f64
                    %138 = memref.load %93[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %139 = arith.addf %138, %137 : f64
                    memref.store %139, %93[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %140 = arith.mulf %133, %126 : f64
                    %141 = arith.addf %140, %135 : f64
                    %142 = arith.mulf %141, %cst_1 : f64
                    %143 = memref.load %94[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %144 = arith.addf %143, %142 : f64
                    memref.store %144, %94[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %145 = arith.mulf %133, %130 : f64
                    %146 = arith.addf %145, %135 : f64
                    %147 = arith.mulf %146, %cst_1 : f64
                    %148 = memref.load %95[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %149 = arith.addf %148, %147 : f64
                    memref.store %149, %95[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %150 = arith.mulf %115, %cst_1 : f64
                    %151 = memref.load %90[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %152 = memref.load %90[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %153 = arith.subf %151, %152 : f64
                    %154 = arith.mulf %153, %cst_0 : f64
                    %155 = memref.load %91[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %156 = memref.load %91[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %157 = arith.subf %155, %156 : f64
                    %158 = arith.mulf %157, %cst_0 : f64
                    %159 = arith.addf %154, %158 : f64
                    %160 = arith.mulf %150, %159 : f64
                    %161 = memref.load %96[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %162 = arith.addf %161, %160 : f64
                    memref.store %162, %96[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %163 = memref.load %90[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %164 = memref.load %90[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %165 = arith.subf %163, %164 : f64
                    %166 = arith.mulf %165, %cst_0 : f64
                    %167 = memref.load %92[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %168 = memref.load %92[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %169 = arith.subf %167, %168 : f64
                    %170 = arith.mulf %169, %cst_0 : f64
                    %171 = arith.addf %166, %170 : f64
                    %172 = arith.mulf %150, %171 : f64
                    %173 = memref.load %97[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %174 = arith.addf %173, %172 : f64
                    memref.store %174, %97[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %175 = memref.load %91[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %176 = memref.load %91[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %177 = arith.subf %175, %176 : f64
                    %178 = arith.mulf %177, %cst_0 : f64
                    %179 = memref.load %92[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %180 = memref.load %92[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %181 = arith.subf %179, %180 : f64
                    %182 = arith.mulf %181, %cst_0 : f64
                    %183 = arith.addf %178, %182 : f64
                    %184 = arith.mulf %150, %183 : f64
                    %185 = memref.load %98[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %186 = arith.addf %185, %184 : f64
                    memref.store %186, %98[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                  } {arts.id = 194 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
                } {arts.id = 196 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
              }
              %104 = arith.maxui %78, %c8 : index
              %105 = arith.minui %82, %c9 : index
              %106 = arith.addi %arg12, %c1 : index
              %107 = arith.subi %106, %59 : index
              %108 = arts.db_ref %arg5[%107] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %109 = arts.db_ref %arg3[%107] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              %110 = arts.db_ref %arg4[%107] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
              scf.for %arg13 = %104 to %105 step %c1 {
                %111 = arith.addi %arg13, %c-1 : index
                %112 = arith.addi %arg13, %c-8 : index
                scf.for %arg14 = %c2 to %c38 step %c1 {
                  %113 = arith.addi %arg14, %c1 : index
                  %114 = arith.addi %arg14, %c-1 : index
                  scf.for %arg15 = %c2 to %c38 step %c1 {
                    %115 = memref.load %88[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %116 = memref.load %89[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %117 = arith.addi %arg15, %c1 : index
                    %118 = memref.load %90[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %119 = arith.addi %arg15, %c-1 : index
                    %120 = memref.load %90[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %121 = arith.subf %118, %120 : f64
                    %122 = arith.mulf %121, %cst_0 : f64
                    %123 = memref.load %91[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %124 = memref.load %91[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %125 = arith.subf %123, %124 : f64
                    %126 = arith.mulf %125, %cst_0 : f64
                    %127 = memref.load %108[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %128 = memref.load %92[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %129 = arith.subf %127, %128 : f64
                    %130 = arith.mulf %129, %cst_0 : f64
                    %131 = arith.addf %122, %126 : f64
                    %132 = arith.addf %131, %130 : f64
                    %133 = arith.mulf %115, %cst : f64
                    %134 = arith.mulf %133, %122 : f64
                    %135 = arith.mulf %116, %132 : f64
                    %136 = arith.addf %134, %135 : f64
                    %137 = arith.mulf %136, %cst_1 : f64
                    %138 = memref.load %93[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %139 = arith.addf %138, %137 : f64
                    memref.store %139, %93[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %140 = arith.mulf %133, %126 : f64
                    %141 = arith.addf %140, %135 : f64
                    %142 = arith.mulf %141, %cst_1 : f64
                    %143 = memref.load %94[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %144 = arith.addf %143, %142 : f64
                    memref.store %144, %94[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %145 = arith.mulf %133, %130 : f64
                    %146 = arith.addf %145, %135 : f64
                    %147 = arith.mulf %146, %cst_1 : f64
                    %148 = memref.load %95[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %149 = arith.addf %148, %147 : f64
                    memref.store %149, %95[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %150 = arith.mulf %115, %cst_1 : f64
                    %151 = memref.load %90[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %152 = memref.load %90[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %153 = arith.subf %151, %152 : f64
                    %154 = arith.mulf %153, %cst_0 : f64
                    %155 = memref.load %91[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %156 = memref.load %91[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %157 = arith.subf %155, %156 : f64
                    %158 = arith.mulf %157, %cst_0 : f64
                    %159 = arith.addf %154, %158 : f64
                    %160 = arith.mulf %150, %159 : f64
                    %161 = memref.load %96[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %162 = arith.addf %161, %160 : f64
                    memref.store %162, %96[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %163 = memref.load %109[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %164 = memref.load %90[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %165 = arith.subf %163, %164 : f64
                    %166 = arith.mulf %165, %cst_0 : f64
                    %167 = memref.load %92[%117, %arg14, %arg13] : memref<?x?x?xf64>
                    %168 = memref.load %92[%119, %arg14, %arg13] : memref<?x?x?xf64>
                    %169 = arith.subf %167, %168 : f64
                    %170 = arith.mulf %169, %cst_0 : f64
                    %171 = arith.addf %166, %170 : f64
                    %172 = arith.mulf %150, %171 : f64
                    %173 = memref.load %97[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %174 = arith.addf %173, %172 : f64
                    memref.store %174, %97[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %175 = memref.load %110[%arg15, %arg14, %112] : memref<?x?x?xf64>
                    %176 = memref.load %91[%arg15, %arg14, %111] : memref<?x?x?xf64>
                    %177 = arith.subf %175, %176 : f64
                    %178 = arith.mulf %177, %cst_0 : f64
                    %179 = memref.load %92[%arg15, %113, %arg13] : memref<?x?x?xf64>
                    %180 = memref.load %92[%arg15, %114, %arg13] : memref<?x?x?xf64>
                    %181 = arith.subf %179, %180 : f64
                    %182 = arith.mulf %181, %cst_0 : f64
                    %183 = arith.addf %178, %182 : f64
                    %184 = arith.mulf %150, %183 : f64
                    %185 = memref.load %98[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                    %186 = arith.addf %185, %184 : f64
                    memref.store %186, %98[%arg15, %arg14, %arg13] : memref<?x?x?xf64>
                  } {arts.id = 194 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
                } {arts.id = 196 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
              }
            }
            arts.db_release(%arg1) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg6) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg7) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg8) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg9) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg10) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg11) : memref<?xmemref<?x?x?xf64>>
          }
        }
      } {arts.id = 198 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 36 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">}
    } : i64
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %9 = scf.for %arg0 = %c0 to %c5 step %c1 iter_args(%arg1 = %cst_8) -> (f64) {
      %11 = arith.muli %arg0, %c9 : index
      %12 = arith.subi %c40, %11 : index
      %13 = arith.cmpi uge, %11, %c40 : index
      %14 = arith.select %13, %c0, %12 : index
      %15 = arith.minui %14, %c9 : index
      %16 = arts.db_ref %ptr_18[%arg0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %17 = arts.db_ref %ptr_20[%arg0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %18 = arts.db_ref %ptr_22[%arg0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %19 = arts.db_ref %ptr_24[%arg0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %20 = arts.db_ref %ptr_26[%arg0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %21 = arts.db_ref %ptr_28[%arg0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %22 = scf.for %arg2 = %c0 to %15 step %c1 iter_args(%arg3 = %arg1) -> (f64) {
        %23 = arith.addi %11, %arg2 : index
        %24 = memref.load %16[%23, %23, %arg2] : memref<?x?x?xf64>
        %25 = memref.load %17[%23, %23, %arg2] : memref<?x?x?xf64>
        %26 = arith.addf %24, %25 : f64
        %27 = memref.load %18[%23, %23, %arg2] : memref<?x?x?xf64>
        %28 = arith.addf %26, %27 : f64
        %29 = memref.load %19[%23, %23, %arg2] : memref<?x?x?xf64>
        %30 = arith.addf %28, %29 : f64
        %31 = memref.load %20[%23, %23, %arg2] : memref<?x?x?xf64>
        %32 = arith.addf %30, %31 : f64
        %33 = memref.load %21[%23, %23, %arg2] : memref<?x?x?xf64>
        %34 = arith.addf %32, %33 : f64
        %35 = arith.addf %arg3, %34 : f64
        scf.yield %35 : f64
      }
      scf.yield %22 : f64
    }
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_27) : memref<?xi64>
    arts.db_free(%ptr_28) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_23) : memref<?xi64>
    arts.db_free(%ptr_24) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_9) : memref<?xi64>
    arts.db_free(%ptr_10) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_25) : memref<?xi64>
    arts.db_free(%ptr_26) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?x?xf64>>
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
