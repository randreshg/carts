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
    %c1_3 = arith.constant 1 : index
    %c1_4 = arith.constant 1 : index
    %7 = arts.runtime_query <total_workers> -> i32
    %8 = arith.index_castui %7 : i32 to index
    %9 = arith.maxui %8, %c1_4 : index
    %10 = arith.subi %9, %c1_4 : index
    %11 = arith.addi %c4000, %10 : index
    %12 = arith.divui %11, %9 : index
    %13 = arith.maxui %12, %c1_4 : index
    %c1_5 = arith.constant 1 : index
    %14 = arith.maxui %13, %c1_5 : index
    %c1_i64 = arith.constant 1 : i64
    %15 = arith.index_cast %c4000 : index to i64
    %16 = arith.index_cast %14 : index to i64
    %17 = arith.addi %15, %16 : i64
    %18 = arith.subi %17, %c1_i64 : i64
    %19 = arith.divui %18, %16 : i64
    %20 = arith.index_cast %19 : i64 to index
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    %c0_8 = arith.constant 0 : index
    %c1_9 = arith.constant 1 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%20] elementType(f64) elementSizes[%14, %c4000] {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "atax.c:87:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 128000000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 33 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%14] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_10 = arith.constant 0 : index
    %c1_11 = arith.constant 1 : index
    %c1_12 = arith.constant 1 : index
    %21 = arith.minui %c2000, %c2000 : index
    %c1_13 = arith.constant 1 : index
    %22 = arith.maxui %21, %c1_13 : index
    %c1_i64_14 = arith.constant 1 : i64
    %23 = arith.index_cast %c4000 : index to i64
    %24 = arith.index_cast %22 : index to i64
    %25 = arith.addi %23, %24 : i64
    %26 = arith.subi %25, %c1_i64_14 : i64
    %27 = arith.divui %26, %24 : i64
    %28 = arith.index_cast %27 : i64 to index
    %c0_15 = arith.constant 0 : index
    %c1_16 = arith.constant 1 : index
    %c0_17 = arith.constant 0 : index
    %c1_18 = arith.constant 1 : index
    %guid_19, %ptr_20 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%28] elementType(f64) elementSizes[%22] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:89:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 90 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_20 : memref<?xmemref<?xf64>>) block_shape[%22] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_21 = arith.constant 0 : index
    %c1_22 = arith.constant 1 : index
    %c1_23 = arith.constant 1 : index
    %c1_24 = arith.constant 1 : index
    %29 = arith.maxui %c2000, %c1_24 : index
    %c1_i64_25 = arith.constant 1 : i64
    %30 = arith.index_cast %c4000 : index to i64
    %31 = arith.index_cast %29 : index to i64
    %32 = arith.addi %30, %31 : i64
    %33 = arith.subi %32, %c1_i64_25 : i64
    %34 = arith.divui %33, %31 : i64
    %35 = arith.index_cast %34 : i64 to index
    %c0_26 = arith.constant 0 : index
    %c1_27 = arith.constant 1 : index
    %c0_28 = arith.constant 0 : index
    %c1_29 = arith.constant 1 : index
    %guid_30, %ptr_31 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%35] elementType(f64) elementSizes[%29] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:90:20", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 36 : i64, lastUseId = 91 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_31 : memref<?xmemref<?xf64>>) block_shape[%29] contract(<ownerDims = [0], postDbRefined = true>)
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %41 = arith.index_cast %arg2 : index to i32
      %42 = arith.sitofp %41 : i32 to f64
      scf.for %arg3 = %c0 to %c4000 step %c1 {
        %43 = arith.index_cast %arg3 : index to i32
        %44 = arith.addi %43, %c1_i32 : i32
        %45 = arith.sitofp %44 : i32 to f64
        %46 = arith.mulf %42, %45 : f64
        %47 = arith.divf %46, %cst_0 : f64
        %c1_32 = arith.constant 1 : index
        %48 = arith.maxui %14, %c1_32 : index
        %49 = arith.divui %arg2, %48 : index
        %50 = arith.subi %49, %c0_10 : index
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
          %c1_32 = arith.constant 1 : index
          %c0_33 = arith.constant 0 : index
          %56 = arith.maxui %29, %c1_32 : index
          %57 = arith.muli %35, %56 : index
          %58 = arith.divui %41, %56 : index
          %59 = arith.addi %41, %c2000 : index
          %60 = arith.subi %59, %c1_32 : index
          %61 = arith.divui %60, %56 : index
          %62 = arith.subi %35, %c1_32 : index
          %63 = arith.cmpi ugt, %58, %62 : index
          %64 = arith.minui %61, %62 : index
          %65 = arith.select %63, %61, %64 : index
          %66 = arith.subi %65, %58 : index
          %67 = arith.addi %66, %c1_32 : index
          %68 = arith.select %63, %c0_33, %58 : index
          %69 = arith.select %63, %c0_33, %67 : index
          %guid_34, %ptr_35 = arts.db_acquire[<inout>] (%guid_30 : memref<?xi64>, %ptr_31 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%68], sizes[%c1_32] element_offsets[%41] element_sizes[%c2000] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_35 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          %70 = arith.addi %41, %45 : index
          %71 = arith.cmpi uge, %41, %c1 : index
          %72 = arith.subi %41, %c1 : index
          %73 = arith.select %71, %72, %c0 : index
          %74 = arith.addi %70, %c1 : index
          %75 = arith.minui %74, %c4000 : index
          %76 = arith.cmpi uge, %75, %73 : index
          %77 = arith.subi %75, %73 : index
          %78 = arith.select %76, %77, %c0 : index
          %79 = arith.maxui %78, %c1 : index
          %80 = arith.divui %73, %c4000 : index
          %81 = arith.addi %73, %79 : index
          %82 = arith.subi %81, %c1 : index
          %83 = arith.divui %82, %c4000 : index
          %84 = arith.cmpi ugt, %80, %c0 : index
          %85 = arith.select %84, %83, %c0 : index
          %86 = arith.subi %85, %80 : index
          %87 = arith.addi %86, %c1 : index
          %88 = arith.select %84, %c0, %80 : index
          %89 = arith.select %84, %c0, %87 : index
          %c1_36 = arith.constant 1 : index
          %c0_37 = arith.constant 0 : index
          %90 = arith.maxui %14, %c1_36 : index
          %91 = arith.muli %20, %90 : index
          %92 = arith.divui %73, %90 : index
          %93 = arith.addi %73, %78 : index
          %94 = arith.subi %93, %c1_36 : index
          %95 = arith.divui %94, %90 : index
          %96 = arith.subi %20, %c1_36 : index
          %97 = arith.cmpi ugt, %92, %96 : index
          %98 = arith.minui %95, %96 : index
          %99 = arith.select %97, %95, %98 : index
          %100 = arith.subi %99, %92 : index
          %101 = arith.addi %100, %c1_36 : index
          %102 = arith.select %97, %c0_37, %92 : index
          %103 = arith.select %97, %c0_37, %101 : index
          %guid_38, %ptr_39 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%73], sizes[%78]), offsets[%102], sizes[%103] element_offsets[%73] element_sizes[%78] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000, 4000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_39 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          %c1_40 = arith.constant 1 : index
          %c0_41 = arith.constant 0 : index
          %104 = arith.maxui %22, %c1_40 : index
          %105 = arith.muli %28, %104 : index
          %106 = arith.divui %41, %104 : index
          %107 = arith.addi %41, %c2000 : index
          %108 = arith.subi %107, %c1_40 : index
          %109 = arith.divui %108, %104 : index
          %110 = arith.subi %28, %c1_40 : index
          %111 = arith.cmpi ugt, %106, %110 : index
          %112 = arith.minui %109, %110 : index
          %113 = arith.select %111, %109, %112 : index
          %114 = arith.subi %113, %106 : index
          %115 = arith.addi %114, %c1_40 : index
          %116 = arith.select %111, %c0_41, %106 : index
          %117 = arith.select %111, %c0_41, %115 : index
          %guid_42, %ptr_43 = arts.db_acquire[<out>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%116], sizes[%c1_40] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_43 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_35, %ptr_39, %ptr_43) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 84 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %c1_44 = arith.constant 1 : index
            %c0_45 = arith.constant 0 : index
            %118 = arith.maxui %29, %c1_44 : index
            %c0_46 = arith.constant 0 : index
            %c2000_47 = arith.constant 2000 : index
            %119 = arith.muli %arg2, %c2000_47 : index
            %c1_48 = arith.constant 1 : index
            %120 = arith.maxui %c2000_47, %c1_48 : index
            %c1_49 = arith.constant 1 : index
            %121 = arith.maxui %120, %c1_49 : index
            %122 = arith.divui %119, %121 : index
            %123 = arith.select %63, %c0_46, %122 : index
            %c1_50 = arith.constant 1 : index
            %c0_51 = arith.constant 0 : index
            %124 = arith.maxui %14, %c1_50 : index
            %c0_52 = arith.constant 0 : index
            %c2000_53 = arith.constant 2000 : index
            %125 = arith.muli %arg2, %c2000_53 : index
            %c1_54 = arith.constant 1 : index
            %126 = arith.subi %125, %c1_54 : index
            %c0_55 = arith.constant 0 : index
            %127 = arith.select %71, %126, %c0_55 : index
            %c4000_56 = arith.constant 4000 : index
            %c1_57 = arith.constant 1 : index
            %128 = arith.maxui %8, %c1_57 : index
            %129 = arith.subi %128, %c1_57 : index
            %130 = arith.addi %c4000_56, %129 : index
            %131 = arith.divui %130, %128 : index
            %132 = arith.maxui %131, %c1_57 : index
            %c1_58 = arith.constant 1 : index
            %133 = arith.maxui %132, %c1_58 : index
            %c1_59 = arith.constant 1 : index
            %134 = arith.maxui %133, %c1_59 : index
            %135 = arith.divui %127, %134 : index
            %136 = arith.select %97, %c0_52, %135 : index
            %c1_60 = arith.constant 1 : index
            %c0_61 = arith.constant 0 : index
            %137 = arith.maxui %22, %c1_60 : index
            %c0_62 = arith.constant 0 : index
            %c2000_63 = arith.constant 2000 : index
            %138 = arith.muli %arg2, %c2000_63 : index
            %139 = arith.minui %c2000_63, %c2000_63 : index
            %c1_64 = arith.constant 1 : index
            %140 = arith.maxui %139, %c1_64 : index
            %c1_65 = arith.constant 1 : index
            %141 = arith.maxui %140, %c1_65 : index
            %142 = arith.divui %138, %141 : index
            %143 = arith.select %111, %c0_62, %142 : index
            %144 = arith.subi %c0, %41 : index
            %145 = arith.cmpi slt, %144, %c0 : index
            %146 = arith.select %145, %c0, %144 : index
            %147 = arith.cmpi slt, %43, %c0 : index
            %148 = arith.select %147, %c0, %43 : index
            %149 = arith.minui %148, %45 : index
            scf.for %arg6 = %146 to %149 step %c1 {
              %150 = arith.addi %41, %arg6 : index
              %c1_66 = arith.constant 1 : index
              %151 = arith.maxui %118, %c1_66 : index
              %c0_67 = arith.constant 0 : index
              %c1_68 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %152 = arith.addi %41, %146 : index
              %153 = arith.divui %152, %151 : index
              %154 = arith.subi %arg6, %146 : index
              %155 = arith.addi %146, %154 : index
              %156 = arith.subi %153, %123 : index
              %157 = arts.db_ref %arg3[%c0_45] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %157[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %c1_69 = arith.constant 1 : index
                %158 = arith.maxui %118, %c1_69 : index
                %c0_70 = arith.constant 0 : index
                %c1_71 = arith.constant 1 : index
                %c-1_72 = arith.constant -1 : index
                %159 = arith.addi %41, %146 : index
                %160 = arith.divui %159, %158 : index
                %161 = arith.subi %arg6, %146 : index
                %162 = arith.addi %146, %161 : index
                %163 = arith.subi %160, %123 : index
                %164 = arts.db_ref %arg3[%c0_45] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %165 = memref.load %164[%arg6] : memref<?xf64>
                %c1_73 = arith.constant 1 : index
                %166 = arith.maxui %124, %c1_73 : index
                %167 = arith.divui %150, %166 : index
                %168 = arith.subi %167, %136 : index
                %169 = arith.remui %150, %166 : index
                %170 = arts.db_ref %arg4[%168] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %171 = memref.load %170[%169, %arg7] : memref<?x?xf64>
                %172 = arith.index_cast %arg7 : index to i32
                %173 = arith.sitofp %172 : i32 to f64
                %174 = arith.mulf %173, %cst : f64
                %175 = arith.mulf %171, %174 : f64
                %176 = arith.addf %165, %175 : f64
                %c1_74 = arith.constant 1 : index
                %177 = arith.maxui %118, %c1_74 : index
                %c0_75 = arith.constant 0 : index
                %c1_76 = arith.constant 1 : index
                %c-1_77 = arith.constant -1 : index
                %178 = arith.addi %41, %146 : index
                %179 = arith.divui %178, %177 : index
                %180 = arith.subi %arg6, %146 : index
                %181 = arith.addi %146, %180 : index
                %182 = arith.subi %179, %123 : index
                %183 = arts.db_ref %arg3[%c0_45] : memref<?xmemref<?xf64>> -> memref<?xf64>
                memref.store %176, %183[%arg6] : memref<?xf64>
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
          %c0_32 = arith.constant 0 : index
          %56 = arith.cmpi slt, %43, %c0 : index
          %57 = arith.select %56, %c0, %43 : index
          %58 = arith.minui %57, %45 : index
          %c1_33 = arith.constant 1 : index
          %c0_34 = arith.constant 0 : index
          %guid_35, %ptr_36 = arts.db_acquire[<inout>] (%guid_30 : memref<?xi64>, %ptr_31 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%c0_34], sizes[%35] {arts.id = 86 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_36 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %59 = arith.addi %41, %45 : index
          %60 = arith.cmpi uge, %41, %c1 : index
          %61 = arith.subi %41, %c1 : index
          %62 = arith.select %60, %61, %c0 : index
          %63 = arith.addi %59, %c1 : index
          %64 = arith.minui %63, %c4000 : index
          %65 = arith.cmpi uge, %64, %62 : index
          %66 = arith.subi %64, %62 : index
          %67 = arith.select %65, %66, %c0 : index
          %68 = arith.maxui %67, %c1 : index
          %69 = arith.divui %62, %c4000 : index
          %70 = arith.addi %62, %68 : index
          %71 = arith.subi %70, %c1 : index
          %72 = arith.divui %71, %c4000 : index
          %73 = arith.cmpi ugt, %69, %c0 : index
          %74 = arith.select %73, %72, %c0 : index
          %75 = arith.subi %74, %69 : index
          %76 = arith.addi %75, %c1 : index
          %77 = arith.select %73, %c0, %69 : index
          %78 = arith.select %73, %c0, %76 : index
          %79 = arith.cmpi slt, %43, %c0 : index
          %80 = arith.select %79, %c0, %43 : index
          %81 = arith.minui %80, %45 : index
          %82 = arith.cmpi slt, %43, %c0 : index
          %83 = arith.select %82, %c0, %43 : index
          %84 = arith.minui %83, %45 : index
          %c1_37 = arith.constant 1 : index
          %c0_38 = arith.constant 0 : index
          %guid_39, %ptr_40 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%62], sizes[%67]), offsets[%c0_38], sizes[%20] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [4000, 2000], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_40 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c4000] contract(<ownerDims = [1]>)
          %c1_41 = arith.constant 1 : index
          %c0_42 = arith.constant 0 : index
          %85 = arith.maxui %22, %c1_41 : index
          %86 = arith.muli %28, %85 : index
          %87 = arith.divui %41, %85 : index
          %88 = arith.addi %41, %c2000 : index
          %89 = arith.subi %88, %c1_41 : index
          %90 = arith.divui %89, %85 : index
          %91 = arith.subi %28, %c1_41 : index
          %92 = arith.cmpi ugt, %87, %91 : index
          %93 = arith.minui %90, %91 : index
          %94 = arith.select %92, %90, %93 : index
          %95 = arith.subi %94, %87 : index
          %96 = arith.addi %95, %c1_41 : index
          %97 = arith.select %92, %c0_42, %87 : index
          %98 = arith.select %92, %c0_42, %96 : index
          %guid_43, %ptr_44 = arts.db_acquire[<out>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%41], sizes[%c2000]), offsets[%97], sizes[%c1_41] element_offsets[%41] element_sizes[%c2000] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_44 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_36, %ptr_40, %ptr_44) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 85 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %c1_45 = arith.constant 1 : index
            %c0_46 = arith.constant 0 : index
            %99 = arith.maxui %29, %c1_45 : index
            %c0_47 = arith.constant 0 : index
            %c1_48 = arith.constant 1 : index
            %c0_49 = arith.constant 0 : index
            %100 = arith.maxui %14, %c1_48 : index
            %c0_50 = arith.constant 0 : index
            %c1_51 = arith.constant 1 : index
            %c0_52 = arith.constant 0 : index
            %101 = arith.maxui %22, %c1_51 : index
            %c0_53 = arith.constant 0 : index
            %c2000_54 = arith.constant 2000 : index
            %102 = arith.muli %arg2, %c2000_54 : index
            %103 = arith.minui %c2000_54, %c2000_54 : index
            %c1_55 = arith.constant 1 : index
            %104 = arith.maxui %103, %c1_55 : index
            %c1_56 = arith.constant 1 : index
            %105 = arith.maxui %104, %c1_56 : index
            %106 = arith.divui %102, %105 : index
            %107 = arith.select %92, %c0_53, %106 : index
            %108 = arith.subi %c0, %41 : index
            %109 = arith.cmpi slt, %108, %c0 : index
            %110 = arith.select %109, %c0, %108 : index
            %111 = arith.cmpi slt, %43, %c0 : index
            %112 = arith.select %111, %c0, %43 : index
            %113 = arith.minui %112, %45 : index
            scf.for %arg6 = %110 to %113 step %c1 {
              %114 = arith.addi %41, %arg6 : index
              %c1_57 = arith.constant 1 : index
              %115 = arith.maxui %101, %c1_57 : index
              %c0_58 = arith.constant 0 : index
              %c1_59 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %116 = arith.addi %41, %110 : index
              %117 = arith.divui %116, %115 : index
              %118 = arith.subi %arg6, %110 : index
              %119 = arith.addi %110, %118 : index
              %120 = arith.subi %117, %107 : index
              %121 = arts.db_ref %arg5[%c0_52] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %121[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %c1_60 = arith.constant 1 : index
                %122 = arith.maxui %101, %c1_60 : index
                %c0_61 = arith.constant 0 : index
                %c1_62 = arith.constant 1 : index
                %c-1_63 = arith.constant -1 : index
                %123 = arith.addi %41, %110 : index
                %124 = arith.divui %123, %122 : index
                %125 = arith.subi %arg6, %110 : index
                %126 = arith.addi %110, %125 : index
                %127 = arith.subi %124, %107 : index
                %128 = arts.db_ref %arg5[%c0_52] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %129 = memref.load %128[%arg6] : memref<?xf64>
                %c1_64 = arith.constant 1 : index
                %130 = arith.maxui %100, %c1_64 : index
                %131 = arith.divui %arg7, %130 : index
                %132 = arith.subi %131, %c0_50 : index
                %133 = arith.remui %arg7, %130 : index
                %134 = arts.db_ref %arg4[%132] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %135 = memref.load %134[%133, %114] : memref<?x?xf64>
                %c1_65 = arith.constant 1 : index
                %136 = arith.maxui %99, %c1_65 : index
                %137 = arith.divui %arg7, %136 : index
                %138 = arith.subi %137, %c0_47 : index
                %139 = arith.remui %arg7, %136 : index
                %140 = arts.db_ref %arg3[%138] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %141 = memref.load %140[%139] : memref<?xf64>
                %142 = arith.mulf %135, %141 : f64
                %143 = arith.addf %129, %142 : f64
                %c1_66 = arith.constant 1 : index
                %144 = arith.maxui %101, %c1_66 : index
                %c0_67 = arith.constant 0 : index
                %c1_68 = arith.constant 1 : index
                %c-1_69 = arith.constant -1 : index
                %145 = arith.addi %41, %110 : index
                %146 = arith.divui %145, %144 : index
                %147 = arith.subi %arg6, %110 : index
                %148 = arith.addi %110, %147 : index
                %149 = arith.subi %146, %107 : index
                %150 = arts.db_ref %arg5[%c0_52] : memref<?xmemref<?xf64>> -> memref<?xf64>
                memref.store %143, %150[%arg6] : memref<?xf64>
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
      %c1_32 = arith.constant 1 : index
      %41 = arith.maxui %22, %c1_32 : index
      %42 = arith.divui %arg2, %41 : index
      %43 = arith.subi %42, %c0_21 : index
      %44 = arith.remui %arg2, %41 : index
      %45 = arts.db_ref %ptr_20[%43] : memref<?xmemref<?xf64>> -> memref<?xf64>
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
    arts.db_free(%guid_30) : memref<?xi64>
    arts.db_free(%ptr_31) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?xf64>>
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
