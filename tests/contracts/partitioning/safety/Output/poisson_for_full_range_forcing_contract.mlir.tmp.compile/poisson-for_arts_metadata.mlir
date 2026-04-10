#set = affine_set<(d0) : (d0 == 0)>
module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("poisson-for\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant -9.869604401089358 : f64
    %cst_0 = arith.constant 9.900000e+01 : f64
    %c99_i32 = arith.constant 99 : i32
    %cst_1 = arith.constant 0.010101010101010102 : f64
    %c100 = arith.constant 100 : index
    %cst_2 = arith.constant 2.500000e-01 : f64
    %cst_3 = arith.constant 3.1415926535897931 : f64
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %alloca = memref.alloca() {arts.id = 152 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "poisson-for.c:140:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 152 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 40 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson-for.c:94:16", totalAccesses = 6 : i64, readCount = 3 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 80000 : i64, firstUseId = 40 : i64, lastUseId = 41 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 48 : i64>, arts.metadata_provenance = "exact"} : memref<100x100xf64>
    %alloc_5 = memref.alloc() {arts.id = 42 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson-for.c:95:16", totalAccesses = 6 : i64, readCount = 4 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 80000 : i64, firstUseId = 42 : i64, lastUseId = 43 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 48 : i64>, arts.metadata_provenance = "exact"} : memref<100x100xf64>
    %alloc_6 = memref.alloc() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "poisson-for.c:96:19", totalAccesses = 7 : i64, readCount = 2 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 80000 : i64, firstUseId = 44 : i64, lastUseId = 45 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3], estimatedAccessBytes = 56 : i64>, arts.metadata_provenance = "exact"} : memref<100x100xf64>
    affine.for %arg0 = 0 to 100 {
      scf.for %arg1 = %c0 to %c100 step %c1 {
        memref.store %cst_4, %alloc[%arg0, %arg1] : memref<100x100xf64>
        memref.store %cst_4, %alloc_5[%arg0, %arg1] : memref<100x100xf64>
        memref.store %cst_4, %alloc_6[%arg0, %arg1] : memref<100x100xf64>
      } {arts.id = 47 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:106:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 46 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:105:3">, arts.metadata_provenance = "exact"}
    affine.for %arg0 = 0 to 100 {
      %10 = arith.index_cast %arg0 : index to i32
      %11 = arith.cmpi eq, %10, %c0_i32 : i32
      %12 = arith.cmpi eq, %10, %c99_i32 : i32
      affine.for %arg1 = 0 to 100 {
        %13 = arith.index_cast %arg1 : index to i32
        %14 = affine.if #set(%arg1) -> i1 {
          affine.yield %true : i1
        } else {
          %17 = arith.cmpi eq, %13, %c99_i32 : i32
          affine.yield %17 : i1
        }
        %15 = arith.select %14, %true, %11 : i1
        %16 = arith.select %15, %true, %12 : i1
        scf.if %16 {
          %17 = arith.sitofp %13 : i32 to f64
          %18 = arith.divf %17, %cst_0 : f64
          %19 = arith.sitofp %10 : i32 to f64
          %20 = arith.divf %19, %cst_0 : f64
          %21 = arith.mulf %18, %cst_3 : f64
          %22 = arith.mulf %21, %20 : f64
          %23 = math.sin %22 : f64
          affine.store %23, %alloc[%arg1, %arg0] : memref<100x100xf64>
        } else {
          %17 = arith.sitofp %13 : i32 to f64
          %18 = arith.divf %17, %cst_0 : f64
          %19 = arith.sitofp %10 : i32 to f64
          %20 = arith.divf %19, %cst_0 : f64
          %21 = arith.mulf %18, %18 : f64
          %22 = arith.mulf %20, %20 : f64
          %23 = arith.addf %21, %22 : f64
          %24 = arith.mulf %23, %cst : f64
          %25 = arith.mulf %18, %cst_3 : f64
          %26 = arith.mulf %25, %20 : f64
          %27 = math.sin %26 : f64
          %28 = arith.mulf %24, %27 : f64
          %29 = arith.negf %28 : f64
          affine.store %29, %alloc[%arg1, %arg0] : memref<100x100xf64>
        }
      } {arts.id = 87 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:114:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 89 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:114:3">, arts.metadata_provenance = "exact"}
    affine.for %arg0 = 0 to 100 {
      %10 = arith.index_cast %arg0 : index to i32
      %11 = affine.if #set(%arg0) -> i1 {
        affine.yield %true : i1
      } else {
        %12 = arith.cmpi eq, %10, %c99_i32 : i32
        affine.yield %12 : i1
      }
      affine.for %arg1 = 0 to 100 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = scf.if %11 -> (i1) {
          scf.yield %true : i1
        } else {
          %15 = arith.cmpi eq, %12, %c0_i32 : i32
          scf.yield %15 : i1
        }
        %14 = scf.if %13 -> (i1) {
          scf.yield %true : i1
        } else {
          %15 = arith.cmpi eq, %12, %c99_i32 : i32
          scf.yield %15 : i1
        }
        scf.if %14 {
          %15 = affine.load %alloc[%arg0, %arg1] : memref<100x100xf64>
          affine.store %15, %alloc_6[%arg0, %arg1] : memref<100x100xf64>
        } else {
          affine.store %cst_4, %alloc_6[%arg0, %arg1] : memref<100x100xf64>
        }
      } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:119:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 91 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:118:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    affine.for %arg0 = 1 to 11 {
      scf.for %arg1 = %c0 to %c100 step %c1 {
        scf.for %arg2 = %c0 to %c100 step %c1 {
          %10 = memref.load %alloc_6[%arg1, %arg2] : memref<100x100xf64>
          memref.store %10, %alloc_5[%arg1, %arg2] : memref<100x100xf64>
        } {arts.id = 113 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
      affine.for %arg1 = 0 to 100 {
        %10 = arith.index_cast %arg1 : index to i32
        %11 = arith.cmpi eq, %10, %c99_i32 : i32
        affine.for %arg2 = 0 to 100 {
          %12 = arith.index_cast %arg2 : index to i32
          %13 = affine.if #set(%arg1) -> i1 {
            affine.yield %true : i1
          } else {
            %16 = arith.cmpi eq, %12, %c0_i32 : i32
            affine.yield %16 : i1
          }
          %14 = arith.select %13, %true, %11 : i1
          %15 = scf.if %14 -> (i1) {
            scf.yield %true : i1
          } else {
            %16 = arith.cmpi eq, %12, %c99_i32 : i32
            scf.yield %16 : i1
          }
          scf.if %15 {
            %16 = affine.load %alloc[%arg1, %arg2] : memref<100x100xf64>
            affine.store %16, %alloc_6[%arg1, %arg2] : memref<100x100xf64>
          } else {
            %16 = affine.load %alloc_5[%arg1 - 1, %arg2] : memref<100x100xf64>
            %17 = affine.load %alloc_5[%arg1, %arg2 + 1] : memref<100x100xf64>
            %18 = arith.addf %16, %17 : f64
            %19 = affine.load %alloc_5[%arg1, %arg2 - 1] : memref<100x100xf64>
            %20 = arith.addf %18, %19 : f64
            %21 = affine.load %alloc_5[%arg1 + 1, %arg2] : memref<100x100xf64>
            %22 = arith.addf %20, %21 : f64
            %23 = affine.load %alloc[%arg1, %arg2] : memref<100x100xf64>
            %24 = arith.mulf %23, %cst_1 : f64
            %25 = arith.mulf %24, %cst_1 : f64
            %26 = arith.addf %22, %25 : f64
            %27 = arith.mulf %26, %cst_2 : f64
            affine.store %27, %alloc_6[%arg1, %arg2] : memref<100x100xf64>
          }
        } {arts.id = 147 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 149 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 151 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "poisson-for.c:132:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_4, %alloca[] : memref<f64>
    affine.for %arg0 = 0 to 100 {
      %10 = affine.load %alloc_6[%arg0, %arg0] : memref<100x100xf64>
      %11 = affine.load %alloca[] : memref<f64>
      %12 = arith.addf %11, %10 : f64
      affine.store %12, %alloca[] : memref<f64>
    } {arts.id = 156 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "poisson-for.c:142:3">, arts.metadata_provenance = "exact"}
    %8 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%8) : (f64) -> ()
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_6 {arts.id = 45 : i64} : memref<100x100xf64>
    memref.dealloc %alloc_5 {arts.id = 43 : i64} : memref<100x100xf64>
    memref.dealloc %alloc {arts.id = 41 : i64} : memref<100x100xf64>
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
