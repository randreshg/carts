module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem_velocity_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 1.000000e+00 : f64
    %cst_0 = arith.constant 1.000000e-03 : f64
    %cst_1 = arith.constant 2.300000e+03 : f64
    %c11_i32 = arith.constant 11 : i32
    %cst_2 = arith.constant 2.000000e-02 : f64
    %c2_i32 = arith.constant 2 : i32
    %c17_i32 = arith.constant 17 : i32
    %c3_i32 = arith.constant 3 : i32
    %c19_i32 = arith.constant 19 : i32
    %c5_i32 = arith.constant 5 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_3 = arith.constant 1.000000e-02 : f64
    %c7_i32 = arith.constant 7 : i32
    %c13_i32 = arith.constant 13 : i32
    %c29_i32 = arith.constant 29 : i32
    %c31_i32 = arith.constant 31 : i32
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c48 = arith.constant 48 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 184 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:141:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 184 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:91:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_5 = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:92:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_6 = memref.alloc() {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:93:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_7 = memref.alloc() {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:94:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_8 = memref.alloc() {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:95:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 41 : i64, lastUseId = 42 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_9 = memref.alloc() {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:96:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 43 : i64, lastUseId = 44 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_10 = memref.alloc() {arts.id = 45 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:97:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 45 : i64, lastUseId = 46 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_11 = memref.alloc() {arts.id = 47 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:98:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 47 : i64, lastUseId = 48 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_12 = memref.alloc() {arts.id = 49 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:99:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 49 : i64, lastUseId = 50 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_13 = memref.alloc() {arts.id = 51 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:100:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 884736 : i64, firstUseId = 51 : i64, lastUseId = 52 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %7 = "polygeist.undef"() : () -> i32
    %alloca_14 = memref.alloca() {arts.id = 54 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:127:3", totalAccesses = 11 : i64, readCount = 8 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 54 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    affine.store %7, %alloca_14[] : memref<i32>
    affine.store %c0_i32, %alloca_14[] : memref<i32>
    affine.for %arg0 = 0 to 48 {
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          memref.store %cst_4, %alloc[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          memref.store %cst_4, %alloc_5[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          memref.store %cst_4, %alloc_6[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %11 = affine.load %alloca_14[] : memref<i32>
          %12 = arith.remsi %11, %c11_i32 : i32
          %13 = arith.sitofp %12 : i32 to f64
          %14 = arith.addf %13, %cst_1 : f64
          memref.store %14, %alloc_7[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %15 = affine.load %alloca_14[] : memref<i32>
          %16 = arith.muli %15, %c2_i32 : i32
          %17 = arith.remsi %16, %c17_i32 : i32
          %18 = arith.sitofp %17 : i32 to f64
          %19 = arith.mulf %18, %cst_2 : f64
          memref.store %19, %alloc_8[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %20 = affine.load %alloca_14[] : memref<i32>
          %21 = arith.muli %20, %c3_i32 : i32
          %22 = arith.remsi %21, %c19_i32 : i32
          %23 = arith.sitofp %22 : i32 to f64
          %24 = arith.mulf %23, %cst_2 : f64
          memref.store %24, %alloc_9[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %25 = affine.load %alloca_14[] : memref<i32>
          %26 = arith.muli %25, %c5_i32 : i32
          %27 = arith.remsi %26, %c23_i32 : i32
          %28 = arith.sitofp %27 : i32 to f64
          %29 = arith.mulf %28, %cst_2 : f64
          memref.store %29, %alloc_10[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %30 = affine.load %alloca_14[] : memref<i32>
          %31 = arith.muli %30, %c7_i32 : i32
          %32 = arith.remsi %31, %c13_i32 : i32
          %33 = arith.sitofp %32 : i32 to f64
          %34 = arith.mulf %33, %cst_3 : f64
          memref.store %34, %alloc_11[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %35 = affine.load %alloca_14[] : memref<i32>
          %36 = arith.muli %35, %c11_i32 : i32
          %37 = arith.remsi %36, %c29_i32 : i32
          %38 = arith.sitofp %37 : i32 to f64
          %39 = arith.mulf %38, %cst_3 : f64
          memref.store %39, %alloc_12[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %40 = affine.load %alloca_14[] : memref<i32>
          %41 = arith.muli %40, %c13_i32 : i32
          %42 = arith.remsi %41, %c31_i32 : i32
          %43 = arith.sitofp %42 : i32 to f64
          %44 = arith.mulf %43, %cst_3 : f64
          memref.store %44, %alloc_13[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %45 = affine.load %alloca_14[] : memref<i32>
          %46 = arith.addi %45, %c1_i32 : i32
          affine.store %46, %alloca_14[] : memref<i32>
        } {arts.id = 105 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 107 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_15 = memref.alloca() {arts.id = 110 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 110 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_16 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_17 = memref.alloca() {arts.id = 112 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 112 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %alloca_18 = memref.alloca() {arts.id = 113 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 113 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_15[] : memref<f64>
    affine.store %4, %alloca_16[] : memref<f64>
    affine.store %4, %alloca_17[] : memref<f64>
    affine.store %4, %alloca_18[] : memref<f64>
    affine.for %arg0 = 1 to 47 {
      affine.for %arg1 = 1 to 47 {
        affine.for %arg2 = 1 to 47 {
          %11 = affine.load %alloc_7[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %12 = arith.divf %cst, %11 : f64
          affine.store %12, %alloca_18[] : memref<f64>
          %13 = affine.load %alloc_8[%arg2 + 1, %arg1, %arg0] : memref<48x48x48xf64>
          %14 = affine.load %alloc_8[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %15 = arith.subf %13, %14 : f64
          %16 = affine.load %alloc_11[%arg2, %arg1 + 1, %arg0] : memref<48x48x48xf64>
          %17 = affine.load %alloc_11[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %18 = arith.subf %16, %17 : f64
          %19 = arith.addf %15, %18 : f64
          %20 = affine.load %alloc_12[%arg2, %arg1, %arg0 + 1] : memref<48x48x48xf64>
          %21 = affine.load %alloc_12[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %22 = arith.subf %20, %21 : f64
          %23 = arith.addf %19, %22 : f64
          affine.store %23, %alloca_17[] : memref<f64>
          %24 = affine.load %alloc_11[%arg2 + 1, %arg1, %arg0] : memref<48x48x48xf64>
          %25 = affine.load %alloc_11[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %26 = arith.subf %24, %25 : f64
          %27 = affine.load %alloc_9[%arg2, %arg1 + 1, %arg0] : memref<48x48x48xf64>
          %28 = affine.load %alloc_9[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %29 = arith.subf %27, %28 : f64
          %30 = arith.addf %26, %29 : f64
          %31 = affine.load %alloc_13[%arg2, %arg1, %arg0 + 1] : memref<48x48x48xf64>
          %32 = affine.load %alloc_13[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %33 = arith.subf %31, %32 : f64
          %34 = arith.addf %30, %33 : f64
          affine.store %34, %alloca_16[] : memref<f64>
          %35 = affine.load %alloc_12[%arg2 + 1, %arg1, %arg0] : memref<48x48x48xf64>
          %36 = affine.load %alloc_12[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %37 = arith.subf %35, %36 : f64
          %38 = affine.load %alloc_13[%arg2, %arg1 + 1, %arg0] : memref<48x48x48xf64>
          %39 = affine.load %alloc_13[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %40 = arith.subf %38, %39 : f64
          %41 = arith.addf %37, %40 : f64
          %42 = affine.load %alloc_10[%arg2, %arg1, %arg0 + 1] : memref<48x48x48xf64>
          %43 = affine.load %alloc_10[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %44 = arith.subf %42, %43 : f64
          %45 = arith.addf %41, %44 : f64
          affine.store %45, %alloca_15[] : memref<f64>
          %46 = affine.load %alloca_18[] : memref<f64>
          %47 = arith.mulf %46, %cst_0 : f64
          %48 = affine.load %alloca_17[] : memref<f64>
          %49 = arith.mulf %47, %48 : f64
          %50 = affine.load %alloc[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %51 = arith.addf %50, %49 : f64
          affine.store %51, %alloc[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %52 = affine.load %alloca_18[] : memref<f64>
          %53 = arith.mulf %52, %cst_0 : f64
          %54 = affine.load %alloca_16[] : memref<f64>
          %55 = arith.mulf %53, %54 : f64
          %56 = affine.load %alloc_5[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %57 = arith.addf %56, %55 : f64
          affine.store %57, %alloc_5[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %58 = affine.load %alloca_18[] : memref<f64>
          %59 = arith.mulf %58, %cst_0 : f64
          %60 = affine.load %alloca_15[] : memref<f64>
          %61 = arith.mulf %59, %60 : f64
          %62 = affine.load %alloc_6[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          %63 = arith.addf %62, %61 : f64
          affine.store %63, %alloc_6[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
        } {arts.id = 179 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 181 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 183 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_4, %alloca[] : memref<f64>
    affine.for %arg0 = 0 to 48 {
      %11 = affine.load %alloc[%arg0, %arg0, %arg0] : memref<48x48x48xf64>
      %12 = affine.load %alloc_5[%arg0, %arg0, %arg0] : memref<48x48x48xf64>
      %13 = arith.addf %11, %12 : f64
      %14 = affine.load %alloc_6[%arg0, %arg0, %arg0] : memref<48x48x48xf64>
      %15 = arith.addf %13, %14 : f64
      %16 = affine.load %alloca[] : memref<f64>
      %17 = arith.addf %16, %15 : f64
      affine.store %17, %alloca[] : memref<f64>
    } {arts.id = 188 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:145:3">, arts.metadata_provenance = "exact"}
    %9 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_13 {arts.id = 52 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_12 {arts.id = 50 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_11 {arts.id = 48 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_10 {arts.id = 46 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_9 {arts.id = 44 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_8 {arts.id = 42 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_7 {arts.id = 40 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_6 {arts.id = 38 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_5 {arts.id = 36 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc {arts.id = 34 : i64} : memref<48x48x48xf64>
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
