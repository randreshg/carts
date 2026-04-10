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
    %c1_12 = arith.constant 1 : index
    %c0_13 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c48, %c48, %c48] {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:116:17", totalAccesses = 16 : i64, readCount = 15 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 1327104 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 34 : i64, lastUseId = 35 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 64 : i64>, arts.metadata_origin_id = 34 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %c1_14 = arith.constant 1 : index
    %c0_15 = arith.constant 0 : index
    %guid_16, %ptr_17 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c48, %c48, %c48] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:117:19", totalAccesses = 5 : i64, readCount = 1 : i64, writeCount = 4 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 1327104 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 36 : i64, lastUseId = 37 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
    %c1_18 = arith.constant 1 : index
    %c0_19 = arith.constant 0 : index
    %guid_20, %ptr_21 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 38 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:119:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 38 : i64, lastUseId = 39 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 38 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_22 = arith.constant 1 : index
    %c0_23 = arith.constant 0 : index
    %guid_24, %ptr_25 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 40 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:120:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 40 : i64, lastUseId = 41 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 40 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_26 = memref.alloca() {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:144:3", totalAccesses = 11 : i64, readCount = 5 : i64, writeCount = 6 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 43 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_26[] : memref<i32>
    memref.store %c0_i32, %alloca_26[] : memref<i32>
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %12 = arith.index_cast %arg0 : index to i32
      memref.store %c0_i32, %alloca_26[] : memref<i32>
      %13 = arith.muli %12, %c17_i32 : i32
      %14 = arith.sitofp %12 : i32 to f32
      %15 = arith.mulf %14, %cst_5 : f32
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          scf.for %arg3 = %c0 to %c48 step %c1 {
            %16 = memref.load %alloca_26[] : memref<i32>
            %17 = arith.addi %16, %13 : i32
            %18 = arith.remsi %17, %c23_i32 : i32
            %19 = arith.sitofp %18 : i32 to f32
            %20 = arith.mulf %19, %cst_4 : f32
            %21 = arith.subf %20, %15 : f32
            %22 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            memref.store %21, %22[%arg0, %arg1, %arg2, %arg3] : memref<?x?x?x?xf32>
            %23 = arts.db_ref %ptr_17[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
            memref.store %cst_6, %23[%arg0, %arg1, %arg2, %arg3] : memref<?x?x?x?xf32>
            %24 = memref.load %alloca_26[] : memref<i32>
            %25 = arith.addi %24, %c1_i32 : i32
            memref.store %25, %alloca_26[] : memref<i32>
          } {arts.id = 63 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 65 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 67 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 69 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    memref.store %c0_i32, %alloca_26[] : memref<i32>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          %12 = memref.load %alloca_26[] : memref<i32>
          %13 = arith.remsi %12, %c11_i32 : i32
          %14 = arith.sitofp %13 : i32 to f32
          %15 = arith.mulf %14, %cst_8 : f32
          %16 = arith.addf %15, %cst_7 : f32
          %17 = arts.db_ref %ptr_21[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %16, %17[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %18 = memref.load %alloca_26[] : memref<i32>
          %19 = arith.remsi %18, %c7_i32 : i32
          %20 = arith.sitofp %19 : i32 to f32
          %21 = arith.mulf %20, %cst_10 : f32
          %22 = arith.addf %21, %cst_9 : f32
          %23 = arts.db_ref %ptr_25[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %22, %23[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %24 = memref.load %alloca_26[] : memref<i32>
          %25 = arith.addi %24, %c1_i32 : i32
          memref.store %25, %alloca_26[] : memref<i32>
        } {arts.id = 87 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 89 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 91 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_27 = memref.alloca() {arts.id = 113 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 113 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_28 = memref.alloca() {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 103 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_29 = memref.alloca() {arts.id = 101 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 101 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_30 = memref.alloca() {arts.id = 105 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 105 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_31 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_32 = memref.alloca() {arts.id = 92 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 92 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
    %alloca_33 = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_34 = memref.alloca() {arts.id = 109 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 109 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_35 = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %8 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %12 = arith.muli %arg0, %c6 : index
        %13 = arith.cmpi uge, %12, %c44 : index
        %14 = arith.subi %c44, %12 : index
        %15 = arith.select %13, %c0, %14 : index
        %16 = arith.minui %15, %c6 : index
        %17 = arith.cmpi ugt, %16, %c0 : index
        scf.if %17 {
          %18 = arith.addi %12, %16 : index
          %19 = arith.cmpi uge, %12, %c1 : index
          %20 = arith.subi %12, %c1 : index
          %21 = arith.select %19, %20, %c0 : index
          %22 = arith.addi %18, %c1 : index
          %23 = arith.minui %22, %c48 : index
          %24 = arith.cmpi uge, %23, %21 : index
          %25 = arith.subi %23, %21 : index
          %26 = arith.select %24, %25, %c0 : index
          %27 = arith.maxui %26, %c1 : index
          %28 = arith.divui %21, %c48 : index
          %29 = arith.addi %21, %27 : index
          %30 = arith.subi %29, %c1 : index
          %31 = arith.divui %30, %c48 : index
          %32 = arith.cmpi ugt, %28, %c0 : index
          %33 = arith.select %32, %31, %c0 : index
          %34 = arith.subi %33, %28 : index
          %35 = arith.addi %34, %c1 : index
          %36 = arith.select %32, %c0, %28 : index
          %37 = arith.select %32, %c0, %35 : index
          %38 = arith.cmpi slt, %14, %c0 : index
          %39 = arith.select %38, %c0, %14 : index
          %40 = arith.minui %39, %16 : index
          %41 = arith.cmpi slt, %14, %c0 : index
          %42 = arith.select %41, %c0, %14 : index
          %43 = arith.minui %42, %16 : index
          %guid_36, %ptr_37 = arts.db_acquire[<in>] (%guid_20 : memref<?xi64>, %ptr_21 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_37 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %44 = arith.cmpi slt, %14, %c0 : index
          %45 = arith.select %44, %c0, %14 : index
          %46 = arith.minui %45, %16 : index
          %47 = arith.cmpi slt, %14, %c0 : index
          %48 = arith.select %47, %c0, %14 : index
          %49 = arith.minui %48, %16 : index
          %guid_38, %ptr_39 = arts.db_acquire[<in>] (%guid_24 : memref<?xi64>, %ptr_25 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_39 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %50 = arith.divui %21, %c3 : index
          %51 = arith.divui %30, %c3 : index
          %52 = arith.cmpi ugt, %50, %c0 : index
          %53 = arith.select %52, %51, %c0 : index
          %54 = arith.subi %53, %50 : index
          %55 = arith.addi %54, %c1 : index
          %56 = arith.select %52, %c0, %50 : index
          %57 = arith.select %52, %c0, %55 : index
          %58 = arith.cmpi slt, %14, %c0 : index
          %59 = arith.select %58, %c0, %14 : index
          %60 = arith.minui %59, %16 : index
          %61 = arith.index_cast %c-1_i32 : i32 to index
          %c2_40 = arith.constant 2 : index
          %62 = arith.cmpi uge, %61, %c2_40 : index
          %63 = arith.subi %61, %c2_40 : index
          %c0_41 = arith.constant 0 : index
          %64 = arith.select %62, %63, %c0_41 : index
          %c46 = arith.constant 46 : index
          %65 = arith.addi %60, %c46 : index
          %guid_42, %ptr_43 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%56], sizes[%57] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [3, 48, 48, 6], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
          arts.lowering_contract(%ptr_43 : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true>)
          %66 = arith.divui %12, %c3 : index
          %67 = arith.addi %12, %c5 : index
          %68 = arith.divui %67, %c3 : index
          %69 = arith.cmpi ugt, %66, %c0 : index
          %70 = arith.select %69, %68, %c0 : index
          %71 = arith.subi %70, %66 : index
          %72 = arith.addi %71, %c1 : index
          %73 = arith.select %69, %c0, %66 : index
          %74 = arith.select %69, %c0, %72 : index
          %guid_44, %ptr_45 = arts.db_acquire[<inout>] (%guid_16 : memref<?xi64>, %ptr_17 : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[%12], sizes[%c6]), offsets[%73], sizes[%74] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [3, 48, 48, 6], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?xmemref<?x?x?x?xf32>>)
          arts.lowering_contract(%ptr_45 : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_37, %ptr_39, %ptr_43, %ptr_45) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>>, memref<?xmemref<?x?x?x?xf32>> attributes {arts.id = 259 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 48, 48, 6]} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf32>>, %arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?x?xf32>>):
            memref.store %cst_0, %alloca_32[%c0] : memref<5xf32>
            memref.store %cst_1, %alloca_32[%c1] : memref<5xf32>
            memref.store %cst_6, %alloca_32[%c2] : memref<5xf32>
            memref.store %cst_2, %alloca_32[%c3] : memref<5xf32>
            memref.store %cst_3, %alloca_32[%c4] : memref<5xf32>
            %75 = "polygeist.undef"() : () -> f32
            %76 = arith.subi %c4, %12 : index
            %77 = arith.cmpi slt, %76, %c0 : index
            %78 = arith.select %77, %c0, %76 : index
            %79 = arith.cmpi slt, %14, %c0 : index
            %80 = arith.select %79, %c0, %14 : index
            %81 = arith.minui %80, %16 : index
            scf.for %arg5 = %78 to %81 step %c1 {
              %82 = arith.addi %12, %arg5 : index
              memref.store %75, %alloca_33[] : memref<f32>
              memref.store %75, %alloca_29[] : memref<f32>
              memref.store %75, %alloca_28[] : memref<f32>
              memref.store %75, %alloca_30[] : memref<f32>
              memref.store %75, %alloca_35[] : memref<f32>
              memref.store %75, %alloca_34[] : memref<f32>
              memref.store %75, %alloca_31[] : memref<f32>
              memref.store %75, %alloca_27[] : memref<f32>
              %83 = arith.index_cast %82 : index to i32
              %84 = arith.addi %83, %c1_i32 : i32
              %85 = arith.index_cast %84 : i32 to index
              %86 = arith.addi %83, %c-1_i32 : i32
              %87 = arith.index_cast %86 : i32 to index
              scf.for %arg6 = %c4 to %c44 step %c1 {
                %88 = arith.index_cast %arg6 : index to i32
                %89 = arith.addi %88, %c1_i32 : i32
                %90 = arith.index_cast %89 : i32 to index
                %91 = arith.addi %88, %c-1_i32 : i32
                %92 = arith.index_cast %91 : i32 to index
                scf.for %arg7 = %c4 to %c44 step %c1 {
                  %93 = arith.index_cast %arg7 : index to i32
                  %94 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %95 = memref.load %94[%arg7, %arg6, %82] : memref<?x?x?xf32>
                  memref.store %95, %alloca_27[] : memref<f32>
                  %96 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %97 = memref.load %96[%arg7, %arg6, %82] : memref<?x?x?xf32>
                  memref.store %97, %alloca_31[] : memref<f32>
                  memref.store %cst_6, %alloca_34[] : memref<f32>
                  scf.for %arg8 = %c-2 to %c3 step %c1 {
                    %140 = arith.index_cast %arg8 : index to i32
                    %141 = arith.addi %140, %c2_i32 : i32
                    %142 = arith.index_cast %141 : i32 to index
                    %143 = memref.load %alloca_32[%142] : memref<5xf32>
                    %144 = arith.addi %93, %140 : i32
                    %145 = arith.index_cast %144 : i32 to index
                    %146 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                    %147 = memref.load %146[%c0, %145, %arg6, %82] : memref<?x?x?x?xf32>
                    %148 = arith.addi %88, %140 : i32
                    %149 = arith.index_cast %148 : i32 to index
                    %150 = memref.load %146[%c0, %arg7, %149, %82] : memref<?x?x?x?xf32>
                    %151 = arith.addf %147, %150 : f32
                    %152 = arith.addi %83, %140 : i32
                    %153 = arith.index_cast %152 : i32 to index
                    %154 = memref.load %146[%c0, %arg7, %arg6, %153] : memref<?x?x?x?xf32>
                    %155 = arith.addf %151, %154 : f32
                    %156 = arith.mulf %143, %155 : f32
                    %157 = memref.load %alloca_34[] : memref<f32>
                    %158 = arith.addf %157, %156 : f32
                    memref.store %158, %alloca_34[] : memref<f32>
                  } {arts.id = 151 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                  %98 = arith.addi %93, %c1_i32 : i32
                  %99 = arith.index_cast %98 : i32 to index
                  %100 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  %101 = memref.load %100[%c0, %99, %arg6, %82] : memref<?x?x?x?xf32>
                  %102 = arith.addi %93, %c-1_i32 : i32
                  %103 = arith.index_cast %102 : i32 to index
                  %104 = memref.load %100[%c0, %103, %arg6, %82] : memref<?x?x?x?xf32>
                  %105 = arith.subf %101, %104 : f32
                  memref.store %105, %alloca_35[] : memref<f32>
                  %106 = memref.load %alloca_27[] : memref<f32>
                  %107 = memref.load %alloca_34[] : memref<f32>
                  %108 = arith.mulf %106, %107 : f32
                  %109 = memref.load %alloca_31[] : memref<f32>
                  %110 = arith.addf %109, %106 : f32
                  %111 = memref.load %alloca_35[] : memref<f32>
                  %112 = arith.mulf %110, %111 : f32
                  %113 = arith.mulf %112, %cst : f32
                  %114 = arith.addf %108, %113 : f32
                  %115 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
                  memref.store %114, %115[%c0, %arg7, %arg6, %82] : memref<?x?x?x?xf32>
                  memref.store %cst_6, %alloca_30[] : memref<f32>
                  scf.for %arg8 = %c-2 to %c3 step %c1 {
                    %140 = arith.index_cast %arg8 : index to i32
                    %141 = arith.addi %140, %c2_i32 : i32
                    %142 = arith.index_cast %141 : i32 to index
                    %143 = memref.load %alloca_32[%142] : memref<5xf32>
                    %144 = arith.addi %93, %140 : i32
                    %145 = arith.index_cast %144 : i32 to index
                    %146 = memref.load %100[%c1, %145, %arg6, %82] : memref<?x?x?x?xf32>
                    %147 = arith.addi %88, %140 : i32
                    %148 = arith.index_cast %147 : i32 to index
                    %149 = memref.load %100[%c1, %arg7, %148, %82] : memref<?x?x?x?xf32>
                    %150 = arith.addf %146, %149 : f32
                    %151 = arith.addi %83, %140 : i32
                    %152 = arith.index_cast %151 : i32 to index
                    %153 = memref.load %100[%c1, %arg7, %arg6, %152] : memref<?x?x?x?xf32>
                    %154 = arith.addf %150, %153 : f32
                    %155 = arith.mulf %143, %154 : f32
                    %156 = memref.load %alloca_30[] : memref<f32>
                    %157 = arith.addf %156, %155 : f32
                    memref.store %157, %alloca_30[] : memref<f32>
                  } {arts.id = 191 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                  %116 = memref.load %100[%c1, %arg7, %90, %82] : memref<?x?x?x?xf32>
                  %117 = memref.load %100[%c1, %arg7, %92, %82] : memref<?x?x?x?xf32>
                  %118 = arith.subf %116, %117 : f32
                  memref.store %118, %alloca_28[] : memref<f32>
                  %119 = memref.load %alloca_27[] : memref<f32>
                  %120 = memref.load %alloca_30[] : memref<f32>
                  %121 = arith.mulf %119, %120 : f32
                  %122 = memref.load %alloca_31[] : memref<f32>
                  %123 = arith.addf %122, %119 : f32
                  %124 = memref.load %alloca_28[] : memref<f32>
                  %125 = arith.mulf %123, %124 : f32
                  %126 = arith.mulf %125, %cst : f32
                  %127 = arith.addf %121, %126 : f32
                  memref.store %127, %115[%c1, %arg7, %arg6, %82] : memref<?x?x?x?xf32>
                  memref.store %cst_6, %alloca_29[] : memref<f32>
                  scf.for %arg8 = %c-2 to %c3 step %c1 {
                    %140 = arith.index_cast %arg8 : index to i32
                    %141 = arith.addi %140, %c2_i32 : i32
                    %142 = arith.index_cast %141 : i32 to index
                    %143 = memref.load %alloca_32[%142] : memref<5xf32>
                    %144 = arith.addi %93, %140 : i32
                    %145 = arith.index_cast %144 : i32 to index
                    %146 = memref.load %100[%c2, %145, %arg6, %82] : memref<?x?x?x?xf32>
                    %147 = arith.addi %88, %140 : i32
                    %148 = arith.index_cast %147 : i32 to index
                    %149 = memref.load %100[%c2, %arg7, %148, %82] : memref<?x?x?x?xf32>
                    %150 = arith.addf %146, %149 : f32
                    %151 = arith.addi %83, %140 : i32
                    %152 = arith.index_cast %151 : i32 to index
                    %153 = memref.load %100[%c2, %arg7, %arg6, %152] : memref<?x?x?x?xf32>
                    %154 = arith.addf %150, %153 : f32
                    %155 = arith.mulf %143, %154 : f32
                    %156 = memref.load %alloca_29[] : memref<f32>
                    %157 = arith.addf %156, %155 : f32
                    memref.store %157, %alloca_29[] : memref<f32>
                  } {arts.id = 227 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
                  %128 = memref.load %100[%c2, %arg7, %arg6, %85] : memref<?x?x?x?xf32>
                  %129 = memref.load %100[%c2, %arg7, %arg6, %87] : memref<?x?x?x?xf32>
                  %130 = arith.subf %128, %129 : f32
                  memref.store %130, %alloca_33[] : memref<f32>
                  %131 = memref.load %alloca_27[] : memref<f32>
                  %132 = memref.load %alloca_29[] : memref<f32>
                  %133 = arith.mulf %131, %132 : f32
                  %134 = memref.load %alloca_31[] : memref<f32>
                  %135 = arith.addf %134, %131 : f32
                  %136 = memref.load %alloca_33[] : memref<f32>
                  %137 = arith.mulf %135, %136 : f32
                  %138 = arith.mulf %137, %cst : f32
                  %139 = arith.addf %133, %138 : f32
                  memref.store %139, %115[%c2, %arg7, %arg6, %82] : memref<?x?x?x?xf32>
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
        %12 = arts.db_ref %ptr_17[%c0] : memref<?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
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
    arts.db_free(%guid_24) : memref<?xi64>
    arts.db_free(%ptr_25) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_20) : memref<?xi64>
    arts.db_free(%ptr_21) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?x?xf32>>
    arts.db_free(%guid_16) : memref<?xi64>
    arts.db_free(%ptr_17) : memref<?xmemref<?x?x?x?xf32>>
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
