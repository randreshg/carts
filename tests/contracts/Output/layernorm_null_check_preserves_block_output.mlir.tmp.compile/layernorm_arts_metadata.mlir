module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str4("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("allocation failure\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("layernorm\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 1.024000e+03 : f32
    %c1024 = arith.constant 1024 : index
    %c113_i32 = arith.constant 113 : i32
    %cst_0 = arith.constant 5.000000e+01 : f32
    %cst_1 = arith.constant 3.125000e-02 : f32
    %cst_2 = arith.constant 1.000000e+00 : f32
    %cst_3 = arith.constant 0.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str4 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_5 = arith.constant 9.99999974E-6 : f32
    %c1_i32 = arith.constant 1 : i32
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr
    %3 = llvm.mlir.addressof @stderr : !llvm.ptr
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %alloca = memref.alloca() {arts.id = 118 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:90:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 118 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %7 = "polygeist.undef"() : () -> f64
    affine.store %7, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %8 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%8) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %8) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 33 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "layernorm.c:65:15", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 65536 : i64, firstUseId = 33 : i64, lastUseId = 34 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<16x1024xf32>
    %alloc_6 = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "layernorm.c:66:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4096 : i64, firstUseId = 35 : i64, lastUseId = 130 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1024xf32>
    %alloc_7 = memref.alloc() {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "layernorm.c:67:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4096 : i64, firstUseId = 36 : i64, lastUseId = 131 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1024xf32>
    %10 = polygeist.memref2pointer %alloc : memref<16x1024xf32> to !llvm.ptr
    %11 = llvm.icmp "eq" %10, %4 : !llvm.ptr
    %12 = scf.if %11 -> (i1) {
      scf.yield %true : i1
    } else {
      %15 = polygeist.memref2pointer %alloc_6 : memref<1024xf32> to !llvm.ptr
      %16 = llvm.icmp "eq" %15, %4 : !llvm.ptr
      scf.yield %16 : i1
    }
    %13 = scf.if %12 -> (i1) {
      scf.yield %true : i1
    } else {
      %15 = polygeist.memref2pointer %alloc_7 : memref<1024xf32> to !llvm.ptr
      %16 = llvm.icmp "eq" %15, %4 : !llvm.ptr
      scf.yield %16 : i1
    }
    %14 = arith.extui %13 : i1 to i32
    scf.if %13 {
      %15 = llvm.load %3 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %16 = polygeist.memref2pointer %15 : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>> to !llvm.ptr
      %17 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<20 x i8>
      %18 = llvm.call @fprintf(%16, %17) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
    } else {
      %alloca_8 = memref.alloca() {arts.id = 73 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:84:5", totalAccesses = 7 : i64, readCount = 3 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 73 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 28 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
      %alloca_9 = memref.alloca() {arts.id = 74 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:84:5", totalAccesses = 8 : i64, readCount = 4 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 74 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
      %15 = "polygeist.undef"() : () -> i32
      %alloca_10 = memref.alloca() {arts.id = 53 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "layernorm.c:78:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 53 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
      affine.store %15, %alloca_10[] : memref<i32>
      affine.store %c0_i32, %alloca_10[] : memref<i32>
      affine.for %arg0 = 0 to 16 {
        scf.for %arg1 = %c0 to %c1024 step %c1 {
          %20 = affine.load %alloca_10[] : memref<i32>
          %21 = arith.remsi %20, %c113_i32 : i32
          %22 = arith.sitofp %21 : i32 to f32
          %23 = arith.subf %22, %cst_0 : f32
          %24 = arith.mulf %23, %cst_1 : f32
          memref.store %24, %alloc[%arg0, %arg1] : memref<16x1024xf32>
          %25 = affine.load %alloca_10[] : memref<i32>
          %26 = arith.addi %25, %c1_i32 : i32
          affine.store %26, %alloca_10[] : memref<i32>
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 16 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "exact"}
      affine.for %arg0 = 0 to 1024 {
        affine.store %cst_2, %alloc_6[%arg0] : memref<1024xf32>
        affine.store %cst_3, %alloc_7[%arg0] : memref<1024xf32>
      } {arts.id = 72 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "exact"}
      func.call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
      func.call @carts_kernel_timer_start(%8) : (memref<?xi8>) -> ()
      %16 = "polygeist.undef"() : () -> f32
      affine.store %16, %alloca_8[] : memref<f32>
      affine.store %16, %alloca_9[] : memref<f32>
      affine.for %arg0 = 0 to 16 {
        affine.store %cst_3, %alloca_9[] : memref<f32>
        affine.store %cst_3, %alloca_8[] : memref<f32>
        affine.for %arg1 = 0 to 1024 {
          %24 = affine.load %alloc[%arg0, %arg1] : memref<16x1024xf32>
          %25 = affine.load %alloca_9[] : memref<f32>
          %26 = arith.addf %25, %24 : f32
          affine.store %26, %alloca_9[] : memref<f32>
        } {arts.id = 85 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
        %20 = affine.load %alloca_9[] : memref<f32>
        %21 = arith.divf %20, %cst : f32
        affine.store %21, %alloca_9[] : memref<f32>
        affine.for %arg1 = 0 to 1024 {
          %24 = affine.load %alloc[%arg0, %arg1] : memref<16x1024xf32>
          %25 = affine.load %alloca_9[] : memref<f32>
          %26 = arith.subf %24, %25 : f32
          %27 = arith.mulf %26, %26 : f32
          %28 = affine.load %alloca_8[] : memref<f32>
          %29 = arith.addf %28, %27 : f32
          affine.store %29, %alloca_8[] : memref<f32>
        } {arts.id = 97 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
        %22 = affine.load %alloca_8[] : memref<f32>
        %23 = arith.divf %22, %cst : f32
        affine.store %23, %alloca_8[] : memref<f32>
        affine.for %arg1 = 0 to 1024 {
          %24 = affine.load %alloc[%arg0, %arg1] : memref<16x1024xf32>
          %25 = affine.load %alloca_9[] : memref<f32>
          %26 = arith.subf %24, %25 : f32
          %27 = affine.load %alloca_8[] : memref<f32>
          %28 = arith.addf %27, %cst_5 : f32
          %29 = math.sqrt %28 : f32
          %30 = arith.divf %cst_2, %29 : f32
          %31 = arith.mulf %26, %30 : f32
          %32 = affine.load %alloc_6[%arg1] : memref<1024xf32>
          %33 = arith.mulf %31, %32 : f32
          %34 = affine.load %alloc_7[%arg1] : memref<1024xf32>
          %35 = arith.addf %33, %34 : f32
          affine.store %35, %alloc[%arg0, %arg1] : memref<16x1024xf32>
        } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 117 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 16 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 2 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
      func.call @carts_kernel_timer_accum(%8) : (memref<?xi8>) -> ()
      func.call @carts_kernel_timer_print(%8) : (memref<?xi8>) -> ()
      %17 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%17, %8) : (memref<?xi8>, memref<?xi8>) -> ()
      affine.store %cst_4, %alloca[] : memref<f64>
      affine.for %arg0 = 0 to 16 {
        %20 = affine.load %alloc[%arg0, %arg0] : memref<16x1024xf32>
        %21 = arith.extf %20 : f32 to f64
        %22 = math.absf %21 : f64
        %23 = affine.load %alloca[] : memref<f64>
        %24 = arith.addf %23, %22 : f64
        affine.store %24, %alloca[] : memref<f64>
      } {arts.id = 122 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 16 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "layernorm.c:92:3">, arts.metadata_provenance = "exact"}
      %18 = affine.load %alloca[] : memref<f64>
      func.call @carts_bench_checksum_d(%18) : (f64) -> ()
      func.call @carts_phase_timer_stop(%17) : (memref<?xi8>) -> ()
      %19 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%19, %8) : (memref<?xi8>, memref<?xi8>) -> ()
      memref.dealloc %alloc_6 {arts.id = 130 : i64} : memref<1024xf32>
      memref.dealloc %alloc_7 {arts.id = 131 : i64} : memref<1024xf32>
      func.call @carts_phase_timer_stop(%19) : (memref<?xi8>) -> ()
      func.call @carts_e2e_timer_stop() : () -> ()
    }
    memref.dealloc %alloc {arts.id = 34 : i64} : memref<16x1024xf32>
    return %14 : i32
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
