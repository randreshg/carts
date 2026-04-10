module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %cst = arith.constant 1.000000e+02 : f32
    %cst_0 = arith.constant 0x49800000 : f32
    %cst_1 = arith.constant -1.702000e+00 : f32
    %cst_2 = arith.constant 1.000000e+00 : f32
    %cst_3 = arith.constant 5.000000e-01 : f32
    %cst_4 = arith.constant 4.471500e-02 : f32
    %cst_5 = arith.constant 0.797884583 : f32
    %cst_6 = arith.constant 1.000000e-01 : f32
    %cst_7 = arith.constant 0.000000e+00 : f32
    %cst_8 = arith.constant 6.000000e+00 : f32
    %cst_9 = arith.constant -3.000000e+00 : f32
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %4 = "polygeist.undef"() : () -> f64
    %alloca = memref.alloca() {arts.id = 222 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:226:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 222 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca[] : memref<f64>
    %alloca_11 = memref.alloca() {arts.id = 212 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:223:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 212 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_11[] : memref<f64>
    %alloca_12 = memref.alloca() {arts.id = 203 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:220:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 203 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_12[] : memref<f64>
    %alloca_13 = memref.alloca() {arts.id = 194 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:217:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 194 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_13[] : memref<f64>
    %alloca_14 = memref.alloca() {arts.id = 184 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:214:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 184 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_14[] : memref<f64>
    %alloca_15 = memref.alloca() {arts.id = 175 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:211:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 175 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_15[] : memref<f64>
    %alloca_16 = memref.alloca() {arts.id = 166 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:208:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 166 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_16[] : memref<f64>
    %alloca_17 = memref.alloca() {arts.id = 157 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:205:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 157 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    affine.store %4, %alloca_17[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:174:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 41 : i64, lastUseId = 248 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 8 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_18 = memref.alloc() {arts.id = 42 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:177:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4194304 : i64, firstUseId = 42 : i64, lastUseId = 249 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_19 = memref.alloc() {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:179:27", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 400 : i64, firstUseId = 43 : i64, lastUseId = 250 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<100xf32>
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_20 = memref.alloca() {arts.id = 127 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 127 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_21 = memref.alloca() {arts.id = 128 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 128 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.mulf %31, %cst_3 : f32
      %33 = arith.mulf %31, %31 : f32
      %34 = arith.mulf %33, %31 : f32
      %35 = arith.mulf %34, %cst_4 : f32
      %36 = arith.addf %31, %35 : f32
      %37 = arith.mulf %36, %cst_5 : f32
      %38 = func.call @tanhf(%37) : (f32) -> f32
      %39 = arith.addf %38, %cst_2 : f32
      %40 = arith.mulf %32, %39 : f32
      affine.store %40, %alloc[%arg2] : memref<1048576xf32>
    } {arts.id = 113 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:193:5">, arts.metadata_provenance = "exact"}
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = func.call @tanhf(%31) : (f32) -> f32
      affine.store %32, %alloc_18[%arg2] : memref<1048576xf32>
    } {arts.id = 126 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:196:5">, arts.metadata_provenance = "exact"}
    %7 = "polygeist.undef"() : () -> f32
    affine.store %7, %alloca_20[] : memref<f32>
    affine.store %7, %alloca_21[] : memref<f32>
    affine.store %cst_9, %alloca_21[] : memref<f32>
    affine.for %arg2 = 1 to 100 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = affine.load %alloca_21[] : memref<f32>
      %33 = arith.cmpf ogt, %31, %32 : f32
      scf.if %33 {
        affine.store %31, %alloca_21[] : memref<f32>
      }
    } {arts.id = 139 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 99 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "exact"}
    affine.store %cst_7, %alloca_20[] : memref<f32>
    %8 = affine.load %alloca_21[] : memref<f32>
    affine.for %arg2 = 0 to 100 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.subf %31, %8 : f32
      %33 = math.exp %32 : f32
      affine.store %33, %alloc_19[%arg2] : memref<100xf32>
      %34 = affine.load %alloc_19[%arg2] : memref<100xf32>
      %35 = affine.load %alloca_20[] : memref<f32>
      %36 = arith.addf %35, %34 : f32
      affine.store %36, %alloca_20[] : memref<f32>
    } {arts.id = 150 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "exact"}
    %9 = affine.load %alloca_20[] : memref<f32>
    affine.for %arg2 = 0 to 100 {
      %27 = affine.load %alloc_19[%arg2] : memref<100xf32>
      %28 = arith.divf %27, %9 : f32
      affine.store %28, %alloc_19[%arg2] : memref<100xf32>
    } {arts.id = 156 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:197:5">, arts.metadata_provenance = "exact"}
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    affine.store %cst_10, %alloca_17[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.cmpf ogt, %31, %cst_7 : f32
      %33 = arith.select %32, %31, %cst_7 : f32
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = affine.load %alloca_17[] : memref<f64>
      %37 = arith.addf %36, %35 : f64
      affine.store %37, %alloca_17[] : memref<f64>
    } {arts.id = 160 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:206:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca_16[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.cmpf ogt, %31, %cst_7 : f32
      %33 = scf.if %32 -> (f32) {
        scf.yield %31 : f32
      } else {
        %38 = arith.mulf %31, %cst_6 : f32
        scf.yield %38 : f32
      }
      %34 = arith.extf %33 : f32 to f64
      %35 = math.absf %34 : f64
      %36 = affine.load %alloca_16[] : memref<f64>
      %37 = arith.addf %36, %35 : f64
      affine.store %37, %alloca_16[] : memref<f64>
    } {arts.id = 169 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:209:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca_15[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.cmpf ogt, %31, %cst_7 : f32
      %33 = arith.select %32, %31, %cst_7 : f32
      %34 = arith.cmpf olt, %33, %cst_8 : f32
      %35 = arith.select %34, %33, %cst_8 : f32
      %36 = arith.extf %35 : f32 to f64
      %37 = math.absf %36 : f64
      %38 = affine.load %alloca_15[] : memref<f64>
      %39 = arith.addf %38, %37 : f64
      affine.store %39, %alloca_15[] : memref<f64>
    } {arts.id = 178 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:212:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca_14[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = affine.load %alloc[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = affine.load %alloca_14[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      affine.store %31, %alloca_14[] : memref<f64>
    } {arts.id = 187 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:215:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca_13[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.mulf %31, %cst_1 : f32
      %33 = math.exp %32 : f32
      %34 = arith.addf %33, %cst_2 : f32
      %35 = arith.divf %cst_2, %34 : f32
      %36 = arith.mulf %31, %35 : f32
      %37 = arith.extf %36 : f32 to f64
      %38 = math.absf %37 : f64
      %39 = affine.load %alloca_13[] : memref<f64>
      %40 = arith.addf %39, %38 : f64
      affine.store %40, %alloca_13[] : memref<f64>
    } {arts.id = 197 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:218:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca_12[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst_0 : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.negf %31 : f32
      %33 = math.exp %32 : f32
      %34 = arith.addf %33, %cst_2 : f32
      %35 = arith.divf %cst_2, %34 : f32
      %36 = arith.extf %35 : f32 to f64
      %37 = math.absf %36 : f64
      %38 = affine.load %alloca_12[] : memref<f64>
      %39 = arith.addf %38, %37 : f64
      affine.store %39, %alloca_12[] : memref<f64>
    } {arts.id = 206 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:221:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca_11[] : memref<f64>
    affine.for %arg2 = 0 to 1048576 {
      %27 = affine.load %alloc_18[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = affine.load %alloca_11[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      affine.store %31, %alloca_11[] : memref<f64>
    } {arts.id = 215 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:224:3">, arts.metadata_provenance = "exact"}
    affine.store %cst_10, %alloca[] : memref<f64>
    affine.for %arg2 = 0 to 100 {
      %27 = affine.load %alloc_19[%arg2] : memref<100xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = affine.load %alloca[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      affine.store %31, %alloca[] : memref<f64>
    } {arts.id = 225 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 100 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "activations.c:227:3">, arts.metadata_provenance = "exact"}
    %11 = affine.load %alloca_17[] : memref<f64>
    %12 = affine.load %alloca_16[] : memref<f64>
    %13 = arith.addf %11, %12 : f64
    %14 = affine.load %alloca_15[] : memref<f64>
    %15 = arith.addf %13, %14 : f64
    %16 = affine.load %alloca_14[] : memref<f64>
    %17 = arith.addf %15, %16 : f64
    %18 = affine.load %alloca_13[] : memref<f64>
    %19 = arith.addf %17, %18 : f64
    %20 = affine.load %alloca_12[] : memref<f64>
    %21 = arith.addf %19, %20 : f64
    %22 = affine.load %alloca_11[] : memref<f64>
    %23 = arith.addf %21, %22 : f64
    %24 = affine.load %alloca[] : memref<f64>
    %25 = arith.addf %23, %24 : f64
    call @carts_bench_checksum_d(%25) : (f64) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    %26 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%26, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc {arts.id = 248 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_18 {arts.id = 249 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_19 {arts.id = 250 : i64} : memref<100xf32>
    call @carts_phase_timer_stop(%26) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
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
  func.func private @tanhf(f32) -> f32 attributes {llvm.linkage = #llvm.linkage<external>}
}
