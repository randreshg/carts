module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c524287 = arith.constant 524287 : index
    %c2 = arith.constant 2 : index
    %cst = arith.constant -1.702000e+00 : f32
    %cst_0 = arith.constant 1.000000e+00 : f32
    %cst_1 = arith.constant 5.000000e-01 : f32
    %cst_2 = arith.constant 0.797884583 : f32
    %cst_3 = arith.constant 4.471500e-02 : f32
    %cst_4 = arith.constant 1.000000e-01 : f32
    %cst_5 = arith.constant 0x49800000 : f32
    %c524288 = arith.constant 524288 : index
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
    %c1_18 = arith.constant 1 : index
    %c1_19 = arith.constant 1 : index
    %c1_20 = arith.constant 1 : index
    %c1_21 = arith.constant 1 : index
    %7 = arith.minui %c524288, %c524288 : index
    %8 = arith.minui %7, %c524288 : index
    %9 = arith.minui %8, %c524288 : index
    %c1_22 = arith.constant 1 : index
    %10 = arith.maxui %9, %c1_22 : index
    %c1_i64 = arith.constant 1 : i64
    %11 = arith.index_cast %c1048576 : index to i64
    %12 = arith.index_cast %10 : index to i64
    %13 = arith.addi %11, %12 : i64
    %14 = arith.subi %13, %c1_i64 : i64
    %15 = arith.divui %14, %12 : i64
    %16 = arith.index_cast %15 : i64 to index
    %c0_23 = arith.constant 0 : index
    %c1_24 = arith.constant 1 : index
    %c0_25 = arith.constant 0 : index
    %c1_26 = arith.constant 1 : index
    %c0_27 = arith.constant 0 : index
    %c1_28 = arith.constant 1 : index
    %c0_29 = arith.constant 0 : index
    %c1_30 = arith.constant 1 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%16] elementType(f32) elementSizes[%10] {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:171:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 41 : i64, lastUseId = 294 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 10 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 41 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?xf32>>) block_shape[%10] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_31 = arith.constant 0 : index
    %c1_32 = arith.constant 1 : index
    %c1_33 = arith.constant 1 : index
    %c1_34 = arith.constant 1 : index
    %c1_35 = arith.constant 1 : index
    %17 = arith.minui %c524288, %c524288 : index
    %18 = arith.minui %17, %c524288 : index
    %19 = arith.minui %18, %c524288 : index
    %c1_36 = arith.constant 1 : index
    %20 = arith.maxui %19, %c1_36 : index
    %c1_i64_37 = arith.constant 1 : i64
    %21 = arith.index_cast %c1048576 : index to i64
    %22 = arith.index_cast %20 : index to i64
    %23 = arith.addi %21, %22 : i64
    %24 = arith.subi %23, %c1_i64_37 : i64
    %25 = arith.divui %24, %22 : i64
    %26 = arith.index_cast %25 : i64 to index
    %c0_38 = arith.constant 0 : index
    %c1_39 = arith.constant 1 : index
    %c0_40 = arith.constant 0 : index
    %c1_41 = arith.constant 1 : index
    %c0_42 = arith.constant 0 : index
    %c1_43 = arith.constant 1 : index
    %c0_44 = arith.constant 0 : index
    %c1_45 = arith.constant 1 : index
    %guid_46, %ptr_47 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%26] elementType(f32) elementSizes[%20] {arts.id = 42 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:172:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 42 : i64, lastUseId = 295 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 42 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_47 : memref<?xmemref<?xf32>>) block_shape[%20] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_48 = arith.constant 0 : index
    %c1_49 = arith.constant 1 : index
    %c1_50 = arith.constant 1 : index
    %c1_51 = arith.constant 1 : index
    %c1_52 = arith.constant 1 : index
    %27 = arith.minui %c524288, %c524288 : index
    %28 = arith.minui %27, %c524288 : index
    %29 = arith.minui %28, %c524288 : index
    %c1_53 = arith.constant 1 : index
    %30 = arith.maxui %29, %c1_53 : index
    %c1_i64_54 = arith.constant 1 : i64
    %31 = arith.index_cast %c1048576 : index to i64
    %32 = arith.index_cast %30 : index to i64
    %33 = arith.addi %31, %32 : i64
    %34 = arith.subi %33, %c1_i64_54 : i64
    %35 = arith.divui %34, %32 : i64
    %36 = arith.index_cast %35 : i64 to index
    %c0_55 = arith.constant 0 : index
    %c1_56 = arith.constant 1 : index
    %c0_57 = arith.constant 0 : index
    %c1_58 = arith.constant 1 : index
    %c0_59 = arith.constant 0 : index
    %c1_60 = arith.constant 1 : index
    %c0_61 = arith.constant 0 : index
    %c1_62 = arith.constant 1 : index
    %guid_63, %ptr_64 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%36] elementType(f32) elementSizes[%30] {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:173:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 43 : i64, lastUseId = 296 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 12 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 43 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_64 : memref<?xmemref<?xf32>>) block_shape[%30] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_65 = arith.constant 0 : index
    %c1_66 = arith.constant 1 : index
    %c1_67 = arith.constant 1 : index
    %c1_68 = arith.constant 1 : index
    %c1_69 = arith.constant 1 : index
    %37 = arith.minui %c524288, %c524288 : index
    %38 = arith.minui %37, %c524288 : index
    %39 = arith.minui %38, %c524288 : index
    %c1_70 = arith.constant 1 : index
    %40 = arith.maxui %39, %c1_70 : index
    %c1_i64_71 = arith.constant 1 : i64
    %41 = arith.index_cast %c1048576 : index to i64
    %42 = arith.index_cast %40 : index to i64
    %43 = arith.addi %41, %42 : i64
    %44 = arith.subi %43, %c1_i64_71 : i64
    %45 = arith.divui %44, %42 : i64
    %46 = arith.index_cast %45 : i64 to index
    %c0_72 = arith.constant 0 : index
    %c1_73 = arith.constant 1 : index
    %c0_74 = arith.constant 0 : index
    %c1_75 = arith.constant 1 : index
    %c0_76 = arith.constant 0 : index
    %c1_77 = arith.constant 1 : index
    %c0_78 = arith.constant 0 : index
    %c1_79 = arith.constant 1 : index
    %guid_80, %ptr_81 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%46] elementType(f32) elementSizes[%40] {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:174:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 44 : i64, lastUseId = 297 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 13 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 44 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_81 : memref<?xmemref<?xf32>>) block_shape[%40] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_82 = arith.constant 0 : index
    %c1_83 = arith.constant 1 : index
    %c1_84 = arith.constant 1 : index
    %c1_85 = arith.constant 1 : index
    %c1_86 = arith.constant 1 : index
    %47 = arith.minui %c524288, %c524288 : index
    %48 = arith.minui %47, %c524288 : index
    %49 = arith.minui %48, %c524288 : index
    %c1_87 = arith.constant 1 : index
    %50 = arith.maxui %49, %c1_87 : index
    %c1_i64_88 = arith.constant 1 : i64
    %51 = arith.index_cast %c1048576 : index to i64
    %52 = arith.index_cast %50 : index to i64
    %53 = arith.addi %51, %52 : i64
    %54 = arith.subi %53, %c1_i64_88 : i64
    %55 = arith.divui %54, %52 : i64
    %56 = arith.index_cast %55 : i64 to index
    %c0_89 = arith.constant 0 : index
    %c1_90 = arith.constant 1 : index
    %c0_91 = arith.constant 0 : index
    %c1_92 = arith.constant 1 : index
    %c0_93 = arith.constant 0 : index
    %c1_94 = arith.constant 1 : index
    %c0_95 = arith.constant 0 : index
    %c1_96 = arith.constant 1 : index
    %guid_97, %ptr_98 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%56] elementType(f32) elementSizes[%50] {arts.id = 45 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:175:26", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 45 : i64, lastUseId = 298 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 14 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 45 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_98 : memref<?xmemref<?xf32>>) block_shape[%50] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_99 = arith.constant 0 : index
    %c1_100 = arith.constant 1 : index
    %c1_101 = arith.constant 1 : index
    %c1_102 = arith.constant 1 : index
    %c1_103 = arith.constant 1 : index
    %57 = arith.minui %c524288, %c524288 : index
    %58 = arith.minui %57, %c524288 : index
    %59 = arith.minui %58, %c524288 : index
    %c1_104 = arith.constant 1 : index
    %60 = arith.maxui %59, %c1_104 : index
    %c1_i64_105 = arith.constant 1 : i64
    %61 = arith.index_cast %c1048576 : index to i64
    %62 = arith.index_cast %60 : index to i64
    %63 = arith.addi %61, %62 : i64
    %64 = arith.subi %63, %c1_i64_105 : i64
    %65 = arith.divui %64, %62 : i64
    %66 = arith.index_cast %65 : i64 to index
    %c0_106 = arith.constant 0 : index
    %c1_107 = arith.constant 1 : index
    %c0_108 = arith.constant 0 : index
    %c1_109 = arith.constant 1 : index
    %c0_110 = arith.constant 0 : index
    %c1_111 = arith.constant 1 : index
    %c0_112 = arith.constant 0 : index
    %c1_113 = arith.constant 1 : index
    %guid_114, %ptr_115 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%66] elementType(f32) elementSizes[%60] {arts.id = 46 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:176:24", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 46 : i64, lastUseId = 299 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 46 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_115 : memref<?xmemref<?xf32>>) block_shape[%60] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_116 = arith.constant 0 : index
    %c1_117 = arith.constant 1 : index
    %c1_118 = arith.constant 1 : index
    %c1_119 = arith.constant 1 : index
    %c1_120 = arith.constant 1 : index
    %67 = arith.minui %c524288, %c524288 : index
    %68 = arith.minui %67, %c524288 : index
    %69 = arith.minui %68, %c524288 : index
    %c1_121 = arith.constant 1 : index
    %70 = arith.maxui %69, %c1_121 : index
    %c1_i64_122 = arith.constant 1 : i64
    %71 = arith.index_cast %c1048576 : index to i64
    %72 = arith.index_cast %70 : index to i64
    %73 = arith.addi %71, %72 : i64
    %74 = arith.subi %73, %c1_i64_122 : i64
    %75 = arith.divui %74, %72 : i64
    %76 = arith.index_cast %75 : i64 to index
    %c0_123 = arith.constant 0 : index
    %c1_124 = arith.constant 1 : index
    %c0_125 = arith.constant 0 : index
    %c1_126 = arith.constant 1 : index
    %c0_127 = arith.constant 0 : index
    %c1_128 = arith.constant 1 : index
    %c0_129 = arith.constant 0 : index
    %c1_130 = arith.constant 1 : index
    %guid_131, %ptr_132 = arts.db_alloc[<inout>, <heap>, <write>, <block>, <indexed>] route(%c-1_i32 : i32) sizes[%76] elementType(f32) elementSizes[%70] {arts.id = 47 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:177:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 47 : i64, lastUseId = 300 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 16 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 47 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    arts.lowering_contract(%ptr_132 : memref<?xmemref<?xf32>>) block_shape[%70] contract(<ownerDims = [0], postDbRefined = true>)
    %c0_133 = arith.constant 0 : index
    %alloc = memref.alloc() {arts.id = 48 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:179:27", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 400 : i64, firstUseId = 48 : i64, lastUseId = 301 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<100xf32>
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_134 = memref.alloca() {arts.id = 168 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 168 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_135 = memref.alloca() {arts.id = 169 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 169 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %77 = arts.epoch attributes {arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %98 = arith.muli %arg2, %c524288 : index
        %99 = arith.cmpi uge, %98, %c1048576 : index
        %100 = arith.subi %c1048576, %98 : index
        %101 = arith.select %99, %c0, %100 : index
        %102 = arith.minui %101, %c524288 : index
        %103 = arith.cmpi ugt, %102, %c0 : index
        scf.if %103 {
          %104 = arith.divui %98, %c1048576 : index
          %105 = arith.addi %98, %c524287 : index
          %106 = arith.divui %105, %c1048576 : index
          %107 = arith.cmpi ugt, %104, %c0 : index
          %108 = arith.select %107, %106, %c0 : index
          %109 = arith.subi %108, %104 : index
          %110 = arith.addi %109, %c1 : index
          %111 = arith.select %107, %c0, %104 : index
          %112 = arith.select %107, %c0, %110 : index
          %c1_136 = arith.constant 1 : index
          %c0_137 = arith.constant 0 : index
          %113 = arith.maxui %10, %c1_136 : index
          %114 = arith.muli %16, %113 : index
          %115 = arith.divui %98, %113 : index
          %116 = arith.addi %98, %c524288 : index
          %117 = arith.subi %116, %c1_136 : index
          %118 = arith.divui %117, %113 : index
          %119 = arith.subi %16, %c1_136 : index
          %120 = arith.cmpi ugt, %115, %119 : index
          %121 = arith.minui %118, %119 : index
          %122 = arith.select %120, %118, %121 : index
          %123 = arith.subi %122, %115 : index
          %124 = arith.addi %123, %c1_136 : index
          %125 = arith.select %120, %c0_137, %115 : index
          %126 = arith.select %120, %c0_137, %124 : index
          %guid_138, %ptr_139 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%125], sizes[%c1_136] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_139 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          %c1_140 = arith.constant 1 : index
          %c0_141 = arith.constant 0 : index
          %127 = arith.maxui %20, %c1_140 : index
          %128 = arith.muli %26, %127 : index
          %129 = arith.divui %98, %127 : index
          %130 = arith.addi %98, %c524288 : index
          %131 = arith.subi %130, %c1_140 : index
          %132 = arith.divui %131, %127 : index
          %133 = arith.subi %26, %c1_140 : index
          %134 = arith.cmpi ugt, %129, %133 : index
          %135 = arith.minui %132, %133 : index
          %136 = arith.select %134, %132, %135 : index
          %137 = arith.subi %136, %129 : index
          %138 = arith.addi %137, %c1_140 : index
          %139 = arith.select %134, %c0_141, %129 : index
          %140 = arith.select %134, %c0_141, %138 : index
          %guid_142, %ptr_143 = arts.db_acquire[<out>] (%guid_46 : memref<?xi64>, %ptr_47 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%139], sizes[%c1_140] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_143 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          %c1_144 = arith.constant 1 : index
          %c0_145 = arith.constant 0 : index
          %141 = arith.maxui %30, %c1_144 : index
          %142 = arith.muli %36, %141 : index
          %143 = arith.divui %98, %141 : index
          %144 = arith.addi %98, %c524288 : index
          %145 = arith.subi %144, %c1_144 : index
          %146 = arith.divui %145, %141 : index
          %147 = arith.subi %36, %c1_144 : index
          %148 = arith.cmpi ugt, %143, %147 : index
          %149 = arith.minui %146, %147 : index
          %150 = arith.select %148, %146, %149 : index
          %151 = arith.subi %150, %143 : index
          %152 = arith.addi %151, %c1_144 : index
          %153 = arith.select %148, %c0_145, %143 : index
          %154 = arith.select %148, %c0_145, %152 : index
          %guid_146, %ptr_147 = arts.db_acquire[<out>] (%guid_63 : memref<?xi64>, %ptr_64 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%153], sizes[%c1_144] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_147 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          %c1_148 = arith.constant 1 : index
          %c0_149 = arith.constant 0 : index
          %155 = arith.maxui %40, %c1_148 : index
          %156 = arith.muli %46, %155 : index
          %157 = arith.divui %98, %155 : index
          %158 = arith.addi %98, %c524288 : index
          %159 = arith.subi %158, %c1_148 : index
          %160 = arith.divui %159, %155 : index
          %161 = arith.subi %46, %c1_148 : index
          %162 = arith.cmpi ugt, %157, %161 : index
          %163 = arith.minui %160, %161 : index
          %164 = arith.select %162, %160, %163 : index
          %165 = arith.subi %164, %157 : index
          %166 = arith.addi %165, %c1_148 : index
          %167 = arith.select %162, %c0_149, %157 : index
          %168 = arith.select %162, %c0_149, %166 : index
          %guid_150, %ptr_151 = arts.db_acquire[<out>] (%guid_80 : memref<?xi64>, %ptr_81 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%167], sizes[%c1_148] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_151 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_152 = arith.constant 1 : index
          %c0_153 = arith.constant 0 : index
          %169 = arith.maxui %50, %c1_152 : index
          %170 = arith.muli %56, %169 : index
          %171 = arith.divui %98, %169 : index
          %172 = arith.addi %98, %c524288 : index
          %173 = arith.subi %172, %c1_152 : index
          %174 = arith.divui %173, %169 : index
          %175 = arith.subi %56, %c1_152 : index
          %176 = arith.cmpi ugt, %171, %175 : index
          %177 = arith.minui %174, %175 : index
          %178 = arith.select %176, %174, %177 : index
          %179 = arith.subi %178, %171 : index
          %180 = arith.addi %179, %c1_152 : index
          %181 = arith.select %176, %c0_153, %171 : index
          %182 = arith.select %176, %c0_153, %180 : index
          %guid_154, %ptr_155 = arts.db_acquire[<out>] (%guid_97 : memref<?xi64>, %ptr_98 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%181], sizes[%c1_152] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_155 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_156 = arith.constant 1 : index
          %c0_157 = arith.constant 0 : index
          %183 = arith.maxui %60, %c1_156 : index
          %184 = arith.muli %66, %183 : index
          %185 = arith.divui %98, %183 : index
          %186 = arith.addi %98, %c524288 : index
          %187 = arith.subi %186, %c1_156 : index
          %188 = arith.divui %187, %183 : index
          %189 = arith.subi %66, %c1_156 : index
          %190 = arith.cmpi ugt, %185, %189 : index
          %191 = arith.minui %188, %189 : index
          %192 = arith.select %190, %188, %191 : index
          %193 = arith.subi %192, %185 : index
          %194 = arith.addi %193, %c1_156 : index
          %195 = arith.select %190, %c0_157, %185 : index
          %196 = arith.select %190, %c0_157, %194 : index
          %guid_158, %ptr_159 = arts.db_acquire[<out>] (%guid_114 : memref<?xi64>, %ptr_115 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%195], sizes[%c1_156] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_159 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_160 = arith.constant 1 : index
          %c0_161 = arith.constant 0 : index
          %197 = arith.maxui %70, %c1_160 : index
          %198 = arith.muli %76, %197 : index
          %199 = arith.divui %98, %197 : index
          %200 = arith.addi %98, %c524288 : index
          %201 = arith.subi %200, %c1_160 : index
          %202 = arith.divui %201, %197 : index
          %203 = arith.subi %76, %c1_160 : index
          %204 = arith.cmpi ugt, %199, %203 : index
          %205 = arith.minui %202, %203 : index
          %206 = arith.select %204, %202, %205 : index
          %207 = arith.subi %206, %199 : index
          %208 = arith.addi %207, %c1_160 : index
          %209 = arith.select %204, %c0_161, %199 : index
          %210 = arith.select %204, %c0_161, %208 : index
          %guid_162, %ptr_163 = arts.db_acquire[<out>] (%guid_131 : memref<?xi64>, %ptr_132 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%209], sizes[%c1_160] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_163 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_139, %ptr_143, %ptr_147, %ptr_151, %ptr_155, %ptr_159, %ptr_163) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 274 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>, %arg4: memref<?xmemref<?xf32>>, %arg5: memref<?xmemref<?xf32>>, %arg6: memref<?xmemref<?xf32>>, %arg7: memref<?xmemref<?xf32>>, %arg8: memref<?xmemref<?xf32>>, %arg9: memref<?xmemref<?xf32>>):
            %c1_164 = arith.constant 1 : index
            %c0_165 = arith.constant 0 : index
            %211 = arith.maxui %20, %c1_164 : index
            %c0_166 = arith.constant 0 : index
            %c524288_167 = arith.constant 524288 : index
            %212 = arith.muli %arg2, %c524288_167 : index
            %213 = arith.minui %c524288_167, %c524288_167 : index
            %214 = arith.minui %213, %c524288_167 : index
            %215 = arith.minui %214, %c524288_167 : index
            %c1_168 = arith.constant 1 : index
            %216 = arith.maxui %215, %c1_168 : index
            %c1_169 = arith.constant 1 : index
            %217 = arith.maxui %216, %c1_169 : index
            %218 = arith.divui %212, %217 : index
            %219 = arith.select %134, %c0_166, %218 : index
            %c1_170 = arith.constant 1 : index
            %c0_171 = arith.constant 0 : index
            %220 = arith.maxui %10, %c1_170 : index
            %c0_172 = arith.constant 0 : index
            %c524288_173 = arith.constant 524288 : index
            %221 = arith.muli %arg2, %c524288_173 : index
            %222 = arith.minui %c524288_173, %c524288_173 : index
            %223 = arith.minui %222, %c524288_173 : index
            %224 = arith.minui %223, %c524288_173 : index
            %c1_174 = arith.constant 1 : index
            %225 = arith.maxui %224, %c1_174 : index
            %c1_175 = arith.constant 1 : index
            %226 = arith.maxui %225, %c1_175 : index
            %227 = arith.divui %221, %226 : index
            %228 = arith.select %120, %c0_172, %227 : index
            %c1_176 = arith.constant 1 : index
            %c0_177 = arith.constant 0 : index
            %229 = arith.maxui %50, %c1_176 : index
            %c0_178 = arith.constant 0 : index
            %c524288_179 = arith.constant 524288 : index
            %230 = arith.muli %arg2, %c524288_179 : index
            %231 = arith.minui %c524288_179, %c524288_179 : index
            %232 = arith.minui %231, %c524288_179 : index
            %233 = arith.minui %232, %c524288_179 : index
            %c1_180 = arith.constant 1 : index
            %234 = arith.maxui %233, %c1_180 : index
            %c1_181 = arith.constant 1 : index
            %235 = arith.maxui %234, %c1_181 : index
            %236 = arith.divui %230, %235 : index
            %237 = arith.select %176, %c0_178, %236 : index
            %c1_182 = arith.constant 1 : index
            %c0_183 = arith.constant 0 : index
            %238 = arith.maxui %30, %c1_182 : index
            %c0_184 = arith.constant 0 : index
            %c524288_185 = arith.constant 524288 : index
            %239 = arith.muli %arg2, %c524288_185 : index
            %240 = arith.minui %c524288_185, %c524288_185 : index
            %241 = arith.minui %240, %c524288_185 : index
            %242 = arith.minui %241, %c524288_185 : index
            %c1_186 = arith.constant 1 : index
            %243 = arith.maxui %242, %c1_186 : index
            %c1_187 = arith.constant 1 : index
            %244 = arith.maxui %243, %c1_187 : index
            %245 = arith.divui %239, %244 : index
            %246 = arith.select %148, %c0_184, %245 : index
            %c1_188 = arith.constant 1 : index
            %c0_189 = arith.constant 0 : index
            %247 = arith.maxui %60, %c1_188 : index
            %c0_190 = arith.constant 0 : index
            %c524288_191 = arith.constant 524288 : index
            %248 = arith.muli %arg2, %c524288_191 : index
            %249 = arith.minui %c524288_191, %c524288_191 : index
            %250 = arith.minui %249, %c524288_191 : index
            %251 = arith.minui %250, %c524288_191 : index
            %c1_192 = arith.constant 1 : index
            %252 = arith.maxui %251, %c1_192 : index
            %c1_193 = arith.constant 1 : index
            %253 = arith.maxui %252, %c1_193 : index
            %254 = arith.divui %248, %253 : index
            %255 = arith.select %190, %c0_190, %254 : index
            %c1_194 = arith.constant 1 : index
            %c0_195 = arith.constant 0 : index
            %256 = arith.maxui %40, %c1_194 : index
            %c0_196 = arith.constant 0 : index
            %c524288_197 = arith.constant 524288 : index
            %257 = arith.muli %arg2, %c524288_197 : index
            %258 = arith.minui %c524288_197, %c524288_197 : index
            %259 = arith.minui %258, %c524288_197 : index
            %260 = arith.minui %259, %c524288_197 : index
            %c1_198 = arith.constant 1 : index
            %261 = arith.maxui %260, %c1_198 : index
            %c1_199 = arith.constant 1 : index
            %262 = arith.maxui %261, %c1_199 : index
            %263 = arith.divui %257, %262 : index
            %264 = arith.select %162, %c0_196, %263 : index
            %c1_200 = arith.constant 1 : index
            %c0_201 = arith.constant 0 : index
            %265 = arith.maxui %70, %c1_200 : index
            %c0_202 = arith.constant 0 : index
            %c524288_203 = arith.constant 524288 : index
            %266 = arith.muli %arg2, %c524288_203 : index
            %267 = arith.minui %c524288_203, %c524288_203 : index
            %268 = arith.minui %267, %c524288_203 : index
            %269 = arith.minui %268, %c524288_203 : index
            %c1_204 = arith.constant 1 : index
            %270 = arith.maxui %269, %c1_204 : index
            %c1_205 = arith.constant 1 : index
            %271 = arith.maxui %270, %c1_205 : index
            %272 = arith.divui %266, %271 : index
            %273 = arith.select %204, %c0_202, %272 : index
            %274 = arith.subi %c0, %98 : index
            %275 = arith.cmpi slt, %274, %c0 : index
            %276 = arith.select %275, %c0, %274 : index
            %277 = arith.cmpi slt, %100, %c0 : index
            %278 = arith.select %277, %c0, %100 : index
            %279 = arith.minui %278, %102 : index
            scf.for %arg10 = %276 to %279 step %c1 {
              %280 = arith.addi %98, %arg10 : index
              %281 = arith.index_cast %280 : index to i32
              %282 = arith.sitofp %281 : i32 to f32
              %283 = arith.divf %282, %cst_5 : f32
              %284 = arith.mulf %283, %cst_8 : f32
              %285 = arith.addf %284, %cst_9 : f32
              %286 = arith.cmpf ogt, %285, %cst_7 : f32
              %287 = arith.select %286, %285, %cst_7 : f32
              %c1_206 = arith.constant 1 : index
              %288 = arith.maxui %220, %c1_206 : index
              %c0_207 = arith.constant 0 : index
              %c1_208 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %289 = arith.addi %98, %276 : index
              %290 = arith.divui %289, %288 : index
              %291 = arith.subi %arg10, %276 : index
              %292 = arith.addi %276, %291 : index
              %293 = arith.subi %290, %228 : index
              %294 = arts.db_ref %arg3[%c0_171] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %287, %294[%arg10] : memref<?xf32>
              %295 = scf.if %286 -> (f32) {
                scf.yield %285 : f32
              } else {
                %312 = arith.mulf %285, %cst_4 : f32
                scf.yield %312 : f32
              }
              %c1_209 = arith.constant 1 : index
              %296 = arith.maxui %211, %c1_209 : index
              %c0_210 = arith.constant 0 : index
              %c1_211 = arith.constant 1 : index
              %c-1_212 = arith.constant -1 : index
              %297 = arith.addi %98, %276 : index
              %298 = arith.divui %297, %296 : index
              %299 = arith.subi %arg10, %276 : index
              %300 = arith.addi %276, %299 : index
              %301 = arith.subi %298, %219 : index
              %302 = arts.db_ref %arg4[%c0_165] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %295, %302[%arg10] : memref<?xf32>
              %303 = arith.cmpf olt, %287, %cst_8 : f32
              %304 = arith.select %303, %287, %cst_8 : f32
              %c1_213 = arith.constant 1 : index
              %305 = arith.maxui %238, %c1_213 : index
              %c0_214 = arith.constant 0 : index
              %c1_215 = arith.constant 1 : index
              %c-1_216 = arith.constant -1 : index
              %306 = arith.addi %98, %276 : index
              %307 = arith.divui %306, %305 : index
              %308 = arith.subi %arg10, %276 : index
              %309 = arith.addi %276, %308 : index
              %310 = arith.subi %307, %246 : index
              %311 = arts.db_ref %arg5[%c0_183] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %304, %311[%arg10] : memref<?xf32>
            } {arts.id = 270 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:190:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?xf32>>
            arts.db_release(%arg6) : memref<?xmemref<?xf32>>
            arts.db_release(%arg7) : memref<?xmemref<?xf32>>
            arts.db_release(%arg8) : memref<?xmemref<?xf32>>
            arts.db_release(%arg9) : memref<?xmemref<?xf32>>
          }
        }
      }
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %98 = arith.muli %arg2, %c524288 : index
        %99 = arith.cmpi uge, %98, %c1048576 : index
        %100 = arith.subi %c1048576, %98 : index
        %101 = arith.select %99, %c0, %100 : index
        %102 = arith.minui %101, %c524288 : index
        %103 = arith.cmpi ugt, %102, %c0 : index
        scf.if %103 {
          %104 = arith.divui %98, %c1048576 : index
          %105 = arith.addi %98, %c524287 : index
          %106 = arith.divui %105, %c1048576 : index
          %107 = arith.cmpi ugt, %104, %c0 : index
          %108 = arith.select %107, %106, %c0 : index
          %109 = arith.subi %108, %104 : index
          %110 = arith.addi %109, %c1 : index
          %111 = arith.select %107, %c0, %104 : index
          %112 = arith.select %107, %c0, %110 : index
          %c1_136 = arith.constant 1 : index
          %c0_137 = arith.constant 0 : index
          %113 = arith.maxui %10, %c1_136 : index
          %114 = arith.muli %16, %113 : index
          %115 = arith.divui %98, %113 : index
          %116 = arith.addi %98, %c524288 : index
          %117 = arith.subi %116, %c1_136 : index
          %118 = arith.divui %117, %113 : index
          %119 = arith.subi %16, %c1_136 : index
          %120 = arith.cmpi ugt, %115, %119 : index
          %121 = arith.minui %118, %119 : index
          %122 = arith.select %120, %118, %121 : index
          %123 = arith.subi %122, %115 : index
          %124 = arith.addi %123, %c1_136 : index
          %125 = arith.select %120, %c0_137, %115 : index
          %126 = arith.select %120, %c0_137, %124 : index
          %guid_138, %ptr_139 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%125], sizes[%c1_136] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_139 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_140 = arith.constant 1 : index
          %c0_141 = arith.constant 0 : index
          %127 = arith.maxui %20, %c1_140 : index
          %128 = arith.muli %26, %127 : index
          %129 = arith.divui %98, %127 : index
          %130 = arith.addi %98, %c524288 : index
          %131 = arith.subi %130, %c1_140 : index
          %132 = arith.divui %131, %127 : index
          %133 = arith.subi %26, %c1_140 : index
          %134 = arith.cmpi ugt, %129, %133 : index
          %135 = arith.minui %132, %133 : index
          %136 = arith.select %134, %132, %135 : index
          %137 = arith.subi %136, %129 : index
          %138 = arith.addi %137, %c1_140 : index
          %139 = arith.select %134, %c0_141, %129 : index
          %140 = arith.select %134, %c0_141, %138 : index
          %guid_142, %ptr_143 = arts.db_acquire[<out>] (%guid_46 : memref<?xi64>, %ptr_47 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%139], sizes[%c1_140] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_143 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_144 = arith.constant 1 : index
          %c0_145 = arith.constant 0 : index
          %141 = arith.maxui %30, %c1_144 : index
          %142 = arith.muli %36, %141 : index
          %143 = arith.divui %98, %141 : index
          %144 = arith.addi %98, %c524288 : index
          %145 = arith.subi %144, %c1_144 : index
          %146 = arith.divui %145, %141 : index
          %147 = arith.subi %36, %c1_144 : index
          %148 = arith.cmpi ugt, %143, %147 : index
          %149 = arith.minui %146, %147 : index
          %150 = arith.select %148, %146, %149 : index
          %151 = arith.subi %150, %143 : index
          %152 = arith.addi %151, %c1_144 : index
          %153 = arith.select %148, %c0_145, %143 : index
          %154 = arith.select %148, %c0_145, %152 : index
          %guid_146, %ptr_147 = arts.db_acquire[<out>] (%guid_63 : memref<?xi64>, %ptr_64 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%153], sizes[%c1_144] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_147 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_148 = arith.constant 1 : index
          %c0_149 = arith.constant 0 : index
          %155 = arith.maxui %40, %c1_148 : index
          %156 = arith.muli %46, %155 : index
          %157 = arith.divui %98, %155 : index
          %158 = arith.addi %98, %c524288 : index
          %159 = arith.subi %158, %c1_148 : index
          %160 = arith.divui %159, %155 : index
          %161 = arith.subi %46, %c1_148 : index
          %162 = arith.cmpi ugt, %157, %161 : index
          %163 = arith.minui %160, %161 : index
          %164 = arith.select %162, %160, %163 : index
          %165 = arith.subi %164, %157 : index
          %166 = arith.addi %165, %c1_148 : index
          %167 = arith.select %162, %c0_149, %157 : index
          %168 = arith.select %162, %c0_149, %166 : index
          %guid_150, %ptr_151 = arts.db_acquire[<out>] (%guid_80 : memref<?xi64>, %ptr_81 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%167], sizes[%c1_148] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_151 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          %c1_152 = arith.constant 1 : index
          %c0_153 = arith.constant 0 : index
          %169 = arith.maxui %50, %c1_152 : index
          %170 = arith.muli %56, %169 : index
          %171 = arith.divui %98, %169 : index
          %172 = arith.addi %98, %c524288 : index
          %173 = arith.subi %172, %c1_152 : index
          %174 = arith.divui %173, %169 : index
          %175 = arith.subi %56, %c1_152 : index
          %176 = arith.cmpi ugt, %171, %175 : index
          %177 = arith.minui %174, %175 : index
          %178 = arith.select %176, %174, %177 : index
          %179 = arith.subi %178, %171 : index
          %180 = arith.addi %179, %c1_152 : index
          %181 = arith.select %176, %c0_153, %171 : index
          %182 = arith.select %176, %c0_153, %180 : index
          %guid_154, %ptr_155 = arts.db_acquire[<out>] (%guid_97 : memref<?xi64>, %ptr_98 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%181], sizes[%c1_152] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_155 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_156 = arith.constant 1 : index
          %c0_157 = arith.constant 0 : index
          %183 = arith.maxui %60, %c1_156 : index
          %184 = arith.muli %66, %183 : index
          %185 = arith.divui %98, %183 : index
          %186 = arith.addi %98, %c524288 : index
          %187 = arith.subi %186, %c1_156 : index
          %188 = arith.divui %187, %183 : index
          %189 = arith.subi %66, %c1_156 : index
          %190 = arith.cmpi ugt, %185, %189 : index
          %191 = arith.minui %188, %189 : index
          %192 = arith.select %190, %188, %191 : index
          %193 = arith.subi %192, %185 : index
          %194 = arith.addi %193, %c1_156 : index
          %195 = arith.select %190, %c0_157, %185 : index
          %196 = arith.select %190, %c0_157, %194 : index
          %guid_158, %ptr_159 = arts.db_acquire[<out>] (%guid_114 : memref<?xi64>, %ptr_115 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%195], sizes[%c1_156] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_159 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_160 = arith.constant 1 : index
          %c0_161 = arith.constant 0 : index
          %197 = arith.maxui %70, %c1_160 : index
          %198 = arith.muli %76, %197 : index
          %199 = arith.divui %98, %197 : index
          %200 = arith.addi %98, %c524288 : index
          %201 = arith.subi %200, %c1_160 : index
          %202 = arith.divui %201, %197 : index
          %203 = arith.subi %76, %c1_160 : index
          %204 = arith.cmpi ugt, %199, %203 : index
          %205 = arith.minui %202, %203 : index
          %206 = arith.select %204, %202, %205 : index
          %207 = arith.subi %206, %199 : index
          %208 = arith.addi %207, %c1_160 : index
          %209 = arith.select %204, %c0_161, %199 : index
          %210 = arith.select %204, %c0_161, %208 : index
          %guid_162, %ptr_163 = arts.db_acquire[<out>] (%guid_131 : memref<?xi64>, %ptr_132 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%209], sizes[%c1_160] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_163 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_139, %ptr_143, %ptr_147, %ptr_151, %ptr_155, %ptr_159, %ptr_163) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 275 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>, %arg4: memref<?xmemref<?xf32>>, %arg5: memref<?xmemref<?xf32>>, %arg6: memref<?xmemref<?xf32>>, %arg7: memref<?xmemref<?xf32>>, %arg8: memref<?xmemref<?xf32>>, %arg9: memref<?xmemref<?xf32>>):
            %c1_164 = arith.constant 1 : index
            %c0_165 = arith.constant 0 : index
            %211 = arith.maxui %20, %c1_164 : index
            %c0_166 = arith.constant 0 : index
            %c524288_167 = arith.constant 524288 : index
            %212 = arith.muli %arg2, %c524288_167 : index
            %213 = arith.minui %c524288_167, %c524288_167 : index
            %214 = arith.minui %213, %c524288_167 : index
            %215 = arith.minui %214, %c524288_167 : index
            %c1_168 = arith.constant 1 : index
            %216 = arith.maxui %215, %c1_168 : index
            %c1_169 = arith.constant 1 : index
            %217 = arith.maxui %216, %c1_169 : index
            %218 = arith.divui %212, %217 : index
            %219 = arith.select %134, %c0_166, %218 : index
            %c1_170 = arith.constant 1 : index
            %c0_171 = arith.constant 0 : index
            %220 = arith.maxui %10, %c1_170 : index
            %c0_172 = arith.constant 0 : index
            %c524288_173 = arith.constant 524288 : index
            %221 = arith.muli %arg2, %c524288_173 : index
            %222 = arith.minui %c524288_173, %c524288_173 : index
            %223 = arith.minui %222, %c524288_173 : index
            %224 = arith.minui %223, %c524288_173 : index
            %c1_174 = arith.constant 1 : index
            %225 = arith.maxui %224, %c1_174 : index
            %c1_175 = arith.constant 1 : index
            %226 = arith.maxui %225, %c1_175 : index
            %227 = arith.divui %221, %226 : index
            %228 = arith.select %120, %c0_172, %227 : index
            %c1_176 = arith.constant 1 : index
            %c0_177 = arith.constant 0 : index
            %229 = arith.maxui %50, %c1_176 : index
            %c0_178 = arith.constant 0 : index
            %c524288_179 = arith.constant 524288 : index
            %230 = arith.muli %arg2, %c524288_179 : index
            %231 = arith.minui %c524288_179, %c524288_179 : index
            %232 = arith.minui %231, %c524288_179 : index
            %233 = arith.minui %232, %c524288_179 : index
            %c1_180 = arith.constant 1 : index
            %234 = arith.maxui %233, %c1_180 : index
            %c1_181 = arith.constant 1 : index
            %235 = arith.maxui %234, %c1_181 : index
            %236 = arith.divui %230, %235 : index
            %237 = arith.select %176, %c0_178, %236 : index
            %c1_182 = arith.constant 1 : index
            %c0_183 = arith.constant 0 : index
            %238 = arith.maxui %30, %c1_182 : index
            %c0_184 = arith.constant 0 : index
            %c524288_185 = arith.constant 524288 : index
            %239 = arith.muli %arg2, %c524288_185 : index
            %240 = arith.minui %c524288_185, %c524288_185 : index
            %241 = arith.minui %240, %c524288_185 : index
            %242 = arith.minui %241, %c524288_185 : index
            %c1_186 = arith.constant 1 : index
            %243 = arith.maxui %242, %c1_186 : index
            %c1_187 = arith.constant 1 : index
            %244 = arith.maxui %243, %c1_187 : index
            %245 = arith.divui %239, %244 : index
            %246 = arith.select %148, %c0_184, %245 : index
            %c1_188 = arith.constant 1 : index
            %c0_189 = arith.constant 0 : index
            %247 = arith.maxui %60, %c1_188 : index
            %c0_190 = arith.constant 0 : index
            %c524288_191 = arith.constant 524288 : index
            %248 = arith.muli %arg2, %c524288_191 : index
            %249 = arith.minui %c524288_191, %c524288_191 : index
            %250 = arith.minui %249, %c524288_191 : index
            %251 = arith.minui %250, %c524288_191 : index
            %c1_192 = arith.constant 1 : index
            %252 = arith.maxui %251, %c1_192 : index
            %c1_193 = arith.constant 1 : index
            %253 = arith.maxui %252, %c1_193 : index
            %254 = arith.divui %248, %253 : index
            %255 = arith.select %190, %c0_190, %254 : index
            %c1_194 = arith.constant 1 : index
            %c0_195 = arith.constant 0 : index
            %256 = arith.maxui %40, %c1_194 : index
            %c0_196 = arith.constant 0 : index
            %c524288_197 = arith.constant 524288 : index
            %257 = arith.muli %arg2, %c524288_197 : index
            %258 = arith.minui %c524288_197, %c524288_197 : index
            %259 = arith.minui %258, %c524288_197 : index
            %260 = arith.minui %259, %c524288_197 : index
            %c1_198 = arith.constant 1 : index
            %261 = arith.maxui %260, %c1_198 : index
            %c1_199 = arith.constant 1 : index
            %262 = arith.maxui %261, %c1_199 : index
            %263 = arith.divui %257, %262 : index
            %264 = arith.select %162, %c0_196, %263 : index
            %c1_200 = arith.constant 1 : index
            %c0_201 = arith.constant 0 : index
            %265 = arith.maxui %70, %c1_200 : index
            %c0_202 = arith.constant 0 : index
            %c524288_203 = arith.constant 524288 : index
            %266 = arith.muli %arg2, %c524288_203 : index
            %267 = arith.minui %c524288_203, %c524288_203 : index
            %268 = arith.minui %267, %c524288_203 : index
            %269 = arith.minui %268, %c524288_203 : index
            %c1_204 = arith.constant 1 : index
            %270 = arith.maxui %269, %c1_204 : index
            %c1_205 = arith.constant 1 : index
            %271 = arith.maxui %270, %c1_205 : index
            %272 = arith.divui %266, %271 : index
            %273 = arith.select %204, %c0_202, %272 : index
            %274 = arith.subi %c0, %98 : index
            %275 = arith.cmpi slt, %274, %c0 : index
            %276 = arith.select %275, %c0, %274 : index
            %277 = arith.cmpi slt, %100, %c0 : index
            %278 = arith.select %277, %c0, %100 : index
            %279 = arith.minui %278, %102 : index
            scf.for %arg10 = %276 to %279 step %c1 {
              %280 = arith.addi %98, %arg10 : index
              %281 = arith.index_cast %280 : index to i32
              %282 = arith.sitofp %281 : i32 to f32
              %283 = arith.divf %282, %cst_5 : f32
              %284 = arith.mulf %283, %cst_8 : f32
              %285 = arith.addf %284, %cst_9 : f32
              %286 = arith.mulf %285, %285 : f32
              %287 = arith.mulf %286, %285 : f32
              %288 = arith.mulf %287, %cst_3 : f32
              %289 = arith.addf %285, %288 : f32
              %290 = arith.mulf %289, %cst_2 : f32
              %291 = arith.mulf %285, %cst_1 : f32
              %292 = func.call @tanhf(%290) : (f32) -> f32
              %293 = arith.addf %292, %cst_0 : f32
              %294 = arith.mulf %291, %293 : f32
              %c1_206 = arith.constant 1 : index
              %295 = arith.maxui %256, %c1_206 : index
              %c0_207 = arith.constant 0 : index
              %c1_208 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %296 = arith.addi %98, %276 : index
              %297 = arith.divui %296, %295 : index
              %298 = arith.subi %arg10, %276 : index
              %299 = arith.addi %276, %298 : index
              %300 = arith.subi %297, %264 : index
              %301 = arts.db_ref %arg6[%c0_195] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %294, %301[%arg10] : memref<?xf32>
            } {arts.id = 271 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:193:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?xf32>>
            arts.db_release(%arg6) : memref<?xmemref<?xf32>>
            arts.db_release(%arg7) : memref<?xmemref<?xf32>>
            arts.db_release(%arg8) : memref<?xmemref<?xf32>>
            arts.db_release(%arg9) : memref<?xmemref<?xf32>>
          }
        }
      }
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %98 = arith.muli %arg2, %c524288 : index
        %99 = arith.cmpi uge, %98, %c1048576 : index
        %100 = arith.subi %c1048576, %98 : index
        %101 = arith.select %99, %c0, %100 : index
        %102 = arith.minui %101, %c524288 : index
        %103 = arith.cmpi ugt, %102, %c0 : index
        scf.if %103 {
          %104 = arith.divui %98, %c1048576 : index
          %105 = arith.addi %98, %c524287 : index
          %106 = arith.divui %105, %c1048576 : index
          %107 = arith.cmpi ugt, %104, %c0 : index
          %108 = arith.select %107, %106, %c0 : index
          %109 = arith.subi %108, %104 : index
          %110 = arith.addi %109, %c1 : index
          %111 = arith.select %107, %c0, %104 : index
          %112 = arith.select %107, %c0, %110 : index
          %c1_136 = arith.constant 1 : index
          %c0_137 = arith.constant 0 : index
          %113 = arith.maxui %10, %c1_136 : index
          %114 = arith.muli %16, %113 : index
          %115 = arith.divui %98, %113 : index
          %116 = arith.addi %98, %c524288 : index
          %117 = arith.subi %116, %c1_136 : index
          %118 = arith.divui %117, %113 : index
          %119 = arith.subi %16, %c1_136 : index
          %120 = arith.cmpi ugt, %115, %119 : index
          %121 = arith.minui %118, %119 : index
          %122 = arith.select %120, %118, %121 : index
          %123 = arith.subi %122, %115 : index
          %124 = arith.addi %123, %c1_136 : index
          %125 = arith.select %120, %c0_137, %115 : index
          %126 = arith.select %120, %c0_137, %124 : index
          %guid_138, %ptr_139 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%125], sizes[%c1_136] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_139 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_140 = arith.constant 1 : index
          %c0_141 = arith.constant 0 : index
          %127 = arith.maxui %20, %c1_140 : index
          %128 = arith.muli %26, %127 : index
          %129 = arith.divui %98, %127 : index
          %130 = arith.addi %98, %c524288 : index
          %131 = arith.subi %130, %c1_140 : index
          %132 = arith.divui %131, %127 : index
          %133 = arith.subi %26, %c1_140 : index
          %134 = arith.cmpi ugt, %129, %133 : index
          %135 = arith.minui %132, %133 : index
          %136 = arith.select %134, %132, %135 : index
          %137 = arith.subi %136, %129 : index
          %138 = arith.addi %137, %c1_140 : index
          %139 = arith.select %134, %c0_141, %129 : index
          %140 = arith.select %134, %c0_141, %138 : index
          %guid_142, %ptr_143 = arts.db_acquire[<out>] (%guid_46 : memref<?xi64>, %ptr_47 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%139], sizes[%c1_140] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_143 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_144 = arith.constant 1 : index
          %c0_145 = arith.constant 0 : index
          %141 = arith.maxui %30, %c1_144 : index
          %142 = arith.muli %36, %141 : index
          %143 = arith.divui %98, %141 : index
          %144 = arith.addi %98, %c524288 : index
          %145 = arith.subi %144, %c1_144 : index
          %146 = arith.divui %145, %141 : index
          %147 = arith.subi %36, %c1_144 : index
          %148 = arith.cmpi ugt, %143, %147 : index
          %149 = arith.minui %146, %147 : index
          %150 = arith.select %148, %146, %149 : index
          %151 = arith.subi %150, %143 : index
          %152 = arith.addi %151, %c1_144 : index
          %153 = arith.select %148, %c0_145, %143 : index
          %154 = arith.select %148, %c0_145, %152 : index
          %guid_146, %ptr_147 = arts.db_acquire[<out>] (%guid_63 : memref<?xi64>, %ptr_64 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%153], sizes[%c1_144] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_147 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_148 = arith.constant 1 : index
          %c0_149 = arith.constant 0 : index
          %155 = arith.maxui %40, %c1_148 : index
          %156 = arith.muli %46, %155 : index
          %157 = arith.divui %98, %155 : index
          %158 = arith.addi %98, %c524288 : index
          %159 = arith.subi %158, %c1_148 : index
          %160 = arith.divui %159, %155 : index
          %161 = arith.subi %46, %c1_148 : index
          %162 = arith.cmpi ugt, %157, %161 : index
          %163 = arith.minui %160, %161 : index
          %164 = arith.select %162, %160, %163 : index
          %165 = arith.subi %164, %157 : index
          %166 = arith.addi %165, %c1_148 : index
          %167 = arith.select %162, %c0_149, %157 : index
          %168 = arith.select %162, %c0_149, %166 : index
          %guid_150, %ptr_151 = arts.db_acquire[<out>] (%guid_80 : memref<?xi64>, %ptr_81 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%167], sizes[%c1_148] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_151 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_152 = arith.constant 1 : index
          %c0_153 = arith.constant 0 : index
          %169 = arith.maxui %50, %c1_152 : index
          %170 = arith.muli %56, %169 : index
          %171 = arith.divui %98, %169 : index
          %172 = arith.addi %98, %c524288 : index
          %173 = arith.subi %172, %c1_152 : index
          %174 = arith.divui %173, %169 : index
          %175 = arith.subi %56, %c1_152 : index
          %176 = arith.cmpi ugt, %171, %175 : index
          %177 = arith.minui %174, %175 : index
          %178 = arith.select %176, %174, %177 : index
          %179 = arith.subi %178, %171 : index
          %180 = arith.addi %179, %c1_152 : index
          %181 = arith.select %176, %c0_153, %171 : index
          %182 = arith.select %176, %c0_153, %180 : index
          %guid_154, %ptr_155 = arts.db_acquire[<out>] (%guid_97 : memref<?xi64>, %ptr_98 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%181], sizes[%c1_152] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_155 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          %c1_156 = arith.constant 1 : index
          %c0_157 = arith.constant 0 : index
          %183 = arith.maxui %60, %c1_156 : index
          %184 = arith.muli %66, %183 : index
          %185 = arith.divui %98, %183 : index
          %186 = arith.addi %98, %c524288 : index
          %187 = arith.subi %186, %c1_156 : index
          %188 = arith.divui %187, %183 : index
          %189 = arith.subi %66, %c1_156 : index
          %190 = arith.cmpi ugt, %185, %189 : index
          %191 = arith.minui %188, %189 : index
          %192 = arith.select %190, %188, %191 : index
          %193 = arith.subi %192, %185 : index
          %194 = arith.addi %193, %c1_156 : index
          %195 = arith.select %190, %c0_157, %185 : index
          %196 = arith.select %190, %c0_157, %194 : index
          %guid_158, %ptr_159 = arts.db_acquire[<out>] (%guid_114 : memref<?xi64>, %ptr_115 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%195], sizes[%c1_156] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_159 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          %c1_160 = arith.constant 1 : index
          %c0_161 = arith.constant 0 : index
          %197 = arith.maxui %70, %c1_160 : index
          %198 = arith.muli %76, %197 : index
          %199 = arith.divui %98, %197 : index
          %200 = arith.addi %98, %c524288 : index
          %201 = arith.subi %200, %c1_160 : index
          %202 = arith.divui %201, %197 : index
          %203 = arith.subi %76, %c1_160 : index
          %204 = arith.cmpi ugt, %199, %203 : index
          %205 = arith.minui %202, %203 : index
          %206 = arith.select %204, %202, %205 : index
          %207 = arith.subi %206, %199 : index
          %208 = arith.addi %207, %c1_160 : index
          %209 = arith.select %204, %c0_161, %199 : index
          %210 = arith.select %204, %c0_161, %208 : index
          %guid_162, %ptr_163 = arts.db_acquire[<out>] (%guid_131 : memref<?xi64>, %ptr_132 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%209], sizes[%c1_160] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_163 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_139, %ptr_143, %ptr_147, %ptr_151, %ptr_155, %ptr_159, %ptr_163) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 276 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>, %arg4: memref<?xmemref<?xf32>>, %arg5: memref<?xmemref<?xf32>>, %arg6: memref<?xmemref<?xf32>>, %arg7: memref<?xmemref<?xf32>>, %arg8: memref<?xmemref<?xf32>>, %arg9: memref<?xmemref<?xf32>>):
            %c1_164 = arith.constant 1 : index
            %c0_165 = arith.constant 0 : index
            %211 = arith.maxui %20, %c1_164 : index
            %c0_166 = arith.constant 0 : index
            %c524288_167 = arith.constant 524288 : index
            %212 = arith.muli %arg2, %c524288_167 : index
            %213 = arith.minui %c524288_167, %c524288_167 : index
            %214 = arith.minui %213, %c524288_167 : index
            %215 = arith.minui %214, %c524288_167 : index
            %c1_168 = arith.constant 1 : index
            %216 = arith.maxui %215, %c1_168 : index
            %c1_169 = arith.constant 1 : index
            %217 = arith.maxui %216, %c1_169 : index
            %218 = arith.divui %212, %217 : index
            %219 = arith.select %134, %c0_166, %218 : index
            %c1_170 = arith.constant 1 : index
            %c0_171 = arith.constant 0 : index
            %220 = arith.maxui %10, %c1_170 : index
            %c0_172 = arith.constant 0 : index
            %c524288_173 = arith.constant 524288 : index
            %221 = arith.muli %arg2, %c524288_173 : index
            %222 = arith.minui %c524288_173, %c524288_173 : index
            %223 = arith.minui %222, %c524288_173 : index
            %224 = arith.minui %223, %c524288_173 : index
            %c1_174 = arith.constant 1 : index
            %225 = arith.maxui %224, %c1_174 : index
            %c1_175 = arith.constant 1 : index
            %226 = arith.maxui %225, %c1_175 : index
            %227 = arith.divui %221, %226 : index
            %228 = arith.select %120, %c0_172, %227 : index
            %c1_176 = arith.constant 1 : index
            %c0_177 = arith.constant 0 : index
            %229 = arith.maxui %50, %c1_176 : index
            %c0_178 = arith.constant 0 : index
            %c524288_179 = arith.constant 524288 : index
            %230 = arith.muli %arg2, %c524288_179 : index
            %231 = arith.minui %c524288_179, %c524288_179 : index
            %232 = arith.minui %231, %c524288_179 : index
            %233 = arith.minui %232, %c524288_179 : index
            %c1_180 = arith.constant 1 : index
            %234 = arith.maxui %233, %c1_180 : index
            %c1_181 = arith.constant 1 : index
            %235 = arith.maxui %234, %c1_181 : index
            %236 = arith.divui %230, %235 : index
            %237 = arith.select %176, %c0_178, %236 : index
            %c1_182 = arith.constant 1 : index
            %c0_183 = arith.constant 0 : index
            %238 = arith.maxui %30, %c1_182 : index
            %c0_184 = arith.constant 0 : index
            %c524288_185 = arith.constant 524288 : index
            %239 = arith.muli %arg2, %c524288_185 : index
            %240 = arith.minui %c524288_185, %c524288_185 : index
            %241 = arith.minui %240, %c524288_185 : index
            %242 = arith.minui %241, %c524288_185 : index
            %c1_186 = arith.constant 1 : index
            %243 = arith.maxui %242, %c1_186 : index
            %c1_187 = arith.constant 1 : index
            %244 = arith.maxui %243, %c1_187 : index
            %245 = arith.divui %239, %244 : index
            %246 = arith.select %148, %c0_184, %245 : index
            %c1_188 = arith.constant 1 : index
            %c0_189 = arith.constant 0 : index
            %247 = arith.maxui %60, %c1_188 : index
            %c0_190 = arith.constant 0 : index
            %c524288_191 = arith.constant 524288 : index
            %248 = arith.muli %arg2, %c524288_191 : index
            %249 = arith.minui %c524288_191, %c524288_191 : index
            %250 = arith.minui %249, %c524288_191 : index
            %251 = arith.minui %250, %c524288_191 : index
            %c1_192 = arith.constant 1 : index
            %252 = arith.maxui %251, %c1_192 : index
            %c1_193 = arith.constant 1 : index
            %253 = arith.maxui %252, %c1_193 : index
            %254 = arith.divui %248, %253 : index
            %255 = arith.select %190, %c0_190, %254 : index
            %c1_194 = arith.constant 1 : index
            %c0_195 = arith.constant 0 : index
            %256 = arith.maxui %40, %c1_194 : index
            %c0_196 = arith.constant 0 : index
            %c524288_197 = arith.constant 524288 : index
            %257 = arith.muli %arg2, %c524288_197 : index
            %258 = arith.minui %c524288_197, %c524288_197 : index
            %259 = arith.minui %258, %c524288_197 : index
            %260 = arith.minui %259, %c524288_197 : index
            %c1_198 = arith.constant 1 : index
            %261 = arith.maxui %260, %c1_198 : index
            %c1_199 = arith.constant 1 : index
            %262 = arith.maxui %261, %c1_199 : index
            %263 = arith.divui %257, %262 : index
            %264 = arith.select %162, %c0_196, %263 : index
            %c1_200 = arith.constant 1 : index
            %c0_201 = arith.constant 0 : index
            %265 = arith.maxui %70, %c1_200 : index
            %c0_202 = arith.constant 0 : index
            %c524288_203 = arith.constant 524288 : index
            %266 = arith.muli %arg2, %c524288_203 : index
            %267 = arith.minui %c524288_203, %c524288_203 : index
            %268 = arith.minui %267, %c524288_203 : index
            %269 = arith.minui %268, %c524288_203 : index
            %c1_204 = arith.constant 1 : index
            %270 = arith.maxui %269, %c1_204 : index
            %c1_205 = arith.constant 1 : index
            %271 = arith.maxui %270, %c1_205 : index
            %272 = arith.divui %266, %271 : index
            %273 = arith.select %204, %c0_202, %272 : index
            %274 = arith.subi %c0, %98 : index
            %275 = arith.cmpi slt, %274, %c0 : index
            %276 = arith.select %275, %c0, %274 : index
            %277 = arith.cmpi slt, %100, %c0 : index
            %278 = arith.select %277, %c0, %100 : index
            %279 = arith.minui %278, %102 : index
            scf.for %arg10 = %276 to %279 step %c1 {
              %280 = arith.addi %98, %arg10 : index
              %281 = arith.index_cast %280 : index to i32
              %282 = arith.sitofp %281 : i32 to f32
              %283 = arith.divf %282, %cst_5 : f32
              %284 = arith.mulf %283, %cst_8 : f32
              %285 = arith.addf %284, %cst_9 : f32
              %286 = arith.mulf %285, %cst : f32
              %287 = math.exp %286 : f32
              %288 = arith.addf %287, %cst_0 : f32
              %289 = arith.divf %cst_0, %288 : f32
              %290 = arith.mulf %285, %289 : f32
              %c1_206 = arith.constant 1 : index
              %291 = arith.maxui %229, %c1_206 : index
              %c0_207 = arith.constant 0 : index
              %c1_208 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %292 = arith.addi %98, %276 : index
              %293 = arith.divui %292, %291 : index
              %294 = arith.subi %arg10, %276 : index
              %295 = arith.addi %276, %294 : index
              %296 = arith.subi %293, %237 : index
              %297 = arts.db_ref %arg7[%c0_177] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %290, %297[%arg10] : memref<?xf32>
              %298 = arith.negf %285 : f32
              %299 = math.exp %298 : f32
              %300 = arith.addf %299, %cst_0 : f32
              %301 = arith.divf %cst_0, %300 : f32
              %c1_209 = arith.constant 1 : index
              %302 = arith.maxui %247, %c1_209 : index
              %c0_210 = arith.constant 0 : index
              %c1_211 = arith.constant 1 : index
              %c-1_212 = arith.constant -1 : index
              %303 = arith.addi %98, %276 : index
              %304 = arith.divui %303, %302 : index
              %305 = arith.subi %arg10, %276 : index
              %306 = arith.addi %276, %305 : index
              %307 = arith.subi %304, %255 : index
              %308 = arts.db_ref %arg8[%c0_189] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %301, %308[%arg10] : memref<?xf32>
            } {arts.id = 272 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:194:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?xf32>>
            arts.db_release(%arg6) : memref<?xmemref<?xf32>>
            arts.db_release(%arg7) : memref<?xmemref<?xf32>>
            arts.db_release(%arg8) : memref<?xmemref<?xf32>>
            arts.db_release(%arg9) : memref<?xmemref<?xf32>>
          }
        }
      }
      scf.for %arg2 = %c0 to %c2 step %c1 {
        %98 = arith.muli %arg2, %c524288 : index
        %99 = arith.cmpi uge, %98, %c1048576 : index
        %100 = arith.subi %c1048576, %98 : index
        %101 = arith.select %99, %c0, %100 : index
        %102 = arith.minui %101, %c524288 : index
        %103 = arith.cmpi ugt, %102, %c0 : index
        scf.if %103 {
          %104 = arith.divui %98, %c1048576 : index
          %105 = arith.addi %98, %c524287 : index
          %106 = arith.divui %105, %c1048576 : index
          %107 = arith.cmpi ugt, %104, %c0 : index
          %108 = arith.select %107, %106, %c0 : index
          %109 = arith.subi %108, %104 : index
          %110 = arith.addi %109, %c1 : index
          %111 = arith.select %107, %c0, %104 : index
          %112 = arith.select %107, %c0, %110 : index
          %c1_136 = arith.constant 1 : index
          %c0_137 = arith.constant 0 : index
          %113 = arith.maxui %10, %c1_136 : index
          %114 = arith.muli %16, %113 : index
          %115 = arith.divui %98, %113 : index
          %116 = arith.addi %98, %c524288 : index
          %117 = arith.subi %116, %c1_136 : index
          %118 = arith.divui %117, %113 : index
          %119 = arith.subi %16, %c1_136 : index
          %120 = arith.cmpi ugt, %115, %119 : index
          %121 = arith.minui %118, %119 : index
          %122 = arith.select %120, %118, %121 : index
          %123 = arith.subi %122, %115 : index
          %124 = arith.addi %123, %c1_136 : index
          %125 = arith.select %120, %c0_137, %115 : index
          %126 = arith.select %120, %c0_137, %124 : index
          %guid_138, %ptr_139 = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%125], sizes[%c1_136] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_139 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_140 = arith.constant 1 : index
          %c0_141 = arith.constant 0 : index
          %127 = arith.maxui %20, %c1_140 : index
          %128 = arith.muli %26, %127 : index
          %129 = arith.divui %98, %127 : index
          %130 = arith.addi %98, %c524288 : index
          %131 = arith.subi %130, %c1_140 : index
          %132 = arith.divui %131, %127 : index
          %133 = arith.subi %26, %c1_140 : index
          %134 = arith.cmpi ugt, %129, %133 : index
          %135 = arith.minui %132, %133 : index
          %136 = arith.select %134, %132, %135 : index
          %137 = arith.subi %136, %129 : index
          %138 = arith.addi %137, %c1_140 : index
          %139 = arith.select %134, %c0_141, %129 : index
          %140 = arith.select %134, %c0_141, %138 : index
          %guid_142, %ptr_143 = arts.db_acquire[<out>] (%guid_46 : memref<?xi64>, %ptr_47 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%139], sizes[%c1_140] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_143 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_144 = arith.constant 1 : index
          %c0_145 = arith.constant 0 : index
          %141 = arith.maxui %30, %c1_144 : index
          %142 = arith.muli %36, %141 : index
          %143 = arith.divui %98, %141 : index
          %144 = arith.addi %98, %c524288 : index
          %145 = arith.subi %144, %c1_144 : index
          %146 = arith.divui %145, %141 : index
          %147 = arith.subi %36, %c1_144 : index
          %148 = arith.cmpi ugt, %143, %147 : index
          %149 = arith.minui %146, %147 : index
          %150 = arith.select %148, %146, %149 : index
          %151 = arith.subi %150, %143 : index
          %152 = arith.addi %151, %c1_144 : index
          %153 = arith.select %148, %c0_145, %143 : index
          %154 = arith.select %148, %c0_145, %152 : index
          %guid_146, %ptr_147 = arts.db_acquire[<out>] (%guid_63 : memref<?xi64>, %ptr_64 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%153], sizes[%c1_144] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_147 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_148 = arith.constant 1 : index
          %c0_149 = arith.constant 0 : index
          %155 = arith.maxui %40, %c1_148 : index
          %156 = arith.muli %46, %155 : index
          %157 = arith.divui %98, %155 : index
          %158 = arith.addi %98, %c524288 : index
          %159 = arith.subi %158, %c1_148 : index
          %160 = arith.divui %159, %155 : index
          %161 = arith.subi %46, %c1_148 : index
          %162 = arith.cmpi ugt, %157, %161 : index
          %163 = arith.minui %160, %161 : index
          %164 = arith.select %162, %160, %163 : index
          %165 = arith.subi %164, %157 : index
          %166 = arith.addi %165, %c1_148 : index
          %167 = arith.select %162, %c0_149, %157 : index
          %168 = arith.select %162, %c0_149, %166 : index
          %guid_150, %ptr_151 = arts.db_acquire[<out>] (%guid_80 : memref<?xi64>, %ptr_81 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%167], sizes[%c1_148] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_151 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_152 = arith.constant 1 : index
          %c0_153 = arith.constant 0 : index
          %169 = arith.maxui %50, %c1_152 : index
          %170 = arith.muli %56, %169 : index
          %171 = arith.divui %98, %169 : index
          %172 = arith.addi %98, %c524288 : index
          %173 = arith.subi %172, %c1_152 : index
          %174 = arith.divui %173, %169 : index
          %175 = arith.subi %56, %c1_152 : index
          %176 = arith.cmpi ugt, %171, %175 : index
          %177 = arith.minui %174, %175 : index
          %178 = arith.select %176, %174, %177 : index
          %179 = arith.subi %178, %171 : index
          %180 = arith.addi %179, %c1_152 : index
          %181 = arith.select %176, %c0_153, %171 : index
          %182 = arith.select %176, %c0_153, %180 : index
          %guid_154, %ptr_155 = arts.db_acquire[<out>] (%guid_97 : memref<?xi64>, %ptr_98 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%181], sizes[%c1_152] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_155 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_156 = arith.constant 1 : index
          %c0_157 = arith.constant 0 : index
          %183 = arith.maxui %60, %c1_156 : index
          %184 = arith.muli %66, %183 : index
          %185 = arith.divui %98, %183 : index
          %186 = arith.addi %98, %c524288 : index
          %187 = arith.subi %186, %c1_156 : index
          %188 = arith.divui %187, %183 : index
          %189 = arith.subi %66, %c1_156 : index
          %190 = arith.cmpi ugt, %185, %189 : index
          %191 = arith.minui %188, %189 : index
          %192 = arith.select %190, %188, %191 : index
          %193 = arith.subi %192, %185 : index
          %194 = arith.addi %193, %c1_156 : index
          %195 = arith.select %190, %c0_157, %185 : index
          %196 = arith.select %190, %c0_157, %194 : index
          %guid_158, %ptr_159 = arts.db_acquire[<out>] (%guid_114 : memref<?xi64>, %ptr_115 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%195], sizes[%c1_156] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = []} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_159 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>)
          %c1_160 = arith.constant 1 : index
          %c0_161 = arith.constant 0 : index
          %197 = arith.maxui %70, %c1_160 : index
          %198 = arith.muli %76, %197 : index
          %199 = arith.divui %98, %197 : index
          %200 = arith.addi %98, %c524288 : index
          %201 = arith.subi %200, %c1_160 : index
          %202 = arith.divui %201, %197 : index
          %203 = arith.subi %76, %c1_160 : index
          %204 = arith.cmpi ugt, %199, %203 : index
          %205 = arith.minui %202, %203 : index
          %206 = arith.select %204, %202, %205 : index
          %207 = arith.subi %206, %199 : index
          %208 = arith.addi %207, %c1_160 : index
          %209 = arith.select %204, %c0_161, %199 : index
          %210 = arith.select %204, %c0_161, %208 : index
          %guid_162, %ptr_163 = arts.db_acquire[<out>] (%guid_131 : memref<?xi64>, %ptr_132 : memref<?xmemref<?xf32>>) partitioning(<block>, offsets[%98], sizes[%c524288]), offsets[%209], sizes[%c1_160] element_offsets[%98] element_sizes[%c524288] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, preserve_dep_edge = #arts.preserve_dep_edge, stencil_block_shape = [524288], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
          arts.lowering_contract(%ptr_163 : memref<?xmemref<?xf32>>) pattern(<depPattern = <elementwise_pipeline>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c524288] contract(<ownerDims = [0]>)
          arts.edt <task> <intranode> route(%c-1_i32) (%ptr_139, %ptr_143, %ptr_147, %ptr_151, %ptr_155, %ptr_159, %ptr_163) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 277 : i64, arts.pattern_revision = 2 : i64, arts.plan.async_strategy = "blocking", arts.plan.cost.expected_local_work = 0 : i64, arts.plan.cost.relaunch_amortization = 0 : i64, arts.plan.cost.scheduler_overhead = 300 : i64, arts.plan.cost.slice_widening_pressure = 0 : i64, arts.plan.iteration_topology = "owner_strip", arts.plan.kernel_family = "uniform", arts.plan.repetition_structure = "none", depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [524288]} {
          ^bb0(%arg3: memref<?xmemref<?xf32>>, %arg4: memref<?xmemref<?xf32>>, %arg5: memref<?xmemref<?xf32>>, %arg6: memref<?xmemref<?xf32>>, %arg7: memref<?xmemref<?xf32>>, %arg8: memref<?xmemref<?xf32>>, %arg9: memref<?xmemref<?xf32>>):
            %c1_164 = arith.constant 1 : index
            %c0_165 = arith.constant 0 : index
            %211 = arith.maxui %20, %c1_164 : index
            %c0_166 = arith.constant 0 : index
            %c524288_167 = arith.constant 524288 : index
            %212 = arith.muli %arg2, %c524288_167 : index
            %213 = arith.minui %c524288_167, %c524288_167 : index
            %214 = arith.minui %213, %c524288_167 : index
            %215 = arith.minui %214, %c524288_167 : index
            %c1_168 = arith.constant 1 : index
            %216 = arith.maxui %215, %c1_168 : index
            %c1_169 = arith.constant 1 : index
            %217 = arith.maxui %216, %c1_169 : index
            %218 = arith.divui %212, %217 : index
            %219 = arith.select %134, %c0_166, %218 : index
            %c1_170 = arith.constant 1 : index
            %c0_171 = arith.constant 0 : index
            %220 = arith.maxui %10, %c1_170 : index
            %c0_172 = arith.constant 0 : index
            %c524288_173 = arith.constant 524288 : index
            %221 = arith.muli %arg2, %c524288_173 : index
            %222 = arith.minui %c524288_173, %c524288_173 : index
            %223 = arith.minui %222, %c524288_173 : index
            %224 = arith.minui %223, %c524288_173 : index
            %c1_174 = arith.constant 1 : index
            %225 = arith.maxui %224, %c1_174 : index
            %c1_175 = arith.constant 1 : index
            %226 = arith.maxui %225, %c1_175 : index
            %227 = arith.divui %221, %226 : index
            %228 = arith.select %120, %c0_172, %227 : index
            %c1_176 = arith.constant 1 : index
            %c0_177 = arith.constant 0 : index
            %229 = arith.maxui %50, %c1_176 : index
            %c0_178 = arith.constant 0 : index
            %c524288_179 = arith.constant 524288 : index
            %230 = arith.muli %arg2, %c524288_179 : index
            %231 = arith.minui %c524288_179, %c524288_179 : index
            %232 = arith.minui %231, %c524288_179 : index
            %233 = arith.minui %232, %c524288_179 : index
            %c1_180 = arith.constant 1 : index
            %234 = arith.maxui %233, %c1_180 : index
            %c1_181 = arith.constant 1 : index
            %235 = arith.maxui %234, %c1_181 : index
            %236 = arith.divui %230, %235 : index
            %237 = arith.select %176, %c0_178, %236 : index
            %c1_182 = arith.constant 1 : index
            %c0_183 = arith.constant 0 : index
            %238 = arith.maxui %30, %c1_182 : index
            %c0_184 = arith.constant 0 : index
            %c524288_185 = arith.constant 524288 : index
            %239 = arith.muli %arg2, %c524288_185 : index
            %240 = arith.minui %c524288_185, %c524288_185 : index
            %241 = arith.minui %240, %c524288_185 : index
            %242 = arith.minui %241, %c524288_185 : index
            %c1_186 = arith.constant 1 : index
            %243 = arith.maxui %242, %c1_186 : index
            %c1_187 = arith.constant 1 : index
            %244 = arith.maxui %243, %c1_187 : index
            %245 = arith.divui %239, %244 : index
            %246 = arith.select %148, %c0_184, %245 : index
            %c1_188 = arith.constant 1 : index
            %c0_189 = arith.constant 0 : index
            %247 = arith.maxui %60, %c1_188 : index
            %c0_190 = arith.constant 0 : index
            %c524288_191 = arith.constant 524288 : index
            %248 = arith.muli %arg2, %c524288_191 : index
            %249 = arith.minui %c524288_191, %c524288_191 : index
            %250 = arith.minui %249, %c524288_191 : index
            %251 = arith.minui %250, %c524288_191 : index
            %c1_192 = arith.constant 1 : index
            %252 = arith.maxui %251, %c1_192 : index
            %c1_193 = arith.constant 1 : index
            %253 = arith.maxui %252, %c1_193 : index
            %254 = arith.divui %248, %253 : index
            %255 = arith.select %190, %c0_190, %254 : index
            %c1_194 = arith.constant 1 : index
            %c0_195 = arith.constant 0 : index
            %256 = arith.maxui %40, %c1_194 : index
            %c0_196 = arith.constant 0 : index
            %c524288_197 = arith.constant 524288 : index
            %257 = arith.muli %arg2, %c524288_197 : index
            %258 = arith.minui %c524288_197, %c524288_197 : index
            %259 = arith.minui %258, %c524288_197 : index
            %260 = arith.minui %259, %c524288_197 : index
            %c1_198 = arith.constant 1 : index
            %261 = arith.maxui %260, %c1_198 : index
            %c1_199 = arith.constant 1 : index
            %262 = arith.maxui %261, %c1_199 : index
            %263 = arith.divui %257, %262 : index
            %264 = arith.select %162, %c0_196, %263 : index
            %c1_200 = arith.constant 1 : index
            %c0_201 = arith.constant 0 : index
            %265 = arith.maxui %70, %c1_200 : index
            %c0_202 = arith.constant 0 : index
            %c524288_203 = arith.constant 524288 : index
            %266 = arith.muli %arg2, %c524288_203 : index
            %267 = arith.minui %c524288_203, %c524288_203 : index
            %268 = arith.minui %267, %c524288_203 : index
            %269 = arith.minui %268, %c524288_203 : index
            %c1_204 = arith.constant 1 : index
            %270 = arith.maxui %269, %c1_204 : index
            %c1_205 = arith.constant 1 : index
            %271 = arith.maxui %270, %c1_205 : index
            %272 = arith.divui %266, %271 : index
            %273 = arith.select %204, %c0_202, %272 : index
            %274 = arith.subi %c0, %98 : index
            %275 = arith.cmpi slt, %274, %c0 : index
            %276 = arith.select %275, %c0, %274 : index
            %277 = arith.cmpi slt, %100, %c0 : index
            %278 = arith.select %277, %c0, %100 : index
            %279 = arith.minui %278, %102 : index
            scf.for %arg10 = %276 to %279 step %c1 {
              %280 = arith.addi %98, %arg10 : index
              %281 = arith.index_cast %280 : index to i32
              %282 = arith.sitofp %281 : i32 to f32
              %283 = arith.divf %282, %cst_5 : f32
              %284 = arith.mulf %283, %cst_8 : f32
              %285 = arith.addf %284, %cst_9 : f32
              %286 = func.call @tanhf(%285) : (f32) -> f32
              %c1_206 = arith.constant 1 : index
              %287 = arith.maxui %265, %c1_206 : index
              %c0_207 = arith.constant 0 : index
              %c1_208 = arith.constant 1 : index
              %c-1 = arith.constant -1 : index
              %288 = arith.addi %98, %276 : index
              %289 = arith.divui %288, %287 : index
              %290 = arith.subi %arg10, %276 : index
              %291 = arith.addi %276, %290 : index
              %292 = arith.subi %289, %273 : index
              %293 = arts.db_ref %arg9[%c0_201] : memref<?xmemref<?xf32>> -> memref<?xf32>
              memref.store %286, %293[%arg10] : memref<?xf32>
            } {arts.id = 273 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:196:5">, arts.metadata_provenance = "exact"}
            arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            arts.db_release(%arg4) : memref<?xmemref<?xf32>>
            arts.db_release(%arg5) : memref<?xmemref<?xf32>>
            arts.db_release(%arg6) : memref<?xmemref<?xf32>>
            arts.db_release(%arg7) : memref<?xmemref<?xf32>>
            arts.db_release(%arg8) : memref<?xmemref<?xf32>>
            arts.db_release(%arg9) : memref<?xmemref<?xf32>>
          }
        }
      }
    } : i64
    %78 = "polygeist.undef"() : () -> f32
    memref.store %78, %alloca_134[] : memref<f32>
    memref.store %78, %alloca_135[] : memref<f32>
    memref.store %cst_9, %alloca_135[] : memref<f32>
    scf.for %arg2 = %c1 to %c100 step %c1 {
      %98 = arith.index_cast %arg2 : index to i32
      %99 = arith.sitofp %98 : i32 to f32
      %100 = arith.divf %99, %cst_6 : f32
      %101 = arith.mulf %100, %cst_8 : f32
      %102 = arith.addf %101, %cst_9 : f32
      %103 = memref.load %alloca_135[] : memref<f32>
      %104 = arith.cmpf ogt, %102, %103 : f32
      scf.if %104 {
        memref.store %102, %alloca_135[] : memref<f32>
      }
    } {arts.id = 139 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 99 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    memref.store %cst_7, %alloca_134[] : memref<f32>
    %79 = memref.load %alloca_135[] : memref<f32>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %98 = arith.index_cast %arg2 : index to i32
      %99 = arith.sitofp %98 : i32 to f32
      %100 = arith.divf %99, %cst_6 : f32
      %101 = arith.mulf %100, %cst_8 : f32
      %102 = arith.addf %101, %cst_9 : f32
      %103 = arith.subf %102, %79 : f32
      %104 = math.exp %103 : f32
      memref.store %104, %alloc[%arg2] : memref<100xf32>
      %105 = memref.load %alloc[%arg2] : memref<100xf32>
      %106 = memref.load %alloca_134[] : memref<f32>
      %107 = arith.addf %106, %105 : f32
      memref.store %107, %alloca_134[] : memref<f32>
    } {arts.id = 150 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    %80 = memref.load %alloca_134[] : memref<f32>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %98 = memref.load %alloc[%arg2] : memref<100xf32>
      %99 = arith.divf %98, %80 : f32
      memref.store %99, %alloc[%arg2] : memref<100xf32>
    } {arts.id = 156 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %81 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%81, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_10, %alloca_17[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %10, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_31 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_17[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_17[] : memref<f64>
    } {arts.id = 160 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:206:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_16[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %20, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_48 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr_47[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_16[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_16[] : memref<f64>
    } {arts.id = 169 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:209:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_15[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %30, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_65 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr_64[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_15[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_15[] : memref<f64>
    } {arts.id = 178 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:212:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_14[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %40, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_82 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr_81[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_14[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_14[] : memref<f64>
    } {arts.id = 187 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:215:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_13[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %50, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_99 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr_98[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_13[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_13[] : memref<f64>
    } {arts.id = 197 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:218:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_12[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %60, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_116 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr_115[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_12[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_12[] : memref<f64>
    } {arts.id = 206 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:221:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca_11[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %c1_136 = arith.constant 1 : index
      %98 = arith.maxui %70, %c1_136 : index
      %99 = arith.divui %arg2, %98 : index
      %100 = arith.subi %99, %c0_133 : index
      %101 = arith.remui %arg2, %98 : index
      %102 = arts.db_ref %ptr_132[%100] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %103 = memref.load %102[%101] : memref<?xf32>
      %104 = arith.extf %103 : f32 to f64
      %105 = math.absf %104 : f64
      %106 = memref.load %alloca_11[] : memref<f64>
      %107 = arith.addf %106, %105 : f64
      memref.store %107, %alloca_11[] : memref<f64>
    } {arts.id = 215 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:224:3">, arts.metadata_provenance = "recovered"}
    memref.store %cst_10, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %98 = memref.load %alloc[%arg2] : memref<100xf32>
      %99 = arith.extf %98 : f32 to f64
      %100 = math.absf %99 : f64
      %101 = memref.load %alloca[] : memref<f64>
      %102 = arith.addf %101, %100 : f64
      memref.store %102, %alloca[] : memref<f64>
    } {arts.id = 225 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:227:3">, arts.metadata_provenance = "recovered"}
    %82 = memref.load %alloca_17[] : memref<f64>
    %83 = memref.load %alloca_16[] : memref<f64>
    %84 = arith.addf %82, %83 : f64
    %85 = memref.load %alloca_15[] : memref<f64>
    %86 = arith.addf %84, %85 : f64
    %87 = memref.load %alloca_14[] : memref<f64>
    %88 = arith.addf %86, %87 : f64
    %89 = memref.load %alloca_13[] : memref<f64>
    %90 = arith.addf %88, %89 : f64
    %91 = memref.load %alloca_12[] : memref<f64>
    %92 = arith.addf %90, %91 : f64
    %93 = memref.load %alloca_11[] : memref<f64>
    %94 = arith.addf %92, %93 : f64
    %95 = memref.load %alloca[] : memref<f64>
    %96 = arith.addf %94, %95 : f64
    call @carts_bench_checksum_d(%96) : (f64) -> ()
    call @carts_phase_timer_stop(%81) : (memref<?xi8>) -> ()
    %97 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%97, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc {arts.id = 301 : i64} : memref<100xf32>
    call @carts_phase_timer_stop(%97) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_63) : memref<?xi64>
    arts.db_free(%ptr_64) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_114) : memref<?xi64>
    arts.db_free(%ptr_115) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_46) : memref<?xi64>
    arts.db_free(%ptr_47) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_131) : memref<?xi64>
    arts.db_free(%ptr_132) : memref<?xmemref<?xf32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_97) : memref<?xi64>
    arts.db_free(%ptr_98) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_80) : memref<?xi64>
    arts.db_free(%ptr_81) : memref<?xmemref<?xf32>>
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
