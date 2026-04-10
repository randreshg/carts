module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("convolution-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 1.024000e+03 : f32
    %c1024 = arith.constant 1024 : index
    %cst_0 = arith.constant 1.000000e-01 : f64
    %cst_1 = arith.constant 0.69999999999999996 : f64
    %cst_2 = arith.constant 4.000000e-01 : f64
    %cst_3 = arith.constant -9.000000e-01 : f64
    %cst_4 = arith.constant 6.000000e-01 : f64
    %cst_5 = arith.constant -3.000000e-01 : f64
    %cst_6 = arith.constant -8.000000e-01 : f64
    %cst_7 = arith.constant 5.000000e-01 : f64
    %cst_8 = arith.constant 2.000000e-01 : f64
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_9 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 94 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "convolution-2d.c:111:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 94 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "convolution-2d.c:82:19", totalAccesses = 10 : i64, readCount = 9 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<1024x1024xf32>
    %alloc_11 = memref.alloc() {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "convolution-2d.c:83:19", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<1024x1024xf32>
    affine.for %arg2 = 0 to 1024 {
      %10 = arith.index_cast %arg2 : index to i32
      affine.for %arg3 = 0 to 1024 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.addi %10, %11 : i32
        %13 = arith.sitofp %12 : i32 to f32
        %14 = arith.divf %13, %cst : f32
        affine.store %14, %alloc[%arg2, %arg3] : memref<1024x1024xf32>
      } {arts.id = 47 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:91:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:91:3">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 1024 {
      scf.for %arg3 = %c0 to %c1024 step %c1 {
        memref.store %cst_10, %alloc_11[%arg2, %arg3] : memref<1024x1024xf32>
      } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:95:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 50 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:94:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    affine.for %arg2 = 1 to 1023 {
      affine.for %arg3 = 1 to 1023 {
        %10 = affine.load %alloc[%arg2 - 1, %arg3 - 1] : memref<1024x1024xf32>
        %11 = arith.extf %10 : f32 to f64
        %12 = arith.mulf %11, %cst_8 : f64
        %13 = affine.load %alloc[%arg2 - 1, %arg3] : memref<1024x1024xf32>
        %14 = arith.extf %13 : f32 to f64
        %15 = arith.mulf %14, %cst_7 : f64
        %16 = arith.addf %12, %15 : f64
        %17 = affine.load %alloc[%arg2 - 1, %arg3 + 1] : memref<1024x1024xf32>
        %18 = arith.extf %17 : f32 to f64
        %19 = arith.mulf %18, %cst_6 : f64
        %20 = arith.addf %16, %19 : f64
        %21 = affine.load %alloc[%arg2, %arg3 - 1] : memref<1024x1024xf32>
        %22 = arith.extf %21 : f32 to f64
        %23 = arith.mulf %22, %cst_5 : f64
        %24 = arith.addf %20, %23 : f64
        %25 = affine.load %alloc[%arg2, %arg3] : memref<1024x1024xf32>
        %26 = arith.extf %25 : f32 to f64
        %27 = arith.mulf %26, %cst_4 : f64
        %28 = arith.addf %24, %27 : f64
        %29 = affine.load %alloc[%arg2, %arg3 + 1] : memref<1024x1024xf32>
        %30 = arith.extf %29 : f32 to f64
        %31 = arith.mulf %30, %cst_3 : f64
        %32 = arith.addf %28, %31 : f64
        %33 = affine.load %alloc[%arg2 + 1, %arg3 - 1] : memref<1024x1024xf32>
        %34 = arith.extf %33 : f32 to f64
        %35 = arith.mulf %34, %cst_2 : f64
        %36 = arith.addf %32, %35 : f64
        %37 = affine.load %alloc[%arg2 + 1, %arg3] : memref<1024x1024xf32>
        %38 = arith.extf %37 : f32 to f64
        %39 = arith.mulf %38, %cst_1 : f64
        %40 = arith.addf %36, %39 : f64
        %41 = affine.load %alloc[%arg2 + 1, %arg3 + 1] : memref<1024x1024xf32>
        %42 = arith.extf %41 : f32 to f64
        %43 = arith.mulf %42, %cst_0 : f64
        %44 = arith.addf %40, %43 : f64
        %45 = arith.truncf %44 : f64 to f32
        affine.store %45, %alloc_11[%arg2, %arg3] : memref<1024x1024xf32>
      } {arts.id = 91 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1022 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:104:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1022 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:104:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_9, %alloca[] : memref<f64>
    affine.for %arg2 = 0 to 1024 {
      %10 = affine.load %alloc_11[%arg2, %arg2] : memref<1024x1024xf32>
      %11 = arith.extf %10 : f32 to f64
      %12 = affine.load %alloca[] : memref<f64>
      %13 = arith.addf %12, %11 : f64
      affine.store %13, %alloca[] : memref<f64>
    } {arts.id = 98 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "convolution-2d.c:113:3">, arts.metadata_provenance = "exact"}
    %8 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%8) : (f64) -> ()
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_11 {arts.id = 38 : i64} : memref<1024x1024xf32>
    memref.dealloc %alloc {arts.id = 36 : i64} : memref<1024x1024xf32>
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
