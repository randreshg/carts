#set = affine_set<(d0) : (d0 == 0)>
module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c63_i32 = arith.constant 63 : i32
    %cst = arith.constant 2.500000e-01 : f64
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %true = arith.constant true
    %alloc = memref.alloc() {arts.id = 2 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson_mixed_orientation.c:41:16", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32768 : i64, firstUseId = 2 : i64, lastUseId = 3 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<64x64xf64>
    %alloc_0 = memref.alloc() {arts.id = 4 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson_mixed_orientation.c:42:16", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32768 : i64, firstUseId = 4 : i64, lastUseId = 5 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<64x64xf64>
    %alloc_1 = memref.alloc() {arts.id = 6 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson_mixed_orientation.c:43:19", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 32768 : i64, firstUseId = 6 : i64, lastUseId = 7 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<64x64xf64>
    affine.for %arg0 = 0 to 64 {
      %0 = arith.index_cast %arg0 : index to i32
      affine.for %arg1 = 0 to 64 {
        %1 = arith.index_cast %arg1 : index to i32
        %2 = arith.addi %1, %0 : i32
        %3 = arith.sitofp %2 : i32 to f64
        affine.store %3, %alloc[%arg1, %arg0] : memref<64x64xf64>
      } {arts.id = 14 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 16 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:51:3">, arts.metadata_provenance = "exact"}
    affine.for %arg0 = 0 to 64 {
      scf.for %arg1 = %c0 to %c64 step %c1 {
        %0 = memref.load %alloc_1[%arg0, %arg1] : memref<64x64xf64>
        memref.store %0, %alloc_0[%arg0, %arg1] : memref<64x64xf64>
      } {arts.id = 20 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 22 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
    affine.for %arg0 = 0 to 64 {
      %0 = arith.index_cast %arg0 : index to i32
      %1 = arith.cmpi eq, %0, %c63_i32 : i32
      affine.for %arg1 = 0 to 64 {
        %2 = arith.index_cast %arg1 : index to i32
        %3 = affine.if #set(%arg0) -> i1 {
          affine.yield %true : i1
        } else {
          %6 = arith.cmpi eq, %2, %c0_i32 : i32
          affine.yield %6 : i1
        }
        %4 = arith.select %3, %true, %1 : i1
        %5 = scf.if %4 -> (i1) {
          scf.yield %true : i1
        } else {
          %6 = arith.cmpi eq, %2, %c63_i32 : i32
          scf.yield %6 : i1
        }
        scf.if %5 {
          %6 = affine.load %alloc[%arg0, %arg1] : memref<64x64xf64>
          affine.store %6, %alloc_1[%arg0, %arg1] : memref<64x64xf64>
        } else {
          %6 = affine.load %alloc_0[%arg0 - 1, %arg1] : memref<64x64xf64>
          %7 = affine.load %alloc_0[%arg0, %arg1 + 1] : memref<64x64xf64>
          %8 = arith.addf %6, %7 : f64
          %9 = affine.load %alloc_0[%arg0, %arg1 - 1] : memref<64x64xf64>
          %10 = arith.addf %8, %9 : f64
          %11 = affine.load %alloc_0[%arg0 + 1, %arg1] : memref<64x64xf64>
          %12 = arith.addf %10, %11 : f64
          %13 = affine.load %alloc[%arg0, %arg1] : memref<64x64xf64>
          %14 = arith.addf %12, %13 : f64
          %15 = arith.mulf %14, %cst : f64
          affine.store %15, %alloc_1[%arg0, %arg1] : memref<64x64xf64>
        }
      } {arts.id = 52 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 54 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson_mixed_orientation.c:52:3">, arts.metadata_provenance = "exact"}
    memref.dealloc %alloc_1 {arts.id = 7 : i64} : memref<64x64xf64>
    memref.dealloc %alloc_0 {arts.id = 5 : i64} : memref<64x64xf64>
    memref.dealloc %alloc {arts.id = 3 : i64} : memref<64x64xf64>
    return %c0_i32 : i32
  }
}
