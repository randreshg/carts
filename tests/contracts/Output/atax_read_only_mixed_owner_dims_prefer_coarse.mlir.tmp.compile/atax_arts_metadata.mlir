module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("atax\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 4.000000e+03 : f64
    %cst_0 = arith.constant 3.1415926535897931 : f64
    %c1_i32 = arith.constant 1 : i32
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 32 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "atax.c:87:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 128000000 : i64, firstUseId = 32 : i64, lastUseId = 33 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<4000x4000xf64>
    %alloc_2 = memref.alloc() {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:89:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32000 : i64, firstUseId = 34 : i64, lastUseId = 79 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<4000xf64>
    %alloc_3 = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "atax.c:90:20", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32000 : i64, firstUseId = 35 : i64, lastUseId = 80 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<4000xf64>
    affine.for %arg2 = 0 to 4000 {
      %9 = arith.index_cast %arg2 : index to i32
      %10 = arith.sitofp %9 : i32 to f64
      affine.for %arg3 = 0 to 4000 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.addi %11, %c1_i32 : i32
        %13 = arith.sitofp %12 : i32 to f64
        %14 = arith.mulf %10, %13 : f64
        %15 = arith.divf %14, %cst : f64
        affine.store %15, %alloc[%arg2, %arg3] : memref<4000x4000xf64>
      } {arts.id = 48 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 50 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:97:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    affine.for %arg2 = 0 to 4000 {
      affine.store %cst_1, %alloc_3[%arg2] : memref<4000xf64>
      affine.for %arg3 = 0 to 4000 {
        %9 = affine.load %alloc_3[%arg2] : memref<4000xf64>
        %10 = affine.load %alloc[%arg2, %arg3] : memref<4000x4000xf64>
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.sitofp %11 : i32 to f64
        %13 = arith.mulf %12, %cst_0 : f64
        %14 = arith.mulf %10, %13 : f64
        %15 = arith.addf %9, %14 : f64
        affine.store %15, %alloc_3[%arg2] : memref<4000xf64>
      } {arts.id = 61 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 63 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 4000 {
      affine.store %cst_1, %alloc_2[%arg2] : memref<4000xf64>
      affine.for %arg3 = 0 to 4000 {
        %9 = affine.load %alloc_2[%arg2] : memref<4000xf64>
        %10 = affine.load %alloc[%arg3, %arg2] : memref<4000x4000xf64>
        %11 = affine.load %alloc_3[%arg3] : memref<4000xf64>
        %12 = arith.mulf %10, %11 : f64
        %13 = arith.addf %9, %12 : f64
        affine.store %13, %alloc_2[%arg2] : memref<4000xf64>
      } {arts.id = 72 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 74 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:103:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%4) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%4) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %7 = affine.for %arg2 = 0 to 4000 iter_args(%arg3 = %cst_1) -> (f64) {
      %9 = affine.load %alloc_2[%arg2] : memref<4000xf64>
      %10 = arith.addf %arg3, %9 : f64
      affine.yield %10 : f64
    } {arts.id = 76 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 4000 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "atax.c:111:21">, arts.metadata_provenance = "exact"}
    call @carts_bench_checksum_d(%7) : (f64) -> ()
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc_2 {arts.id = 79 : i64} : memref<4000xf64>
    memref.dealloc %alloc_3 {arts.id = 80 : i64} : memref<4000xf64>
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc {arts.id = 33 : i64} : memref<4000x4000xf64>
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
