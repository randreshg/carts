module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("poisson-for\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c12 = arith.constant 12 : index
    %cst = arith.constant 2.500000e-01 : f64
    %cst_0 = arith.constant 0.010101010101010102 : f64
    %c1_i32 = arith.constant 1 : i32
    %c13 = arith.constant 13 : index
    %cst_1 = arith.constant -9.869604401089358 : f64
    %cst_2 = arith.constant 3.1415926535897931 : f64
    %cst_3 = arith.constant 9.900000e+01 : f64
    %c8 = arith.constant 8 : index
    %c11 = arith.constant 11 : index
    %c99_i32 = arith.constant 99 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %c100_i32 = arith.constant 100 : i32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %alloca = memref.alloca() {arts.id = 197 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:140:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 197 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %c1_5 = arith.constant 1 : index
    %c1_6 = arith.constant 1 : index
    %c1_7 = arith.constant 1 : index
    %7 = arith.maxui %c13, %c1_7 : index
    %c1_i64 = arith.constant 1 : i64
    %8 = arith.index_cast %c100 : index to i64
    %9 = arith.index_cast %7 : index to i64
    %10 = arith.addi %8, %9 : i64
    %11 = arith.subi %10, %c1_i64 : i64
    %12 = arith.divui %11, %9 : i64
    %13 = arith.index_cast %12 : i64 to index
    %c0_8 = arith.constant 0 : index
    %c1_9 = arith.constant 1 : index
    %c0_10 = arith.constant 0 : index
    %c1_11 = arith.constant 1 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%13] elementType(f64) elementSizes[%c100, %7] {arts.id = 51 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson-for.c:94:16", totalAccesses = 6 : i64, readCount = 3 : i64, writeCount = 3 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 80000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 51 : i64, lastUseId = 52 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 4], estimatedAccessBytes = 48 : i64>, arts.metadata_origin_id = 51 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%7] contract(<ownerDims = [1], postDbRefined = true>)
    %c0_12 = arith.constant 0 : index
    %c0_13 = arith.constant 0 : index
    %c1_14 = arith.constant 1 : index
    %c1_15 = arith.constant 1 : index
    %c1_16 = arith.constant 1 : index
    %14 = arts.runtime_query <total_workers> -> i32
    %15 = arith.index_castui %14 : i32 to index
    %16 = arith.maxui %15, %c1_16 : index
    %17 = arith.subi %16, %c1_16 : index
    %18 = arith.addi %c100, %17 : index
    %19 = arith.divui %18, %16 : index
    %20 = arith.maxui %19, %c1_16 : index
    %c1_17 = arith.constant 1 : index
    %21 = arith.maxui %20, %c1_17 : index
    %c100_18 = arith.constant 100 : index
    %c2 = arith.constant 2 : index
    %c0_19 = arith.constant 0 : index
    %22 = arith.cmpi uge, %c100_18, %c2 : index
    %23 = arith.subi %c100_18, %c2 : index
    %24 = arith.select %22, %23, %c0_19 : index
    %c1_i64_20 = arith.constant 1 : i64
    %25 = arith.index_cast %c100 : index to i64
    %26 = arith.index_cast %21 : index to i64
    %27 = arith.addi %25, %26 : i64
    %28 = arith.subi %27, %c1_i64_20 : i64
    %29 = arith.divui %28, %26 : i64
    %30 = arith.index_cast %29 : i64 to index
    %c0_21 = arith.constant 0 : index
    %c1_22 = arith.constant 1 : index
    %c0_23 = arith.constant 0 : index
    %c1_24 = arith.constant 1 : index
    %guid_25, %ptr_26 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <stencil>] route(%c-1_i32 : i32) sizes[%30] elementType(f64) elementSizes[%21, %c100] {arts.id = 53 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson-for.c:95:16", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 80000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 53 : i64, lastUseId = 54 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 2], estimatedAccessBytes = 48 : i64>, arts.metadata_origin_id = 53 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_26 : memref<?xmemref<?x?xf64>>) block_shape[%21] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_27 = arith.constant 0 : index
    %c1_28 = arith.constant 1 : index
    %c1_29 = arith.constant 1 : index
    %c1_30 = arith.constant 1 : index
    %31 = arts.runtime_query <total_workers> -> i32
    %32 = arith.index_castui %31 : i32 to index
    %33 = arith.maxui %32, %c1_30 : index
    %34 = arith.subi %33, %c1_30 : index
    %35 = arith.addi %c100, %34 : index
    %36 = arith.divui %35, %33 : index
    %37 = arith.maxui %36, %c1_30 : index
    %c1_31 = arith.constant 1 : index
    %38 = arith.maxui %37, %c1_31 : index
    %c100_32 = arith.constant 100 : index
    %c1_i64_33 = arith.constant 1 : i64
    %39 = arith.index_cast %c100 : index to i64
    %40 = arith.index_cast %38 : index to i64
    %41 = arith.addi %39, %40 : i64
    %42 = arith.subi %41, %c1_i64_33 : i64
    %43 = arith.divui %42, %40 : i64
    %44 = arith.index_cast %43 : i64 to index
    %c0_34 = arith.constant 0 : index
    %c1_35 = arith.constant 1 : index
    %c0_36 = arith.constant 0 : index
    %c1_37 = arith.constant 1 : index
    %guid_38, %ptr_39 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%44] elementType(f64) elementSizes[%38, %c100] {arts.id = 55 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson-for.c:96:19", totalAccesses = 7 : i64, readCount = 2 : i64, writeCount = 5 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 80000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 55 : i64, lastUseId = 56 : i64, hasUniformAccess = false, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 55 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr_39 : memref<?xmemref<?x?xf64>>) block_shape[%38] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_40 = arith.constant 0 : index
    %c0_41 = arith.constant 0 : index
    %c0_42 = arith.constant 0 : index
    %c0_43 = arith.constant 0 : index
    scf.for %arg0 = %c0 to %c100 step %c1 {
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %c1_51 = arith.constant 1 : index
        %50 = arith.maxui %7, %c1_51 : index
        %51 = arith.divui %arg1, %50 : index
        %52 = arith.subi %51, %c0_12 : index
        %53 = arith.remui %arg1, %50 : index
        %54 = arts.db_ref %ptr[%52] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %cst_4, %54[%arg0, %53] : memref<?x?xf64>
        %c1_52 = arith.constant 1 : index
        %55 = arith.maxui %21, %c1_52 : index
        %56 = arith.divui %arg0, %55 : index
        %57 = arith.subi %56, %c0_27 : index
        %58 = arith.remui %arg0, %55 : index
        %59 = arts.db_ref %ptr_26[%57] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %cst_4, %59[%58, %arg1] : memref<?x?xf64>
        %c1_53 = arith.constant 1 : index
        %60 = arith.maxui %38, %c1_53 : index
        %61 = arith.divui %arg0, %60 : index
        %62 = arith.subi %61, %c0_40 : index
        %63 = arith.remui %arg0, %60 : index
        %64 = arts.db_ref %ptr_39[%62] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %cst_4, %64[%63, %arg1] : memref<?x?xf64>
      } {arts.id = 58 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:106:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 46 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:105:3">, arts.metadata_provenance = "recovered"}
    %alloca_44 = memref.alloca() {arts.id = 64 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:114:3", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 64 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_45 = memref.alloca() {arts.id = 62 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:114:3", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 62 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %45 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %50 = arith.muli %arg0, %c13 : index
        %51 = arith.cmpi uge, %50, %c100 : index
        %52 = arith.subi %c100, %50 : index
        %53 = arith.select %51, %c0, %52 : index
        %54 = arith.minui %53, %c13 : index
        %55 = arith.cmpi ugt, %54, %c0 : index
        scf.if %55 {
          %56 = arith.divui %50, %c100 : index
          %57 = arith.addi %50, %c12 : index
          %58 = arith.divui %57, %c100 : index
          %59 = arith.cmpi ugt, %56, %c0 : index
          %60 = arith.select %59, %58, %c0 : index
          %61 = arith.subi %60, %56 : index
          %62 = arith.addi %61, %c1 : index
          %63 = arith.select %59, %c0, %56 : index
          %64 = arith.select %59, %c0, %62 : index
          %c1_51 = arith.constant 1 : index
          %c0_52 = arith.constant 0 : index
          %65 = arith.maxui %7, %c1_51 : index
          %66 = arith.muli %13, %65 : index
          %67 = arith.divui %50, %65 : index
          %68 = arith.addi %50, %c13 : index
          %69 = arith.subi %68, %c1_51 : index
          %70 = arith.divui %69, %65 : index
          %71 = arith.subi %13, %c1_51 : index
          %72 = arith.cmpi ugt, %67, %71 : index
          %73 = arith.minui %70, %71 : index
          %74 = arith.select %72, %70, %73 : index
          %75 = arith.subi %74, %67 : index
          %76 = arith.addi %75, %c1_51 : index
          %77 = arith.select %72, %c0_52, %67 : index
          %78 = arith.select %72, %c0_52, %76 : index
          %guid_53, %ptr_54 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%50], sizes[%c13]), offsets[%77], sizes[%c1_51] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [100, 13], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_54 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c100] contract(<ownerDims = [1]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_54) : memref<?xmemref<?x?xf64>> attributes {arts.id = 215 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [100, 13]} {
          ^bb0(%arg1: memref<?xmemref<?x?xf64>>):
            %c1_55 = arith.constant 1 : index
            %c0_56 = arith.constant 0 : index
            %79 = arith.maxui %7, %c1_55 : index
            %c0_57 = arith.constant 0 : index
            %c13_58 = arith.constant 13 : index
            %80 = arith.muli %arg0, %c13_58 : index
            %c1_59 = arith.constant 1 : index
            %81 = arith.maxui %c13_58, %c1_59 : index
            %c1_60 = arith.constant 1 : index
            %82 = arith.maxui %81, %c1_60 : index
            %83 = arith.divui %80, %82 : index
            %84 = arith.select %72, %c0_57, %83 : index
            memref.store %4, %alloca_44[] : memref<f64>
            memref.store %4, %alloca_45[] : memref<f64>
            %85 = arith.subi %c0, %50 : index
            %86 = arith.cmpi slt, %85, %c0 : index
            %87 = arith.select %86, %c0, %85 : index
            %88 = arith.cmpi slt, %52, %c0 : index
            %89 = arith.select %88, %c0, %52 : index
            %90 = arith.minui %89, %54 : index
            scf.for %arg2 = %87 to %90 step %c1 {
              %91 = arith.addi %50, %arg2 : index
              %92 = arith.index_cast %91 : index to i32
              %93 = arith.sitofp %92 : i32 to f64
              %94 = arith.divf %93, %cst_3 : f64
              memref.store %94, %alloca_45[] : memref<f64>
              %95 = arith.cmpi eq, %92, %c0_i32 : i32
              %96 = arith.cmpi eq, %92, %c99_i32 : i32
              scf.for %arg3 = %c0 to %c100 step %c1 {
                %97 = arith.index_cast %arg3 : index to i32
                %98 = arith.sitofp %97 : i32 to f64
                %99 = arith.divf %98, %cst_3 : f64
                memref.store %99, %alloca_44[] : memref<f64>
                %100 = arith.cmpi eq, %97, %c0_i32 : i32
                %101 = scf.if %100 -> (i1) {
                  scf.yield %true : i1
                } else {
                  %104 = arith.cmpi eq, %97, %c99_i32 : i32
                  scf.yield %104 : i1
                }
                %102 = arith.select %101, %true, %95 : i1
                %103 = arith.select %102, %true, %96 : i1
                scf.if %103 {
                  %104 = memref.load %alloca_44[] : memref<f64>
                  %105 = memref.load %alloca_45[] : memref<f64>
                  %106 = arith.mulf %104, %cst_2 : f64
                  %107 = arith.mulf %106, %105 : f64
                  %108 = math.sin %107 : f64
                  %c1_61 = arith.constant 1 : index
                  %109 = arith.maxui %79, %c1_61 : index
                  %c0_62 = arith.constant 0 : index
                  %c1_63 = arith.constant 1 : index
                  %c-1 = arith.constant -1 : index
                  %110 = arith.addi %50, %87 : index
                  %111 = arith.divui %110, %109 : index
                  %112 = arith.subi %arg2, %87 : index
                  %113 = arith.addi %87, %112 : index
                  %114 = arith.subi %111, %84 : index
                  %115 = arts.db_ref %arg1[%c0_56] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %108, %115[%arg3, %arg2] : memref<?x?xf64>
                } else {
                  %104 = memref.load %alloca_44[] : memref<f64>
                  %105 = memref.load %alloca_45[] : memref<f64>
                  %106 = arith.mulf %104, %104 : f64
                  %107 = arith.mulf %105, %105 : f64
                  %108 = arith.addf %106, %107 : f64
                  %109 = arith.mulf %108, %cst_1 : f64
                  %110 = arith.mulf %104, %cst_2 : f64
                  %111 = arith.mulf %110, %105 : f64
                  %112 = math.sin %111 : f64
                  %113 = arith.mulf %109, %112 : f64
                  %114 = arith.negf %113 : f64
                  %c1_61 = arith.constant 1 : index
                  %115 = arith.maxui %79, %c1_61 : index
                  %c0_62 = arith.constant 0 : index
                  %c1_63 = arith.constant 1 : index
                  %c-1 = arith.constant -1 : index
                  %116 = arith.addi %50, %87 : index
                  %117 = arith.divui %116, %115 : index
                  %118 = arith.subi %arg2, %87 : index
                  %119 = arith.addi %87, %118 : index
                  %120 = arith.subi %117, %84 : index
                  %121 = arts.db_ref %arg1[%c0_56] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                  memref.store %114, %121[%arg3, %arg2] : memref<?x?xf64>
                }
              } {arts.id = 94 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "poisson-for.c:114:3">, arts.metadata_provenance = "exact"}
            } {arts.id = 213 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:114:3">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg1) : memref<?xmemref<?x?xf64>>
          }
        }
      }
    } : i64
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %50 = arith.index_cast %arg0 : index to i32
      %51 = arith.cmpi eq, %arg0, %c0 : index
      %52 = scf.if %51 -> (i1) {
        scf.yield %true : i1
      } else {
        %53 = arith.cmpi eq, %50, %c99_i32 : i32
        scf.yield %53 : i1
      }
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %53 = arith.index_cast %arg1 : index to i32
        %54 = scf.if %52 -> (i1) {
          scf.yield %true : i1
        } else {
          %56 = arith.cmpi eq, %53, %c0_i32 : i32
          scf.yield %56 : i1
        }
        %55 = scf.if %54 -> (i1) {
          scf.yield %true : i1
        } else {
          %56 = arith.cmpi eq, %53, %c99_i32 : i32
          scf.yield %56 : i1
        }
        scf.if %55 {
          %c1_51 = arith.constant 1 : index
          %56 = arith.maxui %7, %c1_51 : index
          %57 = arith.divui %arg1, %56 : index
          %58 = arith.subi %57, %c0_13 : index
          %59 = arith.remui %arg1, %56 : index
          %60 = arts.db_ref %ptr[%58] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          %61 = memref.load %60[%arg0, %59] : memref<?x?xf64>
          %c1_52 = arith.constant 1 : index
          %62 = arith.maxui %38, %c1_52 : index
          %63 = arith.divui %arg0, %62 : index
          %64 = arith.subi %63, %c0_41 : index
          %65 = arith.remui %arg0, %62 : index
          %66 = arts.db_ref %ptr_39[%64] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          memref.store %61, %66[%65, %arg1] : memref<?x?xf64>
        } else {
          %c1_51 = arith.constant 1 : index
          %56 = arith.maxui %38, %c1_51 : index
          %57 = arith.divui %arg0, %56 : index
          %58 = arith.subi %57, %c0_42 : index
          %59 = arith.remui %arg0, %56 : index
          %60 = arts.db_ref %ptr_39[%58] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          memref.store %cst_4, %60[%59, %arg1] : memref<?x?xf64>
        }
      } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:119:5">, arts.metadata_provenance = "recovered"}
    } {arts.id = 91 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:118:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_46 = memref.alloca() {arts.id = 120 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:132:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 120 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    %alloca_47 = memref.alloca() {arts.id = 121 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:132:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 121 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    %alloca_48 = memref.alloca() {arts.id = 122 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:132:5", totalAccesses = 6 : i64, readCount = 1 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, firstUseId = 122 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true>, arts.metadata_provenance = "exact"} : memref<i1>
    %46 = "polygeist.undef"() : () -> i32
    memref.store %46, %alloca_46[] : memref<i32>
    memref.store %46, %alloca_47[] : memref<i32>
    memref.store %true, %alloca_48[] : memref<i1>
    %alloca_49 = memref.alloca() {arts.id = 121 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:132:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 121 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    %alloca_50 = memref.alloca() {arts.id = 120 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:132:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 120 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    scf.for %arg0 = %c1 to %c11 step %c1 {
      %50 = memref.load %alloca_48[] : memref<i1>
      scf.if %50 {
        memref.store %c100_i32, %alloca_47[] : memref<i32>
        %51 = memref.load %alloca_47[] : memref<i32>
        %52 = arith.index_cast %51 : i32 to index
        %53 = arith.addi %52, %c12 : index
        %54 = arith.divui %53, %c13 : index
        %55 = arith.minui %54, %c8 : index
        %56 = arith.maxui %55, %c1 : index
        %57 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "cps_chain", arts.plan.cost.expected_local_work = 5000000 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "full_timestep", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
          scf.for %arg1 = %c0 to %56 step %c1 {
            %65 = arith.divui %54, %c8 : index
            %66 = arith.muli %arg1, %65 : index
            %67 = arith.muli %66, %c13 : index
            %68 = arith.muli %65, %c13 : index
            %69 = arith.cmpi uge, %67, %52 : index
            %70 = arith.subi %52, %67 : index
            %71 = arith.select %69, %c0, %70 : index
            %72 = arith.minui %68, %71 : index
            %73 = arith.cmpi ugt, %72, %c0 : index
            scf.if %73 {
              %74 = arith.addi %67, %72 : index
              %75 = arith.cmpi uge, %67, %c1 : index
              %76 = arith.subi %67, %c1 : index
              %77 = arith.select %75, %76, %c0 : index
              %78 = arith.addi %74, %c1 : index
              %79 = arith.minui %78, %c100 : index
              %80 = arith.cmpi uge, %79, %77 : index
              %81 = arith.subi %79, %77 : index
              %82 = arith.select %80, %81, %c0 : index
              %83 = arith.maxui %82, %c1 : index
              %84 = arith.divui %77, %c100 : index
              %85 = arith.addi %77, %83 : index
              %86 = arith.subi %85, %c1 : index
              %87 = arith.divui %86, %c100 : index
              %88 = arith.cmpi ugt, %84, %c0 : index
              %89 = arith.select %88, %87, %c0 : index
              %90 = arith.subi %89, %84 : index
              %91 = arith.addi %90, %c1 : index
              %92 = arith.select %88, %c0, %84 : index
              %93 = arith.select %88, %c0, %91 : index
              %c1_51 = arith.constant 1 : index
              %c0_52 = arith.constant 0 : index
              %94 = arith.maxui %38, %c1_51 : index
              %95 = arith.muli %44, %94 : index
              %96 = arith.divui %77, %94 : index
              %97 = arith.addi %77, %82 : index
              %98 = arith.subi %97, %c1_51 : index
              %99 = arith.divui %98, %94 : index
              %100 = arith.subi %44, %c1_51 : index
              %101 = arith.cmpi ugt, %96, %100 : index
              %102 = arith.minui %99, %100 : index
              %103 = arith.select %101, %99, %102 : index
              %104 = arith.subi %103, %96 : index
              %105 = arith.addi %104, %c1_51 : index
              %106 = arith.select %101, %c0_52, %96 : index
              %107 = arith.select %101, %c0_52, %105 : index
              %guid_53, %ptr_54 = arts.db_acquire[<in>] (%guid_38 : memref<?xi64>, %ptr_39 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%77], sizes[%82]), offsets[%106], sizes[%107] element_offsets[%77] element_sizes[%82] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [13, 100], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_54 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c13] contract(<ownerDims = [0]>)
              %108 = arith.maxui %68, %c1 : index
              %109 = arith.divui %67, %c100 : index
              %110 = arith.addi %67, %108 : index
              %111 = arith.subi %110, %c1 : index
              %112 = arith.divui %111, %c100 : index
              %113 = arith.cmpi ugt, %109, %c0 : index
              %114 = arith.select %113, %112, %c0 : index
              %115 = arith.subi %114, %109 : index
              %116 = arith.addi %115, %c1 : index
              %117 = arith.select %113, %c0, %109 : index
              %118 = arith.select %113, %c0, %116 : index
              %c0_55 = arith.constant 0 : index
              %c1_56 = arith.constant 1 : index
              %119 = arith.maxui %21, %c1_56 : index
              %c13_57 = arith.constant 13 : index
              %120 = arith.maxui %c13_57, %c1_56 : index
              %c1_58 = arith.constant 1 : index
              %121 = arith.addi %67, %c1_58 : index
              %122 = arith.divui %121, %119 : index
              %123 = arith.muli %122, %119 : index
              %c0_59 = arith.constant 0 : index
              %c1_60 = arith.constant 1 : index
              %124 = arith.maxui %21, %c1_60 : index
              %125 = arith.muli %30, %124 : index
              %126 = arith.maxui %68, %c1_60 : index
              %127 = arith.divui %67, %124 : index
              %128 = arith.addi %67, %126 : index
              %129 = arith.subi %128, %c1_60 : index
              %130 = arith.divui %129, %124 : index
              %131 = arith.subi %30, %c1_60 : index
              %132 = arith.cmpi ugt, %127, %131 : index
              %133 = arith.minui %130, %131 : index
              %134 = arith.select %132, %130, %133 : index
              %135 = arith.subi %134, %127 : index
              %136 = arith.addi %135, %c1_60 : index
              %137 = arith.select %132, %c0_59, %127 : index
              %138 = arith.select %132, %c0_59, %136 : index
              %guid_61, %ptr_62 = arts.db_acquire[<out>] (%guid_25 : memref<?xi64>, %ptr_26 : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%137], sizes[%138] element_offsets[%67] element_sizes[%68] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [13, 100], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_62 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c13] contract(<ownerDims = [0]>)
              arts.edt <task> <intranode> route(%c-1_i32) (%ptr_54, %ptr_62) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 216 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "cps_chain", arts.plan.cost.expected_local_work = 5000000 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "full_timestep", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [13, 100]} {
              ^bb0(%arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>):
                %c1_63 = arith.constant 1 : index
                %c0_64 = arith.constant 0 : index
                %139 = arith.maxui %38, %c1_63 : index
                %c0_65 = arith.constant 0 : index
                %140 = arith.index_cast %51 : i32 to index
                %c12_66 = arith.constant 12 : index
                %141 = arith.addi %140, %c12_66 : index
                %c13_67 = arith.constant 13 : index
                %142 = arith.divui %141, %c13_67 : index
                %c8_68 = arith.constant 8 : index
                %143 = arith.divui %142, %c8_68 : index
                %144 = arith.muli %arg1, %143 : index
                %145 = arith.muli %144, %c13_67 : index
                %c1_69 = arith.constant 1 : index
                %146 = arith.subi %145, %c1_69 : index
                %c0_70 = arith.constant 0 : index
                %147 = arith.select %75, %146, %c0_70 : index
                %c100_71 = arith.constant 100 : index
                %c1_72 = arith.constant 1 : index
                %148 = arith.maxui %32, %c1_72 : index
                %149 = arith.subi %148, %c1_72 : index
                %150 = arith.addi %c100_71, %149 : index
                %151 = arith.divui %150, %148 : index
                %152 = arith.maxui %151, %c1_72 : index
                %c1_73 = arith.constant 1 : index
                %153 = arith.maxui %152, %c1_73 : index
                %c1_74 = arith.constant 1 : index
                %154 = arith.maxui %153, %c1_74 : index
                %155 = arith.divui %147, %154 : index
                %156 = arith.select %101, %c0_65, %155 : index
                %c1_75 = arith.constant 1 : index
                %c0_76 = arith.constant 0 : index
                %157 = arith.maxui %21, %c1_75 : index
                %158 = arith.select %132, %c0_59, %127 : index
                memref.store %46, %alloca_49[] : memref<i32>
                memref.store %c100_i32, %alloca_49[] : memref<i32>
                %159 = memref.load %alloca_49[] : memref<i32>
                %160 = arith.index_cast %159 : i32 to index
                %161 = arith.addi %160, %c12 : index
                %162 = arith.divui %161, %c13 : index
                %163 = arith.divui %162, %c8 : index
                %164 = arith.muli %arg1, %163 : index
                %165 = arith.muli %164, %c13 : index
                %166 = arith.subi %c0, %165 : index
                %167 = arith.subi %160, %165 : index
                %168 = arith.cmpi slt, %166, %c0 : index
                %169 = arith.select %168, %c0, %166 : index
                %170 = arith.cmpi slt, %167, %c0 : index
                %171 = arith.select %170, %c0, %167 : index
                %172 = arith.minui %171, %72 : index
                scf.for %arg4 = %169 to %172 step %c1 {
                  %173 = arith.addi %165, %arg4 : index
                  scf.for %arg5 = %c0 to %c100 step %c1 {
                    %c1_77 = arith.constant 1 : index
                    %174 = arith.maxui %139, %c1_77 : index
                    %175 = arith.divui %173, %174 : index
                    %176 = arith.subi %175, %156 : index
                    %177 = arith.remui %173, %174 : index
                    %178 = arts.db_ref %arg2[%176] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    %179 = memref.load %178[%177, %arg5] : memref<?x?xf64>
                    %c1_78 = arith.constant 1 : index
                    %180 = arith.maxui %157, %c1_78 : index
                    %181 = arith.divui %173, %180 : index
                    %182 = arith.subi %181, %158 : index
                    %183 = arith.remui %173, %180 : index
                    %184 = arts.db_ref %arg3[%182] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                    memref.store %179, %184[%183, %arg5] : memref<?x?xf64>
                  } {arts.id = 135 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
                } {arts.id = 94 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
                arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
              }
            }
          } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "recovered"}
        } : i64
        memref.store %true, %alloca_48[] : memref<i1>
        memref.store %c100_i32, %alloca_46[] : memref<i32>
        %58 = memref.load %alloca_46[] : memref<i32>
        %59 = arith.index_cast %58 : i32 to index
        %60 = arith.addi %59, %c12 : index
        %61 = arith.divui %60, %c13 : index
        %62 = arith.minui %61, %c8 : index
        %63 = arith.maxui %62, %c1 : index
        %64 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "cps_chain", arts.plan.cost.expected_local_work = 30087500 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.repetition_structure = "full_timestep", depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
          scf.for %arg1 = %c0 to %63 step %c1 {
            %65 = arith.divui %61, %c8 : index
            %66 = arith.muli %arg1, %65 : index
            %67 = arith.muli %66, %c13 : index
            %68 = arith.muli %65, %c13 : index
            %69 = arith.cmpi uge, %67, %59 : index
            %70 = arith.subi %59, %67 : index
            %71 = arith.select %69, %c0, %70 : index
            %72 = arith.minui %68, %71 : index
            %73 = arith.cmpi ugt, %72, %c0 : index
            scf.if %73 {
              %74 = arith.addi %67, %72 : index
              %75 = arith.cmpi uge, %67, %c1 : index
              %76 = arith.subi %67, %c1 : index
              %77 = arith.select %75, %76, %c0 : index
              %78 = arith.addi %74, %c1 : index
              %79 = arith.minui %78, %c100 : index
              %80 = arith.cmpi uge, %79, %77 : index
              %81 = arith.subi %79, %77 : index
              %82 = arith.select %80, %81, %c0 : index
              %83 = arith.maxui %82, %c1 : index
              %84 = arith.divui %77, %c100 : index
              %85 = arith.addi %77, %83 : index
              %86 = arith.subi %85, %c1 : index
              %87 = arith.divui %86, %c100 : index
              %88 = arith.cmpi ugt, %84, %c0 : index
              %89 = arith.select %88, %87, %c0 : index
              %90 = arith.subi %89, %84 : index
              %91 = arith.addi %90, %c1 : index
              %92 = arith.select %88, %c0, %84 : index
              %93 = arith.select %88, %c0, %91 : index
              %c1_51 = arith.constant 1 : index
              %c0_52 = arith.constant 0 : index
              %guid_53, %ptr_54 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%77], sizes[%82]), offsets[%c0_52], sizes[%13] element_offsets[%77] element_sizes[%82] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [13, 100], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_54 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <stencil>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c13] contract(<ownerDims = [0]>)
              %94 = arith.maxui %68, %c1 : index
              %95 = arith.divui %67, %c100 : index
              %96 = arith.addi %67, %94 : index
              %97 = arith.subi %96, %c1 : index
              %98 = arith.divui %97, %c100 : index
              %99 = arith.cmpi ugt, %95, %c0 : index
              %100 = arith.select %99, %98, %c0 : index
              %101 = arith.subi %100, %95 : index
              %102 = arith.addi %101, %c1 : index
              %103 = arith.select %99, %c0, %95 : index
              %104 = arith.select %99, %c0, %102 : index
              %c1_55 = arith.constant 1 : index
              %c0_56 = arith.constant 0 : index
              %guid_57, %ptr_58 = arts.db_acquire[<out>] (%guid_38 : memref<?xi64>, %ptr_39 : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%67], sizes[%68]), offsets[%c0_56], sizes[%44] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [13, 100], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_58 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <stencil>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c13] contract(<ownerDims = [0]>)
              %c0_59 = arith.constant 0 : index
              %c1_60 = arith.constant 1 : index
              %105 = arith.maxui %21, %c1_60 : index
              %c13_61 = arith.constant 13 : index
              %106 = arith.maxui %c13_61, %c1_60 : index
              %c1_62 = arith.constant 1 : index
              %107 = arith.addi %77, %c1_62 : index
              %108 = arith.divui %107, %105 : index
              %109 = arith.muli %108, %105 : index
              %c0_63 = arith.constant 0 : index
              %c1_64 = arith.constant 1 : index
              %110 = arith.maxui %21, %c1_64 : index
              %111 = arith.muli %30, %110 : index
              %112 = arith.maxui %82, %c1_64 : index
              %113 = arith.divui %77, %110 : index
              %114 = arith.addi %77, %112 : index
              %115 = arith.subi %114, %c1_64 : index
              %116 = arith.divui %115, %110 : index
              %117 = arith.subi %30, %c1_64 : index
              %118 = arith.cmpi ugt, %113, %117 : index
              %119 = arith.minui %116, %117 : index
              %120 = arith.select %118, %116, %119 : index
              %121 = arith.subi %120, %113 : index
              %122 = arith.addi %121, %c1_64 : index
              %123 = arith.select %118, %c0_63, %113 : index
              %124 = arith.select %118, %c0_63, %122 : index
              %guid_65, %ptr_66 = arts.db_acquire[<in>] (%guid_25 : memref<?xi64>, %ptr_26 : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%123], sizes[%124] element_offsets[%77] element_sizes[%82] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [13, 100], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
              arts.lowering_contract(%ptr_66 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <stencil>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c13] contract(<ownerDims = [0]>)
              arts.edt <task> <intranode> route(%c-1_i32) (%ptr_54, %ptr_58, %ptr_66) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {arts.id = 217 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "cps_chain", arts.plan.cost.expected_local_work = 30087500 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.repetition_structure = "full_timestep", depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [13, 100]} {
              ^bb0(%arg2: memref<?xmemref<?x?xf64>>, %arg3: memref<?xmemref<?x?xf64>>, %arg4: memref<?xmemref<?x?xf64>>):
                %c1_67 = arith.constant 1 : index
                %c0_68 = arith.constant 0 : index
                %125 = arith.maxui %38, %c1_67 : index
                %c0_69 = arith.constant 0 : index
                %c1_70 = arith.constant 1 : index
                %c0_71 = arith.constant 0 : index
                %126 = arith.maxui %7, %c1_70 : index
                %c0_72 = arith.constant 0 : index
                %c1_73 = arith.constant 1 : index
                %c0_74 = arith.constant 0 : index
                %127 = arith.maxui %21, %c1_73 : index
                %128 = arith.select %118, %c0_63, %113 : index
                memref.store %46, %alloca_50[] : memref<i32>
                memref.store %c100_i32, %alloca_50[] : memref<i32>
                %129 = memref.load %alloca_50[] : memref<i32>
                %130 = arith.index_cast %129 : i32 to index
                %131 = arith.addi %130, %c12 : index
                %132 = arith.divui %131, %c13 : index
                %133 = arith.divui %132, %c8 : index
                %134 = arith.muli %arg1, %133 : index
                %135 = arith.muli %134, %c13 : index
                %136 = arith.subi %c0, %135 : index
                %137 = arith.subi %130, %135 : index
                %138 = arith.cmpi slt, %136, %c0 : index
                %139 = arith.select %138, %c0, %136 : index
                %140 = arith.cmpi slt, %137, %c0 : index
                %141 = arith.select %140, %c0, %137 : index
                %142 = arith.minui %141, %72 : index
                scf.for %arg5 = %139 to %142 step %c1 {
                  %143 = arith.addi %135, %arg5 : index
                  %144 = arith.index_cast %143 : index to i32
                  %145 = arith.cmpi eq, %144, %c0_i32 : i32
                  %146 = arith.cmpi eq, %144, %c99_i32 : i32
                  %147 = arith.addi %144, %c-1_i32 : i32
                  %148 = arith.index_cast %147 : i32 to index
                  %149 = arith.addi %144, %c1_i32 : i32
                  %150 = arith.index_cast %149 : i32 to index
                  scf.for %arg6 = %c0 to %c100 step %c1 {
                    %151 = arith.index_cast %arg6 : index to i32
                    %152 = scf.if %145 -> (i1) {
                      scf.yield %true : i1
                    } else {
                      %155 = arith.cmpi eq, %151, %c0_i32 : i32
                      scf.yield %155 : i1
                    }
                    %153 = arith.select %152, %true, %146 : i1
                    %154 = scf.if %153 -> (i1) {
                      scf.yield %true : i1
                    } else {
                      %155 = arith.cmpi eq, %151, %c99_i32 : i32
                      scf.yield %155 : i1
                    }
                    scf.if %154 {
                      %c1_75 = arith.constant 1 : index
                      %155 = arith.maxui %126, %c1_75 : index
                      %156 = arith.divui %arg6, %155 : index
                      %157 = arith.subi %156, %c0_72 : index
                      %158 = arith.remui %arg6, %155 : index
                      %159 = arts.db_ref %arg2[%157] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %160 = memref.load %159[%143, %158] : memref<?x?xf64>
                      %c1_76 = arith.constant 1 : index
                      %161 = arith.maxui %125, %c1_76 : index
                      %162 = arith.divui %143, %161 : index
                      %163 = arith.subi %162, %c0_69 : index
                      %164 = arith.remui %143, %161 : index
                      %165 = arts.db_ref %arg3[%163] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      memref.store %160, %165[%164, %arg6] : memref<?x?xf64>
                    } else {
                      %c1_75 = arith.constant 1 : index
                      %155 = arith.maxui %127, %c1_75 : index
                      %156 = arith.divui %148, %155 : index
                      %157 = arith.subi %156, %128 : index
                      %158 = arith.remui %148, %155 : index
                      %159 = arts.db_ref %arg4[%157] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %160 = memref.load %159[%158, %arg6] : memref<?x?xf64>
                      %161 = arith.addi %151, %c1_i32 : i32
                      %162 = arith.index_cast %161 : i32 to index
                      %c1_76 = arith.constant 1 : index
                      %163 = arith.maxui %127, %c1_76 : index
                      %164 = arith.divui %143, %163 : index
                      %165 = arith.subi %164, %128 : index
                      %166 = arith.remui %143, %163 : index
                      %167 = arts.db_ref %arg4[%165] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %168 = memref.load %167[%166, %162] : memref<?x?xf64>
                      %169 = arith.addf %160, %168 : f64
                      %170 = arith.addi %151, %c-1_i32 : i32
                      %171 = arith.index_cast %170 : i32 to index
                      %c1_77 = arith.constant 1 : index
                      %172 = arith.maxui %127, %c1_77 : index
                      %173 = arith.divui %143, %172 : index
                      %174 = arith.subi %173, %128 : index
                      %175 = arith.remui %143, %172 : index
                      %176 = arts.db_ref %arg4[%174] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %177 = memref.load %176[%175, %171] : memref<?x?xf64>
                      %178 = arith.addf %169, %177 : f64
                      %c1_78 = arith.constant 1 : index
                      %179 = arith.maxui %127, %c1_78 : index
                      %180 = arith.divui %150, %179 : index
                      %181 = arith.subi %180, %128 : index
                      %182 = arith.remui %150, %179 : index
                      %183 = arts.db_ref %arg4[%181] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %184 = memref.load %183[%182, %arg6] : memref<?x?xf64>
                      %185 = arith.addf %178, %184 : f64
                      %c1_79 = arith.constant 1 : index
                      %186 = arith.maxui %126, %c1_79 : index
                      %187 = arith.divui %arg6, %186 : index
                      %188 = arith.subi %187, %c0_72 : index
                      %189 = arith.remui %arg6, %186 : index
                      %190 = arts.db_ref %arg2[%188] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      %191 = memref.load %190[%143, %189] : memref<?x?xf64>
                      %192 = arith.mulf %191, %cst_0 : f64
                      %193 = arith.mulf %192, %cst_0 : f64
                      %194 = arith.addf %185, %193 : f64
                      %195 = arith.mulf %194, %cst : f64
                      %c1_80 = arith.constant 1 : index
                      %196 = arith.maxui %125, %c1_80 : index
                      %197 = arith.divui %143, %196 : index
                      %198 = arith.subi %197, %c0_69 : index
                      %199 = arith.remui %143, %196 : index
                      %200 = arts.db_ref %arg3[%198] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                      memref.store %195, %200[%199, %arg6] : memref<?x?xf64>
                    }
                  } {arts.id = 186 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
                } {arts.id = 214 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
                arts.db_release(%arg2) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg3) : memref<?xmemref<?x?xf64>>
                arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
              }
            }
          } {arts.id = 149 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "recovered"}
        } : i64
        memref.store %true, %alloca_48[] : memref<i1>
      }
    } {arts.id = 151 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %47 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%47, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_4, %alloca[] : memref<f64>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %c1_51 = arith.constant 1 : index
      %50 = arith.maxui %38, %c1_51 : index
      %51 = arith.divui %arg0, %50 : index
      %52 = arith.subi %51, %c0_43 : index
      %53 = arith.remui %arg0, %50 : index
      %54 = arts.db_ref %ptr_39[%52] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      %55 = memref.load %54[%53, %arg0] : memref<?x?xf64>
      %56 = memref.load %alloca[] : memref<f64>
      %57 = arith.addf %56, %55 : f64
      memref.store %57, %alloca[] : memref<f64>
    } {arts.id = 156 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "poisson-for.c:142:3">, arts.metadata_provenance = "recovered"}
    %48 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%48) : (f64) -> ()
    call @carts_phase_timer_stop(%47) : (memref<?xi8>) -> ()
    %49 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%49, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%49) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_38) : memref<?xi64>
    arts.db_free(%ptr_39) : memref<?xmemref<?x?xf64>>
    arts.db_free(%guid_25) : memref<?xi64>
    arts.db_free(%ptr_26) : memref<?xmemref<?x?xf64>>
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
