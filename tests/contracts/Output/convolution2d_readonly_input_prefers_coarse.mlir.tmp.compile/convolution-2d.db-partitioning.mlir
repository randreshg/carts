module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("convolution-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c15 = arith.constant 15 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 1.000000e-01 : f64
    %cst_0 = arith.constant 0.69999999999999996 : f64
    %cst_1 = arith.constant 4.000000e-01 : f64
    %cst_2 = arith.constant -9.000000e-01 : f64
    %cst_3 = arith.constant 6.000000e-01 : f64
    %cst_4 = arith.constant -3.000000e-01 : f64
    %cst_5 = arith.constant -8.000000e-01 : f64
    %cst_6 = arith.constant 5.000000e-01 : f64
    %cst_7 = arith.constant 2.000000e-01 : f64
    %c1_i32 = arith.constant 1 : i32
    %c16 = arith.constant 16 : index
    %c1023 = arith.constant 1023 : index
    %cst_8 = arith.constant 1.024000e+03 : f32
    %c1024 = arith.constant 1024 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_9 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "convolution-2d.c:111:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %c1_11 = arith.constant 1 : index
    %c0_12 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1024, %c1024] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "convolution-2d.c:82:19", totalAccesses = 10 : i64, readCount = 9 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %c1_13 = arith.constant 1 : index
    %c1024_14 = arith.constant 1024 : index
    %c1_15 = arith.constant 1 : index
    %7 = arith.maxui %c16, %c1_15 : index
    %c1024_16 = arith.constant 1024 : index
    %c1_i64 = arith.constant 1 : i64
    %8 = arith.index_cast %c1024 : index to i64
    %9 = arith.index_cast %7 : index to i64
    %10 = arith.addi %8, %9 : i64
    %11 = arith.subi %10, %c1_i64 : i64
    %12 = arith.divui %11, %9 : i64
    %13 = arith.index_cast %12 : i64 to index
    %c0_17 = arith.constant 0 : index
    %c1_18 = arith.constant 1 : index
    %guid_19, %ptr_20 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%13] elementType(f32) elementSizes[%7, %c1024] {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "convolution-2d.c:83:19", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 12 : i64>, arts.metadata_origin_id = 37 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    arts.lowering_contract(%ptr_20 : memref<?xmemref<?x?xf32>>) block_shape[%7] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_21 = arith.constant 0 : index
    %c0_22 = arith.constant 0 : index
    scf.for %arg2 = %c0 to %c1024 step %c1 {
      %18 = arith.index_cast %arg2 : index to i32
      scf.for %arg3 = %c0 to %c1024 step %c1 {
        %19 = arith.index_cast %arg3 : index to i32
        %20 = arith.addi %18, %19 : i32
        %21 = arith.sitofp %20 : i32 to f32
        %22 = arith.divf %21, %cst_8 : f32
        %23 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        memref.store %22, %23[%arg2, %arg3] : memref<?x?xf32>
      } {arts.id = 47 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:91:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:91:3">, arts.metadata_provenance = "recovered"}
    scf.for %arg2 = %c0 to %c1024 step %c1 {
      scf.for %arg3 = %c0 to %c1024 step %c1 {
        %c1_23 = arith.constant 1 : index
        %18 = arith.maxui %7, %c1_23 : index
        %19 = arith.divui %arg2, %18 : index
        %20 = arith.subi %19, %c0_21 : index
        %21 = arith.remui %arg2, %18 : index
        %22 = arts.db_ref %ptr_20[%20] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        memref.store %cst_10, %22[%21, %arg3] : memref<?x?xf32>
      } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:95:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 50 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:94:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %14 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c64 step %c1 {
        %18 = arith.muli %arg2, %c16 : index
        %19 = arith.cmpi uge, %18, %c1023 : index
        %20 = arith.subi %c1023, %18 : index
        %21 = arith.select %19, %c0, %20 : index
        %22 = arith.minui %21, %c16 : index
        %23 = arith.cmpi ugt, %22, %c0 : index
        scf.if %23 {
          %24 = arith.addi %18, %22 : index
          %25 = arith.cmpi uge, %18, %c1 : index
          %26 = arith.subi %18, %c1 : index
          %27 = arith.select %25, %26, %c0 : index
          %28 = arith.addi %24, %c1 : index
          %29 = arith.minui %28, %c1024 : index
          %30 = arith.cmpi uge, %29, %27 : index
          %31 = arith.subi %29, %27 : index
          %32 = arith.select %30, %31, %c0 : index
          %33 = arith.maxui %32, %c1 : index
          %34 = arith.divui %27, %c1024 : index
          %35 = arith.addi %27, %33 : index
          %36 = arith.subi %35, %c1 : index
          %37 = arith.divui %36, %c1024 : index
          %38 = arith.cmpi ugt, %34, %c0 : index
          %39 = arith.select %38, %37, %c0 : index
          %40 = arith.subi %39, %34 : index
          %41 = arith.addi %40, %c1 : index
          %42 = arith.select %38, %c0, %34 : index
          %43 = arith.select %38, %c0, %41 : index
          %guid_23, %ptr_24 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[%27], sizes[%32]), offsets[%42], sizes[%43] element_offsets[%27] element_sizes[%32] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [16, 1024], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)
          arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?xf32>>) pattern(<depPattern = <stencil>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c16] contract(<ownerDims = [0], narrowableDep = true, postDbRefined = true>)
          %44 = arith.divui %18, %c1024 : index
          %45 = arith.addi %18, %c15 : index
          %46 = arith.divui %45, %c1024 : index
          %47 = arith.cmpi ugt, %44, %c0 : index
          %48 = arith.select %47, %46, %c0 : index
          %49 = arith.subi %48, %44 : index
          %50 = arith.addi %49, %c1 : index
          %51 = arith.select %47, %c0, %44 : index
          %52 = arith.select %47, %c0, %50 : index
          %c1_25 = arith.constant 1 : index
          %c0_26 = arith.constant 0 : index
          %53 = arith.maxui %7, %c1_25 : index
          %54 = arith.muli %13, %53 : index
          %55 = arith.divui %18, %53 : index
          %56 = arith.addi %18, %c16 : index
          %57 = arith.subi %56, %c1_25 : index
          %58 = arith.divui %57, %53 : index
          %59 = arith.subi %13, %c1_25 : index
          %60 = arith.cmpi ugt, %55, %59 : index
          %61 = arith.minui %58, %59 : index
          %62 = arith.select %60, %58, %61 : index
          %63 = arith.subi %62, %55 : index
          %64 = arith.addi %63, %c1_25 : index
          %65 = arith.select %60, %c0_26, %55 : index
          %66 = arith.select %60, %c0_26, %64 : index
          %guid_27, %ptr_28 = arts.db_acquire[<inout>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[%18], sizes[%c16]), offsets[%65], sizes[%c1_25] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [16, 1024], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)
          arts.lowering_contract(%ptr_28 : memref<?xmemref<?x?xf32>>) pattern(<depPattern = <stencil>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c16] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_24, %ptr_28) : memref<?xmemref<?x?xf32>>, memref<?xmemref<?x?xf32>> attributes {arts.id = 110 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "stencil", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<stencil>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [16, 1024]} {
          ^bb0(%arg3: memref<?xmemref<?x?xf32>>, %arg4: memref<?xmemref<?x?xf32>>):
            %c1_29 = arith.constant 1 : index
            %c0_30 = arith.constant 0 : index
            %67 = arith.maxui %7, %c1_29 : index
            %c0_31 = arith.constant 0 : index
            %c16_32 = arith.constant 16 : index
            %68 = arith.muli %arg2, %c16_32 : index
            %c1_33 = arith.constant 1 : index
            %69 = arith.maxui %c16_32, %c1_33 : index
            %c1_34 = arith.constant 1 : index
            %70 = arith.maxui %69, %c1_34 : index
            %71 = arith.divui %68, %70 : index
            %72 = arith.select %60, %c0_31, %71 : index
            %73 = arith.subi %c1, %18 : index
            %74 = arith.cmpi slt, %73, %c0 : index
            %75 = arith.select %74, %c0, %73 : index
            %76 = arith.cmpi slt, %20, %c0 : index
            %77 = arith.select %76, %c0, %20 : index
            %78 = arith.minui %77, %22 : index
            scf.for %arg5 = %75 to %78 step %c1 {
              %79 = arith.addi %18, %arg5 : index
              %80 = arith.index_cast %79 : index to i32
              %81 = arith.addi %80, %c-1_i32 : i32
              %82 = arith.index_cast %81 : i32 to index
              %83 = arith.addi %80, %c1_i32 : i32
              %84 = arith.index_cast %83 : i32 to index
              scf.for %arg6 = %c1 to %c1023 step %c1 {
                %85 = arith.index_cast %arg6 : index to i32
                %86 = arith.addi %85, %c-1_i32 : i32
                %87 = arith.index_cast %86 : i32 to index
                %88 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
                %89 = memref.load %88[%82, %87] : memref<?x?xf32>
                %90 = arith.extf %89 : f32 to f64
                %91 = arith.mulf %90, %cst_7 : f64
                %92 = memref.load %88[%82, %arg6] : memref<?x?xf32>
                %93 = arith.extf %92 : f32 to f64
                %94 = arith.mulf %93, %cst_6 : f64
                %95 = arith.addf %91, %94 : f64
                %96 = arith.addi %85, %c1_i32 : i32
                %97 = arith.index_cast %96 : i32 to index
                %98 = memref.load %88[%82, %97] : memref<?x?xf32>
                %99 = arith.extf %98 : f32 to f64
                %100 = arith.mulf %99, %cst_5 : f64
                %101 = arith.addf %95, %100 : f64
                %102 = memref.load %88[%79, %87] : memref<?x?xf32>
                %103 = arith.extf %102 : f32 to f64
                %104 = arith.mulf %103, %cst_4 : f64
                %105 = arith.addf %101, %104 : f64
                %106 = memref.load %88[%79, %arg6] : memref<?x?xf32>
                %107 = arith.extf %106 : f32 to f64
                %108 = arith.mulf %107, %cst_3 : f64
                %109 = arith.addf %105, %108 : f64
                %110 = memref.load %88[%79, %97] : memref<?x?xf32>
                %111 = arith.extf %110 : f32 to f64
                %112 = arith.mulf %111, %cst_2 : f64
                %113 = arith.addf %109, %112 : f64
                %114 = memref.load %88[%84, %87] : memref<?x?xf32>
                %115 = arith.extf %114 : f32 to f64
                %116 = arith.mulf %115, %cst_1 : f64
                %117 = arith.addf %113, %116 : f64
                %118 = memref.load %88[%84, %arg6] : memref<?x?xf32>
                %119 = arith.extf %118 : f32 to f64
                %120 = arith.mulf %119, %cst_0 : f64
                %121 = arith.addf %117, %120 : f64
                %122 = memref.load %88[%84, %97] : memref<?x?xf32>
                %123 = arith.extf %122 : f32 to f64
                %124 = arith.mulf %123, %cst : f64
                %125 = arith.addf %121, %124 : f64
                %126 = arith.truncf %125 : f64 to f32
                %c1_35 = arith.constant 1 : index
                %127 = arith.maxui %67, %c1_35 : index
                %c0_36 = arith.constant 0 : index
                %c1_37 = arith.constant 1 : index
                %c-1 = arith.constant -1 : index
                %128 = arith.addi %18, %75 : index
                %129 = arith.divui %128, %127 : index
                %130 = arith.subi %arg5, %75 : index
                %131 = arith.addi %75, %130 : index
                %132 = arith.subi %129, %72 : index
                %133 = arts.db_ref %arg4[%c0_30] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
                memref.store %126, %133[%arg5, %arg6] : memref<?x?xf32>
              } {arts.id = 101 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1022 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:104:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:104:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?x?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf32>>
          }
        }
      }
    } : i64
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %15 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%15, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_9, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c1024 step %c1 {
      %c1_23 = arith.constant 1 : index
      %18 = arith.maxui %7, %c1_23 : index
      %19 = arith.divui %arg2, %18 : index
      %20 = arith.subi %19, %c0_22 : index
      %21 = arith.remui %arg2, %18 : index
      %22 = arts.db_ref %ptr_20[%20] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
      %23 = memref.load %22[%21, %arg2] : memref<?x?xf32>
      %24 = arith.extf %23 : f32 to f64
      %25 = memref.load %alloca[] : memref<f64>
      %26 = arith.addf %25, %24 : f64
      memref.store %26, %alloca[] : memref<f64>
    } {arts.id = 98 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "convolution-2d.c:113:3">, arts.metadata_provenance = "recovered"}
    %16 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%16) : (f64) -> ()
    call @carts_phase_timer_stop(%15) : (memref<?xi8>) -> ()
    %17 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%17, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%17) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?xmemref<?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf32>>
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
