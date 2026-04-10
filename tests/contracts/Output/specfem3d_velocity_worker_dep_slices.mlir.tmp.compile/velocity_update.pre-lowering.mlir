module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %c288 = arith.constant 288 : index
    %c384 = arith.constant 384 : index
    %cst = arith.constant 1.000000e+00 : f64
    %cst_0 = arith.constant 1.000000e-03 : f64
    %c287 = arith.constant 287 : index
    %c10 = arith.constant 10 : index
    %0:7 = arts_rt.edt_param_unpack %arg1 : memref<?xi64> : (index, index, index, index, index, index, index)
    %1 = arith.addi %0#3, %0#3 : index
    %2 = arith.addi %1, %0#3 : index
    %3 = arith.addi %2, %0#3 : index
    %4 = arith.addi %3, %0#3 : index
    %5 = arith.addi %4, %0#3 : index
    %6 = arith.addi %5, %0#3 : index
    %7 = arith.addi %6, %0#4 : index
    %8 = arith.addi %7, %0#5 : index
    %9 = arith.subi %c1, %0#0 : index
    %10 = arith.cmpi slt, %9, %c0 : index
    %11 = arith.select %10, %c0, %9 : index
    %12 = arith.cmpi slt, %0#1, %c0 : index
    %13 = arith.select %12, %c0, %0#1 : index
    %14 = arith.minui %13, %0#2 : index
    %guid, %ptr = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_1, %ptr_2 = arts_rt.dep_gep(%arg3) offset[%0#3 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_3, %ptr_4 = arts_rt.dep_gep(%arg3) offset[%1 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_5, %ptr_6 = arts_rt.dep_gep(%arg3) offset[%2 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_7, %ptr_8 = arts_rt.dep_gep(%arg3) offset[%3 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_9, %ptr_10 = arts_rt.dep_gep(%arg3) offset[%4 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_11, %ptr_12 = arts_rt.dep_gep(%arg3) offset[%5 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_13, %ptr_14 = arts_rt.dep_gep(%arg3) offset[%6 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_15, %ptr_16 = arts_rt.dep_gep(%arg3) offset[%7 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_17, %ptr_18 = arts_rt.dep_gep(%arg3) offset[%8 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %15 = llvm.load %ptr : !llvm.ptr -> !llvm.ptr
    %16 = llvm.load %ptr_2 : !llvm.ptr -> !llvm.ptr
    %17 = llvm.load %ptr_4 : !llvm.ptr -> !llvm.ptr
    %18 = llvm.load %ptr_6 : !llvm.ptr -> !llvm.ptr
    %19 = llvm.load %ptr_8 : !llvm.ptr -> !llvm.ptr
    %20 = llvm.load %ptr_10 : !llvm.ptr -> !llvm.ptr
    %21 = llvm.load %ptr_12 : !llvm.ptr -> !llvm.ptr
    %22 = llvm.load %ptr_14 : !llvm.ptr -> !llvm.ptr
    %23 = llvm.load %ptr_16 : !llvm.ptr -> !llvm.ptr
    %24 = llvm.load %ptr_18 : !llvm.ptr -> !llvm.ptr
    %25 = polygeist.pointer2memref %15 : !llvm.ptr to memref<?x?x?xf64>
    %26 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?xf64>
    %27 = polygeist.pointer2memref %17 : !llvm.ptr to memref<?x?x?xf64>
    %28 = polygeist.pointer2memref %18 : !llvm.ptr to memref<?x?x?xf64>
    %29 = polygeist.pointer2memref %19 : !llvm.ptr to memref<?x?x?xf64>
    %30 = polygeist.pointer2memref %20 : !llvm.ptr to memref<?x?x?xf64>
    %31 = polygeist.pointer2memref %21 : !llvm.ptr to memref<?x?x?xf64>
    %32 = polygeist.pointer2memref %22 : !llvm.ptr to memref<?x?x?xf64>
    %33 = polygeist.pointer2memref %23 : !llvm.ptr to memref<?x?x?xf64>
    %34 = polygeist.pointer2memref %24 : !llvm.ptr to memref<?x?x?xf64>
    scf.for %arg4 = %c0 to %c10 step %c1 {
      scf.for %arg5 = %11 to %14 step %c1 {
        %35 = arith.addi %0#0, %arg5 : index
        %36 = arith.index_cast %35 : index to i32
        %37 = arith.addi %36, %c1_i32 : i32
        %38 = arith.index_cast %37 : i32 to index
        scf.for %arg6 = %c1 to %c287 step %c1 {
          %39 = arith.index_cast %arg6 : index to i32
          %40 = arith.addi %39, %c1_i32 : i32
          %41 = arith.index_cast %40 : i32 to index
          scf.for %arg7 = %c1 to %c287 step %c1 {
            %42 = arith.index_cast %arg7 : index to i32
            %43 = polygeist.load %25[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %44 = arith.divf %cst, %43 : f64
            %45 = arith.addi %42, %c1_i32 : i32
            %46 = arith.index_cast %45 : i32 to index
            %47 = polygeist.load %26[%46, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %48 = polygeist.load %26[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %49 = arith.subf %47, %48 : f64
            %50 = polygeist.load %27[%arg7, %41, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %51 = polygeist.load %27[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %52 = arith.subf %50, %51 : f64
            %53 = arith.addf %49, %52 : f64
            %54 = polygeist.load %28[%arg7, %arg6, %38] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %55 = polygeist.load %28[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %56 = arith.subf %54, %55 : f64
            %57 = arith.addf %53, %56 : f64
            %58 = polygeist.load %27[%46, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %59 = polygeist.load %27[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %60 = arith.subf %58, %59 : f64
            %61 = polygeist.load %29[%arg7, %41, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %62 = polygeist.load %29[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %63 = arith.subf %61, %62 : f64
            %64 = arith.addf %60, %63 : f64
            %65 = polygeist.load %30[%arg7, %arg6, %38] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %66 = polygeist.load %30[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %67 = arith.subf %65, %66 : f64
            %68 = arith.addf %64, %67 : f64
            %69 = polygeist.load %28[%46, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %70 = polygeist.load %28[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %71 = arith.subf %69, %70 : f64
            %72 = polygeist.load %30[%arg7, %41, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %73 = polygeist.load %30[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %74 = arith.subf %72, %73 : f64
            %75 = arith.addf %71, %74 : f64
            %76 = polygeist.load %31[%arg7, %arg6, %38] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %77 = polygeist.load %31[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %78 = arith.subf %76, %77 : f64
            %79 = arith.addf %75, %78 : f64
            %80 = arith.mulf %44, %cst_0 : f64
            %81 = arith.mulf %80, %57 : f64
            %82 = polygeist.load %32[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %83 = arith.addf %82, %81 : f64
            polygeist.store %83, %32[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
            %84 = arith.mulf %80, %68 : f64
            %85 = polygeist.load %33[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %86 = arith.addf %85, %84 : f64
            polygeist.store %86, %33[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
            %87 = arith.mulf %80, %79 : f64
            %88 = polygeist.load %34[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
            %89 = arith.addf %88, %87 : f64
            polygeist.store %89, %34[%arg7, %arg6, %35] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          } {arts.id = 190 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 286 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
        } {arts.id = 192 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 286 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 7 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 214 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "velocity_update.c:133:5">, arts.metadata_provenance = "exact"}
    }
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("specfem_velocity_update\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5 = arith.constant 5 : index
    %true = arith.constant true
    %c2_i64 = arith.constant 2 : i64
    %c0_i64 = arith.constant 0 : i64
    %c64 = arith.constant 64 : index
    %c6 = arith.constant 6 : index
    %c287 = arith.constant 287 : index
    %cst = arith.constant 1.000000e-03 : f64
    %cst_0 = arith.constant 1.000000e+00 : f64
    %c-1_i32 = arith.constant -1 : i32
    %c10 = arith.constant 10 : index
    %c383 = arith.constant 383 : index
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
    %c384 = arith.constant 384 : index
    %c1 = arith.constant 1 : index
    %c288 = arith.constant 288 : index
    %c0 = arith.constant 0 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_4 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 198 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:141:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 198 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 67000 : i64, arts.id = 67 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:91:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 67 : i64, lastUseId = 68 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 67 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_5, %ptr_6 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 69000 : i64, arts.id = 69 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:92:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 69 : i64, lastUseId = 70 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 69 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_7, %ptr_8 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 71000 : i64, arts.id = 71 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:93:18", totalAccesses = 4 : i64, readCount = 2 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 71 : i64, lastUseId = 72 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 32 : i64>, arts.metadata_origin_id = 71 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_9, %ptr_10 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 73000 : i64, arts.id = 73 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:94:19", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 73 : i64, lastUseId = 74 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 16 : i64>, arts.metadata_origin_id = 73 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_11, %ptr_12 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 75000 : i64, arts.id = 75 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:95:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 75 : i64, lastUseId = 76 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 75 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_13, %ptr_14 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 77000 : i64, arts.id = 77 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:96:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 77 : i64, lastUseId = 78 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 77 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_15, %ptr_16 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 79000 : i64, arts.id = 79 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:97:19", totalAccesses = 3 : i64, readCount = 2 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 79 : i64, lastUseId = 80 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 5 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 24 : i64>, arts.metadata_origin_id = 79 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_17, %ptr_18 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 81000 : i64, arts.id = 81 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:98:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 81 : i64, lastUseId = 82 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 81 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_19, %ptr_20 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 83000 : i64, arts.id = 83 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:99:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 83 : i64, lastUseId = 84 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 83 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_21, %ptr_22 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c288, %c288, %c384] {arts.create_id = 85000 : i64, arts.id = 85 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "velocity_update.c:100:19", totalAccesses = 5 : i64, readCount = 4 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 254803968 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 85 : i64, lastUseId = 86 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 40 : i64>, arts.metadata_origin_id = 85 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_23 = memref.alloca() {arts.id = 88 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "velocity_update.c:127:3", totalAccesses = 11 : i64, readCount = 8 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 88 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
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
    scf.for %arg0 = %c0 to %c288 step %c1 {
      scf.for %arg1 = %c0 to %c288 step %c1 {
        scf.for %arg2 = %c0 to %c384 step %c1 {
          polygeist.store %cst_4, %19[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          polygeist.store %cst_4, %21[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          polygeist.store %cst_4, %23[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %47 = memref.load %alloca_23[] : memref<i32>
          %48 = arith.remsi %47, %c11_i32 : i32
          %49 = arith.sitofp %48 : i32 to f64
          %50 = arith.addf %49, %cst_1 : f64
          polygeist.store %50, %25[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %51 = memref.load %alloca_23[] : memref<i32>
          %52 = arith.muli %51, %c2_i32 : i32
          %53 = arith.remsi %52, %c17_i32 : i32
          %54 = arith.sitofp %53 : i32 to f64
          %55 = arith.mulf %54, %cst_2 : f64
          polygeist.store %55, %27[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %56 = memref.load %alloca_23[] : memref<i32>
          %57 = arith.muli %56, %c3_i32 : i32
          %58 = arith.remsi %57, %c19_i32 : i32
          %59 = arith.sitofp %58 : i32 to f64
          %60 = arith.mulf %59, %cst_2 : f64
          polygeist.store %60, %29[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %61 = memref.load %alloca_23[] : memref<i32>
          %62 = arith.muli %61, %c5_i32 : i32
          %63 = arith.remsi %62, %c23_i32 : i32
          %64 = arith.sitofp %63 : i32 to f64
          %65 = arith.mulf %64, %cst_2 : f64
          polygeist.store %65, %31[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %66 = memref.load %alloca_23[] : memref<i32>
          %67 = arith.muli %66, %c7_i32 : i32
          %68 = arith.remsi %67, %c13_i32 : i32
          %69 = arith.sitofp %68 : i32 to f64
          %70 = arith.mulf %69, %cst_3 : f64
          polygeist.store %70, %33[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %71 = memref.load %alloca_23[] : memref<i32>
          %72 = arith.muli %71, %c11_i32 : i32
          %73 = arith.remsi %72, %c29_i32 : i32
          %74 = arith.sitofp %73 : i32 to f64
          %75 = arith.mulf %74, %cst_3 : f64
          polygeist.store %75, %35[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %76 = memref.load %alloca_23[] : memref<i32>
          %77 = arith.muli %76, %c13_i32 : i32
          %78 = arith.remsi %77, %c31_i32 : i32
          %79 = arith.sitofp %78 : i32 to f64
          %80 = arith.mulf %79, %cst_3 : f64
          polygeist.store %80, %37[%arg0, %arg1, %arg2] sizes(%c288, %c288, %c384) : f64, memref<?x?x?xf64>
          %81 = memref.load %alloca_23[] : memref<i32>
          %82 = arith.addi %81, %c1_i32 : i32
          memref.store %82, %alloca_23[] : memref<i32>
        } {arts.id = 139 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 384 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 141 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 288 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 110 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 288 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:127:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %38 = arts_rt.create_epoch : i64
    scf.for %arg0 = %c0 to %c64 step %c1 {
      %47 = arith.muli %arg0, %c6 : index
      %48 = arith.cmpi uge, %47, %c383 : index
      %49 = arith.subi %c383, %47 : index
      %50 = arith.select %48, %c0, %49 : index
      %51 = arith.minui %50, %c6 : index
      %52 = arith.cmpi ugt, %51, %c0 : index
      scf.if %52 {
        %53 = arith.addi %47, %51 : index
        %54 = arith.cmpi uge, %47, %c1 : index
        %55 = arith.subi %47, %c1 : index
        %56 = arith.select %54, %55, %c0 : index
        %57 = arith.addi %53, %c1 : index
        %58 = arith.minui %57, %c384 : index
        %59 = arith.cmpi uge, %58, %56 : index
        %60 = arith.subi %58, %56 : index
        %61 = arith.select %59, %60, %c0 : index
        %62 = arith.maxui %61, %c1 : index
        %63 = arith.divui %56, %c288 : index
        %64 = arith.addi %56, %62 : index
        %65 = arith.subi %64, %c1 : index
        %66 = arith.divui %65, %c288 : index
        %67 = arith.cmpi ugt, %63, %c0 : index
        %68 = arith.select %67, %66, %c0 : index
        %69 = arith.subi %68, %63 : index
        %70 = arith.addi %69, %c1 : index
        %71 = arith.select %67, %c0, %63 : index
        %72 = arith.select %67, %c0, %70 : index
        %guid_24, %ptr_25 = arts.db_acquire[<in>] (%guid_9 : memref<?xi64>, %ptr_10 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_25 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_26, %ptr_27 = arts.db_acquire[<in>] (%guid_11 : memref<?xi64>, %ptr_12 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_27 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_28, %ptr_29 = arts.db_acquire[<in>] (%guid_17 : memref<?xi64>, %ptr_18 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_29 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_30, %ptr_31 = arts.db_acquire[<in>] (%guid_19 : memref<?xi64>, %ptr_20 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_31 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_32, %ptr_33 = arts.db_acquire[<in>] (%guid_13 : memref<?xi64>, %ptr_14 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_33 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_34, %ptr_35 = arts.db_acquire[<in>] (%guid_21 : memref<?xi64>, %ptr_22 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_35 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_36, %ptr_37 = arts.db_acquire[<in>] (%guid_15 : memref<?xi64>, %ptr_16 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%56], sizes[%61]), offsets[%71], sizes[%72] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_37 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %73 = arith.divui %47, %c384 : index
        %74 = arith.addi %47, %c5 : index
        %75 = arith.divui %74, %c384 : index
        %76 = arith.cmpi ugt, %73, %c0 : index
        %77 = arith.select %76, %75, %c0 : index
        %78 = arith.subi %77, %73 : index
        %79 = arith.addi %78, %c1 : index
        %80 = arith.select %76, %c0, %73 : index
        %81 = arith.select %76, %c0, %79 : index
        %guid_38, %ptr_39 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%47], sizes[%c6]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_39 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_40, %ptr_41 = arts.db_acquire[<inout>] (%guid_5 : memref<?xi64>, %ptr_6 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%47], sizes[%c6]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_41 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_42, %ptr_43 = arts.db_acquire[<inout>] (%guid_7 : memref<?xi64>, %ptr_8 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%47], sizes[%c6]), offsets[%80], sizes[%81] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [288, 288, 6], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_43 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c288] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %82 = arts_rt.edt_param_pack(%47, %49, %51, %71, %72, %56, %61, %80, %81, %80, %81, %80, %81) : index, index, index, index, index, index, index, index, index, index, index, index, index : memref<?xi64>
        %83 = arts_rt.edt_param_pack(%47, %49, %51, %72, %81, %81, %81) : index, index, index, index, index, index, index : memref<?xi64>
        %84 = arts_rt.state_pack(%47, %49, %51, %4, %c1, %c0, %c1_i32, %c288, %c384, %cst_0, %cst, %c287, %c10) : index, index, index, f64, index, index, i32, index, index, f64, f64, index, index -> memref<13xi64>
        %85 = memref.load %guid_24[%c0] : memref<?xi64>
        %86 = arts_rt.dep_bind(%85, %c0_i64)
        %87 = memref.load %guid_26[%c0] : memref<?xi64>
        %88 = arts_rt.dep_bind(%87, %c0_i64)
        %89 = memref.load %guid_28[%c0] : memref<?xi64>
        %90 = arts_rt.dep_bind(%89, %c0_i64)
        %91 = memref.load %guid_30[%c0] : memref<?xi64>
        %92 = arts_rt.dep_bind(%91, %c0_i64)
        %93 = memref.load %guid_32[%c0] : memref<?xi64>
        %94 = arts_rt.dep_bind(%93, %c0_i64)
        %95 = memref.load %guid_34[%c0] : memref<?xi64>
        %96 = arts_rt.dep_bind(%95, %c0_i64)
        %97 = memref.load %guid_36[%c0] : memref<?xi64>
        %98 = arts_rt.dep_bind(%97, %c0_i64)
        %99 = memref.load %guid_38[%c0] : memref<?xi64>
        %100 = arts_rt.dep_bind(%99, %c2_i64)
        %101 = memref.load %guid_40[%c0] : memref<?xi64>
        %102 = arts_rt.dep_bind(%101, %c2_i64)
        %103 = memref.load %guid_42[%c0] : memref<?xi64>
        %104 = arts_rt.dep_bind(%103, %c2_i64)
        %105 = arith.index_cast %72 : index to i32
        %106 = arith.addi %105, %105 : i32
        %107 = arith.addi %106, %105 : i32
        %108 = arith.addi %107, %105 : i32
        %109 = arith.addi %108, %105 : i32
        %110 = arith.addi %109, %105 : i32
        %111 = arith.addi %110, %105 : i32
        %112 = arith.index_cast %81 : index to i32
        %113 = arith.addi %111, %112 : i32
        %114 = arith.addi %113, %112 : i32
        %115 = arith.addi %114, %112 : i32
        %116 = arts_rt.edt_create(%83 : memref<?xi64>) depCount(%115) route(%c-1_i32) epoch(%38 : i64) {arts.create_id = 215000 : i64, arts.plan.kernel_family = "uniform", outlined_func = "__arts_edt_1"}
        arts_rt.rec_dep %116(%guid_24, %guid_26, %guid_28, %guid_30, %guid_32, %guid_34, %guid_36, %guid_38, %guid_40, %guid_42 : memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>) bounds_valids(%true, %true, %true, %true, %true, %true, %true, %true, %true, %true) {acquire_modes = array<i32: 1, 1, 1, 1, 1, 1, 1, 2, 2, 2>}
      }
    }
    arts_rt.wait_on_epoch %38 : i64
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
    scf.for %arg0 = %c0 to %c288 step %c1 {
      %47 = polygeist.load %41[%arg0, %arg0, %arg0] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
      %48 = polygeist.load %43[%arg0, %arg0, %arg0] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
      %49 = arith.addf %47, %48 : f64
      %50 = polygeist.load %45[%arg0, %arg0, %arg0] sizes(%c288, %c288, %c384) : memref<?x?x?xf64> -> f64
      %51 = arith.addf %49, %50 : f64
      %52 = memref.load %alloca[] : memref<f64>
      %53 = arith.addf %52, %51 : f64
      memref.store %53, %alloca[] : memref<f64>
    } {arts.id = 190 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 288 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "velocity_update.c:145:3">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%cst_4) : (f64) -> ()
    call @carts_phase_timer_stop(%39) : (memref<?xi8>) -> ()
    %46 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%46, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%46) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_19) : memref<?xi64>
    arts.db_free(%ptr_20) : memref<?x!llvm.ptr>
    arts.db_free(%guid_17) : memref<?xi64>
    arts.db_free(%ptr_18) : memref<?x!llvm.ptr>
    arts.db_free(%guid_11) : memref<?xi64>
    arts.db_free(%ptr_12) : memref<?x!llvm.ptr>
    arts.db_free(%guid_9) : memref<?xi64>
    arts.db_free(%ptr_10) : memref<?x!llvm.ptr>
    arts.db_free(%guid_15) : memref<?xi64>
    arts.db_free(%ptr_16) : memref<?x!llvm.ptr>
    arts.db_free(%guid_5) : memref<?xi64>
    arts.db_free(%ptr_6) : memref<?x!llvm.ptr>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?x!llvm.ptr>
    arts.db_free(%guid_13) : memref<?xi64>
    arts.db_free(%ptr_14) : memref<?x!llvm.ptr>
    arts.db_free(%guid_7) : memref<?xi64>
    arts.db_free(%ptr_8) : memref<?x!llvm.ptr>
    arts.db_free(%guid_21) : memref<?xi64>
    arts.db_free(%ptr_22) : memref<?x!llvm.ptr>
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
