module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c7 = arith.constant 7 : index
    %c6 = arith.constant 6 : index
    %c5 = arith.constant 5 : index
    %c4 = arith.constant 4 : index
    %c3 = arith.constant 3 : index
    %c2 = arith.constant 2 : index
    %c2304 = arith.constant 2304 : index
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %c1 = arith.constant 1 : index
    %c48 = arith.constant 48 : index
    %cst = arith.constant 1.000000e+00 : f64
    %cst_0 = arith.constant 1.000000e-03 : f64
    %c47 = arith.constant 47 : index
    %0:7 = arts_rt.edt_param_unpack %arg1 : memref<?xi64> : (index, index, index, index, index, index, index)
    %guid, %ptr = arts_rt.dep_db_acquire(%arg3) offset[%c0 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_1, %ptr_2 = arts_rt.dep_db_acquire(%arg3) offset[%c1 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_3, %ptr_4 = arts_rt.dep_db_acquire(%arg3) offset[%c2 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_5, %ptr_6 = arts_rt.dep_db_acquire(%arg3) offset[%c3 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_7, %ptr_8 = arts_rt.dep_db_acquire(%arg3) offset[%c4 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_9, %ptr_10 = arts_rt.dep_db_acquire(%arg3) offset[%c5 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_11, %ptr_12 = arts_rt.dep_db_acquire(%arg3) offset[%c6 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %1 = arith.addi %0#4, %c7 : index
    %2 = arith.addi %1, %0#5 : index
    %3 = arith.subi %c0, %0#0 : index
    %4 = arith.cmpi slt, %3, %c0 : index
    %5 = arith.select %4, %c0, %3 : index
    %6 = arith.cmpi slt, %0#1, %c0 : index
    %7 = arith.select %6, %c0, %0#1 : index
    %8 = arith.minui %7, %0#2 : index
    %9 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %10 = arts_rt.db_gep(%ptr_2 : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %11 = arts_rt.db_gep(%ptr_4 : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %12 = arts_rt.db_gep(%ptr_6 : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %13 = arts_rt.db_gep(%ptr_8 : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %14 = arts_rt.db_gep(%ptr_10 : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %15 = arts_rt.db_gep(%ptr_12 : memref<?x!llvm.ptr>) indices[%c0] strides[%c2304, %c48, %c1] : !llvm.ptr
    %guid_13, %ptr_14 = arts_rt.dep_gep(%arg3) offset[%c7 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_15, %ptr_16 = arts_rt.dep_gep(%arg3) offset[%1 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_17, %ptr_18 = arts_rt.dep_gep(%arg3) offset[%2 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %16 = llvm.load %ptr_14 : !llvm.ptr -> !llvm.ptr
    %17 = llvm.load %ptr_16 : !llvm.ptr -> !llvm.ptr
    %18 = llvm.load %ptr_18 : !llvm.ptr -> !llvm.ptr
    %19 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %20 = polygeist.pointer2memref %19 : !llvm.ptr to memref<?x?x?xf64>
    %21 = llvm.load %10 : !llvm.ptr -> !llvm.ptr
    %22 = polygeist.pointer2memref %21 : !llvm.ptr to memref<?x?x?xf64>
    %23 = llvm.load %11 : !llvm.ptr -> !llvm.ptr
    %24 = polygeist.pointer2memref %23 : !llvm.ptr to memref<?x?x?xf64>
    %25 = llvm.load %12 : !llvm.ptr -> !llvm.ptr
    %26 = polygeist.pointer2memref %25 : !llvm.ptr to memref<?x?x?xf64>
    %27 = llvm.load %13 : !llvm.ptr -> !llvm.ptr
    %28 = polygeist.pointer2memref %27 : !llvm.ptr to memref<?x?x?xf64>
    %29 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
    %30 = polygeist.pointer2memref %29 : !llvm.ptr to memref<?x?x?xf64>
    %31 = llvm.load %15 : !llvm.ptr -> !llvm.ptr
    %32 = polygeist.pointer2memref %31 : !llvm.ptr to memref<?x?x?xf64>
    %33 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?xf64>
    %34 = polygeist.pointer2memref %17 : !llvm.ptr to memref<?x?x?xf64>
    %35 = polygeist.pointer2memref %18 : !llvm.ptr to memref<?x?x?xf64>
    scf.for %arg4 = %5 to %8 step %c1 {
      %36 = arith.addi %0#3, %arg4 : index
      %37 = arith.index_cast %36 : index to i32
      %38 = arith.addi %37, %c1_i32 : i32
      %39 = arith.index_cast %38 : i32 to index
      scf.for %arg5 = %c1 to %c47 step %c1 {
        %40 = arith.index_cast %arg5 : index to i32
        %41 = arith.addi %40, %c1_i32 : i32
        %42 = arith.index_cast %41 : i32 to index
        scf.for %arg6 = %c1 to %c47 step %c1 {
          %43 = arith.index_cast %arg6 : index to i32
          %44 = polygeist.load %20[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %45 = arith.divf %cst, %44 : f64
          %46 = arith.addi %43, %c1_i32 : i32
          %47 = arith.index_cast %46 : i32 to index
          %48 = polygeist.load %22[%47, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %49 = polygeist.load %22[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %50 = arith.subf %48, %49 : f64
          %51 = polygeist.load %24[%arg6, %42, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %52 = polygeist.load %24[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %53 = arith.subf %51, %52 : f64
          %54 = arith.addf %50, %53 : f64
          %55 = polygeist.load %26[%arg6, %arg5, %39] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %56 = polygeist.load %26[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %57 = arith.subf %55, %56 : f64
          %58 = arith.addf %54, %57 : f64
          %59 = polygeist.load %24[%47, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %60 = polygeist.load %24[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %61 = arith.subf %59, %60 : f64
          %62 = polygeist.load %28[%arg6, %42, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %63 = polygeist.load %28[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %64 = arith.subf %62, %63 : f64
          %65 = arith.addf %61, %64 : f64
          %66 = polygeist.load %30[%arg6, %arg5, %39] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %67 = polygeist.load %30[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %68 = arith.subf %66, %67 : f64
          %69 = arith.addf %65, %68 : f64
          %70 = polygeist.load %26[%47, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %71 = polygeist.load %26[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %72 = arith.subf %70, %71 : f64
          %73 = polygeist.load %30[%arg6, %42, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %74 = polygeist.load %30[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %75 = arith.subf %73, %74 : f64
          %76 = arith.addf %72, %75 : f64
          %77 = polygeist.load %32[%arg6, %arg5, %39] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %78 = polygeist.load %32[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %79 = arith.subf %77, %78 : f64
          %80 = arith.addf %76, %79 : f64
          %81 = arith.mulf %45, %cst_0 : f64
          %82 = arith.mulf %81, %58 : f64
          %83 = polygeist.load %33[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %84 = arith.addf %83, %82 : f64
          polygeist.store %84, %33[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %85 = arith.mulf %81, %69 : f64
          %86 = polygeist.load %34[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %87 = arith.addf %86, %85 : f64
          polygeist.store %87, %34[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %88 = arith.mulf %81, %80 : f64
          %89 = polygeist.load %35[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
          %90 = arith.addf %89, %88 : f64
          polygeist.store %90, %35[%arg6, %arg5, %36] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
        } {arts.id = 188 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 190 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 212 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem_velocity_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c18432 = arith.constant 18432 : index
    %c8 = arith.constant 8 : index
    %true = arith.constant true
    %c2_i64 = arith.constant 2 : i64
    %c0_i64 = arith.constant 0 : i64
    %c46 = arith.constant 46 : index
    %cst = arith.constant 1.000000e-03 : f64
    %cst_0 = arith.constant 1.000000e+00 : f64
    %c-1_i32 = arith.constant -1 : i32
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
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 66000 : i64, arts.id = 66 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:91:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 66 : i64, lastUseId = 67 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 66 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_5, %ptr_6 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 68000 : i64, arts.id = 68 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:92:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 68 : i64, lastUseId = 69 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 68 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_7, %ptr_8 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 70000 : i64, arts.id = 70 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:93:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 70 : i64, lastUseId = 71 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 70 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_9, %ptr_10 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 72000 : i64, arts.id = 72 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:94:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 72 : i64, lastUseId = 73 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 72 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_11, %ptr_12 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 74000 : i64, arts.id = 74 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:95:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 74 : i64, lastUseId = 75 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 74 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_13, %ptr_14 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 76000 : i64, arts.id = 76 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:96:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 76 : i64, lastUseId = 77 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 76 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_15, %ptr_16 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 78000 : i64, arts.id = 78 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:97:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 78 : i64, lastUseId = 79 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 78 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_17, %ptr_18 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 80000 : i64, arts.id = 80 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:98:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 80 : i64, lastUseId = 81 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 80 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_19, %ptr_20 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 82000 : i64, arts.id = 82 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:99:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 82 : i64, lastUseId = 83 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 82 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_21, %ptr_22 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c48, %c48, %c48] {arts.create_id = 84000 : i64, arts.id = 84 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:100:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 884736 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 84 : i64, lastUseId = 85 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 84 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_23 = memref.alloca() {arts.id = 87 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:127:3", totalAccesses = 11 : i64, readCount = 8 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 87 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_23[] : memref<i32>
    memref.store %c0_i32, %alloca_23[] : memref<i32>
    %8 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %9 = arts_rt.db_gep(%ptr_6 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %10 = arts_rt.db_gep(%ptr_8 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %11 = arts_rt.db_gep(%ptr_10 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %12 = arts_rt.db_gep(%ptr_12 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %13 = arts_rt.db_gep(%ptr_14 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %14 = arts_rt.db_gep(%ptr_16 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %15 = arts_rt.db_gep(%ptr_18 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %16 = arts_rt.db_gep(%ptr_20 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %17 = arts_rt.db_gep(%ptr_22 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %18 = llvm.load %8 : !llvm.ptr -> !llvm.ptr
    %19 = polygeist.pointer2memref %18 : !llvm.ptr to memref<?x?x?xf64>
    %20 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %21 = polygeist.pointer2memref %20 : !llvm.ptr to memref<?x?x?xf64>
    %22 = llvm.load %10 : !llvm.ptr -> !llvm.ptr
    %23 = polygeist.pointer2memref %22 : !llvm.ptr to memref<?x?x?xf64>
    %24 = llvm.load %11 : !llvm.ptr -> !llvm.ptr
    %25 = polygeist.pointer2memref %24 : !llvm.ptr to memref<?x?x?xf64>
    %26 = llvm.load %12 : !llvm.ptr -> !llvm.ptr
    %27 = polygeist.pointer2memref %26 : !llvm.ptr to memref<?x?x?xf64>
    %28 = llvm.load %13 : !llvm.ptr -> !llvm.ptr
    %29 = polygeist.pointer2memref %28 : !llvm.ptr to memref<?x?x?xf64>
    %30 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
    %31 = polygeist.pointer2memref %30 : !llvm.ptr to memref<?x?x?xf64>
    %32 = llvm.load %15 : !llvm.ptr -> !llvm.ptr
    %33 = polygeist.pointer2memref %32 : !llvm.ptr to memref<?x?x?xf64>
    %34 = llvm.load %16 : !llvm.ptr -> !llvm.ptr
    %35 = polygeist.pointer2memref %34 : !llvm.ptr to memref<?x?x?xf64>
    %36 = llvm.load %17 : !llvm.ptr -> !llvm.ptr
    %37 = polygeist.pointer2memref %36 : !llvm.ptr to memref<?x?x?xf64>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      scf.for %arg1 = %c0 to %c48 step %c1 {
        scf.for %arg2 = %c0 to %c48 step %c1 {
          polygeist.store %cst_4, %19[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          polygeist.store %cst_4, %21[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          polygeist.store %cst_4, %23[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %47 = memref.load %alloca_23[] : memref<i32>
          %48 = arith.remsi %47, %c11_i32 : i32
          %49 = arith.sitofp %48 : i32 to f64
          %50 = arith.addf %49, %cst_1 : f64
          polygeist.store %50, %25[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %51 = memref.load %alloca_23[] : memref<i32>
          %52 = arith.muli %51, %c2_i32 : i32
          %53 = arith.remsi %52, %c17_i32 : i32
          %54 = arith.sitofp %53 : i32 to f64
          %55 = arith.mulf %54, %cst_2 : f64
          polygeist.store %55, %27[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %56 = memref.load %alloca_23[] : memref<i32>
          %57 = arith.muli %56, %c3_i32 : i32
          %58 = arith.remsi %57, %c19_i32 : i32
          %59 = arith.sitofp %58 : i32 to f64
          %60 = arith.mulf %59, %cst_2 : f64
          polygeist.store %60, %29[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %61 = memref.load %alloca_23[] : memref<i32>
          %62 = arith.muli %61, %c5_i32 : i32
          %63 = arith.remsi %62, %c23_i32 : i32
          %64 = arith.sitofp %63 : i32 to f64
          %65 = arith.mulf %64, %cst_2 : f64
          polygeist.store %65, %31[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %66 = memref.load %alloca_23[] : memref<i32>
          %67 = arith.muli %66, %c7_i32 : i32
          %68 = arith.remsi %67, %c13_i32 : i32
          %69 = arith.sitofp %68 : i32 to f64
          %70 = arith.mulf %69, %cst_3 : f64
          polygeist.store %70, %33[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %71 = memref.load %alloca_23[] : memref<i32>
          %72 = arith.muli %71, %c11_i32 : i32
          %73 = arith.remsi %72, %c29_i32 : i32
          %74 = arith.sitofp %73 : i32 to f64
          %75 = arith.mulf %74, %cst_3 : f64
          polygeist.store %75, %35[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %76 = memref.load %alloca_23[] : memref<i32>
          %77 = arith.muli %76, %c13_i32 : i32
          %78 = arith.remsi %77, %c31_i32 : i32
          %79 = arith.sitofp %78 : i32 to f64
          %80 = arith.mulf %79, %cst_3 : f64
          polygeist.store %80, %37[%arg0, %arg1, %arg2] sizes(%c48, %c48, %c48) : f64, memref<?x?x?xf64>
          %81 = memref.load %alloca_23[] : memref<i32>
          %82 = arith.addi %81, %c1_i32 : i32
          memref.store %82, %alloca_23[] : memref<i32>
        } {arts.id = 138 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 140 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 109 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %guid_24, %ptr_25 = arts.db_acquire[<in>] (%guid_9 : memref<?xi64>, %ptr_10 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_25 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %guid_26, %ptr_27 = arts.db_acquire[<in>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_27 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %guid_28, %ptr_29 = arts.db_acquire[<in>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_29 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %guid_30, %ptr_31 = arts.db_acquire[<in>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_31 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %guid_32, %ptr_33 = arts.db_acquire[<in>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_33 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %guid_34, %ptr_35 = arts.db_acquire[<in>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_35 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %guid_36, %ptr_37 = arts.db_acquire[<in>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr_37 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
    %38 = arts_rt.create_epoch : i64
    scf.for %arg0 = %c0 to %c46 step %c1 {
      %47 = arith.cmpi ult, %arg0, %c46 : index
      %48 = arith.select %47, %c1, %c0 : index
      %49 = arith.minui %arg0, %c46 : index
      %50 = arith.cmpi uge, %49, %c46 : index
      %51 = arith.subi %c46, %49 : index
      %52 = arith.select %50, %c0, %51 : index
      %53 = arith.minui %48, %52 : index
      %54 = arith.cmpi ugt, %53, %c0 : index
      scf.if %54 {
        %55 = arith.addi %49, %c1 : index
        %56 = arith.maxui %48, %c1 : index
        %57 = arith.divui %55, %c48 : index
        %58 = arith.addi %55, %56 : index
        %59 = arith.subi %58, %c1 : index
        %60 = arith.divui %59, %c48 : index
        %61 = arith.cmpi ugt, %57, %c0 : index
        %62 = arith.select %61, %60, %c0 : index
        %63 = arith.subi %62, %57 : index
        %64 = arith.addi %63, %c1 : index
        %65 = arith.select %61, %c0, %57 : index
        %66 = arith.select %61, %c0, %64 : index
        %guid_38, %ptr_39 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%55], sizes[%48]), offsets[%65], sizes[%66] element_offsets[%c0, %c0, %55] element_sizes[%c48, %c48, %48] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_39 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_40, %ptr_41 = arts.db_acquire[<inout>] (%guid_5 : memref<?xi64>, %ptr_6 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%55], sizes[%48]), offsets[%65], sizes[%66] element_offsets[%c0, %c0, %55] element_sizes[%c48, %c48, %48] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_41 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_42, %ptr_43 = arts.db_acquire[<inout>] (%guid_7 : memref<?xi64>, %ptr_8 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%55], sizes[%48]), offsets[%65], sizes[%66] element_offsets[%c0, %c0, %55] element_sizes[%c48, %c48, %48] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_43 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %67 = arts_rt.edt_param_pack(%49, %51, %53, %55, %65, %66, %48, %65, %66, %65, %66) : index, index, index, index, index, index, index, index, index, index, index : memref<?xi64>
        %68 = arts_rt.edt_param_pack(%49, %51, %53, %55, %66, %66, %66) : index, index, index, index, index, index, index : memref<?xi64>
        %69 = arts_rt.state_pack(%49, %51, %53, %55, %4, %c0, %c1_i32, %c1, %c48, %cst_0, %cst, %c47) : index, index, index, index, f64, index, i32, index, index, f64, f64, index -> memref<12xi64>
        %70 = memref.load %guid_24[%c0] : memref<?xi64>
        %71 = arts_rt.dep_bind(%70, %c0_i64)
        %72 = memref.load %guid_26[%c0] : memref<?xi64>
        %73 = arts_rt.dep_bind(%72, %c0_i64)
        %74 = memref.load %guid_28[%c0] : memref<?xi64>
        %75 = arts_rt.dep_bind(%74, %c0_i64)
        %76 = memref.load %guid_30[%c0] : memref<?xi64>
        %77 = arts_rt.dep_bind(%76, %c0_i64)
        %78 = memref.load %guid_32[%c0] : memref<?xi64>
        %79 = arts_rt.dep_bind(%78, %c0_i64)
        %80 = memref.load %guid_34[%c0] : memref<?xi64>
        %81 = arts_rt.dep_bind(%80, %c0_i64)
        %82 = memref.load %guid_36[%c0] : memref<?xi64>
        %83 = arts_rt.dep_bind(%82, %c0_i64)
        %84 = memref.load %guid_38[%c0] : memref<?xi64>
        %85 = arts_rt.dep_bind(%84, %c2_i64)
        %86 = memref.load %guid_40[%c0] : memref<?xi64>
        %87 = arts_rt.dep_bind(%86, %c2_i64)
        %88 = memref.load %guid_42[%c0] : memref<?xi64>
        %89 = arts_rt.dep_bind(%88, %c2_i64)
        %90 = arith.index_cast %66 : index to i32
        %91 = arith.addi %90, %c7_i32 : i32
        %92 = arith.addi %91, %90 : i32
        %93 = arith.addi %92, %90 : i32
        %94 = arts_rt.edt_create(%68 : memref<?xi64>) depCount(%93) route(%c-1_i32) epoch(%38 : i64) {arts.create_id = 213000 : i64, arts.plan.kernel_family = "uniform", outlined_func = "__arts_edt_1"}
        %95 = arith.addi %55, %48 : index
        %96 = arith.muli %65, %c48 : index
        %97 = arith.cmpi ult, %55, %c48 : index
        %98 = arith.cmpi ule, %95, %c48 : index
        %99 = arith.andi %97, %98 : i1
        %100 = arith.cmpi ule, %66, %c1 : index
        %101 = arith.cmpi uge, %55, %96 : index
        %102 = arith.subi %55, %96 : index
        %103 = arith.select %101, %102, %c0 : index
        %104 = arith.minui %103, %c48 : index
        %105 = arith.cmpi uge, %95, %96 : index
        %106 = arith.subi %95, %96 : index
        %107 = arith.select %105, %106, %c0 : index
        %108 = arith.minui %107, %c48 : index
        %109 = arith.cmpi uge, %108, %104 : index
        %110 = arith.subi %108, %104 : index
        %111 = arith.select %109, %110, %c0 : index
        %112 = arith.muli %66, %c48 : index
        %113 = arith.addi %96, %112 : index
        %114 = arith.cmpi ule, %55, %96 : index
        %115 = arith.cmpi uge, %95, %113 : index
        %116 = arith.andi %114, %115 : i1
        %117 = arith.ori %100, %116 : i1
        %118 = arith.ori %99, %117 : i1
        %119 = arith.select %100, %104, %c0 : index
        %120 = arith.select %100, %111, %c48 : index
        %121 = arith.select %99, %55, %119 : index
        %122 = arith.select %99, %48, %120 : index
        %123 = arith.cmpi eq, %121, %c0 : index
        %124 = arith.cmpi eq, %122, %c48 : index
        %125 = arith.andi %123, %124 : i1
        %126 = arith.xori %125, %true : i1
        %127 = arith.andi %118, %125 : i1
        %128 = arith.andi %126, %127 : i1
        %129 = arith.muli %121, %c8 : index
        %130 = arith.muli %122, %c18432 : index
        %131 = arith.select %128, %129, %c0 : index
        %132 = arith.select %128, %130, %c0 : index
        arts_rt.rec_dep %94(%guid_24, %guid_26, %guid_28, %guid_30, %guid_32, %guid_34, %guid_36, %guid_38, %guid_40, %guid_42 : memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>) bounds_valids(%true, %true, %true, %true, %true, %true, %true, %true, %true, %true) byte_offsets(%c0, %c0, %c0, %c0, %c0, %c0, %c0, %131, %131, %131) byte_sizes(%c0, %c0, %c0, %c0, %c0, %c0, %c0, %132, %132, %132) {acquire_modes = array<i32: 1, 1, 1, 1, 1, 1, 1, 2, 2, 2>}
      }
    } {arts.id = 183 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 46 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 4 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "recovered"}
    arts_rt.wait_on_epoch %38 : i64
    arts.db_release(%ptr_37) : memref<?x!llvm.ptr>
    arts.db_release(%ptr_35) : memref<?x!llvm.ptr>
    arts.db_release(%ptr_33) : memref<?x!llvm.ptr>
    arts.db_release(%ptr_31) : memref<?x!llvm.ptr>
    arts.db_release(%ptr_29) : memref<?x!llvm.ptr>
    arts.db_release(%ptr_27) : memref<?x!llvm.ptr>
    arts.db_release(%ptr_25) : memref<?x!llvm.ptr>
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %39 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%39, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_4, %alloca[] : memref<f64>
    %40 = llvm.load %8 : !llvm.ptr -> !llvm.ptr
    %41 = polygeist.pointer2memref %40 : !llvm.ptr to memref<?x?x?xf64>
    %42 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %43 = polygeist.pointer2memref %42 : !llvm.ptr to memref<?x?x?xf64>
    %44 = llvm.load %10 : !llvm.ptr -> !llvm.ptr
    %45 = polygeist.pointer2memref %44 : !llvm.ptr to memref<?x?x?xf64>
    scf.for %arg0 = %c0 to %c48 step %c1 {
      %47 = polygeist.load %41[%arg0, %arg0, %arg0] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
      %48 = polygeist.load %43[%arg0, %arg0, %arg0] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
      %49 = arith.addf %47, %48 : f64
      %50 = polygeist.load %45[%arg0, %arg0, %arg0] sizes(%c48, %c48, %c48) : memref<?x?x?xf64> -> f64
      %51 = arith.addf %49, %50 : f64
      %52 = memref.load %alloca[] : memref<f64>
      %53 = arith.addf %52, %51 : f64
      memref.store %53, %alloca[] : memref<f64>
    } {arts.id = 188 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 48 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:145:3">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%cst_4) : (f64) -> ()
    call @carts_phase_timer_stop(%39) : (memref<?xi8>) -> ()
    %46 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%46, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%46) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_7) : memref<?xi64>
    arts.db_free(%ptr_8) : memref<?x!llvm.ptr>
    arts.db_free(%guid_9) : memref<?xi64>
    arts.db_free(%ptr_10) : memref<?x!llvm.ptr>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?x!llvm.ptr>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?x!llvm.ptr>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?x!llvm.ptr>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?x!llvm.ptr>
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?x!llvm.ptr>
    arts.db_free(%guid_5) : memref<?xi64>
    arts.db_free(%ptr_6) : memref<?x!llvm.ptr>
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?x!llvm.ptr>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?x!llvm.ptr>
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
