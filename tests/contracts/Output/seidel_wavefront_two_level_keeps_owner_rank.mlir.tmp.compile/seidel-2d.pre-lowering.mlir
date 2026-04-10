module attributes {arts.runtime_config_data = "[ARTS]\0A# Two-node local config used by step-boundary contract tests.\0Aworker_threads=8\0A\0A# Keep network threads disabled for local launcher tests.\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0Alauncher=local\0Amaster_node=localhost\0Anode_count=2\0Anodes=localhost,localhost\0Adefault_ports=34739,34740\0A\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 16 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
    %c0 = arith.constant 0 : index
    %c599 = arith.constant 599 : index
    %c600 = arith.constant 600 : index
    %c2 = arith.constant 2 : index
    %c1 = arith.constant 1 : index
    %c960 = arith.constant 960 : index
    %c9599 = arith.constant 9599 : index
    %c4800 = arith.constant 4800 : index
    %c4799 = arith.constant 4799 : index
    %c-1 = arith.constant -1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c9600 = arith.constant 9600 : index
    %c1_i32 = arith.constant 1 : i32
    %cst = arith.constant 9.000000e+00 : f64
    %c-4799 = arith.constant -4799 : index
    %0:7 = arts_rt.edt_param_unpack %arg1 : memref<?xi64> : (index, index, index, index, index, index, index)
    %1 = arith.subi %0#0, %0#1 : index
    %2 = arith.cmpi slt, %1, %c0 : index
    %3 = arith.select %2, %c0, %1 : index
    %4 = arith.addi %3, %c599 : index
    %5 = arith.divui %4, %c600 : index
    %6 = arith.maxui %0#6, %c1 : index
    %7 = arith.subi %6, %c1 : index
    %8 = arith.cmpi eq, %0#6, %c1 : index
    scf.for %arg4 = %5 to %0#5 step %c1 {
      %9 = arith.muli %arg4, %c600 : index
      %10 = arith.addi %0#1, %9 : index
      %11 = arith.divui %10, %c600 : index
      %12 = arith.muli %11, %c2 : index
      %13 = arith.subi %0#2, %12 : index
      %14 = arith.maxui %10, %c1 : index
      %15 = arith.addi %10, %c600 : index
      %16 = arith.minui %15, %0#3 : index
      %17 = arith.muli %13, %c960 : index
      %18 = arith.maxui %17, %c1 : index
      %19 = arith.addi %17, %c960 : index
      %20 = arith.minui %19, %c9599 : index
      %21 = arith.divui %14, %c4800 : index
      %22 = arith.addi %16, %c4799 : index
      %23 = arith.divui %22, %c4800 : index
      %24 = arith.cmpi ugt, %16, %14 : index
      %25 = arith.select %24, %23, %21 : index
      scf.for %arg5 = %21 to %25 step %c1 {
        %26 = arith.muli %arg5, %c4800 : index
        %27 = arith.cmpi uge, %26, %14 : index
        %28 = arith.subi %14, %26 : index
        %29 = arith.select %27, %c0, %28 : index
        %30 = arith.minui %29, %c4800 : index
        %31 = arith.cmpi uge, %26, %16 : index
        %32 = arith.subi %16, %26 : index
        %33 = arith.select %31, %c0, %32 : index
        %34 = arith.minui %33, %c4800 : index
        %35 = arith.minui %34, %c1 : index
        %36 = arith.addi %arg5, %c-1 : index
        %37 = arith.subi %arg5, %0#4 : index
        %38 = arith.subi %36, %0#4 : index
        %39 = arith.minui %38, %7 : index
        %40 = arith.select %8, %c0, %39 : index
        %guid, %ptr = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%40 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
        %41 = arith.minui %37, %7 : index
        %42 = arith.select %8, %c0, %41 : index
        %guid_0, %ptr_1 = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%42 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
        %43 = llvm.load %ptr : !llvm.ptr -> !llvm.ptr
        %44 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
        %45 = polygeist.pointer2memref %43 : !llvm.ptr to memref<?x?xf64>
        %46 = polygeist.pointer2memref %44 : !llvm.ptr to memref<?x?xf64>
        scf.for %arg6 = %30 to %35 step %c1 {
          %61 = arith.addi %arg6, %c4799 : index
          %62 = arith.addi %arg6, %c1 : index
          scf.for %arg7 = %18 to %20 step %c1 {
            %63 = arith.index_cast %arg7 : index to i32
            %64 = arith.addi %63, %c-1_i32 : i32
            %65 = arith.index_cast %64 : i32 to index
            %66 = polygeist.load %45[%61, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %67 = polygeist.load %45[%61, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %68 = arith.addf %66, %67 : f64
            %69 = arith.addi %63, %c1_i32 : i32
            %70 = arith.index_cast %69 : i32 to index
            %71 = polygeist.load %45[%61, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %72 = arith.addf %68, %71 : f64
            %73 = polygeist.load %46[%arg6, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %74 = arith.addf %72, %73 : f64
            %75 = polygeist.load %46[%arg6, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %76 = arith.addf %74, %75 : f64
            %77 = polygeist.load %46[%arg6, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %78 = arith.addf %76, %77 : f64
            %79 = polygeist.load %46[%62, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %80 = arith.addf %78, %79 : f64
            %81 = polygeist.load %46[%62, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %82 = arith.addf %80, %81 : f64
            %83 = polygeist.load %46[%62, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %84 = arith.addf %82, %83 : f64
            %85 = arith.divf %84, %cst : f64
            polygeist.store %85, %46[%arg6, %arg7] sizes(%c4800, %c9600) : f64, memref<?x?xf64>
          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
        }
        %47 = arith.maxui %30, %c1 : index
        %48 = arith.minui %34, %c4799 : index
        %49 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
        %50 = polygeist.pointer2memref %49 : !llvm.ptr to memref<?x?xf64>
        scf.for %arg6 = %47 to %48 step %c1 {
          %61 = arith.addi %arg6, %c-1 : index
          %62 = arith.addi %arg6, %c1 : index
          scf.for %arg7 = %18 to %20 step %c1 {
            %63 = arith.index_cast %arg7 : index to i32
            %64 = arith.addi %63, %c-1_i32 : i32
            %65 = arith.index_cast %64 : i32 to index
            %66 = polygeist.load %50[%61, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %67 = polygeist.load %50[%61, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %68 = arith.addf %66, %67 : f64
            %69 = arith.addi %63, %c1_i32 : i32
            %70 = arith.index_cast %69 : i32 to index
            %71 = polygeist.load %50[%61, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %72 = arith.addf %68, %71 : f64
            %73 = polygeist.load %50[%arg6, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %74 = arith.addf %72, %73 : f64
            %75 = polygeist.load %50[%arg6, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %76 = arith.addf %74, %75 : f64
            %77 = polygeist.load %50[%arg6, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %78 = arith.addf %76, %77 : f64
            %79 = polygeist.load %50[%62, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %80 = arith.addf %78, %79 : f64
            %81 = polygeist.load %50[%62, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %82 = arith.addf %80, %81 : f64
            %83 = polygeist.load %50[%62, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %84 = arith.addf %82, %83 : f64
            %85 = arith.divf %84, %cst : f64
            polygeist.store %85, %50[%arg6, %arg7] sizes(%c4800, %c9600) : f64, memref<?x?xf64>
          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
        }
        %51 = arith.maxui %30, %c4799 : index
        %52 = arith.minui %34, %c4800 : index
        %53 = arith.addi %arg5, %c1 : index
        %54 = arith.subi %53, %0#4 : index
        %55 = arith.minui %54, %7 : index
        %56 = arith.select %8, %c0, %55 : index
        %guid_2, %ptr_3 = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%56 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
        %57 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
        %58 = llvm.load %ptr_3 : !llvm.ptr -> !llvm.ptr
        %59 = polygeist.pointer2memref %57 : !llvm.ptr to memref<?x?xf64>
        %60 = polygeist.pointer2memref %58 : !llvm.ptr to memref<?x?xf64>
        scf.for %arg6 = %51 to %52 step %c1 {
          %61 = arith.addi %arg6, %c-1 : index
          %62 = arith.addi %arg6, %c-4799 : index
          scf.for %arg7 = %18 to %20 step %c1 {
            %63 = arith.index_cast %arg7 : index to i32
            %64 = arith.addi %63, %c-1_i32 : i32
            %65 = arith.index_cast %64 : i32 to index
            %66 = polygeist.load %59[%61, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %67 = polygeist.load %59[%61, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %68 = arith.addf %66, %67 : f64
            %69 = arith.addi %63, %c1_i32 : i32
            %70 = arith.index_cast %69 : i32 to index
            %71 = polygeist.load %59[%61, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %72 = arith.addf %68, %71 : f64
            %73 = polygeist.load %59[%arg6, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %74 = arith.addf %72, %73 : f64
            %75 = polygeist.load %59[%arg6, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %76 = arith.addf %74, %75 : f64
            %77 = polygeist.load %59[%arg6, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %78 = arith.addf %76, %77 : f64
            %79 = polygeist.load %60[%62, %65] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %80 = arith.addf %78, %79 : f64
            %81 = polygeist.load %60[%62, %arg7] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %82 = arith.addf %80, %81 : f64
            %83 = polygeist.load %60[%62, %70] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
            %84 = arith.addf %82, %83 : f64
            %85 = arith.divf %84, %cst : f64
            polygeist.store %85, %59[%arg6, %arg7] sizes(%c4800, %c9600) : f64, memref<?x?xf64>
          } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
        }
      }
    } {arts.id = 115 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 0 : i64, nestingLevel = 3 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "">, arts.metadata_provenance = "exact"}
    return
  }
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("seidel-2d\00") {addr_space = 0 : i32}
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c40 = arith.constant 40 : index
    %c16 = arith.constant 16 : index
    %c76800 = arith.constant 76800 : index
    %c2_i64 = arith.constant 2 : i64
    %c-4799 = arith.constant -4799 : index
    %c4799 = arith.constant 4799 : index
    %c4800 = arith.constant 4800 : index
    %c7 = arith.constant 7 : index
    %c599 = arith.constant 599 : index
    %cst = arith.constant 9.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c9599 = arith.constant 9599 : index
    %c960 = arith.constant 960 : index
    %c-1 = arith.constant -1 : index
    %c8 = arith.constant 8 : index
    %c9 = arith.constant 9 : index
    %c600 = arith.constant 600 : index
    %c2 = arith.constant 2 : index
    %c320 = arith.constant 320 : index
    %c9600 = arith.constant 9600 : index
    %c0 = arith.constant 0 : index
    %cst_0 = arith.constant 9.600000e+03 : f64
    %c-1_i32 = arith.constant -1 : i32
    %cst_1 = arith.constant 2.000000e+00 : f64
    %c1 = arith.constant 1 : index
    %false = arith.constant false
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr
    %cst_2 = arith.constant 0.000000e+00 : f64
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr
    %c2_i32 = arith.constant 2 : i32
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr
    %c0_i32 = arith.constant 0 : i32
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %true = arith.constant true
    %alloca = memref.alloca() {arts.id = 99 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:62:3", totalAccesses = 5 : i64, readCount = 2 : i64, writeCount = 3 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, memoryFootprint = 8 : i64, firstUseId = 99 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = true, reuseDistance = 1 : i64, hasGoodSpatialLocality = true, estimatedAccessBytes = 40 : i64>, arts.metadata_provenance = "exact"} : memref<f64>
    %4 = "polygeist.undef"() : () -> f64
    memref.store %4, %alloca[] : memref<f64>
    %alloca_3 = memref.alloca() {arts.id = 35 : i64, arts.memref = #arts.memref_metadata<rank = 0 : i64, allocationId = "seidel-2d.c:20:1", totalAccesses = 6 : i64, readCount = 2 : i64, writeCount = 4 : i64, allAccessesAffine = true, hasNonAffineAccesses = false, firstUseId = 35 : i64, hasUniformAccess = true, dominantAccessPattern = 1 : i64, accessedInParallelLoop = false, hasLoopCarriedDeps = false, reuseDistance = 1 : i64, hasGoodSpatialLocality = true>, arts.metadata_provenance = "exact"} : memref<i1>
    memref.store %true, %alloca_3[] : memref<i1>
    call @carts_benchmarks_start() : () -> ()
    %5 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8>
    call @carts_e2e_timer_start(%5) : (memref<?xi8>) -> ()
    %6 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%6, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%c-1_i32 : i32) sizes[%c2] elementType(f64) elementSizes[%c4800, %c9600] {arts.create_id = 39000 : i64, arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "seidel-2d.c:30:19", totalAccesses = 12 : i64, readCount = 10 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 737280000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = false, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 96 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr : memref<?x!llvm.ptr>) block_shape[%c4800] contract(<ownerDims = [0], postDbRefined = true>)
    scf.for %arg2 = %c0 to %c9600 step %c1 {
      %9 = arith.index_cast %arg2 : index to i32
      %10 = arith.sitofp %9 : i32 to f64
      %11 = arith.divui %arg2, %c4800 : index
      %12 = arith.remui %arg2, %c4800 : index
      %13 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%11] strides[%c1] : !llvm.ptr
      %14 = llvm.load %13 : !llvm.ptr -> !llvm.ptr
      %15 = polygeist.pointer2memref %14 : !llvm.ptr to memref<?x?xf64>
      scf.for %arg3 = %c0 to %c9600 step %c1 {
        %16 = arith.index_cast %arg3 : index to i32
        %17 = arith.addi %16, %c2_i32 : i32
        %18 = arith.sitofp %17 : i32 to f64
        %19 = arith.mulf %10, %18 : f64
        %20 = arith.addf %19, %cst_1 : f64
        %21 = arith.divf %20, %cst_0 : f64
        polygeist.store %21, %15[%12, %arg3] sizes(%c4800, %c9600) : f64, memref<?x?xf64>
      } {arts.id = 42 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:5">, arts.metadata_provenance = "recovered"}
    } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %9 = memref.load %alloca_3[] : memref<i1>
      scf.if %9 {
        %10 = arts_rt.create_epoch : i64
        scf.for %arg3 = %c0 to %c40 step %c1 {
          %11 = arith.cmpi uge, %arg3, %c9 : index
          %12 = arith.subi %arg3, %c9 : index
          %13 = arith.select %11, %12, %c0 : index
          %14 = arith.addi %13, %c1 : index
          %15 = arith.divui %14, %c2 : index
          %16 = arith.divui %arg3, %c2 : index
          %17 = arith.addi %16, %c1 : index
          %18 = arith.minui %17, %c16 : index
          %19 = arith.muli %15, %c600 : index
          %20 = arith.muli %18, %c600 : index
          %21 = arith.subi %20, %19 : index
          %22 = arith.addi %21, %c599 : index
          %23 = arith.divui %22, %c600 : index
          %24 = arith.minui %23, %c2 : index
          %25 = arith.muli %24, %c8 : index
          %26 = arith.maxui %25, %c1 : index
          %27 = arith.divui %23, %c2 : index
          %28 = arith.remui %23, %c2 : index
          %29 = arith.addi %27, %c1 : index
          scf.for %arg4 = %c0 to %26 step %c1 {
            %30 = arith.divui %arg4, %c8 : index
            %31 = arith.remui %arg4, %c8 : index
            %32 = arith.cmpi ult, %30, %28 : index
            %33 = arith.select %32, %29, %27 : index
            %34 = arith.minui %30, %28 : index
            %35 = arith.muli %30, %27 : index
            %36 = arith.addi %35, %34 : index
            %37 = arith.cmpi uge, %36, %23 : index
            %38 = arith.subi %23, %36 : index
            %39 = arith.select %37, %c0, %38 : index
            %40 = arith.minui %33, %39 : index
            %41 = arith.addi %40, %c7 : index
            %42 = arith.divui %41, %c8 : index
            %43 = arith.muli %31, %42 : index
            %44 = arith.cmpi uge, %43, %40 : index
            %45 = arith.subi %40, %43 : index
            %46 = arith.select %44, %c0, %45 : index
            %47 = arith.minui %42, %46 : index
            %48 = arith.addi %36, %43 : index
            %49 = arith.cmpi ugt, %47, %c0 : index
            scf.if %49 {
              %50 = arith.muli %48, %c600 : index
              %51 = arith.addi %19, %50 : index
              %52 = arith.muli %42, %c600 : index
              %53 = arith.maxui %52, %c1 : index
              %54 = arith.subi %20, %51 : index
              %55 = arith.cmpi slt, %54, %c0 : index
              %56 = arith.select %55, %c0, %54 : index
              %57 = arith.addi %56, %c599 : index
              %58 = arith.divui %57, %c600 : index
              %59 = arith.minui %58, %47 : index
              %60 = arith.divui %51, %c4800 : index
              %61 = arith.cmpi ugt, %60, %c1 : index
              %62 = arith.select %61, %c0, %60 : index
              %63 = arith.maxui %53, %c1 : index
              %64 = arith.addi %51, %63 : index
              %65 = arith.subi %64, %c1 : index
              %66 = arith.divui %65, %c4800 : index
              %67 = arith.minui %66, %c1 : index
              %68 = arith.select %61, %66, %67 : index
              %69 = arith.subi %68, %60 : index
              %70 = arith.addi %69, %c1 : index
              %71 = arith.select %61, %c0, %70 : index
              %guid_4, %ptr_5 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%51], sizes[%53]), offsets[%62], sizes[%71] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<two_level>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [600, 960], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
              arts.lowering_contract(%ptr_5 : memref<?x!llvm.ptr>) pattern(<depPattern = <wavefront_2d>, distributionKind = <two_level>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c600] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] contract(<ownerDims = [0], centerOffset = 1 : i64, spatialDims = [0, 1], supportedBlockHalo = true, postDbRefined = true, criticalPathDistance = 0 : i64, contractKind = 1 : i64>)
              %72 = arith.index_cast %30 : index to i32
              %73 = arts_rt.edt_param_pack(%19, %51, %arg3, %c9599, %62, %59, %62, %71, %53) : index, index, index, index, index, index, index, index, index : memref<?xi64>
              %74 = arts_rt.edt_param_pack(%19, %51, %arg3, %c9599, %62, %59, %71) : index, index, index, index, index, index, index : memref<?xi64>
              %75 = arts_rt.state_pack(%19, %51, %arg3, %c9599, %62, %59, %c0, %c599, %c600, %c2, %c1, %c960, %c9599, %c4800, %c4799, %c-1, %c-1_i32, %c9600, %c1_i32, %cst, %c-4799) : index, index, index, index, index, index, index, index, index, index, index, index, index, index, index, index, i32, index, i32, f64, index -> memref<21xi64>
              %76 = memref.load %guid_4[%c0] : memref<?xi64>
              %77 = arts_rt.dep_bind(%76, %c2_i64)
              %78 = arith.index_cast %71 : index to i32
              %79 = arts_rt.edt_create(%74 : memref<?xi64>) depCount(%78) route(%72) epoch(%10 : i64) {arts.plan.kernel_family = "wavefront", outlined_func = "__arts_edt_1"}
              %80 = arith.muli %51, %c76800 : index
              %81 = arith.muli %53, %c8 : index
              arts_rt.rec_dep %79(%guid_4 : memref<?xi64>) bounds_valids(%true) byte_offsets(%80) byte_sizes(%81) {acquire_modes = array<i32: 2>}
            }
          } {arts.id = 55 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:49:7">, arts.metadata_provenance = "recovered"}
        } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [600, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
        arts_rt.wait_on_epoch %10 : i64
        memref.store %true, %alloca_3[] : memref<i1>
      }
    } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_stop(%5) : (memref<?xi8>) -> ()
    %7 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%7, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_2, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      %9 = arith.muli %arg2, %c4800 : index
      %10 = arith.subi %c9600, %9 : index
      %11 = arith.cmpi uge, %9, %c9600 : index
      %12 = arith.select %11, %c0, %10 : index
      %13 = arith.minui %12, %c4800 : index
      %14 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%arg2] strides[%c1] : !llvm.ptr
      %15 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
      %16 = polygeist.pointer2memref %15 : !llvm.ptr to memref<?x?xf64>
      scf.for %arg3 = %c0 to %13 step %c1 {
        %17 = arith.addi %9, %arg3 : index
        %18 = polygeist.load %16[%arg3, %17] sizes(%c4800, %c9600) : memref<?x?xf64> -> f64
        %19 = memref.load %alloca[] : memref<f64>
        %20 = arith.addf %19, %18 : f64
        memref.store %20, %alloca[] : memref<f64>
      }
    }
    call @carts_bench_checksum_d(%cst_2) : (f64) -> ()
    call @carts_phase_timer_stop(%7) : (memref<?xi8>) -> ()
    %8 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%8, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%8) : (memref<?xi8>) -> ()
    call @carts_e2e_timer_stop() : () -> ()
    memref.store %false, %alloca_3[] : memref<i1>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?x!llvm.ptr>
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
