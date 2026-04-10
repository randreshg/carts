module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str4("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("allocation failure\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("layernorm\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1023_i64 = arith.constant 1023 : i64
    %c1023 = arith.constant 1023 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 9.99999974E-6 : f32
    %cst_0 = arith.constant 1.024000e+03 : f32
    %c8 = arith.constant 8 : index
    %c-1_i32 = arith.constant -1 : i32
    %c1024 = arith.constant 1024 : index
    %c113_i32 = arith.constant 113 : i32
    %cst_1 = arith.constant 5.000000e+01 : f32
    %cst_2 = arith.constant 3.125000e-02 : f32
    %cst_3 = arith.constant 1.000000e+00 : f32
    %cst_4 = arith.constant 0.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str4 : !llvm.ptr
    %cst_5 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str3 : !llvm.ptr
    %c1_i32 = arith.constant 1 : i32
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr
    %3 = llvm.mlir.addressof @stderr : !llvm.ptr
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %alloca = memref.alloca() {arts.id = 118 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:90:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 118 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %7 = "polygeist.undef"() : () -> f64
    memref.store %7, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %8 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%8) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %8) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c8, %c1024] {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "layernorm.c:65:15", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 65536 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 33 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf32>>) block_shape[%c8] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    %10 = arts.runtime_query <total_workers> -> i32
    %11 = arith.index_castui %10 : i32 to index
    %12 = arith.maxui %11, %c1 : index
    %13 = arith.addi %12, %c1023 : index
    %14 = arith.divui %13, %12 : index
    %15 = arith.maxui %14, %c1 : index
    %16 = arith.maxui %15, %c1 : index
    %17 = arith.index_cast %16 : index to i64
    %18 = arith.addi %17, %c1023_i64 : i64
    %19 = arith.divui %18, %17 : i64
    %20 = arith.index_cast %19 : i64 to index
    %guid_6, %ptr_7 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%20] elementType(f32) elementSizes[%16] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "layernorm.c:66:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasAffineAccesses = false, hasNonAffineAccesses = false, memoryFootprint = 4096 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 130 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_7 : memref<?xmemref<?xf32>>) block_shape[%16] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    %guid_8, %ptr_9 = arts.db_alloc[<in>, <heap>, <read>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%20] elementType(f32) elementSizes[%16] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "layernorm.c:67:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasAffineAccesses = false, hasNonAffineAccesses = false, memoryFootprint = 4096 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 36 : i64, lastUseId = 131 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_9 : memref<?xmemref<?xf32>>) block_shape[%16] contract(<ownerDims = [0], postDbRefined = true>) {arts.proof.dep_slice_soundness = false, arts.proof.halo_legality = false, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = false, arts.proof.relaunch_state_soundness = true}
    %21 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %22 = polygeist.memref2pointer %21 : memref<?x?xf32> to !llvm.ptr
    %23 = llvm.icmp "eq" %22, %4 : !llvm.ptr
    %24 = scf.if %23 -> (i1) {
      scf.yield %true : i1
    } else {
      %27 = arith.maxui %16, %c1 : index
      %28 = arith.divui %c0, %27 : index
      %29 = arts.db_ref %ptr_7[%28] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %30 = polygeist.memref2pointer %29 : memref<?xf32> to !llvm.ptr
      %31 = llvm.icmp "eq" %30, %4 : !llvm.ptr
      scf.yield %31 : i1
    }
    %25 = scf.if %24 -> (i1) {
      scf.yield %true : i1
    } else {
      %27 = arith.maxui %16, %c1 : index
      %28 = arith.divui %c0, %27 : index
      %29 = arts.db_ref %ptr_9[%28] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %30 = polygeist.memref2pointer %29 : memref<?xf32> to !llvm.ptr
      %31 = llvm.icmp "eq" %30, %4 : !llvm.ptr
      scf.yield %31 : i1
    }
    %26 = arith.extui %25 : i1 to i32
    scf.if %25 {
      %27 = llvm.load %3 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %28 = polygeist.memref2pointer %27 : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>> to !llvm.ptr
      %29 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<20 x i8>
      %30 = llvm.call @fprintf(%28, %29) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
    } else {
      %alloca_10 = memref.alloca() {arts.id = 74 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:84:5", totalAccesses = 6 : i64, readCount = 2 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 74 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
      %alloca_11 = memref.alloca() {arts.id = 76 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:84:5", totalAccesses = 6 : i64, readCount = 2 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 76 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
      %27 = "polygeist.undef"() : () -> i32
      %alloca_12 = memref.alloca() {arts.id = 53 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:78:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 53 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
      memref.store %27, %alloca_12[] : memref<i32>
      memref.store %c0_i32, %alloca_12[] : memref<i32>
      scf.for %arg0 = %c0 to %c16 step %c1 {
        scf.for %arg1 = %c0 to %c1024 step %c1 {
          %32 = memref.load %alloca_12[] : memref<i32>
          %33 = arith.remsi %32, %c113_i32 : i32
          %34 = arith.sitofp %33 : i32 to f32
          %35 = arith.subf %34, %cst_1 : f32
          %36 = arith.mulf %35, %cst_2 : f32
          %37 = arith.divui %arg0, %c8 : index
          %38 = arith.remui %arg0, %c8 : index
          %39 = arts.db_ref %ptr[%37] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
          memref.store %36, %39[%38, %arg1] : memref<?x?xf32>
          %40 = memref.load %alloca_12[] : memref<i32>
          %41 = arith.addi %40, %c1_i32 : i32
          memref.store %41, %alloca_12[] : memref<i32>
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 16 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "recovered"}
      scf.for %arg0 = %c0 to %c1024 step %c1 {
        %32 = arith.maxui %16, %c1 : index
        %33 = arith.divui %arg0, %32 : index
        %34 = arith.remui %arg0, %32 : index
        %35 = arts.db_ref %ptr_7[%33] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %cst_3, %35[%34] : memref<?xf32>
        %36 = arts.db_ref %ptr_9[%33] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %cst_4, %36[%34] : memref<?xf32>
      } {arts.id = 72 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "recovered"}
      func.call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
      func.call @carts_kernel_timer_start(%8) : (memref<?xi8>) -> ()
      %28 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
        scf.for %arg0 = %c0 to %c2 step %c1 {
          %32 = arith.muli %arg0, %c8 : index
          %33 = arith.cmpi uge, %32, %c16 : index
          %34 = arith.subi %c16, %32 : index
          %35 = arith.select %33, %c0, %34 : index
          %36 = arith.minui %35, %c8 : index
          %37 = arith.cmpi ugt, %36, %c0 : index
          scf.if %37 {
            %38 = arith.divui %32, %c8 : index
            %39 = arith.cmpi ugt, %38, %c1 : index
            %40 = arith.select %39, %c0, %38 : index
            %guid_13, %ptr_14 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[%32], sizes[%c8]), offsets[%40], sizes[%c1] element_offsets[%32] element_sizes[%c8] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [8, 1024], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)
            arts.lowering_contract(%ptr_14 : memref<?xmemref<?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c8] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
            %41 = arith.maxui %16, %c1 : index
            %42 = arith.divui %c0, %41 : index
            %43 = arith.divui %c1023, %41 : index
            %44 = arith.subi %20, %c1 : index
            %45 = arith.cmpi ugt, %42, %44 : index
            %46 = arith.minui %43, %44 : index
            %47 = arith.select %45, %43, %46 : index
            %48 = arith.subi %47, %42 : index
            %49 = arith.addi %48, %c1 : index
            %50 = arith.select %45, %c0, %42 : index
            %51 = arith.select %45, %c0, %49 : index
            %guid_15, %ptr_16 = arts.db_acquire[<in>] (%guid_6 : memref<?xi64>, %ptr_7 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%50], sizes[%51] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
            arts.lowering_contract(%ptr_16 : memref<?xmemref<?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%16] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
            %guid_17, %ptr_18 = arts.db_acquire[<in>] (%guid_8 : memref<?xi64>, %ptr_9 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%50], sizes[%51] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
            arts.lowering_contract(%ptr_18 : memref<?xmemref<?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%16] contract(<ownerDims = [0], postDbRefined = true, criticalPathDistance = 0 : i64>) {arts.proof.dep_slice_soundness = true, arts.proof.halo_legality = true, arts.proof.owner_dim_reachability = true, arts.proof.partition_access_mapping = true, arts.proof.relaunch_state_soundness = true}
            arts.edt <task> <intranode> route(%c-1_i32) (%ptr_14, %ptr_16, %ptr_18) : memref<?xmemref<?x?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 137 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 1024]} {
            ^bb0(%arg1: memref<?xmemref<?x?xf32>>, %arg2: memref<?xmemref<?xf32>>, %arg3: memref<?xmemref<?xf32>>):
              %52 = "polygeist.undef"() : () -> f32
              %53 = arith.subi %c0, %32 : index
              %54 = arith.cmpi slt, %53, %c0 : index
              %55 = arith.select %54, %c0, %53 : index
              %56 = arith.cmpi slt, %34, %c0 : index
              %57 = arith.select %56, %c0, %34 : index
              %58 = arith.minui %57, %36 : index
              scf.for %arg4 = %55 to %58 step %c1 {
                memref.store %52, %alloca_10[] : memref<f32>
                memref.store %52, %alloca_11[] : memref<f32>
                memref.store %cst_4, %alloca_11[] : memref<f32>
                memref.store %cst_4, %alloca_10[] : memref<f32>
                scf.for %arg5 = %c0 to %c1024 step %c1 {
                  %66 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
                  %67 = memref.load %66[%arg4, %arg5] : memref<?x?xf32>
                  %68 = memref.load %alloca_11[] : memref<f32>
                  %69 = arith.addf %68, %67 : f32
                  memref.store %69, %alloca_11[] : memref<f32>
                } {arts.id = 85 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
                %59 = memref.load %alloca_11[] : memref<f32>
                %60 = arith.divf %59, %cst_0 : f32
                memref.store %60, %alloca_11[] : memref<f32>
                scf.for %arg5 = %c0 to %c1024 step %c1 {
                  %66 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
                  %67 = memref.load %66[%arg4, %arg5] : memref<?x?xf32>
                  %68 = arith.subf %67, %60 : f32
                  %69 = arith.mulf %68, %68 : f32
                  %70 = memref.load %alloca_10[] : memref<f32>
                  %71 = arith.addf %70, %69 : f32
                  memref.store %71, %alloca_10[] : memref<f32>
                } {arts.id = 96 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
                %61 = memref.load %alloca_10[] : memref<f32>
                %62 = arith.divf %61, %cst_0 : f32
                memref.store %62, %alloca_10[] : memref<f32>
                %63 = arith.addf %62, %cst : f32
                %64 = math.sqrt %63 : f32
                %65 = arith.divf %cst_3, %64 : f32
                scf.for %arg5 = %c0 to %c1024 step %c1 {
                  %66 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
                  %67 = memref.load %66[%arg4, %arg5] : memref<?x?xf32>
                  %68 = arith.subf %67, %60 : f32
                  %69 = arith.mulf %68, %65 : f32
                  %70 = arith.maxui %41, %c1 : index
                  %71 = arith.divui %arg5, %70 : index
                  %72 = arith.subi %71, %50 : index
                  %73 = arith.remui %arg5, %70 : index
                  %74 = arts.db_ref %arg2[%72] : memref<?xmemref<?xf32>> -> memref<?xf32>
                  %75 = memref.load %74[%73] : memref<?xf32>
                  %76 = arith.mulf %69, %75 : f32
                  %77 = arts.db_ref %arg3[%72] : memref<?xmemref<?xf32>> -> memref<?xf32>
                  %78 = memref.load %77[%73] : memref<?xf32>
                  %79 = arith.addf %76, %78 : f32
                  memref.store %79, %66[%arg4, %arg5] : memref<?x?xf32>
                } {arts.id = 112 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
              } {arts.id = 136 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
              arts.db_release(%arg1) : memref<?xmemref<?x?xf32>>
              arts.db_release(%arg2) : memref<?xmemref<?xf32>>
              arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            }
          }
        }
      } : i64
      func.call @carts_kernel_timer_accum(%8) : (memref<?xi8>) -> ()
      func.call @carts_kernel_timer_print(%8) : (memref<?xi8>) -> ()
      %29 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%29, %8) : (memref<?xi8>, memref<?xi8>) -> ()
      memref.store %cst_5, %alloca[] : memref<f64>
      scf.for %arg0 = %c0 to %c16 step %c1 {
        %32 = arith.divui %arg0, %c8 : index
        %33 = arith.remui %arg0, %c8 : index
        %34 = arts.db_ref %ptr[%32] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        %35 = memref.load %34[%33, %arg0] : memref<?x?xf32>
        %36 = arith.extf %35 : f32 to f64
        %37 = math.absf %36 : f64
        %38 = memref.load %alloca[] : memref<f64>
        %39 = arith.addf %38, %37 : f64
        memref.store %39, %alloca[] : memref<f64>
      } {arts.id = 122 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 16 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:92:3">, arts.metadata_provenance = "recovered"}
      %30 = memref.load %alloca[] : memref<f64>
      func.call @carts_bench_checksum_d(%30) : (f64) -> ()
      func.call @carts_phase_timer_stop(%29) : (memref<?xi8>) -> ()
      %31 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%31, %8) : (memref<?xi8>, memref<?xi8>) -> ()
      func.call @carts_phase_timer_stop(%31) : (memref<?xi8>) -> ()
      func.call @carts_e2e_timer_stop() : () -> ()
    }
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf32>>
    arts.db_free(%guid_6) : memref<?xi64>
    arts.db_free(%ptr_7) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_8) : memref<?xi64>
    arts.db_free(%ptr_9) : memref<?xmemref<?xf32>>
    return %26 : i32
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
