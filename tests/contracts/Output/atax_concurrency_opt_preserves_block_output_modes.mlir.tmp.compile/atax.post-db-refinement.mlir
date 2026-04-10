module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("atax\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c3999_i64 = arith.constant 3999 : i64
    %c3999 = arith.constant 3999 : index
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
    %7 = arts.runtime_query <total_workers> -> i32
    %8 = arith.index_castui %7 : i32 to index
    %9 = arith.maxui %8, %c1 : index
    %10 = arith.addi %9, %c3999 : index
    %11 = arith.divui %10, %9 : index
    %12 = arith.maxui %11, %c1 : index
    %13 = arith.maxui %12, %c1 : index
    %14 = arith.index_cast %13 : index to i64
    %15 = arith.addi %14, %c3999_i64 : i64
    %16 = arith.divui %15, %14 : i64
    %17 = arith.index_cast %16 : i64 to index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%17] elementType(f64) elementSizes[%13, %c4000] {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "atax.c:87:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 128000000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 33 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf64>>) block_shape[%13] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    %guid_2, %ptr_3 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%c2] elementType(f64) elementSizes[%c2000] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:89:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 90 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_3 : memref<?xmemref<?xf64>>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    %guid_4, %ptr_5 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%c2] elementType(f64) elementSizes[%c2000] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:90:20", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 36 : i64, lastUseId = 91 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.lowering_contract(%ptr_5 : memref<?xmemref<?xf64>>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %23 = arith.index_cast %arg2 : index to i32
      %24 = arith.sitofp %23 : i32 to f64
      scf.for %arg3 = %c0 to %c4000 step %c1 {
        %25 = arith.index_cast %arg3 : index to i32
        %26 = arith.addi %25, %c1_i32 : i32
        %27 = arith.sitofp %26 : i32 to f64
        %28 = arith.mulf %24, %27 : f64
        %29 = arith.divf %28, %cst_0 : f64
        %30 = arith.maxui %13, %c1 : index
        %31 = arith.divui %arg2, %30 : index
        %32 = arith.remui %arg2, %30 : index
        %33 = arts.db_ref %ptr[%31] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %29, %33[%32, %arg3] : memref<?x?xf64>
      } {arts.id = 47 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %18 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %23 = arith.muli %arg2, %c2000 : index
        %24 = arith.cmpi uge, %23, %c4000 : index
        %25 = arith.subi %c4000, %23 : index
        %26 = arith.select %24, %c0, %25 : index
        %27 = arith.minui %26, %c2000 : index
        %28 = arith.cmpi ugt, %27, %c0 : index
        scf.if %28 {
          %29 = arith.divui %23, %c2000 : index
          %30 = arith.cmpi ugt, %29, %c1 : index
          %31 = arith.select %30, %c0, %29 : index
          %guid_6, %ptr_7 = arts.db_acquire[<out>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%23], sizes[%c2000]), offsets[%31], sizes[%c1] element_offsets[%23] element_sizes[%c2000] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_7 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
          %32 = arith.addi %23, %27 : index
          %33 = arith.cmpi uge, %23, %c1 : index
          %34 = arith.subi %23, %c1 : index
          %35 = arith.select %33, %34, %c0 : index
          %36 = arith.addi %32, %c1 : index
          %37 = arith.minui %36, %c4000 : index
          %38 = arith.cmpi uge, %37, %35 : index
          %39 = arith.subi %37, %35 : index
          %40 = arith.select %38, %39, %c0 : index
          %41 = arith.maxui %13, %c1 : index
          %42 = arith.divui %35, %41 : index
          %43 = arith.addi %35, %40 : index
          %44 = arith.subi %43, %c1 : index
          %45 = arith.divui %44, %41 : index
          %46 = arith.subi %17, %c1 : index
          %47 = arith.cmpi ugt, %42, %46 : index
          %48 = arith.minui %45, %46 : index
          %49 = arith.select %47, %45, %48 : index
          %50 = arith.subi %49, %42 : index
          %51 = arith.addi %50, %c1 : index
          %52 = arith.select %47, %c0, %42 : index
          %53 = arith.select %47, %c0, %51 : index
          %guid_8, %ptr_9 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%35], sizes[%40]), offsets[%52], sizes[%53] element_offsets[%35] element_sizes[%40] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000, 4000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_9 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
          %guid_10, %ptr_11 = arts.db_acquire[<out>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%23], sizes[%c2000]), offsets[%31], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_11 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_7, %ptr_9, %ptr_11) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 84 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000, 4000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %54 = arith.subi %c0, %23 : index
            %55 = arith.cmpi slt, %54, %c0 : index
            %56 = arith.select %55, %c0, %54 : index
            %57 = arith.cmpi slt, %25, %c0 : index
            %58 = arith.select %57, %c0, %25 : index
            %59 = arith.minui %58, %27 : index
            scf.for %arg6 = %56 to %59 step %c1 {
              %60 = arith.addi %23, %arg6 : index
              %61 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %61[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %62 = memref.load %61[%arg6] : memref<?xf64>
                %63 = arith.maxui %41, %c1 : index
                %64 = arith.divui %60, %63 : index
                %65 = arith.subi %64, %52 : index
                %66 = arith.remui %60, %63 : index
                %67 = arts.db_ref %arg4[%65] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %68 = memref.load %67[%66, %arg7] : memref<?x?xf64>
                %69 = arith.index_cast %arg7 : index to i32
                %70 = arith.sitofp %69 : i32 to f64
                %71 = arith.mulf %70, %cst : f64
                %72 = arith.mulf %68, %71 : f64
                %73 = arith.addf %62, %72 : f64
                memref.store %73, %61[%arg6] : memref<?xf64>
              } {arts.id = 60 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 82 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?xf64>>
          }
        }
      }
    } : i64
    %19 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %23 = arith.muli %arg2, %c2000 : index
        %24 = arith.cmpi uge, %23, %c4000 : index
        %25 = arith.subi %c4000, %23 : index
        %26 = arith.select %24, %c0, %25 : index
        %27 = arith.minui %26, %c2000 : index
        %28 = arith.cmpi ugt, %27, %c0 : index
        scf.if %28 {
          %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_4 : memref<?xi64>, %ptr_5 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%23], sizes[%c2000]), offsets[%c0], sizes[%c2] {arts.id = 86 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_7 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
          %29 = arith.addi %23, %27 : index
          %30 = arith.cmpi uge, %23, %c1 : index
          %31 = arith.subi %23, %c1 : index
          %32 = arith.select %30, %31, %c0 : index
          %33 = arith.addi %29, %c1 : index
          %34 = arith.minui %33, %c4000 : index
          %35 = arith.cmpi uge, %34, %32 : index
          %36 = arith.subi %34, %32 : index
          %37 = arith.select %35, %36, %c0 : index
          %guid_8, %ptr_9 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>, offsets[%32], sizes[%37]), offsets[%c0], sizes[%17] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [4000, 2000], stencil_owner_dims = [1]} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
          arts.lowering_contract(%ptr_9 : memref<?xmemref<?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c4000] contract(<ownerDims = [1], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = false, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
          %38 = arith.divui %23, %c2000 : index
          %39 = arith.cmpi ugt, %38, %c1 : index
          %40 = arith.select %39, %c0, %38 : index
          %guid_10, %ptr_11 = arts.db_acquire[<out>] (%guid_2 : memref<?xi64>, %ptr_3 : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[%23], sizes[%c2000]), offsets[%40], sizes[%c1] element_offsets[%23] element_sizes[%c2000] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [2000], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
          arts.lowering_contract(%ptr_11 : memref<?xmemref<?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c2000] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_7, %ptr_9, %ptr_11) : memref<?xmemref<?xf64>>, memref<?xmemref<?x?xf64>>, memref<?xmemref<?xf64>> attributes {arts.id = 85 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [2000]} {
          ^bb0(%arg3: memref<?xmemref<?xf64>>, %arg4: memref<?xmemref<?x?xf64>>, %arg5: memref<?xmemref<?xf64>>):
            %41 = arith.maxui %13, %c1 : index
            %42 = arith.subi %c0, %23 : index
            %43 = arith.cmpi slt, %42, %c0 : index
            %44 = arith.select %43, %c0, %42 : index
            %45 = arith.cmpi slt, %25, %c0 : index
            %46 = arith.select %45, %c0, %25 : index
            %47 = arith.minui %46, %27 : index
            scf.for %arg6 = %44 to %47 step %c1 {
              %48 = arith.addi %23, %arg6 : index
              %49 = arts.db_ref %arg5[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
              memref.store %cst_1, %49[%arg6] : memref<?xf64>
              scf.for %arg7 = %c0 to %c4000 step %c1 {
                %50 = memref.load %49[%arg6] : memref<?xf64>
                %51 = arith.maxui %41, %c1 : index
                %52 = arith.divui %arg7, %51 : index
                %53 = arith.remui %arg7, %51 : index
                %54 = arts.db_ref %arg4[%52] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
                %55 = memref.load %54[%53, %48] : memref<?x?xf64>
                %56 = arith.divui %arg7, %c2000 : index
                %57 = arith.remui %arg7, %c2000 : index
                %58 = arts.db_ref %arg3[%56] : memref<?xmemref<?xf64>> -> memref<?xf64>
                %59 = memref.load %58[%57] : memref<?xf64>
                %60 = arith.mulf %55, %59 : f64
                %61 = arith.addf %50, %60 : f64
                memref.store %61, %49[%arg6] : memref<?xf64>
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
    %20 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%20, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_1, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c4000 step %c1 {
      %23 = arith.divui %arg2, %c2000 : index
      %24 = arith.remui %arg2, %c2000 : index
      %25 = arts.db_ref %ptr_3[%23] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %26 = memref.load %25[%24] : memref<?xf64>
      %27 = memref.load %alloca[] : memref<f64>
      %28 = arith.addf %27, %26 : f64
      memref.store %28, %alloca[] : memref<f64>
    } {arts.id = 78 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:111:3">, arts.metadata_provenance = "recovered"}
    %21 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%21) : (f64) -> ()
    call @carts_phase_timer_stop(%20) : (memref<?xi8>) -> ()
    %22 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%22, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%22) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_4) : memref<?xi64>
    arts.db_free(%ptr_5) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_2) : memref<?xi64>
    arts.db_free(%ptr_3) : memref<?xmemref<?xf64>>
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
