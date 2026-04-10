module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_vel4sg_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 5.000000e-01 : f32
    %cst_0 = arith.constant 1.000000e+00 : f32
    %cst_1 = arith.constant 5.000000e-04 : f32
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
    %c448 = arith.constant 448 : index
    %c1 = arith.constant 1 : index
    %c320 = arith.constant 320 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_8 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 196 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:149:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 196 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:99:17", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 34 : i64, lastUseId = 35 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_9 = memref.alloc() {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:100:17", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 36 : i64, lastUseId = 37 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_10 = memref.alloc() {arts.id = 38 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:101:17", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 38 : i64, lastUseId = 39 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_11 = memref.alloc() {arts.id = 40 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:102:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 40 : i64, lastUseId = 41 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_12 = memref.alloc() {arts.id = 42 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:103:18", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 42 : i64, lastUseId = 43 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_13 = memref.alloc() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:104:18", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 44 : i64, lastUseId = 45 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_14 = memref.alloc() {arts.id = 46 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:105:18", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 46 : i64, lastUseId = 47 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_15 = memref.alloc() {arts.id = 48 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:106:18", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 48 : i64, lastUseId = 49 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_16 = memref.alloc() {arts.id = 50 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:107:18", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 50 : i64, lastUseId = 51 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %alloc_17 = memref.alloc() {arts.id = 52 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "vel4sg_base.c:108:18", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 183500800 : i64, firstUseId = 52 : i64, lastUseId = 53 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x448xf32>
    %7 = "polygeist.undef"() : () -> i32
    %alloca_18 = memref.alloca() {arts.id = 55 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:135:3", totalAccesses = 11 : i64, readCount = 8 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 55 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    affine.store %7, %alloca_18[] : memref<i32>
    affine.store %c0_i32, %alloca_18[] : memref<i32>
    affine.for %arg0 = 0 to 320 {
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c448 step %c1 {
          memref.store %cst_2, %alloc[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          memref.store %cst_2, %alloc_9[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          memref.store %cst_2, %alloc_10[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %11 = affine.load %alloca_18[] : memref<i32>
          %12 = arith.remsi %11, %c13_i32 : i32
          %13 = arith.sitofp %12 : i32 to f32
          %14 = arith.addf %13, %cst_3 : f32
          memref.store %14, %alloc_11[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %15 = affine.load %alloca_18[] : memref<i32>
          %16 = arith.muli %15, %c7_i32 : i32
          %17 = arith.remsi %16, %c19_i32 : i32
          %18 = arith.sitofp %17 : i32 to f32
          %19 = arith.mulf %18, %cst_4 : f32
          memref.store %19, %alloc_12[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %20 = affine.load %alloca_18[] : memref<i32>
          %21 = arith.muli %20, %c11_i32 : i32
          %22 = arith.remsi %21, %c23_i32 : i32
          %23 = arith.sitofp %22 : i32 to f32
          %24 = arith.mulf %23, %cst_4 : f32
          memref.store %24, %alloc_13[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %25 = affine.load %alloca_18[] : memref<i32>
          %26 = arith.muli %25, %c13_i32 : i32
          %27 = arith.remsi %26, %c17_i32 : i32
          %28 = arith.sitofp %27 : i32 to f32
          %29 = arith.mulf %28, %cst_4 : f32
          memref.store %29, %alloc_14[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %30 = affine.load %alloca_18[] : memref<i32>
          %31 = arith.muli %30, %c5_i32 : i32
          %32 = arith.remsi %31, %c29_i32 : i32
          %33 = arith.sitofp %32 : i32 to f32
          %34 = arith.mulf %33, %cst_5 : f32
          memref.store %34, %alloc_15[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %35 = affine.load %alloca_18[] : memref<i32>
          %36 = arith.muli %35, %c3_i32 : i32
          %37 = arith.remsi %36, %c31_i32 : i32
          %38 = arith.sitofp %37 : i32 to f32
          %39 = arith.mulf %38, %cst_6 : f32
          memref.store %39, %alloc_16[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %40 = affine.load %alloca_18[] : memref<i32>
          %41 = arith.muli %40, %c2_i32 : i32
          %42 = arith.remsi %41, %c37_i32 : i32
          %43 = arith.sitofp %42 : i32 to f32
          %44 = arith.mulf %43, %cst_7 : f32
          memref.store %44, %alloc_17[%arg0, %arg1, %arg2] : memref<320x320x448xf32>
          %45 = affine.load %alloca_18[] : memref<i32>
          %46 = arith.addi %45, %c1_i32 : i32
          affine.store %46, %alloca_18[] : memref<i32>
        } {arts.id = 106 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 448 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:135:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 108 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:135:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 110 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:135:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_19 = memref.alloca() {arts.id = 112 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 112 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_20 = memref.alloca() {arts.id = 113 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 113 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_21 = memref.alloca() {arts.id = 114 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 114 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_22 = memref.alloca() {arts.id = 115 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "vel4sg_base.c:141:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 115 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    affine.for %arg0 = 0 to 10 {
      %11 = "polygeist.undef"() : () -> f32
      affine.store %11, %alloca_19[] : memref<f32>
      affine.store %11, %alloca_20[] : memref<f32>
      affine.store %11, %alloca_21[] : memref<f32>
      affine.store %11, %alloca_22[] : memref<f32>
      affine.for %arg1 = 2 to 446 {
        affine.for %arg2 = 2 to 318 {
          affine.for %arg3 = 2 to 318 {
            %12 = affine.load %alloc_11[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
            %13 = arith.divf %cst_0, %12 : f32
            affine.store %13, %alloca_22[] : memref<f32>
            %14 = affine.load %alloc_12[%arg3 + 1, %arg2, %arg1] : memref<320x320x448xf32>
            %15 = affine.load %alloc_12[%arg3 - 1, %arg2, %arg1] : memref<320x320x448xf32>
            %16 = arith.subf %14, %15 : f32
            %17 = arith.mulf %16, %cst : f32
            %18 = affine.load %alloc_15[%arg3, %arg2 + 1, %arg1] : memref<320x320x448xf32>
            %19 = affine.load %alloc_15[%arg3, %arg2 - 1, %arg1] : memref<320x320x448xf32>
            %20 = arith.subf %18, %19 : f32
            %21 = arith.mulf %20, %cst : f32
            %22 = arith.addf %17, %21 : f32
            %23 = affine.load %alloc_16[%arg3, %arg2, %arg1 + 1] : memref<320x320x448xf32>
            %24 = affine.load %alloc_16[%arg3, %arg2, %arg1 - 1] : memref<320x320x448xf32>
            %25 = arith.subf %23, %24 : f32
            %26 = arith.mulf %25, %cst : f32
            %27 = arith.addf %22, %26 : f32
            affine.store %27, %alloca_21[] : memref<f32>
            %28 = affine.load %alloc_15[%arg3 + 1, %arg2, %arg1] : memref<320x320x448xf32>
            %29 = affine.load %alloc_15[%arg3 - 1, %arg2, %arg1] : memref<320x320x448xf32>
            %30 = arith.subf %28, %29 : f32
            %31 = arith.mulf %30, %cst : f32
            %32 = affine.load %alloc_13[%arg3, %arg2 + 1, %arg1] : memref<320x320x448xf32>
            %33 = affine.load %alloc_13[%arg3, %arg2 - 1, %arg1] : memref<320x320x448xf32>
            %34 = arith.subf %32, %33 : f32
            %35 = arith.mulf %34, %cst : f32
            %36 = arith.addf %31, %35 : f32
            %37 = affine.load %alloc_17[%arg3, %arg2, %arg1 + 1] : memref<320x320x448xf32>
            %38 = affine.load %alloc_17[%arg3, %arg2, %arg1 - 1] : memref<320x320x448xf32>
            %39 = arith.subf %37, %38 : f32
            %40 = arith.mulf %39, %cst : f32
            %41 = arith.addf %36, %40 : f32
            affine.store %41, %alloca_20[] : memref<f32>
            %42 = affine.load %alloc_16[%arg3 + 1, %arg2, %arg1] : memref<320x320x448xf32>
            %43 = affine.load %alloc_16[%arg3 - 1, %arg2, %arg1] : memref<320x320x448xf32>
            %44 = arith.subf %42, %43 : f32
            %45 = arith.mulf %44, %cst : f32
            %46 = affine.load %alloc_17[%arg3, %arg2 + 1, %arg1] : memref<320x320x448xf32>
            %47 = affine.load %alloc_17[%arg3, %arg2 - 1, %arg1] : memref<320x320x448xf32>
            %48 = arith.subf %46, %47 : f32
            %49 = arith.mulf %48, %cst : f32
            %50 = arith.addf %45, %49 : f32
            %51 = affine.load %alloc_14[%arg3, %arg2, %arg1 + 1] : memref<320x320x448xf32>
            %52 = affine.load %alloc_14[%arg3, %arg2, %arg1 - 1] : memref<320x320x448xf32>
            %53 = arith.subf %51, %52 : f32
            %54 = arith.mulf %53, %cst : f32
            %55 = arith.addf %50, %54 : f32
            affine.store %55, %alloca_19[] : memref<f32>
            %56 = affine.load %alloca_22[] : memref<f32>
            %57 = arith.mulf %56, %cst_1 : f32
            %58 = affine.load %alloca_21[] : memref<f32>
            %59 = arith.mulf %57, %58 : f32
            %60 = affine.load %alloc[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
            %61 = arith.addf %60, %59 : f32
            affine.store %61, %alloc[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
            %62 = affine.load %alloca_22[] : memref<f32>
            %63 = arith.mulf %62, %cst_1 : f32
            %64 = affine.load %alloca_20[] : memref<f32>
            %65 = arith.mulf %63, %64 : f32
            %66 = affine.load %alloc_9[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
            %67 = arith.addf %66, %65 : f32
            affine.store %67, %alloc_9[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
            %68 = affine.load %alloca_22[] : memref<f32>
            %69 = arith.mulf %68, %cst_1 : f32
            %70 = affine.load %alloca_19[] : memref<f32>
            %71 = arith.mulf %69, %70 : f32
            %72 = affine.load %alloc_10[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
            %73 = arith.addf %72, %71 : f32
            affine.store %73, %alloc_10[%arg3, %arg2, %arg1] : memref<320x320x448xf32>
          } {arts.id = 191 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 316 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:141:5">, arts.metadata_provenance = "exact"}
        } {arts.id = 193 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 316 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:141:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 195 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 444 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:141:5">, arts.metadata_provenance = "exact"}
      func.call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    } {arts.id = 111 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:140:3">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_8, %alloca[] : memref<f64>
    affine.for %arg0 = 0 to 320 {
      %11 = affine.load %alloc[%arg0, %arg0, %arg0] : memref<320x320x448xf32>
      %12 = arith.extf %11 : f32 to f64
      %13 = math.absf %12 : f64
      %14 = affine.load %alloc_9[%arg0, %arg0, %arg0] : memref<320x320x448xf32>
      %15 = arith.extf %14 : f32 to f64
      %16 = math.absf %15 : f64
      %17 = arith.addf %13, %16 : f64
      %18 = affine.load %alloc_10[%arg0, %arg0, %arg0] : memref<320x320x448xf32>
      %19 = arith.extf %18 : f32 to f64
      %20 = math.absf %19 : f64
      %21 = arith.addf %17, %20 : f64
      %22 = affine.load %alloca[] : memref<f64>
      %23 = arith.addf %22, %21 : f64
      affine.store %23, %alloca[] : memref<f64>
    } {arts.id = 200 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "vel4sg_base.c:153:3">, arts.metadata_provenance = "exact"}
    %9 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_17 {arts.id = 53 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_16 {arts.id = 51 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_15 {arts.id = 49 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_14 {arts.id = 47 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_13 {arts.id = 45 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_12 {arts.id = 43 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_11 {arts.id = 41 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_10 {arts.id = 39 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc_9 {arts.id = 37 : i64} : memref<320x320x448xf32>
    memref.dealloc %alloc {arts.id = 35 : i64} : memref<320x320x448xf32>
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
