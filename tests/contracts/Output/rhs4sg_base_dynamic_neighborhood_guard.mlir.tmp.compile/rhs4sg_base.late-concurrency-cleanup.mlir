module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c64 = arith.constant 64 : index
    %c9 = arith.constant 9 : index
    %c316 = arith.constant 316 : index
    %cst = arith.constant 5.000000e-01 : f32
    %c-2 = arith.constant -2 : index
    %c2_i32 = arith.constant 2 : i32
    %c4 = arith.constant 4 : index
    %c2 = arith.constant 2 : index
    %c10 = arith.constant 10 : index
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
    %c1_i32 = arith.constant 1 : i32
    %cst_7 = arith.constant 3.000000e+00 : f32
    %cst_8 = arith.constant 1.000000e-03 : f32
    %c11_i32 = arith.constant 11 : i32
    %cst_9 = arith.constant 2.000000e+00 : f32
    %cst_10 = arith.constant 1.500000e-03 : f32
    %c7_i32 = arith.constant 7 : i32
    %c576 = arith.constant 576 : index
    %c320 = arith.constant 320 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c3 = arith.constant 3 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_11 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 253 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:158:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 253 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c320, %c320, %c576] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:116:17", totalAccesses = 16 : i64, readCount = 15 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 707788800 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 64 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %guid_12, %ptr_13 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c320, %c320, %c576] {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:117:19", totalAccesses = 5 : i64, readCount = 1 : i64, writeCount = 4 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 707788800 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 37 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %guid_14, %ptr_15 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c320, %c320, %c576] {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:119:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 235929600 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %guid_16, %ptr_17 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c320, %c320, %c576] {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:120:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 235929600 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 41 : i64, lastUseId = 42 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 41 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_18 = memref.alloca() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:144:3", totalAccesses = 11 : i64, readCount = 5 : i64, writeCount = 6 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 44 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_18[] : memref<i32>
    memref.store %c0_i32, %alloca_18[] : memref<i32>
    %8 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    %9 = arts.db_ref %ptr_13[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %15 = arith.index_cast %arg0 : index to i32
      memref.store %c0_i32, %alloca_18[] : memref<i32>
      %16 = arith.muli %15, %c17_i32 : i32
      %17 = arith.sitofp %15 : i32 to f32
      %18 = arith.mulf %17, %cst_5 : f32
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c320 step %c1 {
          scf.for %arg3 = %c0 to %c576 step %c1 {
            %19 = memref.load %alloca_18[] : memref<i32>
            %20 = arith.addi %19, %16 : i32
            %21 = arith.remsi %20, %c23_i32 : i32
            %22 = arith.sitofp %21 : i32 to f32
            %23 = arith.mulf %22, %cst_4 : f32
            %24 = arith.subf %23, %18 : f32
            memref.store %24, %8[%arg0, %arg1, %arg2, %arg3] : memref<?x?x?x?xf32>
            memref.store %cst_6, %9[%arg0, %arg1, %arg2, %arg3] : memref<?x?x?x?xf32>
            %25 = memref.load %alloca_18[] : memref<i32>
            %26 = arith.addi %25, %c1_i32 : i32
            memref.store %26, %alloca_18[] : memref<i32>
          } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 70 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    memref.store %c0_i32, %alloca_18[] : memref<i32>
    %10 = arts.db_ref %ptr_15[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
    %11 = arts.db_ref %ptr_17[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
    scf.for %arg0 = %c0 to %c320 step %c1 {
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c576 step %c1 {
          %15 = memref.load %alloca_18[] : memref<i32>
          %16 = arith.remsi %15, %c11_i32 : i32
          %17 = arith.sitofp %16 : i32 to f32
          %18 = arith.mulf %17, %cst_8 : f32
          %19 = arith.addf %18, %cst_7 : f32
          memref.store %19, %10[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %20 = memref.load %alloca_18[] : memref<i32>
          %21 = arith.remsi %20, %c7_i32 : i32
          %22 = arith.sitofp %21 : i32 to f32
          %23 = arith.mulf %22, %cst_10 : f32
          %24 = arith.addf %23, %cst_9 : f32
          memref.store %24, %11[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %25 = memref.load %alloca_18[] : memref<i32>
          %26 = arith.addi %25, %c1_i32 : i32
          memref.store %26, %alloca_18[] : memref<i32>
        } {arts.id = 88 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 90 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %guid_19, %ptr_20 = arts.db_acquire[<in>] (%guid_14 : memref<?xi64>, %ptr_15 : memref<?xmemref<?x?x?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [320, 320, 9], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
      %guid_21, %ptr_22 = arts.db_acquire[<in>] (%guid_16 : memref<?xi64>, %ptr_17 : memref<?xmemref<?x?x?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [320, 320, 9], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
      %guid_23, %ptr_24 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?x?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
      %15 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 300660904375 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "full_timestep", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
        scf.for %arg1 = %c0 to %c64 step %c1 {
          %16 = arith.muli %arg1, %c9 : index
          %17 = arith.cmpi uge, %16, %c572 : index
          %18 = arith.subi %c572, %16 : index
          %19 = arith.select %17, %c0, %18 : index
          %20 = arith.minui %19, %c9 : index
          %21 = arith.cmpi ugt, %20, %c0 : index
          scf.if %21 {
            arts.lowering_contract(%ptr_20 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c320] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = false, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
            arts.lowering_contract(%ptr_22 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c320] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = false, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
            arts.lowering_contract(%ptr_24 : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = false, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
            %22 = arith.divui %16, %c3 : index
            %23 = arith.addi %16, %c8 : index
            %24 = arith.divui %23, %c3 : index
            %25 = arith.cmpi ugt, %22, %c0 : index
            %26 = arith.select %25, %24, %c0 : index
            %27 = arith.subi %26, %22 : index
            %28 = arith.addi %27, %c1 : index
            %29 = arith.select %25, %c0, %22 : index
            %30 = arith.select %25, %c0, %28 : index
            %guid_25, %ptr_26 = arts.db_acquire[<inout>] (%guid_12 : memref<?xi64>, %ptr_13 : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%16], sizes[%c9]), offsets[%29], sizes[%30] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [3, 320, 320, 9], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
            arts.lowering_contract(%ptr_26 : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = false, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
            arts.edt <task> <intranode> route(%c-1_i32) (%ptr_20, %ptr_22, %ptr_24, %ptr_26) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>> attributes {arts.id = 261 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 300660904375 : i64, arts.plan.cost.relaunch_amortization = 1000 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "full_timestep", critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9]} {
            ^bb0(%arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?x?xf32>>, %arg5: memref<?xmemref<?x?x?x?xf32>>):
              %alloca_27 = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_28 = memref.alloca() {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 103 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_29 = memref.alloca() {arts.id = 94 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 94 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
              %alloca_30 = memref.alloca() {arts.id = 109 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 109 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_31 = memref.alloca() {arts.id = 115 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 115 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_32 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_33 = memref.alloca() {arts.id = 101 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 101 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_34 = memref.alloca() {arts.id = 105 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 105 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              %alloca_35 = memref.alloca() {arts.id = 113 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 113 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
              memref.store %cst_0, %alloca_29[%c0] : memref<5xf32>
              memref.store %cst_1, %alloca_29[%c1] : memref<5xf32>
              memref.store %cst_6, %alloca_29[%c2] : memref<5xf32>
              memref.store %cst_2, %alloca_29[%c3] : memref<5xf32>
              memref.store %cst_3, %alloca_29[%c4] : memref<5xf32>
              %31 = "polygeist.undef"() : () -> f32
              %32 = arith.subi %c4, %16 : index
              %33 = arith.cmpi slt, %32, %c0 : index
              %34 = arith.select %33, %c0, %32 : index
              %35 = arith.cmpi slt, %18, %c0 : index
              %36 = arith.select %35, %c0, %18 : index
              %37 = arith.minui %36, %20 : index
              %38 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %39 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
              %40 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
              %41 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
              scf.for %arg6 = %34 to %37 step %c1 {
                %42 = arith.addi %16, %arg6 : index
                memref.store %31, %alloca_33[] : memref<f32>
                memref.store %31, %alloca_28[] : memref<f32>
                memref.store %31, %alloca_34[] : memref<f32>
                memref.store %31, %alloca_27[] : memref<f32>
                memref.store %31, %alloca_30[] : memref<f32>
                memref.store %31, %alloca_32[] : memref<f32>
                memref.store %31, %alloca_35[] : memref<f32>
                memref.store %31, %alloca_31[] : memref<f32>
                %43 = arith.index_cast %42 : index to i32
                %44 = arith.addi %43, %c1_i32 : i32
                %45 = arith.index_cast %44 : i32 to index
                %46 = arith.addi %43, %c-1_i32 : i32
                %47 = arith.index_cast %46 : i32 to index
                scf.for %arg7 = %c4 to %c316 step %c1 {
                  %48 = arith.index_cast %arg7 : index to i32
                  %49 = arith.addi %48, %c1_i32 : i32
                  %50 = arith.index_cast %49 : i32 to index
                  %51 = arith.addi %48, %c-1_i32 : i32
                  %52 = arith.index_cast %51 : i32 to index
                  scf.for %arg8 = %c4 to %c316 step %c1 {
                    %53 = arith.index_cast %arg8 : index to i32
                    %54 = memref.load %38[%arg8, %arg7, %42] : memref<?x?x?xf32>
                    memref.store %54, %alloca_31[] : memref<f32>
                    %55 = memref.load %39[%arg8, %arg7, %42] : memref<?x?x?xf32>
                    memref.store %55, %alloca_35[] : memref<f32>
                    memref.store %cst_6, %alloca_32[] : memref<f32>
                    scf.for %arg9 = %c-2 to %c3 step %c1 {
                      %96 = arith.index_cast %arg9 : index to i32
                      %97 = arith.addi %96, %c2_i32 : i32
                      %98 = arith.index_cast %97 : i32 to index
                      %99 = memref.load %alloca_29[%98] : memref<5xf32>
                      %100 = arith.addi %53, %96 : i32
                      %101 = arith.index_cast %100 : i32 to index
                      %102 = memref.load %40[%c0, %101, %arg7, %42] : memref<?x?x?x?xf32>
                      %103 = arith.addi %48, %96 : i32
                      %104 = arith.index_cast %103 : i32 to index
                      %105 = memref.load %40[%c0, %arg8, %104, %42] : memref<?x?x?x?xf32>
                      %106 = arith.addf %102, %105 : f32
                      %107 = arith.addi %43, %96 : i32
                      %108 = arith.index_cast %107 : i32 to index
                      %109 = memref.load %40[%c0, %arg8, %arg7, %108] : memref<?x?x?x?xf32>
                      %110 = arith.addf %106, %109 : f32
                      %111 = arith.mulf %99, %110 : f32
                      %112 = memref.load %alloca_32[] : memref<f32>
                      %113 = arith.addf %112, %111 : f32
                      memref.store %113, %alloca_32[] : memref<f32>
                    } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                    %56 = arith.addi %53, %c1_i32 : i32
                    %57 = arith.index_cast %56 : i32 to index
                    %58 = memref.load %40[%c0, %57, %arg7, %42] : memref<?x?x?x?xf32>
                    %59 = arith.addi %53, %c-1_i32 : i32
                    %60 = arith.index_cast %59 : i32 to index
                    %61 = memref.load %40[%c0, %60, %arg7, %42] : memref<?x?x?x?xf32>
                    %62 = arith.subf %58, %61 : f32
                    memref.store %62, %alloca_30[] : memref<f32>
                    %63 = memref.load %alloca_31[] : memref<f32>
                    %64 = memref.load %alloca_32[] : memref<f32>
                    %65 = arith.mulf %63, %64 : f32
                    %66 = memref.load %alloca_35[] : memref<f32>
                    %67 = arith.addf %66, %63 : f32
                    %68 = memref.load %alloca_30[] : memref<f32>
                    %69 = arith.mulf %67, %68 : f32
                    %70 = arith.mulf %69, %cst : f32
                    %71 = arith.addf %65, %70 : f32
                    memref.store %71, %41[%c0, %arg8, %arg7, %42] : memref<?x?x?x?xf32>
                    memref.store %cst_6, %alloca_27[] : memref<f32>
                    scf.for %arg9 = %c-2 to %c3 step %c1 {
                      %96 = arith.index_cast %arg9 : index to i32
                      %97 = arith.addi %96, %c2_i32 : i32
                      %98 = arith.index_cast %97 : i32 to index
                      %99 = memref.load %alloca_29[%98] : memref<5xf32>
                      %100 = arith.addi %53, %96 : i32
                      %101 = arith.index_cast %100 : i32 to index
                      %102 = memref.load %40[%c1, %101, %arg7, %42] : memref<?x?x?x?xf32>
                      %103 = arith.addi %48, %96 : i32
                      %104 = arith.index_cast %103 : i32 to index
                      %105 = memref.load %40[%c1, %arg8, %104, %42] : memref<?x?x?x?xf32>
                      %106 = arith.addf %102, %105 : f32
                      %107 = arith.addi %43, %96 : i32
                      %108 = arith.index_cast %107 : i32 to index
                      %109 = memref.load %40[%c1, %arg8, %arg7, %108] : memref<?x?x?x?xf32>
                      %110 = arith.addf %106, %109 : f32
                      %111 = arith.mulf %99, %110 : f32
                      %112 = memref.load %alloca_27[] : memref<f32>
                      %113 = arith.addf %112, %111 : f32
                      memref.store %113, %alloca_27[] : memref<f32>
                    } {arts.id = 193 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                    %72 = memref.load %40[%c1, %arg8, %50, %42] : memref<?x?x?x?xf32>
                    %73 = memref.load %40[%c1, %arg8, %52, %42] : memref<?x?x?x?xf32>
                    %74 = arith.subf %72, %73 : f32
                    memref.store %74, %alloca_34[] : memref<f32>
                    %75 = memref.load %alloca_31[] : memref<f32>
                    %76 = memref.load %alloca_27[] : memref<f32>
                    %77 = arith.mulf %75, %76 : f32
                    %78 = memref.load %alloca_35[] : memref<f32>
                    %79 = arith.addf %78, %75 : f32
                    %80 = memref.load %alloca_34[] : memref<f32>
                    %81 = arith.mulf %79, %80 : f32
                    %82 = arith.mulf %81, %cst : f32
                    %83 = arith.addf %77, %82 : f32
                    memref.store %83, %41[%c1, %arg8, %arg7, %42] : memref<?x?x?x?xf32>
                    memref.store %cst_6, %alloca_28[] : memref<f32>
                    scf.for %arg9 = %c-2 to %c3 step %c1 {
                      %96 = arith.index_cast %arg9 : index to i32
                      %97 = arith.addi %96, %c2_i32 : i32
                      %98 = arith.index_cast %97 : i32 to index
                      %99 = memref.load %alloca_29[%98] : memref<5xf32>
                      %100 = arith.addi %53, %96 : i32
                      %101 = arith.index_cast %100 : i32 to index
                      %102 = memref.load %40[%c2, %101, %arg7, %42] : memref<?x?x?x?xf32>
                      %103 = arith.addi %48, %96 : i32
                      %104 = arith.index_cast %103 : i32 to index
                      %105 = memref.load %40[%c2, %arg8, %104, %42] : memref<?x?x?x?xf32>
                      %106 = arith.addf %102, %105 : f32
                      %107 = arith.addi %43, %96 : i32
                      %108 = arith.index_cast %107 : i32 to index
                      %109 = memref.load %40[%c2, %arg8, %arg7, %108] : memref<?x?x?x?xf32>
                      %110 = arith.addf %106, %109 : f32
                      %111 = arith.mulf %99, %110 : f32
                      %112 = memref.load %alloca_28[] : memref<f32>
                      %113 = arith.addf %112, %111 : f32
                      memref.store %113, %alloca_28[] : memref<f32>
                    } {arts.id = 229 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                    %84 = memref.load %40[%c2, %arg8, %arg7, %45] : memref<?x?x?x?xf32>
                    %85 = memref.load %40[%c2, %arg8, %arg7, %47] : memref<?x?x?x?xf32>
                    %86 = arith.subf %84, %85 : f32
                    memref.store %86, %alloca_33[] : memref<f32>
                    %87 = memref.load %alloca_31[] : memref<f32>
                    %88 = memref.load %alloca_28[] : memref<f32>
                    %89 = arith.mulf %87, %88 : f32
                    %90 = memref.load %alloca_35[] : memref<f32>
                    %91 = arith.addf %90, %87 : f32
                    %92 = memref.load %alloca_33[] : memref<f32>
                    %93 = arith.mulf %91, %92 : f32
                    %94 = arith.mulf %93, %cst : f32
                    %95 = arith.addf %89, %94 : f32
                    memref.store %95, %41[%c2, %arg8, %arg7, %42] : memref<?x?x?x?xf32>
                  } {arts.id = 245 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                } {arts.id = 247 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
              } {arts.id = 260 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
              arts.db_release(%arg2) : memref<?xmemref<?x?x?xf32>>
              arts.db_release(%arg3) : memref<?xmemref<?x?x?xf32>>
              arts.db_release(%arg4) : memref<?xmemref<?x?x?x?xf32>>
              arts.db_release(%arg5) : memref<?xmemref<?x?x?x?xf32>>
            }
          }
        }
      } : i64
      arts.db_release(%ptr_24) : memref<?xmemref<?x?x?x?xf32>>
      arts.db_release(%ptr_22) : memref<?xmemref<?x?x?xf32>>
      arts.db_release(%ptr_20) : memref<?xmemref<?x?x?xf32>>
      func.call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 9 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:149:3">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%12, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_11, %alloca[] : memref<f64>
    scf.for %arg0 = %c0 to %c320 step %c1 {
      scf.for %arg1 = %c0 to %c3 step %c1 {
        %15 = memref.load %9[%arg1, %arg0, %arg0, %arg0] : memref<?x?x?x?xf32>
        %16 = arith.extf %15 : f32 to f64
        %17 = math.absf %16 : f64
        %18 = memref.load %alloca[] : memref<f64>
        %19 = arith.addf %18, %17 : f64
        memref.store %19, %alloca[] : memref<f64>
      } {arts.id = 258 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:163:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 212 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:162:3">, arts.metadata_provenance = "recovered"}
    %13 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%13) : (f64) -> ()
    call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> ()
    %14 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%14, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%14) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_12) : memref<?xi64>
    arts.db_free(%ptr_13) : memref<?xmemref<?x?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?x?xf32>>
    arts.db_free(%guid_14) : memref<?xi64>
    arts.db_free(%ptr_15) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_16) : memref<?xi64>
    arts.db_free(%ptr_17) : memref<?xmemref<?x?x?xf32>>
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
