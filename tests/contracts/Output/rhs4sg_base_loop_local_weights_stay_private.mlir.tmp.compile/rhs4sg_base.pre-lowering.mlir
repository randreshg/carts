module attributes {arts.runtime_config_data = "[ARTS]\0A# Default ARTS configuration for examples (v2 format)\0Aworker_threads=8\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %c20 = arith.constant 20 : index
    %c2_i32 = arith.constant 2 : i32
    %c-2 = arith.constant -2 : index
    %cst_4 = arith.constant 5.000000e-01 : f32
    %c16 = arith.constant 16 : index
    %0:7 = arts_rt.edt_param_unpack %arg1 : memref<?xi64> : (index, index, index, index, index, index, index)
    %1 = arith.addi %0#4, %0#4 : index
    %2 = arith.addi %1, %0#5 : index
    %alloca = memref.alloca() {arts.id = 94 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 94 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
    %alloca_5 = memref.alloca() {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 103 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_6 = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_7 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %3 = "polygeist.undef"() : () -> f32
    %4 = arith.subi %c1, %0#0 : index
    %5 = arith.cmpi slt, %4, %c0 : index
    %6 = arith.select %5, %c0, %4 : index
    %7 = arith.cmpi slt, %0#1, %c0 : index
    %8 = arith.select %7, %c0, %0#1 : index
    %9 = arith.minui %8, %0#2 : index
    %guid, %ptr = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_8, %ptr_9 = arts_rt.dep_gep(%arg3) offset[%0#4 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_10, %ptr_11 = arts_rt.dep_gep(%arg3) offset[%1 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %guid_12, %ptr_13 = arts_rt.dep_gep(%arg3) offset[%2 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %10 = llvm.load %ptr : !llvm.ptr -> !llvm.ptr
    %11 = llvm.load %ptr_9 : !llvm.ptr -> !llvm.ptr
    %12 = llvm.load %ptr_11 : !llvm.ptr -> !llvm.ptr
    %13 = llvm.load %ptr_13 : !llvm.ptr -> !llvm.ptr
    %14 = polygeist.pointer2memref %10 : !llvm.ptr to memref<?x?x?xf32>
    %15 = polygeist.pointer2memref %11 : !llvm.ptr to memref<?x?x?xf32>
    %16 = polygeist.pointer2memref %12 : !llvm.ptr to memref<?x?x?x?xf32>
    %17 = polygeist.pointer2memref %13 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg4 = %c0 to %c2 step %c1 {
      memref.store %cst, %alloca[%c0] : memref<5xf32>
      memref.store %cst_0, %alloca[%c1] : memref<5xf32>
      memref.store %cst_1, %alloca[%c2] : memref<5xf32>
      memref.store %cst_2, %alloca[%c3] : memref<5xf32>
      memref.store %cst_3, %alloca[%c4] : memref<5xf32>
      scf.for %arg5 = %6 to %9 step %c1 {
        %18 = arith.addi %0#3, %arg5 : index
        memref.store %3, %alloca_5[] : memref<f32>
        memref.store %3, %alloca_6[] : memref<f32>
        memref.store %3, %alloca_7[] : memref<f32>
        %19 = arith.index_cast %18 : index to i32
        %20 = arith.addi %19, %c1_i32 : i32
        %21 = arith.index_cast %20 : i32 to index
        %22 = arith.addi %19, %c-1_i32 : i32
        %23 = arith.index_cast %22 : i32 to index
        scf.for %arg6 = %c4 to %c16 step %c1 {
          %24 = arith.index_cast %arg6 : index to i32
          %25 = arith.addi %24, %c1_i32 : i32
          %26 = arith.index_cast %25 : i32 to index
          %27 = arith.addi %24, %c-1_i32 : i32
          %28 = arith.index_cast %27 : i32 to index
          scf.for %arg7 = %c4 to %c16 step %c1 {
            %29 = arith.index_cast %arg7 : index to i32
            %30 = polygeist.load %14[%arg7, %arg6, %18] sizes(%c20, %c20, %c20) : memref<?x?x?xf32> -> f32
            %31 = polygeist.load %15[%arg7, %arg6, %18] sizes(%c20, %c20, %c20) : memref<?x?x?xf32> -> f32
            memref.store %cst_1, %alloca_7[] : memref<f32>
            scf.for %arg8 = %c-2 to %c3 step %c1 {
              %56 = arith.index_cast %arg8 : index to i32
              %57 = arith.addi %56, %c2_i32 : i32
              %58 = arith.index_cast %57 : i32 to index
              %59 = memref.load %alloca[%58] : memref<5xf32>
              %60 = arith.addi %29, %56 : i32
              %61 = arith.index_cast %60 : i32 to index
              %62 = polygeist.load %16[%c0, %61, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %63 = arith.addi %24, %56 : i32
              %64 = arith.index_cast %63 : i32 to index
              %65 = polygeist.load %16[%c0, %arg7, %64, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %66 = arith.addf %62, %65 : f32
              %67 = arith.addi %19, %56 : i32
              %68 = arith.index_cast %67 : i32 to index
              %69 = polygeist.load %16[%c0, %arg7, %arg6, %68] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %70 = arith.addf %66, %69 : f32
              %71 = arith.mulf %59, %70 : f32
              %72 = memref.load %alloca_7[] : memref<f32>
              %73 = arith.addf %72, %71 : f32
              memref.store %73, %alloca_7[] : memref<f32>
            } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            %32 = arith.addi %29, %c1_i32 : i32
            %33 = arith.index_cast %32 : i32 to index
            %34 = polygeist.load %16[%c0, %33, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
            %35 = arith.addi %29, %c-1_i32 : i32
            %36 = arith.index_cast %35 : i32 to index
            %37 = polygeist.load %16[%c0, %36, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
            %38 = arith.subf %34, %37 : f32
            %39 = arith.mulf %30, %cst_1 : f32
            %40 = arith.addf %31, %30 : f32
            %41 = arith.mulf %40, %38 : f32
            %42 = arith.mulf %41, %cst_4 : f32
            %43 = arith.addf %39, %42 : f32
            polygeist.store %43, %17[%c0, %arg7, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : f32, memref<?x?x?x?xf32>
            memref.store %cst_1, %alloca_6[] : memref<f32>
            scf.for %arg8 = %c-2 to %c3 step %c1 {
              %56 = arith.index_cast %arg8 : index to i32
              %57 = arith.addi %56, %c2_i32 : i32
              %58 = arith.index_cast %57 : i32 to index
              %59 = memref.load %alloca[%58] : memref<5xf32>
              %60 = arith.addi %29, %56 : i32
              %61 = arith.index_cast %60 : i32 to index
              %62 = polygeist.load %16[%c1, %61, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %63 = arith.addi %24, %56 : i32
              %64 = arith.index_cast %63 : i32 to index
              %65 = polygeist.load %16[%c1, %arg7, %64, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %66 = arith.addf %62, %65 : f32
              %67 = arith.addi %19, %56 : i32
              %68 = arith.index_cast %67 : i32 to index
              %69 = polygeist.load %16[%c1, %arg7, %arg6, %68] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %70 = arith.addf %66, %69 : f32
              %71 = arith.mulf %59, %70 : f32
              %72 = memref.load %alloca_6[] : memref<f32>
              %73 = arith.addf %72, %71 : f32
              memref.store %73, %alloca_6[] : memref<f32>
            } {arts.id = 193 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            %44 = polygeist.load %16[%c1, %arg7, %26, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
            %45 = polygeist.load %16[%c1, %arg7, %28, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
            %46 = arith.subf %44, %45 : f32
            %47 = arith.mulf %40, %46 : f32
            %48 = arith.mulf %47, %cst_4 : f32
            %49 = arith.addf %39, %48 : f32
            polygeist.store %49, %17[%c1, %arg7, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : f32, memref<?x?x?x?xf32>
            memref.store %cst_1, %alloca_5[] : memref<f32>
            scf.for %arg8 = %c-2 to %c3 step %c1 {
              %56 = arith.index_cast %arg8 : index to i32
              %57 = arith.addi %56, %c2_i32 : i32
              %58 = arith.index_cast %57 : i32 to index
              %59 = memref.load %alloca[%58] : memref<5xf32>
              %60 = arith.addi %29, %56 : i32
              %61 = arith.index_cast %60 : i32 to index
              %62 = polygeist.load %16[%c2, %61, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %63 = arith.addi %24, %56 : i32
              %64 = arith.index_cast %63 : i32 to index
              %65 = polygeist.load %16[%c2, %arg7, %64, %18] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %66 = arith.addf %62, %65 : f32
              %67 = arith.addi %19, %56 : i32
              %68 = arith.index_cast %67 : i32 to index
              %69 = polygeist.load %16[%c2, %arg7, %arg6, %68] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
              %70 = arith.addf %66, %69 : f32
              %71 = arith.mulf %59, %70 : f32
              %72 = memref.load %alloca_5[] : memref<f32>
              %73 = arith.addf %72, %71 : f32
              memref.store %73, %alloca_5[] : memref<f32>
            } {arts.id = 229 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
            %50 = polygeist.load %16[%c2, %arg7, %arg6, %21] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
            %51 = polygeist.load %16[%c2, %arg7, %arg6, %23] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
            %52 = arith.subf %50, %51 : f32
            %53 = arith.mulf %40, %52 : f32
            %54 = arith.mulf %53, %cst_4 : f32
            %55 = arith.addf %39, %54 : f32
            polygeist.store %55, %17[%c2, %arg7, %arg6, %18] sizes(%c3, %c20, %c20, %c20) : f32, memref<?x?x?x?xf32>
          } {arts.id = 245 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 12 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
        } {arts.id = 247 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 12 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 260 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
    }
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4800 = arith.constant 4800 : index
    %c1600 = arith.constant 1600 : index
    %true = arith.constant true
    %c2_i64 = arith.constant 2 : i64
    %c0_i64 = arith.constant 0 : i64
    %c5 = arith.constant 5 : index
    %c13 = arith.constant 13 : index
    %cst = arith.constant 5.000000e-01 : f32
    %c-2 = arith.constant -2 : index
    %c2_i32 = arith.constant 2 : i32
    %c4 = arith.constant 4 : index
    %cst_0 = arith.constant -0.0833333358 : f32
    %cst_1 = arith.constant 0.666666686 : f32
    %cst_2 = arith.constant -0.666666686 : f32
    %cst_3 = arith.constant 0.0833333358 : f32
    %c16 = arith.constant 16 : index
    %c-1_i32 = arith.constant -1 : i32
    %c2 = arith.constant 2 : index
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
    %c20 = arith.constant 20 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c3 = arith.constant 3 : index
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_11 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %alloca = memref.alloca() {arts.id = 253 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:158:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 253 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c20, %c20, %c20] {arts.create_id = 35000 : i64, arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:116:17", totalAccesses = 16 : i64, readCount = 15 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 96000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 64 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_12, %ptr_13 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c20, %c20, %c20] {arts.create_id = 37000 : i64, arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:117:19", totalAccesses = 5 : i64, readCount = 1 : i64, writeCount = 4 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 96000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 37 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_14, %ptr_15 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c20, %c20, %c20] {arts.create_id = 39000 : i64, arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:119:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_16, %ptr_17 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c20, %c20, %c20] {arts.create_id = 41000 : i64, arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:120:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 32000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 41 : i64, lastUseId = 42 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 41 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %7 = "polygeist.undef"() : () -> i32
    %alloca_18 = memref.alloca() {arts.id = 44 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:144:3", totalAccesses = 11 : i64, readCount = 5 : i64, writeCount = 6 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 44 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 44 : i64>, arts.metadata_provenance = "exact"} : memref<i32>
    memref.store %7, %alloca_18[] : memref<i32>
    memref.store %c0_i32, %alloca_18[] : memref<i32>
    %8 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %9 = arts_rt.db_gep(%ptr_13 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %10 = llvm.load %8 : !llvm.ptr -> !llvm.ptr
    %11 = polygeist.pointer2memref %10 : !llvm.ptr to memref<?x?x?x?xf32>
    %12 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %13 = polygeist.pointer2memref %12 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c3 step %c1 {
      %25 = arith.index_cast %arg0 : index to i32
      memref.store %c0_i32, %alloca_18[] : memref<i32>
      %26 = arith.muli %25, %c17_i32 : i32
      %27 = arith.sitofp %25 : i32 to f32
      %28 = arith.mulf %27, %cst_5 : f32
      scf.for %arg1 = %c0 to %c20 step %c1 {
        scf.for %arg2 = %c0 to %c20 step %c1 {
          scf.for %arg3 = %c0 to %c20 step %c1 {
            %29 = memref.load %alloca_18[] : memref<i32>
            %30 = arith.addi %29, %26 : i32
            %31 = arith.remsi %30, %c23_i32 : i32
            %32 = arith.sitofp %31 : i32 to f32
            %33 = arith.mulf %32, %cst_4 : f32
            %34 = arith.subf %33, %28 : f32
            polygeist.store %34, %11[%arg0, %arg1, %arg2, %arg3] sizes(%c3, %c20, %c20, %c20) : f32, memref<?x?x?x?xf32>
            polygeist.store %cst_6, %13[%arg0, %arg1, %arg2, %arg3] sizes(%c3, %c20, %c20, %c20) : f32, memref<?x?x?x?xf32>
            %35 = memref.load %alloca_18[] : memref<i32>
            %36 = arith.addi %35, %c1_i32 : i32
            memref.store %36, %alloca_18[] : memref<i32>
          } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 70 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    memref.store %c0_i32, %alloca_18[] : memref<i32>
    %14 = arts_rt.db_gep(%ptr_15 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %15 = arts_rt.db_gep(%ptr_17 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %16 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
    %17 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?xf32>
    %18 = llvm.load %15 : !llvm.ptr -> !llvm.ptr
    %19 = polygeist.pointer2memref %18 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg0 = %c0 to %c20 step %c1 {
      scf.for %arg1 = %c0 to %c20 step %c1 {
        scf.for %arg2 = %c0 to %c20 step %c1 {
          %25 = memref.load %alloca_18[] : memref<i32>
          %26 = arith.remsi %25, %c11_i32 : i32
          %27 = arith.sitofp %26 : i32 to f32
          %28 = arith.mulf %27, %cst_8 : f32
          %29 = arith.addf %28, %cst_7 : f32
          polygeist.store %29, %17[%arg0, %arg1, %arg2] sizes(%c20, %c20, %c20) : f32, memref<?x?x?xf32>
          %30 = memref.load %alloca_18[] : memref<i32>
          %31 = arith.remsi %30, %c7_i32 : i32
          %32 = arith.sitofp %31 : i32 to f32
          %33 = arith.mulf %32, %cst_10 : f32
          %34 = arith.addf %33, %cst_9 : f32
          polygeist.store %34, %19[%arg0, %arg1, %arg2] sizes(%c20, %c20, %c20) : f32, memref<?x?x?xf32>
          %35 = memref.load %alloca_18[] : memref<i32>
          %36 = arith.addi %35, %c1_i32 : i32
          memref.store %36, %alloca_18[] : memref<i32>
        } {arts.id = 88 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 90 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    %20 = arts_rt.create_epoch : i64
    scf.for %arg0 = %c0 to %c5 step %c1 {
      %25 = arith.cmpi ult, %arg0, %c5 : index
      %26 = arith.select %25, %c1, %c0 : index
      %27 = arith.minui %arg0, %c5 : index
      %28 = arith.muli %27, %c3 : index
      %29 = arith.muli %26, %c3 : index
      %30 = arith.cmpi uge, %28, %c13 : index
      %31 = arith.subi %c13, %28 : index
      %32 = arith.select %30, %c0, %31 : index
      %33 = arith.minui %29, %32 : index
      %34 = arith.cmpi ugt, %33, %c0 : index
      scf.if %34 {
        %35 = arith.addi %28, %c3 : index
        %36 = arith.addi %35, %33 : index
        %37 = arith.cmpi uge, %35, %c1 : index
        %38 = arith.addi %28, %c2 : index
        %39 = arith.select %37, %38, %c0 : index
        %40 = arith.addi %36, %c1 : index
        %41 = arith.minui %40, %c20 : index
        %42 = arith.cmpi uge, %41, %39 : index
        %43 = arith.subi %41, %39 : index
        %44 = arith.select %42, %43, %c0 : index
        %45 = arith.maxui %44, %c1 : index
        %46 = arith.divui %39, %c20 : index
        %47 = arith.addi %39, %45 : index
        %48 = arith.subi %47, %c1 : index
        %49 = arith.divui %48, %c20 : index
        %50 = arith.cmpi ugt, %46, %c0 : index
        %51 = arith.select %50, %49, %c0 : index
        %52 = arith.subi %51, %46 : index
        %53 = arith.addi %52, %c1 : index
        %54 = arith.select %50, %c0, %46 : index
        %55 = arith.select %50, %c0, %53 : index
        %guid_19, %ptr_20 = arts.db_acquire[<in>] (%guid_14 : memref<?xi64>, %ptr_15 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%39], sizes[%44]), offsets[%54], sizes[%55] element_offsets[%c0, %c0, %39] element_sizes[%c20, %c20, %44] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_20 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %guid_21, %ptr_22 = arts.db_acquire[<in>] (%guid_16 : memref<?xi64>, %ptr_17 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%39], sizes[%44]), offsets[%54], sizes[%55] element_offsets[%c0, %c0, %39] element_sizes[%c20, %c20, %44] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_22 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %56 = arith.divui %39, %c3 : index
        %57 = arith.divui %48, %c3 : index
        %58 = arith.cmpi ugt, %56, %c0 : index
        %59 = arith.select %58, %57, %c0 : index
        %60 = arith.subi %59, %56 : index
        %61 = arith.addi %60, %c1 : index
        %62 = arith.select %58, %c0, %56 : index
        %63 = arith.select %58, %c0, %61 : index
        %guid_23, %ptr_24 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%39], sizes[%44]), offsets[%62], sizes[%63] element_offsets[%c0, %c0, %c0, %39] element_sizes[%c3, %c20, %c20, %44] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_24 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %64 = arith.maxui %29, %c1 : index
        %65 = arith.divui %35, %c20 : index
        %66 = arith.addi %35, %64 : index
        %67 = arith.subi %66, %c1 : index
        %68 = arith.divui %67, %c20 : index
        %69 = arith.cmpi ugt, %65, %c0 : index
        %70 = arith.select %69, %68, %c0 : index
        %71 = arith.subi %70, %65 : index
        %72 = arith.addi %71, %c1 : index
        %73 = arith.select %69, %c0, %65 : index
        %74 = arith.select %69, %c0, %72 : index
        %guid_25, %ptr_26 = arts.db_acquire[<inout>] (%guid_12 : memref<?xi64>, %ptr_13 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%35], sizes[%29]), offsets[%73], sizes[%74] element_offsets[%c0, %c0, %c0, %35] element_sizes[%c3, %c20, %c20, %29] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
        arts.lowering_contract(%ptr_26 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)
        %75 = arts_rt.edt_param_pack(%28, %31, %33, %35, %54, %55, %39, %44, %62, %63, %73, %74, %29) : index, index, index, index, index, index, index, index, index, index, index, index, index : memref<?xi64>
        %76 = arts_rt.edt_param_pack(%28, %31, %33, %35, %55, %63, %74) : index, index, index, index, index, index, index : memref<?xi64>
        %77 = arts_rt.state_pack(%28, %31, %33, %35, %cst_0, %c0, %cst_1, %c1, %cst_6, %c2, %cst_2, %c3, %cst_3, %c4, %c1_i32, %c-1_i32, %c20, %c2_i32, %c-2, %cst, %c16) : index, index, index, index, f32, index, f32, index, f32, index, f32, index, f32, index, i32, i32, index, i32, index, f32, index -> memref<21xi64>
        %78 = memref.load %guid_19[%c0] : memref<?xi64>
        %79 = arts_rt.dep_bind(%78, %c0_i64)
        %80 = memref.load %guid_21[%c0] : memref<?xi64>
        %81 = arts_rt.dep_bind(%80, %c0_i64)
        %82 = memref.load %guid_23[%c0] : memref<?xi64>
        %83 = arts_rt.dep_bind(%82, %c0_i64)
        %84 = memref.load %guid_25[%c0] : memref<?xi64>
        %85 = arts_rt.dep_bind(%84, %c2_i64)
        %86 = arith.index_cast %55 : index to i32
        %87 = arith.addi %86, %86 : i32
        %88 = arith.index_cast %63 : index to i32
        %89 = arith.addi %87, %88 : i32
        %90 = arith.index_cast %74 : index to i32
        %91 = arith.addi %89, %90 : i32
        %92 = arts_rt.edt_create(%76 : memref<?xi64>) depCount(%91) route(%c-1_i32) epoch(%20 : i64) {arts.create_id = 261000 : i64, arts.plan.kernel_family = "uniform", outlined_func = "__arts_edt_1"}
        %93 = arith.addi %39, %44 : index
        %94 = arith.muli %54, %c20 : index
        %95 = arith.cmpi ult, %39, %c20 : index
        %96 = arith.cmpi ule, %93, %c20 : index
        %97 = arith.andi %95, %96 : i1
        %98 = arith.cmpi ule, %55, %c1 : index
        %99 = arith.cmpi uge, %39, %94 : index
        %100 = arith.subi %39, %94 : index
        %101 = arith.select %99, %100, %c0 : index
        %102 = arith.minui %101, %c20 : index
        %103 = arith.cmpi uge, %93, %94 : index
        %104 = arith.subi %93, %94 : index
        %105 = arith.select %103, %104, %c0 : index
        %106 = arith.minui %105, %c20 : index
        %107 = arith.cmpi uge, %106, %102 : index
        %108 = arith.subi %106, %102 : index
        %109 = arith.select %107, %108, %c0 : index
        %110 = arith.muli %55, %c20 : index
        %111 = arith.addi %94, %110 : index
        %112 = arith.cmpi ule, %39, %94 : index
        %113 = arith.cmpi uge, %93, %111 : index
        %114 = arith.andi %112, %113 : i1
        %115 = arith.ori %98, %114 : i1
        %116 = arith.ori %97, %115 : i1
        %117 = arith.select %98, %102, %c0 : index
        %118 = arith.select %98, %109, %c20 : index
        %119 = arith.select %97, %39, %117 : index
        %120 = arith.select %97, %44, %118 : index
        %121 = arith.cmpi eq, %119, %c0 : index
        %122 = arith.cmpi eq, %120, %c20 : index
        %123 = arith.andi %121, %122 : i1
        %124 = arith.xori %123, %true : i1
        %125 = arith.andi %116, %123 : i1
        %126 = arith.andi %124, %125 : i1
        %127 = arith.muli %119, %c4 : index
        %128 = arith.muli %120, %c1600 : index
        %129 = arith.select %126, %127, %c0 : index
        %130 = arith.select %126, %128, %c0 : index
        %131 = arith.muli %62, %c20 : index
        %132 = arith.cmpi ule, %63, %c1 : index
        %133 = arith.cmpi uge, %39, %131 : index
        %134 = arith.subi %39, %131 : index
        %135 = arith.select %133, %134, %c0 : index
        %136 = arith.minui %135, %c20 : index
        %137 = arith.cmpi uge, %93, %131 : index
        %138 = arith.subi %93, %131 : index
        %139 = arith.select %137, %138, %c0 : index
        %140 = arith.minui %139, %c20 : index
        %141 = arith.cmpi uge, %140, %136 : index
        %142 = arith.subi %140, %136 : index
        %143 = arith.select %141, %142, %c0 : index
        %144 = arith.muli %63, %c20 : index
        %145 = arith.addi %131, %144 : index
        %146 = arith.cmpi ule, %39, %131 : index
        %147 = arith.cmpi uge, %93, %145 : index
        %148 = arith.andi %146, %147 : i1
        %149 = arith.ori %132, %148 : i1
        %150 = arith.ori %97, %149 : i1
        %151 = arith.select %132, %136, %c0 : index
        %152 = arith.select %132, %143, %c20 : index
        %153 = arith.select %97, %39, %151 : index
        %154 = arith.select %97, %44, %152 : index
        %155 = arith.cmpi eq, %153, %c0 : index
        %156 = arith.cmpi eq, %154, %c20 : index
        %157 = arith.andi %155, %156 : i1
        %158 = arith.xori %157, %true : i1
        %159 = arith.andi %150, %157 : i1
        %160 = arith.andi %158, %159 : i1
        %161 = arith.muli %153, %c4 : index
        %162 = arith.muli %154, %c4800 : index
        %163 = arith.select %160, %161, %c0 : index
        %164 = arith.select %160, %162, %c0 : index
        %165 = arith.addi %35, %29 : index
        %166 = arith.muli %73, %c20 : index
        %167 = arith.cmpi ult, %35, %c20 : index
        %168 = arith.cmpi ule, %165, %c20 : index
        %169 = arith.andi %167, %168 : i1
        %170 = arith.cmpi ule, %74, %c1 : index
        %171 = arith.cmpi uge, %35, %166 : index
        %172 = arith.subi %35, %166 : index
        %173 = arith.select %171, %172, %c0 : index
        %174 = arith.minui %173, %c20 : index
        %175 = arith.cmpi uge, %165, %166 : index
        %176 = arith.subi %165, %166 : index
        %177 = arith.select %175, %176, %c0 : index
        %178 = arith.minui %177, %c20 : index
        %179 = arith.cmpi uge, %178, %174 : index
        %180 = arith.subi %178, %174 : index
        %181 = arith.select %179, %180, %c0 : index
        %182 = arith.muli %74, %c20 : index
        %183 = arith.addi %166, %182 : index
        %184 = arith.cmpi ule, %35, %166 : index
        %185 = arith.cmpi uge, %165, %183 : index
        %186 = arith.andi %184, %185 : i1
        %187 = arith.ori %170, %186 : i1
        %188 = arith.ori %169, %187 : i1
        %189 = arith.select %170, %174, %c0 : index
        %190 = arith.select %170, %181, %c20 : index
        %191 = arith.select %169, %35, %189 : index
        %192 = arith.select %169, %29, %190 : index
        %193 = arith.cmpi eq, %191, %c0 : index
        %194 = arith.cmpi eq, %192, %c20 : index
        %195 = arith.andi %193, %194 : i1
        %196 = arith.xori %195, %true : i1
        %197 = arith.andi %188, %195 : i1
        %198 = arith.andi %196, %197 : i1
        %199 = arith.muli %191, %c4 : index
        %200 = arith.muli %192, %c4800 : index
        %201 = arith.select %198, %199, %c0 : index
        %202 = arith.select %198, %200, %c0 : index
        arts_rt.rec_dep %92(%guid_19, %guid_21, %guid_23, %guid_25 : memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>) bounds_valids(%true, %true, %true, %true) byte_offsets(%129, %129, %163, %201) byte_sizes(%130, %130, %164, %202) {acquire_modes = array<i32: 1, 1, 1, 2>}
      }
    } {arts.id = 133 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 4 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "recovered"}
    arts_rt.wait_on_epoch %20 : i64
    call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %21 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%21, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_11, %alloca[] : memref<f64>
    %22 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %23 = polygeist.pointer2memref %22 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c20 step %c1 {
      scf.for %arg1 = %c0 to %c3 step %c1 {
        %25 = polygeist.load %23[%arg1, %arg0, %arg0, %arg0] sizes(%c3, %c20, %c20, %c20) : memref<?x?x?x?xf32> -> f32
        %26 = arith.extf %25 : f32 to f64
        %27 = math.absf %26 : f64
        %28 = memref.load %alloca[] : memref<f64>
        %29 = arith.addf %28, %27 : f64
        memref.store %29, %alloca[] : memref<f64>
      } {arts.id = 258 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:163:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 212 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 20 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:162:3">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%cst_11) : (f64) -> ()
    call @carts_phase_timer_stop(%21) : (memref<?xi8>) -> ()
    %24 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%24, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%24) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_14) : memref<?xi64>
    arts.db_free(%ptr_15) : memref<?x!llvm.ptr>
    arts.db_free(%guid_12) : memref<?xi64>
    arts.db_free(%ptr_13) : memref<?x!llvm.ptr>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?x!llvm.ptr>
    arts.db_free(%guid_16) : memref<?xi64>
    arts.db_free(%ptr_17) : memref<?x!llvm.ptr>
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
