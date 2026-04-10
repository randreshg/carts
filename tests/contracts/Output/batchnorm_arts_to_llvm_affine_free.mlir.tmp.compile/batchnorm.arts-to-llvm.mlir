module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @arts_guid_from_index(i64, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nofree, llvm.nosync, llvm.nounwind, llvm.readnone, llvm.willreturn, memory = #llvm.memory_effects<other = none, argMem = none, inaccessibleMem = none, errnoMem = none, targetMem0 = none, targetMem1 = none>, passthrough = ["nounwind", "nosync", "nofree", "willreturn"]}
  func.func private @arts_guid_reserve_range(i32 {llvm.signext}, i32 {llvm.zeroext}, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_rt(i32 {llvm.signext}, memref<?xmemref<?xi8>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> (i32 {llvm.signext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @artsSetConfigData(memref<?xi8> {llvm.nocapture, llvm.nofree, llvm.readonly}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  llvm.mlir.global internal constant @str_4("[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A\00") {addr_space = 0 : i32}
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
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = memref.load %arg1[%c1] : memref<?xi64>
    %2 = arith.trunci %0 : i64 to i32
    %3 = llvm.inttoptr %1 : i64 to !llvm.ptr
    %4 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xmemref<?xi8>>
    %5 = call @mainBody(%2, %4) : (i32, memref<?xmemref<?xi8>>) -> i32
    call @arts_shutdown() : () -> ()
    return
  }
  func.func private @arts_db_create_with_guid(i64, i64, i32 {llvm.signext}, memref<?xi8> {llvm.nocapture, llvm.nofree, llvm.readonly}, memref<?x!llvm.struct<(i32, i64)>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> (!llvm.ptr {llvm.noalias}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nosync, llvm.nounwind, llvm.willreturn, passthrough = ["nounwind", "nosync", "willreturn"]}
  func.func private @arts_guid_reserve(i32 {llvm.signext}, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_wait_on_handle(i64) -> (i1 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_signal_edt_null(i64, i32 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_add_dependence_at(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}, i64, i64) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_add_dependence(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_edt_create_with_epoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>> {llvm.nocapture, llvm.nofree, llvm.readonly}, i32 {llvm.zeroext}, memref<?xi64> {llvm.nocapture, llvm.nofree, llvm.readonly}, i32 {llvm.zeroext}, i64, memref<?x!llvm.struct<(i32, i64)>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_initialize_and_start_epoch(i64, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_get_total_workers() -> (i32 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nofree, llvm.nosync, llvm.nounwind, llvm.readnone, llvm.willreturn, memory = #llvm.memory_effects<other = none, argMem = none, inaccessibleMem = none, errnoMem = none, targetMem0 = none, targetMem1 = none>, passthrough = ["nounwind", "nosync", "nofree", "willreturn"]}
  func.func private @__arts_edt_6(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %cst = arith.constant 0.000000e+00 : f32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.subi %c0, %1 : index
    %8 = arith.cmpi slt, %7, %c0 : index
    %9 = arith.select %8, %c0, %7 : index
    %10 = arith.cmpi slt, %3, %c0 : index
    %11 = arith.select %10, %c0, %3 : index
    %12 = arith.minui %11, %5 : index
    %13 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %14 = llvm.getelementptr %13[%6] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %15 = llvm.getelementptr %14[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %16 = llvm.load %15 : !llvm.ptr -> !llvm.ptr
    %17 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg4 = %9 to %12 step %c1 {
      %18 = arith.addi %1, %arg4 : index
      scf.for %arg5 = %c0 to %c64 step %c1 {
        scf.for %arg6 = %c0 to %c1024 step %c1 {
          %19 = polygeist.load %17[%18, %arg5, %arg6] sizes(%c4, %c64, %c1024) : memref<?x?x?xf32> -> f32
          %20 = arith.addf %19, %cst : f32
          polygeist.store %20, %17[%18, %arg5, %arg6] sizes(%c4, %c64, %c1024) : f32, memref<?x?x?xf32>
        } {arts.id = 146 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 148 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 161 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.subi %c0, %1 : index
    %8 = arith.cmpi slt, %7, %c0 : index
    %9 = arith.select %8, %c0, %7 : index
    %10 = arith.cmpi slt, %3, %c0 : index
    %11 = arith.select %10, %c0, %3 : index
    %12 = arith.minui %11, %5 : index
    %13 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %14 = llvm.getelementptr %13[%6] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %15 = llvm.getelementptr %14[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %16 = llvm.load %15 : !llvm.ptr -> !llvm.ptr
    %17 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg4 = %9 to %12 step %c1 {
      %18 = arith.addi %1, %arg4 : index
      scf.for %arg5 = %c0 to %c64 step %c1 {
        scf.for %arg6 = %c0 to %c1024 step %c1 {
          %19 = polygeist.load %17[%18, %arg5, %arg6] sizes(%c4, %c64, %c1024) : memref<?x?x?xf32> -> f32
          polygeist.store %19, %17[%18, %arg5, %arg6] sizes(%c4, %c64, %c1024) : f32, memref<?x?x?xf32>
        } {arts.id = 134 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 136 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 160 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c3 = arith.constant 3 : index
    %c31 = arith.constant 31 : index
    %c32 = arith.constant 32 : index
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %cst = arith.constant 9.99999974E-6 : f32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.index_cast %6 : i64 to index
    %8 = arith.addi %7, %c32 : index
    %9 = arith.subi %c0, %1 : index
    %10 = arith.cmpi slt, %9, %c0 : index
    %11 = arith.select %10, %c0, %9 : index
    %12 = arith.cmpi slt, %3, %c0 : index
    %13 = arith.select %12, %c0, %3 : index
    %14 = arith.minui %13, %5 : index
    %15 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %16 = llvm.getelementptr %15[32] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %17 = llvm.getelementptr %16[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %18 = llvm.load %17 : !llvm.ptr -> !llvm.ptr
    %19 = polygeist.pointer2memref %18 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg4 = %11 to %14 step %c1 {
      %20 = arith.addi %1, %arg4 : index
      scf.for %arg5 = %c0 to %c64 step %c1 {
        %21 = arith.divui %arg5, %c2 : index
        %22 = arith.remui %arg5, %c2 : index
        %23 = arith.minui %21, %c31 : index
        %24 = arith.index_cast %8 : index to i64
        %25 = arith.index_cast %23 : index to i64
        %26 = arith.addi %24, %25 : i64
        %27 = llvm.getelementptr %15[%26] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
        %28 = llvm.getelementptr %27[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
        %29 = llvm.getelementptr %15[%25] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
        %30 = llvm.getelementptr %29[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
        %31 = llvm.load %28 : !llvm.ptr -> !llvm.ptr
        %32 = llvm.load %30 : !llvm.ptr -> !llvm.ptr
        %33 = polygeist.pointer2memref %31 : !llvm.ptr to memref<?xf32>
        %34 = polygeist.pointer2memref %32 : !llvm.ptr to memref<?xf32>
        scf.for %arg6 = %c0 to %c1024 step %c1 {
          %35 = polygeist.load %19[%20, %arg5, %arg6] sizes(%c4, %c64, %c1024) : memref<?x?x?xf32> -> f32
          %36 = polygeist.load %33[%22] sizes(%c2) : memref<?xf32> -> f32
          %37 = arith.subf %35, %36 : f32
          %38 = polygeist.load %34[%22] sizes(%c2) : memref<?xf32> -> f32
          %39 = arith.addf %38, %cst : f32
          %40 = math.sqrt %39 : f32
          %41 = arith.divf %37, %40 : f32
          polygeist.store %41, %19[%20, %arg5, %arg6] sizes(%c4, %c64, %c1024) : f32, memref<?x?x?xf32>
        } {arts.id = 123 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 125 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 159 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c3 = arith.constant 3 : index
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f32
    %c4 = arith.constant 4 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %cst_0 = arith.constant 2.44200259E-4 : f32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.index_cast %6 : i64 to index
    %8 = memref.load %arg1[%c4] : memref<?xi64>
    %9 = arith.index_cast %8 : i64 to index
    %10 = memref.load %arg1[%c5] : memref<?xi64>
    %11 = arith.index_cast %10 : i64 to index
    %12 = memref.load %arg1[%c6] : memref<?xi64>
    %13 = arith.index_cast %12 : i64 to index
    %14 = memref.load %arg1[%c7] : memref<?xi64>
    %15 = arith.index_cast %14 : i64 to index
    %16 = arith.addi %11, %13 : index
    %17 = arith.subi %c0, %1 : index
    %18 = arith.cmpi slt, %17, %c0 : index
    %19 = arith.select %18, %c0, %17 : index
    %20 = arith.cmpi slt, %3, %c0 : index
    %21 = arith.select %20, %c0, %3 : index
    %22 = arith.minui %21, %5 : index
    %23 = arith.maxui %11, %c1 : index
    %24 = arith.subi %23, %c1 : index
    %25 = arith.cmpi eq, %11, %c1 : index
    %26 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %27 = llvm.getelementptr %26[%10] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %28 = llvm.getelementptr %27[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %29 = arith.maxui %15, %c1 : index
    %30 = arith.subi %29, %c1 : index
    %31 = arith.cmpi eq, %15, %c1 : index
    %32 = llvm.load %28 : !llvm.ptr -> !llvm.ptr
    %33 = polygeist.pointer2memref %32 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg4 = %19 to %22 step %c1 {
      %34 = arith.addi %1, %arg4 : index
      %35 = arith.divui %34, %c2 : index
      %36 = arith.subi %35, %7 : index
      %37 = arith.remui %34, %c2 : index
      %38 = arith.minui %36, %24 : index
      %39 = arith.select %25, %c0, %38 : index
      %40 = arith.index_cast %39 : index to i64
      %41 = llvm.getelementptr %26[%40] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %42 = llvm.getelementptr %41[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %43 = llvm.load %42 : !llvm.ptr -> !llvm.ptr
      %44 = polygeist.pointer2memref %43 : !llvm.ptr to memref<?xf32>
      polygeist.store %cst, %44[%37] sizes(%c2) : f32, memref<?xf32>
      %45 = arith.cmpi ult, %35, %9 : index
      %46 = arith.subi %35, %9 : index
      %47 = arith.select %45, %c0, %46 : index
      %48 = arith.minui %47, %30 : index
      %49 = arith.select %31, %c0, %48 : index
      %50 = arith.index_cast %16 : index to i64
      %51 = arith.index_cast %49 : index to i64
      %52 = arith.addi %50, %51 : i64
      %53 = llvm.getelementptr %26[%52] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %54 = llvm.getelementptr %53[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %55 = llvm.load %54 : !llvm.ptr -> !llvm.ptr
      %56 = llvm.load %42 : !llvm.ptr -> !llvm.ptr
      %57 = polygeist.pointer2memref %55 : !llvm.ptr to memref<?xf32>
      %58 = polygeist.pointer2memref %56 : !llvm.ptr to memref<?xf32>
      scf.for %arg5 = %c0 to %c4 step %c1 {
        %65 = polygeist.load %58[%37] sizes(%c2) : memref<?xf32> -> f32
        %66 = scf.for %arg6 = %c0 to %c1024 step %c1 iter_args(%arg7 = %65) -> (f32) {
          %67 = polygeist.load %33[%arg5, %34, %arg6] sizes(%c4, %c64, %c1024) : memref<?x?x?xf32> -> f32
          %68 = polygeist.load %57[%37] sizes(%c2) : memref<?xf32> -> f32
          %69 = arith.subf %67, %68 : f32
          %70 = arith.mulf %69, %69 : f32
          %71 = arith.addf %arg7, %70 : f32
          scf.yield %71 : f32
        } {arts.id = 103 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 2 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
        polygeist.store %66, %58[%37] sizes(%c2) : f32, memref<?xf32>
      } {arts.id = 105 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 2 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      %59 = llvm.load %42 : !llvm.ptr -> !llvm.ptr
      %60 = polygeist.pointer2memref %59 : !llvm.ptr to memref<?xf32>
      %61 = polygeist.load %60[%37] sizes(%c2) : memref<?xf32> -> f32
      %62 = arith.mulf %61, %cst_0 : f32
      %63 = llvm.load %42 : !llvm.ptr -> !llvm.ptr
      %64 = polygeist.pointer2memref %63 : !llvm.ptr to memref<?xf32>
      polygeist.store %62, %64[%37] sizes(%c2) : f32, memref<?xf32>
    } {arts.id = 80 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c3 = arith.constant 3 : index
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 0.000000e+00 : f32
    %c4 = arith.constant 4 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %cst_0 = arith.constant 2.44140625E-4 : f32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.index_cast %6 : i64 to index
    %8 = memref.load %arg1[%c4] : memref<?xi64>
    %9 = arith.index_cast %8 : i64 to index
    %10 = memref.load %arg1[%c5] : memref<?xi64>
    %11 = arith.index_cast %10 : i64 to index
    %12 = memref.load %arg1[%c6] : memref<?xi64>
    %13 = arith.index_cast %12 : i64 to index
    %14 = arith.addi %9, %11 : index
    %15 = arith.subi %c0, %1 : index
    %16 = arith.cmpi slt, %15, %c0 : index
    %17 = arith.select %16, %c0, %15 : index
    %18 = arith.cmpi slt, %3, %c0 : index
    %19 = arith.select %18, %c0, %3 : index
    %20 = arith.minui %19, %5 : index
    %21 = arith.maxui %13, %c1 : index
    %22 = arith.subi %21, %c1 : index
    %23 = arith.cmpi eq, %13, %c1 : index
    %24 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %25 = llvm.getelementptr %24[%8] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %26 = llvm.getelementptr %25[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %27 = llvm.load %26 : !llvm.ptr -> !llvm.ptr
    %28 = polygeist.pointer2memref %27 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg4 = %17 to %20 step %c1 {
      %29 = arith.addi %1, %arg4 : index
      %30 = arith.divui %29, %c2 : index
      %31 = arith.subi %30, %7 : index
      %32 = arith.remui %29, %c2 : index
      %33 = arith.minui %31, %22 : index
      %34 = arith.select %23, %c0, %33 : index
      %35 = arith.index_cast %14 : index to i64
      %36 = arith.index_cast %34 : index to i64
      %37 = arith.addi %35, %36 : i64
      %38 = llvm.getelementptr %24[%37] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %39 = llvm.getelementptr %38[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %40 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
      %41 = polygeist.pointer2memref %40 : !llvm.ptr to memref<?xf32>
      polygeist.store %cst, %41[%32] sizes(%c2) : f32, memref<?xf32>
      %42 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
      %43 = polygeist.pointer2memref %42 : !llvm.ptr to memref<?xf32>
      scf.for %arg5 = %c0 to %c4 step %c1 {
        %50 = polygeist.load %43[%32] sizes(%c2) : memref<?xf32> -> f32
        %51 = scf.for %arg6 = %c0 to %c1024 step %c1 iter_args(%arg7 = %50) -> (f32) {
          %52 = polygeist.load %28[%arg5, %29, %arg6] sizes(%c4, %c64, %c1024) : memref<?x?x?xf32> -> f32
          %53 = arith.addf %arg7, %52 : f32
          scf.yield %53 : f32
        } {arts.id = 78 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
        polygeist.store %51, %43[%32] sizes(%c2) : f32, memref<?xf32>
      } {arts.id = 80 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      %44 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
      %45 = polygeist.pointer2memref %44 : !llvm.ptr to memref<?xf32>
      %46 = polygeist.load %45[%32] sizes(%c2) : memref<?xf32> -> f32
      %47 = arith.mulf %46, %cst_0 : f32
      %48 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
      %49 = polygeist.pointer2memref %48 : !llvm.ptr to memref<?xf32>
      polygeist.store %47, %49[%32] sizes(%c2) : f32, memref<?xf32>
    } {arts.id = 158 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    return
  }
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %c4 = arith.constant 4 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.index_cast %6 : i64 to index
    %8 = memref.load %arg1[%c4] : memref<?xi64>
    %9 = arith.index_cast %8 : i64 to index
    %10 = memref.load %arg1[%c5] : memref<?xi64>
    %11 = arith.index_cast %10 : i64 to index
    %12 = memref.load %arg1[%c6] : memref<?xi64>
    %13 = arith.index_cast %12 : i64 to index
    %14 = arith.subi %c0, %1 : index
    %15 = arith.cmpi slt, %14, %c0 : index
    %16 = arith.select %15, %c0, %14 : index
    %17 = arith.cmpi slt, %3, %c0 : index
    %18 = arith.select %17, %c0, %3 : index
    %19 = arith.minui %18, %5 : index
    %20 = arith.maxui %7, %c1 : index
    %21 = arith.maxui %13, %c1 : index
    %22 = arith.subi %21, %c1 : index
    %23 = arith.cmpi eq, %13, %c1 : index
    %24 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
    %25 = llvm.getelementptr %24[%12] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %26 = llvm.getelementptr %25[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %27 = llvm.load %26 : !llvm.ptr -> !llvm.ptr
    %28 = polygeist.pointer2memref %27 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg4 = %16 to %19 step %c1 {
      %29 = arith.addi %1, %arg4 : index
      %30 = arith.divui %29, %20 : index
      %31 = arith.remui %29, %20 : index
      %32 = arith.cmpi ult, %30, %9 : index
      %33 = arith.subi %30, %9 : index
      %34 = arith.select %32, %c0, %33 : index
      %35 = arith.minui %34, %22 : index
      %36 = arith.select %23, %c0, %35 : index
      %37 = arith.index_cast %36 : index to i64
      %38 = llvm.getelementptr %24[%37] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %39 = llvm.getelementptr %38[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
      %40 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
      %41 = polygeist.pointer2memref %40 : !llvm.ptr to memref<?x?x?xf32>
      scf.for %arg5 = %c0 to %c64 step %c1 {
        scf.for %arg6 = %c0 to %c1024 step %c1 {
          %42 = polygeist.load %41[%31, %arg5, %arg6] sizes(%11, %c64, %c1024) : memref<?x?x?xf32> -> f32
          polygeist.store %42, %28[%29, %arg5, %arg6] sizes(%c4, %c64, %c1024) : f32, memref<?x?x?xf32>
        } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 157 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "batchnorm.c:280:5">, arts.metadata_provenance = "exact"}
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("batchnorm\00") {addr_space = 0 : i32}
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c167000_i64 = arith.constant 167000 : i64
    %c166000_i64 = arith.constant 166000 : i64
    %c4_i32 = arith.constant 4 : i32
    %c165000_i64 = arith.constant 165000 : i64
    %c64_i32 = arith.constant 64 : i32
    %c164000_i64 = arith.constant 164000 : i64
    %c7_i32 = arith.constant 7 : i32
    %c163000_i64 = arith.constant 163000 : i64
    %c162000_i64 = arith.constant 162000 : i64
    %c1048576_i64 = arith.constant 1048576 : i64
    %c38000_i64 = arith.constant 38000 : i64
    %c37000_i64 = arith.constant 37000 : i64
    %c35000_i64 = arith.constant 35000 : i64
    %0 = llvm.mlir.zero : !llvm.ptr
    %c33000_i64 = arith.constant 33000 : i64
    %c6_i32 = arith.constant 6 : i32
    %c5_i32 = arith.constant 5 : i32
    %c8_i64 = arith.constant 8 : i64
    %c2_i32 = arith.constant 2 : i32
    %1 = llvm.mlir.undef : !llvm.struct<(i32, i64)>
    %c8_i32 = arith.constant 8 : i32
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c0_i64 = arith.constant 0 : i64
    %c32_i32 = arith.constant 32 : i32
    %c8 = arith.constant 8 : index
    %c262144 = arith.constant 262144 : index
    %true = arith.constant true
    %c3_i64 = arith.constant 3 : i64
    %c3 = arith.constant 3 : index
    %c31 = arith.constant 31 : index
    %c32 = arith.constant 32 : index
    %c2 = arith.constant 2 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 2.621440e+05 : f32
    %c1024 = arith.constant 1024 : index
    %c64 = arith.constant 64 : index
    %c4 = arith.constant 4 : index
    %c1_i32 = arith.constant 1 : i32
    %cst_0 = arith.constant 1.000000e+00 : f32
    %cst_1 = arith.constant 2.000000e+00 : f32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %2 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_2 = arith.constant 0.000000e+00 : f64
    %3 = llvm.mlir.addressof @str2 : !llvm.ptr
    %4 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %5 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 154 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "batchnorm.c:288:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 154 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %6 = "polygeist.undef"() : () -> f64
    memref.store %6, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %7 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%7) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %4 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    %9 = call @arts_get_total_workers() : () -> i32
    %10 = arith.index_castui %9 : i32 to index
    %11 = arith.maxui %10, %c1 : index
    %12 = arith.addi %11, %c3 : index
    %13 = arith.divui %12, %11 : index
    %14 = arith.maxui %13, %c1 : index
    %15 = arith.maxui %14, %c1 : index
    %16 = arith.index_cast %15 : index to i64
    %17 = arith.addi %16, %c3_i64 : i64
    %18 = arith.divui %17, %16 : i64
    %19 = arith.index_cast %18 : i64 to index
    %20 = arith.muli %15, %c262144 : index
    %alloc = memref.alloc(%19) : memref<?xi64>
    %alloc_3 = memref.alloc(%19) : memref<?x!llvm.ptr>
    %21 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    scf.for %arg2 = %c0 to %19 step %c1 {
      %56 = func.call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
      memref.store %56, %alloc[%arg2] : memref<?xi64>
      %57 = arith.index_cast %20 : index to i64
      %58 = arith.index_cast %arg2 : index to i64
      %59 = arith.addi %58, %c33000_i64 : i64
      %60 = llvm.insertvalue %c0_i32, %1[0] : !llvm.struct<(i32, i64)> 
      %61 = llvm.insertvalue %59, %60[1] : !llvm.struct<(i32, i64)> 
      llvm.store %61, %21 : !llvm.struct<(i32, i64)>, !llvm.ptr
      %62 = polygeist.pointer2memref %21 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
      %63 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      %64 = func.call @arts_db_create_with_guid(%56, %57, %c0_i32, %63, %62) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
      memref.store %64, %alloc_3[%arg2] : memref<?x!llvm.ptr>
    }
    %alloc_4 = memref.alloc() : memref<1xi64>
    %alloc_5 = memref.alloc() : memref<1x!llvm.ptr>
    %22 = call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
    memref.store %22, %alloc_4[%c0] : memref<1xi64>
    %23 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %24 = llvm.insertvalue %c0_i32, %1[0] : !llvm.struct<(i32, i64)> 
    %25 = llvm.insertvalue %c35000_i64, %24[1] : !llvm.struct<(i32, i64)> 
    llvm.store %25, %23 : !llvm.struct<(i32, i64)>, !llvm.ptr
    %26 = polygeist.pointer2memref %23 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
    %27 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    %28 = call @arts_db_create_with_guid(%22, %c1048576_i64, %c0_i32, %27, %26) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    memref.store %28, %alloc_5[%c0] : memref<1x!llvm.ptr>
    %alloc_6 = memref.alloc() : memref<32xi64>
    %29 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    scf.for %arg2 = %c0 to %c32 step %c1 {
      %56 = func.call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
      memref.store %56, %alloc_6[%arg2] : memref<32xi64>
      %57 = arith.index_cast %arg2 : index to i64
      %58 = arith.addi %57, %c37000_i64 : i64
      %59 = llvm.insertvalue %58, %24[1] : !llvm.struct<(i32, i64)> 
      llvm.store %59, %29 : !llvm.struct<(i32, i64)>, !llvm.ptr
      %60 = polygeist.pointer2memref %29 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
      %61 = func.call @arts_db_create_with_guid(%56, %c8_i64, %c0_i32, %27, %60) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    }
    %alloc_7 = memref.alloc() : memref<32xi64>
    %30 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    scf.for %arg2 = %c0 to %c32 step %c1 {
      %56 = func.call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
      memref.store %56, %alloc_7[%arg2] : memref<32xi64>
      %57 = arith.index_cast %arg2 : index to i64
      %58 = arith.addi %57, %c38000_i64 : i64
      %59 = llvm.insertvalue %58, %24[1] : !llvm.struct<(i32, i64)> 
      llvm.store %59, %30 : !llvm.struct<(i32, i64)>, !llvm.ptr
      %60 = polygeist.pointer2memref %30 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
      %61 = func.call @arts_db_create_with_guid(%56, %c8_i64, %c0_i32, %27, %60) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
    }
    %alloca_8 = memref.alloca() {arts.id = 40 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "batchnorm.c:273:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 40 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    %31 = "polygeist.undef"() : () -> i32
    memref.store %31, %alloca_8[] : memref<i32>
    memref.store %c0_i32, %alloca_8[] : memref<i32>
    %32 = arith.maxui %15, %c1 : index
    scf.for %arg2 = %c0 to %c4 step %c1 {
      %56 = arith.divui %arg2, %32 : index
      %57 = arith.remui %arg2, %32 : index
      %58 = polygeist.memref2pointer %alloc_3 : memref<?x!llvm.ptr> to !llvm.ptr
      %59 = arith.index_cast %56 : index to i64
      %60 = llvm.getelementptr %58[%59] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %61 = llvm.load %60 : !llvm.ptr -> !llvm.ptr
      %62 = polygeist.pointer2memref %61 : !llvm.ptr to memref<?x?x?xf32>
      scf.for %arg3 = %c0 to %c64 step %c1 {
        scf.for %arg4 = %c0 to %c1024 step %c1 {
          %63 = memref.load %alloca_8[] : memref<i32>
          %64 = arith.sitofp %63 : i32 to f32
          %65 = arith.divf %64, %cst : f32
          %66 = arith.mulf %65, %cst_1 : f32
          %67 = arith.subf %66, %cst_0 : f32
          polygeist.store %67, %62[%57, %arg3, %arg4] sizes(%15, %c64, %c1024) : f32, memref<?x?x?xf32>
          %68 = memref.load %alloca_8[] : memref<i32>
          %69 = arith.addi %68, %c1_i32 : i32
          memref.store %69, %alloca_8[] : memref<i32>
        } {arts.id = 54 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:273:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 56 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 64 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:273:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 58 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:273:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%7) : (memref<?xi8>) -> ()
    %33 = call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
    %alloca_9 = memref.alloca() : memref<8xi64>
    %34 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_10 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %56 = arith.muli %arg2, %c2 : index
      %57 = arith.cmpi uge, %56, %c4 : index
      %58 = arith.subi %c4, %56 : index
      %59 = arith.select %57, %c0, %58 : index
      %60 = arith.minui %59, %c2 : index
      %61 = arith.cmpi ugt, %60, %c0 : index
      scf.if %61 {
        %cast = memref.cast %alloca_9 : memref<8xi64> to memref<?xi64>
        %62 = arith.addi %56, %60 : index
        %63 = arith.cmpi uge, %56, %c1 : index
        %64 = arith.subi %56, %c1 : index
        %65 = arith.select %63, %64, %c0 : index
        %66 = arith.addi %62, %c1 : index
        %67 = arith.minui %66, %c4 : index
        %68 = arith.cmpi uge, %67, %65 : index
        %69 = arith.subi %67, %65 : index
        %70 = arith.select %68, %69, %c0 : index
        %71 = arith.divui %65, %32 : index
        %72 = arith.addi %65, %70 : index
        %73 = arith.subi %72, %c1 : index
        %74 = arith.divui %73, %32 : index
        %75 = arith.subi %19, %c1 : index
        %76 = arith.cmpi ugt, %71, %75 : index
        %77 = arith.minui %74, %75 : index
        %78 = arith.select %76, %74, %77 : index
        %79 = arith.subi %78, %71 : index
        %80 = arith.addi %79, %c1 : index
        %81 = arith.select %76, %c0, %71 : index
        %82 = arith.select %76, %c0, %80 : index
        %83 = arith.divui %56, %c4 : index
        %84 = arith.addi %56, %c1 : index
        %85 = arith.divui %84, %c4 : index
        %86 = arith.cmpi ugt, %83, %c0 : index
        %87 = arith.select %86, %85, %c0 : index
        %88 = arith.subi %87, %83 : index
        %89 = arith.addi %88, %c1 : index
        %90 = arith.select %86, %c0, %83 : index
        %91 = arith.select %86, %c0, %89 : index
        %92 = arith.divui %56, %c2 : index
        %93 = arith.divui %84, %c2 : index
        %94 = arith.cmpi ugt, %92, %c31 : index
        %95 = arith.minui %93, %c31 : index
        %96 = arith.select %94, %93, %95 : index
        %97 = arith.subi %96, %92 : index
        %98 = arith.addi %97, %c1 : index
        %99 = arith.select %94, %c0, %92 : index
        %100 = arith.select %94, %c0, %98 : index
        %101 = arith.index_cast %56 : index to i64
        memref.store %101, %alloca_9[%c0] : memref<8xi64>
        %102 = arith.index_cast %58 : index to i64
        memref.store %102, %alloca_9[%c1] : memref<8xi64>
        %103 = arith.index_cast %60 : index to i64
        memref.store %103, %alloca_9[%c2] : memref<8xi64>
        %104 = arith.index_cast %32 : index to i64
        memref.store %104, %alloca_9[%c3] : memref<8xi64>
        %105 = arith.index_cast %81 : index to i64
        memref.store %105, %alloca_9[%c4] : memref<8xi64>
        memref.store %16, %alloca_9[%c5] : memref<8xi64>
        %106 = arith.index_cast %82 : index to i64
        memref.store %106, %alloca_9[%c6] : memref<8xi64>
        %107 = arith.index_cast %91 : index to i64
        memref.store %107, %alloca_9[%c7] : memref<8xi64>
        %108 = arith.index_cast %82 : index to i32
        %109 = arith.index_cast %91 : index to i32
        %110 = arith.addi %108, %109 : i32
        %111 = arith.index_cast %100 : index to i32
        %112 = arith.addi %110, %111 : i32
        %113 = polygeist.get_func @__arts_edt_1 : !llvm.ptr
        %114 = polygeist.pointer2memref %113 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
        %115 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
        %116 = llvm.insertvalue %c162000_i64, %115[1] : !llvm.struct<(i32, i64)> 
        llvm.store %116, %34 : !llvm.struct<(i32, i64)>, !llvm.ptr
        %117 = polygeist.pointer2memref %34 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
        %118 = func.call @arts_edt_create_with_epoch(%114, %c8_i32, %cast, %112, %33, %117) {arts.create_id = 162000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
        memref.store %c0_i32, %alloca_10[] : memref<i32>
        %119 = arith.addi %81, %82 : index
        scf.for %arg3 = %81 to %119 step %c1 {
          %122 = arith.cmpi ult, %arg3, %19 : index
          %123 = memref.load %alloca_10[] : memref<i32>
          scf.if %122 {
            %125 = arith.cmpi ule, %72, %15 : index
            %126 = arith.cmpi ult, %65, %15 : index
            %127 = arith.muli %81, %15 : index
            %128 = arith.subi %65, %127 : index
            %129 = arith.cmpi uge, %65, %127 : index
            %130 = arith.select %129, %128, %c0 : index
            %131 = arith.minui %130, %15 : index
            %132 = arith.cmpi ule, %82, %c1 : index
            %133 = arith.select %132, %131, %c0 : index
            %134 = arith.andi %126, %125 : i1
            %135 = arith.select %134, %65, %133 : index
            %136 = arith.subi %72, %127 : index
            %137 = arith.cmpi uge, %72, %127 : index
            %138 = arith.select %137, %136, %c0 : index
            %139 = arith.minui %138, %15 : index
            %140 = arith.subi %139, %131 : index
            %141 = arith.cmpi uge, %139, %131 : index
            %142 = arith.select %141, %140, %c0 : index
            %143 = arith.select %132, %142, %15 : index
            %144 = arith.select %134, %70, %143 : index
            %145 = arith.cmpi eq, %144, %15 : index
            %146 = arith.cmpi eq, %135, %c0 : index
            %147 = arith.andi %146, %145 : i1
            %148 = arith.muli %82, %15 : index
            %149 = arith.addi %127, %148 : index
            %150 = arith.cmpi uge, %72, %149 : index
            %151 = arith.cmpi ule, %65, %127 : index
            %152 = arith.andi %151, %150 : i1
            %153 = arith.ori %132, %152 : i1
            %154 = arith.ori %134, %153 : i1
            %155 = arith.xori %147, %true : i1
            %156 = arith.muli %144, %c262144 : index
            %157 = arith.andi %155, %154 : i1
            %158 = arith.select %157, %156, %c0 : index
            %159 = arith.index_cast %158 : index to i64
            %160 = polygeist.memref2pointer %alloc : memref<?xi64> to !llvm.ptr
            %161 = arith.index_cast %arg3 : index to i64
            %162 = llvm.getelementptr %160[%161] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %163 = llvm.load %162 : !llvm.ptr -> i64
            %164 = arith.cmpi eq, %159, %c0_i64 : i64
            scf.if %164 {
              func.call @arts_add_dependence(%163, %118, %123, %c1_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %165 = arith.muli %135, %c262144 : index
              %166 = arith.select %157, %165, %c0 : index
              %167 = arith.index_cast %166 : index to i64
              func.call @arts_add_dependence_at(%163, %118, %123, %c1_i32, %167, %159) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%118, %123) : (i64, i32) -> ()
          }
          %124 = arith.addi %123, %c1_i32 : i32
          memref.store %124, %alloca_10[] : memref<i32>
        }
        %120 = arith.addi %90, %91 : index
        scf.for %arg3 = %90 to %120 step %c1 {
          %122 = arith.cmpi ult, %arg3, %c1 : index
          %123 = memref.load %alloca_10[] : memref<i32>
          scf.if %122 {
            %125 = arith.addi %56, %c2 : index
            %126 = arith.cmpi ule, %125, %c4 : index
            %127 = arith.cmpi ult, %56, %c4 : index
            %128 = arith.muli %90, %c4 : index
            %129 = arith.subi %56, %128 : index
            %130 = arith.cmpi uge, %56, %128 : index
            %131 = arith.select %130, %129, %c0 : index
            %132 = arith.minui %131, %c4 : index
            %133 = arith.cmpi ule, %91, %c1 : index
            %134 = arith.select %133, %132, %c0 : index
            %135 = arith.andi %127, %126 : i1
            %136 = arith.select %135, %56, %134 : index
            %137 = arith.subi %125, %128 : index
            %138 = arith.cmpi uge, %125, %128 : index
            %139 = arith.select %138, %137, %c0 : index
            %140 = arith.minui %139, %c4 : index
            %141 = arith.subi %140, %132 : index
            %142 = arith.cmpi uge, %140, %132 : index
            %143 = arith.select %142, %141, %c0 : index
            %144 = arith.select %133, %143, %c4 : index
            %145 = arith.select %135, %c2, %144 : index
            %146 = arith.cmpi eq, %145, %c4 : index
            %147 = arith.cmpi eq, %136, %c0 : index
            %148 = arith.andi %147, %146 : i1
            %149 = arith.muli %91, %c4 : index
            %150 = arith.addi %128, %149 : index
            %151 = arith.cmpi uge, %125, %150 : index
            %152 = arith.cmpi ule, %56, %128 : index
            %153 = arith.andi %152, %151 : i1
            %154 = arith.ori %133, %153 : i1
            %155 = arith.ori %135, %154 : i1
            %156 = arith.xori %148, %true : i1
            %157 = arith.muli %145, %c262144 : index
            %158 = arith.andi %156, %155 : i1
            %159 = arith.select %158, %157, %c0 : index
            %160 = arith.index_cast %159 : index to i64
            %161 = polygeist.memref2pointer %alloc_4 : memref<1xi64> to !llvm.ptr
            %162 = arith.index_cast %arg3 : index to i64
            %163 = llvm.getelementptr %161[%162] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %164 = llvm.load %163 : !llvm.ptr -> i64
            %165 = arith.cmpi eq, %160, %c0_i64 : i64
            scf.if %165 {
              func.call @arts_add_dependence(%164, %118, %123, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %166 = arith.muli %136, %c262144 : index
              %167 = arith.select %158, %166, %c0 : index
              %168 = arith.index_cast %167 : index to i64
              func.call @arts_add_dependence_at(%164, %118, %123, %c2_i32, %168, %160) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%118, %123) : (i64, i32) -> ()
          }
          %124 = arith.addi %123, %c1_i32 : i32
          memref.store %124, %alloca_10[] : memref<i32>
        }
        %121 = arith.addi %99, %100 : index
        scf.for %arg3 = %99 to %121 step %c1 {
          %122 = arith.cmpi ult, %arg3, %c32 : index
          %123 = memref.load %alloca_10[] : memref<i32>
          scf.if %122 {
            %125 = arith.muli %arg2, %c8 : index
            %126 = arith.index_cast %125 : index to i64
            %127 = polygeist.memref2pointer %alloc_6 : memref<32xi64> to !llvm.ptr
            %128 = arith.index_cast %arg3 : index to i64
            %129 = llvm.getelementptr %127[%128] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %130 = llvm.load %129 : !llvm.ptr -> i64
            func.call @arts_add_dependence_at(%130, %118, %123, %c2_i32, %126, %c8_i64) : (i64, i64, i32, i32, i64, i64) -> ()
          } else {
            func.call @arts_signal_edt_null(%118, %123) : (i64, i32) -> ()
          }
          %124 = arith.addi %123, %c1_i32 : i32
          memref.store %124, %alloca_10[] : memref<i32>
        }
      }
    }
    %35 = call @arts_wait_on_handle(%33) : (i64) -> i1
    %36 = call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
    %alloca_11 = memref.alloca() : memref<7xi64>
    %37 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_12 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %56 = arith.muli %arg2, %c32 : index
      %57 = arith.cmpi uge, %56, %c64 : index
      %58 = arith.subi %c64, %56 : index
      %59 = arith.select %57, %c0, %58 : index
      %60 = arith.minui %59, %c32 : index
      %61 = arith.cmpi ugt, %60, %c0 : index
      scf.if %61 {
        %cast = memref.cast %alloca_11 : memref<7xi64> to memref<?xi64>
        %62 = arith.divui %56, %c4 : index
        %63 = arith.addi %56, %c31 : index
        %64 = arith.divui %63, %c4 : index
        %65 = arith.cmpi ugt, %62, %c0 : index
        %66 = arith.select %65, %64, %c0 : index
        %67 = arith.subi %66, %62 : index
        %68 = arith.addi %67, %c1 : index
        %69 = arith.select %65, %c0, %62 : index
        %70 = arith.select %65, %c0, %68 : index
        %71 = arith.divui %56, %c2 : index
        %72 = arith.cmpi ugt, %71, %c31 : index
        %73 = arith.select %72, %c0, %71 : index
        %74 = arith.divui %63, %c2 : index
        %75 = arith.minui %74, %c31 : index
        %76 = arith.select %72, %74, %75 : index
        %77 = arith.subi %76, %71 : index
        %78 = arith.addi %77, %c1 : index
        %79 = arith.select %72, %c0, %78 : index
        %80 = arith.index_cast %56 : index to i64
        memref.store %80, %alloca_11[%c0] : memref<7xi64>
        %81 = arith.index_cast %58 : index to i64
        memref.store %81, %alloca_11[%c1] : memref<7xi64>
        %82 = arith.index_cast %60 : index to i64
        memref.store %82, %alloca_11[%c2] : memref<7xi64>
        %83 = arith.index_cast %73 : index to i64
        memref.store %83, %alloca_11[%c3] : memref<7xi64>
        memref.store %18, %alloca_11[%c4] : memref<7xi64>
        %84 = arith.index_cast %70 : index to i64
        memref.store %84, %alloca_11[%c5] : memref<7xi64>
        %85 = arith.index_cast %79 : index to i64
        memref.store %85, %alloca_11[%c6] : memref<7xi64>
        %86 = arith.index_cast %19 : index to i32
        %87 = arith.index_cast %70 : index to i32
        %88 = arith.addi %86, %87 : i32
        %89 = arith.index_cast %79 : index to i32
        %90 = arith.addi %88, %89 : i32
        %91 = polygeist.get_func @__arts_edt_2 : !llvm.ptr
        %92 = polygeist.pointer2memref %91 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
        %93 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
        %94 = llvm.insertvalue %c163000_i64, %93[1] : !llvm.struct<(i32, i64)> 
        llvm.store %94, %37 : !llvm.struct<(i32, i64)>, !llvm.ptr
        %95 = polygeist.pointer2memref %37 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
        %96 = func.call @arts_edt_create_with_epoch(%92, %c7_i32, %cast, %90, %36, %95) {arts.create_id = 163000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
        memref.store %c0_i32, %alloca_12[] : memref<i32>
        scf.for %arg3 = %c0 to %19 step %c1 {
          %99 = arith.cmpi ult, %arg3, %19 : index
          %100 = memref.load %alloca_12[] : memref<i32>
          scf.if %99 {
            %102 = polygeist.memref2pointer %alloc : memref<?xi64> to !llvm.ptr
            %103 = arith.index_cast %arg3 : index to i64
            %104 = llvm.getelementptr %102[%103] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %105 = llvm.load %104 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%105, %96, %100, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%96, %100) : (i64, i32) -> ()
          }
          %101 = arith.addi %100, %c1_i32 : i32
          memref.store %101, %alloca_12[] : memref<i32>
        }
        %97 = arith.addi %69, %70 : index
        scf.for %arg3 = %69 to %97 step %c1 {
          %99 = arith.cmpi ult, %arg3, %c1 : index
          %100 = memref.load %alloca_12[] : memref<i32>
          scf.if %99 {
            %102 = polygeist.memref2pointer %alloc_4 : memref<1xi64> to !llvm.ptr
            %103 = arith.index_cast %arg3 : index to i64
            %104 = llvm.getelementptr %102[%103] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %105 = llvm.load %104 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%105, %96, %100, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%96, %100) : (i64, i32) -> ()
          }
          %101 = arith.addi %100, %c1_i32 : i32
          memref.store %101, %alloca_12[] : memref<i32>
        }
        %98 = arith.addi %73, %79 : index
        scf.for %arg3 = %73 to %98 step %c1 {
          %99 = arith.cmpi ult, %arg3, %c32 : index
          %100 = memref.load %alloca_12[] : memref<i32>
          scf.if %99 {
            %102 = arith.addi %56, %c32 : index
            %103 = arith.cmpi ule, %102, %c2 : index
            %104 = arith.cmpi ult, %56, %c2 : index
            %105 = arith.muli %73, %c2 : index
            %106 = arith.subi %56, %105 : index
            %107 = arith.cmpi uge, %56, %105 : index
            %108 = arith.select %107, %106, %c0 : index
            %109 = arith.minui %108, %c2 : index
            %110 = arith.cmpi ule, %79, %c1 : index
            %111 = arith.select %110, %109, %c0 : index
            %112 = arith.andi %104, %103 : i1
            %113 = arith.select %112, %56, %111 : index
            %114 = arith.subi %102, %105 : index
            %115 = arith.cmpi uge, %102, %105 : index
            %116 = arith.select %115, %114, %c0 : index
            %117 = arith.minui %116, %c2 : index
            %118 = arith.subi %117, %109 : index
            %119 = arith.cmpi uge, %117, %109 : index
            %120 = arith.select %119, %118, %c0 : index
            %121 = arith.select %110, %120, %c2 : index
            %122 = arith.select %112, %c32, %121 : index
            %123 = arith.cmpi eq, %122, %c2 : index
            %124 = arith.cmpi eq, %113, %c0 : index
            %125 = arith.andi %124, %123 : i1
            %126 = arith.muli %79, %c2 : index
            %127 = arith.addi %105, %126 : index
            %128 = arith.cmpi uge, %102, %127 : index
            %129 = arith.cmpi ule, %56, %105 : index
            %130 = arith.andi %129, %128 : i1
            %131 = arith.ori %110, %130 : i1
            %132 = arith.ori %112, %131 : i1
            %133 = arith.xori %125, %true : i1
            %134 = arith.muli %122, %c4 : index
            %135 = arith.andi %133, %132 : i1
            %136 = arith.select %135, %134, %c0 : index
            %137 = arith.index_cast %136 : index to i64
            %138 = polygeist.memref2pointer %alloc_6 : memref<32xi64> to !llvm.ptr
            %139 = arith.index_cast %arg3 : index to i64
            %140 = llvm.getelementptr %138[%139] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %141 = llvm.load %140 : !llvm.ptr -> i64
            %142 = arith.cmpi eq, %137, %c0_i64 : i64
            scf.if %142 {
              func.call @arts_add_dependence(%141, %96, %100, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %143 = arith.muli %113, %c4 : index
              %144 = arith.select %135, %143, %c0 : index
              %145 = arith.index_cast %144 : index to i64
              func.call @arts_add_dependence_at(%141, %96, %100, %c2_i32, %145, %137) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%96, %100) : (i64, i32) -> ()
          }
          %101 = arith.addi %100, %c1_i32 : i32
          memref.store %101, %alloca_12[] : memref<i32>
        }
      }
    }
    %38 = call @arts_wait_on_handle(%36) : (i64) -> i1
    %39 = call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
    %alloca_13 = memref.alloca() : memref<8xi64>
    %40 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_14 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %56 = arith.muli %arg2, %c32 : index
      %57 = arith.cmpi uge, %56, %c64 : index
      %58 = arith.subi %c64, %56 : index
      %59 = arith.select %57, %c0, %58 : index
      %60 = arith.minui %59, %c32 : index
      %61 = arith.cmpi ugt, %60, %c0 : index
      scf.if %61 {
        %cast = memref.cast %alloca_13 : memref<8xi64> to memref<?xi64>
        %62 = arith.addi %56, %c31 : index
        %63 = arith.divui %56, %c2 : index
        %64 = arith.cmpi ugt, %63, %c31 : index
        %65 = arith.select %64, %c0, %63 : index
        %66 = arith.divui %62, %c2 : index
        %67 = arith.minui %66, %c31 : index
        %68 = arith.select %64, %66, %67 : index
        %69 = arith.subi %68, %63 : index
        %70 = arith.addi %69, %c1 : index
        %71 = arith.select %64, %c0, %70 : index
        %72 = arith.divui %56, %c4 : index
        %73 = arith.divui %62, %c4 : index
        %74 = arith.cmpi ugt, %72, %c0 : index
        %75 = arith.select %74, %73, %c0 : index
        %76 = arith.subi %75, %72 : index
        %77 = arith.addi %76, %c1 : index
        %78 = arith.select %74, %c0, %72 : index
        %79 = arith.select %74, %c0, %77 : index
        %80 = arith.addi %56, %60 : index
        %81 = arith.cmpi uge, %56, %c1 : index
        %82 = arith.subi %56, %c1 : index
        %83 = arith.select %81, %82, %c0 : index
        %84 = arith.addi %80, %c1 : index
        %85 = arith.minui %84, %c64 : index
        %86 = arith.cmpi uge, %85, %83 : index
        %87 = arith.subi %85, %83 : index
        %88 = arith.select %86, %87, %c0 : index
        %89 = arith.divui %83, %c2 : index
        %90 = arith.addi %83, %88 : index
        %91 = arith.subi %90, %c1 : index
        %92 = arith.divui %91, %c2 : index
        %93 = arith.cmpi ugt, %89, %c31 : index
        %94 = arith.minui %92, %c31 : index
        %95 = arith.select %93, %92, %94 : index
        %96 = arith.subi %95, %89 : index
        %97 = arith.addi %96, %c1 : index
        %98 = arith.select %93, %c0, %89 : index
        %99 = arith.select %93, %c0, %97 : index
        %100 = arith.index_cast %56 : index to i64
        memref.store %100, %alloca_13[%c0] : memref<8xi64>
        %101 = arith.index_cast %58 : index to i64
        memref.store %101, %alloca_13[%c1] : memref<8xi64>
        %102 = arith.index_cast %60 : index to i64
        memref.store %102, %alloca_13[%c2] : memref<8xi64>
        %103 = arith.index_cast %65 : index to i64
        memref.store %103, %alloca_13[%c3] : memref<8xi64>
        %104 = arith.index_cast %98 : index to i64
        memref.store %104, %alloca_13[%c4] : memref<8xi64>
        %105 = arith.index_cast %71 : index to i64
        memref.store %105, %alloca_13[%c5] : memref<8xi64>
        %106 = arith.index_cast %79 : index to i64
        memref.store %106, %alloca_13[%c6] : memref<8xi64>
        %107 = arith.index_cast %99 : index to i64
        memref.store %107, %alloca_13[%c7] : memref<8xi64>
        %108 = arith.index_cast %71 : index to i32
        %109 = arith.index_cast %79 : index to i32
        %110 = arith.addi %108, %109 : i32
        %111 = arith.index_cast %99 : index to i32
        %112 = arith.addi %110, %111 : i32
        %113 = polygeist.get_func @__arts_edt_3 : !llvm.ptr
        %114 = polygeist.pointer2memref %113 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
        %115 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
        %116 = llvm.insertvalue %c164000_i64, %115[1] : !llvm.struct<(i32, i64)> 
        llvm.store %116, %40 : !llvm.struct<(i32, i64)>, !llvm.ptr
        %117 = polygeist.pointer2memref %40 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
        %118 = func.call @arts_edt_create_with_epoch(%114, %c8_i32, %cast, %112, %39, %117) {arts.create_id = 164000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
        memref.store %c0_i32, %alloca_14[] : memref<i32>
        %119 = arith.addi %65, %71 : index
        scf.for %arg3 = %65 to %119 step %c1 {
          %122 = arith.cmpi ult, %arg3, %c32 : index
          %123 = memref.load %alloca_14[] : memref<i32>
          scf.if %122 {
            %125 = arith.addi %56, %c32 : index
            %126 = arith.cmpi ule, %125, %c2 : index
            %127 = arith.cmpi ult, %56, %c2 : index
            %128 = arith.muli %65, %c2 : index
            %129 = arith.subi %56, %128 : index
            %130 = arith.cmpi uge, %56, %128 : index
            %131 = arith.select %130, %129, %c0 : index
            %132 = arith.minui %131, %c2 : index
            %133 = arith.cmpi ule, %71, %c1 : index
            %134 = arith.select %133, %132, %c0 : index
            %135 = arith.andi %127, %126 : i1
            %136 = arith.select %135, %56, %134 : index
            %137 = arith.subi %125, %128 : index
            %138 = arith.cmpi uge, %125, %128 : index
            %139 = arith.select %138, %137, %c0 : index
            %140 = arith.minui %139, %c2 : index
            %141 = arith.subi %140, %132 : index
            %142 = arith.cmpi uge, %140, %132 : index
            %143 = arith.select %142, %141, %c0 : index
            %144 = arith.select %133, %143, %c2 : index
            %145 = arith.select %135, %c32, %144 : index
            %146 = arith.cmpi eq, %145, %c2 : index
            %147 = arith.cmpi eq, %136, %c0 : index
            %148 = arith.andi %147, %146 : i1
            %149 = arith.muli %71, %c2 : index
            %150 = arith.addi %128, %149 : index
            %151 = arith.cmpi uge, %125, %150 : index
            %152 = arith.cmpi ule, %56, %128 : index
            %153 = arith.andi %152, %151 : i1
            %154 = arith.ori %133, %153 : i1
            %155 = arith.ori %135, %154 : i1
            %156 = arith.xori %148, %true : i1
            %157 = arith.muli %145, %c4 : index
            %158 = arith.andi %156, %155 : i1
            %159 = arith.select %158, %157, %c0 : index
            %160 = arith.index_cast %159 : index to i64
            %161 = polygeist.memref2pointer %alloc_7 : memref<32xi64> to !llvm.ptr
            %162 = arith.index_cast %arg3 : index to i64
            %163 = llvm.getelementptr %161[%162] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %164 = llvm.load %163 : !llvm.ptr -> i64
            %165 = arith.cmpi eq, %160, %c0_i64 : i64
            scf.if %165 {
              func.call @arts_add_dependence(%164, %118, %123, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %166 = arith.muli %136, %c4 : index
              %167 = arith.select %158, %166, %c0 : index
              %168 = arith.index_cast %167 : index to i64
              func.call @arts_add_dependence_at(%164, %118, %123, %c2_i32, %168, %160) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%118, %123) : (i64, i32) -> ()
          }
          %124 = arith.addi %123, %c1_i32 : i32
          memref.store %124, %alloca_14[] : memref<i32>
        }
        %120 = arith.addi %78, %79 : index
        scf.for %arg3 = %78 to %120 step %c1 {
          %122 = arith.cmpi ult, %arg3, %c1 : index
          %123 = memref.load %alloca_14[] : memref<i32>
          scf.if %122 {
            %125 = polygeist.memref2pointer %alloc_4 : memref<1xi64> to !llvm.ptr
            %126 = arith.index_cast %arg3 : index to i64
            %127 = llvm.getelementptr %125[%126] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %128 = llvm.load %127 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%128, %118, %123, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%118, %123) : (i64, i32) -> ()
          }
          %124 = arith.addi %123, %c1_i32 : i32
          memref.store %124, %alloca_14[] : memref<i32>
        }
        %121 = arith.addi %98, %99 : index
        scf.for %arg3 = %98 to %121 step %c1 {
          %122 = arith.cmpi ult, %arg3, %c32 : index
          %123 = memref.load %alloca_14[] : memref<i32>
          scf.if %122 {
            %125 = arith.cmpi ule, %90, %c2 : index
            %126 = arith.cmpi ult, %83, %c2 : index
            %127 = arith.muli %98, %c2 : index
            %128 = arith.subi %83, %127 : index
            %129 = arith.cmpi uge, %83, %127 : index
            %130 = arith.select %129, %128, %c0 : index
            %131 = arith.minui %130, %c2 : index
            %132 = arith.cmpi ule, %99, %c1 : index
            %133 = arith.select %132, %131, %c0 : index
            %134 = arith.andi %126, %125 : i1
            %135 = arith.select %134, %83, %133 : index
            %136 = arith.subi %90, %127 : index
            %137 = arith.cmpi uge, %90, %127 : index
            %138 = arith.select %137, %136, %c0 : index
            %139 = arith.minui %138, %c2 : index
            %140 = arith.subi %139, %131 : index
            %141 = arith.cmpi uge, %139, %131 : index
            %142 = arith.select %141, %140, %c0 : index
            %143 = arith.select %132, %142, %c2 : index
            %144 = arith.select %134, %88, %143 : index
            %145 = arith.cmpi eq, %144, %c2 : index
            %146 = arith.cmpi eq, %135, %c0 : index
            %147 = arith.andi %146, %145 : i1
            %148 = arith.muli %99, %c2 : index
            %149 = arith.addi %127, %148 : index
            %150 = arith.cmpi uge, %90, %149 : index
            %151 = arith.cmpi ule, %83, %127 : index
            %152 = arith.andi %151, %150 : i1
            %153 = arith.ori %132, %152 : i1
            %154 = arith.ori %134, %153 : i1
            %155 = arith.xori %147, %true : i1
            %156 = arith.muli %144, %c4 : index
            %157 = arith.andi %155, %154 : i1
            %158 = arith.select %157, %156, %c0 : index
            %159 = arith.index_cast %158 : index to i64
            %160 = polygeist.memref2pointer %alloc_6 : memref<32xi64> to !llvm.ptr
            %161 = arith.index_cast %arg3 : index to i64
            %162 = llvm.getelementptr %160[%161] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %163 = llvm.load %162 : !llvm.ptr -> i64
            %164 = arith.cmpi eq, %159, %c0_i64 : i64
            scf.if %164 {
              func.call @arts_add_dependence(%163, %118, %123, %c1_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %165 = arith.muli %135, %c4 : index
              %166 = arith.select %157, %165, %c0 : index
              %167 = arith.index_cast %166 : index to i64
              func.call @arts_add_dependence_at(%163, %118, %123, %c1_i32, %167, %159) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%118, %123) : (i64, i32) -> ()
          }
          %124 = arith.addi %123, %c1_i32 : i32
          memref.store %124, %alloca_14[] : memref<i32>
        }
      }
    }
    %41 = call @arts_wait_on_handle(%39) : (i64) -> i1
    %42 = call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
    %alloca_15 = memref.alloca() : memref<4xi64>
    %43 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_16 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %56 = arith.muli %arg2, %c2 : index
      %57 = arith.cmpi uge, %56, %c4 : index
      %58 = arith.subi %c4, %56 : index
      %59 = arith.select %57, %c0, %58 : index
      %60 = arith.minui %59, %c2 : index
      %61 = arith.cmpi ugt, %60, %c0 : index
      scf.if %61 {
        %cast = memref.cast %alloca_15 : memref<4xi64> to memref<?xi64>
        %62 = arith.divui %56, %c4 : index
        %63 = arith.addi %56, %c1 : index
        %64 = arith.divui %63, %c4 : index
        %65 = arith.cmpi ugt, %62, %c0 : index
        %66 = arith.select %65, %64, %c0 : index
        %67 = arith.subi %66, %62 : index
        %68 = arith.addi %67, %c1 : index
        %69 = arith.select %65, %c0, %62 : index
        %70 = arith.select %65, %c0, %68 : index
        %71 = arith.index_cast %56 : index to i64
        memref.store %71, %alloca_15[%c0] : memref<4xi64>
        %72 = arith.index_cast %58 : index to i64
        memref.store %72, %alloca_15[%c1] : memref<4xi64>
        %73 = arith.index_cast %60 : index to i64
        memref.store %73, %alloca_15[%c2] : memref<4xi64>
        %74 = arith.index_cast %70 : index to i64
        memref.store %74, %alloca_15[%c3] : memref<4xi64>
        %75 = arith.index_cast %70 : index to i32
        %76 = arith.addi %75, %c64_i32 : i32
        %77 = polygeist.get_func @__arts_edt_4 : !llvm.ptr
        %78 = polygeist.pointer2memref %77 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
        %79 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
        %80 = llvm.insertvalue %c165000_i64, %79[1] : !llvm.struct<(i32, i64)> 
        llvm.store %80, %43 : !llvm.struct<(i32, i64)>, !llvm.ptr
        %81 = polygeist.pointer2memref %43 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
        %82 = func.call @arts_edt_create_with_epoch(%78, %c4_i32, %cast, %76, %42, %81) {arts.create_id = 165000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
        memref.store %c0_i32, %alloca_16[] : memref<i32>
        scf.for %arg3 = %c0 to %c32 step %c1 {
          %84 = arith.cmpi ult, %arg3, %c32 : index
          %85 = memref.load %alloca_16[] : memref<i32>
          scf.if %84 {
            %87 = polygeist.memref2pointer %alloc_7 : memref<32xi64> to !llvm.ptr
            %88 = arith.index_cast %arg3 : index to i64
            %89 = llvm.getelementptr %87[%88] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %90 = llvm.load %89 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%90, %82, %85, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%82, %85) : (i64, i32) -> ()
          }
          %86 = arith.addi %85, %c1_i32 : i32
          memref.store %86, %alloca_16[] : memref<i32>
        }
        %83 = arith.addi %69, %70 : index
        scf.for %arg3 = %69 to %83 step %c1 {
          %84 = arith.cmpi ult, %arg3, %c1 : index
          %85 = memref.load %alloca_16[] : memref<i32>
          scf.if %84 {
            %87 = arith.addi %56, %c2 : index
            %88 = arith.cmpi ule, %87, %c4 : index
            %89 = arith.cmpi ult, %56, %c4 : index
            %90 = arith.muli %69, %c4 : index
            %91 = arith.subi %56, %90 : index
            %92 = arith.cmpi uge, %56, %90 : index
            %93 = arith.select %92, %91, %c0 : index
            %94 = arith.minui %93, %c4 : index
            %95 = arith.cmpi ule, %70, %c1 : index
            %96 = arith.select %95, %94, %c0 : index
            %97 = arith.andi %89, %88 : i1
            %98 = arith.select %97, %56, %96 : index
            %99 = arith.subi %87, %90 : index
            %100 = arith.cmpi uge, %87, %90 : index
            %101 = arith.select %100, %99, %c0 : index
            %102 = arith.minui %101, %c4 : index
            %103 = arith.subi %102, %94 : index
            %104 = arith.cmpi uge, %102, %94 : index
            %105 = arith.select %104, %103, %c0 : index
            %106 = arith.select %95, %105, %c4 : index
            %107 = arith.select %97, %c2, %106 : index
            %108 = arith.cmpi eq, %107, %c4 : index
            %109 = arith.cmpi eq, %98, %c0 : index
            %110 = arith.andi %109, %108 : i1
            %111 = arith.muli %70, %c4 : index
            %112 = arith.addi %90, %111 : index
            %113 = arith.cmpi uge, %87, %112 : index
            %114 = arith.cmpi ule, %56, %90 : index
            %115 = arith.andi %114, %113 : i1
            %116 = arith.ori %95, %115 : i1
            %117 = arith.ori %97, %116 : i1
            %118 = arith.xori %110, %true : i1
            %119 = arith.muli %107, %c262144 : index
            %120 = arith.andi %118, %117 : i1
            %121 = arith.select %120, %119, %c0 : index
            %122 = arith.index_cast %121 : index to i64
            %123 = polygeist.memref2pointer %alloc_4 : memref<1xi64> to !llvm.ptr
            %124 = arith.index_cast %arg3 : index to i64
            %125 = llvm.getelementptr %123[%124] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %126 = llvm.load %125 : !llvm.ptr -> i64
            %127 = arith.cmpi eq, %122, %c0_i64 : i64
            scf.if %127 {
              func.call @arts_add_dependence(%126, %82, %85, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %128 = arith.muli %98, %c262144 : index
              %129 = arith.select %120, %128, %c0 : index
              %130 = arith.index_cast %129 : index to i64
              func.call @arts_add_dependence_at(%126, %82, %85, %c2_i32, %130, %122) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%82, %85) : (i64, i32) -> ()
          }
          %86 = arith.addi %85, %c1_i32 : i32
          memref.store %86, %alloca_16[] : memref<i32>
        }
        scf.for %arg3 = %c0 to %c32 step %c1 {
          %84 = arith.cmpi ult, %arg3, %c32 : index
          %85 = memref.load %alloca_16[] : memref<i32>
          scf.if %84 {
            %87 = polygeist.memref2pointer %alloc_6 : memref<32xi64> to !llvm.ptr
            %88 = arith.index_cast %arg3 : index to i64
            %89 = llvm.getelementptr %87[%88] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %90 = llvm.load %89 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%90, %82, %85, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%82, %85) : (i64, i32) -> ()
          }
          %86 = arith.addi %85, %c1_i32 : i32
          memref.store %86, %alloca_16[] : memref<i32>
        }
      }
    }
    %44 = call @arts_wait_on_handle(%42) : (i64) -> i1
    %45 = call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
    %alloca_17 = memref.alloca() : memref<5xi64>
    %46 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_18 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %56 = arith.muli %arg2, %c2 : index
      %57 = arith.cmpi uge, %56, %c4 : index
      %58 = arith.subi %c4, %56 : index
      %59 = arith.select %57, %c0, %58 : index
      %60 = arith.minui %59, %c2 : index
      %61 = arith.cmpi ugt, %60, %c0 : index
      scf.if %61 {
        %cast = memref.cast %alloca_17 : memref<5xi64> to memref<?xi64>
        %62 = arith.divui %56, %c2 : index
        %63 = arith.addi %56, %c1 : index
        %64 = arith.divui %63, %c2 : index
        %65 = arith.cmpi ugt, %62, %c31 : index
        %66 = arith.minui %64, %c31 : index
        %67 = arith.select %65, %64, %66 : index
        %68 = arith.subi %67, %62 : index
        %69 = arith.addi %68, %c1 : index
        %70 = arith.select %65, %c0, %62 : index
        %71 = arith.select %65, %c0, %69 : index
        %72 = arith.divui %56, %c4 : index
        %73 = arith.divui %63, %c4 : index
        %74 = arith.cmpi ugt, %72, %c0 : index
        %75 = arith.select %74, %73, %c0 : index
        %76 = arith.subi %75, %72 : index
        %77 = arith.addi %76, %c1 : index
        %78 = arith.select %74, %c0, %72 : index
        %79 = arith.select %74, %c0, %77 : index
        %80 = arith.index_cast %56 : index to i64
        memref.store %80, %alloca_17[%c0] : memref<5xi64>
        %81 = arith.index_cast %58 : index to i64
        memref.store %81, %alloca_17[%c1] : memref<5xi64>
        %82 = arith.index_cast %60 : index to i64
        memref.store %82, %alloca_17[%c2] : memref<5xi64>
        %83 = arith.index_cast %71 : index to i64
        memref.store %83, %alloca_17[%c3] : memref<5xi64>
        %84 = arith.index_cast %79 : index to i64
        memref.store %84, %alloca_17[%c4] : memref<5xi64>
        %85 = arith.index_cast %71 : index to i32
        %86 = arith.index_cast %79 : index to i32
        %87 = arith.addi %85, %86 : i32
        %88 = arith.addi %87, %c32_i32 : i32
        %89 = polygeist.get_func @__arts_edt_5 : !llvm.ptr
        %90 = polygeist.pointer2memref %89 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
        %91 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
        %92 = llvm.insertvalue %c166000_i64, %91[1] : !llvm.struct<(i32, i64)> 
        llvm.store %92, %46 : !llvm.struct<(i32, i64)>, !llvm.ptr
        %93 = polygeist.pointer2memref %46 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
        %94 = func.call @arts_edt_create_with_epoch(%90, %c5_i32, %cast, %88, %45, %93) {arts.create_id = 166000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
        memref.store %c0_i32, %alloca_18[] : memref<i32>
        %95 = arith.addi %70, %71 : index
        scf.for %arg3 = %70 to %95 step %c1 {
          %97 = arith.cmpi ult, %arg3, %c32 : index
          %98 = memref.load %alloca_18[] : memref<i32>
          scf.if %97 {
            %100 = arith.muli %arg2, %c8 : index
            %101 = arith.index_cast %100 : index to i64
            %102 = polygeist.memref2pointer %alloc_7 : memref<32xi64> to !llvm.ptr
            %103 = arith.index_cast %arg3 : index to i64
            %104 = llvm.getelementptr %102[%103] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %105 = llvm.load %104 : !llvm.ptr -> i64
            func.call @arts_add_dependence_at(%105, %94, %98, %c2_i32, %101, %c8_i64) : (i64, i64, i32, i32, i64, i64) -> ()
          } else {
            func.call @arts_signal_edt_null(%94, %98) : (i64, i32) -> ()
          }
          %99 = arith.addi %98, %c1_i32 : i32
          memref.store %99, %alloca_18[] : memref<i32>
        }
        %96 = arith.addi %78, %79 : index
        scf.for %arg3 = %78 to %96 step %c1 {
          %97 = arith.cmpi ult, %arg3, %c1 : index
          %98 = memref.load %alloca_18[] : memref<i32>
          scf.if %97 {
            %100 = arith.addi %56, %c2 : index
            %101 = arith.cmpi ule, %100, %c4 : index
            %102 = arith.cmpi ult, %56, %c4 : index
            %103 = arith.muli %78, %c4 : index
            %104 = arith.subi %56, %103 : index
            %105 = arith.cmpi uge, %56, %103 : index
            %106 = arith.select %105, %104, %c0 : index
            %107 = arith.minui %106, %c4 : index
            %108 = arith.cmpi ule, %79, %c1 : index
            %109 = arith.select %108, %107, %c0 : index
            %110 = arith.andi %102, %101 : i1
            %111 = arith.select %110, %56, %109 : index
            %112 = arith.subi %100, %103 : index
            %113 = arith.cmpi uge, %100, %103 : index
            %114 = arith.select %113, %112, %c0 : index
            %115 = arith.minui %114, %c4 : index
            %116 = arith.subi %115, %107 : index
            %117 = arith.cmpi uge, %115, %107 : index
            %118 = arith.select %117, %116, %c0 : index
            %119 = arith.select %108, %118, %c4 : index
            %120 = arith.select %110, %c2, %119 : index
            %121 = arith.cmpi eq, %120, %c4 : index
            %122 = arith.cmpi eq, %111, %c0 : index
            %123 = arith.andi %122, %121 : i1
            %124 = arith.muli %79, %c4 : index
            %125 = arith.addi %103, %124 : index
            %126 = arith.cmpi uge, %100, %125 : index
            %127 = arith.cmpi ule, %56, %103 : index
            %128 = arith.andi %127, %126 : i1
            %129 = arith.ori %108, %128 : i1
            %130 = arith.ori %110, %129 : i1
            %131 = arith.xori %123, %true : i1
            %132 = arith.muli %120, %c262144 : index
            %133 = arith.andi %131, %130 : i1
            %134 = arith.select %133, %132, %c0 : index
            %135 = arith.index_cast %134 : index to i64
            %136 = polygeist.memref2pointer %alloc_4 : memref<1xi64> to !llvm.ptr
            %137 = arith.index_cast %arg3 : index to i64
            %138 = llvm.getelementptr %136[%137] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %139 = llvm.load %138 : !llvm.ptr -> i64
            %140 = arith.cmpi eq, %135, %c0_i64 : i64
            scf.if %140 {
              func.call @arts_add_dependence(%139, %94, %98, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %141 = arith.muli %111, %c262144 : index
              %142 = arith.select %133, %141, %c0 : index
              %143 = arith.index_cast %142 : index to i64
              func.call @arts_add_dependence_at(%139, %94, %98, %c2_i32, %143, %135) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%94, %98) : (i64, i32) -> ()
          }
          %99 = arith.addi %98, %c1_i32 : i32
          memref.store %99, %alloca_18[] : memref<i32>
        }
        scf.for %arg3 = %c0 to %c32 step %c1 {
          %97 = arith.cmpi ult, %arg3, %c32 : index
          %98 = memref.load %alloca_18[] : memref<i32>
          scf.if %97 {
            %100 = polygeist.memref2pointer %alloc_6 : memref<32xi64> to !llvm.ptr
            %101 = arith.index_cast %arg3 : index to i64
            %102 = llvm.getelementptr %100[%101] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %103 = llvm.load %102 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%103, %94, %98, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%94, %98) : (i64, i32) -> ()
          }
          %99 = arith.addi %98, %c1_i32 : i32
          memref.store %99, %alloca_18[] : memref<i32>
        }
      }
    }
    %47 = call @arts_wait_on_handle(%45) : (i64) -> i1
    %48 = call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
    %alloca_19 = memref.alloca() : memref<5xi64>
    %49 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_20 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %56 = arith.muli %arg2, %c2 : index
      %57 = arith.cmpi uge, %56, %c4 : index
      %58 = arith.subi %c4, %56 : index
      %59 = arith.select %57, %c0, %58 : index
      %60 = arith.minui %59, %c2 : index
      %61 = arith.cmpi ugt, %60, %c0 : index
      scf.if %61 {
        %cast = memref.cast %alloca_19 : memref<5xi64> to memref<?xi64>
        %62 = arith.divui %56, %c2 : index
        %63 = arith.addi %56, %c1 : index
        %64 = arith.divui %63, %c2 : index
        %65 = arith.cmpi ugt, %62, %c31 : index
        %66 = arith.minui %64, %c31 : index
        %67 = arith.select %65, %64, %66 : index
        %68 = arith.subi %67, %62 : index
        %69 = arith.addi %68, %c1 : index
        %70 = arith.select %65, %c0, %62 : index
        %71 = arith.select %65, %c0, %69 : index
        %72 = arith.divui %56, %c4 : index
        %73 = arith.divui %63, %c4 : index
        %74 = arith.cmpi ugt, %72, %c0 : index
        %75 = arith.select %74, %73, %c0 : index
        %76 = arith.subi %75, %72 : index
        %77 = arith.addi %76, %c1 : index
        %78 = arith.select %74, %c0, %72 : index
        %79 = arith.select %74, %c0, %77 : index
        %80 = arith.index_cast %56 : index to i64
        memref.store %80, %alloca_19[%c0] : memref<5xi64>
        %81 = arith.index_cast %58 : index to i64
        memref.store %81, %alloca_19[%c1] : memref<5xi64>
        %82 = arith.index_cast %60 : index to i64
        memref.store %82, %alloca_19[%c2] : memref<5xi64>
        %83 = arith.index_cast %71 : index to i64
        memref.store %83, %alloca_19[%c3] : memref<5xi64>
        %84 = arith.index_cast %79 : index to i64
        memref.store %84, %alloca_19[%c4] : memref<5xi64>
        %85 = arith.index_cast %71 : index to i32
        %86 = arith.index_cast %79 : index to i32
        %87 = arith.addi %85, %86 : i32
        %88 = arith.addi %87, %c32_i32 : i32
        %89 = polygeist.get_func @__arts_edt_6 : !llvm.ptr
        %90 = polygeist.pointer2memref %89 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
        %91 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
        %92 = llvm.insertvalue %c167000_i64, %91[1] : !llvm.struct<(i32, i64)> 
        llvm.store %92, %49 : !llvm.struct<(i32, i64)>, !llvm.ptr
        %93 = polygeist.pointer2memref %49 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
        %94 = func.call @arts_edt_create_with_epoch(%90, %c5_i32, %cast, %88, %48, %93) {arts.create_id = 167000 : i64} : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
        memref.store %c0_i32, %alloca_20[] : memref<i32>
        %95 = arith.addi %70, %71 : index
        scf.for %arg3 = %70 to %95 step %c1 {
          %97 = arith.cmpi ult, %arg3, %c32 : index
          %98 = memref.load %alloca_20[] : memref<i32>
          scf.if %97 {
            %100 = arith.muli %arg2, %c8 : index
            %101 = arith.index_cast %100 : index to i64
            %102 = polygeist.memref2pointer %alloc_7 : memref<32xi64> to !llvm.ptr
            %103 = arith.index_cast %arg3 : index to i64
            %104 = llvm.getelementptr %102[%103] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %105 = llvm.load %104 : !llvm.ptr -> i64
            func.call @arts_add_dependence_at(%105, %94, %98, %c2_i32, %101, %c8_i64) : (i64, i64, i32, i32, i64, i64) -> ()
          } else {
            func.call @arts_signal_edt_null(%94, %98) : (i64, i32) -> ()
          }
          %99 = arith.addi %98, %c1_i32 : i32
          memref.store %99, %alloca_20[] : memref<i32>
        }
        %96 = arith.addi %78, %79 : index
        scf.for %arg3 = %78 to %96 step %c1 {
          %97 = arith.cmpi ult, %arg3, %c1 : index
          %98 = memref.load %alloca_20[] : memref<i32>
          scf.if %97 {
            %100 = arith.addi %56, %c2 : index
            %101 = arith.cmpi ule, %100, %c4 : index
            %102 = arith.cmpi ult, %56, %c4 : index
            %103 = arith.muli %78, %c4 : index
            %104 = arith.subi %56, %103 : index
            %105 = arith.cmpi uge, %56, %103 : index
            %106 = arith.select %105, %104, %c0 : index
            %107 = arith.minui %106, %c4 : index
            %108 = arith.cmpi ule, %79, %c1 : index
            %109 = arith.select %108, %107, %c0 : index
            %110 = arith.andi %102, %101 : i1
            %111 = arith.select %110, %56, %109 : index
            %112 = arith.subi %100, %103 : index
            %113 = arith.cmpi uge, %100, %103 : index
            %114 = arith.select %113, %112, %c0 : index
            %115 = arith.minui %114, %c4 : index
            %116 = arith.subi %115, %107 : index
            %117 = arith.cmpi uge, %115, %107 : index
            %118 = arith.select %117, %116, %c0 : index
            %119 = arith.select %108, %118, %c4 : index
            %120 = arith.select %110, %c2, %119 : index
            %121 = arith.cmpi eq, %120, %c4 : index
            %122 = arith.cmpi eq, %111, %c0 : index
            %123 = arith.andi %122, %121 : i1
            %124 = arith.muli %79, %c4 : index
            %125 = arith.addi %103, %124 : index
            %126 = arith.cmpi uge, %100, %125 : index
            %127 = arith.cmpi ule, %56, %103 : index
            %128 = arith.andi %127, %126 : i1
            %129 = arith.ori %108, %128 : i1
            %130 = arith.ori %110, %129 : i1
            %131 = arith.xori %123, %true : i1
            %132 = arith.muli %120, %c262144 : index
            %133 = arith.andi %131, %130 : i1
            %134 = arith.select %133, %132, %c0 : index
            %135 = arith.index_cast %134 : index to i64
            %136 = polygeist.memref2pointer %alloc_4 : memref<1xi64> to !llvm.ptr
            %137 = arith.index_cast %arg3 : index to i64
            %138 = llvm.getelementptr %136[%137] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %139 = llvm.load %138 : !llvm.ptr -> i64
            %140 = arith.cmpi eq, %135, %c0_i64 : i64
            scf.if %140 {
              func.call @arts_add_dependence(%139, %94, %98, %c2_i32) : (i64, i64, i32, i32) -> ()
            } else {
              %141 = arith.muli %111, %c262144 : index
              %142 = arith.select %133, %141, %c0 : index
              %143 = arith.index_cast %142 : index to i64
              func.call @arts_add_dependence_at(%139, %94, %98, %c2_i32, %143, %135) : (i64, i64, i32, i32, i64, i64) -> ()
            }
          } else {
            func.call @arts_signal_edt_null(%94, %98) : (i64, i32) -> ()
          }
          %99 = arith.addi %98, %c1_i32 : i32
          memref.store %99, %alloca_20[] : memref<i32>
        }
        scf.for %arg3 = %c0 to %c32 step %c1 {
          %97 = arith.cmpi ult, %arg3, %c32 : index
          %98 = memref.load %alloca_20[] : memref<i32>
          scf.if %97 {
            %100 = polygeist.memref2pointer %alloc_6 : memref<32xi64> to !llvm.ptr
            %101 = arith.index_cast %arg3 : index to i64
            %102 = llvm.getelementptr %100[%101] : (!llvm.ptr, i64) -> !llvm.ptr, i64
            %103 = llvm.load %102 : !llvm.ptr -> i64
            func.call @arts_add_dependence(%103, %94, %98, %c1_i32) : (i64, i64, i32, i32) -> ()
          } else {
            func.call @arts_signal_edt_null(%94, %98) : (i64, i32) -> ()
          }
          %99 = arith.addi %98, %c1_i32 : i32
          memref.store %99, %alloca_20[] : memref<i32>
        }
      }
    }
    %50 = call @arts_wait_on_handle(%48) : (i64) -> i1
    call @carts_kernel_timer_accum(%7) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%7) : (memref<?xi8>) -> ()
    %51 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%51, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_2, %alloca[] : memref<f64>
    %52 = polygeist.memref2pointer %alloc_5 : memref<1x!llvm.ptr> to !llvm.ptr
    %53 = llvm.load %52 : !llvm.ptr -> !llvm.ptr
    %54 = polygeist.pointer2memref %53 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg2 = %c0 to %c4 step %c1 {
      %56 = polygeist.load %54[%arg2, %arg2, %arg2] sizes(%c4, %c64, %c1024) : memref<?x?x?xf32> -> f32
      %57 = arith.extf %56 : f32 to f64
      %58 = math.absf %57 : f64
      %59 = memref.load %alloca[] : memref<f64>
      %60 = arith.addf %59, %58 : f64
      memref.store %60, %alloca[] : memref<f64>
    } {arts.id = 135 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "batchnorm.c:292:3">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%cst_2) : (f64) -> ()
    call @carts_phase_timer_stop(%51) : (memref<?xi8>) -> ()
    %55 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%55, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%55) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.dealloc %alloc_7 : memref<32xi64>
    memref.dealloc %alloc : memref<?xi64>
    memref.dealloc %alloc_3 : memref<?x!llvm.ptr>
    memref.dealloc %alloc_6 : memref<32xi64>
    memref.dealloc %alloc_4 : memref<1xi64>
    memref.dealloc %alloc_5 : memref<1x!llvm.ptr>
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
