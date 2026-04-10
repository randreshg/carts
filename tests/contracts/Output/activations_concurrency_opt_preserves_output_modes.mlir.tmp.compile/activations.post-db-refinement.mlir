module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for single-worker lowering checks.\0Aworker_threads=1\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 1 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant -1.702000e+00 : f32
    %cst_0 = arith.constant 1.000000e+00 : f32
    %cst_1 = arith.constant 5.000000e-01 : f32
    %cst_2 = arith.constant 0.797884583 : f32
    %cst_3 = arith.constant 4.471500e-02 : f32
    %cst_4 = arith.constant 1.000000e-01 : f32
    %cst_5 = arith.constant 0x49800000 : f32
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %cst_6 = arith.constant 1.000000e+02 : f32
    %c1048576 = arith.constant 1048576 : index
    %cst_7 = arith.constant 0.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %cst_8 = arith.constant 6.000000e+00 : f32
    %cst_9 = arith.constant -3.000000e+00 : f32
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %4 = "polygeist.undef"() : () -> f64
    %alloca = memref.alloca() {arts.id = 268 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:226:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 268 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca[] : memref<f64>
    %alloca_11 = memref.alloca() {arts.id = 258 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:223:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 258 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_11[] : memref<f64>
    %alloca_12 = memref.alloca() {arts.id = 248 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:220:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 248 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_12[] : memref<f64>
    %alloca_13 = memref.alloca() {arts.id = 238 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:217:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 238 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_13[] : memref<f64>
    %alloca_14 = memref.alloca() {arts.id = 228 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:214:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 228 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_14[] : memref<f64>
    %alloca_15 = memref.alloca() {arts.id = 218 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:211:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 218 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_15[] : memref<f64>
    %alloca_16 = memref.alloca() {arts.id = 208 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:208:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 208 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_16[] : memref<f64>
    %alloca_17 = memref.alloca() {arts.id = 198 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:205:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 198 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_17[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:171:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 41 : i64, lastUseId = 294 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 10 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 41 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_18, %ptr_19 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 42 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:172:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 42 : i64, lastUseId = 295 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 42 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_20, %ptr_21 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:173:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 43 : i64, lastUseId = 296 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 12 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 43 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_22, %ptr_23 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:174:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 44 : i64, lastUseId = 297 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 13 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 44 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_24, %ptr_25 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 45 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:175:26", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 45 : i64, lastUseId = 298 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 14 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 45 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_26, %ptr_27 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 46 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:176:24", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 46 : i64, lastUseId = 299 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 46 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_28, %ptr_29 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1048576] {arts.id = 47 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:177:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 47 : i64, lastUseId = 300 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 16 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 47 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %alloc = memref.alloc() {arts.id = 48 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:179:27", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 400 : i64, firstUseId = 48 : i64, lastUseId = 301 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<100xf32>
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_30 = memref.alloca() {arts.id = 168 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 168 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_31 = memref.alloca() {arts.id = 169 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 169 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %7 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      %guid_32, %ptr_33 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_33 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      %guid_34, %ptr_35 = arts.db_acquire[<out>] (%guid_18 : memref<?xi64>, %ptr_19 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_35 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      %guid_36, %ptr_37 = arts.db_acquire[<out>] (%guid_20 : memref<?xi64>, %ptr_21 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_37 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %28 = arith.index_cast %arg2 : index to i32
        %29 = arith.sitofp %28 : i32 to f32
        %30 = arith.divf %29, %cst_5 : f32
        %31 = arith.mulf %30, %cst_8 : f32
        %32 = arith.addf %31, %cst_9 : f32
        %33 = arith.cmpf ogt, %32, %cst_7 : f32
        %34 = arith.select %33, %32, %cst_7 : f32
        %35 = arts.db_ref %ptr_33[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %34, %35[%arg2] : memref<?xf32>
        %36 = scf.if %33 -> (f32) {
          scf.yield %32 : f32
        } else {
          %41 = arith.mulf %32, %cst_4 : f32
          scf.yield %41 : f32
        }
        %37 = arts.db_ref %ptr_35[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %36, %37[%arg2] : memref<?xf32>
        %38 = arith.cmpf olt, %34, %cst_8 : f32
        %39 = arith.select %38, %34, %cst_8 : f32
        %40 = arts.db_ref %ptr_37[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %39, %40[%arg2] : memref<?xf32>
      } {arts.id = 270 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:190:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_33) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_35) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_37) : memref<?xmemref<?xf32>>
      %guid_38, %ptr_39 = arts.db_acquire[<out>] (%guid_22 : memref<?xi64>, %ptr_23 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_39 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %28 = arith.index_cast %arg2 : index to i32
        %29 = arith.sitofp %28 : i32 to f32
        %30 = arith.divf %29, %cst_5 : f32
        %31 = arith.mulf %30, %cst_8 : f32
        %32 = arith.addf %31, %cst_9 : f32
        %33 = arith.mulf %32, %32 : f32
        %34 = arith.mulf %33, %32 : f32
        %35 = arith.mulf %34, %cst_3 : f32
        %36 = arith.addf %32, %35 : f32
        %37 = arith.mulf %36, %cst_2 : f32
        %38 = arith.mulf %32, %cst_1 : f32
        %39 = func.call @tanhf(%37) : (f32) -> f32
        %40 = arith.addf %39, %cst_0 : f32
        %41 = arith.mulf %38, %40 : f32
        %42 = arts.db_ref %ptr_39[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %41, %42[%arg2] : memref<?xf32>
      } {arts.id = 271 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:193:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_39) : memref<?xmemref<?xf32>>
      %guid_40, %ptr_41 = arts.db_acquire[<out>] (%guid_24 : memref<?xi64>, %ptr_25 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_41 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      %guid_42, %ptr_43 = arts.db_acquire[<out>] (%guid_26 : memref<?xi64>, %ptr_27 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_43 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %28 = arith.index_cast %arg2 : index to i32
        %29 = arith.sitofp %28 : i32 to f32
        %30 = arith.divf %29, %cst_5 : f32
        %31 = arith.mulf %30, %cst_8 : f32
        %32 = arith.addf %31, %cst_9 : f32
        %33 = arith.mulf %32, %cst : f32
        %34 = math.exp %33 : f32
        %35 = arith.addf %34, %cst_0 : f32
        %36 = arith.divf %cst_0, %35 : f32
        %37 = arith.mulf %32, %36 : f32
        %38 = arts.db_ref %ptr_41[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %37, %38[%arg2] : memref<?xf32>
        %39 = arith.negf %32 : f32
        %40 = math.exp %39 : f32
        %41 = arith.addf %40, %cst_0 : f32
        %42 = arith.divf %cst_0, %41 : f32
        %43 = arts.db_ref %ptr_43[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %42, %43[%arg2] : memref<?xf32>
      } {arts.id = 272 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:194:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_41) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_43) : memref<?xmemref<?xf32>>
      %guid_44, %ptr_45 = arts.db_acquire[<out>] (%guid_28 : memref<?xi64>, %ptr_29 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [1048576], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.lowering_contract(%ptr_45 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c1048576] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
      scf.for %arg2 = %c0 to %c1048576 step %c1 {
        %28 = arith.index_cast %arg2 : index to i32
        %29 = arith.sitofp %28 : i32 to f32
        %30 = arith.divf %29, %cst_5 : f32
        %31 = arith.mulf %30, %cst_8 : f32
        %32 = arith.addf %31, %cst_9 : f32
        %33 = func.call @tanhf(%32) : (f32) -> f32
        %34 = arts.db_ref %ptr_45[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %33, %34[%arg2] : memref<?xf32>
      } {arts.id = 273 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:196:5">, arts.metadata_provenance = "exact"}
      arts.db_release(%ptr_45) : memref<?xmemref<?xf32>>
    } : i64
    %8 = "polygeist.undef"() : () -> f32
    memref.store %8, %alloca_30[] : memref<f32>
    memref.store %8, %alloca_31[] : memref<f32>
    memref.store %cst_9, %alloca_31[] : memref<f32>
    scf.for %arg2 = %c1 to %c100 step %c1 {
      %28 = arith.index_cast %arg2 : index to i32
      %29 = arith.sitofp %28 : i32 to f32
      %30 = arith.divf %29, %cst_6 : f32
      %31 = arith.mulf %30, %cst_8 : f32
      %32 = arith.addf %31, %cst_9 : f32
      %33 = memref.load %alloca_31[] : memref<f32>
      %34 = arith.cmpf ogt, %32, %33 : f32
      scf.if %34 {
        memref.store %32, %alloca_31[] : memref<f32>
      }
    } {arts.id = 139 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 99 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    memref.store %cst_7, %alloca_30[] : memref<f32>
    %9 = memref.load %alloca_31[] : memref<f32>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %28 = arith.index_cast %arg2 : index to i32
      %29 = arith.sitofp %28 : i32 to f32
      %30 = arith.divf %29, %cst_6 : f32
      %31 = arith.mulf %30, %cst_8 : f32
      %32 = arith.addf %31, %cst_9 : f32
      %33 = arith.subf %32, %9 : f32
      %34 = math.exp %33 : f32
      memref.store %34, %alloc[%arg2] : memref<100xf32>
      %35 = memref.load %alloc[%arg2] : memref<100xf32>
      %36 = memref.load %alloca_30[] : memref<f32>
      %37 = arith.addf %36, %35 : f32
      memref.store %37, %alloca_30[] : memref<f32>
    } {arts.id = 150 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    %10 = memref.load %alloca_30[] : memref<f32>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %28 = memref.load %alloc[%arg2] : memref<100xf32>
      %29 = arith.divf %28, %10 : f32
      memref.store %29, %alloc[%arg2] : memref<100xf32>
    } {arts.id = 156 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %11 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%11, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_10, %alloca_17[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_17[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_17[] : memref<f64>
    } {arts.id = 160 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:206:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_16[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr_19[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_16[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_16[] : memref<f64>
    } {arts.id = 169 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:209:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_15[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr_21[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_15[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_15[] : memref<f64>
    } {arts.id = 178 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:212:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_14[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr_23[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_14[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_14[] : memref<f64>
    } {arts.id = 187 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:215:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_13[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr_25[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_13[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_13[] : memref<f64>
    } {arts.id = 197 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:218:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_12[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr_27[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_12[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_12[] : memref<f64>
    } {arts.id = 206 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:221:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_11[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %28 = arts.db_ref %ptr_29[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %29 = memref.load %28[%arg2] : memref<?xf32>
      %30 = arith.extf %29 : f32 to f64
      %31 = math.absf %30 : f64
      %32 = memref.load %alloca_11[] : memref<f64>
      %33 = arith.addf %32, %31 : f64
      memref.store %33, %alloca_11[] : memref<f64>
    } {arts.id = 215 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:224:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %28 = memref.load %alloc[%arg2] : memref<100xf32>
      %29 = arith.extf %28 : f32 to f64
      %30 = math.absf %29 : f64
      %31 = memref.load %alloca[] : memref<f64>
      %32 = arith.addf %31, %30 : f64
      memref.store %32, %alloca[] : memref<f64>
    } {arts.id = 225 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:227:3">, arts.metadata_provenance = "recovered"}
    %12 = memref.load %alloca_17[] : memref<f64>
    %13 = memref.load %alloca_16[] : memref<f64>
    %14 = arith.addf %12, %13 : f64
    %15 = memref.load %alloca_15[] : memref<f64>
    %16 = arith.addf %14, %15 : f64
    %17 = memref.load %alloca_14[] : memref<f64>
    %18 = arith.addf %16, %17 : f64
    %19 = memref.load %alloca_13[] : memref<f64>
    %20 = arith.addf %18, %19 : f64
    %21 = memref.load %alloca_12[] : memref<f64>
    %22 = arith.addf %20, %21 : f64
    %23 = memref.load %alloca_11[] : memref<f64>
    %24 = arith.addf %22, %23 : f64
    %25 = memref.load %alloca[] : memref<f64>
    %26 = arith.addf %24, %25 : f64
    call @carts_bench_checksum_d(%26) : (f64) -> ()
    call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> ()
    %27 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%27, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc {arts.id = 301 : i64} : memref<100xf32>
    call @carts_phase_timer_stop(%27) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_26) : memref<?xi64>
    arts.db_free(%ptr_27) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_20) : memref<?xi64>
    arts.db_free(%ptr_21) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_22) : memref<?xi64>
    arts.db_free(%ptr_23) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_24) : memref<?xi64>
    arts.db_free(%ptr_25) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_18) : memref<?xi64>
    arts.db_free(%ptr_19) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_28) : memref<?xi64>
    arts.db_free(%ptr_29) : memref<?xmemref<?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf32>>
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
  func.func private @tanhf(f32) -> f32 attributes {llvm.linkage = #llvm.linkage<external>}
}
