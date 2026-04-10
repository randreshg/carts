module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 5.000000e-01 : f32
    %cst_0 = arith.constant -0.0833333358 : f32
    %cst_1 = arith.constant 0.666666686 : f32
    %cst_2 = arith.constant -0.666666686 : f32
    %cst_3 = arith.constant 0.0833333358 : f32
    %cst_4 = arith.constant 5.000000e-02 : f32
    %c17_i32 = arith.constant 17 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_5 = arith.constant 1.000000e-01 : f32
    %cst_6 = arith.constant 0.000000e+00 : f32
    %c1_i32 = arith.constant 1 : i32
    %cst_7 = arith.constant 3.000000e+00 : f32
    %cst_8 = arith.constant 1.000000e-03 : f32
    %c11_i32 = arith.constant 11 : i32
    %cst_9 = arith.constant 2.000000e+00 : f32
    %cst_10 = arith.constant 1.500000e-03 : f32
    %c7_i32 = arith.constant 7 : i32
    %c576 = arith.constant 576 : index
    %c320 = arith.constant 320 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c3 = arith.constant 3 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_11 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 208 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:158:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 208 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    affine.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:116:17", totalAccesses = 16 : i64, readCount = 15 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 707788800 : i64, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = false, dominantAccessPattern = 2 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 3], estimatedAccessBytes = 64 : i64>, arts.metadata_provenance = "exact"} : memref<3x320x320x576xf32>
    %alloc_12 = memref.alloc() {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:117:19", totalAccesses = 5 : i64, readCount = 1 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 707788800 : i64, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<3x320x320x576xf32>
    %alloc_13 = memref.alloc() {arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:119:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 235929600 : i64, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x576xf32>
    %alloc_14 = memref.alloc() {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:120:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 235929600 : i64, firstUseId = 41 : i64, lastUseId = 42 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<320x320x576xf32>
    %7 = "polygeist.undef"() : () -> i32
    %alloca_15 = memref.alloca() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:144:3", totalAccesses = 11 : i64, readCount = 5 : i64, writeCount = 6 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 44 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    affine.store %7, %alloca_15[] : memref<i32>
    affine.store %c0_i32, %alloca_15[] : memref<i32>
    affine.for %arg0 = 0 to 3 {
      %11 = arith.index_cast %arg0 : index to i32
      affine.store %c0_i32, %alloca_15[] : memref<i32>
      %12 = arith.muli %11, %c17_i32 : i32
      %13 = arith.sitofp %11 : i32 to f32
      %14 = arith.mulf %13, %cst_5 : f32
      affine.for %arg1 = 0 to 320 {
        scf.for %arg2 = %c0 to %c320 step %c1 {
          scf.for %arg3 = %c0 to %c576 step %c1 {
            %15 = affine.load %alloca_15[] : memref<i32>
            %16 = arith.addi %15, %12 : i32
            %17 = arith.remsi %16, %c23_i32 : i32
            %18 = arith.sitofp %17 : i32 to f32
            %19 = arith.mulf %18, %cst_4 : f32
            %20 = arith.subf %19, %14 : f32
            memref.store %20, %alloc[%arg0, %arg1, %arg2, %arg3] : memref<3x320x320x576xf32>
            memref.store %cst_6, %alloc_12[%arg0, %arg1, %arg2, %arg3] : memref<3x320x320x576xf32>
            %21 = affine.load %alloca_15[] : memref<i32>
            %22 = arith.addi %21, %c1_i32 : i32
            affine.store %22, %alloca_15[] : memref<i32>
          } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 70 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    affine.store %c0_i32, %alloca_15[] : memref<i32>
    affine.for %arg0 = 0 to 320 {
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c576 step %c1 {
          %11 = affine.load %alloca_15[] : memref<i32>
          %12 = arith.remsi %11, %c11_i32 : i32
          %13 = arith.sitofp %12 : i32 to f32
          %14 = arith.mulf %13, %cst_8 : f32
          %15 = arith.addf %14, %cst_7 : f32
          memref.store %15, %alloc_13[%arg0, %arg1, %arg2] : memref<320x320x576xf32>
          %16 = affine.load %alloca_15[] : memref<i32>
          %17 = arith.remsi %16, %c7_i32 : i32
          %18 = arith.sitofp %17 : i32 to f32
          %19 = arith.mulf %18, %cst_10 : f32
          %20 = arith.addf %19, %cst_9 : f32
          memref.store %20, %alloc_14[%arg0, %arg1, %arg2] : memref<320x320x576xf32>
          %21 = affine.load %alloca_15[] : memref<i32>
          %22 = arith.addi %21, %c1_i32 : i32
          affine.store %22, %alloca_15[] : memref<i32>
        } {arts.id = 88 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 90 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_16 = memref.alloca() {arts.id = 94 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 94 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_17 = memref.alloca() {arts.id = 95 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 95 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_18 = memref.alloca() {arts.id = 96 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 96 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_19 = memref.alloca() {arts.id = 97 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 97 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_20 = memref.alloca() {arts.id = 98 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 98 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 12 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_21 = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_22 = memref.alloca() {arts.id = 100 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 100 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_23 = memref.alloca() {arts.id = 101 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 101 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_24 = memref.alloca() {arts.id = 102 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 102 : i64, hasUniformAccess = false, dominantAccessPattern = 4 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = false, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
    affine.for %arg0 = 0 to 10 {
      %11 = "polygeist.undef"() : () -> f32
      affine.store %11, %alloca_16[] : memref<f32>
      affine.store %11, %alloca_17[] : memref<f32>
      affine.store %11, %alloca_18[] : memref<f32>
      affine.store %11, %alloca_19[] : memref<f32>
      affine.store %11, %alloca_20[] : memref<f32>
      affine.store %11, %alloca_21[] : memref<f32>
      affine.store %11, %alloca_22[] : memref<f32>
      affine.store %11, %alloca_23[] : memref<f32>
      affine.store %cst_0, %alloca_24[0] : memref<5xf32>
      affine.store %cst_1, %alloca_24[1] : memref<5xf32>
      affine.store %cst_6, %alloca_24[2] : memref<5xf32>
      affine.store %cst_2, %alloca_24[3] : memref<5xf32>
      affine.store %cst_3, %alloca_24[4] : memref<5xf32>
      affine.for %arg1 = 4 to 572 {
        affine.for %arg2 = 4 to 316 {
          affine.for %arg3 = 4 to 316 {
            %12 = affine.load %alloc_13[%arg3, %arg2, %arg1] : memref<320x320x576xf32>
            affine.store %12, %alloca_23[] : memref<f32>
            %13 = affine.load %alloc_14[%arg3, %arg2, %arg1] : memref<320x320x576xf32>
            affine.store %13, %alloca_22[] : memref<f32>
            affine.store %cst_6, %alloca_21[] : memref<f32>
            affine.for %arg4 = -2 to 3 {
              %50 = affine.load %alloca_24[%arg4 + 2] : memref<5xf32>
              %51 = affine.load %alloc[0, %arg3 + %arg4, %arg2, %arg1] : memref<3x320x320x576xf32>
              %52 = affine.load %alloc[0, %arg3, %arg2 + %arg4, %arg1] : memref<3x320x320x576xf32>
              %53 = arith.addf %51, %52 : f32
              %54 = affine.load %alloc[0, %arg3, %arg2, %arg1 + %arg4] : memref<3x320x320x576xf32>
              %55 = arith.addf %53, %54 : f32
              %56 = arith.mulf %50, %55 : f32
              %57 = affine.load %alloca_21[] : memref<f32>
              %58 = arith.addf %57, %56 : f32
              affine.store %58, %alloca_21[] : memref<f32>
            } {arts.id = 133 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            %14 = affine.load %alloc[0, %arg3 + 1, %arg2, %arg1] : memref<3x320x320x576xf32>
            %15 = affine.load %alloc[0, %arg3 - 1, %arg2, %arg1] : memref<3x320x320x576xf32>
            %16 = arith.subf %14, %15 : f32
            affine.store %16, %alloca_20[] : memref<f32>
            %17 = affine.load %alloca_23[] : memref<f32>
            %18 = affine.load %alloca_21[] : memref<f32>
            %19 = arith.mulf %17, %18 : f32
            %20 = affine.load %alloca_22[] : memref<f32>
            %21 = arith.addf %20, %17 : f32
            %22 = affine.load %alloca_20[] : memref<f32>
            %23 = arith.mulf %21, %22 : f32
            %24 = arith.mulf %23, %cst : f32
            %25 = arith.addf %19, %24 : f32
            affine.store %25, %alloc_12[0, %arg3, %arg2, %arg1] : memref<3x320x320x576xf32>
            affine.store %cst_6, %alloca_19[] : memref<f32>
            affine.for %arg4 = -2 to 3 {
              %50 = affine.load %alloca_24[%arg4 + 2] : memref<5xf32>
              %51 = affine.load %alloc[1, %arg3 + %arg4, %arg2, %arg1] : memref<3x320x320x576xf32>
              %52 = affine.load %alloc[1, %arg3, %arg2 + %arg4, %arg1] : memref<3x320x320x576xf32>
              %53 = arith.addf %51, %52 : f32
              %54 = affine.load %alloc[1, %arg3, %arg2, %arg1 + %arg4] : memref<3x320x320x576xf32>
              %55 = arith.addf %53, %54 : f32
              %56 = arith.mulf %50, %55 : f32
              %57 = affine.load %alloca_19[] : memref<f32>
              %58 = arith.addf %57, %56 : f32
              affine.store %58, %alloca_19[] : memref<f32>
            } {arts.id = 160 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            %26 = affine.load %alloc[1, %arg3, %arg2 + 1, %arg1] : memref<3x320x320x576xf32>
            %27 = affine.load %alloc[1, %arg3, %arg2 - 1, %arg1] : memref<3x320x320x576xf32>
            %28 = arith.subf %26, %27 : f32
            affine.store %28, %alloca_18[] : memref<f32>
            %29 = affine.load %alloca_23[] : memref<f32>
            %30 = affine.load %alloca_19[] : memref<f32>
            %31 = arith.mulf %29, %30 : f32
            %32 = affine.load %alloca_22[] : memref<f32>
            %33 = arith.addf %32, %29 : f32
            %34 = affine.load %alloca_18[] : memref<f32>
            %35 = arith.mulf %33, %34 : f32
            %36 = arith.mulf %35, %cst : f32
            %37 = arith.addf %31, %36 : f32
            affine.store %37, %alloc_12[1, %arg3, %arg2, %arg1] : memref<3x320x320x576xf32>
            affine.store %cst_6, %alloca_17[] : memref<f32>
            affine.for %arg4 = -2 to 3 {
              %50 = affine.load %alloca_24[%arg4 + 2] : memref<5xf32>
              %51 = affine.load %alloc[2, %arg3 + %arg4, %arg2, %arg1] : memref<3x320x320x576xf32>
              %52 = affine.load %alloc[2, %arg3, %arg2 + %arg4, %arg1] : memref<3x320x320x576xf32>
              %53 = arith.addf %51, %52 : f32
              %54 = affine.load %alloc[2, %arg3, %arg2, %arg1 + %arg4] : memref<3x320x320x576xf32>
              %55 = arith.addf %53, %54 : f32
              %56 = arith.mulf %50, %55 : f32
              %57 = affine.load %alloca_17[] : memref<f32>
              %58 = arith.addf %57, %56 : f32
              affine.store %58, %alloca_17[] : memref<f32>
            } {arts.id = 187 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            %38 = affine.load %alloc[2, %arg3, %arg2, %arg1 + 1] : memref<3x320x320x576xf32>
            %39 = affine.load %alloc[2, %arg3, %arg2, %arg1 - 1] : memref<3x320x320x576xf32>
            %40 = arith.subf %38, %39 : f32
            affine.store %40, %alloca_16[] : memref<f32>
            %41 = affine.load %alloca_23[] : memref<f32>
            %42 = affine.load %alloca_17[] : memref<f32>
            %43 = arith.mulf %41, %42 : f32
            %44 = affine.load %alloca_22[] : memref<f32>
            %45 = arith.addf %44, %41 : f32
            %46 = affine.load %alloca_16[] : memref<f32>
            %47 = arith.mulf %45, %46 : f32
            %48 = arith.mulf %47, %cst : f32
            %49 = arith.addf %43, %48 : f32
            affine.store %49, %alloc_12[2, %arg3, %arg2, %arg1] : memref<3x320x320x576xf32>
          } {arts.id = 203 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
        } {arts.id = 205 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 207 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 568 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
      func.call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 9 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:149:3">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_11, %alloca[] : memref<f64>
    affine.for %arg0 = 0 to 320 {
      scf.for %arg1 = %c0 to %c3 step %c1 {
        %11 = memref.load %alloc_12[%arg1, %arg0, %arg0, %arg0] : memref<3x320x320x576xf32>
        %12 = arith.extf %11 : f32 to f64
        %13 = math.absf %12 : f64
        %14 = affine.load %alloca[] : memref<f64>
        %15 = arith.addf %14, %13 : f64
        affine.store %15, %alloca[] : memref<f64>
      } {arts.id = 213 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:163:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 212 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:162:3">, arts.metadata_provenance = "exact"}
    %9 = affine.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_14 {arts.id = 42 : i64} : memref<320x320x576xf32>
    memref.dealloc %alloc_13 {arts.id = 40 : i64} : memref<320x320x576xf32>
    memref.dealloc %alloc_12 {arts.id = 38 : i64} : memref<3x320x320x576xf32>
    memref.dealloc %alloc {arts.id = 36 : i64} : memref<3x320x320x576xf32>
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
