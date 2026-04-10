module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-7 = arith.constant -7 : index
    %c7 = arith.constant 7 : index
    %c5 = arith.constant 5 : index
    %c6 = arith.constant 6 : index
    %c8 = arith.constant 8 : index
    %cst = arith.constant 5.000000e-01 : f32
    %c-1 = arith.constant -1 : index
    %c-2 = arith.constant -2 : index
    %c4 = arith.constant 4 : index
    %c2 = arith.constant 2 : index
    %c3 = arith.constant 3 : index
    %cst_0 = arith.constant -0.0833333358 : f32
    %cst_1 = arith.constant 0.666666686 : f32
    %cst_2 = arith.constant -0.666666686 : f32
    %cst_3 = arith.constant 0.0833333358 : f32
    %c44 = arith.constant 44 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst_4 = arith.constant 5.000000e-02 : f32
    %c17_i32 = arith.constant 17 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_5 = arith.constant 1.000000e-01 : f32
    %cst_6 = arith.constant 0.000000e+00 : f32
    %cst_7 = arith.constant 3.000000e+00 : f32
    %cst_8 = arith.constant 1.000000e-03 : f32
    %c11_i32 = arith.constant 11 : i32
    %cst_9 = arith.constant 2.000000e+00 : f32
    %cst_10 = arith.constant 1.500000e-03 : f32
    %c7_i32 = arith.constant 7 : i32
    %c48 = arith.constant 48 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_11 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c3, %c48, %c48, %c8] {arts.id = 414 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?x?x?xf32>>) block_shape[%c8] {owner_dims = array<i64: 3>, post_db_refined}
    %guid_12, %ptr_13 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c48, %c48, %c48] {arts.id = 415 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %guid_14, %ptr_15 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c8] {arts.id = 416 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_15 : memref<?xmemref<?x?x?xf32>>) block_shape[%c8] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_16, %ptr_17 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c6] elementType(f32) elementSizes[%c48, %c48, %c8] {arts.id = 417 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_17 : memref<?xmemref<?x?x?xf32>>) block_shape[%c8] {owner_dims = array<i64: 2>, post_db_refined}
    %6 = arts.db_ref %ptr_13[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %12 = arith.index_cast %arg0 : index to i32
      %13 = arith.muli %12, %c17_i32 : i32
      %14 = arith.sitofp %12 : i32 to f32
      %15 = arith.mulf %14, %cst_5 : f32
      %16 = scf.for %arg1 = %c0 to %c48 step %c1 iter_args(%arg2 = %c0_i32) -> (i32) {
        %17 = scf.for %arg3 = %c0 to %c48 step %c1 iter_args(%arg4 = %arg2) -> (i32) {
          %18 = arith.index_cast %arg4 : i32 to index
          %19 = arith.addi %18, %c48 : index
          %20 = arith.index_cast %19 : index to i32
          scf.for %arg5 = %c0 to %c6 step %c1 {
            %21 = arith.muli %arg5, %c8 : index
            %22 = arith.subi %c48, %21 : index
            %23 = arith.cmpi uge, %21, %c48 : index
            %24 = arith.select %23, %c0, %22 : index
            %25 = arith.minui %24, %c8 : index
            %26 = arts.db_ref %ptr[%arg5] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            scf.for %arg6 = %c0 to %25 step %c1 {
              %27 = arith.addi %21, %arg6 : index
              %28 = arith.addi %18, %27 : index
              %29 = arith.index_cast %28 : index to i32
              %30 = arith.addi %29, %13 : i32
              %31 = arith.remsi %30, %c23_i32 : i32
              %32 = arith.sitofp %31 : i32 to f32
              %33 = arith.mulf %32, %cst_4 : f32
              %34 = arith.subf %33, %15 : f32
              memref.store %34, %26[%arg0, %arg1, %arg3, %arg6] : memref<?x?x?x?xf32>
              memref.store %cst_6, %6[%arg0, %arg1, %arg3, %27] : memref<?x?x?x?xf32>
            }
          }
          scf.yield %20 : i32
        } {arts.id = 69 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
        scf.yield %17 : i32
      } {arts.id = 71 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
    } {arts.id = 73 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
    %7 = scf.for %arg0 = %c0 to %c48 step %c1 iter_args(%arg1 = %c0_i32) -> (i32) {
      %12 = scf.for %arg2 = %c0 to %c48 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %13 = arith.index_cast %arg3 : i32 to index
        %14 = arith.addi %13, %c48 : index
        %15 = arith.index_cast %14 : index to i32
        scf.for %arg4 = %c0 to %c6 step %c1 {
          %16 = arith.muli %arg4, %c8 : index
          %17 = arith.subi %c48, %16 : index
          %18 = arith.cmpi uge, %16, %c48 : index
          %19 = arith.select %18, %c0, %17 : index
          %20 = arith.minui %19, %c8 : index
          %21 = arts.db_ref %ptr_15[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %22 = arts.db_ref %ptr_17[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          scf.for %arg5 = %c0 to %20 step %c1 {
            %23 = arith.addi %16, %arg5 : index
            %24 = arith.addi %13, %23 : index
            %25 = arith.index_cast %24 : index to i32
            %26 = arith.remsi %25, %c11_i32 : i32
            %27 = arith.sitofp %26 : i32 to f32
            %28 = arith.mulf %27, %cst_8 : f32
            %29 = arith.addf %28, %cst_7 : f32
            memref.store %29, %21[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %30 = arith.remsi %25, %c7_i32 : i32
            %31 = arith.sitofp %30 : i32 to f32
            %32 = arith.mulf %31, %cst_10 : f32
            %33 = arith.addf %32, %cst_9 : f32
            memref.store %33, %22[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
          }
        }
        scf.yield %15 : i32
      } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
      scf.yield %12 : i32
    } {arts.id = 94 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    %8 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c6 step %c1 {
        %12 = arith.cmpi ult, %arg0, %c6 : index
        %13 = arith.select %12, %c1, %c0 : index
        %14 = arith.minui %arg0, %c6 : index
        %15 = arith.muli %14, %c8 : index
        %16 = arith.muli %13, %c8 : index
        %17 = arith.cmpi uge, %15, %c44 : index
        %18 = arith.subi %c44, %15 : index
        %19 = arith.select %17, %c0, %18 : index
        %20 = arith.minui %16, %19 : index
        %21 = arith.cmpi ugt, %20, %c0 : index
        scf.if %21 {
          %22 = arith.divui %15, %c8 : index
          %23 = arith.addi %15, %16 : index
          %24 = arith.subi %23, %c1 : index
          %25 = arith.divui %24, %c8 : index
          %26 = arith.cmpi ugt, %22, %c5 : index
          %27 = arith.minui %25, %c5 : index
          %28 = arith.select %26, %25, %27 : index
          %29 = arith.subi %28, %22 : index
          %30 = arith.addi %29, %c1 : index
          %31 = arith.select %26, %c0, %22 : index
          %32 = arith.select %26, %c0, %30 : index
          %guid_18, %ptr_19 = arts.db_acquire[<in>] (%guid_14 : memref<?xi64>, %ptr_15 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%15], sizes[%16]), offsets[%31], sizes[%32] element_offsets[%c0, %c0, %15] element_sizes[%c48, %c48, %16] {arts.partition_hint = {blockSize = 8 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_19 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_20, %ptr_21 = arts.db_acquire[<in>] (%guid_16 : memref<?xi64>, %ptr_17 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%15], sizes[%16]), offsets[%31], sizes[%32] element_offsets[%c0, %c0, %15] element_sizes[%c48, %c48, %16] {arts.partition_hint = {blockSize = 8 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_21 : memref<?xmemref<?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %33 = arith.addi %15, %20 : index
          %34 = arith.cmpi uge, %15, %c2 : index
          %35 = arith.subi %15, %c2 : index
          %36 = arith.select %34, %35, %c0 : index
          %37 = arith.addi %33, %c2 : index
          %38 = arith.minui %37, %c48 : index
          %39 = arith.cmpi uge, %38, %36 : index
          %40 = arith.subi %38, %36 : index
          %41 = arith.select %39, %40, %c0 : index
          %42 = arith.cmpi uge, %36, %c2 : index
          %43 = arith.subi %36, %c2 : index
          %44 = arith.select %42, %43, %c0 : index
          %45 = arith.addi %36, %41 : index
          %46 = arith.addi %45, %c2 : index
          %47 = arith.minui %46, %c48 : index
          %48 = arith.cmpi uge, %47, %44 : index
          %49 = arith.subi %47, %44 : index
          %50 = arith.select %48, %49, %c0 : index
          %51 = arith.divui %44, %c8 : index
          %52 = arith.addi %44, %50 : index
          %53 = arith.subi %52, %c1 : index
          %54 = arith.divui %53, %c8 : index
          %55 = arith.cmpi ugt, %51, %c5 : index
          %56 = arith.minui %54, %c5 : index
          %57 = arith.select %55, %54, %56 : index
          %58 = arith.subi %57, %51 : index
          %59 = arith.addi %58, %c1 : index
          %60 = arith.select %55, %c0, %51 : index
          %61 = arith.select %55, %c0, %59 : index
          %guid_22, %ptr_23 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%36], sizes[%41]), offsets[%60], sizes[%61] element_offsets[%c0, %c0, %c0, %44] element_sizes[%c3, %c48, %c48, %50] {arts.partition_hint = {blockSize = 8 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_center_offset = 2 : i64, stencil_max_offsets = [2], stencil_min_offsets = [-2], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
          arts.lowering_contract(%ptr_23 : memref<?xmemref<?x?x?x?xf32>>) {critical_path_distance = 0 : i64}
          %guid_24, %ptr_25 = arts.db_acquire[<inout>] (%guid_12 : memref<?xi64>, %ptr_13 : memref<?xmemref<?x?x?x?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.partition_hint = {blockSize = 8 : i64, mode = 1 : i8}, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
          arts.lowering_contract(%ptr_25 : memref<?xmemref<?x?x?x?xf32>>) distribution_kind(<block>) distribution_pattern(<stencil>) {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 3>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_19, %ptr_21, %ptr_23, %ptr_25) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>> attributes {arts.id = 174 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf32>>, %arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?x?xf32>>):
            %alloca = memref.alloca() {arts.id = 105 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 105 : i64, hasUniformAccess = false, dominantAccessPattern = 4 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = false, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>} : memref<5xf32>
            memref.store %cst_0, %alloca[%c0] : memref<5xf32>
            memref.store %cst_1, %alloca[%c1] : memref<5xf32>
            memref.store %cst_6, %alloca[%c2] : memref<5xf32>
            memref.store %cst_2, %alloca[%c3] : memref<5xf32>
            memref.store %cst_3, %alloca[%c4] : memref<5xf32>
            %62 = arith.subi %c4, %15 : index
            %63 = arith.cmpi slt, %62, %c0 : index
            %64 = arith.select %63, %c0, %62 : index
            %65 = arith.cmpi slt, %18, %c0 : index
            %66 = arith.select %65, %c0, %18 : index
            %67 = arith.minui %66, %20 : index
            %68 = arith.addi %64, %15 : index
            %69 = arith.addi %67, %15 : index
            %70 = arith.divui %68, %c8 : index
            %71 = arith.addi %69, %c7 : index
            %72 = arith.divui %71, %c8 : index
            %73 = arith.cmpi ugt, %69, %68 : index
            %74 = arith.select %73, %72, %70 : index
            %75 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            scf.for %arg5 = %70 to %74 step %c1 {
              %76 = arith.muli %arg5, %c8 : index
              %77 = arith.cmpi uge, %76, %68 : index
              %78 = arith.subi %68, %76 : index
              %79 = arith.select %77, %c0, %78 : index
              %80 = arith.minui %79, %c8 : index
              %81 = arith.cmpi uge, %76, %69 : index
              %82 = arith.subi %69, %76 : index
              %83 = arith.select %81, %c0, %82 : index
              %84 = arith.minui %83, %c8 : index
              %85 = arith.minui %84, %c1 : index
              %86 = arith.addi %arg5, %c-1 : index
              %87 = arith.subi %arg5, %31 : index
              %88 = arith.subi %arg5, %60 : index
              %89 = arith.subi %86, %60 : index
              %90 = arts.db_ref %arg1[%87] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %91 = arts.db_ref %arg2[%87] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %92 = arts.db_ref %arg3[%88] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
              %93 = arts.db_ref %arg3[%89] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
              scf.for %arg6 = %80 to %85 step %c1 {
                %103 = arith.addi %76, %arg6 : index
                %104 = arith.addi %arg6, %c6 : index
                %105 = arith.addi %arg6, %c7 : index
                %106 = arith.addi %arg6, %c1 : index
                scf.for %arg7 = %c4 to %c44 step %c1 {
                  %107 = arith.addi %arg7, %c1 : index
                  %108 = arith.addi %arg7, %c-1 : index
                  scf.for %arg8 = %c4 to %c44 step %c1 {
                    %109 = memref.load %90[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %110 = memref.load %91[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %111 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c0, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c0, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %89, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c0, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %112 = arith.addi %arg8, %c1 : index
                    %113 = memref.load %92[%c0, %112, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %114 = arith.addi %arg8, %c-1 : index
                    %115 = memref.load %92[%c0, %114, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %116 = arith.subf %113, %115 : f32
                    %117 = arith.mulf %109, %111 : f32
                    %118 = arith.addf %110, %109 : f32
                    %119 = arith.mulf %118, %116 : f32
                    %120 = arith.mulf %119, %cst : f32
                    %121 = arith.addf %117, %120 : f32
                    memref.store %121, %75[%c0, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %122 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c1, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c1, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %89, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c1, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 136 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %123 = memref.load %92[%c1, %arg8, %107, %arg6] : memref<?x?x?x?xf32>
                    %124 = memref.load %92[%c1, %arg8, %108, %arg6] : memref<?x?x?x?xf32>
                    %125 = arith.subf %123, %124 : f32
                    %126 = arith.mulf %109, %122 : f32
                    %127 = arith.mulf %118, %125 : f32
                    %128 = arith.mulf %127, %cst : f32
                    %129 = arith.addf %126, %128 : f32
                    memref.store %129, %75[%c1, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %130 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c2, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c2, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %89, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c2, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 154 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %131 = memref.load %92[%c2, %arg8, %arg7, %106] : memref<?x?x?x?xf32>
                    %132 = memref.load %93[%c2, %arg8, %arg7, %105] : memref<?x?x?x?xf32>
                    %133 = arith.subf %131, %132 : f32
                    %134 = arith.mulf %109, %130 : f32
                    %135 = arith.mulf %118, %133 : f32
                    %136 = arith.mulf %135, %cst : f32
                    %137 = arith.addf %134, %136 : f32
                    memref.store %137, %75[%c2, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                  } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                } {arts.id = 166 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
              }
              %94 = arith.maxui %80, %c1 : index
              %95 = arith.minui %84, %c2 : index
              scf.for %arg6 = %94 to %95 step %c1 {
                %103 = arith.addi %76, %arg6 : index
                %104 = arith.addi %arg6, %c6 : index
                %105 = arith.addi %arg6, %c-1 : index
                %106 = arith.addi %arg6, %c1 : index
                scf.for %arg7 = %c4 to %c44 step %c1 {
                  %107 = arith.addi %arg7, %c1 : index
                  %108 = arith.addi %arg7, %c-1 : index
                  scf.for %arg8 = %c4 to %c44 step %c1 {
                    %109 = memref.load %90[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %110 = memref.load %91[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %111 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c0, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c0, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %89, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c0, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %112 = arith.addi %arg8, %c1 : index
                    %113 = memref.load %92[%c0, %112, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %114 = arith.addi %arg8, %c-1 : index
                    %115 = memref.load %92[%c0, %114, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %116 = arith.subf %113, %115 : f32
                    %117 = arith.mulf %109, %111 : f32
                    %118 = arith.addf %110, %109 : f32
                    %119 = arith.mulf %118, %116 : f32
                    %120 = arith.mulf %119, %cst : f32
                    %121 = arith.addf %117, %120 : f32
                    memref.store %121, %75[%c0, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %122 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c1, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c1, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %89, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c1, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 136 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %123 = memref.load %92[%c1, %arg8, %107, %arg6] : memref<?x?x?x?xf32>
                    %124 = memref.load %92[%c1, %arg8, %108, %arg6] : memref<?x?x?x?xf32>
                    %125 = arith.subf %123, %124 : f32
                    %126 = arith.mulf %109, %122 : f32
                    %127 = arith.mulf %118, %125 : f32
                    %128 = arith.mulf %127, %cst : f32
                    %129 = arith.addf %126, %128 : f32
                    memref.store %129, %75[%c1, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %130 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c2, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c2, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %89, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c2, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 154 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %131 = memref.load %92[%c2, %arg8, %arg7, %106] : memref<?x?x?x?xf32>
                    %132 = memref.load %92[%c2, %arg8, %arg7, %105] : memref<?x?x?x?xf32>
                    %133 = arith.subf %131, %132 : f32
                    %134 = arith.mulf %109, %130 : f32
                    %135 = arith.mulf %118, %133 : f32
                    %136 = arith.mulf %135, %cst : f32
                    %137 = arith.addf %134, %136 : f32
                    memref.store %137, %75[%c2, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                  } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                } {arts.id = 166 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
              }
              %96 = arith.maxui %80, %c2 : index
              %97 = arith.minui %84, %c7 : index
              scf.for %arg6 = %96 to %97 step %c1 {
                %103 = arith.addi %76, %arg6 : index
                %104 = arith.addi %arg6, %c-2 : index
                %105 = arith.addi %arg6, %c-1 : index
                %106 = arith.addi %arg6, %c1 : index
                scf.for %arg7 = %c4 to %c44 step %c1 {
                  %107 = arith.addi %arg7, %c1 : index
                  %108 = arith.addi %arg7, %c-1 : index
                  scf.for %arg8 = %c4 to %c44 step %c1 {
                    %109 = memref.load %90[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %110 = memref.load %91[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %111 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c0, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c0, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %88, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c0, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %112 = arith.addi %arg8, %c1 : index
                    %113 = memref.load %92[%c0, %112, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %114 = arith.addi %arg8, %c-1 : index
                    %115 = memref.load %92[%c0, %114, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %116 = arith.subf %113, %115 : f32
                    %117 = arith.mulf %109, %111 : f32
                    %118 = arith.addf %110, %109 : f32
                    %119 = arith.mulf %118, %116 : f32
                    %120 = arith.mulf %119, %cst : f32
                    %121 = arith.addf %117, %120 : f32
                    memref.store %121, %75[%c0, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %122 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c1, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c1, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %88, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c1, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 136 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %123 = memref.load %92[%c1, %arg8, %107, %arg6] : memref<?x?x?x?xf32>
                    %124 = memref.load %92[%c1, %arg8, %108, %arg6] : memref<?x?x?x?xf32>
                    %125 = arith.subf %123, %124 : f32
                    %126 = arith.mulf %109, %122 : f32
                    %127 = arith.mulf %118, %125 : f32
                    %128 = arith.mulf %127, %cst : f32
                    %129 = arith.addf %126, %128 : f32
                    memref.store %129, %75[%c1, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %130 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c2, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c2, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %88, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c2, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 154 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %131 = memref.load %92[%c2, %arg8, %arg7, %106] : memref<?x?x?x?xf32>
                    %132 = memref.load %92[%c2, %arg8, %arg7, %105] : memref<?x?x?x?xf32>
                    %133 = arith.subf %131, %132 : f32
                    %134 = arith.mulf %109, %130 : f32
                    %135 = arith.mulf %118, %133 : f32
                    %136 = arith.mulf %135, %cst : f32
                    %137 = arith.addf %134, %136 : f32
                    memref.store %137, %75[%c2, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                  } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                } {arts.id = 166 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
              }
              %98 = arith.maxui %80, %c7 : index
              %99 = arith.minui %84, %c8 : index
              %100 = arith.addi %arg5, %c1 : index
              %101 = arith.subi %100, %60 : index
              %102 = arts.db_ref %arg3[%101] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
              scf.for %arg6 = %98 to %99 step %c1 {
                %103 = arith.addi %76, %arg6 : index
                %104 = arith.addi %arg6, %c-2 : index
                %105 = arith.addi %arg6, %c-1 : index
                %106 = arith.addi %arg6, %c-7 : index
                scf.for %arg7 = %c4 to %c44 step %c1 {
                  %107 = arith.addi %arg7, %c1 : index
                  %108 = arith.addi %arg7, %c-1 : index
                  scf.for %arg8 = %c4 to %c44 step %c1 {
                    %109 = memref.load %90[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %110 = memref.load %91[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                    %111 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c0, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c0, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %88, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c0, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %112 = arith.addi %arg8, %c1 : index
                    %113 = memref.load %92[%c0, %112, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %114 = arith.addi %arg8, %c-1 : index
                    %115 = memref.load %92[%c0, %114, %arg7, %arg6] : memref<?x?x?x?xf32>
                    %116 = arith.subf %113, %115 : f32
                    %117 = arith.mulf %109, %111 : f32
                    %118 = arith.addf %110, %109 : f32
                    %119 = arith.mulf %118, %116 : f32
                    %120 = arith.mulf %119, %cst : f32
                    %121 = arith.addf %117, %120 : f32
                    memref.store %121, %75[%c0, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %122 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c1, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c1, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %88, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c1, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 136 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %123 = memref.load %92[%c1, %arg8, %107, %arg6] : memref<?x?x?x?xf32>
                    %124 = memref.load %92[%c1, %arg8, %108, %arg6] : memref<?x?x?x?xf32>
                    %125 = arith.subf %123, %124 : f32
                    %126 = arith.mulf %109, %122 : f32
                    %127 = arith.mulf %118, %125 : f32
                    %128 = arith.mulf %127, %cst : f32
                    %129 = arith.addf %126, %128 : f32
                    memref.store %129, %75[%c1, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                    %130 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                      %138 = arith.addi %arg9, %c2 : index
                      %139 = memref.load %alloca[%138] : memref<5xf32>
                      %140 = arith.addi %arg8, %arg9 : index
                      %141 = memref.load %92[%c2, %140, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %142 = arith.addi %arg7, %arg9 : index
                      %143 = memref.load %92[%c2, %arg8, %142, %arg6] : memref<?x?x?x?xf32>
                      %144 = arith.addf %141, %143 : f32
                      %145 = arith.subi %arg9, %c-2 : index
                      %146 = arith.addi %104, %145 : index
                      %147 = arith.cmpi slt, %146, %c0 : index
                      %148 = arith.cmpi sge, %146, %c8 : index
                      %149 = arith.select %147, %c-1, %c0 : index
                      %150 = arith.select %148, %c1, %149 : index
                      %151 = arith.addi %88, %150 : index
                      %152 = arith.subi %146, %c8 : index
                      %153 = arith.addi %146, %c8 : index
                      %154 = arith.select %147, %153, %146 : index
                      %155 = arith.select %148, %152, %154 : index
                      %156 = arts.db_ref %arg3[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                      %157 = memref.load %156[%c2, %arg8, %arg7, %155] : memref<?x?x?x?xf32>
                      %158 = arith.addf %144, %157 : f32
                      %159 = arith.mulf %139, %158 : f32
                      %160 = arith.addf %arg10, %159 : f32
                      scf.yield %160 : f32
                    } {arts.id = 154 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    %131 = memref.load %102[%c2, %arg8, %arg7, %106] : memref<?x?x?x?xf32>
                    %132 = memref.load %92[%c2, %arg8, %arg7, %105] : memref<?x?x?x?xf32>
                    %133 = arith.subf %131, %132 : f32
                    %134 = arith.mulf %109, %130 : f32
                    %135 = arith.mulf %118, %133 : f32
                    %136 = arith.mulf %135, %cst : f32
                    %137 = arith.addf %134, %136 : f32
                    memref.store %137, %75[%c2, %arg8, %arg7, %103] : memref<?x?x?x?xf32>
                  } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                } {arts.id = 166 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
              }
            }
            arts.db_release(%arg1) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg2) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg3) : memref<?xmemref<?x?x?x?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?x?x?x?xf32>>
          }
        }
      } {arts.id = 168 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
    } : i64
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %10 = scf.for %arg0 = %c0 to %c48 step %c1 iter_args(%arg1 = %cst_11) -> (f64) {
      %12 = scf.for %arg2 = %c0 to %c3 step %c1 iter_args(%arg3 = %arg1) -> (f64) {
        %13 = memref.load %6[%arg2, %arg0, %arg0, %arg0] : memref<?x?x?x?xf32>
        %14 = arith.extf %13 : f32 to f64
        %15 = math.absf %14 : f64
        %16 = arith.addf %arg3, %15 : f64
        scf.yield %16 : f64
      } {arts.id = 172 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:163:23">}
      scf.yield %12 : f64
    } {arts.id = 170 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:162:21">}
    call @carts_bench_checksum_d(%10) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%11, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_14) : memref<?xi64>
    arts.db_free(%ptr_15) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_16) : memref<?xi64>
    arts.db_free(%ptr_17) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?x?xf32>>
    arts.db_free(%guid_12) : memref<?xi64>
    arts.db_free(%ptr_13) : memref<?xmemref<?x?x?x?xf32>>
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
