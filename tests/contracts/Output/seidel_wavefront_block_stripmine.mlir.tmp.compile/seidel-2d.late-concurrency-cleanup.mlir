module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c9599_i64 = arith.constant 9599 : i64
    %c1200 = arith.constant 1200 : index
    %c149 = arith.constant 149 : index
    %cst = arith.constant 9.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c9599 = arith.constant 9599 : index
    %c960 = arith.constant 960 : index
    %c-1 = arith.constant -1 : index
    %c64 = arith.constant 64 : index
    %c10 = arith.constant 10 : index
    %c9 = arith.constant 9 : index
    %c148 = arith.constant 148 : index
    %c150 = arith.constant 150 : index
    %c2 = arith.constant 2 : index
    %c320 = arith.constant 320 : index
    %c9600 = arith.constant 9600 : index
    %c0 = arith.constant 0 : index
    %c9599_i32 = arith.constant 9599 : i32
    %cst_0 = arith.constant 9.600000e+03 : f64
    %c-1_i32 = arith.constant -1 : i32
    %cst_1 = arith.constant 2.000000e+00 : f64
    %c1 = arith.constant 1 : index
    %false = arith.constant false
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_2 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %c2_i32 = arith.constant 2 : i32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %4 = "polygeist.undef"() : () -> i32
    %alloca = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:62:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %5 = "polygeist.undef"() : () -> f64
    memref.store %5, %alloca[] : memref<f64>
    %alloca_3 = memref.alloca() {arts.id = 64 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:48:25", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 64 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %4, %alloca_3[] : memref<i32>
    %alloca_4 = memref.alloca() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:20:1", totalAccesses = 6 : i64, readCount = 2 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, firstUseId = 35 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true>, arts.metadata_provenance = "exact"} : memref<i1>
    memref.store %true, %alloca_4[] : memref<i1>
    call @carts_benchmarks_start() : () -> ()
    %6 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%6) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %6) : (memref<?xi8>, memref<?xi8>) -> ()
    %8 = arts.runtime_query <total_workers> -> i32
    %9 = arith.index_castui %8 : i32 to index
    %10 = arith.maxui %9, %c1 : index
    %11 = arith.minui %10, %c1200 : index
    %12 = arith.addi %11, %c9599 : index
    %13 = arith.divui %12, %11 : index
    %14 = arith.maxui %13, %c1 : index
    %15 = arith.maxui %14, %c1 : index
    %16 = arith.index_cast %15 : index to i64
    %17 = arith.addi %16, %c9599_i64 : i64
    %18 = arith.divui %17, %16 : i64
    %19 = arith.index_cast %18 : i64 to index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>, <stencil>] route(%c-1_i32 : i32) sizes[%19] elementType(f64) elementSizes[%15, %c9600] {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "seidel-2d.c:30:19", totalAccesses = 12 : i64, readCount = 10 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 737280000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = false, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 96 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%15] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    %20 = arith.maxui %15, %c1 : index
    scf.for %arg2 = %c0 to %c9600 step %c1 {
      %23 = arith.index_cast %arg2 : index to i32
      %24 = arith.sitofp %23 : i32 to f64
      %25 = arith.divui %arg2, %20 : index
      %26 = arith.remui %arg2, %20 : index
      %27 = arts.db_ref %ptr[%25] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      scf.for %arg3 = %c0 to %c9600 step %c1 {
        %28 = arith.index_cast %arg3 : index to i32
        %29 = arith.addi %28, %c2_i32 : i32
        %30 = arith.sitofp %29 : i32 to f64
        %31 = arith.mulf %24, %30 : f64
        %32 = arith.addf %31, %cst_1 : f64
        %33 = arith.divf %32, %cst_0 : f64
        memref.store %33, %27[%26, %arg3] : memref<?x?xf64>
      } {arts.id = 42 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:5">, arts.metadata_provenance = "recovered"}
    } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%6) : (memref<?xi8>) -> ()
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %23 = memref.load %alloca_4[] : memref<i1>
      scf.if %23 {
        memref.store %c9599_i32, %alloca_3[] : memref<i32>
        %24 = memref.load %alloca_3[] : memref<i32>
        %25 = arith.index_cast %24 : i32 to index
        %26 = arith.addi %25, %c148 : index
        %27 = arith.divui %26, %c150 : index
        %28 = arith.subi %27, %c1 : index
        %29 = arith.muli %28, %c2 : index
        %30 = arith.addi %29, %c10 : index
        %31 = arts.epoch {
          scf.for %arg3 = %c0 to %30 step %c1 {
            %32 = arith.cmpi uge, %arg3, %c9 : index
            %33 = arith.subi %arg3, %c9 : index
            %34 = arith.select %32, %33, %c0 : index
            %35 = arith.addi %34, %c1 : index
            %36 = arith.divui %35, %c2 : index
            %37 = arith.divui %arg3, %c2 : index
            %38 = arith.addi %37, %c1 : index
            %39 = arith.minui %38, %27 : index
            %40 = arith.muli %36, %c150 : index
            %41 = arith.muli %39, %c150 : index
            %42 = arith.subi %41, %40 : index
            %43 = arith.addi %42, %c149 : index
            %44 = arith.divui %43, %c150 : index
            %45 = arith.minui %44, %c64 : index
            %46 = arith.maxui %45, %c1 : index
            %47 = arith.divui %44, %c64 : index
            %48 = arith.remui %44, %c64 : index
            %49 = arith.addi %47, %c1 : index
            scf.for %arg4 = %c0 to %46 step %c1 {
              %50 = arith.cmpi ult, %arg4, %48 : index
              %51 = arith.select %50, %49, %47 : index
              %52 = arith.minui %arg4, %48 : index
              %53 = arith.muli %arg4, %47 : index
              %54 = arith.addi %53, %52 : index
              %55 = arith.cmpi uge, %54, %44 : index
              %56 = arith.subi %44, %54 : index
              %57 = arith.select %55, %c0, %56 : index
              %58 = arith.minui %51, %57 : index
              %59 = arith.cmpi ugt, %58, %c0 : index
              scf.if %59 {
                %60 = arith.muli %54, %c150 : index
                %61 = arith.addi %40, %60 : index
                %62 = arith.subi %41, %61 : index
                %63 = arith.cmpi slt, %62, %c0 : index
                %64 = arith.select %63, %c0, %62 : index
                %65 = arith.addi %64, %c149 : index
                %66 = arith.divui %65, %c150 : index
                %67 = arith.minui %66, %58 : index
                %68 = arith.maxui %67, %c1 : index
                %69 = arith.divui %61, %20 : index
                %70 = arith.addi %61, %68 : index
                %71 = arith.subi %70, %c1 : index
                %72 = arith.divui %71, %20 : index
                %73 = arith.subi %19, %c1 : index
                %74 = arith.cmpi ugt, %69, %73 : index
                %75 = arith.minui %72, %73 : index
                %76 = arith.select %74, %72, %75 : index
                %77 = arith.subi %76, %69 : index
                %78 = arith.addi %77, %c1 : index
                %79 = arith.select %74, %c0, %69 : index
                %80 = arith.select %74, %c0, %78 : index
                %guid_5, %ptr_6 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), offsets[%79], sizes[%80] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [150, 960], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
                arts.lowering_contract(%ptr_6 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <wavefront_2d>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c150] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] contract(<ownerDims = [0], centerOffset = 1 : i64, spatialDims = [0, 1], supportedBlockHalo = true, postDbRefined = true, criticalPathDistance = 0 : i64, contractKind = 1 : i64>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
                arts.edt <task> <intranode> route(%c-1_i32) (%ptr_6) : memref<?xmemref<?x?xf64>> attributes {arts.pattern_revision = 1 : i64, arts.plan.async_strategy = "cps_chain", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 700 : i64, arts.plan.cost.slice_widening_pressure = 4000 : i64, arts.plan.halo_shape = [2, 2], arts.plan.iteration_topology = "wavefront", arts.plan.kernel_family = "wavefront", arts.plan.owner_dims = [0, 1], arts.plan.physical_block_shape = [150, 960], arts.plan.repetition_structure = "full_timestep", critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]} {
                ^bb0(%arg5: memref<?xmemref<?x?xf64>>):
                  %81 = arith.subi %40, %61 : index
                  %82 = arith.cmpi slt, %81, %c0 : index
                  %83 = arith.select %82, %c0, %81 : index
                  %84 = arith.addi %83, %c149 : index
                  %85 = arith.divui %84, %c150 : index
                  %86 = arith.maxui %20, %c1 : index
                  %87 = arith.cmpi ugt, %86, %c2 : index
                  scf.for %arg6 = %85 to %67 step %c1 {
                    %88 = arith.muli %arg6, %c150 : index
                    %89 = arith.addi %61, %88 : index
                    %90 = arith.divui %89, %c150 : index
                    %91 = arith.muli %90, %c2 : index
                    %92 = arith.subi %arg3, %91 : index
                    %93 = arith.maxui %89, %c1 : index
                    %94 = arith.addi %89, %c150 : index
                    %95 = arith.minui %94, %25 : index
                    %96 = arith.muli %92, %c960 : index
                    %97 = arith.maxui %96, %c1 : index
                    %98 = arith.addi %96, %c960 : index
                    %99 = arith.minui %98, %c9599 : index
                    scf.if %87 {
                      %100 = arith.subi %86, %c1 : index
                      %101 = arith.divui %93, %86 : index
                      %102 = arith.addi %95, %100 : index
                      %103 = arith.divui %102, %86 : index
                      %104 = arith.cmpi ugt, %95, %93 : index
                      %105 = arith.select %104, %103, %101 : index
                      scf.for %arg7 = %101 to %105 step %c1 {
                        %106 = arith.muli %arg7, %86 : index
                        %107 = arith.cmpi uge, %106, %93 : index
                        %108 = arith.subi %93, %106 : index
                        %109 = arith.select %107, %c0, %108 : index
                        %110 = arith.minui %109, %86 : index
                        %111 = arith.cmpi uge, %106, %95 : index
                        %112 = arith.subi %95, %106 : index
                        %113 = arith.select %111, %c0, %112 : index
                        %114 = arith.minui %113, %86 : index
                        %115 = arith.minui %114, %c1 : index
                        %116 = arith.addi %arg7, %c-1 : index
                        %117 = arith.subi %arg7, %79 : index
                        %118 = arith.subi %116, %79 : index
                        %119 = arts.db_ref %arg5[%117] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                        %120 = arts.db_ref %arg5[%118] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                        scf.for %arg8 = %110 to %115 step %c1 {
                          %128 = arith.addi %arg8, %c-1 : index
                          %129 = arith.addi %128, %86 : index
                          %130 = arith.addi %arg8, %c1 : index
                          scf.for %arg9 = %97 to %99 step %c1 {
                            %131 = arith.index_cast %arg9 : index to i32
                            %132 = arith.addi %131, %c-1_i32 : i32
                            %133 = arith.index_cast %132 : i32 to index
                            %134 = memref.load %120[%129, %133] : memref<?x?xf64>
                            %135 = memref.load %120[%129, %arg9] : memref<?x?xf64>
                            %136 = arith.addf %134, %135 : f64
                            %137 = arith.addi %131, %c1_i32 : i32
                            %138 = arith.index_cast %137 : i32 to index
                            %139 = memref.load %120[%129, %138] : memref<?x?xf64>
                            %140 = arith.addf %136, %139 : f64
                            %141 = memref.load %119[%arg8, %133] : memref<?x?xf64>
                            %142 = arith.addf %140, %141 : f64
                            %143 = memref.load %119[%arg8, %arg9] : memref<?x?xf64>
                            %144 = arith.addf %142, %143 : f64
                            %145 = memref.load %119[%arg8, %138] : memref<?x?xf64>
                            %146 = arith.addf %144, %145 : f64
                            %147 = memref.load %119[%130, %133] : memref<?x?xf64>
                            %148 = arith.addf %146, %147 : f64
                            %149 = memref.load %119[%130, %arg9] : memref<?x?xf64>
                            %150 = arith.addf %148, %149 : f64
                            %151 = memref.load %119[%130, %138] : memref<?x?xf64>
                            %152 = arith.addf %150, %151 : f64
                            %153 = arith.divf %152, %cst : f64
                            memref.store %153, %119[%arg8, %arg9] : memref<?x?xf64>
                          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
                        }
                        %121 = arith.maxui %110, %c1 : index
                        %122 = arith.minui %114, %100 : index
                        scf.for %arg8 = %121 to %122 step %c1 {
                          %128 = arith.addi %arg8, %c-1 : index
                          %129 = arith.addi %arg8, %c1 : index
                          scf.for %arg9 = %97 to %99 step %c1 {
                            %130 = arith.index_cast %arg9 : index to i32
                            %131 = arith.addi %130, %c-1_i32 : i32
                            %132 = arith.index_cast %131 : i32 to index
                            %133 = memref.load %119[%128, %132] : memref<?x?xf64>
                            %134 = memref.load %119[%128, %arg9] : memref<?x?xf64>
                            %135 = arith.addf %133, %134 : f64
                            %136 = arith.addi %130, %c1_i32 : i32
                            %137 = arith.index_cast %136 : i32 to index
                            %138 = memref.load %119[%128, %137] : memref<?x?xf64>
                            %139 = arith.addf %135, %138 : f64
                            %140 = memref.load %119[%arg8, %132] : memref<?x?xf64>
                            %141 = arith.addf %139, %140 : f64
                            %142 = memref.load %119[%arg8, %arg9] : memref<?x?xf64>
                            %143 = arith.addf %141, %142 : f64
                            %144 = memref.load %119[%arg8, %137] : memref<?x?xf64>
                            %145 = arith.addf %143, %144 : f64
                            %146 = memref.load %119[%129, %132] : memref<?x?xf64>
                            %147 = arith.addf %145, %146 : f64
                            %148 = memref.load %119[%129, %arg9] : memref<?x?xf64>
                            %149 = arith.addf %147, %148 : f64
                            %150 = memref.load %119[%129, %137] : memref<?x?xf64>
                            %151 = arith.addf %149, %150 : f64
                            %152 = arith.divf %151, %cst : f64
                            memref.store %152, %119[%arg8, %arg9] : memref<?x?xf64>
                          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
                        }
                        %123 = arith.maxui %110, %100 : index
                        %124 = arith.minui %114, %86 : index
                        %125 = arith.addi %arg7, %c1 : index
                        %126 = arith.subi %125, %79 : index
                        %127 = arts.db_ref %arg5[%126] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                        scf.for %arg8 = %123 to %124 step %c1 {
                          %128 = arith.addi %arg8, %c-1 : index
                          %129 = arith.addi %arg8, %c1 : index
                          %130 = arith.subi %129, %86 : index
                          scf.for %arg9 = %97 to %99 step %c1 {
                            %131 = arith.index_cast %arg9 : index to i32
                            %132 = arith.addi %131, %c-1_i32 : i32
                            %133 = arith.index_cast %132 : i32 to index
                            %134 = memref.load %119[%128, %133] : memref<?x?xf64>
                            %135 = memref.load %119[%128, %arg9] : memref<?x?xf64>
                            %136 = arith.addf %134, %135 : f64
                            %137 = arith.addi %131, %c1_i32 : i32
                            %138 = arith.index_cast %137 : i32 to index
                            %139 = memref.load %119[%128, %138] : memref<?x?xf64>
                            %140 = arith.addf %136, %139 : f64
                            %141 = memref.load %119[%arg8, %133] : memref<?x?xf64>
                            %142 = arith.addf %140, %141 : f64
                            %143 = memref.load %119[%arg8, %arg9] : memref<?x?xf64>
                            %144 = arith.addf %142, %143 : f64
                            %145 = memref.load %119[%arg8, %138] : memref<?x?xf64>
                            %146 = arith.addf %144, %145 : f64
                            %147 = memref.load %127[%130, %133] : memref<?x?xf64>
                            %148 = arith.addf %146, %147 : f64
                            %149 = memref.load %127[%130, %arg9] : memref<?x?xf64>
                            %150 = arith.addf %148, %149 : f64
                            %151 = memref.load %127[%130, %138] : memref<?x?xf64>
                            %152 = arith.addf %150, %151 : f64
                            %153 = arith.divf %152, %cst : f64
                            memref.store %153, %119[%arg8, %arg9] : memref<?x?xf64>
                          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
                        }
                      }
                    } else {
                      scf.for %arg7 = %93 to %95 step %c1 {
                        %100 = arith.index_cast %arg7 : index to i32
                        %101 = arith.addi %100, %c-1_i32 : i32
                        %102 = arith.index_cast %101 : i32 to index
                        %103 = arith.addi %100, %c1_i32 : i32
                        %104 = arith.index_cast %103 : i32 to index
                        %105 = arith.divui %102, %86 : index
                        %106 = arith.remui %102, %86 : index
                        %107 = arith.divui %arg7, %86 : index
                        %108 = arith.remui %arg7, %86 : index
                        %109 = arith.divui %104, %86 : index
                        %110 = arith.remui %104, %86 : index
                        %111 = arith.subi %105, %79 : index
                        %112 = arith.subi %107, %79 : index
                        %113 = arith.subi %109, %79 : index
                        %114 = arts.db_ref %arg5[%111] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                        %115 = arts.db_ref %arg5[%112] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                        %116 = arts.db_ref %arg5[%113] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                        scf.for %arg8 = %97 to %99 step %c1 {
                          %117 = arith.index_cast %arg8 : index to i32
                          %118 = arith.addi %117, %c-1_i32 : i32
                          %119 = arith.index_cast %118 : i32 to index
                          %120 = memref.load %114[%106, %119] : memref<?x?xf64>
                          %121 = memref.load %114[%106, %arg8] : memref<?x?xf64>
                          %122 = arith.addf %120, %121 : f64
                          %123 = arith.addi %117, %c1_i32 : i32
                          %124 = arith.index_cast %123 : i32 to index
                          %125 = memref.load %114[%106, %124] : memref<?x?xf64>
                          %126 = arith.addf %122, %125 : f64
                          %127 = memref.load %115[%108, %119] : memref<?x?xf64>
                          %128 = arith.addf %126, %127 : f64
                          %129 = memref.load %115[%108, %arg8] : memref<?x?xf64>
                          %130 = arith.addf %128, %129 : f64
                          %131 = memref.load %115[%108, %124] : memref<?x?xf64>
                          %132 = arith.addf %130, %131 : f64
                          %133 = memref.load %116[%110, %119] : memref<?x?xf64>
                          %134 = arith.addf %132, %133 : f64
                          %135 = memref.load %116[%110, %arg8] : memref<?x?xf64>
                          %136 = arith.addf %134, %135 : f64
                          %137 = memref.load %116[%110, %124] : memref<?x?xf64>
                          %138 = arith.addf %136, %137 : f64
                          %139 = arith.divf %138, %cst : f64
                          memref.store %139, %115[%108, %arg8] : memref<?x?xf64>
                        } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
                      } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
                    }
                  } {arts.id = 113 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "">, arts.metadata_provenance = "exact"}
                  arts.db_release(%arg5) : memref<?xmemref<?x?xf64>>
                }
              }
            } {arts.id = 55 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:49:7">, arts.metadata_provenance = "recovered"}
          } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
        } : i64
        memref.store %true, %alloca_4[] : memref<i1>
      }
    } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
    %21 = memref.load %alloca_4[] : memref<i1>
    %22 = arith.select %21, %c0_i32, %4 : i32
    scf.if %21 {
      func.call @carts_kernel_timer_stop(%6) : (memref<?xi8>) -> ()
      %23 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%23, %6) : (memref<?xi8>, memref<?xi8>) -> ()
      memref.store %cst_2, %alloca[] : memref<f64>
      scf.for %arg2 = %c0 to %c64 step %c1 {
        %26 = arith.muli %arg2, %c150 : index
        %27 = arith.subi %c9600, %26 : index
        %28 = arith.cmpi uge, %26, %c9600 : index
        %29 = arith.select %28, %c0, %27 : index
        %30 = arith.minui %29, %c150 : index
        %31 = arts.db_ref %ptr[%arg2] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        scf.for %arg3 = %c0 to %30 step %c1 {
          %32 = arith.addi %26, %arg3 : index
          %33 = memref.load %31[%arg3, %32] : memref<?x?xf64>
          %34 = memref.load %alloca[] : memref<f64>
          %35 = arith.addf %34, %33 : f64
          memref.store %35, %alloca[] : memref<f64>
        }
      }
      %24 = memref.load %alloca[] : memref<f64>
      func.call @carts_bench_checksum_d(%24) : (f64) -> ()
      func.call @carts_phase_timer_stop(%23) : (memref<?xi8>) -> ()
      %25 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%25, %6) : (memref<?xi8>, memref<?xi8>) -> ()
      func.call @carts_phase_timer_stop(%25) : (memref<?xi8>) -> ()
      func.call @carts_e2e_timer_stop() : () -> ()
      memref.store %false, %alloca_4[] : memref<i1>
    }
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf64>>
    return %22 : i32
  }
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>}
}
