module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem3d_update_stress\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 5.000000e-01 : f64
    %cst_0 = arith.constant 2.000000e+00 : f64
    %cst_1 = arith.constant 1.000000e-03 : f64
    %c17_i32 = arith.constant 17 : i32
    %cst_2 = arith.constant 1.500000e-03 : f64
    %c3_i32 = arith.constant 3 : i32
    %c19_i32 = arith.constant 19 : i32
    %cst_3 = arith.constant 8.000000e-04 : f64
    %c5_i32 = arith.constant 5 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_4 = arith.constant 2.500000e+03 : f64
    %c7_i32 = arith.constant 7 : i32
    %cst_5 = arith.constant 3.000000e+01 : f64
    %cst_6 = arith.constant 5.000000e-02 : f64
    %c11_i32 = arith.constant 11 : i32
    %cst_7 = arith.constant 2.000000e+01 : f64
    %cst_8 = arith.constant 4.000000e-02 : f64
    %c13_i32 = arith.constant 13 : i32
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c40 = arith.constant 40 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_9 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 236 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:153:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 236 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:97:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 56 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_10 = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:98:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 56 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_11 = memref.alloc() {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:99:18", totalAccesses = 7 : i64, readCount = 6 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 56 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_12 = memref.alloc() {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:100:19", totalAccesses = 1 : i64, readCount = 0 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 2], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_13 = memref.alloc() {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:101:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 41 : i64, lastUseId = 42 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_14 = memref.alloc() {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:102:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 43 : i64, lastUseId = 44 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_15 = memref.alloc() {arts.id = 45 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:103:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 45 : i64, lastUseId = 46 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 9 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_16 = memref.alloc() {arts.id = 47 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:104:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 47 : i64, lastUseId = 48 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 9 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_17 = memref.alloc() {arts.id = 49 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:105:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 49 : i64, lastUseId = 50 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_18 = memref.alloc() {arts.id = 51 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:106:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 51 : i64, lastUseId = 52 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_19 = memref.alloc() {arts.id = 53 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:107:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 53 : i64, lastUseId = 54 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %alloc_20 = memref.alloc() {arts.id = 55 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "stress_update.c:108:19", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 512000 : i64, firstUseId = 55 : i64, lastUseId = 56 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<40x40x40xf64>
    %7 = "polygeist.undef"() : () -> i32
    %alloca_21 = memref.alloca() {arts.id = 58 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:139:3", totalAccesses = 10 : i64, readCount = 7 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 58 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    affine.store %7, %alloca_21[] : memref<i32>
    affine.store %c0_i32, %alloca_21[] : memref<i32>
    affine.for %arg0 = 0 to 40 {
      scf.for %arg1 = %c0 to %c40 step %c1 {
        scf.for %arg2 = %c0 to %c40 step %c1 {
          %11 = affine.load %alloca_21[] : memref<i32>
          %12 = arith.remsi %11, %c17_i32 : i32
          %13 = arith.sitofp %12 : i32 to f64
          %14 = arith.mulf %13, %cst_1 : f64
          memref.store %14, %alloc[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          %15 = affine.load %alloca_21[] : memref<i32>
          %16 = arith.muli %15, %c3_i32 : i32
          %17 = arith.remsi %16, %c19_i32 : i32
          %18 = arith.sitofp %17 : i32 to f64
          %19 = arith.mulf %18, %cst_2 : f64
          memref.store %19, %alloc_10[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          %20 = affine.load %alloca_21[] : memref<i32>
          %21 = arith.muli %20, %c5_i32 : i32
          %22 = arith.remsi %21, %c23_i32 : i32
          %23 = arith.sitofp %22 : i32 to f64
          %24 = arith.mulf %23, %cst_3 : f64
          memref.store %24, %alloc_11[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          %25 = affine.load %alloca_21[] : memref<i32>
          %26 = arith.remsi %25, %c7_i32 : i32
          %27 = arith.sitofp %26 : i32 to f64
          %28 = arith.addf %27, %cst_4 : f64
          memref.store %28, %alloc_12[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          %29 = affine.load %alloca_21[] : memref<i32>
          %30 = arith.remsi %29, %c11_i32 : i32
          %31 = arith.sitofp %30 : i32 to f64
          %32 = arith.mulf %31, %cst_6 : f64
          %33 = arith.addf %32, %cst_5 : f64
          memref.store %33, %alloc_13[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          %34 = affine.load %alloca_21[] : memref<i32>
          %35 = arith.remsi %34, %c13_i32 : i32
          %36 = arith.sitofp %35 : i32 to f64
          %37 = arith.mulf %36, %cst_8 : f64
          %38 = arith.addf %37, %cst_7 : f64
          memref.store %38, %alloc_14[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          memref.store %cst_9, %alloc_20[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          memref.store %cst_9, %alloc_19[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          memref.store %cst_9, %alloc_18[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          memref.store %cst_9, %alloc_17[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          memref.store %cst_9, %alloc_16[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          memref.store %cst_9, %alloc_15[%arg0, %arg1, %arg2] : memref<40x40x40xf64>
          %39 = affine.load %alloca_21[] : memref<i32>
          %40 = arith.addi %39, %c1_i32 : i32
          affine.store %40, %alloca_21[] : memref<i32>
        } {arts.id = 105 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 107 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:139:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_22 = memref.alloca() {arts.id = 110 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 110 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_23 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_24 = memref.alloca() {arts.id = 112 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 112 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_25 = memref.alloca() {arts.id = 113 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 113 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_26 = memref.alloca() {arts.id = 114 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 114 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_27 = memref.alloca() {arts.id = 115 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 115 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_28 = memref.alloca() {arts.id = 116 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "stress_update.c:145:5", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 116 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 48 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_22[] : memref<f64>
    affine.store %4, %alloca_23[] : memref<f64>
    affine.store %4, %alloca_24[] : memref<f64>
    affine.store %4, %alloca_25[] : memref<f64>
    affine.store %4, %alloca_26[] : memref<f64>
    affine.store %4, %alloca_27[] : memref<f64>
    affine.store %4, %alloca_28[] : memref<f64>
    affine.for %arg0 = 2 to 38 {
      affine.for %arg1 = 2 to 38 {
        affine.for %arg2 = 2 to 38 {
          %11 = affine.load %alloc_13[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          affine.store %11, %alloca_28[] : memref<f64>
          %12 = affine.load %alloc_14[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          affine.store %12, %alloca_27[] : memref<f64>
          %13 = affine.load %alloc[%arg2 + 1, %arg1, %arg0] : memref<40x40x40xf64>
          %14 = affine.load %alloc[%arg2 - 1, %arg1, %arg0] : memref<40x40x40xf64>
          %15 = arith.subf %13, %14 : f64
          %16 = arith.mulf %15, %cst : f64
          affine.store %16, %alloca_26[] : memref<f64>
          %17 = affine.load %alloc_10[%arg2, %arg1 + 1, %arg0] : memref<40x40x40xf64>
          %18 = affine.load %alloc_10[%arg2, %arg1 - 1, %arg0] : memref<40x40x40xf64>
          %19 = arith.subf %17, %18 : f64
          %20 = arith.mulf %19, %cst : f64
          affine.store %20, %alloca_25[] : memref<f64>
          %21 = affine.load %alloc_11[%arg2, %arg1, %arg0 + 1] : memref<40x40x40xf64>
          %22 = affine.load %alloc_11[%arg2, %arg1, %arg0 - 1] : memref<40x40x40xf64>
          %23 = arith.subf %21, %22 : f64
          %24 = arith.mulf %23, %cst : f64
          affine.store %24, %alloca_24[] : memref<f64>
          %25 = affine.load %alloca_26[] : memref<f64>
          %26 = affine.load %alloca_25[] : memref<f64>
          %27 = arith.addf %25, %26 : f64
          %28 = affine.load %alloca_24[] : memref<f64>
          %29 = arith.addf %27, %28 : f64
          affine.store %29, %alloca_23[] : memref<f64>
          %30 = affine.load %alloca_28[] : memref<f64>
          %31 = arith.mulf %30, %cst_0 : f64
          affine.store %31, %alloca_22[] : memref<f64>
          %32 = affine.load %alloca_22[] : memref<f64>
          %33 = affine.load %alloca_26[] : memref<f64>
          %34 = arith.mulf %32, %33 : f64
          %35 = affine.load %alloca_27[] : memref<f64>
          %36 = affine.load %alloca_23[] : memref<f64>
          %37 = arith.mulf %35, %36 : f64
          %38 = arith.addf %34, %37 : f64
          %39 = arith.mulf %38, %cst_1 : f64
          %40 = affine.load %alloc_15[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %41 = arith.addf %40, %39 : f64
          affine.store %41, %alloc_15[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %42 = affine.load %alloca_22[] : memref<f64>
          %43 = affine.load %alloca_25[] : memref<f64>
          %44 = arith.mulf %42, %43 : f64
          %45 = affine.load %alloca_27[] : memref<f64>
          %46 = affine.load %alloca_23[] : memref<f64>
          %47 = arith.mulf %45, %46 : f64
          %48 = arith.addf %44, %47 : f64
          %49 = arith.mulf %48, %cst_1 : f64
          %50 = affine.load %alloc_16[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %51 = arith.addf %50, %49 : f64
          affine.store %51, %alloc_16[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %52 = affine.load %alloca_22[] : memref<f64>
          %53 = affine.load %alloca_24[] : memref<f64>
          %54 = arith.mulf %52, %53 : f64
          %55 = affine.load %alloca_27[] : memref<f64>
          %56 = affine.load %alloca_23[] : memref<f64>
          %57 = arith.mulf %55, %56 : f64
          %58 = arith.addf %54, %57 : f64
          %59 = arith.mulf %58, %cst_1 : f64
          %60 = affine.load %alloc_17[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %61 = arith.addf %60, %59 : f64
          affine.store %61, %alloc_17[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %62 = affine.load %alloca_28[] : memref<f64>
          %63 = arith.mulf %62, %cst_1 : f64
          %64 = affine.load %alloc[%arg2, %arg1 + 1, %arg0] : memref<40x40x40xf64>
          %65 = affine.load %alloc[%arg2, %arg1 - 1, %arg0] : memref<40x40x40xf64>
          %66 = arith.subf %64, %65 : f64
          %67 = arith.mulf %66, %cst : f64
          %68 = affine.load %alloc_10[%arg2 + 1, %arg1, %arg0] : memref<40x40x40xf64>
          %69 = affine.load %alloc_10[%arg2 - 1, %arg1, %arg0] : memref<40x40x40xf64>
          %70 = arith.subf %68, %69 : f64
          %71 = arith.mulf %70, %cst : f64
          %72 = arith.addf %67, %71 : f64
          %73 = arith.mulf %63, %72 : f64
          %74 = affine.load %alloc_18[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %75 = arith.addf %74, %73 : f64
          affine.store %75, %alloc_18[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %76 = affine.load %alloca_28[] : memref<f64>
          %77 = arith.mulf %76, %cst_1 : f64
          %78 = affine.load %alloc[%arg2, %arg1, %arg0 + 1] : memref<40x40x40xf64>
          %79 = affine.load %alloc[%arg2, %arg1, %arg0 - 1] : memref<40x40x40xf64>
          %80 = arith.subf %78, %79 : f64
          %81 = arith.mulf %80, %cst : f64
          %82 = affine.load %alloc_11[%arg2 + 1, %arg1, %arg0] : memref<40x40x40xf64>
          %83 = affine.load %alloc_11[%arg2 - 1, %arg1, %arg0] : memref<40x40x40xf64>
          %84 = arith.subf %82, %83 : f64
          %85 = arith.mulf %84, %cst : f64
          %86 = arith.addf %81, %85 : f64
          %87 = arith.mulf %77, %86 : f64
          %88 = affine.load %alloc_19[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %89 = arith.addf %88, %87 : f64
          affine.store %89, %alloc_19[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %90 = affine.load %alloca_28[] : memref<f64>
          %91 = arith.mulf %90, %cst_1 : f64
          %92 = affine.load %alloc_10[%arg2, %arg1, %arg0 + 1] : memref<40x40x40xf64>
          %93 = affine.load %alloc_10[%arg2, %arg1, %arg0 - 1] : memref<40x40x40xf64>
          %94 = arith.subf %92, %93 : f64
          %95 = arith.mulf %94, %cst : f64
          %96 = affine.load %alloc_11[%arg2, %arg1 + 1, %arg0] : memref<40x40x40xf64>
          %97 = affine.load %alloc_11[%arg2, %arg1 - 1, %arg0] : memref<40x40x40xf64>
          %98 = arith.subf %96, %97 : f64
          %99 = arith.mulf %98, %cst : f64
          %100 = arith.addf %95, %99 : f64
          %101 = arith.mulf %91, %100 : f64
          %102 = affine.load %alloc_20[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
          %103 = arith.addf %102, %101 : f64
          affine.store %103, %alloc_20[%arg2, %arg1, %arg0] : memref<40x40x40xf64>
        } {arts.id = 231 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 36 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:145:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 233 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 36 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:145:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 235 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 36 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:145:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_9, %alloca[] : memref<f64>
    affine.for %arg0 = 0 to 40 {
      %11 = affine.load %alloc_15[%arg0, %arg0, %arg0] : memref<40x40x40xf64>
      %12 = affine.load %alloc_16[%arg0, %arg0, %arg0] : memref<40x40x40xf64>
      %13 = arith.addf %11, %12 : f64
      %14 = affine.load %alloc_17[%arg0, %arg0, %arg0] : memref<40x40x40xf64>
      %15 = arith.addf %13, %14 : f64
      %16 = affine.load %alloc_18[%arg0, %arg0, %arg0] : memref<40x40x40xf64>
      %17 = arith.addf %15, %16 : f64
      %18 = affine.load %alloc_19[%arg0, %arg0, %arg0] : memref<40x40x40xf64>
      %19 = arith.addf %17, %18 : f64
      %20 = affine.load %alloc_20[%arg0, %arg0, %arg0] : memref<40x40x40xf64>
      %21 = arith.addf %19, %20 : f64
      %22 = affine.load %alloca[] : memref<f64>
      %23 = arith.addf %22, %21 : f64
      affine.store %23, %alloca[] : memref<f64>
    } {arts.id = 240 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 40 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stress_update.c:157:3">, arts.metadata_provenance = "exact"}
    %9 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_20 {arts.id = 56 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_19 {arts.id = 54 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_18 {arts.id = 52 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_17 {arts.id = 50 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_16 {arts.id = 48 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_15 {arts.id = 46 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_14 {arts.id = 44 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_13 {arts.id = 42 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_12 {arts.id = 40 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_11 {arts.id = 38 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc_10 {arts.id = 36 : i64} : memref<40x40x40xf64>
    memref.dealloc %alloc {arts.id = 34 : i64} : memref<40x40x40xf64>
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
