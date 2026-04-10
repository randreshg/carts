module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @arts_guid_from_index(i64, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nofree, llvm.nosync, llvm.nounwind, llvm.readnone, llvm.willreturn, memory = #llvm.memory_effects<other = none, argMem = none, inaccessibleMem = none, errnoMem = none, targetMem0 = none, targetMem1 = none>, passthrough = ["nounwind", "nosync", "nofree", "willreturn"]}
  func.func private @arts_guid_reserve_range(i32 {llvm.signext}, i32 {llvm.zeroext}, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_rt(i32 {llvm.signext}, memref<?xmemref<?xi8>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> (i32 {llvm.signext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @artsSetConfigData(memref<?xi8> {llvm.nocapture, llvm.nofree, llvm.readonly}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  llvm.mlir.global internal constant @str_4("[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %0 = llvm.mlir.addressof @str_4 : !llvm.ptr
    %1 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @artsSetConfigData(%1) : (memref<?xi8>) -> ()
    %2 = call @arts_rt(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    return %c0_i32 : i32
  }
  func.func private @arts_shutdown() attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func @main_edt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>) {
    %0 = call @mainBody() : () -> i32
    call @arts_shutdown() : () -> ()
    return
  }
  func.func private @arts_db_create_with_guid(i64, i64, i32 {llvm.signext}, memref<?xi8> {llvm.nocapture, llvm.nofree, llvm.readonly}, memref<?x!llvm.struct<(i32, i64)>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> (!llvm.ptr {llvm.noalias}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nosync, llvm.nounwind, llvm.willreturn, passthrough = ["nounwind", "nosync", "willreturn"]}
  func.func private @arts_guid_reserve(i32 {llvm.signext}, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_db_release(i64) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_signal_edt_null(i64, i32 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_add_dependence(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_edt_create_with_epoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>> {llvm.nocapture, llvm.nofree, llvm.readonly}, i32 {llvm.zeroext}, memref<?xi64> {llvm.nocapture, llvm.nofree, llvm.readonly}, i32 {llvm.zeroext}, i64, memref<?x!llvm.struct<(i32, i64)>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_wait_on_handle(i64) -> (i1 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_initialize_and_start_epoch(i64, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %cst = arith.constant -0.0833333358 : f32
    %c0 = arith.constant 0 : index
    %cst_0 = arith.constant 0.666666686 : f32
    %c1 = arith.constant 1 : index
    %cst_1 = arith.constant 0.000000e+00 : f32
    %c2 = arith.constant 2 : index
    %cst_2 = arith.constant -0.666666686 : f32
    %c3 = arith.constant 3 : index
    %cst_3 = arith.constant 0.0833333358 : f32
    %c4 = arith.constant 4 : index
    %c1_i32 = arith.constant 1 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c320 = arith.constant 320 : index
    %c576 = arith.constant 576 : index
    %c2_i32 = arith.constant 2 : i32
    %c-2 = arith.constant -2 : index
    %cst_4 = arith.constant 5.000000e-01 : f32
    %c316 = arith.constant 316 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %7 = llvm.getelementptr %6[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %8 = llvm.getelementptr %6[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %9 = llvm.getelementptr %8[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %10 = llvm.getelementptr %6[2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %11 = llvm.getelementptr %10[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %alloca = memref.alloca() {arts.id = 94 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 94 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
    %alloca_5 = memref.alloca() {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 103 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_6 = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_7 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    memref.store %cst, %alloca[%c0] : memref<5xf32>
    memref.store %cst_0, %alloca[%c1] : memref<5xf32>
    memref.store %cst_1, %alloca[%c2] : memref<5xf32>
    memref.store %cst_2, %alloca[%c3] : memref<5xf32>
    memref.store %cst_3, %alloca[%c4] : memref<5xf32>
    %12 = "polygeist.undef"() : () -> f32
    %13 = arith.subi %c4, %1 : index
    %14 = arith.cmpi slt, %13, %c0 : index
    %15 = arith.select %14, %c0, %13 : index
    %16 = arith.cmpi slt, %3, %c0 : index
    %17 = arith.select %16, %c0, %3 : index
    %18 = arith.minui %17, %5 : index
    %19 = llvm.getelementptr %6[3] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %20 = llvm.getelementptr %19[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %21 = llvm.load %20 : !llvm.ptr -> !llvm.ptr
    %22 = llvm.load %7 : !llvm.ptr -> !llvm.ptr
    %23 = polygeist.pointer2memref %22 : !llvm.ptr to memref<?x?x?xf32>
    %24 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %25 = polygeist.pointer2memref %24 : !llvm.ptr to memref<?x?x?xf32>
    %26 = llvm.load %11 : !llvm.ptr -> !llvm.ptr
    %27 = polygeist.pointer2memref %26 : !llvm.ptr to memref<?x?x?x?xf32>
    %28 = polygeist.pointer2memref %21 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg4 = %15 to %18 step %c1 {
      %29 = arith.addi %1, %arg4 : index
      memref.store %12, %alloca_5[] : memref<f32>
      memref.store %12, %alloca_6[] : memref<f32>
      memref.store %12, %alloca_7[] : memref<f32>
      %30 = arith.index_cast %29 : index to i32
      %31 = arith.addi %30, %c1_i32 : i32
      %32 = arith.index_cast %31 : i32 to index
      %33 = arith.addi %30, %c-1_i32 : i32
      %34 = arith.index_cast %33 : i32 to index
      scf.for %arg5 = %c4 to %c316 step %c1 {
        %35 = arith.index_cast %arg5 : index to i32
        %36 = arith.addi %35, %c1_i32 : i32
        %37 = arith.index_cast %36 : i32 to index
        %38 = arith.addi %35, %c-1_i32 : i32
        %39 = arith.index_cast %38 : i32 to index
        scf.for %arg6 = %c4 to %c316 step %c1 {
          %40 = arith.index_cast %arg6 : index to i32
          %41 = polygeist.load %23[%arg6, %arg5, %29] sizes(%c320, %c320, %c576) : memref<?x?x?xf32> -> f32
          %42 = polygeist.load %25[%arg6, %arg5, %29] sizes(%c320, %c320, %c576) : memref<?x?x?xf32> -> f32
          memref.store %cst_1, %alloca_7[] : memref<f32>
          scf.for %arg7 = %c-2 to %c3 step %c1 {
            %67 = arith.index_cast %arg7 : index to i32
            %68 = arith.addi %67, %c2_i32 : i32
            %69 = arith.index_cast %68 : i32 to index
            %70 = memref.load %alloca[%69] : memref<5xf32>
            %71 = arith.addi %40, %67 : i32
            %72 = arith.index_cast %71 : i32 to index
            %73 = polygeist.load %27[%c0, %72, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %74 = arith.addi %35, %67 : i32
            %75 = arith.index_cast %74 : i32 to index
            %76 = polygeist.load %27[%c0, %arg6, %75, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %77 = arith.addf %73, %76 : f32
            %78 = arith.addi %30, %67 : i32
            %79 = arith.index_cast %78 : i32 to index
            %80 = polygeist.load %27[%c0, %arg6, %arg5, %79] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %81 = arith.addf %77, %80 : f32
            %82 = arith.mulf %70, %81 : f32
            %83 = memref.load %alloca_7[] : memref<f32>
            %84 = arith.addf %83, %82 : f32
            memref.store %84, %alloca_7[] : memref<f32>
          } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
          %43 = arith.addi %40, %c1_i32 : i32
          %44 = arith.index_cast %43 : i32 to index
          %45 = polygeist.load %27[%c0, %44, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %46 = arith.addi %40, %c-1_i32 : i32
          %47 = arith.index_cast %46 : i32 to index
          %48 = polygeist.load %27[%c0, %47, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %49 = arith.subf %45, %48 : f32
          %50 = arith.mulf %41, %cst_1 : f32
          %51 = arith.addf %42, %41 : f32
          %52 = arith.mulf %51, %49 : f32
          %53 = arith.mulf %52, %cst_4 : f32
          %54 = arith.addf %50, %53 : f32
          polygeist.store %54, %28[%c0, %arg6, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
          memref.store %cst_1, %alloca_6[] : memref<f32>
          scf.for %arg7 = %c-2 to %c3 step %c1 {
            %67 = arith.index_cast %arg7 : index to i32
            %68 = arith.addi %67, %c2_i32 : i32
            %69 = arith.index_cast %68 : i32 to index
            %70 = memref.load %alloca[%69] : memref<5xf32>
            %71 = arith.addi %40, %67 : i32
            %72 = arith.index_cast %71 : i32 to index
            %73 = polygeist.load %27[%c1, %72, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %74 = arith.addi %35, %67 : i32
            %75 = arith.index_cast %74 : i32 to index
            %76 = polygeist.load %27[%c1, %arg6, %75, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %77 = arith.addf %73, %76 : f32
            %78 = arith.addi %30, %67 : i32
            %79 = arith.index_cast %78 : i32 to index
            %80 = polygeist.load %27[%c1, %arg6, %arg5, %79] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %81 = arith.addf %77, %80 : f32
            %82 = arith.mulf %70, %81 : f32
            %83 = memref.load %alloca_6[] : memref<f32>
            %84 = arith.addf %83, %82 : f32
            memref.store %84, %alloca_6[] : memref<f32>
          } {arts.id = 193 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
          %55 = polygeist.load %27[%c1, %arg6, %37, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %56 = polygeist.load %27[%c1, %arg6, %39, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %57 = arith.subf %55, %56 : f32
          %58 = arith.mulf %51, %57 : f32
          %59 = arith.mulf %58, %cst_4 : f32
          %60 = arith.addf %50, %59 : f32
          polygeist.store %60, %28[%c1, %arg6, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
          memref.store %cst_1, %alloca_5[] : memref<f32>
          scf.for %arg7 = %c-2 to %c3 step %c1 {
            %67 = arith.index_cast %arg7 : index to i32
            %68 = arith.addi %67, %c2_i32 : i32
            %69 = arith.index_cast %68 : i32 to index
            %70 = memref.load %alloca[%69] : memref<5xf32>
            %71 = arith.addi %40, %67 : i32
            %72 = arith.index_cast %71 : i32 to index
            %73 = polygeist.load %27[%c2, %72, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %74 = arith.addi %35, %67 : i32
            %75 = arith.index_cast %74 : i32 to index
            %76 = polygeist.load %27[%c2, %arg6, %75, %29] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %77 = arith.addf %73, %76 : f32
            %78 = arith.addi %30, %67 : i32
            %79 = arith.index_cast %78 : i32 to index
            %80 = polygeist.load %27[%c2, %arg6, %arg5, %79] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %81 = arith.addf %77, %80 : f32
            %82 = arith.mulf %70, %81 : f32
            %83 = memref.load %alloca_5[] : memref<f32>
            %84 = arith.addf %83, %82 : f32
            memref.store %84, %alloca_5[] : memref<f32>
          } {arts.id = 229 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
          %61 = polygeist.load %27[%c2, %arg6, %arg5, %32] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %62 = polygeist.load %27[%c2, %arg6, %arg5, %34] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %63 = arith.subf %61, %62 : f32
          %64 = arith.mulf %51, %63 : f32
          %65 = arith.mulf %64, %cst_4 : f32
          %66 = arith.addf %50, %65 : f32
          polygeist.store %66, %28[%c2, %arg6, %arg5, %29] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
        } {arts.id = 245 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 247 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 260 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @mainBody() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2_i32 = arith.constant 2 : i32
    %c4_i32 = arith.constant 4 : i32
    %c261000_i64 = arith.constant 261000 : i64
    %c3_i32 = arith.constant 3 : i32
    %c2 = arith.constant 2 : index
    %c8 = arith.constant 8 : index
    %c235929600_i64 = arith.constant 235929600 : i64
    %c707788800_i64 = arith.constant 707788800 : i64
    %c41000_i64 = arith.constant 41000 : i64
    %c39000_i64 = arith.constant 39000 : i64
    %c37000_i64 = arith.constant 37000 : i64
    %0 = llvm.mlir.zero : !llvm.ptr
    %c35000_i64 = arith.constant 35000 : i64
    %c6_i32 = arith.constant 6 : i32
    %1 = llvm.mlir.undef : !llvm.struct<(i32, i64)>
    %c0_i64 = arith.constant 0 : i64
    %c64 = arith.constant 64 : index
    %c9 = arith.constant 9 : index
    %c10 = arith.constant 10 : index
    %c572 = arith.constant 572 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 5.000000e-02 : f32
    %c17_i32 = arith.constant 17 : i32
    %c23_i32 = arith.constant 23 : i32
    %cst_0 = arith.constant 1.000000e-01 : f32
    %cst_1 = arith.constant 0.000000e+00 : f32
    %c1_i32 = arith.constant 1 : i32
    %cst_2 = arith.constant 3.000000e+00 : f32
    %cst_3 = arith.constant 1.000000e-03 : f32
    %c11_i32 = arith.constant 11 : i32
    %cst_4 = arith.constant 2.000000e+00 : f32
    %cst_5 = arith.constant 1.500000e-03 : f32
    %c7_i32 = arith.constant 7 : i32
    %c576 = arith.constant 576 : index
    %c320 = arith.constant 320 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c3 = arith.constant 3 : index
    %2 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_6 = arith.constant 0.000000e+00 : f64
    %3 = llvm.mlir.addressof @str2 : !llvm.ptr
    %4 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %5 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 253 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:158:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 253 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %6 = "polygeist.undef"() : () -> f64
    memref.store %6, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %7 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%7) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %4 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    %alloc = memref.alloc() : memref<1xi64>
    %alloc_7 = memref.alloc() : memref<1x!llvm.ptr>
    %9 = call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
    memref.store %9, %alloc[%c0] : memref<1xi64>
    %10 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %11 = llvm.insertvalue %c0_i32, %1[0] : !llvm.struct<(i32, i64)> 
    %12 = llvm.insertvalue %c35000_i64, %11[1] : !llvm.struct<(i32, i64)> 
    llvm.store %12, %10 : !llvm.struct<(i32, i64)>, !llvm.ptr
    %13 = polygeist.pointer2memref %10 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
    %14 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    %15 = call @arts_db_create_with_guid(%9, %c707788800_i64, %c0_i32, %14, %13) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    memref.store %15, %alloc_7[%c0] : memref<1x!llvm.ptr>
    %alloc_8 = memref.alloc() : memref<1xi64>
    %alloc_9 = memref.alloc() : memref<1x!llvm.ptr>
    %16 = call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
    memref.store %16, %alloc_8[%c0] : memref<1xi64>
    %17 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %18 = llvm.insertvalue %c37000_i64, %11[1] : !llvm.struct<(i32, i64)> 
    llvm.store %18, %17 : !llvm.struct<(i32, i64)>, !llvm.ptr
    %19 = polygeist.pointer2memref %17 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
    %20 = call @arts_db_create_with_guid(%16, %c707788800_i64, %c0_i32, %14, %19) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    memref.store %20, %alloc_9[%c0] : memref<1x!llvm.ptr>
    %alloc_10 = memref.alloc() : memref<1xi64>
    %alloc_11 = memref.alloc() : memref<1x!llvm.ptr>
    %21 = call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
    memref.store %21, %alloc_10[%c0] : memref<1xi64>
    %22 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %23 = llvm.insertvalue %c39000_i64, %11[1] : !llvm.struct<(i32, i64)> 
    llvm.store %23, %22 : !llvm.struct<(i32, i64)>, !llvm.ptr
    %24 = polygeist.pointer2memref %22 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
    %25 = call @arts_db_create_with_guid(%21, %c235929600_i64, %c0_i32, %14, %24) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    memref.store %25, %alloc_11[%c0] : memref<1x!llvm.ptr>
    %alloc_12 = memref.alloc() : memref<1xi64>
    %alloc_13 = memref.alloc() : memref<1x!llvm.ptr>
    %26 = call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
    memref.store %26, %alloc_12[%c0] : memref<1xi64>
    %27 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %28 = llvm.insertvalue %c41000_i64, %11[1] : !llvm.struct<(i32, i64)> 
    llvm.store %28, %27 : !llvm.struct<(i32, i64)>, !llvm.ptr
    %29 = polygeist.pointer2memref %27 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
    %30 = call @arts_db_create_with_guid(%26, %c235929600_i64, %c0_i32, %14, %29) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    memref.store %30, %alloc_13[%c0] : memref<1x!llvm.ptr>
    %31 = "polygeist.undef"() : () -> i32
    %alloca_14 = memref.alloca() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:144:3", totalAccesses = 11 : i64, readCount = 5 : i64, writeCount = 6 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 44 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %31, %alloca_14[] : memref<i32>
    memref.store %c0_i32, %alloca_14[] : memref<i32>
    %32 = polygeist.memref2pointer %alloc_7 : memref<1x!llvm.ptr> to !llvm.ptr
    %33 = polygeist.memref2pointer %alloc_9 : memref<1x!llvm.ptr> to !llvm.ptr
    %34 = llvm.load %32 : !llvm.ptr -> !llvm.ptr
    %35 = polygeist.pointer2memref %34 : !llvm.ptr to memref<?x?x?x?xf32>
    %36 = llvm.load %33 : !llvm.ptr -> !llvm.ptr
    %37 = polygeist.pointer2memref %36 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %49 = arith.index_cast %arg0 : index to i32
      memref.store %c0_i32, %alloca_14[] : memref<i32>
      %50 = arith.muli %49, %c17_i32 : i32
      %51 = arith.sitofp %49 : i32 to f32
      %52 = arith.mulf %51, %cst_0 : f32
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c320 step %c1 {
          scf.for %arg3 = %c0 to %c576 step %c1 {
            %53 = memref.load %alloca_14[] : memref<i32>
            %54 = arith.addi %53, %50 : i32
            %55 = arith.remsi %54, %c23_i32 : i32
            %56 = arith.sitofp %55 : i32 to f32
            %57 = arith.mulf %56, %cst : f32
            %58 = arith.subf %57, %52 : f32
            polygeist.store %58, %35[%arg0, %arg1, %arg2, %arg3] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
            polygeist.store %cst_1, %37[%arg0, %arg1, %arg2, %arg3] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
            %59 = memref.load %alloca_14[] : memref<i32>
            %60 = arith.addi %59, %c1_i32 : i32
            memref.store %60, %alloca_14[] : memref<i32>
          } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 70 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    memref.store %c0_i32, %alloca_14[] : memref<i32>
    %38 = polygeist.memref2pointer %alloc_11 : memref<1x!llvm.ptr> to !llvm.ptr
    %39 = polygeist.memref2pointer %alloc_13 : memref<1x!llvm.ptr> to !llvm.ptr
    %40 = llvm.load %38 : !llvm.ptr -> !llvm.ptr
    %41 = polygeist.pointer2memref %40 : !llvm.ptr to memref<?x?x?xf32>
    %42 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
    %43 = polygeist.pointer2memref %42 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg0 = %c0 to %c320 step %c1 {
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c576 step %c1 {
          %49 = memref.load %alloca_14[] : memref<i32>
          %50 = arith.remsi %49, %c11_i32 : i32
          %51 = arith.sitofp %50 : i32 to f32
          %52 = arith.mulf %51, %cst_3 : f32
          %53 = arith.addf %52, %cst_2 : f32
          polygeist.store %53, %41[%arg0, %arg1, %arg2] sizes(%c320, %c320, %c576) : f32, memref<?x?x?xf32>
          %54 = memref.load %alloca_14[] : memref<i32>
          %55 = arith.remsi %54, %c7_i32 : i32
          %56 = arith.sitofp %55 : i32 to f32
          %57 = arith.mulf %56, %cst_5 : f32
          %58 = arith.addf %57, %cst_4 : f32
          polygeist.store %58, %43[%arg0, %arg1, %arg2] sizes(%c320, %c320, %c576) : f32, memref<?x?x?xf32>
          %59 = memref.load %alloca_14[] : memref<i32>
          %60 = arith.addi %59, %c1_i32 : i32
          memref.store %60, %alloca_14[] : memref<i32>
        } {arts.id = 88 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 90 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%7) : (memref<?xi8>) -> ()
    %alloca_15 = memref.alloca() : memref<4xi64>
    %44 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_16 = memref.alloca() : memref<i32>
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %49 = func.call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
      scf.for %arg1 = %c0 to %c64 step %c1 {
        %54 = arith.muli %arg1, %c9 : index
        %55 = arith.cmpi uge, %54, %c572 : index
        %56 = arith.subi %c572, %54 : index
        %57 = arith.select %55, %c0, %56 : index
        %58 = arith.minui %57, %c9 : index
        %59 = arith.cmpi ugt, %58, %c0 : index
        scf.if %59 {
          %cast = memref.cast %alloca_15 : memref<4xi64> to memref<?xi64>
          %60 = arith.divui %54, %c576 : index
          %61 = arith.addi %54, %c8 : index
          %62 = arith.divui %61, %c576 : index
          %63 = arith.cmpi ugt, %60, %c0 : index
          %64 = arith.select %63, %62, %c0 : index
          %65 = arith.subi %64, %60 : index
          %66 = arith.addi %65, %c1 : index
          %67 = arith.select %63, %c0, %60 : index
          %68 = arith.select %63, %c0, %66 : index
          %69 = arith.index_cast %54 : index to i64
          memref.store %69, %alloca_15[%c0] : memref<4xi64>
          %70 = arith.index_cast %56 : index to i64
          memref.store %70, %alloca_15[%c1] : memref<4xi64>
          %71 = arith.index_cast %58 : index to i64
          memref.store %71, %alloca_15[%c2] : memref<4xi64>
          %72 = arith.index_cast %68 : index to i64
          memref.store %72, %alloca_15[%c3] : memref<4xi64>
          %73 = arith.index_cast %68 : index to i32
          %74 = arith.addi %73, %c3_i32 : i32
          %75 = polygeist.get_func @__arts_edt_1 : !llvm.ptr
          %76 = polygeist.pointer2memref %75 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
          %77 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
          %78 = llvm.insertvalue %c261000_i64, %77[1] : !llvm.struct<(i32, i64)> 
          llvm.store %78, %44 : !llvm.struct<(i32, i64)>, !llvm.ptr
          %79 = polygeist.pointer2memref %44 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
          %80 = func.call @arts_edt_create_with_epoch(%76, %c4_i32, %cast, %74, %49, %79) {arts.create_id = 261000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
          memref.store %c0_i32, %alloca_16[] : memref<i32>
          %81 = memref.load %alloca_16[] : memref<i32>
          %82 = polygeist.memref2pointer %alloc_10 : memref<1xi64> to !llvm.ptr
          %83 = llvm.load %82 : !llvm.ptr -> i64
          func.call @arts_add_dependence(%83, %80, %81, %c1_i32) : (i64, i64, i32, i32) -> ()
          %84 = arith.addi %81, %c1_i32 : i32
          memref.store %84, %alloca_16[] : memref<i32>
          %85 = memref.load %alloca_16[] : memref<i32>
          %86 = polygeist.memref2pointer %alloc_12 : memref<1xi64> to !llvm.ptr
          %87 = llvm.load %86 : !llvm.ptr -> i64
          func.call @arts_add_dependence(%87, %80, %85, %c1_i32) : (i64, i64, i32, i32) -> ()
          %88 = arith.addi %85, %c1_i32 : i32
          memref.store %88, %alloca_16[] : memref<i32>
          %89 = memref.load %alloca_16[] : memref<i32>
          %90 = polygeist.memref2pointer %alloc : memref<1xi64> to !llvm.ptr
          %91 = llvm.load %90 : !llvm.ptr -> i64
          func.call @arts_add_dependence(%91, %80, %89, %c1_i32) : (i64, i64, i32, i32) -> ()
          %92 = arith.addi %89, %c1_i32 : i32
          memref.store %92, %alloca_16[] : memref<i32>
          %93 = arith.addi %67, %68 : index
          scf.for %arg2 = %67 to %93 step %c1 {
            %94 = arith.cmpi ult, %arg2, %c1 : index
            %95 = memref.load %alloca_16[] : memref<i32>
            scf.if %94 {
              %97 = polygeist.memref2pointer %alloc_8 : memref<1xi64> to !llvm.ptr
              %98 = arith.index_cast %arg2 : index to i64
              %99 = llvm.getelementptr %97[%98] : (!llvm.ptr, i64) -> !llvm.ptr, i64
              %100 = llvm.load %99 : !llvm.ptr -> i64
              func.call @arts_add_dependence(%100, %80, %95, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              func.call @arts_signal_edt_null(%80, %95) : (i64, i32) -> ()
            }
            %96 = arith.addi %95, %c1_i32 : i32
            memref.store %96, %alloca_16[] : memref<i32>
          }
        }
      }
      %50 = func.call @arts_wait_on_handle(%49) : (i64) -> i1
      %51 = memref.load %alloc[%c0] : memref<1xi64>
      func.call @arts_db_release(%51) : (i64) -> ()
      %52 = memref.load %alloc_12[%c0] : memref<1xi64>
      func.call @arts_db_release(%52) : (i64) -> ()
      %53 = memref.load %alloc_10[%c0] : memref<1xi64>
      func.call @arts_db_release(%53) : (i64) -> ()
      func.call @carts_kernel_timer_accum(%7) : (memref<?xi8>) -> ()
    } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 9 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:149:3">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_print(%7) : (memref<?xi8>) -> ()
    %45 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%45, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_6, %alloca[] : memref<f64>
    %46 = llvm.load %33 : !llvm.ptr -> !llvm.ptr
    %47 = polygeist.pointer2memref %46 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c320 step %c1 {
      scf.for %arg1 = %c0 to %c3 step %c1 {
        %49 = polygeist.load %47[%arg1, %arg0, %arg0, %arg0] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
        %50 = arith.extf %49 : f32 to f64
        %51 = math.absf %50 : f64
        %52 = memref.load %alloca[] : memref<f64>
        %53 = arith.addf %52, %51 : f64
        memref.store %53, %alloca[] : memref<f64>
      } {arts.id = 258 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:163:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 212 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:162:3">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%cst_6) : (f64) -> ()
    call @carts_phase_timer_stop(%45) : (memref<?xi8>) -> ()
    %48 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%48, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%48) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_8 : memref<1xi64>
    memref.dealloc %alloc_9 : memref<1x!llvm.ptr>
    memref.dealloc %alloc_12 : memref<1xi64>
    memref.dealloc %alloc_13 : memref<1x!llvm.ptr>
    memref.dealloc %alloc : memref<1xi64>
    memref.dealloc %alloc_7 : memref<1x!llvm.ptr>
    memref.dealloc %alloc_10 : memref<1xi64>
    memref.dealloc %alloc_11 : memref<1x!llvm.ptr>
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
