module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c58982400 = arith.constant 58982400 : index
    %c184320 = arith.constant 184320 : index
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
    %0:4 = arts_rt.edt_param_unpack %arg1 : memref<?xi64> : (index, index, index, index)
    %guid, %ptr = arts_rt.dep_db_acquire(%arg3) offset[%c0 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_5, %ptr_6 = arts_rt.dep_db_acquire(%arg3) offset[%c1 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %guid_7, %ptr_8 = arts_rt.dep_db_acquire(%arg3) offset[%c2 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
    %alloca = memref.alloca() {arts.id = 94 : i64, arts.memref = #arts.memref_metadata<rank = 1 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 8 : i64, readCount = 3 : i64, writeCount = 5 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 20 : i64, firstUseId = 94 : i64, hasUniformAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3], estimatedAccessBytes = 32 : i64>, arts.metadata_provenance = "exact"} : memref<5xf32>
    %alloca_9 = memref.alloca() {arts.id = 103 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 103 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 4 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_10 = memref.alloca() {arts.id = 107 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 107 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 3 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    %alloca_11 = memref.alloca() {arts.id = 111 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "rhs4sg_base.c:150:5", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 4 : i64, firstUseId = 111 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 20 : i64>, arts.metadata_provenance = "exact"} : memref<f32>
    memref.store %cst, %alloca[%c0] : memref<5xf32>
    memref.store %cst_0, %alloca[%c1] : memref<5xf32>
    memref.store %cst_1, %alloca[%c2] : memref<5xf32>
    memref.store %cst_2, %alloca[%c3] : memref<5xf32>
    memref.store %cst_3, %alloca[%c4] : memref<5xf32>
    %1 = "polygeist.undef"() : () -> f32
    %2 = arith.subi %c4, %0#0 : index
    %3 = arith.cmpi slt, %2, %c0 : index
    %4 = arith.select %3, %c0, %2 : index
    %5 = arith.cmpi slt, %0#1, %c0 : index
    %6 = arith.select %5, %c0, %0#1 : index
    %7 = arith.minui %6, %0#2 : index
    %8 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%c0] strides[%c184320, %c576, %c1] : !llvm.ptr
    %9 = arts_rt.db_gep(%ptr_6 : memref<?x!llvm.ptr>) indices[%c0] strides[%c184320, %c576, %c1] : !llvm.ptr
    %10 = arts_rt.db_gep(%ptr_8 : memref<?x!llvm.ptr>) indices[%c0] strides[%c58982400, %c184320, %c576, %c1] : !llvm.ptr
    %guid_12, %ptr_13 = arts_rt.dep_gep(%arg3) offset[%c3 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
    %11 = llvm.load %ptr_13 : !llvm.ptr -> !llvm.ptr
    %12 = llvm.load %8 : !llvm.ptr -> !llvm.ptr
    %13 = polygeist.pointer2memref %12 : !llvm.ptr to memref<?x?x?xf32>
    %14 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %15 = polygeist.pointer2memref %14 : !llvm.ptr to memref<?x?x?xf32>
    %16 = llvm.load %10 : !llvm.ptr -> !llvm.ptr
    %17 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?x?xf32>
    %18 = polygeist.pointer2memref %11 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg4 = %4 to %7 step %c1 {
      %19 = arith.addi %0#0, %arg4 : index
      memref.store %1, %alloca_9[] : memref<f32>
      memref.store %1, %alloca_10[] : memref<f32>
      memref.store %1, %alloca_11[] : memref<f32>
      %20 = arith.index_cast %19 : index to i32
      %21 = arith.addi %20, %c1_i32 : i32
      %22 = arith.index_cast %21 : i32 to index
      %23 = arith.addi %20, %c-1_i32 : i32
      %24 = arith.index_cast %23 : i32 to index
      scf.for %arg5 = %c4 to %c316 step %c1 {
        %25 = arith.index_cast %arg5 : index to i32
        %26 = arith.addi %25, %c1_i32 : i32
        %27 = arith.index_cast %26 : i32 to index
        %28 = arith.addi %25, %c-1_i32 : i32
        %29 = arith.index_cast %28 : i32 to index
        scf.for %arg6 = %c4 to %c316 step %c1 {
          %30 = arith.index_cast %arg6 : index to i32
          %31 = polygeist.load %13[%arg6, %arg5, %19] sizes(%c320, %c320, %c576) : memref<?x?x?xf32> -> f32
          %32 = polygeist.load %15[%arg6, %arg5, %19] sizes(%c320, %c320, %c576) : memref<?x?x?xf32> -> f32
          memref.store %cst_1, %alloca_11[] : memref<f32>
          scf.for %arg7 = %c-2 to %c3 step %c1 {
            %57 = arith.index_cast %arg7 : index to i32
            %58 = arith.addi %57, %c2_i32 : i32
            %59 = arith.index_cast %58 : i32 to index
            %60 = memref.load %alloca[%59] : memref<5xf32>
            %61 = arith.addi %30, %57 : i32
            %62 = arith.index_cast %61 : i32 to index
            %63 = polygeist.load %17[%c0, %62, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %64 = arith.addi %25, %57 : i32
            %65 = arith.index_cast %64 : i32 to index
            %66 = polygeist.load %17[%c0, %arg6, %65, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %67 = arith.addf %63, %66 : f32
            %68 = arith.addi %20, %57 : i32
            %69 = arith.index_cast %68 : i32 to index
            %70 = polygeist.load %17[%c0, %arg6, %arg5, %69] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %71 = arith.addf %67, %70 : f32
            %72 = arith.mulf %60, %71 : f32
            %73 = memref.load %alloca_11[] : memref<f32>
            %74 = arith.addf %73, %72 : f32
            memref.store %74, %alloca_11[] : memref<f32>
          } {arts.id = 153 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
          %33 = arith.addi %30, %c1_i32 : i32
          %34 = arith.index_cast %33 : i32 to index
          %35 = polygeist.load %17[%c0, %34, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %36 = arith.addi %30, %c-1_i32 : i32
          %37 = arith.index_cast %36 : i32 to index
          %38 = polygeist.load %17[%c0, %37, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %39 = arith.subf %35, %38 : f32
          %40 = arith.mulf %31, %cst_1 : f32
          %41 = arith.addf %32, %31 : f32
          %42 = arith.mulf %41, %39 : f32
          %43 = arith.mulf %42, %cst_4 : f32
          %44 = arith.addf %40, %43 : f32
          polygeist.store %44, %18[%c0, %arg6, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
          memref.store %cst_1, %alloca_10[] : memref<f32>
          scf.for %arg7 = %c-2 to %c3 step %c1 {
            %57 = arith.index_cast %arg7 : index to i32
            %58 = arith.addi %57, %c2_i32 : i32
            %59 = arith.index_cast %58 : i32 to index
            %60 = memref.load %alloca[%59] : memref<5xf32>
            %61 = arith.addi %30, %57 : i32
            %62 = arith.index_cast %61 : i32 to index
            %63 = polygeist.load %17[%c1, %62, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %64 = arith.addi %25, %57 : i32
            %65 = arith.index_cast %64 : i32 to index
            %66 = polygeist.load %17[%c1, %arg6, %65, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %67 = arith.addf %63, %66 : f32
            %68 = arith.addi %20, %57 : i32
            %69 = arith.index_cast %68 : i32 to index
            %70 = polygeist.load %17[%c1, %arg6, %arg5, %69] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %71 = arith.addf %67, %70 : f32
            %72 = arith.mulf %60, %71 : f32
            %73 = memref.load %alloca_10[] : memref<f32>
            %74 = arith.addf %73, %72 : f32
            memref.store %74, %alloca_10[] : memref<f32>
          } {arts.id = 193 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
          %45 = polygeist.load %17[%c1, %arg6, %27, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %46 = polygeist.load %17[%c1, %arg6, %29, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %47 = arith.subf %45, %46 : f32
          %48 = arith.mulf %41, %47 : f32
          %49 = arith.mulf %48, %cst_4 : f32
          %50 = arith.addf %40, %49 : f32
          polygeist.store %50, %18[%c1, %arg6, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
          memref.store %cst_1, %alloca_9[] : memref<f32>
          scf.for %arg7 = %c-2 to %c3 step %c1 {
            %57 = arith.index_cast %arg7 : index to i32
            %58 = arith.addi %57, %c2_i32 : i32
            %59 = arith.index_cast %58 : i32 to index
            %60 = memref.load %alloca[%59] : memref<5xf32>
            %61 = arith.addi %30, %57 : i32
            %62 = arith.index_cast %61 : i32 to index
            %63 = polygeist.load %17[%c2, %62, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %64 = arith.addi %25, %57 : i32
            %65 = arith.index_cast %64 : i32 to index
            %66 = polygeist.load %17[%c2, %arg6, %65, %19] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %67 = arith.addf %63, %66 : f32
            %68 = arith.addi %20, %57 : i32
            %69 = arith.index_cast %68 : i32 to index
            %70 = polygeist.load %17[%c2, %arg6, %arg5, %69] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
            %71 = arith.addf %67, %70 : f32
            %72 = arith.mulf %60, %71 : f32
            %73 = memref.load %alloca_9[] : memref<f32>
            %74 = arith.addf %73, %72 : f32
            memref.store %74, %alloca_9[] : memref<f32>
          } {arts.id = 229 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 5 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
          %51 = polygeist.load %17[%c2, %arg6, %arg5, %22] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %52 = polygeist.load %17[%c2, %arg6, %arg5, %24] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
          %53 = arith.subf %51, %52 : f32
          %54 = arith.mulf %41, %53 : f32
          %55 = arith.mulf %54, %cst_4 : f32
          %56 = arith.addf %40, %55 : f32
          polygeist.store %56, %18[%c2, %arg6, %arg5, %19] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
        } {arts.id = 245 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
      } {arts.id = 247 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 312 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 8 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 260 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "rhs4sg_base.c:150:5">, arts.metadata_provenance = "exact"}
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("sw4lite_rhs4sg_base\00") {addr_space = 0 : i32}
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c3_i32 = arith.constant 3 : i32
    %true = arith.constant true
    %c2_i64 = arith.constant 2 : i64
    %c0_i64 = arith.constant 0 : i64
    %c64 = arith.constant 64 : index
    %c9 = arith.constant 9 : index
    %c316 = arith.constant 316 : index
    %cst = arith.constant 5.000000e-01 : f32
    %c-2 = arith.constant -2 : index
    %c2_i32 = arith.constant 2 : i32
    %c4 = arith.constant 4 : index
    %c2 = arith.constant 2 : index
    %c10 = arith.constant 10 : index
    %cst_0 = arith.constant -0.0833333358 : f32
    %cst_1 = arith.constant 0.666666686 : f32
    %cst_2 = arith.constant -0.666666686 : f32
    %cst_3 = arith.constant 0.0833333358 : f32
    %c572 = arith.constant 572 : index
    %c-1_i32 = arith.constant -1 : i32
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
    %c576 = arith.constant 576 : index
    %c320 = arith.constant 320 : index
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
    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c320, %c320, %c576] {arts.create_id = 35000 : i64, arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:116:17", totalAccesses = 16 : i64, readCount = 15 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 707788800 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 35 : i64, lastUseId = 36 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 2 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 64 : i64>, arts.metadata_origin_id = 35 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_12, %ptr_13 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c3, %c320, %c320, %c576] {arts.create_id = 37000 : i64, arts.id = 37 : i64, arts.memref = #arts.memref_metadata<rank = 4 : i64, allocationId = "rhs4sg_base.c:117:19", totalAccesses = 5 : i64, readCount = 1 : i64, writeCount = 4 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 707788800 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 37 : i64, lastUseId = 38 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 6 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 3, 4], estimatedAccessBytes = 20 : i64>, arts.metadata_origin_id = 37 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_14, %ptr_15 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c320, %c320, %c576] {arts.create_id = 39000 : i64, arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:119:17", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 235929600 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
    %guid_16, %ptr_17 = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c320, %c320, %c576] {arts.create_id = 41000 : i64, arts.id = 41 : i64, arts.memref = #arts.memref_metadata<rank = 3 : i64, allocationId = "rhs4sg_base.c:120:21", totalAccesses = 2 : i64, readCount = 1 : i64, writeCount = 1 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 235929600 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 41 : i64, lastUseId = 42 : i64, hasUniformAccess = true, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = false, reuseDistance = 11 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [3, 3, 4], estimatedAccessBytes = 8 : i64>, arts.metadata_origin_id = 41 : i64, arts.metadata_provenance = "transferred"} : (memref<?xi64>, memref<?x!llvm.ptr>)
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
      %24 = arith.index_cast %arg0 : index to i32
      memref.store %c0_i32, %alloca_18[] : memref<i32>
      %25 = arith.muli %24, %c17_i32 : i32
      %26 = arith.sitofp %24 : i32 to f32
      %27 = arith.mulf %26, %cst_5 : f32
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c320 step %c1 {
          scf.for %arg3 = %c0 to %c576 step %c1 {
            %28 = memref.load %alloca_18[] : memref<i32>
            %29 = arith.addi %28, %25 : i32
            %30 = arith.remsi %29, %c23_i32 : i32
            %31 = arith.sitofp %30 : i32 to f32
            %32 = arith.mulf %31, %cst_4 : f32
            %33 = arith.subf %32, %27 : f32
            polygeist.store %33, %11[%arg0, %arg1, %arg2, %arg3] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
            polygeist.store %cst_6, %13[%arg0, %arg1, %arg2, %arg3] sizes(%c3, %c320, %c320, %c576) : f32, memref<?x?x?x?xf32>
            %34 = memref.load %alloca_18[] : memref<i32>
            %35 = arith.addi %34, %c1_i32 : i32
            memref.store %35, %alloca_18[] : memref<i32>
          } {arts.id = 64 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
        } {arts.id = 66 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 68 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    } {arts.id = 70 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    memref.store %c0_i32, %alloca_18[] : memref<i32>
    %14 = arts_rt.db_gep(%ptr_15 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %15 = arts_rt.db_gep(%ptr_17 : memref<?x!llvm.ptr>) indices[%c0] strides[%c1] : !llvm.ptr
    %16 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
    %17 = polygeist.pointer2memref %16 : !llvm.ptr to memref<?x?x?xf32>
    %18 = llvm.load %15 : !llvm.ptr -> !llvm.ptr
    %19 = polygeist.pointer2memref %18 : !llvm.ptr to memref<?x?x?xf32>
    scf.for %arg0 = %c0 to %c320 step %c1 {
      scf.for %arg1 = %c0 to %c320 step %c1 {
        scf.for %arg2 = %c0 to %c576 step %c1 {
          %24 = memref.load %alloca_18[] : memref<i32>
          %25 = arith.remsi %24, %c11_i32 : i32
          %26 = arith.sitofp %25 : i32 to f32
          %27 = arith.mulf %26, %cst_8 : f32
          %28 = arith.addf %27, %cst_7 : f32
          polygeist.store %28, %17[%arg0, %arg1, %arg2] sizes(%c320, %c320, %c576) : f32, memref<?x?x?xf32>
          %29 = memref.load %alloca_18[] : memref<i32>
          %30 = arith.remsi %29, %c7_i32 : i32
          %31 = arith.sitofp %30 : i32 to f32
          %32 = arith.mulf %31, %cst_10 : f32
          %33 = arith.addf %32, %cst_9 : f32
          polygeist.store %33, %19[%arg0, %arg1, %arg2] sizes(%c320, %c320, %c576) : f32, memref<?x?x?xf32>
          %34 = memref.load %alloca_18[] : memref<i32>
          %35 = arith.addi %34, %c1_i32 : i32
          memref.store %35, %alloca_18[] : memref<i32>
        } {arts.id = 88 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 576 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
      } {arts.id = 90 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "exact"}
    } {arts.id = 92 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:144:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    scf.for %arg0 = %c0 to %c10 step %c1 {
      %guid_19, %ptr_20 = arts.db_acquire[<in>] (%guid_14 : memref<?xi64>, %ptr_15 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [320, 320, 9], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
      arts.lowering_contract(%ptr_20 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c320] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
      %guid_21, %ptr_22 = arts.db_acquire[<in>] (%guid_16 : memref<?xi64>, %ptr_17 : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [320, 320, 9], stencil_owner_dims = [2]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
      arts.lowering_contract(%ptr_22 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c320] contract(<ownerDims = [2], postDbRefined = true, criticalPathDistance = 0 : i64>)
      %guid_23, %ptr_24 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
      arts.lowering_contract(%ptr_24 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)
      %24 = arts_rt.create_epoch : i64
      scf.for %arg1 = %c0 to %c64 step %c1 {
        %25 = arith.muli %arg1, %c9 : index
        %26 = arith.cmpi uge, %25, %c572 : index
        %27 = arith.subi %c572, %25 : index
        %28 = arith.select %26, %c0, %27 : index
        %29 = arith.minui %28, %c9 : index
        %30 = arith.cmpi ugt, %29, %c0 : index
        scf.if %30 {
          %31 = arith.divui %25, %c576 : index
          %32 = arith.addi %25, %c8 : index
          %33 = arith.divui %32, %c576 : index
          %34 = arith.cmpi ugt, %31, %c0 : index
          %35 = arith.select %34, %33, %c0 : index
          %36 = arith.subi %35, %31 : index
          %37 = arith.addi %36, %c1 : index
          %38 = arith.select %34, %c0, %31 : index
          %39 = arith.select %34, %c0, %37 : index
          %guid_25, %ptr_26 = arts.db_acquire[<inout>] (%guid_12 : memref<?xi64>, %ptr_13 : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%25], sizes[%c9]), offsets[%38], sizes[%39] {arts.pattern_revision = 2 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32, stencil_block_shape = [3, 320, 320, 9], stencil_owner_dims = [3]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
          arts.lowering_contract(%ptr_26 : memref<?x!llvm.ptr>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>, distributionVersion = 1 : i64, revision = 2 : i64>) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)
          %40 = arts_rt.edt_param_pack(%25, %27, %29, %38, %39) : index, index, index, index, index : memref<?xi64>
          %41 = arts_rt.edt_param_pack(%25, %27, %29, %39) : index, index, index, index : memref<?xi64>
          %42 = arts_rt.state_pack(%25, %27, %29, %cst_0, %c0, %cst_1, %c1, %cst_6, %c2, %cst_2, %c3, %cst_3, %c4, %c1_i32, %c-1_i32, %c320, %c576, %c2_i32, %c-2, %cst, %c316) : index, index, index, f32, index, f32, index, f32, index, f32, index, f32, index, i32, i32, index, index, i32, index, f32, index -> memref<21xi64>
          %43 = memref.load %guid_19[%c0] : memref<?xi64>
          %44 = arts_rt.dep_bind(%43, %c0_i64)
          %45 = memref.load %guid_21[%c0] : memref<?xi64>
          %46 = arts_rt.dep_bind(%45, %c0_i64)
          %47 = memref.load %guid_23[%c0] : memref<?xi64>
          %48 = arts_rt.dep_bind(%47, %c0_i64)
          %49 = memref.load %guid_25[%c0] : memref<?xi64>
          %50 = arts_rt.dep_bind(%49, %c2_i64)
          %51 = arith.index_cast %39 : index to i32
          %52 = arith.addi %51, %c3_i32 : i32
          %53 = arts_rt.edt_create(%41 : memref<?xi64>) depCount(%52) route(%c-1_i32) epoch(%24 : i64) {arts.create_id = 261000 : i64, arts.plan.kernel_family = "uniform", outlined_func = "__arts_edt_1"}
          arts_rt.rec_dep %53(%guid_19, %guid_21, %guid_23, %guid_25 : memref<?xi64>, memref<?xi64>, memref<?xi64>, memref<?xi64>) bounds_valids(%true, %true, %true, %true) {acquire_modes = array<i32: 1, 1, 1, 2>}
        }
      }
      arts_rt.wait_on_epoch %24 : i64
      arts.db_release(%ptr_24) : memref<?x!llvm.ptr>
      arts.db_release(%ptr_22) : memref<?x!llvm.ptr>
      arts.db_release(%ptr_20) : memref<?x!llvm.ptr>
      func.call @carts_kernel_timer_accum(%5) : (memref<?xi8>) -> ()
    } {arts.id = 93 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 10 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 9 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:149:3">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_print(%5) : (memref<?xi8>) -> ()
    %20 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%20, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_11, %alloca[] : memref<f64>
    %21 = llvm.load %9 : !llvm.ptr -> !llvm.ptr
    %22 = polygeist.pointer2memref %21 : !llvm.ptr to memref<?x?x?x?xf32>
    scf.for %arg0 = %c0 to %c320 step %c1 {
      scf.for %arg1 = %c0 to %c3 step %c1 {
        %24 = polygeist.load %22[%arg1, %arg0, %arg0, %arg0] sizes(%c3, %c320, %c320, %c576) : memref<?x?x?x?xf32> -> f32
        %25 = arith.extf %24 : f32 to f64
        %26 = math.absf %25 : f64
        %27 = memref.load %alloca[] : memref<f64>
        %28 = arith.addf %27, %26 : f64
        memref.store %28, %alloca[] : memref<f64>
      } {arts.id = 258 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 3 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:163:5">, arts.metadata_provenance = "exact"}
    } {arts.id = 212 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "rhs4sg_base.c:162:3">, arts.metadata_provenance = "recovered"}
    call @carts_bench_checksum_d(%cst_11) : (f64) -> ()
    call @carts_phase_timer_stop(%20) : (memref<?xi8>) -> ()
    %23 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%23, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%23) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    arts.db_free(%guid_12) : memref<?xi64>
    arts.db_free(%ptr_13) : memref<?x!llvm.ptr>
    arts.db_free(%guid_16) : memref<?xi64>
    arts.db_free(%ptr_17) : memref<?x!llvm.ptr>
    arts.db_free(%guid_14) : memref<?xi64>
    arts.db_free(%ptr_15) : memref<?x!llvm.ptr>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?x!llvm.ptr>
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
