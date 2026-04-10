module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("atax\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1999 = arith.constant 1999 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 3.1415926535897931 : f64
    %c2000 = arith.constant 2000 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 4.000000e+03 : f64
    %c4000 = arith.constant 4000 : index
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 80 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "atax.c:110:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 80 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %c1_2 = arith.constant 1 : index
    %7 = arts.runtime_query <total_workers> -> i32
    %8 = arith.index_castui %7 : i32 to index
    %9 = arith.maxui %8, %c1_2 : index
    %10 = arith.subi %9, %c1_2 : index
    %11 = arith.addi %c4000, %10 : index
    %12 = arith.divui %11, %9 : index
    %13 = arith.maxui %12, %c1_2 : index
    %c1_3 = arith.constant 1 : index
    %14 = arith.maxui %13, %c1_3 : index
    %c1_i64 = arith.constant 1 : i64
    %15 = arith.index_cast %c4000 : index to i64
    %16 = arith.index_cast %14 : index to i64
    %17 = arith.addi %15, %16 : i64
    %18 = arith.subi %17, %c1_i64 : i64
    %19 = arith.divui %18, %16 : i64
    %20 = arith.index_cast %19 : i64 to index
    %c0_4 = arith.constant 0 : index
    %c1_5 = arith.constant 1 : index
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%20] elementType(f64) elementSizes[%14, %c4000] {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "atax.c:87:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 128000000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 33 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%14] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_8 = arith.constant 0 : index
    %c1_9 = arith.constant 1 : index
    %c1_10 = arith.constant 1 : index
    %21 = arith.minui %c2000, %c2000 : index
    %c1_11 = arith.constant 1 : index
    %22 = arith.maxui %21, %c1_11 : index
    %c1_i64_12 = arith.constant 1 : i64
    %23 = arith.index_cast %c4000 : index to i64
    %24 = arith.index_cast %22 : index to i64
    %25 = arith.addi %23, %24 : i64
    %26 = arith.subi %25, %c1_i64_12 : i64
    %27 = arith.divui %26, %24 : i64
    %28 = arith.index_cast %27 : i64 to index
    %c0_13 = arith.constant 0 : index
    %c1_14 = arith.constant 1 : index
    %c0_15 = arith.constant 0 : index
    %c1_16 = arith.constant 1 : index
    %guid_17, %ptr_18 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%28] elementType(f64) elementSizes[%22] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:89:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 90 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_18 : memref<?xmemref<?xf64>>) block_shape[%22] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_19 = arith.constant 0 : index
    %c1_20 = arith.constant 1 : index
    %c1_21 = arith.constant 1 : index
    %c1_22 = arith.constant 1 : index
    %29 = arith.maxui %c2000, %c1_22 : index
    %c1_i64_23 = arith.constant 1 : i64
    %30 = arith.index_cast %c4000 : index to i64
    %31 = arith.index_cast %29 : index to i64
    %32 = arith.addi %30, %31 : i64
    %33 = arith.subi %32, %c1_i64_23 : i64
    %34 = arith.divui %33, %31 : i64
    %35 = arith.index_cast %34 : i64 to index
    %c0_24 = arith.constant 0 : index
    %c1_25 = arith.constant 1 : index
    %c0_26 = arith.constant 0 : index
    %c1_27 = arith.constant 1 : index
    %guid_28, %ptr_29 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%35] elementType(f64) elementSizes[%29] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:90:20", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 36 : i64, lastUseId = 91 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_29 : memref<?xmemref<?xf64>>) block_shape[%29] contract(<ownerDims = [0], postDbRefined = true>)
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %41 = arith.index_cast %arg2 : index to i32
      %42 = arith.sitofp %41 : i32 to f64
      scf.for %arg3 = %c0 to %c4000 step %c1 {
        %43 = arith.index_cast %arg3 : index to i32
        %44 = arith.addi %43, %c1_i32 : i32
        %45 = arith.sitofp %44 : i32 to f64
        %46 = arith.mulf %42, %45 : f64
        %47 = arith.divf %46, %cst_0 : f64
        %c1_30 = arith.constant 1 : index
        %48 = arith.maxui %14, %c1_30 : index
        %49 = arith.divui %arg2, %48 : index
        %50 = arith.subi %49, %c0_8 : index
        %51 = arith.remui %arg2, %48 : index
        %52 = arts.db_ref %ptr[%50] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %47, %52[%51, %arg3] : memref<?x?xf64>
      } {arts.id = 47 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %36 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %41 = arith.muli %arg2, %c2000 : index
        %42 = arith.cmpi uge, %41, %c4000 : index
        %43 = arith.subi %c4000, %41 : index
        %44 = arith.select %42, %c0, %43 : index
        %45 = arith.minui %44, %c2000 : index
        %46 = arith.cmpi ugt, %45, %c0 : index
        scf.if %46 {
          %47 = arith.divui %41, %c4000 : index
          %48 = arith.addi %41, %c1999 : index
          %49 = arith.divui %48, %c4000 : index
          %50 = arith.cmpi ugt, %47, %c0 : index
          %51 = arith.select %50, %49, %c0 : index
          %52 = arith.subi %51, %47 : index
          %53 = arith.addi %52, %c1 : index
          %54 = arith.select %50, %c0, %47 : index
          %55 = arith.select %50, %c0, %53 : index
          %c1_30 = arith.constant 1 : index
          %c0_31 = arith.constant 0 : index
          %56 = arith.maxui %29, %c1_30 : index
          %57 = arith.muli %35, %56 : index
          %58 = arith.divui %41, %56 : index
          %59 = arith.addi %41, %c2000 : index
          %60 = arith.subi %59, %c1_30 : index
          %61 = arith.divui %60, %56 : index
          %62 = arith.subi %35, %c1_30 : index
          %63 = arith.cmpi ugt, %58, %62 : index
          %64 = arith.minui %61, %62 : index
          %65 = arith.select %63, %61, %64 : index
          %66 = arith.subi %65, %58 : index
          %67 = arith.addi %66, %c1_30 : index
          %68 = arith.select %63, %c0_31, %58 : index
          %69 = arith.select %63, %c0_31, %67 : index
          %guid_32, %ptr_33 = arts.db_acquire[<inout>] (%guid_28 : memref<?xi64>, %ptr_29 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%68], sizes[%c1_30] element_offsets[%41] element_sizes[%c2000] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_33 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          %70 = arith.cmpi slt, %43, %c0 : index
          %71 = arith.select %70, %c0, %43 : index
          %72 = arith.minui %71, %45 : index
          %c1_34 = arith.constant 1 : index
          %c0_35 = arith.constant 0 : index
          %73 = arith.maxui %14, %c1_34 : index
          %74 = arith.muli %20, %73 : index
          %75 = arith.divui %41, %73 : index
          %76 = arith.addi %41, %72 : index
          %77 = arith.subi %76, %c1_34 : index
          %78 = arith.divui %77, %73 : index
          %79 = arith.subi %20, %c1_34 : index
          %80 = arith.cmpi ugt, %75, %79 : index
          %81 = arith.minui %78, %79 : index
          %82 = arith.select %80, %78, %81 : index
          %83 = arith.subi %82, %75 : index
          %84 = arith.addi %83, %c1_34 : index
          %85 = arith.select %80, %c0_35, %75 : index
          %86 = arith.select %80, %c0_35, %84 : index
          %guid_36, %ptr_37 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%85], sizes[%86] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_37 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          %c1_38 = arith.constant 1 : index
          %c0_39 = arith.constant 0 : index
          %87 = arith.maxui %22, %c1_38 : index
          %88 = arith.muli %28, %87 : index
          %89 = arith.divui %41, %87 : index
          %90 = arith.addi %41, %c2000 : index
          %91 = arith.subi %90, %c1_38 : index
          %92 = arith.divui %91, %87 : index
          %93 = arith.subi %28, %c1_38 : index
          %94 = arith.cmpi ugt, %89, %93 : index
          %95 = arith.minui %92, %93 : index
          %96 = arith.select %94, %92, %95 : index
          %97 = arith.subi %96, %89 : index
          %98 = arith.addi %97, %c1_38 : index
          %99 = arith.select %94, %c0_39, %89 : index
          %100 = arith.select %94, %c0_39, %98 : index
          %guid_40, %ptr_41 = arts.db_acquire[<out>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%99], sizes[%c1_38] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_41 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_33, %ptr_37, %ptr_41) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 84 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %c1_42 = arith.constant 1 : index
            %c0_43 = arith.constant 0 : index
            %101 = arith.maxui %29, %c1_42 : index
            %c0_44 = arith.constant 0 : index
            %c2000_45 = arith.constant 2000 : index
            %102 = arith.muli %arg2, %c2000_45 : index
            %c1_46 = arith.constant 1 : index
            %103 = arith.maxui %c2000_45, %c1_46 : index
            %c1_47 = arith.constant 1 : index
            %104 = arith.maxui %103, %c1_47 : index
            %105 = arith.divui %102, %104 : index
            %106 = arith.select %63, %c0_44, %105 : index
            %c1_48 = arith.constant 1 : index
            %c0_49 = arith.constant 0 : index
            %107 = arith.maxui %14, %c1_48 : index
            %c0_50 = arith.constant 0 : index
            %c2000_51 = arith.constant 2000 : index
            %108 = arith.muli %arg2, %c2000_51 : index
            %c4000_52 = arith.constant 4000 : index
            %c1_53 = arith.constant 1 : index
            %109 = arith.maxui %8, %c1_53 : index
            %110 = arith.subi %109, %c1_53 : index
            %111 = arith.addi %c4000_52, %110 : index
            %112 = arith.divui %111, %109 : index
            %113 = arith.maxui %112, %c1_53 : index
            %c1_54 = arith.constant 1 : index
            %114 = arith.maxui %113, %c1_54 : index
            %c1_55 = arith.constant 1 : index
            %115 = arith.maxui %114, %c1_55 : index
            %116 = arith.divui %108, %115 : index
            %117 = arith.select %80, %c0_50, %116 : index
            %c1_56 = arith.constant 1 : index
            %c0_57 = arith.constant 0 : index
            %118 = arith.maxui %22, %c1_56 : index
            %c0_58 = arith.constant 0 : index
            %c2000_59 = arith.constant 2000 : index
            %119 = arith.muli %arg2, %c2000_59 : index
            %120 = arith.minui %c2000_59, %c2000_59 : index
            %c1_60 = arith.constant 1 : index
            %121 = arith.maxui %120, %c1_60 : index
            %c1_61 = arith.constant 1 : index
            %122 = arith.maxui %121, %c1_61 : index
            %123 = arith.divui %119, %122 : index
            %124 = arith.select %94, %c0_58, %123 : index
            %125 = arith.subi %c0, %41 : index
            %126 = arith.cmpi slt, %125, %c0 : index
            %127 = arith.select %126, %c0, %125 : index
            %128 = arith.cmpi slt, %43, %c0 : index
            %129 = arith.select %128, %c0, %43 : index
            %130 = arith.minui %129, %45 : index
            scf.for %arg6 = %127 to %130 step %c1 {
              %131 = arith.addi %41, %arg6 : index
              %c1_62 = arith.constant 1 : index
              %132 = arith.maxui %101, %c1_62 : index
              %c0_63 = arith.constant 0 : index
              %c1_64 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %133 = arith.addi %41, %127 : index
              %134 = arith.divui %133, %132 : index
              %135 = arith.subi %arg6, %127 : index
              %136 = arith.addi %127, %135 : index
              %137 = arith.subi %134, %106 : index
              %138 = arts.db_ref %arg3[%c0_43] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %138[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %c1_65 = arith.constant 1 : index
                %139 = arith.maxui %101, %c1_65 : index
                %c0_66 = arith.constant 0 : index
                %c1_67 = arith.constant 1 : index
                %c-1_68 = arith.constant -1 : index
                %140 = arith.addi %41, %127 : index
                %141 = arith.divui %140, %139 : index
                %142 = arith.subi %arg6, %127 : index
                %143 = arith.addi %127, %142 : index
                %144 = arith.subi %141, %106 : index
                %145 = arts.db_ref %arg3[%c0_43] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %146 = memref.load %145[%arg6] : memref<?xf64>
                %c1_69 = arith.constant 1 : index
                %147 = arith.maxui %107, %c1_69 : index
                %148 = arith.divui %131, %147 : index
                %149 = arith.subi %148, %117 : index
                %150 = arith.remui %131, %147 : index
                %151 = arts.db_ref %arg4[%149] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %152 = memref.load %151[%150, %arg7] : memref<?x?xf64>
                %153 = arith.index_cast %arg7 : index to i32
                %154 = arith.sitofp %153 : i32 to f64
                %155 = arith.mulf %154, %cst : f64
                %156 = arith.mulf %152, %155 : f64
                %157 = arith.addf %146, %156 : f64
                %c1_70 = arith.constant 1 : index
                %158 = arith.maxui %101, %c1_70 : index
                %c0_71 = arith.constant 0 : index
                %c1_72 = arith.constant 1 : index
                %c-1_73 = arith.constant -1 : index
                %159 = arith.addi %41, %127 : index
                %160 = arith.divui %159, %158 : index
                %161 = arith.subi %arg6, %127 : index
                %162 = arith.addi %127, %161 : index
                %163 = arith.subi %160, %106 : index
                %164 = arts.db_ref %arg3[%c0_43] : memref<?xmemref<?xf64>> -> memref<?xf64>
                memref.store %157, %164[%arg6] : memref<?xf64>
              } {arts.id = 60 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 82 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?xf64>>
          }
        }
      }
    } : i64
    %37 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %41 = arith.muli %arg2, %c2000 : index
        %42 = arith.cmpi uge, %41, %c4000 : index
        %43 = arith.subi %c4000, %41 : index
        %44 = arith.select %42, %c0, %43 : index
        %45 = arith.minui %44, %c2000 : index
        %46 = arith.cmpi ugt, %45, %c0 : index
        scf.if %46 {
          %47 = arith.divui %41, %c4000 : index
          %48 = arith.addi %41, %c1999 : index
          %49 = arith.divui %48, %c4000 : index
          %50 = arith.cmpi ugt, %47, %c0 : index
          %51 = arith.select %50, %49, %c0 : index
          %52 = arith.subi %51, %47 : index
          %53 = arith.addi %52, %c1 : index
          %54 = arith.select %50, %c0, %47 : index
          %55 = arith.select %50, %c0, %53 : index
          %c0_30 = arith.constant 0 : index
          %56 = arith.cmpi slt, %43, %c0 : index
          %57 = arith.select %56, %c0, %43 : index
          %58 = arith.minui %57, %45 : index
          %c1_31 = arith.constant 1 : index
          %c0_32 = arith.constant 0 : index
          %guid_33, %ptr_34 = arts.db_acquire[<inout>] (%guid_28 : memref<?xi64>, %ptr_29 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%c0_32], sizes[%35] {arts.id = 86 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_34 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %59 = arith.cmpi slt, %43, %c0 : index
          %60 = arith.select %59, %c0, %43 : index
          %61 = arith.minui %60, %45 : index
          %c1_35 = arith.constant 1 : index
          %c0_36 = arith.constant 0 : index
          %guid_37, %ptr_38 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%c0_36], sizes[%20] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [4000, 2000], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_38 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c4000] contract(<ownerDims = [1]>)
          %c1_39 = arith.constant 1 : index
          %c0_40 = arith.constant 0 : index
          %62 = arith.maxui %22, %c1_39 : index
          %63 = arith.muli %28, %62 : index
          %64 = arith.divui %41, %62 : index
          %65 = arith.addi %41, %c2000 : index
          %66 = arith.subi %65, %c1_39 : index
          %67 = arith.divui %66, %62 : index
          %68 = arith.subi %28, %c1_39 : index
          %69 = arith.cmpi ugt, %64, %68 : index
          %70 = arith.minui %67, %68 : index
          %71 = arith.select %69, %67, %70 : index
          %72 = arith.subi %71, %64 : index
          %73 = arith.addi %72, %c1_39 : index
          %74 = arith.select %69, %c0_40, %64 : index
          %75 = arith.select %69, %c0_40, %73 : index
          %guid_41, %ptr_42 = arts.db_acquire[<out>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%74], sizes[%c1_39] element_offsets[%41] element_sizes[%c2000] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_42 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_34, %ptr_38, %ptr_42) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 85 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %c1_43 = arith.constant 1 : index
            %c0_44 = arith.constant 0 : index
            %76 = arith.maxui %29, %c1_43 : index
            %c0_45 = arith.constant 0 : index
            %c1_46 = arith.constant 1 : index
            %c0_47 = arith.constant 0 : index
            %77 = arith.maxui %14, %c1_46 : index
            %c0_48 = arith.constant 0 : index
            %c1_49 = arith.constant 1 : index
            %c0_50 = arith.constant 0 : index
            %78 = arith.maxui %22, %c1_49 : index
            %c0_51 = arith.constant 0 : index
            %c2000_52 = arith.constant 2000 : index
            %79 = arith.muli %arg2, %c2000_52 : index
            %80 = arith.minui %c2000_52, %c2000_52 : index
            %c1_53 = arith.constant 1 : index
            %81 = arith.maxui %80, %c1_53 : index
            %c1_54 = arith.constant 1 : index
            %82 = arith.maxui %81, %c1_54 : index
            %83 = arith.divui %79, %82 : index
            %84 = arith.select %69, %c0_51, %83 : index
            %85 = arith.subi %c0, %41 : index
            %86 = arith.cmpi slt, %85, %c0 : index
            %87 = arith.select %86, %c0, %85 : index
            %88 = arith.cmpi slt, %43, %c0 : index
            %89 = arith.select %88, %c0, %43 : index
            %90 = arith.minui %89, %45 : index
            scf.for %arg6 = %87 to %90 step %c1 {
              %91 = arith.addi %41, %arg6 : index
              %c1_55 = arith.constant 1 : index
              %92 = arith.maxui %78, %c1_55 : index
              %c0_56 = arith.constant 0 : index
              %c1_57 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %93 = arith.addi %41, %87 : index
              %94 = arith.divui %93, %92 : index
              %95 = arith.subi %arg6, %87 : index
              %96 = arith.addi %87, %95 : index
              %97 = arith.subi %94, %84 : index
              %98 = arts.db_ref %arg5[%c0_50] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %98[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %c1_58 = arith.constant 1 : index
                %99 = arith.maxui %78, %c1_58 : index
                %c0_59 = arith.constant 0 : index
                %c1_60 = arith.constant 1 : index
                %c-1_61 = arith.constant -1 : index
                %100 = arith.addi %41, %87 : index
                %101 = arith.divui %100, %99 : index
                %102 = arith.subi %arg6, %87 : index
                %103 = arith.addi %87, %102 : index
                %104 = arith.subi %101, %84 : index
                %105 = arts.db_ref %arg5[%c0_50] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %106 = memref.load %105[%arg6] : memref<?xf64>
                %c1_62 = arith.constant 1 : index
                %107 = arith.maxui %77, %c1_62 : index
                %108 = arith.divui %arg7, %107 : index
                %109 = arith.subi %108, %c0_48 : index
                %110 = arith.remui %arg7, %107 : index
                %111 = arts.db_ref %arg4[%109] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %112 = memref.load %111[%110, %91] : memref<?x?xf64>
                %c1_63 = arith.constant 1 : index
                %113 = arith.maxui %76, %c1_63 : index
                %114 = arith.divui %arg7, %113 : index
                %115 = arith.subi %114, %c0_45 : index
                %116 = arith.remui %arg7, %113 : index
                %117 = arts.db_ref %arg3[%115] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %118 = memref.load %117[%116] : memref<?xf64>
                %119 = arith.mulf %112, %118 : f64
                %120 = arith.addf %106, %119 : f64
                %c1_64 = arith.constant 1 : index
                %121 = arith.maxui %78, %c1_64 : index
                %c0_65 = arith.constant 0 : index
                %c1_66 = arith.constant 1 : index
                %c-1_67 = arith.constant -1 : index
                %122 = arith.addi %41, %87 : index
                %123 = arith.divui %122, %121 : index
                %124 = arith.subi %arg6, %87 : index
                %125 = arith.addi %87, %124 : index
                %126 = arith.subi %123, %84 : index
                %127 = arts.db_ref %arg5[%c0_50] : memref<?xmemref<?xf64>> -> memref<?xf64>
                memref.store %120, %127[%arg6] : memref<?xf64>
              } {arts.id = 74 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 83 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?xf64>>
          }
        }
      }
    } : i64
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %38 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%38, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_1, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %c1_30 = arith.constant 1 : index
      %41 = arith.maxui %22, %c1_30 : index
      %42 = arith.divui %arg2, %41 : index
      %43 = arith.subi %42, %c0_19 : index
      %44 = arith.remui %arg2, %41 : index
      %45 = arts.db_ref %ptr_18[%43] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %46 = memref.load %45[%44] : memref<?xf64>
      %47 = memref.load %alloca[] : memref<f64>
      %48 = arith.addf %47, %46 : f64
      memref.store %48, %alloca[] : memref<f64>
    } {arts.id = 78 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:111:3">, arts.metadata_provenance = "recovered"}
    %39 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%39) : (f64) -> ()
    call @carts_phase_timer_stop(%38) : (memref<?xi8>) -> ()
    %40 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%40, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%40) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_28) : memref<?xi64>
    arts.db_free(%ptr_29) : memref<?xmemref<?xf64>>
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
