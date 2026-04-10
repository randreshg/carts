module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem3d_update_stress\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c5 = arith.constant 5 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 2.000000e+00 : f64
    %cst_0 = arith.constant 5.000000e-01 : f64
    %c-1_i32 = arith.constant -1 : i32
    %c38 = arith.constant 38 : index
    %cst_1 = arith.constant 1.000000e-03 : f64
    %c17_i32 = arith.constant 17 : i32
    %cst_2 = arith.constant 1.500000e-03 : f64
    %c3_i32 = arith.constant 3 : i32
    %c19_i32 = arith.constant 19 : i32
    %cst_3 = arith.constant 8.000000e-04 : f64
    %c5_i32 = arith.constant 5 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_4 = arith.constant 3.000000e+01 : f64
    %cst_5 = arith.constant 5.000000e-02 : f64
    %c11_i32 = arith.constant 11 : i32
    %cst_6 = arith.constant 2.000000e+01 : f64
    %cst_7 = arith.constant 4.000000e-02 : f64
    %c13_i32 = arith.constant 13 : i32
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c40 = arith.constant 40 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_8 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 254 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:153:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 254 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %c0_9 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 81 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:97:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 81 : i64, lastUseId = 82 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 81 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c0_10 = arith.constant 0 : index
    %guid_11, %ptr_12 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 83 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:98:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 83 : i64, lastUseId = 84 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 83 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c0_13 = arith.constant 0 : index
    %guid_14, %ptr_15 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 85 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:99:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 85 : i64, lastUseId = 86 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 85 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c0_16 = arith.constant 0 : index
    %guid_17, %ptr_18 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 89 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:101:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 89 : i64, lastUseId = 90 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 89 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c0_19 = arith.constant 0 : index
    %guid_20, %ptr_21 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 91 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:102:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 91 : i64, lastUseId = 92 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 91 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_22 = arith.constant 1 : index
    %c0_23 = arith.constant 0 : index
    %guid_24, %ptr_25 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 93 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:103:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 93 : i64, lastUseId = 94 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 9 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 93 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_26 = arith.constant 1 : index
    %c0_27 = arith.constant 0 : index
    %guid_28, %ptr_29 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 95 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:104:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 95 : i64, lastUseId = 96 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 9 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 95 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_30 = arith.constant 1 : index
    %c0_31 = arith.constant 0 : index
    %guid_32, %ptr_33 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 97 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:105:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 97 : i64, lastUseId = 98 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 97 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_34 = arith.constant 1 : index
    %c0_35 = arith.constant 0 : index
    %guid_36, %ptr_37 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:106:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 99 : i64, lastUseId = 100 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 99 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_38 = arith.constant 1 : index
    %c0_39 = arith.constant 0 : index
    %guid_40, %ptr_41 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 101 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:107:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 101 : i64, lastUseId = 102 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 101 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_42 = arith.constant 1 : index
    %c0_43 = arith.constant 0 : index
    %guid_44, %ptr_45 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:108:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 103 : i64, lastUseId = 104 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 103 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_46 = memref.alloca() {arts.id = 106 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:139:3", totalAccesses = 10 : i64, readCount = 7 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 106 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_46[] : memref<i32>
    memref.store %c0_i32, %alloca_46[] : memref<i32>
    scf.for %arg0 = %c0 to %c40 step %c1 {
      scf.for %arg1 = %c0 to %c40 step %c1 {
        scf.for %arg2 = %c0 to %c40 step %c1 {
          %12 = memref.load %alloca_46[] : memref<i32>
          %13 = arith.remsi %12, %c17_i32 : i32
          %14 = arith.sitofp %13 : i32 to f64
          %15 = arith.mulf %14, %cst_1 : f64
          %16 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %15, %16[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %17 = memref.load %alloca_46[] : memref<i32>
          %18 = arith.muli %17, %c3_i32 : i32
          %19 = arith.remsi %18, %c19_i32 : i32
          %20 = arith.sitofp %19 : i32 to f64
          %21 = arith.mulf %20, %cst_2 : f64
          %22 = arts.db_ref %ptr_12[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %21, %22[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %23 = memref.load %alloca_46[] : memref<i32>
          %24 = arith.muli %23, %c5_i32 : i32
          %25 = arith.remsi %24, %c23_i32 : i32
          %26 = arith.sitofp %25 : i32 to f64
          %27 = arith.mulf %26, %cst_3 : f64
          %28 = arts.db_ref %ptr_15[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %27, %28[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %29 = memref.load %alloca_46[] : memref<i32>
          %30 = arith.remsi %29, %c11_i32 : i32
          %31 = arith.sitofp %30 : i32 to f64
          %32 = arith.mulf %31, %cst_5 : f64
          %33 = arith.addf %32, %cst_4 : f64
          %34 = arts.db_ref %ptr_18[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %33, %34[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %35 = memref.load %alloca_46[] : memref<i32>
          %36 = arith.remsi %35, %c13_i32 : i32
          %37 = arith.sitofp %36 : i32 to f64
          %38 = arith.mulf %37, %cst_7 : f64
          %39 = arith.addf %38, %cst_6 : f64
          %40 = arts.db_ref %ptr_21[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %39, %40[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %41 = arts.db_ref %ptr_45[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %41[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %42 = arts.db_ref %ptr_41[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %42[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %43 = arts.db_ref %ptr_37[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %43[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %44 = arts.db_ref %ptr_33[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %44[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %45 = arts.db_ref %ptr_29[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %45[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %46 = arts.db_ref %ptr_25[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %46[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %47 = memref.load %alloca_46[] : memref<i32>
          %48 = arith.addi %47, %c1_i32 : i32
          memref.store %48, %alloca_46[] : memref<i32>
        } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 155 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_47 = memref.alloca() {arts.id = 158 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 158 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_48 = memref.alloca() {arts.id = 170 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 170 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 48 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_49 = memref.alloca() {arts.id = 160 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 160 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_50 = memref.alloca() {arts.id = 166 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 166 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_51 = memref.alloca() {arts.id = 168 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 168 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_52 = memref.alloca() {arts.id = 162 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 162 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_53 = memref.alloca() {arts.id = 164 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 164 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %8 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %12 = arith.muli %arg0, %c5 : index
        %13 = arith.cmpi uge, %12, %c38 : index
        %14 = arith.subi %c38, %12 : index
        %15 = arith.select %13, %c0, %14 : index
        %16 = arith.minui %15, %c5 : index
        %17 = arith.cmpi ugt, %16, %c0 : index
        scf.if %17 {
          %18 = arith.cmpi slt, %14, %c0 : index
          %19 = arith.select %18, %c0, %14 : index
          %20 = arith.minui %19, %16 : index
          %guid_54, %ptr_55 = arts.db_acquire[<in>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?x?x?xf64>>) partitioning(<coarse>), offsets[%c0_16], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_55 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %21 = arith.cmpi slt, %14, %c0 : index
          %22 = arith.select %21, %c0, %14 : index
          %23 = arith.minui %22, %16 : index
          %guid_56, %ptr_57 = arts.db_acquire[<in>] (%guid_20 : memref<?xi64>, %ptr_21 : memref<?xmemref<?x?x?xf64>>) partitioning(<coarse>), offsets[%c0_19], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_57 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %24 = arith.cmpi slt, %14, %c0 : index
          %25 = arith.select %24, %c0, %14 : index
          %26 = arith.minui %25, %16 : index
          %27 = arith.index_cast %c-1_i32 : i32 to index
          %c18 = arith.constant 18 : index
          %28 = arith.cmpi uge, %27, %c18 : index
          %29 = arith.subi %27, %c18 : index
          %c0_58 = arith.constant 0 : index
          %30 = arith.select %28, %29, %c0_58 : index
          %c36 = arith.constant 36 : index
          %31 = arith.addi %26, %c36 : index
          %guid_59, %ptr_60 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?xf64>>) partitioning(<coarse>), offsets[%c0_9], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_60 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %32 = arith.cmpi slt, %14, %c0 : index
          %33 = arith.select %32, %c0, %14 : index
          %34 = arith.minui %33, %16 : index
          %35 = arith.index_cast %c-1_i32 : i32 to index
          %c18_61 = arith.constant 18 : index
          %36 = arith.cmpi uge, %35, %c18_61 : index
          %37 = arith.subi %35, %c18_61 : index
          %c0_62 = arith.constant 0 : index
          %38 = arith.select %36, %37, %c0_62 : index
          %c36_63 = arith.constant 36 : index
          %39 = arith.addi %34, %c36_63 : index
          %guid_64, %ptr_65 = arts.db_acquire[<in>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?x?xf64>>) partitioning(<coarse>), offsets[%c0_10], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_65 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %40 = arith.cmpi slt, %14, %c0 : index
          %41 = arith.select %40, %c0, %14 : index
          %42 = arith.minui %41, %16 : index
          %c1_66 = arith.constant 1 : index
          %43 = arith.cmpi uge, %12, %c1_66 : index
          %44 = arith.subi %12, %c1_66 : index
          %c0_67 = arith.constant 0 : index
          %45 = arith.select %43, %44, %c0_67 : index
          %c2_68 = arith.constant 2 : index
          %46 = arith.addi %42, %c2_68 : index
          %guid_69, %ptr_70 = arts.db_acquire[<in>] (%guid_14 : memref<?xi64>, %ptr_15 : memref<?xmemref<?x?x?xf64>>) partitioning(<coarse>), offsets[%c0_13], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_70 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %47 = arith.divui %12, %c40 : index
          %48 = arith.addi %12, %c4 : index
          %49 = arith.divui %48, %c40 : index
          %50 = arith.cmpi ugt, %47, %c0 : index
          %51 = arith.select %50, %49, %c0 : index
          %52 = arith.subi %51, %47 : index
          %53 = arith.addi %52, %c1 : index
          %54 = arith.select %50, %c0, %47 : index
          %55 = arith.select %50, %c0, %53 : index
          %guid_71, %ptr_72 = arts.db_acquire[<inout>] (%guid_24 : memref<?xi64>, %ptr_25 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%54], sizes[%55] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_72 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_73, %ptr_74 = arts.db_acquire[<inout>] (%guid_28 : memref<?xi64>, %ptr_29 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%54], sizes[%55] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_74 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_75, %ptr_76 = arts.db_acquire[<inout>] (%guid_32 : memref<?xi64>, %ptr_33 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%54], sizes[%55] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_76 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_77, %ptr_78 = arts.db_acquire[<inout>] (%guid_36 : memref<?xi64>, %ptr_37 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%54], sizes[%55] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_78 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_79, %ptr_80 = arts.db_acquire[<inout>] (%guid_40 : memref<?xi64>, %ptr_41 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%54], sizes[%55] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_80 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_81, %ptr_82 = arts.db_acquire[<inout>] (%guid_44 : memref<?xi64>, %ptr_45 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%54], sizes[%55] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_82 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_55, %ptr_57, %ptr_60, %ptr_65, %ptr_70, %ptr_72, %ptr_74, %ptr_76, %ptr_78, %ptr_80, %ptr_82) : memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>> attributes {arts.id = 277 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5]} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf64>>, %arg2: memref<?xmemref<?x?x?xf64>>, %arg3: memref<?xmemref<?x?x?xf64>>, %arg4: memref<?xmemref<?x?x?xf64>>, %arg5: memref<?xmemref<?x?x?xf64>>, %arg6: memref<?xmemref<?x?x?xf64>>, %arg7: memref<?xmemref<?x?x?xf64>>, %arg8: memref<?xmemref<?x?x?xf64>>, %arg9: memref<?xmemref<?x?x?xf64>>, %arg10: memref<?xmemref<?x?x?xf64>>, %arg11: memref<?xmemref<?x?x?xf64>>):
            %56 = arith.subi %c2, %12 : index
            %57 = arith.cmpi slt, %56, %c0 : index
            %58 = arith.select %57, %c0, %56 : index
            %59 = arith.cmpi slt, %14, %c0 : index
            %60 = arith.select %59, %c0, %14 : index
            %61 = arith.minui %60, %16 : index
            scf.for %arg12 = %58 to %61 step %c1 {
              %62 = arith.addi %12, %arg12 : index
              memref.store %4, %alloca_47[] : memref<f64>
              memref.store %4, %alloca_49[] : memref<f64>
              memref.store %4, %alloca_52[] : memref<f64>
              memref.store %4, %alloca_53[] : memref<f64>
              memref.store %4, %alloca_50[] : memref<f64>
              memref.store %4, %alloca_51[] : memref<f64>
              memref.store %4, %alloca_48[] : memref<f64>
              %63 = arith.index_cast %62 : index to i32
              scf.for %arg13 = %c2 to %c38 step %c1 {
                %64 = arith.index_cast %arg13 : index to i32
                scf.for %arg14 = %c2 to %c38 step %c1 {
                  %65 = arith.index_cast %arg14 : index to i32
                  %66 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %67 = memref.load %66[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  memref.store %67, %alloca_48[] : memref<f64>
                  %68 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %69 = memref.load %68[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  memref.store %69, %alloca_51[] : memref<f64>
                  %70 = arith.addi %65, %c1_i32 : i32
                  %71 = arith.index_cast %70 : i32 to index
                  %72 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %73 = memref.load %72[%71, %arg13, %62] : memref<?x?x?xf64>
                  %74 = arith.addi %65, %c-1_i32 : i32
                  %75 = arith.index_cast %74 : i32 to index
                  %76 = memref.load %72[%75, %arg13, %62] : memref<?x?x?xf64>
                  %77 = arith.subf %73, %76 : f64
                  %78 = arith.mulf %77, %cst_0 : f64
                  memref.store %78, %alloca_50[] : memref<f64>
                  %79 = arith.addi %64, %c1_i32 : i32
                  %80 = arith.index_cast %79 : i32 to index
                  %81 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %82 = memref.load %81[%arg14, %80, %62] : memref<?x?x?xf64>
                  %83 = arith.addi %64, %c-1_i32 : i32
                  %84 = arith.index_cast %83 : i32 to index
                  %85 = memref.load %81[%arg14, %84, %62] : memref<?x?x?xf64>
                  %86 = arith.subf %82, %85 : f64
                  %87 = arith.mulf %86, %cst_0 : f64
                  memref.store %87, %alloca_53[] : memref<f64>
                  %88 = arith.addi %63, %c1_i32 : i32
                  %89 = arith.index_cast %88 : i32 to index
                  %90 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %91 = memref.load %90[%arg14, %arg13, %89] : memref<?x?x?xf64>
                  %92 = arith.addi %63, %c-1_i32 : i32
                  %93 = arith.index_cast %92 : i32 to index
                  %94 = memref.load %90[%arg14, %arg13, %93] : memref<?x?x?xf64>
                  %95 = arith.subf %91, %94 : f64
                  %96 = arith.mulf %95, %cst_0 : f64
                  memref.store %96, %alloca_52[] : memref<f64>
                  %97 = memref.load %alloca_50[] : memref<f64>
                  %98 = memref.load %alloca_53[] : memref<f64>
                  %99 = arith.addf %97, %98 : f64
                  %100 = memref.load %alloca_52[] : memref<f64>
                  %101 = arith.addf %99, %100 : f64
                  memref.store %101, %alloca_49[] : memref<f64>
                  %102 = memref.load %alloca_48[] : memref<f64>
                  %103 = arith.mulf %102, %cst : f64
                  memref.store %103, %alloca_47[] : memref<f64>
                  %104 = memref.load %alloca_47[] : memref<f64>
                  %105 = memref.load %alloca_50[] : memref<f64>
                  %106 = arith.mulf %104, %105 : f64
                  %107 = memref.load %alloca_51[] : memref<f64>
                  %108 = memref.load %alloca_49[] : memref<f64>
                  %109 = arith.mulf %107, %108 : f64
                  %110 = arith.addf %106, %109 : f64
                  %111 = arith.mulf %110, %cst_1 : f64
                  %112 = arts.db_ref %arg6[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %113 = memref.load %112[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %114 = arith.addf %113, %111 : f64
                  memref.store %114, %112[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %115 = memref.load %alloca_47[] : memref<f64>
                  %116 = memref.load %alloca_53[] : memref<f64>
                  %117 = arith.mulf %115, %116 : f64
                  %118 = memref.load %alloca_51[] : memref<f64>
                  %119 = memref.load %alloca_49[] : memref<f64>
                  %120 = arith.mulf %118, %119 : f64
                  %121 = arith.addf %117, %120 : f64
                  %122 = arith.mulf %121, %cst_1 : f64
                  %123 = arts.db_ref %arg7[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %124 = memref.load %123[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %125 = arith.addf %124, %122 : f64
                  memref.store %125, %123[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %126 = memref.load %alloca_47[] : memref<f64>
                  %127 = memref.load %alloca_52[] : memref<f64>
                  %128 = arith.mulf %126, %127 : f64
                  %129 = memref.load %alloca_51[] : memref<f64>
                  %130 = memref.load %alloca_49[] : memref<f64>
                  %131 = arith.mulf %129, %130 : f64
                  %132 = arith.addf %128, %131 : f64
                  %133 = arith.mulf %132, %cst_1 : f64
                  %134 = arts.db_ref %arg8[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %135 = memref.load %134[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %136 = arith.addf %135, %133 : f64
                  memref.store %136, %134[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %137 = memref.load %alloca_48[] : memref<f64>
                  %138 = arith.mulf %137, %cst_1 : f64
                  %139 = memref.load %72[%arg14, %80, %62] : memref<?x?x?xf64>
                  %140 = memref.load %72[%arg14, %84, %62] : memref<?x?x?xf64>
                  %141 = arith.subf %139, %140 : f64
                  %142 = arith.mulf %141, %cst_0 : f64
                  %143 = memref.load %81[%71, %arg13, %62] : memref<?x?x?xf64>
                  %144 = memref.load %81[%75, %arg13, %62] : memref<?x?x?xf64>
                  %145 = arith.subf %143, %144 : f64
                  %146 = arith.mulf %145, %cst_0 : f64
                  %147 = arith.addf %142, %146 : f64
                  %148 = arith.mulf %138, %147 : f64
                  %149 = arts.db_ref %arg9[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %150 = memref.load %149[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %151 = arith.addf %150, %148 : f64
                  memref.store %151, %149[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %152 = memref.load %alloca_48[] : memref<f64>
                  %153 = arith.mulf %152, %cst_1 : f64
                  %154 = memref.load %72[%arg14, %arg13, %89] : memref<?x?x?xf64>
                  %155 = memref.load %72[%arg14, %arg13, %93] : memref<?x?x?xf64>
                  %156 = arith.subf %154, %155 : f64
                  %157 = arith.mulf %156, %cst_0 : f64
                  %158 = memref.load %90[%71, %arg13, %62] : memref<?x?x?xf64>
                  %159 = memref.load %90[%75, %arg13, %62] : memref<?x?x?xf64>
                  %160 = arith.subf %158, %159 : f64
                  %161 = arith.mulf %160, %cst_0 : f64
                  %162 = arith.addf %157, %161 : f64
                  %163 = arith.mulf %153, %162 : f64
                  %164 = arts.db_ref %arg10[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %165 = memref.load %164[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %166 = arith.addf %165, %163 : f64
                  memref.store %166, %164[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %167 = memref.load %alloca_48[] : memref<f64>
                  %168 = arith.mulf %167, %cst_1 : f64
                  %169 = memref.load %81[%arg14, %arg13, %89] : memref<?x?x?xf64>
                  %170 = memref.load %81[%arg14, %arg13, %93] : memref<?x?x?xf64>
                  %171 = arith.subf %169, %170 : f64
                  %172 = arith.mulf %171, %cst_0 : f64
                  %173 = memref.load %90[%arg14, %80, %62] : memref<?x?x?xf64>
                  %174 = memref.load %90[%arg14, %84, %62] : memref<?x?x?xf64>
                  %175 = arith.subf %173, %174 : f64
                  %176 = arith.mulf %175, %cst_0 : f64
                  %177 = arith.addf %172, %176 : f64
                  %178 = arith.mulf %168, %177 : f64
                  %179 = arts.db_ref %arg11[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %180 = memref.load %179[%arg14, %arg13, %62] : memref<?x?x?xf64>
                  %181 = arith.addf %180, %178 : f64
                  memref.store %181, %179[%arg14, %arg13, %62] : memref<?x?x?xf64>
                } {arts.id = 246 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 36 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 13 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:145:5">, arts.metadata_provenance = "exact"}
              } {arts.id = 248 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 36 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 13 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:145:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 276 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "stress_update.c:145:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg1) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg2) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg3) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg4) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg5) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg6) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg7) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg8) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg9) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg10) : memref<?xmemref<?x?x?xf64>>
            arts.db_release(%arg11) : memref<?xmemref<?x?x?xf64>>
          }
        }
      }
    } : i64
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_8, %alloca[] : memref<f64>
    scf.for %arg0 = %c0 to %c40 step %c1 {
      %12 = arts.db_ref %ptr_25[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %13 = memref.load %12[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %14 = arts.db_ref %ptr_29[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %15 = memref.load %14[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %16 = arith.addf %13, %15 : f64
      %17 = arts.db_ref %ptr_33[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %18 = memref.load %17[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %19 = arith.addf %16, %18 : f64
      %20 = arts.db_ref %ptr_37[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %21 = memref.load %20[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %22 = arith.addf %19, %21 : f64
      %23 = arts.db_ref %ptr_41[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %24 = memref.load %23[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %25 = arith.addf %22, %24 : f64
      %26 = arts.db_ref %ptr_45[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %27 = memref.load %26[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %28 = arith.addf %25, %27 : f64
      %29 = memref.load %alloca[] : memref<f64>
      %30 = arith.addf %29, %28 : f64
      memref.store %30, %alloca[] : memref<f64>
    } {arts.id = 240 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:157:3">, arts.metadata_provenance = "recovered"}
    %10 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%10) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%11, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_24) : memref<?xi64>
    arts.db_free(%ptr_25) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_32) : memref<?xi64>
    arts.db_free(%ptr_33) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_40) : memref<?xi64>
    arts.db_free(%ptr_41) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_20) : memref<?xi64>
    arts.db_free(%ptr_21) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_36) : memref<?xi64>
    arts.db_free(%ptr_37) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_44) : memref<?xi64>
    arts.db_free(%ptr_45) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_14) : memref<?xi64>
    arts.db_free(%ptr_15) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_28) : memref<?xi64>
    arts.db_free(%ptr_29) : memref<?xmemref<?x?x?xf64>>
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
