module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_vel4sg_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5 = arith.constant 5 : index
    %c8 = arith.constant 8 : index
    %c6 = arith.constant 6 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant 5.000000e-04 : f32
    %cst_0 = arith.constant 5.000000e-01 : f32
    %cst_1 = arith.constant 1.000000e+00 : f32
    %c-1_i32 = arith.constant -1 : i32
    %c46 = arith.constant 46 : index
    %cst_2 = arith.constant 0.000000e+00 : f32
    %cst_3 = arith.constant 2.500000e+03 : f32
    %c13_i32 = arith.constant 13 : i32
    %cst_4 = arith.constant 0.00999999977 : f32
    %c7_i32 = arith.constant 7 : i32
    %c19_i32 = arith.constant 19 : i32
    %c11_i32 = arith.constant 11 : i32
    %c23_i32 = arith.constant 23 : i32
    %c17_i32 = arith.constant 17 : i32
    %cst_5 = arith.constant 5.000000e-03 : f32
    %c5_i32 = arith.constant 5 : i32
    %c29_i32 = arith.constant 29 : i32
    %cst_6 = arith.constant 4.000000e-03 : f32
    %c3_i32 = arith.constant 3 : i32
    %c31_i32 = arith.constant 31 : i32
    %cst_7 = arith.constant 6.000000e-03 : f32
    %c2_i32 = arith.constant 2 : i32
    %c37_i32 = arith.constant 37 : i32
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c48 = arith.constant 48 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_8 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 212 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:149:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 212 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %c1_9 = arith.constant 1 : index
    %c0_10 = arith.constant 0 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 81 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:99:17", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 81 : i64, lastUseId = 82 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 81 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_11 = arith.constant 1 : index
    %c0_12 = arith.constant 0 : index
    %guid_13, %ptr_14 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 83 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:100:17", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 83 : i64, lastUseId = 84 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 83 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_15 = arith.constant 1 : index
    %c0_16 = arith.constant 0 : index
    %guid_17, %ptr_18 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 85 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:101:17", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 85 : i64, lastUseId = 86 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 85 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_19 = arith.constant 1 : index
    %c0_20 = arith.constant 0 : index
    %guid_21, %ptr_22 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 87 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:102:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 87 : i64, lastUseId = 88 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 87 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_23 = arith.constant 1 : index
    %c0_24 = arith.constant 0 : index
    %guid_25, %ptr_26 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 89 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:103:18", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 89 : i64, lastUseId = 90 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 12 : i64>, arts.metadata_origin_id = 89 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_27 = arith.constant 1 : index
    %c0_28 = arith.constant 0 : index
    %guid_29, %ptr_30 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 91 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:104:18", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 91 : i64, lastUseId = 92 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 12 : i64>, arts.metadata_origin_id = 91 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_31 = arith.constant 1 : index
    %c0_32 = arith.constant 0 : index
    %guid_33, %ptr_34 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 93 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:105:18", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 93 : i64, lastUseId = 94 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 12 : i64>, arts.metadata_origin_id = 93 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_35 = arith.constant 1 : index
    %c0_36 = arith.constant 0 : index
    %guid_37, %ptr_38 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 95 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:106:18", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 95 : i64, lastUseId = 96 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 95 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_39 = arith.constant 1 : index
    %c0_40 = arith.constant 0 : index
    %guid_41, %ptr_42 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 97 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:107:18", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 97 : i64, lastUseId = 98 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 97 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %c1_43 = arith.constant 1 : index
    %c0_44 = arith.constant 0 : index
    %guid_45, %ptr_46 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c48, %c48, %c48] {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:108:18", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 442368 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 99 : i64, lastUseId = 100 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 99 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_47 = memref.alloca() {arts.id = 102 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:135:3", totalAccesses = 11 : i64, readCount = 8 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 102 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_47[] : memref<i32>
    memref.store %c0_i32, %alloca_47[] : memref<i32>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          %12 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %cst_2, %12[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %13 = arts.db_ref %ptr_14[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %cst_2, %13[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %14 = arts.db_ref %ptr_18[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %cst_2, %14[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %15 = memref.load %alloca_47[] : memref<i32>
          %16 = arith.remsi %15, %c13_i32 : i32
          %17 = arith.sitofp %16 : i32 to f32
          %18 = arith.addf %17, %cst_3 : f32
          %19 = arts.db_ref %ptr_22[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %18, %19[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %20 = memref.load %alloca_47[] : memref<i32>
          %21 = arith.muli %20, %c7_i32 : i32
          %22 = arith.remsi %21, %c19_i32 : i32
          %23 = arith.sitofp %22 : i32 to f32
          %24 = arith.mulf %23, %cst_4 : f32
          %25 = arts.db_ref %ptr_26[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %24, %25[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %26 = memref.load %alloca_47[] : memref<i32>
          %27 = arith.muli %26, %c11_i32 : i32
          %28 = arith.remsi %27, %c23_i32 : i32
          %29 = arith.sitofp %28 : i32 to f32
          %30 = arith.mulf %29, %cst_4 : f32
          %31 = arts.db_ref %ptr_30[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %30, %31[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %32 = memref.load %alloca_47[] : memref<i32>
          %33 = arith.muli %32, %c13_i32 : i32
          %34 = arith.remsi %33, %c17_i32 : i32
          %35 = arith.sitofp %34 : i32 to f32
          %36 = arith.mulf %35, %cst_4 : f32
          %37 = arts.db_ref %ptr_34[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %36, %37[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %38 = memref.load %alloca_47[] : memref<i32>
          %39 = arith.muli %38, %c5_i32 : i32
          %40 = arith.remsi %39, %c29_i32 : i32
          %41 = arith.sitofp %40 : i32 to f32
          %42 = arith.mulf %41, %cst_5 : f32
          %43 = arts.db_ref %ptr_38[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %42, %43[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %44 = memref.load %alloca_47[] : memref<i32>
          %45 = arith.muli %44, %c3_i32 : i32
          %46 = arith.remsi %45, %c31_i32 : i32
          %47 = arith.sitofp %46 : i32 to f32
          %48 = arith.mulf %47, %cst_6 : f32
          %49 = arts.db_ref %ptr_42[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %48, %49[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %50 = memref.load %alloca_47[] : memref<i32>
          %51 = arith.muli %50, %c2_i32 : i32
          %52 = arith.remsi %51, %c37_i32 : i32
          %53 = arith.sitofp %52 : i32 to f32
          %54 = arith.mulf %53, %cst_7 : f32
          %55 = arts.db_ref %ptr_46[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
          memref.store %54, %55[%arg0, %arg1, %arg2] : memref<?x?x?xf32>
          %56 = memref.load %alloca_47[] : memref<i32>
          %57 = arith.addi %56, %c1_i32 : i32
          memref.store %57, %alloca_47[] : memref<i32>
        } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:135:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 155 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:135:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:135:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_48 = memref.alloca() {arts.id = 165 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 165 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_49 = memref.alloca() {arts.id = 161 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 161 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_50 = memref.alloca() {arts.id = 163 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 163 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_51 = memref.alloca() {arts.id = 158 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 158 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %8 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg0 = %c0 to %c8 step %c1 {
        %12 = arith.muli %arg0, %c6 : index
        %13 = arith.cmpi uge, %12, %c46 : index
        %14 = arith.subi %c46, %12 : index
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
          %guid_52, %ptr_53 = arts.db_acquire[<in>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_53 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %44 = arith.cmpi slt, %14, %c0 : index
          %45 = arith.select %44, %c0, %14 : index
          %46 = arith.minui %45, %16 : index
          %47 = arith.cmpi slt, %14, %c0 : index
          %48 = arith.select %47, %c0, %14 : index
          %49 = arith.minui %48, %16 : index
          %guid_54, %ptr_55 = arts.db_acquire[<in>] (%guid_25 : memref<?xi64>, %ptr_26 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_55 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %50 = arith.cmpi slt, %14, %c0 : index
          %51 = arith.select %50, %c0, %14 : index
          %52 = arith.minui %51, %16 : index
          %53 = arith.cmpi slt, %14, %c0 : index
          %54 = arith.select %53, %c0, %14 : index
          %55 = arith.minui %54, %16 : index
          %guid_56, %ptr_57 = arts.db_acquire[<in>] (%guid_37 : memref<?xi64>, %ptr_38 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_57 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %56 = arith.cmpi slt, %14, %c0 : index
          %57 = arith.select %56, %c0, %14 : index
          %58 = arith.minui %57, %16 : index
          %c1_58 = arith.constant 1 : index
          %59 = arith.cmpi uge, %12, %c1_58 : index
          %60 = arith.subi %12, %c1_58 : index
          %c0_59 = arith.constant 0 : index
          %61 = arith.select %59, %60, %c0_59 : index
          %c2_60 = arith.constant 2 : index
          %62 = arith.addi %58, %c2_60 : index
          %guid_61, %ptr_62 = arts.db_acquire[<in>] (%guid_41 : memref<?xi64>, %ptr_42 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_62 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %63 = arith.cmpi slt, %14, %c0 : index
          %64 = arith.select %63, %c0, %14 : index
          %65 = arith.minui %64, %16 : index
          %66 = arith.cmpi slt, %14, %c0 : index
          %67 = arith.select %66, %c0, %14 : index
          %68 = arith.minui %67, %16 : index
          %guid_63, %ptr_64 = arts.db_acquire[<in>] (%guid_29 : memref<?xi64>, %ptr_30 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_64 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %69 = arith.cmpi slt, %14, %c0 : index
          %70 = arith.select %69, %c0, %14 : index
          %71 = arith.minui %70, %16 : index
          %c1_65 = arith.constant 1 : index
          %72 = arith.cmpi uge, %12, %c1_65 : index
          %73 = arith.subi %12, %c1_65 : index
          %c0_66 = arith.constant 0 : index
          %74 = arith.select %72, %73, %c0_66 : index
          %c2_67 = arith.constant 2 : index
          %75 = arith.addi %71, %c2_67 : index
          %guid_68, %ptr_69 = arts.db_acquire[<in>] (%guid_45 : memref<?xi64>, %ptr_46 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_69 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %76 = arith.cmpi slt, %14, %c0 : index
          %77 = arith.select %76, %c0, %14 : index
          %78 = arith.minui %77, %16 : index
          %79 = arith.index_cast %c-1_i32 : i32 to index
          %c43 = arith.constant 43 : index
          %80 = arith.addi %78, %c43 : index
          %guid_70, %ptr_71 = arts.db_acquire[<in>] (%guid_33 : memref<?xi64>, %ptr_34 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%21], sizes[%26]), offsets[%36], sizes[%37] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_71 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %81 = arith.divui %12, %c48 : index
          %82 = arith.addi %12, %c5 : index
          %83 = arith.divui %82, %c48 : index
          %84 = arith.cmpi ugt, %81, %c0 : index
          %85 = arith.select %84, %83, %c0 : index
          %86 = arith.subi %85, %81 : index
          %87 = arith.addi %86, %c1 : index
          %88 = arith.select %84, %c0, %81 : index
          %89 = arith.select %84, %c0, %87 : index
          %guid_72, %ptr_73 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%12], sizes[%c6]), offsets[%88], sizes[%89] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_73 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_74, %ptr_75 = arts.db_acquire[<inout>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%12], sizes[%c6]), offsets[%88], sizes[%89] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_75 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          %guid_76, %ptr_77 = arts.db_acquire[<inout>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?xmemref<?x?x?xf32>>) partitioning(<block>, offsets[%12], sizes[%c6]), offsets[%88], sizes[%89] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [48, 48, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
          arts.lowering_contract(%ptr_77 : memref<?xmemref<?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c48] contract(<ownerDims = [2], postDbRefined = true>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_53, %ptr_55, %ptr_57, %ptr_62, %ptr_64, %ptr_69, %ptr_71, %ptr_73, %ptr_75, %ptr_77) : memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>>, memref<?xmemref<?x?x?xf32>> attributes {arts.id = 235 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [48, 48, 6]} {
          ^bb0(%arg1: memref<?xmemref<?x?x?xf32>>, %arg2: memref<?xmemref<?x?x?xf32>>, %arg3: memref<?xmemref<?x?x?xf32>>, %arg4: memref<?xmemref<?x?x?xf32>>, %arg5: memref<?xmemref<?x?x?xf32>>, %arg6: memref<?xmemref<?x?x?xf32>>, %arg7: memref<?xmemref<?x?x?xf32>>, %arg8: memref<?xmemref<?x?x?xf32>>, %arg9: memref<?xmemref<?x?x?xf32>>, %arg10: memref<?xmemref<?x?x?xf32>>):
            %90 = arith.subi %c2, %12 : index
            %91 = arith.cmpi slt, %90, %c0 : index
            %92 = arith.select %91, %c0, %90 : index
            %93 = arith.cmpi slt, %14, %c0 : index
            %94 = arith.select %93, %c0, %14 : index
            %95 = arith.minui %94, %16 : index
            scf.for %arg11 = %92 to %95 step %c1 {
              %96 = arith.addi %12, %arg11 : index
              %97 = "polygeist.undef"() : () -> f32
              memref.store %97, %alloca_51[] : memref<f32>
              memref.store %97, %alloca_49[] : memref<f32>
              memref.store %97, %alloca_50[] : memref<f32>
              memref.store %97, %alloca_48[] : memref<f32>
              %98 = arith.index_cast %96 : index to i32
              scf.for %arg12 = %c2 to %c46 step %c1 {
                %99 = arith.index_cast %arg12 : index to i32
                scf.for %arg13 = %c2 to %c46 step %c1 {
                  %100 = arith.index_cast %arg13 : index to i32
                  %101 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %102 = memref.load %101[%arg13, %arg12, %96] : memref<?x?x?xf32>
                  %103 = arith.divf %cst_1, %102 : f32
                  memref.store %103, %alloca_48[] : memref<f32>
                  %104 = arith.addi %100, %c1_i32 : i32
                  %105 = arith.index_cast %104 : i32 to index
                  %106 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %107 = memref.load %106[%105, %arg12, %96] : memref<?x?x?xf32>
                  %108 = arith.addi %100, %c-1_i32 : i32
                  %109 = arith.index_cast %108 : i32 to index
                  %110 = memref.load %106[%109, %arg12, %96] : memref<?x?x?xf32>
                  %111 = arith.subf %107, %110 : f32
                  %112 = arith.mulf %111, %cst_0 : f32
                  %113 = arith.addi %99, %c1_i32 : i32
                  %114 = arith.index_cast %113 : i32 to index
                  %115 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %116 = memref.load %115[%arg13, %114, %96] : memref<?x?x?xf32>
                  %117 = arith.addi %99, %c-1_i32 : i32
                  %118 = arith.index_cast %117 : i32 to index
                  %119 = memref.load %115[%arg13, %118, %96] : memref<?x?x?xf32>
                  %120 = arith.subf %116, %119 : f32
                  %121 = arith.mulf %120, %cst_0 : f32
                  %122 = arith.addf %112, %121 : f32
                  %123 = arith.addi %98, %c1_i32 : i32
                  %124 = arith.index_cast %123 : i32 to index
                  %125 = arts.db_ref %arg4[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %126 = memref.load %125[%arg13, %arg12, %124] : memref<?x?x?xf32>
                  %127 = arith.addi %98, %c-1_i32 : i32
                  %128 = arith.index_cast %127 : i32 to index
                  %129 = memref.load %125[%arg13, %arg12, %128] : memref<?x?x?xf32>
                  %130 = arith.subf %126, %129 : f32
                  %131 = arith.mulf %130, %cst_0 : f32
                  %132 = arith.addf %122, %131 : f32
                  memref.store %132, %alloca_50[] : memref<f32>
                  %133 = memref.load %115[%105, %arg12, %96] : memref<?x?x?xf32>
                  %134 = memref.load %115[%109, %arg12, %96] : memref<?x?x?xf32>
                  %135 = arith.subf %133, %134 : f32
                  %136 = arith.mulf %135, %cst_0 : f32
                  %137 = arts.db_ref %arg5[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %138 = memref.load %137[%arg13, %114, %96] : memref<?x?x?xf32>
                  %139 = memref.load %137[%arg13, %118, %96] : memref<?x?x?xf32>
                  %140 = arith.subf %138, %139 : f32
                  %141 = arith.mulf %140, %cst_0 : f32
                  %142 = arith.addf %136, %141 : f32
                  %143 = arts.db_ref %arg6[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %144 = memref.load %143[%arg13, %arg12, %124] : memref<?x?x?xf32>
                  %145 = memref.load %143[%arg13, %arg12, %128] : memref<?x?x?xf32>
                  %146 = arith.subf %144, %145 : f32
                  %147 = arith.mulf %146, %cst_0 : f32
                  %148 = arith.addf %142, %147 : f32
                  memref.store %148, %alloca_49[] : memref<f32>
                  %149 = memref.load %125[%105, %arg12, %96] : memref<?x?x?xf32>
                  %150 = memref.load %125[%109, %arg12, %96] : memref<?x?x?xf32>
                  %151 = arith.subf %149, %150 : f32
                  %152 = arith.mulf %151, %cst_0 : f32
                  %153 = memref.load %143[%arg13, %114, %96] : memref<?x?x?xf32>
                  %154 = memref.load %143[%arg13, %118, %96] : memref<?x?x?xf32>
                  %155 = arith.subf %153, %154 : f32
                  %156 = arith.mulf %155, %cst_0 : f32
                  %157 = arith.addf %152, %156 : f32
                  %158 = arts.db_ref %arg7[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %159 = memref.load %158[%arg13, %arg12, %124] : memref<?x?x?xf32>
                  %160 = memref.load %158[%arg13, %arg12, %128] : memref<?x?x?xf32>
                  %161 = arith.subf %159, %160 : f32
                  %162 = arith.mulf %161, %cst_0 : f32
                  %163 = arith.addf %157, %162 : f32
                  memref.store %163, %alloca_51[] : memref<f32>
                  %164 = memref.load %alloca_48[] : memref<f32>
                  %165 = arith.mulf %164, %cst : f32
                  %166 = memref.load %alloca_50[] : memref<f32>
                  %167 = arith.mulf %165, %166 : f32
                  %168 = arts.db_ref %arg8[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %169 = memref.load %168[%arg13, %arg12, %96] : memref<?x?x?xf32>
                  %170 = arith.addf %169, %167 : f32
                  memref.store %170, %168[%arg13, %arg12, %96] : memref<?x?x?xf32>
                  %171 = memref.load %alloca_48[] : memref<f32>
                  %172 = arith.mulf %171, %cst : f32
                  %173 = memref.load %alloca_49[] : memref<f32>
                  %174 = arith.mulf %172, %173 : f32
                  %175 = arts.db_ref %arg9[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %176 = memref.load %175[%arg13, %arg12, %96] : memref<?x?x?xf32>
                  %177 = arith.addf %176, %174 : f32
                  memref.store %177, %175[%arg13, %arg12, %96] : memref<?x?x?xf32>
                  %178 = memref.load %alloca_48[] : memref<f32>
                  %179 = arith.mulf %178, %cst : f32
                  %180 = memref.load %alloca_51[] : memref<f32>
                  %181 = arith.mulf %179, %180 : f32
                  %182 = arts.db_ref %arg10[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
                  %183 = memref.load %182[%arg13, %arg12, %96] : memref<?x?x?xf32>
                  %184 = arith.addf %183, %181 : f32
                  memref.store %184, %182[%arg13, %arg12, %96] : memref<?x?x?xf32>
                } {arts.id = 204 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 44 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:141:5">, arts.metadata_provenance = "exact"}
              } {arts.id = 206 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 44 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:141:5">, arts.metadata_provenance = "exact"}
            } {arts.id = 234 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "vel4sg_base.c:141:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg1) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg2) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg3) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg6) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg7) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg8) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg9) : memref<?xmemref<?x?x?xf32>>
            arts.db_release(%arg10) : memref<?xmemref<?x?x?xf32>>
          }
        }
      }
    } : i64
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_8, %alloca[] : memref<f64>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      %12 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
      %13 = memref.load %12[%arg0, %arg0, %arg0] : memref<?x?x?xf32>
      %14 = arith.extf %13 : f32 to f64
      %15 = math.absf %14 : f64
      %16 = arts.db_ref %ptr_14[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
      %17 = memref.load %16[%arg0, %arg0, %arg0] : memref<?x?x?xf32>
      %18 = arith.extf %17 : f32 to f64
      %19 = math.absf %18 : f64
      %20 = arith.addf %15, %19 : f64
      %21 = arts.db_ref %ptr_18[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
      %22 = memref.load %21[%arg0, %arg0, %arg0] : memref<?x?x?xf32>
      %23 = arith.extf %22 : f32 to f64
      %24 = math.absf %23 : f64
      %25 = arith.addf %20, %24 : f64
      %26 = memref.load %alloca[] : memref<f64>
      %27 = arith.addf %26, %25 : f64
      memref.store %27, %alloca[] : memref<f64>
    } {arts.id = 198 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:153:3">, arts.metadata_provenance = "recovered"}
    %10 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%10) : (f64) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%11, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_37) : memref<?xi64>
    arts.db_free(%ptr_38) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_45) : memref<?xi64>
    arts.db_free(%ptr_46) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_25) : memref<?xi64>
    arts.db_free(%ptr_26) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_33) : memref<?xi64>
    arts.db_free(%ptr_34) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_29) : memref<?xi64>
    arts.db_free(%ptr_30) : memref<?xmemref<?x?x?xf32>>
    arts.db_free(%guid_41) : memref<?xi64>
    arts.db_free(%ptr_42) : memref<?xmemref<?x?x?xf32>>
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
