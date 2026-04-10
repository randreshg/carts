#map = affine_map<(d0) -> (d0)>
module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for single-worker lowering checks.\0Aworker_threads=1\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 1 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %cst = arith.constant 1.000000e+02 : f32
    %cst_0 = arith.constant 0x49800000 : f32
    %c1048576 = arith.constant 1048576 : index
    %cst_1 = arith.constant -1.702000e+00 : f32
    %cst_2 = arith.constant 0.797884583 : f32
    %cst_3 = arith.constant 4.471500e-02 : f32
    %cst_4 = arith.constant 5.000000e-01 : f32
    %cst_5 = arith.constant 1.000000e+00 : f32
    %cst_6 = arith.constant 1.000000e-01 : f32
    %cst_7 = arith.constant 0.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %cst_8 = arith.constant 6.000000e+00 : f32
    %cst_9 = arith.constant -3.000000e+00 : f32
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_10 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %4 = "polygeist.undef"() : () -> f64
    %alloca = memref.alloca() {arts.id = 268 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:226:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 268 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca[] : memref<f64>
    %alloca_11 = memref.alloca() {arts.id = 258 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:223:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 258 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_11[] : memref<f64>
    %alloca_12 = memref.alloca() {arts.id = 248 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:220:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 248 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_12[] : memref<f64>
    %alloca_13 = memref.alloca() {arts.id = 238 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:217:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 238 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_13[] : memref<f64>
    %alloca_14 = memref.alloca() {arts.id = 228 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:214:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 228 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_14[] : memref<f64>
    %alloca_15 = memref.alloca() {arts.id = 218 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:211:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 218 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_15[] : memref<f64>
    %alloca_16 = memref.alloca() {arts.id = 208 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:208:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 208 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_16[] : memref<f64>
    %alloca_17 = memref.alloca() {arts.id = 198 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:205:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 198 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    memref.store %4, %alloca_17[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:171:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 41 : i64, lastUseId = 294 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 10 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_18 = memref.alloc() {arts.id = 42 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:172:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 42 : i64, lastUseId = 295 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_19 = memref.alloc() {arts.id = 43 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:173:22", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 43 : i64, lastUseId = 296 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 12 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_20 = memref.alloc() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:174:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 44 : i64, lastUseId = 297 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 13 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_21 = memref.alloc() {arts.id = 45 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:175:26", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 45 : i64, lastUseId = 298 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 14 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_22 = memref.alloc() {arts.id = 46 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:176:24", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 46 : i64, lastUseId = 299 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 15 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_23 = memref.alloc() {arts.id = 47 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:177:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 4194304 : i64, firstUseId = 47 : i64, lastUseId = 300 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 16 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4], estimatedAccessBytes = 8 : i64>, arts.metadata_provenance = "exact"} : memref<1048576xf32>
    %alloc_24 = memref.alloc() {arts.id = 48 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "activations.c:179:27", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 400 : i64, firstUseId = 48 : i64, lastUseId = 301 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<100xf32>
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %alloca_25 = memref.alloca() {arts.id = 168 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 168 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_26 = memref.alloca() {arts.id = 169 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "activations.c:197:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 169 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    arts.edt <parallel> <internode> route(%c-1_i32) attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_pattern = #arts.distribution_pattern<uniform>, no_verify = #arts.no_verify} {
      arts.for(%c0) to(%c1048576) step(%c1) {{
      ^bb0(%arg2: index):
        %27 = arith.index_cast %arg2 : index to i32
        %28 = arith.sitofp %27 : i32 to f32
        %29 = arith.divf %28, %cst_0 : f32
        %30 = arith.mulf %29, %cst_8 : f32
        %31 = arith.addf %30, %cst_9 : f32
        %32 = arith.cmpf ogt, %31, %cst_7 : f32
        %33 = arith.select %32, %31, %cst_7 : f32
        memref.store %33, %alloc[%arg2] : memref<1048576xf32>
        %34 = scf.if %32 -> (f32) {
          scf.yield %31 : f32
        } else {
          %37 = arith.mulf %31, %cst_6 : f32
          scf.yield %37 : f32
        }
        memref.store %34, %alloc_18[%arg2] : memref<1048576xf32>
        %35 = arith.cmpf olt, %33, %cst_8 : f32
        %36 = arith.select %35, %33, %cst_8 : f32
        memref.store %36, %alloc_19[%arg2] : memref<1048576xf32>
      }} {arts.id = 101 : i64, arts.linalg.indexing_maps = [#map], arts.linalg.iterator_types = ["parallel"], arts.linalg.num_inputs = 0 : i64, arts.linalg.num_outputs = 1 : i64, arts.linalg.pattern = "elementwise", arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:190:5">, arts.metadata_origin_id = 101 : i64, arts.metadata_provenance = "transferred", arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_pattern = #arts.distribution_pattern<uniform>}
    }
    arts.edt <parallel> <internode> route(%c-1_i32) attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>, no_verify = #arts.no_verify} {
      arts.for(%c0) to(%c1048576) step(%c1) {{
      ^bb0(%arg2: index):
        %27 = arith.index_cast %arg2 : index to i32
        %28 = arith.sitofp %27 : i32 to f32
        %29 = arith.divf %28, %cst_0 : f32
        %30 = arith.mulf %29, %cst_8 : f32
        %31 = arith.addf %30, %cst_9 : f32
        %32 = arith.mulf %31, %31 : f32
        %33 = arith.mulf %32, %31 : f32
        %34 = arith.mulf %33, %cst_3 : f32
        %35 = arith.addf %31, %34 : f32
        %36 = arith.mulf %35, %cst_2 : f32
        %37 = arith.mulf %31, %cst_4 : f32
        %38 = func.call @tanhf(%36) : (f32) -> f32
        %39 = arith.addf %38, %cst_5 : f32
        %40 = arith.mulf %37, %39 : f32
        memref.store %40, %alloc_20[%arg2] : memref<1048576xf32>
      }} {arts.id = 137 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:193:5">, arts.metadata_origin_id = 137 : i64, arts.metadata_provenance = "transferred", arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>}
    }
    arts.edt <parallel> <internode> route(%c-1_i32) attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_pattern = #arts.distribution_pattern<uniform>, no_verify = #arts.no_verify} {
      arts.for(%c0) to(%c1048576) step(%c1) {{
      ^bb0(%arg2: index):
        %27 = arith.index_cast %arg2 : index to i32
        %28 = arith.sitofp %27 : i32 to f32
        %29 = arith.divf %28, %cst_0 : f32
        %30 = arith.mulf %29, %cst_8 : f32
        %31 = arith.addf %30, %cst_9 : f32
        %32 = arith.mulf %31, %cst_1 : f32
        %33 = math.exp %32 : f32
        %34 = arith.addf %33, %cst_5 : f32
        %35 = arith.divf %cst_5, %34 : f32
        %36 = arith.mulf %31, %35 : f32
        memref.store %36, %alloc_21[%arg2] : memref<1048576xf32>
        %37 = arith.negf %31 : f32
        %38 = math.exp %37 : f32
        %39 = arith.addf %38, %cst_5 : f32
        %40 = arith.divf %cst_5, %39 : f32
        memref.store %40, %alloc_22[%arg2] : memref<1048576xf32>
      }} {arts.id = 148 : i64, arts.linalg.indexing_maps = [#map], arts.linalg.iterator_types = ["parallel"], arts.linalg.num_inputs = 0 : i64, arts.linalg.num_outputs = 1 : i64, arts.linalg.pattern = "elementwise", arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:194:5">, arts.metadata_origin_id = 148 : i64, arts.metadata_provenance = "transferred", arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<elementwise_pipeline>, distribution_pattern = #arts.distribution_pattern<uniform>}
    }
    arts.edt <parallel> <internode> route(%c-1_i32) attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>, no_verify = #arts.no_verify} {
      arts.for(%c0) to(%c1048576) step(%c1) {{
      ^bb0(%arg2: index):
        %27 = arith.index_cast %arg2 : index to i32
        %28 = arith.sitofp %27 : i32 to f32
        %29 = arith.divf %28, %cst_0 : f32
        %30 = arith.mulf %29, %cst_8 : f32
        %31 = arith.addf %30, %cst_9 : f32
        %32 = func.call @tanhf(%31) : (f32) -> f32
        memref.store %32, %alloc_23[%arg2] : memref<1048576xf32>
      }} {arts.id = 165 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1048576 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "activations.c:196:5">, arts.metadata_origin_id = 165 : i64, arts.metadata_provenance = "transferred", arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>}
    }
    %7 = "polygeist.undef"() : () -> f32
    memref.store %7, %alloca_25[] : memref<f32>
    memref.store %7, %alloca_26[] : memref<f32>
    memref.store %cst_9, %alloca_26[] : memref<f32>
    scf.for %arg2 = %c1 to %c100 step %c1 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = memref.load %alloca_26[] : memref<f32>
      %33 = arith.cmpf ogt, %31, %32 : f32
      scf.if %33 {
        memref.store %31, %alloca_26[] : memref<f32>
      }
    }
    memref.store %cst_7, %alloca_25[] : memref<f32>
    %8 = memref.load %alloca_26[] : memref<f32>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f32
      %29 = arith.divf %28, %cst : f32
      %30 = arith.mulf %29, %cst_8 : f32
      %31 = arith.addf %30, %cst_9 : f32
      %32 = arith.subf %31, %8 : f32
      %33 = math.exp %32 : f32
      memref.store %33, %alloc_24[%arg2] : memref<100xf32>
      %34 = memref.load %alloc_24[%arg2] : memref<100xf32>
      %35 = memref.load %alloca_25[] : memref<f32>
      %36 = arith.addf %35, %34 : f32
      memref.store %36, %alloca_25[] : memref<f32>
    }
    %9 = memref.load %alloca_25[] : memref<f32>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %27 = memref.load %alloc_24[%arg2] : memref<100xf32>
      %28 = arith.divf %27, %9 : f32
      memref.store %28, %alloc_24[%arg2] : memref<100xf32>
    }
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_10, %alloca_17[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_17[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_17[] : memref<f64>
    }
    memref.store %cst_10, %alloca_16[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc_18[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_16[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_16[] : memref<f64>
    }
    memref.store %cst_10, %alloca_15[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc_19[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_15[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_15[] : memref<f64>
    }
    memref.store %cst_10, %alloca_14[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc_20[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_14[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_14[] : memref<f64>
    }
    memref.store %cst_10, %alloca_13[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc_21[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_13[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_13[] : memref<f64>
    }
    memref.store %cst_10, %alloca_12[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc_22[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_12[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_12[] : memref<f64>
    }
    memref.store %cst_10, %alloca_11[] : memref<f64>
    scf.for %arg2 = %c0 to %c1048576 step %c1 {
      %27 = memref.load %alloc_23[%arg2] : memref<1048576xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca_11[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca_11[] : memref<f64>
    }
    memref.store %cst_10, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c100 step %c1 {
      %27 = memref.load %alloc_24[%arg2] : memref<100xf32>
      %28 = arith.extf %27 : f32 to f64
      %29 = math.absf %28 : f64
      %30 = memref.load %alloca[] : memref<f64>
      %31 = arith.addf %30, %29 : f64
      memref.store %31, %alloca[] : memref<f64>
    }
    %11 = memref.load %alloca_17[] : memref<f64>
    %12 = memref.load %alloca_16[] : memref<f64>
    %13 = arith.addf %11, %12 : f64
    %14 = memref.load %alloca_15[] : memref<f64>
    %15 = arith.addf %13, %14 : f64
    %16 = memref.load %alloca_14[] : memref<f64>
    %17 = arith.addf %15, %16 : f64
    %18 = memref.load %alloca_13[] : memref<f64>
    %19 = arith.addf %17, %18 : f64
    %20 = memref.load %alloca_12[] : memref<f64>
    %21 = arith.addf %19, %20 : f64
    %22 = memref.load %alloca_11[] : memref<f64>
    %23 = arith.addf %21, %22 : f64
    %24 = memref.load %alloca[] : memref<f64>
    %25 = arith.addf %23, %24 : f64
    call @carts_bench_checksum_d(%25) : (f64) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    %26 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%26, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.dealloc %alloc {arts.id = 294 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_18 {arts.id = 295 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_19 {arts.id = 296 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_20 {arts.id = 297 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_21 {arts.id = 298 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_22 {arts.id = 299 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_23 {arts.id = 300 : i64} : memref<1048576xf32>
    memref.dealloc %alloc_24 {arts.id = 301 : i64} : memref<100xf32>
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
