module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("convolution-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1023 = arith.constant 1023 : index
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
    %c1_i32 = arith.constant 1 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_9 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "convolution-2d.c:111:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1024, %c1024] {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "convolution-2d.c:82:19", totalAccesses = 10 : i64, readCount = 9 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %guid_11, %ptr_12 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <indexed>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1024, %c1024] {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "convolution-2d.c:83:19", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 12 : i64>, arts.metadata_origin_id = 37 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    scf.for %arg2 = %c0 to %c1024 step %c1 {
      %10 = arith.index_cast %arg2 : index to i32
      scf.for %arg3 = %c0 to %c1024 step %c1 {
        %11 = arith.index_cast %arg3 : index to i32
        %12 = arith.addi %10, %11 : i32
        %13 = arith.sitofp %12 : i32 to f32
        %14 = arith.divf %13, %cst : f32
        %15 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        memref.store %14, %15[%arg2, %arg3] : memref<?x?xf32>
      } {arts.id = 47 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:91:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 49 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:91:3">, arts.metadata_provenance = "recovered"}
    scf.for %arg2 = %c0 to %c1024 step %c1 {
      scf.for %arg3 = %c0 to %c1024 step %c1 {
        %10 = arts.db_ref %ptr_12[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        memref.store %cst_10, %10[%arg2, %arg3] : memref<?x?xf32>
      } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:95:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 50 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:94:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %guid_13, %ptr_14 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    arts.lowering_contract(%ptr_14 : memref<?xmemref<?x?xf32>>) pattern(<depPattern = <stencil>, distributionPattern = <stencil>, revision = 2 : i64>)
    %guid_15, %ptr_16 = arts.db_acquire[<inout>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?xmemref<?x?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    arts.lowering_contract(%ptr_16 : memref<?xmemref<?x?xf32>>) pattern(<depPattern = <stencil>, distributionPattern = <stencil>, revision = 2 : i64>)
    arts.edt <parallel> <intranode> route(%c-1_i32) (%ptr_14, %ptr_16) : memref<?xmemref<?x?xf32>>, memref<?xmemref<?x?xf32>> attributes {arts.id = 108 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>, workers = #arts.workers<64>} {
    ^bb0(%arg2: memref<?xmemref<?x?xf32>>, %arg3: memref<?xmemref<?x?xf32>>):
      arts.for(%c1) to(%c1023) step(%c1) schedule(<static>) {{
      ^bb0(%arg4: index):
        %10 = arith.index_cast %arg4 : index to i32
        %11 = arith.addi %10, %c-1_i32 : i32
        %12 = arith.index_cast %11 : i32 to index
        %13 = arith.addi %10, %c1_i32 : i32
        %14 = arith.index_cast %13 : i32 to index
        scf.for %arg5 = %c1 to %c1023 step %c1 {
          %15 = arith.index_cast %arg5 : index to i32
          %16 = arith.addi %15, %c-1_i32 : i32
          %17 = arith.index_cast %16 : i32 to index
          %18 = arts.db_ref %arg2[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
          %19 = memref.load %18[%12, %17] : memref<?x?xf32>
          %20 = arith.extf %19 : f32 to f64
          %21 = arith.mulf %20, %cst_8 : f64
          %22 = memref.load %18[%12, %arg5] : memref<?x?xf32>
          %23 = arith.extf %22 : f32 to f64
          %24 = arith.mulf %23, %cst_7 : f64
          %25 = arith.addf %21, %24 : f64
          %26 = arith.addi %15, %c1_i32 : i32
          %27 = arith.index_cast %26 : i32 to index
          %28 = memref.load %18[%12, %27] : memref<?x?xf32>
          %29 = arith.extf %28 : f32 to f64
          %30 = arith.mulf %29, %cst_6 : f64
          %31 = arith.addf %25, %30 : f64
          %32 = memref.load %18[%arg4, %17] : memref<?x?xf32>
          %33 = arith.extf %32 : f32 to f64
          %34 = arith.mulf %33, %cst_5 : f64
          %35 = arith.addf %31, %34 : f64
          %36 = memref.load %18[%arg4, %arg5] : memref<?x?xf32>
          %37 = arith.extf %36 : f32 to f64
          %38 = arith.mulf %37, %cst_4 : f64
          %39 = arith.addf %35, %38 : f64
          %40 = memref.load %18[%arg4, %27] : memref<?x?xf32>
          %41 = arith.extf %40 : f32 to f64
          %42 = arith.mulf %41, %cst_3 : f64
          %43 = arith.addf %39, %42 : f64
          %44 = memref.load %18[%14, %17] : memref<?x?xf32>
          %45 = arith.extf %44 : f32 to f64
          %46 = arith.mulf %45, %cst_2 : f64
          %47 = arith.addf %43, %46 : f64
          %48 = memref.load %18[%14, %arg5] : memref<?x?xf32>
          %49 = arith.extf %48 : f32 to f64
          %50 = arith.mulf %49, %cst_1 : f64
          %51 = arith.addf %47, %50 : f64
          %52 = memref.load %18[%14, %27] : memref<?x?xf32>
          %53 = arith.extf %52 : f32 to f64
          %54 = arith.mulf %53, %cst_0 : f64
          %55 = arith.addf %51, %54 : f64
          %56 = arith.truncf %55 : f64 to f32
          %57 = arts.db_ref %arg3[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
          memref.store %56, %57[%arg4, %arg5] : memref<?x?xf32>
        } {arts.id = 101 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1022 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:104:5">, arts.metadata_provenance = "exact"}
      }} {arts.id = 104 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1022 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "convolution-2d.c:104:5">, arts.metadata_origin_id = 104 : i64, arts.metadata_provenance = "transferred", arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>}
      arts.db_release(%arg2) : memref<?xmemref<?x?xf32>>
      arts.db_release(%arg3) : memref<?xmemref<?x?xf32>>
    }
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_9, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c1024 step %c1 {
      %10 = arts.db_ref %ptr_12[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
      %11 = memref.load %10[%arg2, %arg2] : memref<?x?xf32>
      %12 = arith.extf %11 : f32 to f64
      %13 = memref.load %alloca[] : memref<f64>
      %14 = arith.addf %13, %12 : f64
      memref.store %14, %alloca[] : memref<f64>
    } {arts.id = 98 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "convolution-2d.c:113:3">, arts.metadata_provenance = "recovered"}
    %8 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%8) : (f64) -> ()
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    %9 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%9, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%9) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf32>>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?xmemref<?x?xf32>>
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
