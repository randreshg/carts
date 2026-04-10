module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5 = arith.constant 5 : index
    %c8 = arith.constant 8 : index
    %c6 = arith.constant 6 : index
    %cst = arith.constant 5.000000e-01 : f32
    %c-2 = arith.constant -2 : index
    %c2_i32 = arith.constant 2 : i32
    %c4 = arith.constant 4 : index
    %c2 = arith.constant 2 : index
    %cst_0 = arith.constant -0.0833333358 : f32
    %cst_1 = arith.constant 0.666666686 : f32
    %cst_2 = arith.constant -0.666666686 : f32
    %cst_3 = arith.constant 0.0833333358 : f32
    %c44 = arith.constant 44 : index
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
    %c48 = arith.constant 48 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c3 = arith.constant 3 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_11 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 251 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:158:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 251 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %c0_12 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c48, %c48, %c48] {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:116:17", totalAccesses = 16 : i64, readCount = 15 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 1327104 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 34 : i64, lastUseId = 35 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 64 : i64>, arts.metadata_origin_id = 34 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %c1_13 = arith.constant 1 : index
    %c0_14 = arith.constant 0 : index
    %guid_15, %ptr_16 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c48, %c48, %c48] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:117:19", totalAccesses = 5 : i64, readCount = 1 : i64, writeCount = 4 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 1327104 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 36 : i64, lastUseId = 37 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %c0_17 = arith.constant 0 : index
    %guid_18, %ptr_19 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 38 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:119:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 38 : i64, lastUseId = 39 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 38 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c0_20 = arith.constant 0 : index
    %guid_21, %ptr_22 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 40 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:120:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 40 : i64, lastUseId = 41 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 40 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_23 = memref.alloca() {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:144:3", totalAccesses = 11 : i64, readCount = 5 : i64, writeCount = 6 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 43 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_23[] : memref<i32>
    memref.store %c0_i32, %alloca_23[] : memref<i32>
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %12 = arith.index_cast %arg0 : index to i32
      memref.store %c0_i32, %alloca_23[] : memref<i32>
      %13 = arith.muli %12, %c17_i32 : i32
      %14 = arith.sitofp %12 : i32 to f32
      %15 = arith.mulf %14, %cst_5 : f32
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          scf.for %arg3 = %c0 to %c48 step %c1 {
            %16 = memref.load %alloca_23[] : memref<i32>
            %17 = arith.addi %16, %13 : i32
            %18 = arith.remsi %17, %c23_i32 : i32
            %19 = arith.sitofp %18 : i32 to f32
            %20 = arith.mulf %19, %cst_4 : f32
            %21 = arith.subf %20, %15 : f32
            %22 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            memref.store %21, %22[%arg0, %arg1, %arg2, %arg3] : memref<?x?x?x?xf32>
            %23 = arts.db_ref %ptr_16[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            memref.store %cst_6, %23[%arg0, %arg1, %arg2, %arg3] : memref<?x?x?x?xf32>
            %24 = memref.load %alloca_23[] : memref<i32>
            %25 = arith.addi %24, %c1_i32 : i32
            memref.store %25, %alloca_23[] : memref<i32>
          } {arts.id = 63 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 65 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 67 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 69 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    memref.store %c0_i32, %alloca_23[] : memref<i32>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          %12 = memref.load %alloca_23[] : memref<i32>
          %13 = arith.remsi %12, %c11_i32 : i32
          %14 = arith.sitofp %13 : i32 to f32
          %15 = arith.mulf %14, %cst_8 : f32
          %16 = arith.addf %15, %cst_7 : f32
          %17 = arts.db_ref %ptr_19[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %16, %17[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %18 = memref.load %alloca_23[] : memref<i32>
          %19 = arith.remsi %18, %c7_i32 : i32
          %20 = arith.sitofp %19 : i32 to f32
          %21 = arith.mulf %20, %cst_10 : f32
          %22 = arith.addf %21, %cst_9 : f32
          %23 = arts.db_ref %ptr_22[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %22, %23[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %24 = memref.load %alloca_23[] : memref<i32>
          %25 = arith.addi %24, %c1_i32 : i32
          memref.store %25, %alloca_23[] : memref<i32>
        } {arts.id = 87 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 89 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 91 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_24 = memref.alloca() {arts.id = 92 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 92 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
    %alloca_25 = memref.alloca() {arts.id = 113 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 113 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_26 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_27 = memref.alloca() {arts.id = 109 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 109 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_28 = memref.alloca() {arts.id = 105 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 105 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_29 = memref.alloca() {arts.id = 101 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 101 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_30 = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_31 = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_32 = memref.alloca() {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 103 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %8 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %12 = arith.muli %arg0, %c6 : index
        %13 = arith.cmpi uge, %12, %c44 : index
        %14 = arith.subi %c44, %12 : index
        %15 = arith.select %13, %c0, %14 : index
        %16 = arith.minui %15, %c6 : index
        %17 = arith.cmpi ugt, %16, %c0 : index
        scf.if %17 {
          %18 = arith.cmpi slt, %14, %c0 : index
          %19 = arith.select %18, %c0, %14 : index
          %20 = arith.minui %19, %16 : index
          %guid_33, %ptr_34 = arts.db_acquire[<in>] (%guid_18 : memref<?xi64>, %ptr_19 : memref<?xmemref<?x?x?xf32>>) partitioning(<coarse>), offsets[%c0_17], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_34 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %21 = arith.cmpi slt, %14, %c0 : index
          %22 = arith.select %21, %c0, %14 : index
          %23 = arith.minui %22, %16 : index
          %guid_35, %ptr_36 = arts.db_acquire[<in>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?x?xf32>>) partitioning(<coarse>), offsets[%c0_20], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_36 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %24 = arith.cmpi slt, %14, %c0 : index
          %25 = arith.select %24, %c0, %14 : index
          %26 = arith.minui %25, %16 : index
          %27 = arith.index_cast %c-1_i32 : i32 to index
          %c2_37 = arith.constant 2 : index
          %28 = arith.cmpi uge, %27, %c2_37 : index
          %29 = arith.subi %27, %c2_37 : index
          %c0_38 = arith.constant 0 : index
          %30 = arith.select %28, %29, %c0_38 : index
          %c46 = arith.constant 46 : index
          %31 = arith.addi %26, %c46 : index
          %guid_39, %ptr_40 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?x?xf32>>) partitioning(<coarse>), offsets[%c0_12], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 48, 48, 6], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
          arts.lowering_contract(%ptr_40 : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true>)
          %32 = arith.divui %12, %c3 : index
          %33 = arith.addi %12, %c5 : index
          %34 = arith.divui %33, %c3 : index
          %35 = arith.cmpi ugt, %32, %c0 : index
          %36 = arith.select %35, %34, %c0 : index
          %37 = arith.subi %36, %32 : index
          %38 = arith.addi %37, %c1 : index
          %39 = arith.select %35, %c0, %32 : index
          %40 = arith.select %35, %c0, %38 : index
          %guid_41, %ptr_42 = arts.db_acquire[<inout>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%12], sizes[%c6]), offsets[%39], sizes[%40] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [3, 48, 48, 6], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
          arts.lowering_contract(%ptr_42 : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_34, %ptr_36, %ptr_40, %ptr_42) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>> attributes {arts.id = 259 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 48, 48, 6]} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf32>>, %arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?x?xf32>>):
            memref.store %cst_0, %alloca_24[%c0] : memref<5xf32>
            memref.store %cst_1, %alloca_24[%c1] : memref<5xf32>
            memref.store %cst_6, %alloca_24[%c2] : memref<5xf32>
            memref.store %cst_2, %alloca_24[%c3] : memref<5xf32>
            memref.store %cst_3, %alloca_24[%c4] : memref<5xf32>
            %41 = "polygeist.undef"() : () -> f32
            %42 = arith.subi %c4, %12 : index
            %43 = arith.cmpi slt, %42, %c0 : index
            %44 = arith.select %43, %c0, %42 : index
            %45 = arith.cmpi slt, %14, %c0 : index
            %46 = arith.select %45, %c0, %14 : index
            %47 = arith.minui %46, %16 : index
            scf.for %arg5 = %44 to %47 step %c1 {
              %48 = arith.addi %12, %arg5 : index
              memref.store %41, %alloca_31[] : memref<f32>
              memref.store %41, %alloca_29[] : memref<f32>
              memref.store %41, %alloca_32[] : memref<f32>
              memref.store %41, %alloca_28[] : memref<f32>
              memref.store %41, %alloca_30[] : memref<f32>
              memref.store %41, %alloca_27[] : memref<f32>
              memref.store %41, %alloca_26[] : memref<f32>
              memref.store %41, %alloca_25[] : memref<f32>
              %49 = arith.index_cast %48 : index to i32
              %50 = arith.addi %49, %c1_i32 : i32
              %51 = arith.index_cast %50 : i32 to index
              %52 = arith.addi %49, %c-1_i32 : i32
              %53 = arith.index_cast %52 : i32 to index
              scf.for %arg6 = %c4 to %c44 step %c1 {
                %54 = arith.index_cast %arg6 : index to i32
                %55 = arith.addi %54, %c1_i32 : i32
                %56 = arith.index_cast %55 : i32 to index
                %57 = arith.addi %54, %c-1_i32 : i32
                %58 = arith.index_cast %57 : i32 to index
                scf.for %arg7 = %c4 to %c44 step %c1 {
                  %59 = arith.index_cast %arg7 : index to i32
                  %60 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %61 = memref.load %60[%arg7, %arg6, %48] : memref<?x?x?xf32>
                  memref.store %61, %alloca_25[] : memref<f32>
                  %62 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %63 = memref.load %62[%arg7, %arg6, %48] : memref<?x?x?xf32>
                  memref.store %63, %alloca_26[] : memref<f32>
                  memref.store %cst_6, %alloca_27[] : memref<f32>
                  scf.for %arg8 = %c-2 to %c3 step %c1 {
                    %106 = arith.index_cast %arg8 : index to i32
                    %107 = arith.addi %106, %c2_i32 : i32
                    %108 = arith.index_cast %107 : i32 to index
                    %109 = memref.load %alloca_24[%108] : memref<5xf32>
                    %110 = arith.addi %59, %106 : i32
                    %111 = arith.index_cast %110 : i32 to index
                    %112 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                    %113 = memref.load %112[%c0, %111, %arg6, %48] : memref<?x?x?x?xf32>
                    %114 = arith.addi %54, %106 : i32
                    %115 = arith.index_cast %114 : i32 to index
                    %116 = memref.load %112[%c0, %arg7, %115, %48] : memref<?x?x?x?xf32>
                    %117 = arith.addf %113, %116 : f32
                    %118 = arith.addi %49, %106 : i32
                    %119 = arith.index_cast %118 : i32 to index
                    %120 = memref.load %112[%c0, %arg7, %arg6, %119] : memref<?x?x?x?xf32>
                    %121 = arith.addf %117, %120 : f32
                    %122 = arith.mulf %109, %121 : f32
                    %123 = memref.load %alloca_27[] : memref<f32>
                    %124 = arith.addf %123, %122 : f32
                    memref.store %124, %alloca_27[] : memref<f32>
                  } {arts.id = 151 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                  %64 = arith.addi %59, %c1_i32 : i32
                  %65 = arith.index_cast %64 : i32 to index
                  %66 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  %67 = memref.load %66[%c0, %65, %arg6, %48] : memref<?x?x?x?xf32>
                  %68 = arith.addi %59, %c-1_i32 : i32
                  %69 = arith.index_cast %68 : i32 to index
                  %70 = memref.load %66[%c0, %69, %arg6, %48] : memref<?x?x?x?xf32>
                  %71 = arith.subf %67, %70 : f32
                  memref.store %71, %alloca_30[] : memref<f32>
                  %72 = memref.load %alloca_25[] : memref<f32>
                  %73 = memref.load %alloca_27[] : memref<f32>
                  %74 = arith.mulf %72, %73 : f32
                  %75 = memref.load %alloca_26[] : memref<f32>
                  %76 = arith.addf %75, %72 : f32
                  %77 = memref.load %alloca_30[] : memref<f32>
                  %78 = arith.mulf %76, %77 : f32
                  %79 = arith.mulf %78, %cst : f32
                  %80 = arith.addf %74, %79 : f32
                  %81 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  memref.store %80, %81[%c0, %arg7, %arg6, %48] : memref<?x?x?x?xf32>
                  memref.store %cst_6, %alloca_28[] : memref<f32>
                  scf.for %arg8 = %c-2 to %c3 step %c1 {
                    %106 = arith.index_cast %arg8 : index to i32
                    %107 = arith.addi %106, %c2_i32 : i32
                    %108 = arith.index_cast %107 : i32 to index
                    %109 = memref.load %alloca_24[%108] : memref<5xf32>
                    %110 = arith.addi %59, %106 : i32
                    %111 = arith.index_cast %110 : i32 to index
                    %112 = memref.load %66[%c1, %111, %arg6, %48] : memref<?x?x?x?xf32>
                    %113 = arith.addi %54, %106 : i32
                    %114 = arith.index_cast %113 : i32 to index
                    %115 = memref.load %66[%c1, %arg7, %114, %48] : memref<?x?x?x?xf32>
                    %116 = arith.addf %112, %115 : f32
                    %117 = arith.addi %49, %106 : i32
                    %118 = arith.index_cast %117 : i32 to index
                    %119 = memref.load %66[%c1, %arg7, %arg6, %118] : memref<?x?x?x?xf32>
                    %120 = arith.addf %116, %119 : f32
                    %121 = arith.mulf %109, %120 : f32
                    %122 = memref.load %alloca_28[] : memref<f32>
                    %123 = arith.addf %122, %121 : f32
                    memref.store %123, %alloca_28[] : memref<f32>
                  } {arts.id = 191 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                  %82 = memref.load %66[%c1, %arg7, %56, %48] : memref<?x?x?x?xf32>
                  %83 = memref.load %66[%c1, %arg7, %58, %48] : memref<?x?x?x?xf32>
                  %84 = arith.subf %82, %83 : f32
                  memref.store %84, %alloca_32[] : memref<f32>
                  %85 = memref.load %alloca_25[] : memref<f32>
                  %86 = memref.load %alloca_28[] : memref<f32>
                  %87 = arith.mulf %85, %86 : f32
                  %88 = memref.load %alloca_26[] : memref<f32>
                  %89 = arith.addf %88, %85 : f32
                  %90 = memref.load %alloca_32[] : memref<f32>
                  %91 = arith.mulf %89, %90 : f32
                  %92 = arith.mulf %91, %cst : f32
                  %93 = arith.addf %87, %92 : f32
                  memref.store %93, %81[%c1, %arg7, %arg6, %48] : memref<?x?x?x?xf32>
                  memref.store %cst_6, %alloca_29[] : memref<f32>
                  scf.for %arg8 = %c-2 to %c3 step %c1 {
                    %106 = arith.index_cast %arg8 : index to i32
                    %107 = arith.addi %106, %c2_i32 : i32
                    %108 = arith.index_cast %107 : i32 to index
                    %109 = memref.load %alloca_24[%108] : memref<5xf32>
                    %110 = arith.addi %59, %106 : i32
                    %111 = arith.index_cast %110 : i32 to index
                    %112 = memref.load %66[%c2, %111, %arg6, %48] : memref<?x?x?x?xf32>
                    %113 = arith.addi %54, %106 : i32
                    %114 = arith.index_cast %113 : i32 to index
                    %115 = memref.load %66[%c2, %arg7, %114, %48] : memref<?x?x?x?xf32>
                    %116 = arith.addf %112, %115 : f32
                    %117 = arith.addi %49, %106 : i32
                    %118 = arith.index_cast %117 : i32 to index
                    %119 = memref.load %66[%c2, %arg7, %arg6, %118] : memref<?x?x?x?xf32>
                    %120 = arith.addf %116, %119 : f32
                    %121 = arith.mulf %109, %120 : f32
                    %122 = memref.load %alloca_29[] : memref<f32>
                    %123 = arith.addf %122, %121 : f32
                    memref.store %123, %alloca_29[] : memref<f32>
                  } {arts.id = 227 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                  %94 = memref.load %66[%c2, %arg7, %arg6, %51] : memref<?x?x?x?xf32>
                  %95 = memref.load %66[%c2, %arg7, %arg6, %53] : memref<?x?x?x?xf32>
                  %96 = arith.subf %94, %95 : f32
                  memref.store %96, %alloca_31[] : memref<f32>
                  %97 = memref.load %alloca_25[] : memref<f32>
                  %98 = memref.load %alloca_29[] : memref<f32>
                  %99 = arith.mulf %97, %98 : f32
                  %100 = memref.load %alloca_26[] : memref<f32>
                  %101 = arith.addf %100, %97 : f32
                  %102 = memref.load %alloca_31[] : memref<f32>
                  %103 = arith.mulf %101, %102 : f32
                  %104 = arith.mulf %103, %cst : f32
                  %105 = arith.addf %99, %104 : f32
                  memref.store %105, %81[%c2, %arg7, %arg6, %48] : memref<?x?x?x?xf32>
                } {arts.id = 243 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
              } {arts.id = 245 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 258 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg1) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg2) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg3) : memref<?xmemref<?x?x?x?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?x?x?x?xf32>>
          }
        }
      }
    } : i64
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_11, %alloca[] : memref<f64>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      scf.for %arg1 = %c0 to %c3 step %c1 {
        %12 = arts.db_ref %ptr_16[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
        %13 = memref.load %12[%arg1, %arg0, %arg0, %arg0] : memref<?x?x?x?xf32>
        %14 = arith.extf %13 : f32 to f64
        %15 = math.absf %14 : f64
        %16 = memref.load %alloca[] : memref<f64>
        %17 = arith.addf %16, %15 : f64
        memref.store %17, %alloca[] : memref<f64>
      } {arts.id = 256 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:163:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 210 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:162:3">, arts.metadata_provenance = "recovered"}
    %10 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%10) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%11, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?xmemref<?x?x?x?xf32>>
    arts.db_free(%guid_18) : memref<?xi64>
    arts.db_free(%ptr_19) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?x?xf32>>
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
