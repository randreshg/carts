module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("batchnorm\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 2.44200259E-4 : f32
    %cst_0 = arith.constant 2.44140625E-4 : f32
    %cst_1 = arith.constant 2.621440e+05 : f32
    %c1024 = arith.constant 1024 : index
    %c64 = arith.constant 64 : index
    %cst_2 = arith.constant 9.99999974E-6 : f32
    %cst_3 = arith.constant 0.000000e+00 : f32
    %c1_i32 = arith.constant 1 : i32
    %cst_4 = arith.constant 1.000000e+00 : f32
    %cst_5 = arith.constant 2.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_6 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 131 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "batchnorm.c:288:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 131 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "batchnorm.c:256:16", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 1048576 : i64, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 2], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<4x64x1024xf32>
    %alloc_7 = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "batchnorm.c:257:21", totalAccesses = 10 : i64, readCount = 6 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 1048576 : i64, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<4x64x1024xf32>
    %alloc_8 = memref.alloc() {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "batchnorm.c:260:17", totalAccesses = 7 : i64, readCount = 4 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 256 : i64, firstUseId = 37 : i64, lastUseId = 143 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 28 : i64>, arts.metadata_provenance = "exact"} : memref<64xf32>
    %alloc_9 = memref.alloc() {arts.id = 38 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "batchnorm.c:261:21", totalAccesses = 6 : i64, readCount = 3 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 256 : i64, firstUseId = 38 : i64, lastUseId = 144 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<64xf32>
    %alloca_10 = memref.alloca() {arts.id = 40 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "batchnorm.c:273:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 40 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    %7 = "polygeist.undef"() : () -> i32
    affine.store %7, %alloca_10[] : memref<i32>
    affine.store %c0_i32, %alloca_10[] : memref<i32>
    affine.for %arg2 = 0 to 4 {
      scf.for %arg3 = %c0 to %c64 step %c1 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %11 = affine.load %alloca_10[] : memref<i32>
          %12 = arith.sitofp %11 : i32 to f32
          %13 = arith.divf %12, %cst_1 : f32
          %14 = arith.mulf %13, %cst_5 : f32
          %15 = arith.subf %14, %cst_4 : f32
          memref.store %15, %alloc[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
          %16 = affine.load %alloca_10[] : memref<i32>
          %17 = arith.addi %16, %c1_i32 : i32
          affine.store %17, %alloca_10[] : memref<i32>
        } {arts.id = 54 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:273:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 56 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:273:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 58 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:273:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    affine.for %arg2 = 0 to 4 {
      scf.for %arg3 = %c0 to %c64 step %c1 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %11 = memref.load %alloc[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
          memref.store %11, %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
        } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 64 {
      affine.store %cst_3, %alloc_8[%arg2] : memref<64xf32>
      affine.for %arg3 = 0 to 4 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %13 = memref.load %alloc_7[%arg3, %arg2, %arg4] : memref<4x64x1024xf32>
          %14 = affine.load %alloc_8[%arg2] : memref<64xf32>
          %15 = arith.addf %14, %13 : f32
          affine.store %15, %alloc_8[%arg2] : memref<64xf32>
        } {arts.id = 75 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 77 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      %11 = affine.load %alloc_8[%arg2] : memref<64xf32>
      %12 = arith.mulf %11, %cst_0 : f32
      affine.store %12, %alloc_8[%arg2] : memref<64xf32>
    } {arts.id = 82 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 64 {
      affine.store %cst_3, %alloc_9[%arg2] : memref<64xf32>
      affine.for %arg3 = 0 to 4 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %13 = memref.load %alloc_7[%arg3, %arg2, %arg4] : memref<4x64x1024xf32>
          %14 = affine.load %alloc_8[%arg2] : memref<64xf32>
          %15 = arith.subf %13, %14 : f32
          %16 = arith.mulf %15, %15 : f32
          %17 = affine.load %alloc_9[%arg2] : memref<64xf32>
          %18 = arith.addf %17, %16 : f32
          affine.store %18, %alloc_9[%arg2] : memref<64xf32>
        } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 94 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      %11 = affine.load %alloc_9[%arg2] : memref<64xf32>
      %12 = arith.mulf %11, %cst : f32
      affine.store %12, %alloc_9[%arg2] : memref<64xf32>
    } {arts.id = 99 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 4 {
      scf.for %arg3 = %c0 to %c64 step %c1 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %11 = memref.load %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
          %12 = memref.load %alloc_8[%arg3] : memref<64xf32>
          %13 = arith.subf %11, %12 : f32
          %14 = memref.load %alloc_9[%arg3] : memref<64xf32>
          %15 = arith.addf %14, %cst_2 : f32
          %16 = math.sqrt %15 : f32
          %17 = arith.divf %13, %16 : f32
          memref.store %17, %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
        } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 111 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 113 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 4 {
      scf.for %arg3 = %c0 to %c64 step %c1 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %11 = memref.load %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
          memref.store %11, %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
        } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 119 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 121 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 4 {
      scf.for %arg3 = %c0 to %c64 step %c1 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %11 = memref.load %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
          %12 = arith.addf %11, %cst_3 : f32
          memref.store %12, %alloc_7[%arg2, %arg3, %arg4] : memref<4x64x1024xf32>
        } {arts.id = 126 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 128 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 130 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_6, %alloca[] : memref<f64>
    affine.for %arg2 = 0 to 4 {
      %11 = affine.load %alloc_7[%arg2, %arg2, %arg2] : memref<4x64x1024xf32>
      %12 = arith.extf %11 : f32 to f64
      %13 = math.absf %12 : f64
      %14 = affine.load %alloca[] : memref<f64>
      %15 = arith.addf %14, %13 : f64
      affine.store %15, %alloca[] : memref<f64>
    } {arts.id = 135 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:292:3">, arts.metadata_provenance = "exact"}
    %9 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc_8 {arts.id = 143 : i64} : memref<64xf32>
    memref.dealloc %alloc_9 {arts.id = 144 : i64} : memref<64xf32>
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_7 {arts.id = 36 : i64} : memref<4x64x1024xf32>
    memref.dealloc %alloc {arts.id = 34 : i64} : memref<4x64x1024xf32>
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
