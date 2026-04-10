module attributes {arts.runtime_config_data = "[ARTS]\0A# Two-node local config used by step-boundary contract tests.\0Aworker_threads=8\0A\0A# Keep network threads disabled for local launcher tests.\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0Alauncher=local\0Amaster_node=localhost\0Anode_count=2\0Anodes=localhost,localhost\0Adefault_ports=34739,34740\0A\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 16 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c9599 = arith.constant 9599 : index
    %cst = arith.constant 9.600000e+03 : f64
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 2.000000e+00 : f64
    %c1 = arith.constant 1 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_2 = arith.constant 9.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 83 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:62:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 83 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "seidel-2d.c:30:19", totalAccesses = 12 : i64, readCount = 10 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 737280000 : i64, firstUseId = 36 : i64, lastUseId = 37 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 96 : i64>, arts.metadata_provenance = "exact"} : memref<9600x9600xf64>
    affine.for %arg2 = 0 to 9600 {
      %10 = arith.index_cast %arg2 : index to i32
      %11 = arith.sitofp %10 : i32 to f64
      affine.for %arg3 = 0 to 9600 {
        %12 = arith.index_cast %arg3 : index to i32
        %13 = arith.addi %12, %c2_i32 : i32
        %14 = arith.sitofp %13 : i32 to f64
        %15 = arith.mulf %11, %14 : f64
        %16 = arith.addf %15, %cst_0 : f64
        %17 = arith.divf %16, %cst : f64
        affine.store %17, %alloc[%arg2, %arg3] : memref<9600x9600xf64>
      } {arts.id = 42 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    affine.for %arg2 = 0 to 320 {
      scf.for %arg3 = %c1 to %c9599 step %c1 {
        %10 = arith.index_cast %arg3 : index to i32
        %11 = arith.addi %10, %c-1_i32 : i32
        %12 = arith.index_cast %11 : i32 to index
        %13 = arith.addi %10, %c1_i32 : i32
        %14 = arith.index_cast %13 : i32 to index
        affine.for %arg4 = 1 to 9599 {
          %15 = arith.index_cast %arg4 : index to i32
          %16 = arith.addi %15, %c-1_i32 : i32
          %17 = arith.index_cast %16 : i32 to index
          %18 = memref.load %alloc[%12, %17] : memref<9600x9600xf64>
          %19 = memref.load %alloc[%12, %arg4] : memref<9600x9600xf64>
          %20 = arith.addf %18, %19 : f64
          %21 = arith.addi %15, %c1_i32 : i32
          %22 = arith.index_cast %21 : i32 to index
          %23 = memref.load %alloc[%12, %22] : memref<9600x9600xf64>
          %24 = arith.addf %20, %23 : f64
          %25 = memref.load %alloc[%arg3, %17] : memref<9600x9600xf64>
          %26 = arith.addf %24, %25 : f64
          %27 = memref.load %alloc[%arg3, %arg4] : memref<9600x9600xf64>
          %28 = arith.addf %26, %27 : f64
          %29 = memref.load %alloc[%arg3, %22] : memref<9600x9600xf64>
          %30 = arith.addf %28, %29 : f64
          %31 = memref.load %alloc[%14, %17] : memref<9600x9600xf64>
          %32 = arith.addf %30, %31 : f64
          %33 = memref.load %alloc[%14, %arg4] : memref<9600x9600xf64>
          %34 = arith.addf %32, %33 : f64
          %35 = memref.load %alloc[%14, %22] : memref<9600x9600xf64>
          %36 = arith.addf %34, %35 : f64
          %37 = arith.divf %36, %cst_2 : f64
          memref.store %37, %alloc[%arg3, %arg4] : memref<9600x9600xf64>
        } {arts.id = 55 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:49:7">, arts.metadata_provenance = "exact"}
      } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_stop(%5) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_1, %alloca[] : memref<f64>
    affine.for %arg2 = 0 to 9600 {
      %10 = affine.load %alloc[%arg2, %arg2] : memref<9600x9600xf64>
      %11 = affine.load %alloca[] : memref<f64>
      %12 = arith.addf %11, %10 : f64
      affine.store %12, %alloca[] : memref<f64>
    } {arts.id = 87 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:63:3">, arts.metadata_provenance = "exact"}
    %8 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%8) : (f64) -> ()
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc {arts.id = 37 : i64} : memref<9600x9600xf64>
    return %c0_i32 : i32
  }
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_kernel_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>}
}
