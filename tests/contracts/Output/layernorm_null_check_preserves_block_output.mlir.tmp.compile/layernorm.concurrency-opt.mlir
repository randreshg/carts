module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for two-worker partitioning checks.\0Aworker_threads=2\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 2 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str4("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("allocation failure\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("layernorm\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2 = arith.constant 2 : index
    %cst = arith.constant 9.99999974E-6 : f32
    %cst_0 = arith.constant 1.024000e+03 : f32
    %c8 = arith.constant 8 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c113_i32 = arith.constant 113 : i32
    %cst_1 = arith.constant 5.000000e+01 : f32
    %cst_2 = arith.constant 3.125000e-02 : f32
    %cst_3 = arith.constant 1.000000e+00 : f32
    %cst_4 = arith.constant 0.000000e+00 : f32
    %c1024 = arith.constant 1024 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str4 : !llvm.ptr
    %cst_5 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str3 : !llvm.ptr
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr
    %3 = llvm.mlir.addressof @stderr : !llvm.ptr
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = llvm.mlir.addressof @str1 : !llvm.ptr
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    call @carts_benchmarks_start() : () -> ()
    %7 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%7) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %7) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c-1_i32 : i32) sizes[%c2] elementType(f32) elementSizes[%c8, %c1024] {arts.id = 173 : i64, arts.read_only_after_init} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    arts.lowering_contract(%ptr : memref<?xmemref<?x?xf32>>) block_shape[%c8] {owner_dims = array<i64: 0>, post_db_refined}
    %guid_6, %ptr_7 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1024] {arts.id = 36 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "layernorm.c:66:18", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4096 : i64, firstUseId = 36 : i64, lastUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 36 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_8, %ptr_9 = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c1024] {arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "layernorm.c:67:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4096 : i64, firstUseId = 37 : i64, lastUseId = 108 : i64, hasUniformAccess = true, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 37 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %9 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %10 = polygeist.memref2pointer %9 : memref<?x?xf32> to !llvm.ptr
    %11 = llvm.icmp "eq" %10, %4 : !llvm.ptr
    %12 = scf.if %11 -> (i1) {
      scf.yield %true : i1
    } else {
      %16 = arts.db_ref %ptr_7[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %17 = polygeist.memref2pointer %16 : memref<?xf32> to !llvm.ptr
      %18 = llvm.icmp "eq" %17, %4 : !llvm.ptr
      scf.yield %18 : i1
    }
    %13 = scf.if %12 -> (i1) {
      scf.yield %true : i1
    } else {
      %16 = arts.db_ref %ptr_9[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %17 = polygeist.memref2pointer %16 : memref<?xf32> to !llvm.ptr
      %18 = llvm.icmp "eq" %17, %4 : !llvm.ptr
      scf.yield %18 : i1
    }
    %14 = arith.xori %13, %true : i1
    scf.if %13 {
      %16 = llvm.load %3 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %17 = polygeist.memref2pointer %16 : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>> to !llvm.ptr
      %18 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<20 x i8>
      %19 = llvm.call @fprintf(%17, %18) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
    }
    %15 = arith.extui %13 : i1 to i32
    scf.if %14 {
      %16 = scf.for %arg0 = %c0 to %c16 step %c1 iter_args(%arg1 = %c0_i32) -> (i32) {
        %23 = arith.index_cast %arg1 : i32 to index
        %24 = arith.addi %23, %c1024 : index
        %25 = arith.index_cast %24 : index to i32
        %26 = arith.divui %arg0, %c8 : index
        %27 = arith.remui %arg0, %c8 : index
        %28 = arts.db_ref %ptr[%26] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        scf.for %arg2 = %c0 to %c1024 step %c1 {
          %29 = arith.addi %23, %arg2 : index
          %30 = arith.index_cast %29 : index to i32
          %31 = arith.remsi %30, %c113_i32 : i32
          %32 = arith.sitofp %31 : i32 to f32
          %33 = arith.subf %32, %cst_1 : f32
          %34 = arith.mulf %33, %cst_2 : f32
          memref.store %34, %28[%27, %arg2] : memref<?x?xf32>
        } {arts.id = 67 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "recovered"}
        scf.yield %25 : i32
      } {arts.id = 69 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 16 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "recovered"}
      %17 = arts.db_ref %ptr_7[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %18 = arts.db_ref %ptr_9[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      scf.for %arg0 = %c0 to %c1024 step %c1 {
        memref.store %cst_3, %17[%arg0] : memref<?xf32>
        memref.store %cst_4, %18[%arg0] : memref<?xf32>
      } {arts.id = 73 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:78:3">, arts.metadata_provenance = "recovered"}
      func.call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
      func.call @carts_kernel_timer_start(%7) : (memref<?xi8>) -> ()
      %guid_10, %ptr_11 = arts.db_acquire[<in>] (%guid_6 : memref<?xi64>, %ptr_7 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.id = 178 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      %guid_12, %ptr_13 = arts.db_acquire[<in>] (%guid_8 : memref<?xi64>, %ptr_9 : memref<?xmemref<?xf32>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.id = 177 : i64, arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      %19 = arts.epoch attributes {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
        scf.for %arg0 = %c0 to %c2 step %c1 {
          %23 = arith.muli %arg0, %c8 : index
          %24 = arith.cmpi uge, %23, %c16 : index
          %25 = arith.subi %c16, %23 : index
          %26 = arith.select %24, %c0, %25 : index
          %27 = arith.minui %26, %c8 : index
          %28 = arith.cmpi ugt, %27, %c0 : index
          scf.if %28 {
            %29 = arith.divui %23, %c8 : index
            %30 = arith.cmpi ugt, %29, %c1 : index
            %31 = arith.select %30, %c0, %29 : index
            %guid_14, %ptr_15 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[%23], sizes[%c8]), offsets[%31], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 1024], stencil_owner_dims = [0]} -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)
            arts.lowering_contract(%ptr_15 : memref<?xmemref<?x?xf32>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) block_shape[%c8] {critical_path_distance = 0 : i64, distribution_version = 1 : i64, owner_dims = array<i64: 0>, post_db_refined}
            arts.lowering_contract(%ptr_11 : memref<?xmemref<?xf32>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) {critical_path_distance = 0 : i64, distribution_version = 1 : i64, post_db_refined}
            arts.lowering_contract(%ptr_13 : memref<?xmemref<?xf32>>) dep_pattern(<uniform>) distribution_kind(<block>) distribution_pattern(<uniform>) {critical_path_distance = 0 : i64, distribution_version = 1 : i64, post_db_refined}
            arts.edt <task> <intranode> route(%c-1_i32) (%ptr_15, %ptr_11, %ptr_13) : memref<?xmemref<?x?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> attributes {arts.id = 176 : i64, arts.pattern_revision = 2 : i64, critical_path_distance = 0 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [8, 1024]} {
            ^bb0(%arg1: memref<?xmemref<?x?xf32>>, %arg2: memref<?xmemref<?xf32>>, %arg3: memref<?xmemref<?xf32>>):
              %32 = arith.subi %c0, %23 : index
              %33 = arith.cmpi slt, %32, %c0 : index
              %34 = arith.select %33, %c0, %32 : index
              %35 = arith.cmpi slt, %25, %c0 : index
              %36 = arith.select %35, %c0, %25 : index
              %37 = arith.minui %36, %27 : index
              %38 = arts.db_ref %arg1[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
              %39 = arts.db_ref %arg2[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
              %40 = arts.db_ref %arg3[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
              scf.for %arg4 = %34 to %37 step %c1 {
                %41 = scf.for %arg5 = %c0 to %c1024 step %c1 iter_args(%arg6 = %cst_4) -> (f32) {
                  %48 = memref.load %38[%arg4, %arg5] : memref<?x?xf32>
                  %49 = arith.addf %arg6, %48 : f32
                  scf.yield %49 : f32
                } {arts.id = 77 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "recovered"}
                %42 = arith.divf %41, %cst_0 : f32
                %43 = scf.for %arg5 = %c0 to %c1024 step %c1 iter_args(%arg6 = %cst_4) -> (f32) {
                  %48 = memref.load %38[%arg4, %arg5] : memref<?x?xf32>
                  %49 = arith.subf %48, %42 : f32
                  %50 = arith.mulf %49, %49 : f32
                  %51 = arith.addf %arg6, %50 : f32
                  scf.yield %51 : f32
                } {arts.id = 84 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = true, reductionKinds = ["add"], tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "recovered"}
                %44 = arith.divf %43, %cst_0 : f32
                %45 = arith.addf %44, %cst : f32
                %46 = math.sqrt %45 : f32
                %47 = arith.divf %cst_3, %46 : f32
                scf.for %arg5 = %c0 to %c1024 step %c1 {
                  %48 = memref.load %38[%arg4, %arg5] : memref<?x?xf32>
                  %49 = arith.subf %48, %42 : f32
                  %50 = arith.mulf %49, %47 : f32
                  %51 = memref.load %39[%arg5] : memref<?xf32>
                  %52 = arith.mulf %50, %51 : f32
                  %53 = memref.load %40[%arg5] : memref<?xf32>
                  %54 = arith.addf %52, %53 : f32
                  memref.store %54, %38[%arg4, %arg5] : memref<?x?xf32>
                } {arts.id = 98 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 1024 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "recovered"}
              } {arts.id = 175 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "layernorm.c:84:5">, arts.metadata_provenance = "exact"}
              arts.db_release(%arg1) : memref<?xmemref<?x?xf32>>
              arts.db_release(%arg2) : memref<?xmemref<?xf32>>
              arts.db_release(%arg3) : memref<?xmemref<?xf32>>
            }
          }
        }
      } : i64
      arts.db_release(%ptr_13) : memref<?xmemref<?xf32>>
      arts.db_release(%ptr_11) : memref<?xmemref<?xf32>>
      func.call @carts_kernel_timer_accum(%7) : (memref<?xi8>) -> ()
      func.call @carts_kernel_timer_print(%7) : (memref<?xi8>) -> ()
      %20 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%20, %7) : (memref<?xi8>, memref<?xi8>) -> ()
      %21 = scf.for %arg0 = %c0 to %c2 step %c1 iter_args(%arg1 = %cst_5) -> (f64) {
        %23 = arith.muli %arg0, %c8 : index
        %24 = arith.subi %c16, %23 : index
        %25 = arith.cmpi uge, %23, %c16 : index
        %26 = arith.select %25, %c0, %24 : index
        %27 = arith.minui %26, %c8 : index
        %28 = arts.db_ref %ptr[%arg0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
        %29 = scf.for %arg2 = %c0 to %27 step %c1 iter_args(%arg3 = %arg1) -> (f64) {
          %30 = arith.addi %23, %arg2 : index
          %31 = memref.load %28[%arg2, %30] : memref<?x?xf32>
          %32 = arith.extf %31 : f32 to f64
          %33 = math.absf %32 : f64
          %34 = arith.addf %arg3, %33 : f64
          scf.yield %34 : f64
        }
        scf.yield %29 : f64
      }
      func.call @carts_bench_checksum_d(%21) : (f64) -> ()
      func.call @carts_phase_timer_stop(%20) : (memref<?xi8>) -> ()
      %22 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
      func.call @carts_phase_timer_start(%22, %7) : (memref<?xi8>, memref<?xi8>) -> ()
      func.call @carts_phase_timer_stop(%22) : (memref<?xi8>) -> ()
      func.call @carts_e2e_timer_stop() : () -> ()
    }
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?x?xf32>>
    arts.db_free(%guid_8) : memref<?xi64>
    arts.db_free(%ptr_9) : memref<?xmemref<?xf32>>
    arts.db_free(%guid_6) : memref<?xi64>
    arts.db_free(%ptr_7) : memref<?xmemref<?xf32>>
    return %15 : i32
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
