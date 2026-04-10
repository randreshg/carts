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
  func.func private @arts_add_dependence_at_ex(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}, i64, i64, i32 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_add_dependence_ex(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}, i32 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_edt_create_with_epoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>> {llvm.nocapture, llvm.nofree, llvm.readonly}, i32 {llvm.zeroext}, memref<?xi64> {llvm.nocapture, llvm.nofree, llvm.readonly}, i32 {llvm.zeroext}, i64, memref<?x!llvm.struct<(i32, i64)>> {llvm.nocapture, llvm.nofree, llvm.readonly}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_initialize_and_start_epoch(i64, i32 {llvm.zeroext}) -> i64 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, passthrough = ["nounwind"]}
  func.func private @arts_get_total_workers() -> (i32 {llvm.zeroext}) attributes {llvm.linkage = #llvm.linkage<external>, llvm.nofree, llvm.nosync, llvm.nounwind, llvm.readnone, llvm.willreturn, memory = #llvm.memory_effects<other = none, argMem = none, inaccessibleMem = none, errnoMem = none, targetMem0 = none, targetMem1 = none>, passthrough = ["nounwind", "nosync", "nofree", "willreturn"]}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c8 = arith.constant 8 : index
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c4 = arith.constant 4 : index
    %c3 = arith.constant 3 : index
    %c0 = arith.constant 0 : index
    %c149 = arith.constant 149 : index
    %c150 = arith.constant 150 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c960 = arith.constant 960 : index
    %c9599 = arith.constant 9599 : index
    %c-1 = arith.constant -1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c9600 = arith.constant 9600 : index
    %c1_i32 = arith.constant 1 : i32
    %cst = arith.constant 9.000000e+00 : f64
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
    %16 = memref.load %arg1[%c8] : memref<?xi64>
    %17 = arith.index_cast %16 : i64 to index
    %18 = arith.subi %1, %3 : index
    %19 = arith.cmpi slt, %18, %c0 : index
    %20 = arith.select %19, %c0, %18 : index
    %21 = arith.addi %20, %c149 : index
    %22 = arith.divui %21, %c150 : index
    %23 = arith.maxui %5, %c1 : index
    %24 = arith.cmpi ugt, %23, %c2 : index
    scf.for %arg4 = %22 to %15 step %c1 {
      %25 = arith.muli %arg4, %c150 : index
      %26 = arith.addi %3, %25 : index
      %27 = arith.divui %26, %c150 : index
      %28 = arith.muli %27, %c2 : index
      %29 = arith.subi %7, %28 : index
      %30 = arith.maxui %26, %c1 : index
      %31 = arith.addi %26, %c150 : index
      %32 = arith.minui %31, %9 : index
      %33 = arith.muli %29, %c960 : index
      %34 = arith.maxui %33, %c1 : index
      %35 = arith.addi %33, %c960 : index
      %36 = arith.minui %35, %c9599 : index
      scf.if %24 {
        %37 = arith.subi %23, %c1 : index
        %38 = arith.divui %30, %23 : index
        %39 = arith.addi %32, %37 : index
        %40 = arith.divui %39, %23 : index
        %41 = arith.cmpi ugt, %32, %30 : index
        %42 = arith.select %41, %40, %38 : index
        %43 = arith.maxui %17, %c1 : index
        %44 = arith.subi %43, %c1 : index
        %45 = arith.cmpi eq, %17, %c1 : index
        scf.for %arg5 = %38 to %42 step %c1 {
          %46 = arith.muli %arg5, %23 : index
          %47 = arith.cmpi uge, %46, %30 : index
          %48 = arith.subi %30, %46 : index
          %49 = arith.select %47, %c0, %48 : index
          %50 = arith.minui %49, %23 : index
          %51 = arith.cmpi uge, %46, %32 : index
          %52 = arith.subi %32, %46 : index
          %53 = arith.select %51, %c0, %52 : index
          %54 = arith.minui %53, %23 : index
          %55 = arith.minui %54, %c1 : index
          %56 = arith.addi %arg5, %c-1 : index
          %57 = arith.cmpi ult, %56, %11 : index
          %58 = arith.subi %56, %11 : index
          %59 = arith.select %57, %c0, %58 : index
          %60 = arith.minui %59, %44 : index
          %61 = arith.select %45, %c0, %60 : index
          %62 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
          %63 = arith.index_cast %61 : index to i64
          %64 = llvm.getelementptr %62[%63] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %65 = llvm.getelementptr %64[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %66 = arith.cmpi ult, %arg5, %11 : index
          %67 = arith.subi %arg5, %11 : index
          %68 = arith.select %66, %c0, %67 : index
          %69 = arith.minui %68, %44 : index
          %70 = arith.select %45, %c0, %69 : index
          %71 = arith.index_cast %70 : index to i64
          %72 = llvm.getelementptr %62[%71] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %73 = llvm.getelementptr %72[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %74 = llvm.load %65 : !llvm.ptr -> !llvm.ptr
          %75 = llvm.load %73 : !llvm.ptr -> !llvm.ptr
          %76 = polygeist.pointer2memref %74 : !llvm.ptr to memref<?x?xf64>
          %77 = polygeist.pointer2memref %75 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %50 to %55 step %c1 {
            %97 = arith.addi %arg6, %c-1 : index
            %98 = arith.addi %97, %23 : index
            %99 = arith.addi %arg6, %c1 : index
            scf.for %arg7 = %34 to %36 step %c1 {
              %100 = arith.index_cast %arg7 : index to i32
              %101 = arith.addi %100, %c-1_i32 : i32
              %102 = arith.index_cast %101 : i32 to index
              %103 = polygeist.load %76[%98, %102] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %104 = polygeist.load %76[%98, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %105 = arith.addf %103, %104 : f64
              %106 = arith.addi %100, %c1_i32 : i32
              %107 = arith.index_cast %106 : i32 to index
              %108 = polygeist.load %76[%98, %107] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %109 = arith.addf %105, %108 : f64
              %110 = polygeist.load %77[%arg6, %102] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %111 = arith.addf %109, %110 : f64
              %112 = polygeist.load %77[%arg6, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %113 = arith.addf %111, %112 : f64
              %114 = polygeist.load %77[%arg6, %107] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %115 = arith.addf %113, %114 : f64
              %116 = polygeist.load %77[%99, %102] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %117 = arith.addf %115, %116 : f64
              %118 = polygeist.load %77[%99, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %119 = arith.addf %117, %118 : f64
              %120 = polygeist.load %77[%99, %107] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %121 = arith.addf %119, %120 : f64
              %122 = arith.divf %121, %cst : f64
              polygeist.store %122, %77[%arg6, %arg7] sizes(%13, %c9600) : f64, memref<?x?xf64>
            } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
          }
          %78 = arith.maxui %50, %c1 : index
          %79 = arith.minui %54, %37 : index
          %80 = llvm.load %73 : !llvm.ptr -> !llvm.ptr
          %81 = polygeist.pointer2memref %80 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %78 to %79 step %c1 {
            %97 = arith.addi %arg6, %c-1 : index
            %98 = arith.addi %arg6, %c1 : index
            scf.for %arg7 = %34 to %36 step %c1 {
              %99 = arith.index_cast %arg7 : index to i32
              %100 = arith.addi %99, %c-1_i32 : i32
              %101 = arith.index_cast %100 : i32 to index
              %102 = polygeist.load %81[%97, %101] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %103 = polygeist.load %81[%97, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %104 = arith.addf %102, %103 : f64
              %105 = arith.addi %99, %c1_i32 : i32
              %106 = arith.index_cast %105 : i32 to index
              %107 = polygeist.load %81[%97, %106] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %108 = arith.addf %104, %107 : f64
              %109 = polygeist.load %81[%arg6, %101] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %110 = arith.addf %108, %109 : f64
              %111 = polygeist.load %81[%arg6, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %112 = arith.addf %110, %111 : f64
              %113 = polygeist.load %81[%arg6, %106] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %114 = arith.addf %112, %113 : f64
              %115 = polygeist.load %81[%98, %101] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %116 = arith.addf %114, %115 : f64
              %117 = polygeist.load %81[%98, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %118 = arith.addf %116, %117 : f64
              %119 = polygeist.load %81[%98, %106] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %120 = arith.addf %118, %119 : f64
              %121 = arith.divf %120, %cst : f64
              polygeist.store %121, %81[%arg6, %arg7] sizes(%13, %c9600) : f64, memref<?x?xf64>
            } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
          }
          %82 = arith.maxui %50, %37 : index
          %83 = arith.minui %54, %23 : index
          %84 = arith.addi %arg5, %c1 : index
          %85 = arith.cmpi ult, %84, %11 : index
          %86 = arith.subi %84, %11 : index
          %87 = arith.select %85, %c0, %86 : index
          %88 = arith.minui %87, %44 : index
          %89 = arith.select %45, %c0, %88 : index
          %90 = arith.index_cast %89 : index to i64
          %91 = llvm.getelementptr %62[%90] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %92 = llvm.getelementptr %91[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %93 = llvm.load %73 : !llvm.ptr -> !llvm.ptr
          %94 = llvm.load %92 : !llvm.ptr -> !llvm.ptr
          %95 = polygeist.pointer2memref %93 : !llvm.ptr to memref<?x?xf64>
          %96 = polygeist.pointer2memref %94 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %82 to %83 step %c1 {
            %97 = arith.addi %arg6, %c-1 : index
            %98 = arith.addi %arg6, %c1 : index
            %99 = arith.subi %98, %23 : index
            scf.for %arg7 = %34 to %36 step %c1 {
              %100 = arith.index_cast %arg7 : index to i32
              %101 = arith.addi %100, %c-1_i32 : i32
              %102 = arith.index_cast %101 : i32 to index
              %103 = polygeist.load %95[%97, %102] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %104 = polygeist.load %95[%97, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %105 = arith.addf %103, %104 : f64
              %106 = arith.addi %100, %c1_i32 : i32
              %107 = arith.index_cast %106 : i32 to index
              %108 = polygeist.load %95[%97, %107] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %109 = arith.addf %105, %108 : f64
              %110 = polygeist.load %95[%arg6, %102] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %111 = arith.addf %109, %110 : f64
              %112 = polygeist.load %95[%arg6, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %113 = arith.addf %111, %112 : f64
              %114 = polygeist.load %95[%arg6, %107] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %115 = arith.addf %113, %114 : f64
              %116 = polygeist.load %96[%99, %102] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %117 = arith.addf %115, %116 : f64
              %118 = polygeist.load %96[%99, %arg7] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %119 = arith.addf %117, %118 : f64
              %120 = polygeist.load %96[%99, %107] sizes(%13, %c9600) : memref<?x?xf64> -> f64
              %121 = arith.addf %119, %120 : f64
              %122 = arith.divf %121, %cst : f64
              polygeist.store %122, %95[%arg6, %arg7] sizes(%13, %c9600) : f64, memref<?x?xf64>
            } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
          }
        }
      } else {
        %37 = arith.maxui %17, %c1 : index
        %38 = arith.subi %37, %c1 : index
        %39 = arith.cmpi eq, %17, %c1 : index
        scf.for %arg5 = %30 to %32 step %c1 {
          %40 = arith.index_cast %arg5 : index to i32
          %41 = arith.addi %40, %c-1_i32 : i32
          %42 = arith.index_cast %41 : i32 to index
          %43 = arith.addi %40, %c1_i32 : i32
          %44 = arith.index_cast %43 : i32 to index
          %45 = arith.divui %42, %23 : index
          %46 = arith.remui %42, %23 : index
          %47 = arith.divui %arg5, %23 : index
          %48 = arith.remui %arg5, %23 : index
          %49 = arith.divui %44, %23 : index
          %50 = arith.remui %44, %23 : index
          %51 = arith.cmpi ult, %45, %11 : index
          %52 = arith.subi %45, %11 : index
          %53 = arith.select %51, %c0, %52 : index
          %54 = arith.minui %53, %38 : index
          %55 = arith.select %39, %c0, %54 : index
          %56 = polygeist.memref2pointer %arg3 : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> to !llvm.ptr
          %57 = arith.index_cast %55 : index to i64
          %58 = llvm.getelementptr %56[%57] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %59 = llvm.getelementptr %58[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %60 = arith.cmpi ult, %47, %11 : index
          %61 = arith.subi %47, %11 : index
          %62 = arith.select %60, %c0, %61 : index
          %63 = arith.minui %62, %38 : index
          %64 = arith.select %39, %c0, %63 : index
          %65 = arith.index_cast %64 : index to i64
          %66 = llvm.getelementptr %56[%65] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %67 = llvm.getelementptr %66[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %68 = arith.cmpi ult, %49, %11 : index
          %69 = arith.subi %49, %11 : index
          %70 = arith.select %68, %c0, %69 : index
          %71 = arith.minui %70, %38 : index
          %72 = arith.select %39, %c0, %71 : index
          %73 = arith.index_cast %72 : index to i64
          %74 = llvm.getelementptr %56[%73] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %75 = llvm.getelementptr %74[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
          %76 = llvm.load %59 : !llvm.ptr -> !llvm.ptr
          %77 = llvm.load %67 : !llvm.ptr -> !llvm.ptr
          %78 = llvm.load %75 : !llvm.ptr -> !llvm.ptr
          %79 = polygeist.pointer2memref %76 : !llvm.ptr to memref<?x?xf64>
          %80 = polygeist.pointer2memref %77 : !llvm.ptr to memref<?x?xf64>
          %81 = polygeist.pointer2memref %78 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %34 to %36 step %c1 {
            %82 = arith.index_cast %arg6 : index to i32
            %83 = arith.addi %82, %c-1_i32 : i32
            %84 = arith.index_cast %83 : i32 to index
            %85 = polygeist.load %79[%46, %84] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %86 = polygeist.load %79[%46, %arg6] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %87 = arith.addf %85, %86 : f64
            %88 = arith.addi %82, %c1_i32 : i32
            %89 = arith.index_cast %88 : i32 to index
            %90 = polygeist.load %79[%46, %89] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %91 = arith.addf %87, %90 : f64
            %92 = polygeist.load %80[%48, %84] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %93 = arith.addf %91, %92 : f64
            %94 = polygeist.load %80[%48, %arg6] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %95 = arith.addf %93, %94 : f64
            %96 = polygeist.load %80[%48, %89] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %97 = arith.addf %95, %96 : f64
            %98 = polygeist.load %81[%50, %84] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %99 = arith.addf %97, %98 : f64
            %100 = polygeist.load %81[%50, %arg6] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %101 = arith.addf %99, %100 : f64
            %102 = polygeist.load %81[%50, %89] sizes(%13, %c9600) : memref<?x?xf64> -> f64
            %103 = arith.addf %101, %102 : f64
            %104 = arith.divf %103, %cst : f64
            polygeist.store %104, %80[%48, %arg6] sizes(%13, %c9600) : f64, memref<?x?xf64>
          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
        } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
      }
    } {arts.id = 114 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "">, arts.metadata_provenance = "exact"}
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c9_i32 = arith.constant 9 : i32
    %c8 = arith.constant 8 : index
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c4 = arith.constant 4 : index
    %c3 = arith.constant 3 : index
    %c136 = arith.constant 136 : index
    %c0_i64 = arith.constant 0 : i64
    %c76800 = arith.constant 76800 : index
    %0 = llvm.mlir.zero : !llvm.ptr
    %c39000_i64 = arith.constant 39000 : i64
    %c6_i32 = arith.constant 6 : i32
    %1 = llvm.mlir.undef : !llvm.struct<(i32, i64)>
    %c1_i32 = arith.constant 1 : i32
    %c9599_i64 = arith.constant 9599 : i64
    %c1200 = arith.constant 1200 : index
    %c149 = arith.constant 149 : index
    %c9599 = arith.constant 9599 : index
    %c64 = arith.constant 64 : index
    %c9 = arith.constant 9 : index
    %c150 = arith.constant 150 : index
    %c2 = arith.constant 2 : index
    %c320 = arith.constant 320 : index
    %c9600 = arith.constant 9600 : index
    %c0 = arith.constant 0 : index
    %cst = arith.constant 9.600000e+03 : f64
    %c-1_i32 = arith.constant -1 : i32
    %cst_0 = arith.constant 2.000000e+00 : f64
    %c1 = arith.constant 1 : index
    %false = arith.constant false
    %2 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_1 = arith.constant 0.000000e+00 : f64
    %3 = llvm.mlir.addressof @str2 : !llvm.ptr
    %c2_i32 = arith.constant 2 : i32
    %4 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %5 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %alloca = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:62:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %6 = "polygeist.undef"() : () -> f64
    memref.store %6, %alloca[] : memref<f64>
    %alloca_2 = memref.alloca() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:20:1", totalAccesses = 6 : i64, readCount = 2 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, firstUseId = 35 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true>, arts.metadata_provenance = "exact"} : memref<i1>
    memref.store %true, %alloca_2[] : memref<i1>
    call @carts_benchmarks_start() : () -> ()
    %7 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%7) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %4 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    %9 = call @arts_get_total_workers() : () -> i32
    %10 = arith.index_castui %9 : i32 to index
    %11 = arith.maxui %10, %c1 : index
    %12 = arith.minui %11, %c1200 : index
    %13 = arith.addi %12, %c9599 : index
    %14 = arith.divui %13, %12 : index
    %15 = arith.maxui %14, %c1 : index
    %16 = arith.maxui %15, %c1 : index
    %17 = arith.index_cast %16 : index to i64
    %18 = arith.addi %17, %c9599_i64 : i64
    %19 = arith.divui %18, %17 : i64
    %20 = arith.index_cast %19 : i64 to index
    %21 = arith.muli %16, %c76800 : index
    %alloc = memref.alloc(%20) : memref<?xi64>
    %alloc_3 = memref.alloc(%20) : memref<?x!llvm.ptr>
    %22 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    scf.for %arg2 = %c0 to %20 step %c1 {
      %27 = func.call @arts_guid_reserve(%c6_i32, %c-1_i32) : (i32, i32) -> i64
      memref.store %27, %alloc[%arg2] : memref<?xi64>
      %28 = arith.index_cast %21 : index to i64
      %29 = arith.index_cast %arg2 : index to i64
      %30 = arith.addi %29, %c39000_i64 : i64
      %31 = llvm.insertvalue %c0_i32, %1[0] : !llvm.struct<(i32, i64)> 
      %32 = llvm.insertvalue %30, %31[1] : !llvm.struct<(i32, i64)> 
      llvm.store %32, %22 : !llvm.struct<(i32, i64)>, !llvm.ptr
      %33 = polygeist.pointer2memref %22 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
      %34 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      %35 = func.call @arts_db_create_with_guid(%27, %28, %c0_i32, %34, %33) : (i64, i64, i32, memref<?xi8>, memref<?x!llvm.struct<(i32, i64)>>) -> !llvm.ptr
      memref.store %35, %alloc_3[%arg2] : memref<?x!llvm.ptr>
    }
    %23 = arith.maxui %16, %c1 : index
    scf.for %arg2 = %c0 to %c9600 step %c1 {
      %27 = arith.index_cast %arg2 : index to i32
      %28 = arith.sitofp %27 : i32 to f64
      %29 = arith.divui %arg2, %23 : index
      %30 = arith.remui %arg2, %23 : index
      %31 = polygeist.memref2pointer %alloc_3 : memref<?x!llvm.ptr> to !llvm.ptr
      %32 = arith.index_cast %29 : index to i64
      %33 = llvm.getelementptr %31[%32] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %34 = llvm.load %33 : !llvm.ptr -> !llvm.ptr
      %35 = polygeist.pointer2memref %34 : !llvm.ptr to memref<?x?xf64>
      scf.for %arg3 = %c0 to %c9600 step %c1 {
        %36 = arith.index_cast %arg3 : index to i32
        %37 = arith.addi %36, %c2_i32 : i32
        %38 = arith.sitofp %37 : i32 to f64
        %39 = arith.mulf %28, %38 : f64
        %40 = arith.addf %39, %cst_0 : f64
        %41 = arith.divf %40, %cst : f64
        polygeist.store %41, %35[%30, %arg3] sizes(%16, %c9600) : f64, memref<?x?xf64>
      } {arts.id = 42 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:5">, arts.metadata_provenance = "recovered"}
    } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%7) : (memref<?xi8>) -> ()
    %alloca_4 = memref.alloca() : memref<9xi64>
    %24 = llvm.alloca %c1_i32 x !llvm.struct<(i32, i64)> : (i32) -> !llvm.ptr
    %alloca_5 = memref.alloca() : memref<i32>
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %27 = memref.load %alloca_2[] : memref<i1>
      scf.if %27 {
        %28 = func.call @arts_initialize_and_start_epoch(%c0_i64, %c0_i32) : (i64, i32) -> i64
        scf.for %arg3 = %c0 to %c136 step %c1 {
          %30 = arith.cmpi uge, %arg3, %c9 : index
          %31 = arith.subi %arg3, %c9 : index
          %32 = arith.select %30, %31, %c0 : index
          %33 = arith.addi %32, %c1 : index
          %34 = arith.divui %33, %c2 : index
          %35 = arith.divui %arg3, %c2 : index
          %36 = arith.addi %35, %c1 : index
          %37 = arith.minui %36, %c64 : index
          %38 = arith.muli %34, %c150 : index
          %39 = arith.muli %37, %c150 : index
          %40 = arith.subi %39, %38 : index
          %41 = arith.addi %40, %c149 : index
          %42 = arith.divui %41, %c150 : index
          %43 = arith.minui %42, %c64 : index
          %44 = arith.maxui %43, %c1 : index
          %45 = arith.divui %42, %c64 : index
          %46 = arith.remui %42, %c64 : index
          %47 = arith.addi %45, %c1 : index
          scf.for %arg4 = %c0 to %44 step %c1 {
            %48 = arith.cmpi ult, %arg4, %46 : index
            %49 = arith.select %48, %47, %45 : index
            %50 = arith.minui %arg4, %46 : index
            %51 = arith.muli %arg4, %45 : index
            %52 = arith.addi %51, %50 : index
            %53 = arith.cmpi uge, %52, %42 : index
            %54 = arith.subi %42, %52 : index
            %55 = arith.select %53, %c0, %54 : index
            %56 = arith.minui %49, %55 : index
            %57 = arith.cmpi ugt, %56, %c0 : index
            scf.if %57 {
              %cast = memref.cast %alloca_4 : memref<9xi64> to memref<?xi64>
              %58 = arith.muli %52, %c150 : index
              %59 = arith.addi %38, %58 : index
              %60 = arith.subi %39, %59 : index
              %61 = arith.cmpi slt, %60, %c0 : index
              %62 = arith.select %61, %c0, %60 : index
              %63 = arith.addi %62, %c149 : index
              %64 = arith.divui %63, %c150 : index
              %65 = arith.minui %64, %56 : index
              %66 = arith.maxui %65, %c1 : index
              %67 = arith.divui %59, %23 : index
              %68 = arith.addi %59, %66 : index
              %69 = arith.subi %68, %c1 : index
              %70 = arith.divui %69, %23 : index
              %71 = arith.subi %20, %c1 : index
              %72 = arith.cmpi ugt, %67, %71 : index
              %73 = arith.minui %70, %71 : index
              %74 = arith.select %72, %70, %73 : index
              %75 = arith.subi %74, %67 : index
              %76 = arith.addi %75, %c1 : index
              %77 = arith.select %72, %c0, %67 : index
              %78 = arith.select %72, %c0, %76 : index
              %79 = arith.index_cast %38 : index to i64
              memref.store %79, %alloca_4[%c0] : memref<9xi64>
              %80 = arith.index_cast %59 : index to i64
              memref.store %80, %alloca_4[%c1] : memref<9xi64>
              %81 = arith.index_cast %23 : index to i64
              memref.store %81, %alloca_4[%c2] : memref<9xi64>
              %82 = arith.index_cast %arg3 : index to i64
              memref.store %82, %alloca_4[%c3] : memref<9xi64>
              memref.store %c9599_i64, %alloca_4[%c4] : memref<9xi64>
              %83 = arith.index_cast %77 : index to i64
              memref.store %83, %alloca_4[%c5] : memref<9xi64>
              memref.store %17, %alloca_4[%c6] : memref<9xi64>
              %84 = arith.index_cast %65 : index to i64
              memref.store %84, %alloca_4[%c7] : memref<9xi64>
              %85 = arith.index_cast %78 : index to i64
              memref.store %85, %alloca_4[%c8] : memref<9xi64>
              %86 = arith.index_cast %78 : index to i32
              %87 = polygeist.get_func @__arts_edt_1 : !llvm.ptr
              %88 = polygeist.pointer2memref %87 : !llvm.ptr to memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>
              %89 = llvm.insertvalue %c-1_i32, %1[0] : !llvm.struct<(i32, i64)> 
              %90 = llvm.insertvalue %c0_i64, %89[1] : !llvm.struct<(i32, i64)> 
              llvm.store %90, %24 : !llvm.struct<(i32, i64)>, !llvm.ptr
              %91 = polygeist.pointer2memref %24 : !llvm.ptr to memref<?x!llvm.struct<(i32, i64)>>
              %92 = func.call @arts_edt_create_with_epoch(%88, %c9_i32, %cast, %86, %28, %91) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>>)>>, i32, memref<?xi64>, i32, i64, memref<?x!llvm.struct<(i32, i64)>>) -> i64
              memref.store %c0_i32, %alloca_5[] : memref<i32>
              %93 = arith.addi %77, %78 : index
              scf.for %arg5 = %77 to %93 step %c1 {
                %94 = arith.cmpi ult, %arg5, %20 : index
                %95 = memref.load %alloca_5[] : memref<i32>
                scf.if %94 {
                  %97 = arith.subi %78, %c1 : index
                  %98 = arith.maxsi %97, %c0 : index
                  %99 = arith.minsi %98, %c1 : index
                  %100 = arith.addi %99, %77 : index
                  %101 = arith.cmpi eq, %arg5, %100 : index
                  %102 = arith.select %101, %c2_i32, %c1_i32 : i32
                  %103 = arith.cmpi ult, %arg5, %100 : index
                  %104 = arith.minui %16, %c1 : index
                  %105 = arith.muli %104, %c76800 : index
                  %106 = arith.select %103, %105, %c0 : index
                  %107 = arith.cmpi ugt, %arg5, %100 : index
                  %108 = arith.select %107, %105, %106 : index
                  %109 = arith.index_cast %108 : index to i64
                  %110 = arith.select %101, %c0_i64, %109 : i64
                  %111 = polygeist.memref2pointer %alloc : memref<?xi64> to !llvm.ptr
                  %112 = arith.index_cast %arg5 : index to i64
                  %113 = llvm.getelementptr %111[%112] : (!llvm.ptr, i64) -> !llvm.ptr, i64
                  %114 = llvm.load %113 : !llvm.ptr -> i64
                  %115 = arith.cmpi eq, %110, %c0_i64 : i64
                  scf.if %115 {
                    func.call @arts_add_dependence_ex(%114, %92, %95, %102, %c2_i32) : (i64, i64, i32, i32, i32) -> ()
                  } else {
                    %116 = arith.subi %16, %104 : index
                    %117 = arith.muli %116, %c76800 : index
                    %118 = arith.select %103, %117, %c0 : index
                    %119 = arith.select %107, %c0, %118 : index
                    %120 = arith.index_cast %119 : index to i64
                    %121 = arith.select %101, %c0_i64, %120 : i64
                    func.call @arts_add_dependence_at_ex(%114, %92, %95, %102, %121, %110, %c2_i32) : (i64, i64, i32, i32, i64, i64, i32) -> ()
                  }
                } else {
                  func.call @arts_signal_edt_null(%92, %95) : (i64, i32) -> ()
                }
                %96 = arith.addi %95, %c1_i32 : i32
                memref.store %96, %alloca_5[] : memref<i32>
              }
            }
          } {arts.id = 55 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:49:7">, arts.metadata_provenance = "recovered"}
        } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
        %29 = func.call @arts_wait_on_handle(%28) : (i64) -> i1
        memref.store %true, %alloca_2[] : memref<i1>
      }
    } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_stop(%7) : (memref<?xi8>) -> ()
    %25 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%25, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_1, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c64 step %c1 {
      %27 = arith.muli %arg2, %c150 : index
      %28 = arith.subi %c9600, %27 : index
      %29 = arith.cmpi uge, %27, %c9600 : index
      %30 = arith.select %29, %c0, %28 : index
      %31 = arith.minui %30, %c150 : index
      %32 = polygeist.memref2pointer %alloc_3 : memref<?x!llvm.ptr> to !llvm.ptr
      %33 = arith.index_cast %arg2 : index to i64
      %34 = llvm.getelementptr %32[%33] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %35 = llvm.load %34 : !llvm.ptr -> !llvm.ptr
      %36 = polygeist.pointer2memref %35 : !llvm.ptr to memref<?x?xf64>
      scf.for %arg3 = %c0 to %31 step %c1 {
        %37 = arith.addi %27, %arg3 : index
        %38 = polygeist.load %36[%arg3, %37] sizes(%16, %c9600) : memref<?x?xf64> -> f64
        %39 = memref.load %alloca[] : memref<f64>
        %40 = arith.addf %39, %38 : f64
        memref.store %40, %alloca[] : memref<f64>
      }
    }
    call @carts_bench_checksum_d(%cst_1) : (f64) -> ()
    call @carts_phase_timer_stop(%25) : (memref<?xi8>) -> ()
    %26 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%26, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%26) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.store %false, %alloca_2[] : memref<i1>
    memref.dealloc %alloc : memref<?xi64>
    memref.dealloc %alloc_3 : memref<?x!llvm.ptr>
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
