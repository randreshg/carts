module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-7 = arith.constant -7 : index
    %c2 = arith.constant 2 : index
    %c-9 = arith.constant -9 : index
    %c7 = arith.constant 7 : index
    %0 = llvm.mlir.zero : !llvm.ptr
    %cst = arith.constant 2.500000e-01 : f64
    %true = arith.constant true
    %c63_i32 = arith.constant 63 : i32
    %c-1 = arith.constant -1 : index
    %c8 = arith.constant 8 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c8] elementType(f64) elementSizes[%c64, %c8] {arts.id = 140 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%c8] {owner_dims = array<i64: 1>, post_db_refined}
    %guid_0, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%c8] elementType(f64) elementSizes[%c8, %c64] {arts.id = 141 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_1 : memref<?xmemref<?x?xf64>>) block_shape[%c8] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_2, %ptr_3 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c8] elementType(f64) elementSizes[%c8, %c64] {arts.id = 142 : i64, arts.local_only, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_3 : memref<?xmemref<?x?xf64>>) block_shape[%c8] {owner_dims = array<i64: 0>, post_db_refined}
    %1 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %3 = arith.muli %arg0, %c8 : index
        %4 = arith.cmpi uge, %3, %c64 : index
        %5 = arith.subi %c64, %3 : index
        %6 = arith.select %4, %c0, %5 : index
        %7 = arith.minui %6, %c8 : index
        %8 = arith.cmpi ugt, %7, %c0 : index
        scf.if %8 {
          %9 = arith.divui %3, %c8 : index
          %10 = arith.cmpi ugt, %9, %c7 : index
          %11 = arith.select %10, %c0, %9 : index
          %guid_4, %ptr_5 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%3], sizes[%c8]), offsets[%11], sizes[%c1] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [64, 8], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_5 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c64] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 1>, post_db_refined}
          %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%3], sizes[%c8]), offsets[%11], sizes[%c1] element_offsets[%3] element_sizes[%c8] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_7 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c8] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          %12 = arith.addi %3, %c7 : index
          %13 = arith.divui %12, %c8 : index
          %14 = arith.minui %13, %c7 : index
          %15 = arith.select %10, %13, %14 : index
          %16 = arith.subi %15, %9 : index
          %17 = arith.addi %16, %c1 : index
          %18 = arith.select %10, %c0, %17 : index
          %guid_8, %ptr_9 = arts.db_acquire[<out>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%11], sizes[%18] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 64], stencil_center_offset = 1 : i64, stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_9 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c8] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_5, %ptr_7, %ptr_9) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 143 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 64]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>, %arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>):
            %19 = arith.subi %c0, %3 : index
            %20 = arith.cmpi slt, %19, %c0 : index
            %21 = arith.select %20, %c0, %19 : index
            %22 = arith.cmpi slt, %5, %c0 : index
            %23 = arith.select %22, %c0, %5 : index
            %24 = arith.minui %23, %7 : index
            %25 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            %26 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            %27 = arith.addi %3, %21 : index
            %28 = arith.divui %27, %c8 : index
            %29 = arith.subi %28, %11 : index
            %30 = arts.db_ref %arg3[%29] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            scf.for %arg4 = %21 to %24 step %c1 {
              %31 = arith.addi %3, %arg4 : index
              %32 = arith.index_cast %31 : index to i32
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %33 = arith.index_cast %arg5 : index to i32
                %34 = arith.addi %33, %32 : i32
                %35 = arith.sitofp %34 : i32 to f64
                memref.store %35, %25[%arg5, %arg4] : memref<?x?xf64>
              } {arts.id = 14 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">}
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %33 = memref.load %26[%arg4, %arg5] : memref<?x?xf64>
                memref.store %33, %30[%arg4, %arg5] : memref<?x?xf64>
              } {arts.id = 22 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">}
            } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">}
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
          }
        }
      } {arts.id = 16 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">}
    } : i64
    %2 = arts.epoch attributes {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %3 = arith.muli %arg0, %c8 : index
        %4 = arith.cmpi uge, %3, %c64 : index
        %5 = arith.subi %c64, %3 : index
        %6 = arith.select %4, %c0, %5 : index
        %7 = arith.minui %6, %c8 : index
        %8 = arith.cmpi ugt, %7, %c0 : index
        scf.if %8 {
          %guid_4, %ptr_5 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%3], sizes[%c8]), offsets[%c0], sizes[%c8] element_offsets[%3] element_sizes[%c8] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_5 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c8] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          %9 = arith.divui %3, %c8 : index
          %10 = arith.cmpi ugt, %9, %c7 : index
          %11 = arith.select %10, %c0, %9 : index
          %guid_6, %ptr_7 = arts.db_acquire[<out>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%3], sizes[%c8]), offsets[%11], sizes[%c1] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_7 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c8] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          %12 = arith.cmpi uge, %3, %c1 : index
          %13 = arith.subi %3, %c1 : index
          %14 = arith.select %12, %13, %c0 : index
          %15 = arith.addi %14, %c1 : index
          %16 = arith.divui %15, %c8 : index
          %17 = arith.subi %16, %c1 : index
          %18 = arith.cmpi ne, %16, %c0 : index
          %guid_8, %ptr_9 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%17], sizes[%c1] bounds_valid(%18) element_offsets[%c7, %c0] element_sizes[%c1, %c64] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_9 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
          %19 = arith.addi %16, %c1 : index
          %20 = arith.cmpi ne, %16, %c7 : index
          %guid_10, %ptr_11 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%19], sizes[%c1] bounds_valid(%20) element_offsets[%c0, %c0] element_sizes[%c1, %c64] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_11 : memref<?xmemref<?x?xf64>>) {critical_path_distance = 0 : i64}
          %guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<?x?xf64>>) partitioning(<stencil>), offsets[%16], sizes[%c1] element_offsets[%c0, %c0] element_sizes[%c8, %c64] {distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, left_halo_arg_idx = 3 : index, right_halo_arg_idx = 4 : index, stencil_block_shape = [8, 64], stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_13 : memref<?xmemref<?x?xf64>>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c8] min_offsets[%c-1] max_offsets[%c1] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_5, %ptr_7, %ptr_13, %ptr_9, %ptr_11) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 16 : i64, critical_path_distance = 0 : i64, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [8, 64]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>, %arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?x?xf64>>):
            %21 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            %22 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            %23 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
            %24 = polygeist.memref2pointer %22 : memref<?x?xf64> to !llvm.ptr
            %25 = llvm.icmp "ne" %24, %0 : !llvm.ptr
            %26 = polygeist.memref2pointer %23 : memref<?x?xf64> to !llvm.ptr
            %27 = llvm.icmp "ne" %26, %0 : !llvm.ptr
            %28 = arith.subi %c0, %3 : index
            %29 = arith.cmpi slt, %28, %c0 : index
            %30 = arith.select %29, %c0, %28 : index
            %31 = arith.cmpi slt, %5, %c0 : index
            %32 = arith.select %31, %c0, %5 : index
            %33 = arith.minui %32, %7 : index
            scf.for %arg6 = %30 to %33 step %c1 {
              %34 = arith.addi %arg6, %c-1 : index
              %35 = arith.cmpi slt, %34, %c0 : index
              %36 = arith.cmpi sge, %34, %c8 : index
              %37 = arith.maxsi %34, %c0 : index
              %38 = arith.minsi %37, %c7 : index
              %39 = arith.addi %arg6, %c-9 : index
              %40 = arith.maxsi %38, %c0 : index
              %41 = arith.minsi %40, %c7 : index
              %42 = arith.maxsi %arg6, %c0 : index
              %43 = arith.minsi %42, %c0 : index
              %44 = arith.maxsi %39, %c0 : index
              %45 = arith.minsi %44, %c0 : index
              %46 = arith.select %25, %43, %41 : index
              %47 = arith.select %35, %46, %41 : index
              %48 = arith.select %27, %45, %41 : index
              %49 = arith.select %36, %48, %47 : index
              %50 = arith.cmpi slt, %arg6, %c0 : index
              %51 = arith.cmpi sge, %arg6, %c8 : index
              %52 = arith.addi %arg6, %c1 : index
              %53 = arith.minsi %42, %c7 : index
              %54 = arith.subi %arg6, %c8 : index
              %55 = arith.maxsi %53, %c0 : index
              %56 = arith.minsi %55, %c7 : index
              %57 = arith.maxsi %52, %c0 : index
              %58 = arith.minsi %57, %c0 : index
              %59 = arith.maxsi %54, %c0 : index
              %60 = arith.minsi %59, %c0 : index
              %61 = arith.select %25, %58, %56 : index
              %62 = arith.select %50, %61, %56 : index
              %63 = arith.select %27, %60, %56 : index
              %64 = arith.select %51, %63, %62 : index
              %65 = arith.cmpi slt, %52, %c0 : index
              %66 = arith.cmpi sge, %52, %c8 : index
              %67 = arith.addi %arg6, %c2 : index
              %68 = arith.minsi %57, %c7 : index
              %69 = arith.addi %arg6, %c-7 : index
              %70 = arith.maxsi %68, %c0 : index
              %71 = arith.minsi %70, %c7 : index
              %72 = arith.maxsi %67, %c0 : index
              %73 = arith.minsi %72, %c0 : index
              %74 = arith.maxsi %69, %c0 : index
              %75 = arith.minsi %74, %c0 : index
              %76 = arith.select %25, %73, %71 : index
              %77 = arith.select %65, %76, %71 : index
              %78 = arith.select %27, %75, %71 : index
              %79 = arith.select %66, %78, %77 : index
              %80 = arith.addi %3, %arg6 : index
              %81 = arith.index_cast %80 : index to i32
              %82 = arith.cmpi eq, %81, %c0_i32 : i32
              %83 = arith.cmpi eq, %81, %c63_i32 : i32
              scf.for %arg7 = %c0 to %c8 step %c1 {
                %84 = arith.muli %arg7, %c8 : index
                %85 = arith.subi %c64, %84 : index
                %86 = arith.cmpi uge, %84, %c64 : index
                %87 = arith.select %86, %c0, %85 : index
                %88 = arith.minui %87, %c8 : index
                scf.for %arg8 = %c0 to %88 step %c1 {
                  %89 = arith.addi %84, %arg8 : index
                  %90 = arith.index_cast %89 : index to i32
                  %91 = scf.if %82 -> (i1) {
                    scf.yield %true : i1
                  } else {
                    %94 = arith.cmpi eq, %90, %c0_i32 : i32
                    scf.yield %94 : i1
                  }
                  %92 = arith.select %91, %true, %83 : i1
                  %93 = scf.if %92 -> (i1) {
                    scf.yield %true : i1
                  } else {
                    %94 = arith.cmpi eq, %90, %c63_i32 : i32
                    scf.yield %94 : i1
                  }
                  scf.if %93 {
                    %94 = arith.divui %89, %c8 : index
                    %95 = arith.remui %89, %c8 : index
                    %96 = arts.db_ref %arg1[%94] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    %97 = memref.load %96[%80, %95] : memref<?x?xf64>
                    %98 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    memref.store %97, %98[%arg6, %89] : memref<?x?xf64>
                  } else {
                    %94 = scf.if %36 -> (f64) {
                      %110 = scf.if %27 -> (f64) {
                        %111 = memref.load %23[%49, %89] : memref<?x?xf64>
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%49, %89] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    } else {
                      %110 = scf.if %35 -> (f64) {
                        %111 = scf.if %25 -> (f64) {
                          %112 = memref.load %22[%49, %89] : memref<?x?xf64>
                          scf.yield %112 : f64
                        } else {
                          %112 = memref.load %21[%49, %89] : memref<?x?xf64>
                          scf.yield %112 : f64
                        }
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%49, %89] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    }
                    %95 = arith.addi %89, %c1 : index
                    %96 = scf.if %51 -> (f64) {
                      %110 = scf.if %27 -> (f64) {
                        %111 = memref.load %23[%64, %95] : memref<?x?xf64>
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%64, %95] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    } else {
                      %110 = scf.if %50 -> (f64) {
                        %111 = scf.if %25 -> (f64) {
                          %112 = memref.load %22[%64, %95] : memref<?x?xf64>
                          scf.yield %112 : f64
                        } else {
                          %112 = memref.load %21[%64, %95] : memref<?x?xf64>
                          scf.yield %112 : f64
                        }
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%64, %95] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    }
                    %97 = arith.addf %94, %96 : f64
                    %98 = arith.addi %89, %c-1 : index
                    %99 = scf.if %51 -> (f64) {
                      %110 = scf.if %27 -> (f64) {
                        %111 = memref.load %23[%64, %98] : memref<?x?xf64>
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%64, %98] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    } else {
                      %110 = scf.if %50 -> (f64) {
                        %111 = scf.if %25 -> (f64) {
                          %112 = memref.load %22[%64, %98] : memref<?x?xf64>
                          scf.yield %112 : f64
                        } else {
                          %112 = memref.load %21[%64, %98] : memref<?x?xf64>
                          scf.yield %112 : f64
                        }
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%64, %98] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    }
                    %100 = arith.addf %97, %99 : f64
                    %101 = scf.if %66 -> (f64) {
                      %110 = scf.if %27 -> (f64) {
                        %111 = memref.load %23[%79, %89] : memref<?x?xf64>
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%79, %89] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    } else {
                      %110 = scf.if %65 -> (f64) {
                        %111 = scf.if %25 -> (f64) {
                          %112 = memref.load %22[%79, %89] : memref<?x?xf64>
                          scf.yield %112 : f64
                        } else {
                          %112 = memref.load %21[%79, %89] : memref<?x?xf64>
                          scf.yield %112 : f64
                        }
                        scf.yield %111 : f64
                      } else {
                        %111 = memref.load %21[%79, %89] : memref<?x?xf64>
                        scf.yield %111 : f64
                      }
                      scf.yield %110 : f64
                    }
                    %102 = arith.addf %100, %101 : f64
                    %103 = arith.divui %89, %c8 : index
                    %104 = arith.remui %89, %c8 : index
                    %105 = arts.db_ref %arg1[%103] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    %106 = memref.load %105[%80, %104] : memref<?x?xf64>
                    %107 = arith.addf %102, %106 : f64
                    %108 = arith.mulf %107, %cst : f64
                    %109 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    memref.store %108, %109[%arg6, %89] : memref<?x?xf64>
                  }
                }
              }
            } {arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">}
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?x?xf64>>
          }
        }
      } {arts.id = 14 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">}
    } : i64
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_0) : memref<?xi64>
    arts.db_free(%ptr_1) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_2) : memref<?xi64>
    arts.db_free(%ptr_3) : memref<?xmemref<?x?xf64>>
    return %c0_i32 : i32
  }
}
