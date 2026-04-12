module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c7 = arith.constant 7 : index
    %cst = arith.constant 2.500000e-01 : f64
    %true = arith.constant true
    %c1_i32 = arith.constant 1 : i32
    %c63_i32 = arith.constant 63 : i32
    %c-1 = arith.constant -1 : index
    %c8 = arith.constant 8 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %c1_0 = arith.constant 1 : index
    %c1_1 = arith.constant 1 : index
    %c0_2 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c64, %c64] {arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %c1_3 = arith.constant 1 : index
    %c1_4 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %0 = arith.addi %c8, %c2 : index
    %c1_5 = arith.constant 1 : index
    %1 = arith.maxui %0, %c1_5 : index
    %c64_6 = arith.constant 64 : index
    %c1_i64 = arith.constant 1 : i64
    %2 = arith.index_cast %c64 : index to i64
    %3 = arith.index_cast %1 : index to i64
    %4 = arith.addi %2, %3 : i64
    %5 = arith.subi %4, %c1_i64 : i64
    %6 = arith.divui %5, %3 : i64
    %7 = arith.index_cast %6 : i64 to index
    %c0_7 = arith.constant 0 : index
    %c1_8 = arith.constant 1 : index
    %c0_9 = arith.constant 0 : index
    %c1_10 = arith.constant 1 : index
    %guid_11, %ptr_12 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%7] elementType(f64) elementSizes[%1, %c64] {arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_12 : memref<?xmemref<?x?xf64>>) block_shape[%1] contract(<ownerDims = [0], postDbRefined = true>)
    %c1_13 = arith.constant 1 : index
    %c1_14 = arith.constant 1 : index
    %c1_15 = arith.constant 1 : index
    %8 = arith.maxui %c8, %c1_15 : index
    %c1_i64_16 = arith.constant 1 : i64
    %9 = arith.index_cast %c64 : index to i64
    %10 = arith.index_cast %8 : index to i64
    %11 = arith.addi %9, %10 : i64
    %12 = arith.subi %11, %c1_i64_16 : i64
    %13 = arith.divui %12, %10 : i64
    %14 = arith.index_cast %13 : i64 to index
    %c0_17 = arith.constant 0 : index
    %c1_18 = arith.constant 1 : index
    %c0_19 = arith.constant 0 : index
    %c1_20 = arith.constant 1 : index
    %guid_21, %ptr_22 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%14] elementType(f64) elementSizes[%8, %c64] {arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?xf64>>) block_shape[%8] contract(<ownerDims = [0], postDbRefined = true>)
    %15 = arts.epoch attributes {arts.pattern_revision = 1 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %17 = arith.muli %arg0, %c8 : index
        %18 = arith.cmpi uge, %17, %c64 : index
        %19 = arith.subi %c64, %17 : index
        %20 = arith.select %18, %c0, %19 : index
        %21 = arith.minui %20, %c8 : index
        %22 = arith.cmpi ugt, %21, %c0 : index
        scf.if %22 {
          %23 = arith.divui %17, %c64 : index
          %24 = arith.addi %17, %c7 : index
          %25 = arith.divui %24, %c64 : index
          %26 = arith.cmpi ugt, %23, %c0 : index
          %27 = arith.select %26, %25, %c0 : index
          %28 = arith.subi %27, %23 : index
          %29 = arith.addi %28, %c1 : index
          %30 = arith.select %26, %c0, %23 : index
          %31 = arith.select %26, %c0, %29 : index
          %guid_23, %ptr_24 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%17], sizes[%c8]), offsets[%30], sizes[%31] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [64, 8], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c64] contract(<ownerDims = [1], postDbRefined = true>)
          %c0_25 = arith.constant 0 : index
          %c1_26 = arith.constant 1 : index
          %c0_27 = arith.constant 0 : index
          %guid_28, %ptr_29 = arts.db_acquire[<inout>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%17], sizes[%c8]), offsets[%c0_27], sizes[%14] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_29 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 1 : i64>)
          %c0_30 = arith.constant 0 : index
          %c1_31 = arith.constant 1 : index
          %c0_32 = arith.constant 0 : index
          %guid_33, %ptr_34 = arts.db_acquire[<inout>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%17], sizes[%c8]), offsets[%c0_32], sizes[%7] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_34 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 1 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_29, %ptr_34) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.pattern_revision = 1 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [64, 8]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>, %arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>):
            %c1_35 = arith.constant 1 : index
            %c0_36 = arith.constant 0 : index
            %32 = arith.maxui %1, %c1_35 : index
            %c0_37 = arith.constant 0 : index
            %c1_38 = arith.constant 1 : index
            %c0_39 = arith.constant 0 : index
            %33 = arith.maxui %8, %c1_38 : index
            %c0_40 = arith.constant 0 : index
            %34 = arith.subi %c0, %17 : index
            %35 = arith.cmpi slt, %34, %c0 : index
            %36 = arith.select %35, %c0, %34 : index
            %37 = arith.cmpi slt, %19, %c0 : index
            %38 = arith.select %37, %c0, %19 : index
            %39 = arith.minui %38, %21 : index
            scf.for %arg4 = %36 to %39 step %c1 {
              %40 = arith.addi %17, %arg4 : index
              %41 = arith.index_cast %40 : index to i32
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %44 = arith.index_cast %arg5 : index to i32
                %45 = arith.addi %44, %41 : i32
                %46 = arith.sitofp %45 : i32 to f64
                %47 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                memref.store %46, %47[%arg5, %40] : memref<?x?xf64>
              }
              %42 = arith.addi %40, %c8 : index
              %43 = arith.minui %42, %c64 : index
              scf.for %arg5 = %40 to %43 step %c1 {
                scf.for %arg6 = %c0 to %c64 step %c1 {
                  %c1_41 = arith.constant 1 : index
                  %44 = arith.maxui %33, %c1_41 : index
                  %c0_42 = arith.constant 0 : index
                  %c1_43 = arith.constant 1 : index
                  %c-1_44 = arith.constant -1 : index
                  %45 = arith.divui %40, %44 : index
                  %46 = arith.remui %40, %44 : index
                  %47 = arith.subi %arg5, %40 : index
                  %48 = arith.addi %46, %47 : index
                  %49 = arith.subi %45, %c0_40 : index
                  %50 = arith.cmpi slt, %48, %c0_42 : index
                  %51 = arith.cmpi sge, %48, %44 : index
                  %52 = arith.select %50, %c-1_44, %c0_42 : index
                  %53 = arith.select %51, %c1_43, %52 : index
                  %54 = arith.addi %49, %53 : index
                  %55 = arith.subi %48, %44 : index
                  %56 = arith.addi %48, %44 : index
                  %57 = arith.select %50, %56, %48 : index
                  %58 = arith.select %51, %55, %57 : index
                  %59 = arts.db_ref %arg2[%54] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %60 = memref.load %59[%58, %arg6] : memref<?x?xf64>
                  %c1_45 = arith.constant 1 : index
                  %61 = arith.maxui %32, %c1_45 : index
                  %62 = arith.divui %arg5, %61 : index
                  %63 = arith.subi %62, %c0_37 : index
                  %64 = arith.remui %arg5, %61 : index
                  %65 = arts.db_ref %arg3[%63] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %60, %65[%64, %arg6] : memref<?x?xf64>
                }
              }
            }
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
          }
        }
      }
    } : i64
    %16 = arts.epoch attributes {arts.pattern_revision = 1 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 4000 : i64, arts.plan.halo_shape = [2, 2], arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.owner_dims = [0, 1], arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [1, 1]} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %17 = arith.muli %arg0, %c8 : index
        %18 = arith.cmpi uge, %17, %c64 : index
        %19 = arith.subi %c64, %17 : index
        %20 = arith.select %18, %c0, %19 : index
        %21 = arith.minui %20, %c8 : index
        %22 = arith.cmpi ugt, %21, %c0 : index
        scf.if %22 {
          %23 = arith.divui %17, %c64 : index
          %24 = arith.addi %17, %c7 : index
          %25 = arith.divui %24, %c64 : index
          %26 = arith.cmpi ugt, %23, %c0 : index
          %27 = arith.select %26, %25, %c0 : index
          %28 = arith.subi %27, %23 : index
          %29 = arith.addi %28, %c1 : index
          %30 = arith.select %26, %c0, %23 : index
          %31 = arith.select %26, %c0, %29 : index
          %guid_23, %ptr_24 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%17], sizes[%c8]), offsets[%30], sizes[%31] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c8] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c1] contract(<ownerDims = [0], spatialDims = [0, 1], postDbRefined = true>)
          %c1_25 = arith.constant 1 : index
          %c0_26 = arith.constant 0 : index
          %32 = arith.maxui %8, %c1_25 : index
          %33 = arith.muli %14, %32 : index
          %34 = arith.divui %17, %32 : index
          %35 = arith.addi %17, %c8 : index
          %36 = arith.subi %35, %c1_25 : index
          %37 = arith.divui %36, %32 : index
          %38 = arith.subi %14, %c1_25 : index
          %39 = arith.cmpi ugt, %34, %38 : index
          %40 = arith.minui %37, %38 : index
          %41 = arith.select %39, %37, %40 : index
          %42 = arith.subi %41, %34 : index
          %43 = arith.addi %42, %c1_25 : index
          %44 = arith.select %39, %c0_26, %34 : index
          %45 = arith.select %39, %c0_26, %43 : index
          %guid_27, %ptr_28 = arts.db_acquire[<inout>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%17], sizes[%c8]), offsets[%44], sizes[%c1_25] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_28 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c8] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c1] contract(<ownerDims = [0], spatialDims = [0, 1]>)
          %c1_29 = arith.constant 1 : index
          %46 = arith.cmpi uge, %17, %c1_29 : index
          %47 = arith.subi %17, %c1_29 : index
          %c0_30 = arith.constant 0 : index
          %48 = arith.select %46, %47, %c0_30 : index
          %c2_31 = arith.constant 2 : index
          %49 = arith.addi %c8, %c2_31 : index
          %c1_32 = arith.constant 1 : index
          %c0_33 = arith.constant 0 : index
          %50 = arith.maxui %1, %c1_32 : index
          %51 = arith.muli %7, %50 : index
          %52 = arith.divui %17, %50 : index
          %53 = arith.addi %17, %49 : index
          %54 = arith.subi %53, %c1_32 : index
          %55 = arith.divui %54, %50 : index
          %56 = arith.subi %7, %c1_32 : index
          %57 = arith.cmpi ugt, %52, %56 : index
          %58 = arith.minui %55, %56 : index
          %59 = arith.select %57, %55, %58 : index
          %60 = arith.subi %59, %52 : index
          %61 = arith.addi %60, %c1_32 : index
          %62 = arith.select %57, %c0_33, %52 : index
          %63 = arith.select %57, %c0_33, %61 : index
          %guid_34, %ptr_35 = arts.db_acquire[<inout>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%17], sizes[%c8]), offsets[%62], sizes[%63] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_35 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c8] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c1] contract(<ownerDims = [0], spatialDims = [0, 1]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_28, %ptr_35) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.pattern_revision = 1 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 4000 : i64, arts.plan.halo_shape = [2, 2], arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.owner_dims = [0, 1], arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<stencil_tiling_nd>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [8, 64], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [1, 1]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>, %arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>):
            %c1_36 = arith.constant 1 : index
            %c0_37 = arith.constant 0 : index
            %64 = arith.maxui %1, %c1_36 : index
            %c0_38 = arith.constant 0 : index
            %c8_39 = arith.constant 8 : index
            %65 = arith.muli %arg0, %c8_39 : index
            %c2_40 = arith.constant 2 : index
            %66 = arith.addi %c8_39, %c2_40 : index
            %c1_41 = arith.constant 1 : index
            %67 = arith.maxui %66, %c1_41 : index
            %c1_42 = arith.constant 1 : index
            %68 = arith.maxui %67, %c1_42 : index
            %69 = arith.divui %65, %68 : index
            %70 = arith.select %57, %c0_38, %69 : index
            %c1_43 = arith.constant 1 : index
            %c0_44 = arith.constant 0 : index
            %71 = arith.maxui %8, %c1_43 : index
            %c0_45 = arith.constant 0 : index
            %c8_46 = arith.constant 8 : index
            %72 = arith.muli %arg0, %c8_46 : index
            %c1_47 = arith.constant 1 : index
            %73 = arith.maxui %c8_46, %c1_47 : index
            %c1_48 = arith.constant 1 : index
            %74 = arith.maxui %73, %c1_48 : index
            %75 = arith.divui %72, %74 : index
            %76 = arith.select %39, %c0_45, %75 : index
            %77 = arith.subi %c0, %17 : index
            %78 = arith.cmpi slt, %77, %c0 : index
            %79 = arith.select %78, %c0, %77 : index
            %80 = arith.cmpi slt, %19, %c0 : index
            %81 = arith.select %80, %c0, %19 : index
            %82 = arith.minui %81, %21 : index
            scf.for %arg4 = %79 to %82 step %c1 {
              %83 = arith.addi %17, %arg4 : index
              %84 = arith.index_cast %83 : index to i32
              %85 = arith.cmpi eq, %84, %c0_i32 : i32
              %86 = arith.cmpi eq, %84, %c63_i32 : i32
              %87 = arith.addi %84, %c-1_i32 : i32
              %88 = arith.index_cast %87 : i32 to index
              %89 = arith.addi %84, %c1_i32 : i32
              %90 = arith.index_cast %89 : i32 to index
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %91 = arith.index_cast %arg5 : index to i32
                %92 = scf.if %85 -> (i1) {
                  scf.yield %true : i1
                } else {
                  %95 = arith.cmpi eq, %91, %c0_i32 : i32
                  scf.yield %95 : i1
                }
                %93 = arith.select %92, %true, %86 : i1
                %94 = scf.if %93 -> (i1) {
                  scf.yield %true : i1
                } else {
                  %95 = arith.cmpi eq, %91, %c63_i32 : i32
                  scf.yield %95 : i1
                }
                scf.if %94 {
                  %95 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %96 = memref.load %95[%83, %arg5] : memref<?x?xf64>
                  %c1_49 = arith.constant 1 : index
                  %97 = arith.maxui %71, %c1_49 : index
                  %c0_50 = arith.constant 0 : index
                  %c1_51 = arith.constant 1 : index
                  %c-1_52 = arith.constant -1 : index
                  %98 = arith.addi %17, %79 : index
                  %99 = arith.divui %98, %97 : index
                  %100 = arith.subi %arg4, %79 : index
                  %101 = arith.addi %79, %100 : index
                  %102 = arith.subi %99, %76 : index
                  %103 = arts.db_ref %arg2[%c0_44] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %96, %103[%arg4, %arg5] : memref<?x?xf64>
                } else {
                  %c1_49 = arith.constant 1 : index
                  %95 = arith.maxui %64, %c1_49 : index
                  %c0_50 = arith.constant 0 : index
                  %c1_51 = arith.constant 1 : index
                  %c-1_52 = arith.constant -1 : index
                  %96 = arith.addi %17, %79 : index
                  %97 = arith.divui %96, %95 : index
                  %98 = arith.remui %96, %95 : index
                  %99 = arith.subi %arg4, %79 : index
                  %100 = arith.addi %98, %99 : index
                  %c1_53 = arith.constant 1 : index
                  %101 = arith.subi %100, %c1_53 : index
                  %102 = arith.subi %97, %70 : index
                  %103 = arith.cmpi slt, %101, %c0_50 : index
                  %104 = arith.cmpi sge, %101, %95 : index
                  %105 = arith.select %103, %c-1_52, %c0_50 : index
                  %106 = arith.select %104, %c1_51, %105 : index
                  %107 = arith.addi %102, %106 : index
                  %108 = arith.subi %101, %95 : index
                  %109 = arith.addi %101, %95 : index
                  %110 = arith.select %103, %109, %101 : index
                  %111 = arith.select %104, %108, %110 : index
                  %112 = arts.db_ref %arg3[%107] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %113 = memref.load %112[%111, %arg5] : memref<?x?xf64>
                  %114 = arith.addi %91, %c1_i32 : i32
                  %115 = arith.index_cast %114 : i32 to index
                  %c1_54 = arith.constant 1 : index
                  %116 = arith.maxui %64, %c1_54 : index
                  %c0_55 = arith.constant 0 : index
                  %c1_56 = arith.constant 1 : index
                  %c-1_57 = arith.constant -1 : index
                  %117 = arith.addi %17, %79 : index
                  %118 = arith.divui %117, %116 : index
                  %119 = arith.remui %117, %116 : index
                  %120 = arith.subi %arg4, %79 : index
                  %121 = arith.addi %119, %120 : index
                  %122 = arith.subi %118, %70 : index
                  %123 = arith.cmpi slt, %121, %c0_55 : index
                  %124 = arith.cmpi sge, %121, %116 : index
                  %125 = arith.select %123, %c-1_57, %c0_55 : index
                  %126 = arith.select %124, %c1_56, %125 : index
                  %127 = arith.addi %122, %126 : index
                  %128 = arith.subi %121, %116 : index
                  %129 = arith.addi %121, %116 : index
                  %130 = arith.select %123, %129, %121 : index
                  %131 = arith.select %124, %128, %130 : index
                  %132 = arts.db_ref %arg3[%127] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %133 = memref.load %132[%131, %115] : memref<?x?xf64>
                  %134 = arith.addf %113, %133 : f64
                  %135 = arith.addi %91, %c-1_i32 : i32
                  %136 = arith.index_cast %135 : i32 to index
                  %c1_58 = arith.constant 1 : index
                  %137 = arith.maxui %64, %c1_58 : index
                  %c0_59 = arith.constant 0 : index
                  %c1_60 = arith.constant 1 : index
                  %c-1_61 = arith.constant -1 : index
                  %138 = arith.addi %17, %79 : index
                  %139 = arith.divui %138, %137 : index
                  %140 = arith.remui %138, %137 : index
                  %141 = arith.subi %arg4, %79 : index
                  %142 = arith.addi %140, %141 : index
                  %143 = arith.subi %139, %70 : index
                  %144 = arith.cmpi slt, %142, %c0_59 : index
                  %145 = arith.cmpi sge, %142, %137 : index
                  %146 = arith.select %144, %c-1_61, %c0_59 : index
                  %147 = arith.select %145, %c1_60, %146 : index
                  %148 = arith.addi %143, %147 : index
                  %149 = arith.subi %142, %137 : index
                  %150 = arith.addi %142, %137 : index
                  %151 = arith.select %144, %150, %142 : index
                  %152 = arith.select %145, %149, %151 : index
                  %153 = arts.db_ref %arg3[%148] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %154 = memref.load %153[%152, %136] : memref<?x?xf64>
                  %155 = arith.addf %134, %154 : f64
                  %c1_62 = arith.constant 1 : index
                  %156 = arith.maxui %64, %c1_62 : index
                  %c0_63 = arith.constant 0 : index
                  %c1_64 = arith.constant 1 : index
                  %c-1_65 = arith.constant -1 : index
                  %157 = arith.addi %17, %79 : index
                  %158 = arith.divui %157, %156 : index
                  %159 = arith.remui %157, %156 : index
                  %160 = arith.subi %arg4, %79 : index
                  %161 = arith.addi %159, %160 : index
                  %c1_66 = arith.constant 1 : index
                  %162 = arith.addi %161, %c1_66 : index
                  %163 = arith.subi %158, %70 : index
                  %164 = arith.cmpi slt, %162, %c0_63 : index
                  %165 = arith.cmpi sge, %162, %156 : index
                  %166 = arith.select %164, %c-1_65, %c0_63 : index
                  %167 = arith.select %165, %c1_64, %166 : index
                  %168 = arith.addi %163, %167 : index
                  %169 = arith.subi %162, %156 : index
                  %170 = arith.addi %162, %156 : index
                  %171 = arith.select %164, %170, %162 : index
                  %172 = arith.select %165, %169, %171 : index
                  %173 = arts.db_ref %arg3[%168] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %174 = memref.load %173[%172, %arg5] : memref<?x?xf64>
                  %175 = arith.addf %155, %174 : f64
                  %176 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %177 = memref.load %176[%83, %arg5] : memref<?x?xf64>
                  %178 = arith.addf %175, %177 : f64
                  %179 = arith.mulf %178, %cst : f64
                  %c1_67 = arith.constant 1 : index
                  %180 = arith.maxui %71, %c1_67 : index
                  %c0_68 = arith.constant 0 : index
                  %c1_69 = arith.constant 1 : index
                  %c-1_70 = arith.constant -1 : index
                  %181 = arith.addi %17, %79 : index
                  %182 = arith.divui %181, %180 : index
                  %183 = arith.subi %arg4, %79 : index
                  %184 = arith.addi %79, %183 : index
                  %185 = arith.subi %182, %76 : index
                  %186 = arts.db_ref %arg2[%c0_44] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %179, %186[%arg4, %arg5] : memref<?x?xf64>
                }
              }
            }
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
          }
        }
      }
    } : i64
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?x?xf64>>
    return %c0_i32 : i32
  }
}
