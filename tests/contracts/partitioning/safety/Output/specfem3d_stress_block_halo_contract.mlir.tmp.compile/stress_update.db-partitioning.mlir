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
    %c1_9 = arith.constant 1 : index
    %c0_10 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 81 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:97:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 81 : i64, lastUseId = 82 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 81 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_11 = arith.constant 1 : index
    %c0_12 = arith.constant 0 : index
    %guid_13, %ptr_14 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 83 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:98:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 83 : i64, lastUseId = 84 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 83 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_15 = arith.constant 1 : index
    %c0_16 = arith.constant 0 : index
    %guid_17, %ptr_18 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 85 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:99:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 85 : i64, lastUseId = 86 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 56 : i64>, arts.metadata_origin_id = 85 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_19 = arith.constant 1 : index
    %c0_20 = arith.constant 0 : index
    %guid_21, %ptr_22 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 89 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:101:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 89 : i64, lastUseId = 90 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 89 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_23 = arith.constant 1 : index
    %c0_24 = arith.constant 0 : index
    %guid_25, %ptr_26 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 91 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:102:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 91 : i64, lastUseId = 92 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 91 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_27 = arith.constant 1 : index
    %c0_28 = arith.constant 0 : index
    %guid_29, %ptr_30 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 93 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:103:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 93 : i64, lastUseId = 94 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 9 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 93 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_31 = arith.constant 1 : index
    %c0_32 = arith.constant 0 : index
    %guid_33, %ptr_34 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 95 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:104:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 95 : i64, lastUseId = 96 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 9 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 95 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_35 = arith.constant 1 : index
    %c0_36 = arith.constant 0 : index
    %guid_37, %ptr_38 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 97 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:105:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 97 : i64, lastUseId = 98 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 97 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_39 = arith.constant 1 : index
    %c0_40 = arith.constant 0 : index
    %guid_41, %ptr_42 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:106:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 99 : i64, lastUseId = 100 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 99 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_43 = arith.constant 1 : index
    %c0_44 = arith.constant 0 : index
    %guid_45, %ptr_46 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 101 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:107:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 101 : i64, lastUseId = 102 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 101 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %c1_47 = arith.constant 1 : index
    %c0_48 = arith.constant 0 : index
    %guid_49, %ptr_50 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c40, %c40, %c40] {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:108:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 512000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 103 : i64, lastUseId = 104 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 103 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_51 = memref.alloca() {arts.id = 106 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:139:3", totalAccesses = 10 : i64, readCount = 7 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 106 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_51[] : memref<i32>
    memref.store %c0_i32, %alloca_51[] : memref<i32>
    scf.for %arg0 = %c0 to %c40 step %c1 {
      scf.for %arg1 = %c0 to %c40 step %c1 {
        scf.for %arg2 = %c0 to %c40 step %c1 {
          %12 = memref.load %alloca_51[] : memref<i32>
          %13 = arith.remsi %12, %c17_i32 : i32
          %14 = arith.sitofp %13 : i32 to f64
          %15 = arith.mulf %14, %cst_1 : f64
          %16 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %15, %16[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %17 = memref.load %alloca_51[] : memref<i32>
          %18 = arith.muli %17, %c3_i32 : i32
          %19 = arith.remsi %18, %c19_i32 : i32
          %20 = arith.sitofp %19 : i32 to f64
          %21 = arith.mulf %20, %cst_2 : f64
          %22 = arts.db_ref %ptr_14[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %21, %22[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %23 = memref.load %alloca_51[] : memref<i32>
          %24 = arith.muli %23, %c5_i32 : i32
          %25 = arith.remsi %24, %c23_i32 : i32
          %26 = arith.sitofp %25 : i32 to f64
          %27 = arith.mulf %26, %cst_3 : f64
          %28 = arts.db_ref %ptr_18[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %27, %28[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %29 = memref.load %alloca_51[] : memref<i32>
          %30 = arith.remsi %29, %c11_i32 : i32
          %31 = arith.sitofp %30 : i32 to f64
          %32 = arith.mulf %31, %cst_5 : f64
          %33 = arith.addf %32, %cst_4 : f64
          %34 = arts.db_ref %ptr_22[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %33, %34[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %35 = memref.load %alloca_51[] : memref<i32>
          %36 = arith.remsi %35, %c13_i32 : i32
          %37 = arith.sitofp %36 : i32 to f64
          %38 = arith.mulf %37, %cst_7 : f64
          %39 = arith.addf %38, %cst_6 : f64
          %40 = arts.db_ref %ptr_26[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %39, %40[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %41 = arts.db_ref %ptr_50[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %41[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %42 = arts.db_ref %ptr_46[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %42[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %43 = arts.db_ref %ptr_42[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %43[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %44 = arts.db_ref %ptr_38[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %44[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %45 = arts.db_ref %ptr_34[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %45[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %46 = arts.db_ref %ptr_30[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
          memref.store %cst_8, %46[%arg0, %arg1, %arg2] : memref<?x?x?xf64>
          %47 = memref.load %alloca_51[] : memref<i32>
          %48 = arith.addi %47, %c1_i32 : i32
          memref.store %48, %alloca_51[] : memref<i32>
        } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 155 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_52 = memref.alloca() {arts.id = 164 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 164 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_53 = memref.alloca() {arts.id = 166 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 166 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_54 = memref.alloca() {arts.id = 162 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 162 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_55 = memref.alloca() {arts.id = 168 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 168 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_56 = memref.alloca() {arts.id = 160 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 160 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_57 = memref.alloca() {arts.id = 170 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 170 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 48 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_58 = memref.alloca() {arts.id = 158 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 158 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %8 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %12 = arith.muli %arg0, %c5 : index
        %13 = arith.cmpi uge, %12, %c38 : index
        %14 = arith.subi %c38, %12 : index
        %15 = arith.select %13, %c0, %14 : index
        %16 = arith.minui %15, %c5 : index
        %17 = arith.cmpi ugt, %16, %c0 : index
        scf.if %17 {
          %18 = arith.addi %12, %16 : index
          %19 = arith.cmpi uge, %12, %c1 : index
          %20 = arith.subi %12, %c1 : index
          %21 = arith.select %19, %20, %c0 : index
          %22 = arith.addi %18, %c1 : index
          %23 = arith.minui %22, %c40 : index
          %24 = arith.cmpi uge, %23, %21 : index
          %25 = arith.subi %23, %21 : index
          %26 = arith.select %24, %25, %c0 : index
          %27 = arith.maxui %26, %c1 : index
          %28 = arith.divui %21, %c40 : index
          %29 = arith.addi %21, %27 : index
          %30 = arith.subi %29, %c1 : index
          %31 = arith.divui %30, %c40 : index
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
          %guid_59, %ptr_60 = arts.db_acquire[<in>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_60 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %44 = arith.cmpi slt, %14, %c0 : index
          %45 = arith.select %44, %c0, %14 : index
          %46 = arith.minui %45, %16 : index
          %47 = arith.cmpi slt, %14, %c0 : index
          %48 = arith.select %47, %c0, %14 : index
          %49 = arith.minui %48, %16 : index
          %guid_61, %ptr_62 = arts.db_acquire[<in>] (%guid_25 : memref<?xi64>, %ptr_26 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_62 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %50 = arith.cmpi slt, %14, %c0 : index
          %51 = arith.select %50, %c0, %14 : index
          %52 = arith.minui %51, %16 : index
          %53 = arith.index_cast %c-1_i32 : i32 to index
          %c18 = arith.constant 18 : index
          %54 = arith.cmpi uge, %53, %c18 : index
          %55 = arith.subi %53, %c18 : index
          %c0_63 = arith.constant 0 : index
          %56 = arith.select %54, %55, %c0_63 : index
          %c36 = arith.constant 36 : index
          %57 = arith.addi %52, %c36 : index
          %guid_64, %ptr_65 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_65 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %58 = arith.cmpi slt, %14, %c0 : index
          %59 = arith.select %58, %c0, %14 : index
          %60 = arith.minui %59, %16 : index
          %61 = arith.index_cast %c-1_i32 : i32 to index
          %c18_66 = arith.constant 18 : index
          %62 = arith.cmpi uge, %61, %c18_66 : index
          %63 = arith.subi %61, %c18_66 : index
          %c0_67 = arith.constant 0 : index
          %64 = arith.select %62, %63, %c0_67 : index
          %c36_68 = arith.constant 36 : index
          %65 = arith.addi %60, %c36_68 : index
          %guid_69, %ptr_70 = arts.db_acquire[<in>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_70 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %66 = arith.cmpi slt, %14, %c0 : index
          %67 = arith.select %66, %c0, %14 : index
          %68 = arith.minui %67, %16 : index
          %c1_71 = arith.constant 1 : index
          %69 = arith.cmpi uge, %12, %c1_71 : index
          %70 = arith.subi %12, %c1_71 : index
          %c0_72 = arith.constant 0 : index
          %71 = arith.select %69, %70, %c0_72 : index
          %c2_73 = arith.constant 2 : index
          %72 = arith.addi %68, %c2_73 : index
          %guid_74, %ptr_75 = arts.db_acquire[<in>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_75 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %73 = arith.divui %12, %c40 : index
          %74 = arith.addi %12, %c4 : index
          %75 = arith.divui %74, %c40 : index
          %76 = arith.cmpi ugt, %73, %c0 : index
          %77 = arith.select %76, %75, %c0 : index
          %78 = arith.subi %77, %73 : index
          %79 = arith.addi %78, %c1 : index
          %80 = arith.select %76, %c0, %73 : index
          %81 = arith.select %76, %c0, %79 : index
          %guid_76, %ptr_77 = arts.db_acquire[<inout>] (%guid_29 : memref<?xi64>, %ptr_30 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_77 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_78, %ptr_79 = arts.db_acquire[<inout>] (%guid_33 : memref<?xi64>, %ptr_34 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_79 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_80, %ptr_81 = arts.db_acquire[<inout>] (%guid_37 : memref<?xi64>, %ptr_38 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_81 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_82, %ptr_83 = arts.db_acquire[<inout>] (%guid_41 : memref<?xi64>, %ptr_42 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_83 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_84, %ptr_85 = arts.db_acquire[<inout>] (%guid_45 : memref<?xi64>, %ptr_46 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_85 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_86, %ptr_87 = arts.db_acquire[<inout>] (%guid_49 : memref<?xi64>, %ptr_50 : memref<?xmemref<?x?x?xf64>>) partitioning(<block>, offsets[%12], sizes[%c5]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [40, 40, 5], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf64>>)
          arts.lowering_contract(%ptr_87 : memref<?xmemref<?x?x?xf64>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c40] contract(<ownerDims = [2], postDbRefined = true>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_60, %ptr_62, %ptr_65, %ptr_70, %ptr_75, %ptr_77, %ptr_79, %ptr_81, %ptr_83, %ptr_85, %ptr_87) : memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>>, memref<?xmemref<?x?x?xf64>> attributes {arts.id = 277 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [40, 40, 5]} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf64>>, %arg2: memref<?xmemref<?x?x?xf64>>, %arg3: memref<?xmemref<?x?x?xf64>>, %arg4: memref<?xmemref<?x?x?xf64>>, %arg5: memref<?xmemref<?x?x?xf64>>, %arg6: memref<?xmemref<?x?x?xf64>>, %arg7: memref<?xmemref<?x?x?xf64>>, %arg8: memref<?xmemref<?x?x?xf64>>, %arg9: memref<?xmemref<?x?x?xf64>>, %arg10: memref<?xmemref<?x?x?xf64>>, %arg11: memref<?xmemref<?x?x?xf64>>):
            %82 = arith.subi %c2, %12 : index
            %83 = arith.cmpi slt, %82, %c0 : index
            %84 = arith.select %83, %c0, %82 : index
            %85 = arith.cmpi slt, %14, %c0 : index
            %86 = arith.select %85, %c0, %14 : index
            %87 = arith.minui %86, %16 : index
            scf.for %arg12 = %84 to %87 step %c1 {
              %88 = arith.addi %12, %arg12 : index
              memref.store %4, %alloca_58[] : memref<f64>
              memref.store %4, %alloca_56[] : memref<f64>
              memref.store %4, %alloca_54[] : memref<f64>
              memref.store %4, %alloca_52[] : memref<f64>
              memref.store %4, %alloca_53[] : memref<f64>
              memref.store %4, %alloca_55[] : memref<f64>
              memref.store %4, %alloca_57[] : memref<f64>
              %89 = arith.index_cast %88 : index to i32
              scf.for %arg13 = %c2 to %c38 step %c1 {
                %90 = arith.index_cast %arg13 : index to i32
                scf.for %arg14 = %c2 to %c38 step %c1 {
                  %91 = arith.index_cast %arg14 : index to i32
                  %92 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %93 = memref.load %92[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  memref.store %93, %alloca_57[] : memref<f64>
                  %94 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %95 = memref.load %94[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  memref.store %95, %alloca_55[] : memref<f64>
                  %96 = arith.addi %91, %c1_i32 : i32
                  %97 = arith.index_cast %96 : i32 to index
                  %98 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %99 = memref.load %98[%97, %arg13, %88] : memref<?x?x?xf64>
                  %100 = arith.addi %91, %c-1_i32 : i32
                  %101 = arith.index_cast %100 : i32 to index
                  %102 = memref.load %98[%101, %arg13, %88] : memref<?x?x?xf64>
                  %103 = arith.subf %99, %102 : f64
                  %104 = arith.mulf %103, %cst_0 : f64
                  memref.store %104, %alloca_53[] : memref<f64>
                  %105 = arith.addi %90, %c1_i32 : i32
                  %106 = arith.index_cast %105 : i32 to index
                  %107 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %108 = memref.load %107[%arg14, %106, %88] : memref<?x?x?xf64>
                  %109 = arith.addi %90, %c-1_i32 : i32
                  %110 = arith.index_cast %109 : i32 to index
                  %111 = memref.load %107[%arg14, %110, %88] : memref<?x?x?xf64>
                  %112 = arith.subf %108, %111 : f64
                  %113 = arith.mulf %112, %cst_0 : f64
                  memref.store %113, %alloca_52[] : memref<f64>
                  %114 = arith.addi %89, %c1_i32 : i32
                  %115 = arith.index_cast %114 : i32 to index
                  %116 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %117 = memref.load %116[%arg14, %arg13, %115] : memref<?x?x?xf64>
                  %118 = arith.addi %89, %c-1_i32 : i32
                  %119 = arith.index_cast %118 : i32 to index
                  %120 = memref.load %116[%arg14, %arg13, %119] : memref<?x?x?xf64>
                  %121 = arith.subf %117, %120 : f64
                  %122 = arith.mulf %121, %cst_0 : f64
                  memref.store %122, %alloca_54[] : memref<f64>
                  %123 = memref.load %alloca_53[] : memref<f64>
                  %124 = memref.load %alloca_52[] : memref<f64>
                  %125 = arith.addf %123, %124 : f64
                  %126 = memref.load %alloca_54[] : memref<f64>
                  %127 = arith.addf %125, %126 : f64
                  memref.store %127, %alloca_56[] : memref<f64>
                  %128 = memref.load %alloca_57[] : memref<f64>
                  %129 = arith.mulf %128, %cst : f64
                  memref.store %129, %alloca_58[] : memref<f64>
                  %130 = memref.load %alloca_58[] : memref<f64>
                  %131 = memref.load %alloca_53[] : memref<f64>
                  %132 = arith.mulf %130, %131 : f64
                  %133 = memref.load %alloca_55[] : memref<f64>
                  %134 = memref.load %alloca_56[] : memref<f64>
                  %135 = arith.mulf %133, %134 : f64
                  %136 = arith.addf %132, %135 : f64
                  %137 = arith.mulf %136, %cst_1 : f64
                  %138 = arts.db_ref %arg6[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %139 = memref.load %138[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %140 = arith.addf %139, %137 : f64
                  memref.store %140, %138[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %141 = memref.load %alloca_58[] : memref<f64>
                  %142 = memref.load %alloca_52[] : memref<f64>
                  %143 = arith.mulf %141, %142 : f64
                  %144 = memref.load %alloca_55[] : memref<f64>
                  %145 = memref.load %alloca_56[] : memref<f64>
                  %146 = arith.mulf %144, %145 : f64
                  %147 = arith.addf %143, %146 : f64
                  %148 = arith.mulf %147, %cst_1 : f64
                  %149 = arts.db_ref %arg7[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %150 = memref.load %149[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %151 = arith.addf %150, %148 : f64
                  memref.store %151, %149[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %152 = memref.load %alloca_58[] : memref<f64>
                  %153 = memref.load %alloca_54[] : memref<f64>
                  %154 = arith.mulf %152, %153 : f64
                  %155 = memref.load %alloca_55[] : memref<f64>
                  %156 = memref.load %alloca_56[] : memref<f64>
                  %157 = arith.mulf %155, %156 : f64
                  %158 = arith.addf %154, %157 : f64
                  %159 = arith.mulf %158, %cst_1 : f64
                  %160 = arts.db_ref %arg8[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %161 = memref.load %160[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %162 = arith.addf %161, %159 : f64
                  memref.store %162, %160[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %163 = memref.load %alloca_57[] : memref<f64>
                  %164 = arith.mulf %163, %cst_1 : f64
                  %165 = memref.load %98[%arg14, %106, %88] : memref<?x?x?xf64>
                  %166 = memref.load %98[%arg14, %110, %88] : memref<?x?x?xf64>
                  %167 = arith.subf %165, %166 : f64
                  %168 = arith.mulf %167, %cst_0 : f64
                  %169 = memref.load %107[%97, %arg13, %88] : memref<?x?x?xf64>
                  %170 = memref.load %107[%101, %arg13, %88] : memref<?x?x?xf64>
                  %171 = arith.subf %169, %170 : f64
                  %172 = arith.mulf %171, %cst_0 : f64
                  %173 = arith.addf %168, %172 : f64
                  %174 = arith.mulf %164, %173 : f64
                  %175 = arts.db_ref %arg9[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %176 = memref.load %175[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %177 = arith.addf %176, %174 : f64
                  memref.store %177, %175[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %178 = memref.load %alloca_57[] : memref<f64>
                  %179 = arith.mulf %178, %cst_1 : f64
                  %180 = memref.load %98[%arg14, %arg13, %115] : memref<?x?x?xf64>
                  %181 = memref.load %98[%arg14, %arg13, %119] : memref<?x?x?xf64>
                  %182 = arith.subf %180, %181 : f64
                  %183 = arith.mulf %182, %cst_0 : f64
                  %184 = memref.load %116[%97, %arg13, %88] : memref<?x?x?xf64>
                  %185 = memref.load %116[%101, %arg13, %88] : memref<?x?x?xf64>
                  %186 = arith.subf %184, %185 : f64
                  %187 = arith.mulf %186, %cst_0 : f64
                  %188 = arith.addf %183, %187 : f64
                  %189 = arith.mulf %179, %188 : f64
                  %190 = arts.db_ref %arg10[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %191 = memref.load %190[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %192 = arith.addf %191, %189 : f64
                  memref.store %192, %190[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %193 = memref.load %alloca_57[] : memref<f64>
                  %194 = arith.mulf %193, %cst_1 : f64
                  %195 = memref.load %107[%arg14, %arg13, %115] : memref<?x?x?xf64>
                  %196 = memref.load %107[%arg14, %arg13, %119] : memref<?x?x?xf64>
                  %197 = arith.subf %195, %196 : f64
                  %198 = arith.mulf %197, %cst_0 : f64
                  %199 = memref.load %116[%arg14, %106, %88] : memref<?x?x?xf64>
                  %200 = memref.load %116[%arg14, %110, %88] : memref<?x?x?xf64>
                  %201 = arith.subf %199, %200 : f64
                  %202 = arith.mulf %201, %cst_0 : f64
                  %203 = arith.addf %198, %202 : f64
                  %204 = arith.mulf %194, %203 : f64
                  %205 = arts.db_ref %arg11[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
                  %206 = memref.load %205[%arg14, %arg13, %88] : memref<?x?x?xf64>
                  %207 = arith.addf %206, %204 : f64
                  memref.store %207, %205[%arg14, %arg13, %88] : memref<?x?x?xf64>
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
      %12 = arts.db_ref %ptr_30[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %13 = memref.load %12[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %14 = arts.db_ref %ptr_34[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %15 = memref.load %14[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %16 = arith.addf %13, %15 : f64
      %17 = arts.db_ref %ptr_38[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %18 = memref.load %17[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %19 = arith.addf %16, %18 : f64
      %20 = arts.db_ref %ptr_42[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %21 = memref.load %20[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %22 = arith.addf %19, %21 : f64
      %23 = arts.db_ref %ptr_46[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
      %24 = memref.load %23[%arg0, %arg0, %arg0] : memref<?x?x?xf64>
      %25 = arith.addf %22, %24 : f64
      %26 = arts.db_ref %ptr_50[%c0] : memref<?xmemref<?x?x?xf64>> -> memref<?x?x?xf64>
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
    arts.db_free(%guid_25) : memref<?xi64>
    arts.db_free(%ptr_26) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_29) : memref<?xi64>
    arts.db_free(%ptr_30) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_33) : memref<?xi64>
    arts.db_free(%ptr_34) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_37) : memref<?xi64>
    arts.db_free(%ptr_38) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_49) : memref<?xi64>
    arts.db_free(%ptr_50) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_45) : memref<?xi64>
    arts.db_free(%ptr_46) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?x?xf64>>
    arts.db_free(%guid_41) : memref<?xi64>
    arts.db_free(%ptr_42) : memref<?xmemref<?x?x?xf64>>
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
