module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("2mm\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 2.560000e+02 : f64
    %c1_i32 = arith.constant 1 : i32
    %c3_i32 = arith.constant 3 : i32
    %c2_i32 = arith.constant 2 : i32
    %cst_0 = arith.constant 3.241200e+04 : f64
    %cst_1 = arith.constant 2.123000e+03 : f64
    %c1 = arith.constant 1 : index
    %c256 = arith.constant 256 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_2 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    call @carts_benchmarks_start() : () -> ()
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%4) : (memref<?xi8>) -> ()
    %5 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%5, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 30 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "2mm.c:113:21", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 524288 : i64, firstUseId = 30 : i64, lastUseId = 31 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 2], estimatedAccessBytes = 32 : i64>} : memref<256x256xf64>
    %alloc_3 = memref.alloc() {arts.id = 32 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "2mm.c:114:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 524288 : i64, firstUseId = 32 : i64, lastUseId = 33 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 16 : i64>} : memref<256x256xf64>
    %alloc_4 = memref.alloc() {arts.id = 34 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "2mm.c:115:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 524288 : i64, firstUseId = 34 : i64, lastUseId = 35 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 16 : i64>} : memref<256x256xf64>
    %alloc_5 = memref.alloc() {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "2mm.c:116:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 524288 : i64, firstUseId = 36 : i64, lastUseId = 37 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 16 : i64>} : memref<256x256xf64>
    %alloc_6 = memref.alloc() {arts.id = 38 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "2mm.c:117:19", totalAccesses = 6 : i64, readCount = 3 : i64, writeCount = 3 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 524288 : i64, firstUseId = 38 : i64, lastUseId = 39 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 48 : i64>} : memref<256x256xf64>
    scf.for %arg2 = %c0 to %c256 step %c1 {
      %9 = arith.index_cast %arg2 : index to i32
      %10 = arith.sitofp %9 : i32 to f64
      scf.for %arg3 = %c0 to %c256 step %c1 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.sitofp %11 : i32 to f64
        %13 = arith.mulf %10, %12 : f64
        %14 = arith.divf %13, %cst : f64
        memref.store %14, %alloc_3[%arg2, %arg3] : memref<256x256xf64>
      } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    } {arts.id = 55 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    scf.for %arg2 = %c0 to %c256 step %c1 {
      %9 = arith.index_cast %arg2 : index to i32
      %10 = arith.sitofp %9 : i32 to f64
      scf.for %arg3 = %c0 to %c256 step %c1 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.addi %11, %c1_i32 : i32
        %13 = arith.sitofp %12 : i32 to f64
        %14 = arith.mulf %10, %13 : f64
        %15 = arith.divf %14, %cst : f64
        memref.store %15, %alloc_4[%arg2, %arg3] : memref<256x256xf64>
      } {arts.id = 65 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    } {arts.id = 67 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    scf.for %arg2 = %c0 to %c256 step %c1 {
      %9 = arith.index_cast %arg2 : index to i32
      %10 = arith.sitofp %9 : i32 to f64
      scf.for %arg3 = %c0 to %c256 step %c1 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.addi %11, %c3_i32 : i32
        %13 = arith.sitofp %12 : i32 to f64
        %14 = arith.mulf %10, %13 : f64
        %15 = arith.divf %14, %cst : f64
        memref.store %15, %alloc_5[%arg2, %arg3] : memref<256x256xf64>
      } {arts.id = 77 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    } {arts.id = 79 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    scf.for %arg2 = %c0 to %c256 step %c1 {
      %9 = arith.index_cast %arg2 : index to i32
      %10 = arith.sitofp %9 : i32 to f64
      scf.for %arg3 = %c0 to %c256 step %c1 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.addi %11, %c2_i32 : i32
        %13 = arith.sitofp %12 : i32 to f64
        %14 = arith.mulf %10, %13 : f64
        %15 = arith.divf %14, %cst : f64
        memref.store %15, %alloc_6[%arg2, %arg3] : memref<256x256xf64>
      } {arts.id = 89 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    } {arts.id = 91 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:132:3">}
    call @carts_phase_timer_stop(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%4) : (memref<?xi8>) -> ()
    arts.edt <parallel> <internode> route(%c-1_i32) attributes {depPattern = #arts.dep_pattern<matmul>, no_verify = #arts.no_verify} {
      arts.for(%c0) to(%c256) step(%c1) {{
      ^bb0(%arg2: index):
        %c64 = arith.constant 64 : index
        scf.for %arg3 = %c0 to %c256 step %c64 {
          %9 = arith.addi %arg3, %c64 : index
          %10 = arith.minui %9, %c256 : index
          scf.for %arg4 = %arg3 to %10 step %c1 {
            memref.store %cst_2, %alloc[%arg2, %arg4] : memref<256x256xf64>
          } {arts.id = 103 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:137:3">}
        }
        scf.for %arg3 = %c0 to %c256 step %c1 {
          scf.for %arg4 = %c0 to %c256 step %c64 {
            %9 = arith.addi %arg4, %c64 : index
            %10 = arith.minui %9, %c256 : index
            scf.for %arg5 = %arg4 to %10 step %c1 {
              %11 = memref.load %alloc_3[%arg2, %arg3] : memref<256x256xf64>
              %12 = arith.mulf %11, %cst_0 : f64
              %13 = memref.load %alloc_4[%arg3, %arg5] : memref<256x256xf64>
              %14 = arith.mulf %12, %13 : f64
              %15 = memref.load %alloc[%arg2, %arg5] : memref<256x256xf64>
              %16 = arith.addf %15, %14 : f64
              memref.store %16, %alloc[%arg2, %arg5] : memref<256x256xf64>
            } {arts.id = 103 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:137:3">}
          }
        } {arts.id = 101 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 256 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "2mm.c:137:3">}
      }} {arts.id = 105 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:137:3">, depPattern = #arts.dep_pattern<matmul>}
      arts.barrier
      arts.for(%c0) to(%c256) step(%c1) {{
      ^bb0(%arg2: index):
        %c64 = arith.constant 64 : index
        scf.for %arg3 = %c0 to %c256 step %c64 {
          %9 = arith.addi %arg3, %c64 : index
          %10 = arith.minui %9, %c256 : index
          scf.for %arg4 = %arg3 to %10 step %c1 {
            %11 = memref.load %alloc_6[%arg2, %arg4] : memref<256x256xf64>
            %12 = arith.mulf %11, %cst_1 : f64
            memref.store %12, %alloc_6[%arg2, %arg4] : memref<256x256xf64>
          } {arts.id = 118 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:137:3">}
        }
        scf.for %arg3 = %c0 to %c256 step %c1 {
          scf.for %arg4 = %c0 to %c256 step %c64 {
            %9 = arith.addi %arg4, %c64 : index
            %10 = arith.minui %9, %c256 : index
            scf.for %arg5 = %arg4 to %10 step %c1 {
              %11 = memref.load %alloc[%arg2, %arg3] : memref<256x256xf64>
              %12 = memref.load %alloc_5[%arg3, %arg5] : memref<256x256xf64>
              %13 = arith.mulf %11, %12 : f64
              %14 = memref.load %alloc_6[%arg2, %arg5] : memref<256x256xf64>
              %15 = arith.addf %14, %13 : f64
              memref.store %15, %alloc_6[%arg2, %arg5] : memref<256x256xf64>
            } {arts.id = 118 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:137:3">}
          }
        } {arts.id = 116 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 256 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "2mm.c:137:3">}
      }} {arts.id = 120 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 256 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "2mm.c:137:3">, depPattern = #arts.dep_pattern<matmul>}
    }
    call @carts_kernel_timer_stop(%4) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    %7 = scf.for %arg2 = %c0 to %c256 step %c1 iter_args(%arg3 = %cst_2) -> (f64) {
      %9 = memref.load %alloc_6[%arg2, %arg2] : memref<256x256xf64>
      %10 = arith.addf %arg3, %9 : f64
      scf.yield %10 : f64
    }
    call @carts_bench_checksum_d(%7) : (f64) -> ()
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %4) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_6 {arts.id = 39 : i64} : memref<256x256xf64>
    memref.dealloc %alloc_5 {arts.id = 37 : i64} : memref<256x256xf64>
    memref.dealloc %alloc_4 {arts.id = 35 : i64} : memref<256x256xf64>
    memref.dealloc %alloc_3 {arts.id = 33 : i64} : memref<256x256xf64>
    memref.dealloc %alloc {arts.id = 31 : i64} : memref<256x256xf64>
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
