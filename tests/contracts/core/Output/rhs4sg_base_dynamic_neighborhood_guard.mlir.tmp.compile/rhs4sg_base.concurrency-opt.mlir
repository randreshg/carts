module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c63 = arith.constant 63 : index
    %c575_i64 = arith.constant 575 : i64
    %c575 = arith.constant 575 : index
    %c64 = arith.constant 64 : index
    %c9 = arith.constant 9 : index
    %c316 = arith.constant 316 : index
    %cst = arith.constant 5.000000e-01 : f32
    %c-1 = arith.constant -1 : index
    %c1_i32 = arith.constant 1 : i32
    %c4 = arith.constant 4 : index
    %c-2 = arith.constant -2 : index
    %c2 = arith.constant 2 : index
    %c10 = arith.constant 10 : index
    %c320 = arith.constant 320 : index
    %c3 = arith.constant 3 : index
    %cst_0 = arith.constant -0.0833333358 : f32
    %cst_1 = arith.constant 0.666666686 : f32
    %cst_2 = arith.constant -0.666666686 : f32
    %cst_3 = arith.constant 0.0833333358 : f32
    %c572 = arith.constant 572 : index
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
    %c576 = arith.constant 576 : index
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
    %6 = arts.runtime_query <total_workers> -> i32
    %7 = arith.index_castui %6 : i32 to index
    %8 = arith.maxui %7, %c1 : index
    %9 = arith.addi %8, %c575 : index
    %10 = arith.divui %9, %8 : index
    %11 = arith.maxui %10, %c1 : index
    %12 = arith.maxui %11, %c1 : index
    %13 = arith.index_cast %12 : index to i64
    %14 = arith.addi %13, %c575_i64 : i64
    %15 = arith.divui %14, %13 : i64
    %16 = arith.index_cast %15 : i64 to index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%16] elementType(f32) elementSizes[%c3, %c320, %c320, %12] {arts.id = 417 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?x?x?xf32>>) block_shape[%12] {owner_dims = array<i64: 3>, post_db_refined}
    %guid_12, %ptr_13 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c64] elementType(f32) elementSizes[%c3, %c320, %c320, %c9] {arts.id = 418 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    arts.lowering_contract(%ptr_13 : memref<?xmemref<?x?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 3>, post_db_refined}
    %guid_14, %ptr_15 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c64] elementType(f32) elementSizes[%c320, %c320, %c9] {arts.id = 419 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_15 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %guid_16, %ptr_17 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c64] elementType(f32) elementSizes[%c320, %c320, %c9] {arts.id = 420 : i64, arts.local_only} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    arts.lowering_contract(%ptr_17 : memref<?xmemref<?x?x?xf32>>) block_shape[%c9] {owner_dims = array<i64: 2>, post_db_refined}
    %17 = arith.maxui %12, %c1 : index
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %22 = arith.index_cast %arg0 : index to i32
      %23 = arith.muli %22, %c17_i32 : i32
      %24 = arith.sitofp %22 : i32 to f32
      %25 = arith.mulf %24, %cst_5 : f32
      %26 = scf.for %arg1 = %c0 to %c320 step %c1 iter_args(%arg2 = %c0_i32) -> (i32) {
        %27 = scf.for %arg3 = %c0 to %c320 step %c1 iter_args(%arg4 = %arg2) -> (i32) {
          %28 = arith.index_cast %arg4 : i32 to index
          %29 = arith.addi %28, %c576 : index
          %30 = arith.index_cast %29 : index to i32
          scf.for %arg5 = %c0 to %c576 step %c1 {
            %31 = arith.addi %28, %arg5 : index
            %32 = arith.index_cast %31 : index to i32
            %33 = arith.addi %32, %23 : i32
            %34 = arith.remsi %33, %c23_i32 : i32
            %35 = arith.sitofp %34 : i32 to f32
            %36 = arith.mulf %35, %cst_4 : f32
            %37 = arith.subf %36, %25 : f32
            %38 = arith.divui %arg5, %17 : index
            %39 = arith.remui %arg5, %17 : index
            %40 = arts.db_ref %ptr[%38] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            memref.store %37, %40[%arg0, %arg1, %arg3, %39] : memref<?x?x?x?xf32>
            %41 = arith.divui %arg5, %c9 : index
            %42 = arith.remui %arg5, %c9 : index
            %43 = arts.db_ref %ptr_13[%41] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            memref.store %cst_6, %43[%arg0, %arg1, %arg3, %42] : memref<?x?x?x?xf32>
          } {arts.id = 69 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 576 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
          scf.yield %30 : i32
        } {arts.id = 71 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
        scf.yield %27 : i32
      } {arts.id = 73 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
    } {arts.id = 75 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
    %18 = scf.for %arg0 = %c0 to %c320 step %c1 iter_args(%arg1 = %c0_i32) -> (i32) {
      %22 = scf.for %arg2 = %c0 to %c320 step %c1 iter_args(%arg3 = %arg1) -> (i32) {
        %23 = arith.index_cast %arg3 : i32 to index
        %24 = arith.addi %23, %c576 : index
        %25 = arith.index_cast %24 : index to i32
        scf.for %arg4 = %c0 to %c64 step %c1 {
          %26 = arith.muli %arg4, %c9 : index
          %27 = arith.subi %c576, %26 : index
          %28 = arith.cmpi uge, %26, %c576 : index
          %29 = arith.select %28, %c0, %27 : index
          %30 = arith.minui %29, %c9 : index
          %31 = arts.db_ref %ptr_15[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          %32 = arts.db_ref %ptr_17[%arg4] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          scf.for %arg5 = %c0 to %30 step %c1 {
            %33 = arith.addi %26, %arg5 : index
            %34 = arith.addi %23, %33 : index
            %35 = arith.index_cast %34 : index to i32
            %36 = arith.remsi %35, %c11_i32 : i32
            %37 = arith.sitofp %36 : i32 to f32
            %38 = arith.mulf %37, %cst_8 : f32
            %39 = arith.addf %38, %cst_7 : f32
            memref.store %39, %31[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
            %40 = arith.remsi %35, %c7_i32 : i32
            %41 = arith.sitofp %40 : i32 to f32
            %42 = arith.mulf %41, %cst_10 : f32
            %43 = arith.addf %42, %cst_9 : f32
            memref.store %43, %32[%arg0, %arg2, %arg5] : memref<?x?x?xf32>
          }
        }
        scf.yield %25 : i32
      } {arts.id = 94 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
      scf.yield %22 : i32
    } {arts.id = 96 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:144:3">}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %22 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
        scf.for %arg1 = %c0 to %c64 step %c1 {
          %23 = arith.muli %arg1, %c9 : index
          %24 = arith.cmpi uge, %23, %c572 : index
          %25 = arith.subi %c572, %23 : index
          %26 = arith.select %24, %c0, %25 : index
          %27 = arith.minui %26, %c9 : index
          %28 = arith.cmpi ugt, %27, %c0 : index
          scf.if %28 {
            %29 = arith.divui %23, %c9 : index
            %30 = arith.cmpi ugt, %29, %c63 : index
            %31 = arith.select %30, %c0, %29 : index
            %guid_18, %ptr_19 = arts.db_acquire[<in>] (%guid_14 : memref<?xi64>, %ptr_15 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%23], sizes[%c9]), offsets[%31], sizes[%c1] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [320, 320, 9], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
            arts.lowering_contract(%ptr_19 : memref<?xmemref<?x?x?xf32>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c320] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 2>, post_db_refined}
            %guid_20, %ptr_21 = arts.db_acquire[<in>] (%guid_16 : memref<?xi64>, %ptr_17 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%23], sizes[%c9]), offsets[%31], sizes[%c1] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [320, 320, 9], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
            arts.lowering_contract(%ptr_21 : memref<?xmemref<?x?x?xf32>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c320] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 2>, post_db_refined}
            %32 = arith.addi %23, %27 : index
            %33 = arith.cmpi uge, %23, %c2 : index
            %34 = arith.subi %23, %c2 : index
            %35 = arith.select %33, %34, %c0 : index
            %36 = arith.addi %32, %c2 : index
            %37 = arith.minui %36, %c576 : index
            %38 = arith.cmpi uge, %37, %35 : index
            %39 = arith.subi %37, %35 : index
            %40 = arith.select %38, %39, %c0 : index
            %41 = arith.muli %16, %17 : index
            %42 = arith.cmpi uge, %35, %c2 : index
            %43 = arith.subi %35, %c2 : index
            %44 = arith.select %42, %43, %c0 : index
            %45 = arith.addi %35, %40 : index
            %46 = arith.addi %45, %c2 : index
            %47 = arith.minui %46, %41 : index
            %48 = arith.cmpi uge, %47, %44 : index
            %49 = arith.subi %47, %44 : index
            %50 = arith.select %48, %49, %c0 : index
            %51 = arith.divui %44, %17 : index
            %52 = arith.addi %44, %50 : index
            %53 = arith.subi %52, %c1 : index
            %54 = arith.divui %53, %17 : index
            %55 = arith.subi %16, %c1 : index
            %56 = arith.cmpi ugt, %51, %55 : index
            %57 = arith.minui %54, %55 : index
            %58 = arith.select %56, %54, %57 : index
            %59 = arith.subi %58, %51 : index
            %60 = arith.addi %59, %c1 : index
            %61 = arith.select %56, %c0, %51 : index
            %62 = arith.select %56, %c0, %60 : index
            %guid_22, %ptr_23 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%61], sizes[%62] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9], stencil_center_offset = 2 : i64, stencil_max_offsets = [2], stencil_min_offsets = [-2], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
            arts.lowering_contract(%ptr_23 : memref<?xmemref<?x?x?x?xf32>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c3] min_offsets[%c-2] max_offsets[%c2] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 3>, post_db_refined}
            %guid_24, %ptr_25 = arts.db_acquire[<inout>] (%guid_12 : memref<?xi64>, %ptr_13 : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%23], sizes[%c9]), offsets[%31], sizes[%c1] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
            arts.lowering_contract(%ptr_25 : memref<?xmemref<?x?x?x?xf32>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c3] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 3>, post_db_refined}
            arts.edt <task> <intranode> route(%c-1_i32) (%ptr_19, %ptr_21, %ptr_23, %ptr_25) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>> attributes {arts.id = 172 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9]} {
            ^bb0(%arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?x?xf32>>, %arg5: memref<?xmemref<?x?x?x?xf32>>):
              %alloca = memref.alloca() {arts.id = 98 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 98 : i64, hasUniformAccess = false, dominantAccessPattern = 4 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = false, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>} : memref<5xf32>
              memref.store %cst_3, %alloca[%c4] : memref<5xf32>
              memref.store %cst_2, %alloca[%c3] : memref<5xf32>
              memref.store %cst_6, %alloca[%c2] : memref<5xf32>
              memref.store %cst_1, %alloca[%c1] : memref<5xf32>
              memref.store %cst_0, %alloca[%c0] : memref<5xf32>
              %63 = arith.subi %c4, %23 : index
              %64 = arith.cmpi slt, %63, %c0 : index
              %65 = arith.select %64, %c0, %63 : index
              %66 = arith.cmpi slt, %25, %c0 : index
              %67 = arith.select %66, %c0, %25 : index
              %68 = arith.minui %67, %27 : index
              %69 = arith.maxui %17, %c1 : index
              %70 = arith.cmpi ugt, %69, %c2 : index
              scf.if %70 {
                %71 = arith.addi %65, %23 : index
                %72 = arith.addi %68, %23 : index
                %73 = arith.subi %69, %c1 : index
                %74 = arith.divui %71, %69 : index
                %75 = arith.addi %72, %73 : index
                %76 = arith.divui %75, %69 : index
                %77 = arith.cmpi ugt, %72, %71 : index
                %78 = arith.select %77, %76, %74 : index
                %79 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                %80 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                %81 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                scf.for %arg6 = %74 to %78 step %c1 {
                  %82 = arith.muli %arg6, %69 : index
                  %83 = arith.cmpi uge, %82, %71 : index
                  %84 = arith.subi %71, %82 : index
                  %85 = arith.select %83, %c0, %84 : index
                  %86 = arith.minui %85, %69 : index
                  %87 = arith.cmpi uge, %82, %72 : index
                  %88 = arith.subi %72, %82 : index
                  %89 = arith.select %87, %c0, %88 : index
                  %90 = arith.minui %89, %69 : index
                  %91 = arith.minui %90, %c1 : index
                  %92 = arith.addi %arg6, %c-1 : index
                  %93 = arith.subi %arg6, %61 : index
                  %94 = arith.subi %92, %61 : index
                  %95 = arts.db_ref %arg4[%93] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  %96 = arts.db_ref %arg4[%94] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  scf.for %arg7 = %86 to %91 step %c1 {
                    %104 = arith.addi %82, %arg7 : index
                    %105 = arith.subi %104, %23 : index
                    %106 = arith.addi %arg7, %c-1 : index
                    %107 = arith.addi %106, %69 : index
                    %108 = arith.addi %arg7, %c1 : index
                    %109 = arith.index_cast %104 : index to i32
                    scf.for %arg8 = %c4 to %c316 step %c1 {
                      %110 = arith.addi %arg8, %c1 : index
                      %111 = arith.addi %arg8, %c-1 : index
                      scf.for %arg9 = %c4 to %c316 step %c1 {
                        %112 = memref.load %79[%arg9, %arg8, %105] : memref<?x?x?xf32>
                        %113 = memref.load %80[%arg9, %arg8, %105] : memref<?x?x?xf32>
                        %114 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %141 = arith.index_cast %arg10 : index to i32
                          %142 = arith.addi %arg10, %c2 : index
                          %143 = memref.load %alloca[%142] : memref<5xf32>
                          %144 = arith.addi %arg9, %arg10 : index
                          %145 = memref.load %95[%c0, %144, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %146 = arith.addi %arg8, %arg10 : index
                          %147 = memref.load %95[%c0, %arg9, %146, %arg7] : memref<?x?x?x?xf32>
                          %148 = arith.addf %145, %147 : f32
                          %149 = arith.addi %109, %141 : i32
                          %150 = arith.index_cast %149 : i32 to index
                          %151 = arith.divui %150, %69 : index
                          %152 = arith.subi %151, %61 : index
                          %153 = arith.remui %150, %69 : index
                          %154 = arts.db_ref %arg4[%152] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %155 = memref.load %154[%c0, %arg9, %arg8, %153] : memref<?x?x?x?xf32>
                          %156 = arith.addf %148, %155 : f32
                          %157 = arith.mulf %143, %156 : f32
                          %158 = arith.addf %arg11, %157 : f32
                          scf.yield %158 : f32
                        } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %115 = arith.addi %arg9, %c1 : index
                        %116 = memref.load %95[%c0, %115, %arg8, %arg7] : memref<?x?x?x?xf32>
                        %117 = arith.addi %arg9, %c-1 : index
                        %118 = memref.load %95[%c0, %117, %arg8, %arg7] : memref<?x?x?x?xf32>
                        %119 = arith.subf %116, %118 : f32
                        %120 = arith.mulf %112, %114 : f32
                        %121 = arith.addf %113, %112 : f32
                        %122 = arith.mulf %121, %119 : f32
                        %123 = arith.mulf %122, %cst : f32
                        %124 = arith.addf %120, %123 : f32
                        memref.store %124, %81[%c0, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                        %125 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %141 = arith.index_cast %arg10 : index to i32
                          %142 = arith.addi %arg10, %c2 : index
                          %143 = memref.load %alloca[%142] : memref<5xf32>
                          %144 = arith.addi %arg9, %arg10 : index
                          %145 = memref.load %95[%c1, %144, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %146 = arith.addi %arg8, %arg10 : index
                          %147 = memref.load %95[%c1, %arg9, %146, %arg7] : memref<?x?x?x?xf32>
                          %148 = arith.addf %145, %147 : f32
                          %149 = arith.addi %109, %141 : i32
                          %150 = arith.index_cast %149 : i32 to index
                          %151 = arith.divui %150, %69 : index
                          %152 = arith.subi %151, %61 : index
                          %153 = arith.remui %150, %69 : index
                          %154 = arts.db_ref %arg4[%152] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %155 = memref.load %154[%c1, %arg9, %arg8, %153] : memref<?x?x?x?xf32>
                          %156 = arith.addf %148, %155 : f32
                          %157 = arith.mulf %143, %156 : f32
                          %158 = arith.addf %arg11, %157 : f32
                          scf.yield %158 : f32
                        } {arts.id = 134 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %126 = memref.load %95[%c1, %arg9, %110, %arg7] : memref<?x?x?x?xf32>
                        %127 = memref.load %95[%c1, %arg9, %111, %arg7] : memref<?x?x?x?xf32>
                        %128 = arith.subf %126, %127 : f32
                        %129 = arith.mulf %112, %125 : f32
                        %130 = arith.mulf %121, %128 : f32
                        %131 = arith.mulf %130, %cst : f32
                        %132 = arith.addf %129, %131 : f32
                        memref.store %132, %81[%c1, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                        %133 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %141 = arith.index_cast %arg10 : index to i32
                          %142 = arith.addi %arg10, %c2 : index
                          %143 = memref.load %alloca[%142] : memref<5xf32>
                          %144 = arith.addi %arg9, %arg10 : index
                          %145 = memref.load %95[%c2, %144, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %146 = arith.addi %arg8, %arg10 : index
                          %147 = memref.load %95[%c2, %arg9, %146, %arg7] : memref<?x?x?x?xf32>
                          %148 = arith.addf %145, %147 : f32
                          %149 = arith.addi %109, %141 : i32
                          %150 = arith.index_cast %149 : i32 to index
                          %151 = arith.divui %150, %69 : index
                          %152 = arith.subi %151, %61 : index
                          %153 = arith.remui %150, %69 : index
                          %154 = arts.db_ref %arg4[%152] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %155 = memref.load %154[%c2, %arg9, %arg8, %153] : memref<?x?x?x?xf32>
                          %156 = arith.addf %148, %155 : f32
                          %157 = arith.mulf %143, %156 : f32
                          %158 = arith.addf %arg11, %157 : f32
                          scf.yield %158 : f32
                        } {arts.id = 152 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %134 = memref.load %95[%c2, %arg9, %arg8, %108] : memref<?x?x?x?xf32>
                        %135 = memref.load %96[%c2, %arg9, %arg8, %107] : memref<?x?x?x?xf32>
                        %136 = arith.subf %134, %135 : f32
                        %137 = arith.mulf %112, %133 : f32
                        %138 = arith.mulf %121, %136 : f32
                        %139 = arith.mulf %138, %cst : f32
                        %140 = arith.addf %137, %139 : f32
                        memref.store %140, %81[%c2, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                      } {arts.id = 162 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                  }
                  %97 = arith.maxui %86, %c1 : index
                  %98 = arith.minui %90, %73 : index
                  scf.for %arg7 = %97 to %98 step %c1 {
                    %104 = arith.addi %82, %arg7 : index
                    %105 = arith.subi %104, %23 : index
                    %106 = arith.addi %arg7, %c-1 : index
                    %107 = arith.addi %arg7, %c1 : index
                    %108 = arith.index_cast %104 : index to i32
                    scf.for %arg8 = %c4 to %c316 step %c1 {
                      %109 = arith.addi %arg8, %c1 : index
                      %110 = arith.addi %arg8, %c-1 : index
                      scf.for %arg9 = %c4 to %c316 step %c1 {
                        %111 = memref.load %79[%arg9, %arg8, %105] : memref<?x?x?xf32>
                        %112 = memref.load %80[%arg9, %arg8, %105] : memref<?x?x?xf32>
                        %113 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %140 = arith.index_cast %arg10 : index to i32
                          %141 = arith.addi %arg10, %c2 : index
                          %142 = memref.load %alloca[%141] : memref<5xf32>
                          %143 = arith.addi %arg9, %arg10 : index
                          %144 = memref.load %95[%c0, %143, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %145 = arith.addi %arg8, %arg10 : index
                          %146 = memref.load %95[%c0, %arg9, %145, %arg7] : memref<?x?x?x?xf32>
                          %147 = arith.addf %144, %146 : f32
                          %148 = arith.addi %108, %140 : i32
                          %149 = arith.index_cast %148 : i32 to index
                          %150 = arith.divui %149, %69 : index
                          %151 = arith.subi %150, %61 : index
                          %152 = arith.remui %149, %69 : index
                          %153 = arts.db_ref %arg4[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %154 = memref.load %153[%c0, %arg9, %arg8, %152] : memref<?x?x?x?xf32>
                          %155 = arith.addf %147, %154 : f32
                          %156 = arith.mulf %142, %155 : f32
                          %157 = arith.addf %arg11, %156 : f32
                          scf.yield %157 : f32
                        } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %114 = arith.addi %arg9, %c1 : index
                        %115 = memref.load %95[%c0, %114, %arg8, %arg7] : memref<?x?x?x?xf32>
                        %116 = arith.addi %arg9, %c-1 : index
                        %117 = memref.load %95[%c0, %116, %arg8, %arg7] : memref<?x?x?x?xf32>
                        %118 = arith.subf %115, %117 : f32
                        %119 = arith.mulf %111, %113 : f32
                        %120 = arith.addf %112, %111 : f32
                        %121 = arith.mulf %120, %118 : f32
                        %122 = arith.mulf %121, %cst : f32
                        %123 = arith.addf %119, %122 : f32
                        memref.store %123, %81[%c0, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                        %124 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %140 = arith.index_cast %arg10 : index to i32
                          %141 = arith.addi %arg10, %c2 : index
                          %142 = memref.load %alloca[%141] : memref<5xf32>
                          %143 = arith.addi %arg9, %arg10 : index
                          %144 = memref.load %95[%c1, %143, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %145 = arith.addi %arg8, %arg10 : index
                          %146 = memref.load %95[%c1, %arg9, %145, %arg7] : memref<?x?x?x?xf32>
                          %147 = arith.addf %144, %146 : f32
                          %148 = arith.addi %108, %140 : i32
                          %149 = arith.index_cast %148 : i32 to index
                          %150 = arith.divui %149, %69 : index
                          %151 = arith.subi %150, %61 : index
                          %152 = arith.remui %149, %69 : index
                          %153 = arts.db_ref %arg4[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %154 = memref.load %153[%c1, %arg9, %arg8, %152] : memref<?x?x?x?xf32>
                          %155 = arith.addf %147, %154 : f32
                          %156 = arith.mulf %142, %155 : f32
                          %157 = arith.addf %arg11, %156 : f32
                          scf.yield %157 : f32
                        } {arts.id = 134 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %125 = memref.load %95[%c1, %arg9, %109, %arg7] : memref<?x?x?x?xf32>
                        %126 = memref.load %95[%c1, %arg9, %110, %arg7] : memref<?x?x?x?xf32>
                        %127 = arith.subf %125, %126 : f32
                        %128 = arith.mulf %111, %124 : f32
                        %129 = arith.mulf %120, %127 : f32
                        %130 = arith.mulf %129, %cst : f32
                        %131 = arith.addf %128, %130 : f32
                        memref.store %131, %81[%c1, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                        %132 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %140 = arith.index_cast %arg10 : index to i32
                          %141 = arith.addi %arg10, %c2 : index
                          %142 = memref.load %alloca[%141] : memref<5xf32>
                          %143 = arith.addi %arg9, %arg10 : index
                          %144 = memref.load %95[%c2, %143, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %145 = arith.addi %arg8, %arg10 : index
                          %146 = memref.load %95[%c2, %arg9, %145, %arg7] : memref<?x?x?x?xf32>
                          %147 = arith.addf %144, %146 : f32
                          %148 = arith.addi %108, %140 : i32
                          %149 = arith.index_cast %148 : i32 to index
                          %150 = arith.divui %149, %69 : index
                          %151 = arith.subi %150, %61 : index
                          %152 = arith.remui %149, %69 : index
                          %153 = arts.db_ref %arg4[%151] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %154 = memref.load %153[%c2, %arg9, %arg8, %152] : memref<?x?x?x?xf32>
                          %155 = arith.addf %147, %154 : f32
                          %156 = arith.mulf %142, %155 : f32
                          %157 = arith.addf %arg11, %156 : f32
                          scf.yield %157 : f32
                        } {arts.id = 152 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %133 = memref.load %95[%c2, %arg9, %arg8, %107] : memref<?x?x?x?xf32>
                        %134 = memref.load %95[%c2, %arg9, %arg8, %106] : memref<?x?x?x?xf32>
                        %135 = arith.subf %133, %134 : f32
                        %136 = arith.mulf %111, %132 : f32
                        %137 = arith.mulf %120, %135 : f32
                        %138 = arith.mulf %137, %cst : f32
                        %139 = arith.addf %136, %138 : f32
                        memref.store %139, %81[%c2, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                      } {arts.id = 162 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                  }
                  %99 = arith.maxui %86, %73 : index
                  %100 = arith.minui %90, %69 : index
                  %101 = arith.addi %arg6, %c1 : index
                  %102 = arith.subi %101, %61 : index
                  %103 = arts.db_ref %arg4[%102] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  scf.for %arg7 = %99 to %100 step %c1 {
                    %104 = arith.addi %82, %arg7 : index
                    %105 = arith.subi %104, %23 : index
                    %106 = arith.addi %arg7, %c-1 : index
                    %107 = arith.addi %arg7, %c1 : index
                    %108 = arith.subi %107, %69 : index
                    %109 = arith.index_cast %104 : index to i32
                    scf.for %arg8 = %c4 to %c316 step %c1 {
                      %110 = arith.addi %arg8, %c1 : index
                      %111 = arith.addi %arg8, %c-1 : index
                      scf.for %arg9 = %c4 to %c316 step %c1 {
                        %112 = memref.load %79[%arg9, %arg8, %105] : memref<?x?x?xf32>
                        %113 = memref.load %80[%arg9, %arg8, %105] : memref<?x?x?xf32>
                        %114 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %141 = arith.index_cast %arg10 : index to i32
                          %142 = arith.addi %arg10, %c2 : index
                          %143 = memref.load %alloca[%142] : memref<5xf32>
                          %144 = arith.addi %arg9, %arg10 : index
                          %145 = memref.load %95[%c0, %144, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %146 = arith.addi %arg8, %arg10 : index
                          %147 = memref.load %95[%c0, %arg9, %146, %arg7] : memref<?x?x?x?xf32>
                          %148 = arith.addf %145, %147 : f32
                          %149 = arith.addi %109, %141 : i32
                          %150 = arith.index_cast %149 : i32 to index
                          %151 = arith.divui %150, %69 : index
                          %152 = arith.subi %151, %61 : index
                          %153 = arith.remui %150, %69 : index
                          %154 = arts.db_ref %arg4[%152] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %155 = memref.load %154[%c0, %arg9, %arg8, %153] : memref<?x?x?x?xf32>
                          %156 = arith.addf %148, %155 : f32
                          %157 = arith.mulf %143, %156 : f32
                          %158 = arith.addf %arg11, %157 : f32
                          scf.yield %158 : f32
                        } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %115 = arith.addi %arg9, %c1 : index
                        %116 = memref.load %95[%c0, %115, %arg8, %arg7] : memref<?x?x?x?xf32>
                        %117 = arith.addi %arg9, %c-1 : index
                        %118 = memref.load %95[%c0, %117, %arg8, %arg7] : memref<?x?x?x?xf32>
                        %119 = arith.subf %116, %118 : f32
                        %120 = arith.mulf %112, %114 : f32
                        %121 = arith.addf %113, %112 : f32
                        %122 = arith.mulf %121, %119 : f32
                        %123 = arith.mulf %122, %cst : f32
                        %124 = arith.addf %120, %123 : f32
                        memref.store %124, %81[%c0, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                        %125 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %141 = arith.index_cast %arg10 : index to i32
                          %142 = arith.addi %arg10, %c2 : index
                          %143 = memref.load %alloca[%142] : memref<5xf32>
                          %144 = arith.addi %arg9, %arg10 : index
                          %145 = memref.load %95[%c1, %144, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %146 = arith.addi %arg8, %arg10 : index
                          %147 = memref.load %95[%c1, %arg9, %146, %arg7] : memref<?x?x?x?xf32>
                          %148 = arith.addf %145, %147 : f32
                          %149 = arith.addi %109, %141 : i32
                          %150 = arith.index_cast %149 : i32 to index
                          %151 = arith.divui %150, %69 : index
                          %152 = arith.subi %151, %61 : index
                          %153 = arith.remui %150, %69 : index
                          %154 = arts.db_ref %arg4[%152] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %155 = memref.load %154[%c1, %arg9, %arg8, %153] : memref<?x?x?x?xf32>
                          %156 = arith.addf %148, %155 : f32
                          %157 = arith.mulf %143, %156 : f32
                          %158 = arith.addf %arg11, %157 : f32
                          scf.yield %158 : f32
                        } {arts.id = 134 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %126 = memref.load %95[%c1, %arg9, %110, %arg7] : memref<?x?x?x?xf32>
                        %127 = memref.load %95[%c1, %arg9, %111, %arg7] : memref<?x?x?x?xf32>
                        %128 = arith.subf %126, %127 : f32
                        %129 = arith.mulf %112, %125 : f32
                        %130 = arith.mulf %121, %128 : f32
                        %131 = arith.mulf %130, %cst : f32
                        %132 = arith.addf %129, %131 : f32
                        memref.store %132, %81[%c1, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                        %133 = scf.for %arg10 = %c-2 to %c3 step %c1 iter_args(%arg11 = %cst_6) -> (f32) {
                          %141 = arith.index_cast %arg10 : index to i32
                          %142 = arith.addi %arg10, %c2 : index
                          %143 = memref.load %alloca[%142] : memref<5xf32>
                          %144 = arith.addi %arg9, %arg10 : index
                          %145 = memref.load %95[%c2, %144, %arg8, %arg7] : memref<?x?x?x?xf32>
                          %146 = arith.addi %arg8, %arg10 : index
                          %147 = memref.load %95[%c2, %arg9, %146, %arg7] : memref<?x?x?x?xf32>
                          %148 = arith.addf %145, %147 : f32
                          %149 = arith.addi %109, %141 : i32
                          %150 = arith.index_cast %149 : i32 to index
                          %151 = arith.divui %150, %69 : index
                          %152 = arith.subi %151, %61 : index
                          %153 = arith.remui %150, %69 : index
                          %154 = arts.db_ref %arg4[%152] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                          %155 = memref.load %154[%c2, %arg9, %arg8, %153] : memref<?x?x?x?xf32>
                          %156 = arith.addf %148, %155 : f32
                          %157 = arith.mulf %143, %156 : f32
                          %158 = arith.addf %arg11, %157 : f32
                          scf.yield %158 : f32
                        } {arts.id = 152 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                        %134 = memref.load %103[%c2, %arg9, %arg8, %108] : memref<?x?x?x?xf32>
                        %135 = memref.load %95[%c2, %arg9, %arg8, %106] : memref<?x?x?x?xf32>
                        %136 = arith.subf %134, %135 : f32
                        %137 = arith.mulf %112, %133 : f32
                        %138 = arith.mulf %121, %136 : f32
                        %139 = arith.mulf %138, %cst : f32
                        %140 = arith.addf %137, %139 : f32
                        memref.store %140, %81[%c2, %arg9, %arg8, %105] : memref<?x?x?x?xf32>
                      } {arts.id = 162 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                    } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                  }
                }
              } else {
                %71 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                %72 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                %73 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                scf.for %arg6 = %65 to %68 step %c1 {
                  %74 = arith.addi %23, %arg6 : index
                  %75 = arith.index_cast %74 : index to i32
                  %76 = arith.addi %75, %c1_i32 : i32
                  %77 = arith.index_cast %76 : i32 to index
                  %78 = arith.addi %75, %c-1_i32 : i32
                  %79 = arith.index_cast %78 : i32 to index
                  %80 = arith.divui %74, %69 : index
                  %81 = arith.remui %74, %69 : index
                  %82 = arith.divui %77, %69 : index
                  %83 = arith.remui %77, %69 : index
                  %84 = arith.divui %79, %69 : index
                  %85 = arith.remui %79, %69 : index
                  %86 = arith.subi %80, %61 : index
                  %87 = arith.subi %82, %61 : index
                  %88 = arith.subi %84, %61 : index
                  %89 = arts.db_ref %arg4[%86] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  %90 = arts.db_ref %arg4[%87] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  %91 = arts.db_ref %arg4[%88] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  scf.for %arg7 = %c4 to %c316 step %c1 {
                    %92 = arith.addi %arg7, %c1 : index
                    %93 = arith.addi %arg7, %c-1 : index
                    scf.for %arg8 = %c4 to %c316 step %c1 {
                      %94 = memref.load %71[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                      %95 = memref.load %72[%arg8, %arg7, %arg6] : memref<?x?x?xf32>
                      %96 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                        %123 = arith.index_cast %arg9 : index to i32
                        %124 = arith.addi %arg9, %c2 : index
                        %125 = memref.load %alloca[%124] : memref<5xf32>
                        %126 = arith.addi %arg8, %arg9 : index
                        %127 = memref.load %89[%c0, %126, %arg7, %81] : memref<?x?x?x?xf32>
                        %128 = arith.addi %arg7, %arg9 : index
                        %129 = memref.load %89[%c0, %arg8, %128, %81] : memref<?x?x?x?xf32>
                        %130 = arith.addf %127, %129 : f32
                        %131 = arith.addi %75, %123 : i32
                        %132 = arith.index_cast %131 : i32 to index
                        %133 = arith.divui %132, %69 : index
                        %134 = arith.subi %133, %61 : index
                        %135 = arith.remui %132, %69 : index
                        %136 = arts.db_ref %arg4[%134] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                        %137 = memref.load %136[%c0, %arg8, %arg7, %135] : memref<?x?x?x?xf32>
                        %138 = arith.addf %130, %137 : f32
                        %139 = arith.mulf %125, %138 : f32
                        %140 = arith.addf %arg10, %139 : f32
                        scf.yield %140 : f32
                      } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                      %97 = arith.addi %arg8, %c1 : index
                      %98 = memref.load %89[%c0, %97, %arg7, %81] : memref<?x?x?x?xf32>
                      %99 = arith.addi %arg8, %c-1 : index
                      %100 = memref.load %89[%c0, %99, %arg7, %81] : memref<?x?x?x?xf32>
                      %101 = arith.subf %98, %100 : f32
                      %102 = arith.mulf %94, %96 : f32
                      %103 = arith.addf %95, %94 : f32
                      %104 = arith.mulf %103, %101 : f32
                      %105 = arith.mulf %104, %cst : f32
                      %106 = arith.addf %102, %105 : f32
                      memref.store %106, %73[%c0, %arg8, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %107 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                        %123 = arith.index_cast %arg9 : index to i32
                        %124 = arith.addi %arg9, %c2 : index
                        %125 = memref.load %alloca[%124] : memref<5xf32>
                        %126 = arith.addi %arg8, %arg9 : index
                        %127 = memref.load %89[%c1, %126, %arg7, %81] : memref<?x?x?x?xf32>
                        %128 = arith.addi %arg7, %arg9 : index
                        %129 = memref.load %89[%c1, %arg8, %128, %81] : memref<?x?x?x?xf32>
                        %130 = arith.addf %127, %129 : f32
                        %131 = arith.addi %75, %123 : i32
                        %132 = arith.index_cast %131 : i32 to index
                        %133 = arith.divui %132, %69 : index
                        %134 = arith.subi %133, %61 : index
                        %135 = arith.remui %132, %69 : index
                        %136 = arts.db_ref %arg4[%134] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                        %137 = memref.load %136[%c1, %arg8, %arg7, %135] : memref<?x?x?x?xf32>
                        %138 = arith.addf %130, %137 : f32
                        %139 = arith.mulf %125, %138 : f32
                        %140 = arith.addf %arg10, %139 : f32
                        scf.yield %140 : f32
                      } {arts.id = 134 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                      %108 = memref.load %89[%c1, %arg8, %92, %81] : memref<?x?x?x?xf32>
                      %109 = memref.load %89[%c1, %arg8, %93, %81] : memref<?x?x?x?xf32>
                      %110 = arith.subf %108, %109 : f32
                      %111 = arith.mulf %94, %107 : f32
                      %112 = arith.mulf %103, %110 : f32
                      %113 = arith.mulf %112, %cst : f32
                      %114 = arith.addf %111, %113 : f32
                      memref.store %114, %73[%c1, %arg8, %arg7, %arg6] : memref<?x?x?x?xf32>
                      %115 = scf.for %arg9 = %c-2 to %c3 step %c1 iter_args(%arg10 = %cst_6) -> (f32) {
                        %123 = arith.index_cast %arg9 : index to i32
                        %124 = arith.addi %arg9, %c2 : index
                        %125 = memref.load %alloca[%124] : memref<5xf32>
                        %126 = arith.addi %arg8, %arg9 : index
                        %127 = memref.load %89[%c2, %126, %arg7, %81] : memref<?x?x?x?xf32>
                        %128 = arith.addi %arg7, %arg9 : index
                        %129 = memref.load %89[%c2, %arg8, %128, %81] : memref<?x?x?x?xf32>
                        %130 = arith.addf %127, %129 : f32
                        %131 = arith.addi %75, %123 : i32
                        %132 = arith.index_cast %131 : i32 to index
                        %133 = arith.divui %132, %69 : index
                        %134 = arith.subi %133, %61 : index
                        %135 = arith.remui %132, %69 : index
                        %136 = arts.db_ref %arg4[%134] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                        %137 = memref.load %136[%c2, %arg8, %arg7, %135] : memref<?x?x?x?xf32>
                        %138 = arith.addf %130, %137 : f32
                        %139 = arith.mulf %125, %138 : f32
                        %140 = arith.addf %arg10, %139 : f32
                        scf.yield %140 : f32
                      } {arts.id = 152 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                      %116 = memref.load %90[%c2, %arg8, %arg7, %83] : memref<?x?x?x?xf32>
                      %117 = memref.load %91[%c2, %arg8, %arg7, %85] : memref<?x?x?x?xf32>
                      %118 = arith.subf %116, %117 : f32
                      %119 = arith.mulf %94, %115 : f32
                      %120 = arith.mulf %103, %118 : f32
                      %121 = arith.mulf %120, %cst : f32
                      %122 = arith.addf %119, %121 : f32
                      memref.store %122, %73[%c2, %arg8, %arg7, %arg6] : memref<?x?x?x?xf32>
                    } {arts.id = 162 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                  } {arts.id = 164 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
                } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 568 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
              }
              arts.db_release(%arg2) : memref<?xmemref<?x?x?xf32>>
              arts.db_release(%arg3) : memref<?xmemref<?x?x?xf32>>
              arts.db_release(%arg4) : memref<?xmemref<?x?x?x?xf32>>
              arts.db_release(%arg5) : memref<?xmemref<?x?x?x?xf32>>
            }
          }
        } {arts.id = 166 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 568 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">}
      } : i64
      func.call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    } {arts.id = 97 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:149:25">}
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %19 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%19, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %20 = scf.for %arg0 = %c0 to %c320 step %c1 iter_args(%arg1 = %cst_11) -> (f64) {
      %22 = arith.divui %arg0, %c9 : index
      %23 = arith.remui %arg0, %c9 : index
      %24 = arts.db_ref %ptr_13[%22] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
      %25 = scf.for %arg2 = %c0 to %c3 step %c1 iter_args(%arg3 = %arg1) -> (f64) {
        %26 = memref.load %24[%arg2, %arg0, %arg0, %23] : memref<?x?x?x?xf32>
        %27 = arith.extf %26 : f32 to f64
        %28 = math.absf %27 : f64
        %29 = arith.addf %arg3, %28 : f64
        scf.yield %29 : f64
      } {arts.id = 170 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:163:23">}
      scf.yield %25 : f64
    } {arts.id = 168 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:162:21">}
    call @carts_bench_checksum_d(%20) : (f64) -> ()
    call @carts_phase_timer_stop(%19) : (memref<?xi8>) -> ()
    %21 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%21, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%21) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_16) : memref<?xi64>
    arts.db_free(%ptr_17) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_14) : memref<?xi64>
    arts.db_free(%ptr_15) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_12) : memref<?xi64>
    arts.db_free(%ptr_13) : memref<?xmemref<?x?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?x?xf32>>
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
