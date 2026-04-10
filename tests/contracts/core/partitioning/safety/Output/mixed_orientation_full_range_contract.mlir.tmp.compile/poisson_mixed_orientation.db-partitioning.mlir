module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c7 = arith.constant 7 : index
    %cst = arith.constant 2.500000e-01 : f64
    %true = arith.constant true
    %c1_i32 = arith.constant 1 : i32
    %c63_i32 = arith.constant 63 : i32
    %c8 = arith.constant 8 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %c1_0 = arith.constant 1 : index
    %c1_1 = arith.constant 1 : index
    %c0_2 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c64, %c64] {arts.id = 2 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson_mixed_orientation.c:41:16", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32768 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 2 : i64, lastUseId = 3 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 2 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %c1_3 = arith.constant 1 : index
    %c1_4 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %0 = arith.addi %c8, %c2 : index
    %1 = arith.minui %c8, %0 : index
    %c1_5 = arith.constant 1 : index
    %2 = arith.maxui %1, %c1_5 : index
    %c64_6 = arith.constant 64 : index
    %c1_i64 = arith.constant 1 : i64
    %3 = arith.index_cast %c64 : index to i64
    %4 = arith.index_cast %2 : index to i64
    %5 = arith.addi %3, %4 : i64
    %6 = arith.subi %5, %c1_i64 : i64
    %7 = arith.divui %6, %4 : i64
    %8 = arith.index_cast %7 : i64 to index
    %c0_7 = arith.constant 0 : index
    %c1_8 = arith.constant 1 : index
    %c0_9 = arith.constant 0 : index
    %c1_10 = arith.constant 1 : index
    %guid_11, %ptr_12 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%8] elementType(f64) elementSizes[%2, %c64] {arts.id = 4 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson_mixed_orientation.c:42:16", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32768 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 4 : i64, lastUseId = 5 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 2], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 4 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_12 : memref<?xmemref<?x?xf64>>) block_shape[%2] contract(<ownerDims = [0], postDbRefined = true>)
    %c1_13 = arith.constant 1 : index
    %c1_14 = arith.constant 1 : index
    %9 = arith.minui %c8, %c8 : index
    %c1_15 = arith.constant 1 : index
    %10 = arith.maxui %9, %c1_15 : index
    %c1_i64_16 = arith.constant 1 : i64
    %11 = arith.index_cast %c64 : index to i64
    %12 = arith.index_cast %10 : index to i64
    %13 = arith.addi %11, %12 : i64
    %14 = arith.subi %13, %c1_i64_16 : i64
    %15 = arith.divui %14, %12 : i64
    %16 = arith.index_cast %15 : i64 to index
    %c0_17 = arith.constant 0 : index
    %c1_18 = arith.constant 1 : index
    %c0_19 = arith.constant 0 : index
    %c1_20 = arith.constant 1 : index
    %guid_21, %ptr_22 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%16] elementType(f64) elementSizes[%10, %c64] {arts.id = 6 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson_mixed_orientation.c:43:19", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32768 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 6 : i64, lastUseId = 7 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 2], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 6 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?xf64>>) block_shape[%10] contract(<ownerDims = [0], postDbRefined = true>)
    %17 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %19 = arith.muli %arg0, %c8 : index
        %20 = arith.cmpi uge, %19, %c64 : index
        %21 = arith.subi %c64, %19 : index
        %22 = arith.select %20, %c0, %21 : index
        %23 = arith.minui %22, %c8 : index
        %24 = arith.cmpi ugt, %23, %c0 : index
        scf.if %24 {
          %25 = arith.divui %19, %c64 : index
          %26 = arith.addi %19, %c7 : index
          %27 = arith.divui %26, %c64 : index
          %28 = arith.cmpi ugt, %25, %c0 : index
          %29 = arith.select %28, %27, %c0 : index
          %30 = arith.subi %29, %25 : index
          %31 = arith.addi %30, %c1 : index
          %32 = arith.select %28, %c0, %25 : index
          %33 = arith.select %28, %c0, %31 : index
          %guid_23, %ptr_24 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%19], sizes[%c8]), offsets[%32], sizes[%33] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [64, 8], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c64] contract(<ownerDims = [1], postDbRefined = true>)
          %c1_25 = arith.constant 1 : index
          %c0_26 = arith.constant 0 : index
          %34 = arith.maxui %10, %c1_25 : index
          %35 = arith.muli %16, %34 : index
          %36 = arith.divui %19, %34 : index
          %37 = arith.addi %19, %c8 : index
          %38 = arith.subi %37, %c1_25 : index
          %39 = arith.divui %38, %34 : index
          %40 = arith.subi %16, %c1_25 : index
          %41 = arith.cmpi ugt, %36, %40 : index
          %42 = arith.minui %39, %40 : index
          %43 = arith.select %41, %39, %42 : index
          %44 = arith.subi %43, %36 : index
          %45 = arith.addi %44, %c1_25 : index
          %46 = arith.select %41, %c0_26, %36 : index
          %47 = arith.select %41, %c0_26, %45 : index
          %guid_27, %ptr_28 = arts.db_acquire[<inout>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%19], sizes[%c8]), offsets[%46], sizes[%c1_25] element_offsets[%19] element_sizes[%c8] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_28 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c8] contract(<ownerDims = [0]>)
          %c1_29 = arith.constant 1 : index
          %c0_30 = arith.constant 0 : index
          %48 = arith.maxui %2, %c1_29 : index
          %49 = arith.muli %8, %48 : index
          %50 = arith.divui %19, %48 : index
          %51 = arith.addi %19, %c8 : index
          %52 = arith.subi %51, %c1_29 : index
          %53 = arith.divui %52, %48 : index
          %54 = arith.subi %8, %c1_29 : index
          %55 = arith.cmpi ugt, %50, %54 : index
          %56 = arith.minui %53, %54 : index
          %57 = arith.select %55, %53, %56 : index
          %58 = arith.subi %57, %50 : index
          %59 = arith.addi %58, %c1_29 : index
          %60 = arith.select %55, %c0_30, %50 : index
          %61 = arith.select %55, %c0_30, %59 : index
          %guid_31, %ptr_32 = arts.db_acquire[<inout>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%19], sizes[%c8]), offsets[%60], sizes[%c1_29] element_offsets[%19] element_sizes[%c8] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_32 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c8] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_28, %ptr_32) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 77 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 64]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>, %arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>):
            %c1_33 = arith.constant 1 : index
            %c0_34 = arith.constant 0 : index
            %62 = arith.maxui %2, %c1_33 : index
            %c0_35 = arith.constant 0 : index
            %c8_36 = arith.constant 8 : index
            %63 = arith.muli %arg0, %c8_36 : index
            %c2_37 = arith.constant 2 : index
            %64 = arith.addi %c8_36, %c2_37 : index
            %65 = arith.minui %c8_36, %64 : index
            %c1_38 = arith.constant 1 : index
            %66 = arith.maxui %65, %c1_38 : index
            %c1_39 = arith.constant 1 : index
            %67 = arith.maxui %66, %c1_39 : index
            %68 = arith.divui %63, %67 : index
            %69 = arith.select %55, %c0_35, %68 : index
            %c1_40 = arith.constant 1 : index
            %c0_41 = arith.constant 0 : index
            %70 = arith.maxui %10, %c1_40 : index
            %c0_42 = arith.constant 0 : index
            %c8_43 = arith.constant 8 : index
            %71 = arith.muli %arg0, %c8_43 : index
            %72 = arith.minui %c8_43, %c8_43 : index
            %c1_44 = arith.constant 1 : index
            %73 = arith.maxui %72, %c1_44 : index
            %c1_45 = arith.constant 1 : index
            %74 = arith.maxui %73, %c1_45 : index
            %75 = arith.divui %71, %74 : index
            %76 = arith.select %41, %c0_42, %75 : index
            %77 = arith.subi %c0, %19 : index
            %78 = arith.cmpi slt, %77, %c0 : index
            %79 = arith.select %78, %c0, %77 : index
            %80 = arith.cmpi slt, %21, %c0 : index
            %81 = arith.select %80, %c0, %21 : index
            %82 = arith.minui %81, %23 : index
            scf.for %arg4 = %79 to %82 step %c1 {
              %83 = arith.addi %19, %arg4 : index
              %84 = arith.index_cast %83 : index to i32
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %85 = arith.index_cast %arg5 : index to i32
                %86 = arith.addi %85, %84 : i32
                %87 = arith.sitofp %86 : i32 to f64
                %88 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                memref.store %87, %88[%arg5, %83] : memref<?x?xf64>
              } {arts.id = 14 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">, arts.metadata_provenance = "exact"}
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %c1_46 = arith.constant 1 : index
                %85 = arith.maxui %70, %c1_46 : index
                %c0_47 = arith.constant 0 : index
                %c1_48 = arith.constant 1 : index
                %c-1 = arith.constant -1 : index
                %86 = arith.addi %19, %79 : index
                %87 = arith.divui %86, %85 : index
                %88 = arith.subi %arg4, %79 : index
                %89 = arith.addi %79, %88 : index
                %90 = arith.subi %87, %76 : index
                %91 = arts.db_ref %arg2[%c0_41] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %92 = memref.load %91[%arg4, %arg5] : memref<?x?xf64>
                %c1_49 = arith.constant 1 : index
                %93 = arith.maxui %62, %c1_49 : index
                %c0_50 = arith.constant 0 : index
                %c1_51 = arith.constant 1 : index
                %c-1_52 = arith.constant -1 : index
                %94 = arith.addi %19, %79 : index
                %95 = arith.divui %94, %93 : index
                %96 = arith.subi %arg4, %79 : index
                %97 = arith.addi %79, %96 : index
                %98 = arith.subi %95, %69 : index
                %99 = arts.db_ref %arg3[%c0_34] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                memref.store %92, %99[%arg4, %arg5] : memref<?x?xf64>
              } {arts.id = 23 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
            } {arts.id = 75 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
          }
        }
      }
    } : i64
    %18 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %19 = arith.muli %arg0, %c8 : index
        %20 = arith.cmpi uge, %19, %c64 : index
        %21 = arith.subi %c64, %19 : index
        %22 = arith.select %20, %c0, %21 : index
        %23 = arith.minui %22, %c8 : index
        %24 = arith.cmpi ugt, %23, %c0 : index
        scf.if %24 {
          %25 = arith.divui %19, %c64 : index
          %26 = arith.addi %19, %c7 : index
          %27 = arith.divui %26, %c64 : index
          %28 = arith.cmpi ugt, %25, %c0 : index
          %29 = arith.select %28, %27, %c0 : index
          %30 = arith.subi %29, %25 : index
          %31 = arith.addi %30, %c1 : index
          %32 = arith.select %28, %c0, %25 : index
          %33 = arith.select %28, %c0, %31 : index
          %guid_23, %ptr_24 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%19], sizes[%c8]), offsets[%32], sizes[%33] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c8] contract(<ownerDims = [0], postDbRefined = true>)
          %c1_25 = arith.constant 1 : index
          %c0_26 = arith.constant 0 : index
          %34 = arith.maxui %10, %c1_25 : index
          %35 = arith.muli %16, %34 : index
          %36 = arith.divui %19, %34 : index
          %37 = arith.addi %19, %c8 : index
          %38 = arith.subi %37, %c1_25 : index
          %39 = arith.divui %38, %34 : index
          %40 = arith.subi %16, %c1_25 : index
          %41 = arith.cmpi ugt, %36, %40 : index
          %42 = arith.minui %39, %40 : index
          %43 = arith.select %41, %39, %42 : index
          %44 = arith.subi %43, %36 : index
          %45 = arith.addi %44, %c1_25 : index
          %46 = arith.select %41, %c0_26, %36 : index
          %47 = arith.select %41, %c0_26, %45 : index
          %guid_27, %ptr_28 = arts.db_acquire[<inout>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%19], sizes[%c8]), offsets[%46], sizes[%c1_25] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_28 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c8] contract(<ownerDims = [0]>)
          %c1_29 = arith.constant 1 : index
          %48 = arith.cmpi uge, %19, %c1_29 : index
          %49 = arith.subi %19, %c1_29 : index
          %c0_30 = arith.constant 0 : index
          %50 = arith.select %48, %49, %c0_30 : index
          %c2_31 = arith.constant 2 : index
          %51 = arith.addi %c8, %c2_31 : index
          %c1_32 = arith.constant 1 : index
          %c0_33 = arith.constant 0 : index
          %52 = arith.maxui %2, %c1_32 : index
          %53 = arith.muli %8, %52 : index
          %54 = arith.divui %19, %52 : index
          %55 = arith.addi %19, %51 : index
          %56 = arith.subi %55, %c1_32 : index
          %57 = arith.divui %56, %52 : index
          %58 = arith.subi %8, %c1_32 : index
          %59 = arith.cmpi ugt, %54, %58 : index
          %60 = arith.minui %57, %58 : index
          %61 = arith.select %59, %57, %60 : index
          %62 = arith.subi %61, %54 : index
          %63 = arith.addi %62, %c1_32 : index
          %64 = arith.select %59, %c0_33, %54 : index
          %65 = arith.select %59, %c0_33, %63 : index
          %guid_34, %ptr_35 = arts.db_acquire[<inout>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%19], sizes[%c8]), offsets[%64], sizes[%65] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 64], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_35 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c8] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_28, %ptr_35) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 78 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [8, 64]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>, %arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>):
            %c1_36 = arith.constant 1 : index
            %c0_37 = arith.constant 0 : index
            %66 = arith.maxui %2, %c1_36 : index
            %c0_38 = arith.constant 0 : index
            %c8_39 = arith.constant 8 : index
            %67 = arith.muli %arg0, %c8_39 : index
            %c2_40 = arith.constant 2 : index
            %68 = arith.addi %c8_39, %c2_40 : index
            %69 = arith.minui %c8_39, %68 : index
            %c1_41 = arith.constant 1 : index
            %70 = arith.maxui %69, %c1_41 : index
            %c1_42 = arith.constant 1 : index
            %71 = arith.maxui %70, %c1_42 : index
            %72 = arith.divui %67, %71 : index
            %73 = arith.select %59, %c0_38, %72 : index
            %c1_43 = arith.constant 1 : index
            %c0_44 = arith.constant 0 : index
            %74 = arith.maxui %10, %c1_43 : index
            %c0_45 = arith.constant 0 : index
            %c8_46 = arith.constant 8 : index
            %75 = arith.muli %arg0, %c8_46 : index
            %76 = arith.minui %c8_46, %c8_46 : index
            %c1_47 = arith.constant 1 : index
            %77 = arith.maxui %76, %c1_47 : index
            %c1_48 = arith.constant 1 : index
            %78 = arith.maxui %77, %c1_48 : index
            %79 = arith.divui %75, %78 : index
            %80 = arith.select %41, %c0_45, %79 : index
            %81 = arith.subi %c0, %19 : index
            %82 = arith.cmpi slt, %81, %c0 : index
            %83 = arith.select %82, %c0, %81 : index
            %84 = arith.cmpi slt, %21, %c0 : index
            %85 = arith.select %84, %c0, %21 : index
            %86 = arith.minui %85, %23 : index
            scf.for %arg4 = %83 to %86 step %c1 {
              %87 = arith.addi %19, %arg4 : index
              %88 = arith.index_cast %87 : index to i32
              %89 = arith.cmpi eq, %88, %c0_i32 : i32
              %90 = arith.cmpi eq, %88, %c63_i32 : i32
              %91 = arith.addi %88, %c-1_i32 : i32
              %92 = arith.index_cast %91 : i32 to index
              %93 = arith.addi %88, %c1_i32 : i32
              %94 = arith.index_cast %93 : i32 to index
              scf.for %arg5 = %c0 to %c64 step %c1 {
                %95 = arith.index_cast %arg5 : index to i32
                %96 = scf.if %89 -> (i1) {
                  scf.yield %true : i1
                } else {
                  %99 = arith.cmpi eq, %95, %c0_i32 : i32
                  scf.yield %99 : i1
                }
                %97 = arith.select %96, %true, %90 : i1
                %98 = scf.if %97 -> (i1) {
                  scf.yield %true : i1
                } else {
                  %99 = arith.cmpi eq, %95, %c63_i32 : i32
                  scf.yield %99 : i1
                }
                scf.if %98 {
                  %99 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %100 = memref.load %99[%87, %arg5] : memref<?x?xf64>
                  %c1_49 = arith.constant 1 : index
                  %101 = arith.maxui %74, %c1_49 : index
                  %c0_50 = arith.constant 0 : index
                  %c1_51 = arith.constant 1 : index
                  %c-1 = arith.constant -1 : index
                  %102 = arith.addi %19, %83 : index
                  %103 = arith.divui %102, %101 : index
                  %104 = arith.subi %arg4, %83 : index
                  %105 = arith.addi %83, %104 : index
                  %106 = arith.subi %103, %80 : index
                  %107 = arts.db_ref %arg2[%c0_44] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %100, %107[%arg4, %arg5] : memref<?x?xf64>
                } else {
                  %c1_49 = arith.constant 1 : index
                  %99 = arith.maxui %66, %c1_49 : index
                  %c0_50 = arith.constant 0 : index
                  %c1_51 = arith.constant 1 : index
                  %c-1 = arith.constant -1 : index
                  %100 = arith.addi %19, %83 : index
                  %101 = arith.divui %100, %99 : index
                  %102 = arith.subi %arg4, %83 : index
                  %103 = arith.addi %83, %102 : index
                  %c1_52 = arith.constant 1 : index
                  %104 = arith.subi %103, %c1_52 : index
                  %105 = arith.subi %101, %73 : index
                  %106 = arith.cmpi slt, %104, %c0_50 : index
                  %107 = arith.cmpi sge, %104, %99 : index
                  %108 = arith.select %106, %c-1, %c0_50 : index
                  %109 = arith.select %107, %c1_51, %108 : index
                  %110 = arith.addi %105, %109 : index
                  %111 = arith.subi %104, %99 : index
                  %112 = arith.addi %104, %99 : index
                  %113 = arith.select %106, %112, %104 : index
                  %114 = arith.select %107, %111, %113 : index
                  %115 = arts.db_ref %arg3[%110] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %116 = memref.load %115[%114, %arg5] : memref<?x?xf64>
                  %117 = arith.addi %95, %c1_i32 : i32
                  %118 = arith.index_cast %117 : i32 to index
                  %c1_53 = arith.constant 1 : index
                  %119 = arith.maxui %66, %c1_53 : index
                  %c0_54 = arith.constant 0 : index
                  %c1_55 = arith.constant 1 : index
                  %c-1_56 = arith.constant -1 : index
                  %120 = arith.addi %19, %83 : index
                  %121 = arith.divui %120, %119 : index
                  %122 = arith.subi %arg4, %83 : index
                  %123 = arith.addi %83, %122 : index
                  %124 = arith.subi %121, %73 : index
                  %125 = arts.db_ref %arg3[%124] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %126 = memref.load %125[%arg4, %118] : memref<?x?xf64>
                  %127 = arith.addf %116, %126 : f64
                  %128 = arith.addi %95, %c-1_i32 : i32
                  %129 = arith.index_cast %128 : i32 to index
                  %c1_57 = arith.constant 1 : index
                  %130 = arith.maxui %66, %c1_57 : index
                  %c0_58 = arith.constant 0 : index
                  %c1_59 = arith.constant 1 : index
                  %c-1_60 = arith.constant -1 : index
                  %131 = arith.addi %19, %83 : index
                  %132 = arith.divui %131, %130 : index
                  %133 = arith.subi %arg4, %83 : index
                  %134 = arith.addi %83, %133 : index
                  %135 = arith.subi %132, %73 : index
                  %136 = arts.db_ref %arg3[%135] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %137 = memref.load %136[%arg4, %129] : memref<?x?xf64>
                  %138 = arith.addf %127, %137 : f64
                  %c1_61 = arith.constant 1 : index
                  %139 = arith.maxui %66, %c1_61 : index
                  %c0_62 = arith.constant 0 : index
                  %c1_63 = arith.constant 1 : index
                  %c-1_64 = arith.constant -1 : index
                  %140 = arith.addi %19, %83 : index
                  %141 = arith.divui %140, %139 : index
                  %142 = arith.subi %arg4, %83 : index
                  %143 = arith.addi %83, %142 : index
                  %c1_65 = arith.constant 1 : index
                  %144 = arith.addi %143, %c1_65 : index
                  %145 = arith.subi %141, %73 : index
                  %146 = arith.cmpi slt, %144, %c0_62 : index
                  %147 = arith.cmpi sge, %144, %139 : index
                  %148 = arith.select %146, %c-1_64, %c0_62 : index
                  %149 = arith.select %147, %c1_63, %148 : index
                  %150 = arith.addi %145, %149 : index
                  %151 = arith.subi %144, %139 : index
                  %152 = arith.addi %144, %139 : index
                  %153 = arith.select %146, %152, %144 : index
                  %154 = arith.select %147, %151, %153 : index
                  %155 = arts.db_ref %arg3[%150] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %156 = memref.load %155[%154, %arg5] : memref<?x?xf64>
                  %157 = arith.addf %138, %156 : f64
                  %158 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  %159 = memref.load %158[%87, %arg5] : memref<?x?xf64>
                  %160 = arith.addf %157, %159 : f64
                  %161 = arith.mulf %160, %cst : f64
                  %c1_66 = arith.constant 1 : index
                  %162 = arith.maxui %74, %c1_66 : index
                  %c0_67 = arith.constant 0 : index
                  %c1_68 = arith.constant 1 : index
                  %c-1_69 = arith.constant -1 : index
                  %163 = arith.addi %19, %83 : index
                  %164 = arith.divui %163, %162 : index
                  %165 = arith.subi %arg4, %83 : index
                  %166 = arith.addi %83, %165 : index
                  %167 = arith.subi %164, %80 : index
                  %168 = arts.db_ref %arg2[%c0_44] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %161, %168[%arg4, %arg5] : memref<?x?xf64>
                }
              } {arts.id = 67 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
            } {arts.id = 76 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
          }
        }
      }
    } : i64
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?x?xf64>>
    return %c0_i32 : i32
  }
}
