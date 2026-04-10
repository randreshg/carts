module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem_velocity_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 1.000000e+00 : f64
    %cst_0 = arith.constant 1.000000e-03 : f64
    %c47 = arith.constant 47 : index
    %cst_1 = arith.constant 2.300000e+03 : f64
    %c11_i32 = arith.constant 11 : i32
    %cst_2 = arith.constant 2.000000e-02 : f64
    %c2_i32 = arith.constant 2 : i32
    %c17_i32 = arith.constant 17 : i32
    %c3_i32 = arith.constant 3 : i32
    %c19_i32 = arith.constant 19 : i32
    %c5_i32 = arith.constant 5 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_3 = arith.constant 1.000000e-02 : f64
    %c7_i32 = arith.constant 7 : i32
    %c13_i32 = arith.constant 13 : i32
    %c29_i32 = arith.constant 29 : i32
    %c31_i32 = arith.constant 31 : i32
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c48 = arith.constant 48 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 196 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:141:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 196 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() {arts.id = 66 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:91:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 66 : i64, lastUseId = 67 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_5 = memref.alloc() {arts.id = 68 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:92:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 68 : i64, lastUseId = 69 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_6 = memref.alloc() {arts.id = 70 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:93:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 70 : i64, lastUseId = 71 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_7 = memref.alloc() {arts.id = 72 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:94:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 72 : i64, lastUseId = 73 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_8 = memref.alloc() {arts.id = 74 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:95:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 74 : i64, lastUseId = 75 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_9 = memref.alloc() {arts.id = 76 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:96:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 76 : i64, lastUseId = 77 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_10 = memref.alloc() {arts.id = 78 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:97:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 78 : i64, lastUseId = 79 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_11 = memref.alloc() {arts.id = 80 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:98:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 80 : i64, lastUseId = 81 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_12 = memref.alloc() {arts.id = 82 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:99:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 82 : i64, lastUseId = 83 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %alloc_13 = memref.alloc() {arts.id = 84 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:100:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, firstUseId = 84 : i64, lastUseId = 85 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<48x48x48xf64>
    %7 = "polygeist.undef"() : () -> i32
    %alloca_14 = memref.alloca() {arts.id = 87 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:127:3", totalAccesses = 11 : i64, readCount = 8 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 87 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_14[] : memref<i32>
    memref.store %c0_i32, %alloca_14[] : memref<i32>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          memref.store %cst_4, %alloc[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          memref.store %cst_4, %alloc_5[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          memref.store %cst_4, %alloc_6[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %11 = memref.load %alloca_14[] : memref<i32>
          %12 = arith.remsi %11, %c11_i32 : i32
          %13 = arith.sitofp %12 : i32 to f64
          %14 = arith.addf %13, %cst_1 : f64
          memref.store %14, %alloc_7[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %15 = memref.load %alloca_14[] : memref<i32>
          %16 = arith.muli %15, %c2_i32 : i32
          %17 = arith.remsi %16, %c17_i32 : i32
          %18 = arith.sitofp %17 : i32 to f64
          %19 = arith.mulf %18, %cst_2 : f64
          memref.store %19, %alloc_8[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %20 = memref.load %alloca_14[] : memref<i32>
          %21 = arith.muli %20, %c3_i32 : i32
          %22 = arith.remsi %21, %c19_i32 : i32
          %23 = arith.sitofp %22 : i32 to f64
          %24 = arith.mulf %23, %cst_2 : f64
          memref.store %24, %alloc_9[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %25 = memref.load %alloca_14[] : memref<i32>
          %26 = arith.muli %25, %c5_i32 : i32
          %27 = arith.remsi %26, %c23_i32 : i32
          %28 = arith.sitofp %27 : i32 to f64
          %29 = arith.mulf %28, %cst_2 : f64
          memref.store %29, %alloc_10[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %30 = memref.load %alloca_14[] : memref<i32>
          %31 = arith.muli %30, %c7_i32 : i32
          %32 = arith.remsi %31, %c13_i32 : i32
          %33 = arith.sitofp %32 : i32 to f64
          %34 = arith.mulf %33, %cst_3 : f64
          memref.store %34, %alloc_11[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %35 = memref.load %alloca_14[] : memref<i32>
          %36 = arith.muli %35, %c11_i32 : i32
          %37 = arith.remsi %36, %c29_i32 : i32
          %38 = arith.sitofp %37 : i32 to f64
          %39 = arith.mulf %38, %cst_3 : f64
          memref.store %39, %alloc_12[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %40 = memref.load %alloca_14[] : memref<i32>
          %41 = arith.muli %40, %c13_i32 : i32
          %42 = arith.remsi %41, %c31_i32 : i32
          %43 = arith.sitofp %42 : i32 to f64
          %44 = arith.mulf %43, %cst_3 : f64
          memref.store %44, %alloc_13[%arg0, %arg1, %arg2] : memref<48x48x48xf64>
          %45 = memref.load %alloca_14[] : memref<i32>
          %46 = arith.addi %45, %c1_i32 : i32
          memref.store %46, %alloca_14[] : memref<i32>
        } {arts.id = 138 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 140 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    arts.edt <parallel> <internode> route(%c-1_i32) attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>, no_verify = #arts.no_verify} {
      arts.for(%c1) to(%c47) step(%c1) schedule(<static>) {{
      ^bb0(%arg0: index):
        %alloca_15 = memref.alloca() {arts.id = 143 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 143 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
        memref.store %4, %alloca_15[] : memref<f64>
        %alloca_16 = memref.alloca() {arts.id = 145 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 145 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 7 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
        memref.store %4, %alloca_16[] : memref<f64>
        %alloca_17 = memref.alloca() {arts.id = 147 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 3 : i64, readCount = 1 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 147 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 24 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
        memref.store %4, %alloca_17[] : memref<f64>
        %alloca_18 = memref.alloca() {arts.id = 149 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:133:5", totalAccesses = 5 : i64, readCount = 3 : i64, writeCount = 2 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 149 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
        memref.store %4, %alloca_18[] : memref<f64>
        %11 = arith.index_cast %arg0 : index to i32
        scf.for %arg1 = %c1 to %c47 step %c1 {
          %12 = arith.index_cast %arg1 : index to i32
          scf.for %arg2 = %c1 to %c47 step %c1 {
            %13 = arith.index_cast %arg2 : index to i32
            %14 = memref.load %alloc_7[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %15 = arith.divf %cst, %14 : f64
            memref.store %15, %alloca_18[] : memref<f64>
            %16 = arith.addi %13, %c1_i32 : i32
            %17 = arith.index_cast %16 : i32 to index
            %18 = memref.load %alloc_8[%17, %arg1, %arg0] : memref<48x48x48xf64>
            %19 = memref.load %alloc_8[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %20 = arith.subf %18, %19 : f64
            %21 = arith.addi %12, %c1_i32 : i32
            %22 = arith.index_cast %21 : i32 to index
            %23 = memref.load %alloc_11[%arg2, %22, %arg0] : memref<48x48x48xf64>
            %24 = memref.load %alloc_11[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %25 = arith.subf %23, %24 : f64
            %26 = arith.addf %20, %25 : f64
            %27 = arith.addi %11, %c1_i32 : i32
            %28 = arith.index_cast %27 : i32 to index
            %29 = memref.load %alloc_12[%arg2, %arg1, %28] : memref<48x48x48xf64>
            %30 = memref.load %alloc_12[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %31 = arith.subf %29, %30 : f64
            %32 = arith.addf %26, %31 : f64
            memref.store %32, %alloca_17[] : memref<f64>
            %33 = memref.load %alloc_11[%17, %arg1, %arg0] : memref<48x48x48xf64>
            %34 = memref.load %alloc_11[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %35 = arith.subf %33, %34 : f64
            %36 = memref.load %alloc_9[%arg2, %22, %arg0] : memref<48x48x48xf64>
            %37 = memref.load %alloc_9[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %38 = arith.subf %36, %37 : f64
            %39 = arith.addf %35, %38 : f64
            %40 = memref.load %alloc_13[%arg2, %arg1, %28] : memref<48x48x48xf64>
            %41 = memref.load %alloc_13[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %42 = arith.subf %40, %41 : f64
            %43 = arith.addf %39, %42 : f64
            memref.store %43, %alloca_16[] : memref<f64>
            %44 = memref.load %alloc_12[%17, %arg1, %arg0] : memref<48x48x48xf64>
            %45 = memref.load %alloc_12[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %46 = arith.subf %44, %45 : f64
            %47 = memref.load %alloc_13[%arg2, %22, %arg0] : memref<48x48x48xf64>
            %48 = memref.load %alloc_13[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %49 = arith.subf %47, %48 : f64
            %50 = arith.addf %46, %49 : f64
            %51 = memref.load %alloc_10[%arg2, %arg1, %28] : memref<48x48x48xf64>
            %52 = memref.load %alloc_10[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %53 = arith.subf %51, %52 : f64
            %54 = arith.addf %50, %53 : f64
            memref.store %54, %alloca_15[] : memref<f64>
            %55 = memref.load %alloca_18[] : memref<f64>
            %56 = arith.mulf %55, %cst_0 : f64
            %57 = memref.load %alloca_17[] : memref<f64>
            %58 = arith.mulf %56, %57 : f64
            %59 = memref.load %alloc[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %60 = arith.addf %59, %58 : f64
            memref.store %60, %alloc[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %61 = memref.load %alloca_18[] : memref<f64>
            %62 = arith.mulf %61, %cst_0 : f64
            %63 = memref.load %alloca_16[] : memref<f64>
            %64 = arith.mulf %62, %63 : f64
            %65 = memref.load %alloc_5[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %66 = arith.addf %65, %64 : f64
            memref.store %66, %alloc_5[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %67 = memref.load %alloca_18[] : memref<f64>
            %68 = arith.mulf %67, %cst_0 : f64
            %69 = memref.load %alloca_15[] : memref<f64>
            %70 = arith.mulf %68, %69 : f64
            %71 = memref.load %alloc_6[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
            %72 = arith.addf %71, %70 : f64
            memref.store %72, %alloc_6[%arg2, %arg1, %arg0] : memref<48x48x48xf64>
          } {arts.id = 188 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
        } {arts.id = 190 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
      }} {arts.id = 193 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 46 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_origin_id = 193 : i64, arts.metadata_provenance = "transferred", arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>}
    }
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_4, %alloca[] : memref<f64>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      %11 = memref.load %alloc[%arg0, %arg0, %arg0] : memref<48x48x48xf64>
      %12 = memref.load %alloc_5[%arg0, %arg0, %arg0] : memref<48x48x48xf64>
      %13 = arith.addf %11, %12 : f64
      %14 = memref.load %alloc_6[%arg0, %arg0, %arg0] : memref<48x48x48xf64>
      %15 = arith.addf %13, %14 : f64
      %16 = memref.load %alloca[] : memref<f64>
      %17 = arith.addf %16, %15 : f64
      memref.store %17, %alloca[] : memref<f64>
    } {arts.id = 188 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:145:3">, arts.metadata_provenance = "recovered"}
    %9 = memref.load %alloca[] : memref<f64>
    call @carts_bench_checksum_d(%9) : (f64) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    %10 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%10, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%10) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_13 {arts.id = 85 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_12 {arts.id = 83 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_11 {arts.id = 81 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_10 {arts.id = 79 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_9 {arts.id = 77 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_8 {arts.id = 75 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_7 {arts.id = 73 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_6 {arts.id = 71 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc_5 {arts.id = 69 : i64} : memref<48x48x48xf64>
    memref.dealloc %alloc {arts.id = 67 : i64} : memref<48x48x48xf64>
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
