module attributes {arts.runtime_config_data = "[ARTS]\0A# Contract config for high-thread single-node lowering checks.\0Aworker_threads=64\0A\0A# Network configuration\0Asender_threads=0\0Areceiver_threads=0\0Aport_count=1\0A\0A# Launcher configuration\0Alauncher=local\0Amaster_node=localhost\0Anode_count=1\0Anodes=localhost\0Adefault_ports=34739\0A\0A# Scheduling\0Aworker_init_deque_size=2048\0Aroute_table_size=16\0A\0A# Debug\0Acore_dump=1\0Acounter_folder=./counters\0A", arts.runtime_static_workers = false, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> {llvm.noalias, llvm.nocapture, llvm.nofree, llvm.readonly}) {
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
    %0:9 = arts_rt.edt_param_unpack %arg1 : memref<?xi64> : (index, index, index, index, index, index, index, index, index)
    %1 = arith.subi %0#0, %0#1 : index
    %2 = arith.cmpi slt, %1, %c0 : index
    %3 = arith.select %2, %c0, %1 : index
    %4 = arith.addi %3, %c149 : index
    %5 = arith.divui %4, %c150 : index
    %6 = arith.maxui %0#2, %c1 : index
    %7 = arith.cmpi ugt, %6, %c2 : index
    scf.for %arg4 = %5 to %0#7 step %c1 {
      %8 = arith.muli %arg4, %c150 : index
      %9 = arith.addi %0#1, %8 : index
      %10 = arith.divui %9, %c150 : index
      %11 = arith.muli %10, %c2 : index
      %12 = arith.subi %0#3, %11 : index
      %13 = arith.maxui %9, %c1 : index
      %14 = arith.addi %9, %c150 : index
      %15 = arith.minui %14, %0#4 : index
      %16 = arith.muli %12, %c960 : index
      %17 = arith.maxui %16, %c1 : index
      %18 = arith.addi %16, %c960 : index
      %19 = arith.minui %18, %c9599 : index
      scf.if %7 {
        %20 = arith.subi %6, %c1 : index
        %21 = arith.divui %13, %6 : index
        %22 = arith.addi %15, %20 : index
        %23 = arith.divui %22, %6 : index
        %24 = arith.cmpi ugt, %15, %13 : index
        %25 = arith.select %24, %23, %21 : index
        %26 = arith.maxui %0#8, %c1 : index
        %27 = arith.subi %26, %c1 : index
        %28 = arith.cmpi eq, %0#8, %c1 : index
        scf.for %arg5 = %21 to %25 step %c1 {
          %29 = arith.muli %arg5, %6 : index
          %30 = arith.cmpi uge, %29, %13 : index
          %31 = arith.subi %13, %29 : index
          %32 = arith.select %30, %c0, %31 : index
          %33 = arith.minui %32, %6 : index
          %34 = arith.cmpi uge, %29, %15 : index
          %35 = arith.subi %15, %29 : index
          %36 = arith.select %34, %c0, %35 : index
          %37 = arith.minui %36, %6 : index
          %38 = arith.minui %37, %c1 : index
          %39 = arith.addi %arg5, %c-1 : index
          %40 = arith.cmpi ult, %39, %0#5 : index
          %41 = arith.subi %39, %0#5 : index
          %42 = arith.select %40, %c0, %41 : index
          %43 = arith.minui %42, %27 : index
          %44 = arith.select %28, %c0, %43 : index
          %guid, %ptr = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%44 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
          %45 = arith.cmpi ult, %arg5, %0#5 : index
          %46 = arith.subi %arg5, %0#5 : index
          %47 = arith.select %45, %c0, %46 : index
          %48 = arith.minui %47, %27 : index
          %49 = arith.select %28, %c0, %48 : index
          %guid_0, %ptr_1 = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%49 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
          %50 = llvm.load %ptr : !llvm.ptr -> !llvm.ptr
          %51 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
          %52 = polygeist.pointer2memref %50 : !llvm.ptr to memref<?x?xf64>
          %53 = polygeist.pointer2memref %51 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %33 to %38 step %c1 {
            %70 = arith.addi %arg6, %c-1 : index
            %71 = arith.addi %70, %6 : index
            %72 = arith.addi %arg6, %c1 : index
            scf.for %arg7 = %17 to %19 step %c1 {
              %73 = arith.index_cast %arg7 : index to i32
              %74 = arith.addi %73, %c-1_i32 : i32
              %75 = arith.index_cast %74 : i32 to index
              %76 = polygeist.load %52[%71, %75] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %77 = polygeist.load %52[%71, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %78 = arith.addf %76, %77 : f64
              %79 = arith.addi %73, %c1_i32 : i32
              %80 = arith.index_cast %79 : i32 to index
              %81 = polygeist.load %52[%71, %80] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %82 = arith.addf %78, %81 : f64
              %83 = polygeist.load %53[%arg6, %75] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %84 = arith.addf %82, %83 : f64
              %85 = polygeist.load %53[%arg6, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %86 = arith.addf %84, %85 : f64
              %87 = polygeist.load %53[%arg6, %80] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %88 = arith.addf %86, %87 : f64
              %89 = polygeist.load %53[%72, %75] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %90 = arith.addf %88, %89 : f64
              %91 = polygeist.load %53[%72, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %92 = arith.addf %90, %91 : f64
              %93 = polygeist.load %53[%72, %80] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %94 = arith.addf %92, %93 : f64
              %95 = arith.divf %94, %cst : f64
              polygeist.store %95, %53[%arg6, %arg7] sizes(%0#6, %c9600) : f64, memref<?x?xf64>
            } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
          }
          %54 = arith.maxui %33, %c1 : index
          %55 = arith.minui %37, %20 : index
          %56 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
          %57 = polygeist.pointer2memref %56 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %54 to %55 step %c1 {
            %70 = arith.addi %arg6, %c-1 : index
            %71 = arith.addi %arg6, %c1 : index
            scf.for %arg7 = %17 to %19 step %c1 {
              %72 = arith.index_cast %arg7 : index to i32
              %73 = arith.addi %72, %c-1_i32 : i32
              %74 = arith.index_cast %73 : i32 to index
              %75 = polygeist.load %57[%70, %74] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %76 = polygeist.load %57[%70, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %77 = arith.addf %75, %76 : f64
              %78 = arith.addi %72, %c1_i32 : i32
              %79 = arith.index_cast %78 : i32 to index
              %80 = polygeist.load %57[%70, %79] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %81 = arith.addf %77, %80 : f64
              %82 = polygeist.load %57[%arg6, %74] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %83 = arith.addf %81, %82 : f64
              %84 = polygeist.load %57[%arg6, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %85 = arith.addf %83, %84 : f64
              %86 = polygeist.load %57[%arg6, %79] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %87 = arith.addf %85, %86 : f64
              %88 = polygeist.load %57[%71, %74] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %89 = arith.addf %87, %88 : f64
              %90 = polygeist.load %57[%71, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %91 = arith.addf %89, %90 : f64
              %92 = polygeist.load %57[%71, %79] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %93 = arith.addf %91, %92 : f64
              %94 = arith.divf %93, %cst : f64
              polygeist.store %94, %57[%arg6, %arg7] sizes(%0#6, %c9600) : f64, memref<?x?xf64>
            } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
          }
          %58 = arith.maxui %33, %20 : index
          %59 = arith.minui %37, %6 : index
          %60 = arith.addi %arg5, %c1 : index
          %61 = arith.cmpi ult, %60, %0#5 : index
          %62 = arith.subi %60, %0#5 : index
          %63 = arith.select %61, %c0, %62 : index
          %64 = arith.minui %63, %27 : index
          %65 = arith.select %28, %c0, %64 : index
          %guid_2, %ptr_3 = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%65 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
          %66 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
          %67 = llvm.load %ptr_3 : !llvm.ptr -> !llvm.ptr
          %68 = polygeist.pointer2memref %66 : !llvm.ptr to memref<?x?xf64>
          %69 = polygeist.pointer2memref %67 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %58 to %59 step %c1 {
            %70 = arith.addi %arg6, %c-1 : index
            %71 = arith.addi %arg6, %c1 : index
            %72 = arith.subi %71, %6 : index
            scf.for %arg7 = %17 to %19 step %c1 {
              %73 = arith.index_cast %arg7 : index to i32
              %74 = arith.addi %73, %c-1_i32 : i32
              %75 = arith.index_cast %74 : i32 to index
              %76 = polygeist.load %68[%70, %75] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %77 = polygeist.load %68[%70, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %78 = arith.addf %76, %77 : f64
              %79 = arith.addi %73, %c1_i32 : i32
              %80 = arith.index_cast %79 : i32 to index
              %81 = polygeist.load %68[%70, %80] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %82 = arith.addf %78, %81 : f64
              %83 = polygeist.load %68[%arg6, %75] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %84 = arith.addf %82, %83 : f64
              %85 = polygeist.load %68[%arg6, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %86 = arith.addf %84, %85 : f64
              %87 = polygeist.load %68[%arg6, %80] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %88 = arith.addf %86, %87 : f64
              %89 = polygeist.load %69[%72, %75] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %90 = arith.addf %88, %89 : f64
              %91 = polygeist.load %69[%72, %arg7] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %92 = arith.addf %90, %91 : f64
              %93 = polygeist.load %69[%72, %80] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
              %94 = arith.addf %92, %93 : f64
              %95 = arith.divf %94, %cst : f64
              polygeist.store %95, %68[%arg6, %arg7] sizes(%0#6, %c9600) : f64, memref<?x?xf64>
            } {arts.id = 53 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "seidel-2d.c:48:5">, arts.metadata_provenance = "recovered"}
          }
        }
      } else {
        %20 = arith.maxui %0#8, %c1 : index
        %21 = arith.subi %20, %c1 : index
        %22 = arith.cmpi eq, %0#8, %c1 : index
        scf.for %arg5 = %13 to %15 step %c1 {
          %23 = arith.index_cast %arg5 : index to i32
          %24 = arith.addi %23, %c-1_i32 : i32
          %25 = arith.index_cast %24 : i32 to index
          %26 = arith.addi %23, %c1_i32 : i32
          %27 = arith.index_cast %26 : i32 to index
          %28 = arith.divui %25, %6 : index
          %29 = arith.remui %25, %6 : index
          %30 = arith.divui %arg5, %6 : index
          %31 = arith.remui %arg5, %6 : index
          %32 = arith.divui %27, %6 : index
          %33 = arith.remui %27, %6 : index
          %34 = arith.cmpi ult, %28, %0#5 : index
          %35 = arith.subi %28, %0#5 : index
          %36 = arith.select %34, %c0, %35 : index
          %37 = arith.minui %36, %21 : index
          %38 = arith.select %22, %c0, %37 : index
          %guid, %ptr = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%38 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
          %39 = arith.cmpi ult, %30, %0#5 : index
          %40 = arith.subi %30, %0#5 : index
          %41 = arith.select %39, %c0, %40 : index
          %42 = arith.minui %41, %21 : index
          %43 = arith.select %22, %c0, %42 : index
          %guid_0, %ptr_1 = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%43 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
          %44 = arith.cmpi ult, %32, %0#5 : index
          %45 = arith.subi %32, %0#5 : index
          %46 = arith.select %44, %c0, %45 : index
          %47 = arith.minui %46, %21 : index
          %48 = arith.select %22, %c0, %47 : index
          %guid_2, %ptr_3 = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%48 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
          %49 = llvm.load %ptr : !llvm.ptr -> !llvm.ptr
          %50 = llvm.load %ptr_1 : !llvm.ptr -> !llvm.ptr
          %51 = llvm.load %ptr_3 : !llvm.ptr -> !llvm.ptr
          %52 = polygeist.pointer2memref %49 : !llvm.ptr to memref<?x?xf64>
          %53 = polygeist.pointer2memref %50 : !llvm.ptr to memref<?x?xf64>
          %54 = polygeist.pointer2memref %51 : !llvm.ptr to memref<?x?xf64>
          scf.for %arg6 = %17 to %19 step %c1 {
            %55 = arith.index_cast %arg6 : index to i32
            %56 = arith.addi %55, %c-1_i32 : i32
            %57 = arith.index_cast %56 : i32 to index
            %58 = polygeist.load %52[%29, %57] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %59 = polygeist.load %52[%29, %arg6] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %60 = arith.addf %58, %59 : f64
            %61 = arith.addi %55, %c1_i32 : i32
            %62 = arith.index_cast %61 : i32 to index
            %63 = polygeist.load %52[%29, %62] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %64 = arith.addf %60, %63 : f64
            %65 = polygeist.load %53[%31, %57] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %66 = arith.addf %64, %65 : f64
            %67 = polygeist.load %53[%31, %arg6] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %68 = arith.addf %66, %67 : f64
            %69 = polygeist.load %53[%31, %62] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %70 = arith.addf %68, %69 : f64
            %71 = polygeist.load %54[%33, %57] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %72 = arith.addf %70, %71 : f64
            %73 = polygeist.load %54[%33, %arg6] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %74 = arith.addf %72, %73 : f64
            %75 = polygeist.load %54[%33, %62] sizes(%0#6, %c9600) : memref<?x?xf64> -> f64
            %76 = arith.addf %74, %75 : f64
            %77 = arith.divf %76, %cst : f64
            polygeist.store %77, %53[%31, %arg6] sizes(%0#6, %c9600) : f64, memref<?x?xf64>
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
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c136 = arith.constant 136 : index
    %c2_i64 = arith.constant 2 : i64
    %c9599_i64 = arith.constant 9599 : i64
    %c1200 = arith.constant 1200 : index
    %c149 = arith.constant 149 : index
    %cst = arith.constant 9.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c9599 = arith.constant 9599 : index
    %c960 = arith.constant 960 : index
    %c-1 = arith.constant -1 : index
    %c64 = arith.constant 64 : index
    %c9 = arith.constant 9 : index
    %c150 = arith.constant 150 : index
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
    %7 = arts.runtime_query <total_workers> -> i32
    %8 = arith.index_castui %7 : i32 to index
    %9 = arith.maxui %8, %c1 : index
    %10 = arith.minui %9, %c1200 : index
    %11 = arith.addi %10, %c9599 : index
    %12 = arith.divui %11, %10 : index
    %13 = arith.maxui %12, %c1 : index
    %14 = arith.maxui %13, %c1 : index
    %15 = arith.index_cast %14 : index to i64
    %16 = arith.addi %15, %c9599_i64 : i64
    %17 = arith.divui %16, %15 : i64
    %18 = arith.index_cast %17 : i64 to index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <stencil>] route(%c-1_i32 : i32) sizes[%18] elementType(f64) elementSizes[%14, %c9600] {arts.create_id = 39000 : i64, arts.id = 39 : i64, arts.memref = #arts.memref_metadata<rank = 2 : i64, allocationId = "seidel-2d.c:30:19", totalAccesses = 12 : i64, readCount = 10 : i64, writeCount = 2 : i64, allAccessesAffine = false, hasAffineAccesses = false, hasNonAffineAccesses = true, memoryFootprint = 737280000 : i64, isFlattenedArray = false, shouldCanonicalize = false, isCanonicalized = false, firstUseId = 39 : i64, lastUseId = 40 : i64, hasUniformAccess = false, hasStrideOneAccess = false, dominantAccessPattern = 0 : i64, accessedInParallelLoop = true, hasLoopCarriedDeps = true, reuseDistance = 0 : i64, hasGoodSpatialLocality = true, dimAccessPatterns = [4, 3], estimatedAccessBytes = 96 : i64>, arts.metadata_origin_id = 39 : i64, arts.metadata_provenance = "transferred", arts.read_only_after_init} : (memref<?xi64>, memref<?x!llvm.ptr>)
    arts.lowering_contract(%ptr : memref<?x!llvm.ptr>) block_shape[%14] contract(<ownerDims = [0], postDbRefined = true>)
    %19 = arith.maxui %14, %c1 : index
    scf.for %arg2 = %c0 to %c9600 step %c1 {
      %22 = arith.index_cast %arg2 : index to i32
      %23 = arith.sitofp %22 : i32 to f64
      %24 = arith.divui %arg2, %19 : index
      %25 = arith.remui %arg2, %19 : index
      %26 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%24] strides[%c1] : !llvm.ptr
      %27 = llvm.load %26 : !llvm.ptr -> !llvm.ptr
      %28 = polygeist.pointer2memref %27 : !llvm.ptr to memref<?x?xf64>
      scf.for %arg3 = %c0 to %c9600 step %c1 {
        %29 = arith.index_cast %arg3 : index to i32
        %30 = arith.addi %29, %c2_i32 : i32
        %31 = arith.sitofp %30 : i32 to f64
        %32 = arith.mulf %23, %31 : f64
        %33 = arith.addf %32, %cst_1 : f64
        %34 = arith.divf %33, %cst_0 : f64
        polygeist.store %34, %28[%25, %arg3] sizes(%14, %c9600) : f64, memref<?x?xf64>
      } {arts.id = 42 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 1 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:37:5">, arts.metadata_provenance = "recovered"}
    } {arts.id = 40 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9600 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:36:3">, arts.metadata_provenance = "recovered"}
    call @carts_phase_timer_stop(%6) : (memref<?xi8>) -> ()
    call @carts_kernel_timer_start(%5) : (memref<?xi8>) -> ()
    scf.for %arg2 = %c0 to %c320 step %c1 {
      %22 = memref.load %alloca_3[] : memref<i1>
      scf.if %22 {
        %23 = arts_rt.create_epoch : i64
        scf.for %arg3 = %c0 to %c136 step %c1 {
          %24 = arith.cmpi uge, %arg3, %c9 : index
          %25 = arith.subi %arg3, %c9 : index
          %26 = arith.select %24, %25, %c0 : index
          %27 = arith.addi %26, %c1 : index
          %28 = arith.divui %27, %c2 : index
          %29 = arith.divui %arg3, %c2 : index
          %30 = arith.addi %29, %c1 : index
          %31 = arith.minui %30, %c64 : index
          %32 = arith.muli %28, %c150 : index
          %33 = arith.muli %31, %c150 : index
          %34 = arith.subi %33, %32 : index
          %35 = arith.addi %34, %c149 : index
          %36 = arith.divui %35, %c150 : index
          %37 = arith.minui %36, %c64 : index
          %38 = arith.maxui %37, %c1 : index
          %39 = arith.divui %36, %c64 : index
          %40 = arith.remui %36, %c64 : index
          %41 = arith.addi %39, %c1 : index
          scf.for %arg4 = %c0 to %38 step %c1 {
            %42 = arith.cmpi ult, %arg4, %40 : index
            %43 = arith.select %42, %41, %39 : index
            %44 = arith.minui %arg4, %40 : index
            %45 = arith.muli %arg4, %39 : index
            %46 = arith.addi %45, %44 : index
            %47 = arith.cmpi uge, %46, %36 : index
            %48 = arith.subi %36, %46 : index
            %49 = arith.select %47, %c0, %48 : index
            %50 = arith.minui %43, %49 : index
            %51 = arith.cmpi ugt, %50, %c0 : index
            scf.if %51 {
              %52 = arith.muli %46, %c150 : index
              %53 = arith.addi %32, %52 : index
              %54 = arith.subi %33, %53 : index
              %55 = arith.cmpi slt, %54, %c0 : index
              %56 = arith.select %55, %c0, %54 : index
              %57 = arith.addi %56, %c149 : index
              %58 = arith.divui %57, %c150 : index
              %59 = arith.minui %58, %50 : index
              %60 = arith.maxui %59, %c1 : index
              %61 = arith.divui %53, %19 : index
              %62 = arith.addi %53, %60 : index
              %63 = arith.subi %62, %c1 : index
              %64 = arith.divui %63, %19 : index
              %65 = arith.subi %18, %c1 : index
              %66 = arith.cmpi ugt, %61, %65 : index
              %67 = arith.minui %64, %65 : index
              %68 = arith.select %66, %64, %67 : index
              %69 = arith.subi %68, %61 : index
              %70 = arith.addi %69, %c1 : index
              %71 = arith.select %66, %c0, %61 : index
              %72 = arith.select %66, %c0, %70 : index
              %guid_4, %ptr_5 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>), offsets[%71], sizes[%72] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [150, 960], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]} -> (memref<?xi64>, memref<?x!llvm.ptr>)
              arts.lowering_contract(%ptr_5 : memref<?x!llvm.ptr>) pattern(<depPattern = <wavefront_2d>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c150] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] contract(<ownerDims = [0], centerOffset = 1 : i64, spatialDims = [0, 1], supportedBlockHalo = true, postDbRefined = true, criticalPathDistance = 0 : i64, contractKind = 1 : i64>)
              %73 = arts_rt.edt_param_pack(%32, %53, %19, %arg3, %c9599, %71, %14, %59, %72) : index, index, index, index, index, index, index, index, index : memref<?xi64>
              %74 = arts_rt.state_pack(%32, %53, %19, %arg3, %c9599, %71, %14, %59, %c0, %c149, %c150, %c1, %c2, %c960, %c9599, %c-1, %c-1_i32, %c9600, %c1_i32, %cst) : index, index, index, index, index, index, index, index, index, index, index, index, index, index, index, index, i32, index, i32, f64 -> memref<20xi64>
              %75 = memref.load %guid_4[%c0] : memref<?xi64>
              %76 = arts_rt.dep_bind(%75, %c2_i64)
              %77 = arith.index_cast %72 : index to i32
              %78 = arts_rt.edt_create(%73 : memref<?xi64>) depCount(%77) route(%c-1_i32) epoch(%23 : i64) {arts.plan.kernel_family = "wavefront", outlined_func = "__arts_edt_1"}
              arts_rt.rec_dep %78(%guid_4 : memref<?xi64>) bounds_valids(%true) {acquire_modes = array<i32: 2>}
            }
          } {arts.id = 55 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 9598 : i64, nestingLevel = 2 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:49:7">, arts.metadata_provenance = "recovered"}
        } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [150, 960], stencil_max_offsets = [1, 1], stencil_min_offsets = [-1, -1], stencil_owner_dims = [0, 1], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0, 0]}
        arts_rt.wait_on_epoch %23 : i64
        memref.store %true, %alloca_3[] : memref<i1>
      }
    } {arts.id = 51 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 320 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "seidel-2d.c:46:3">, arts.metadata_provenance = "recovered"}
    call @carts_kernel_timer_stop(%5) : (memref<?xi8>) -> ()
    %20 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%20, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    memref.store %cst_2, %alloca[] : memref<f64>
    scf.for %arg2 = %c0 to %c64 step %c1 {
      %22 = arith.muli %arg2, %c150 : index
      %23 = arith.subi %c9600, %22 : index
      %24 = arith.cmpi uge, %22, %c9600 : index
      %25 = arith.select %24, %c0, %23 : index
      %26 = arith.minui %25, %c150 : index
      %27 = arts_rt.db_gep(%ptr : memref<?x!llvm.ptr>) indices[%arg2] strides[%c1] : !llvm.ptr
      %28 = llvm.load %27 : !llvm.ptr -> !llvm.ptr
      %29 = polygeist.pointer2memref %28 : !llvm.ptr to memref<?x?xf64>
      scf.for %arg3 = %c0 to %26 step %c1 {
        %30 = arith.addi %22, %arg3 : index
        %31 = polygeist.load %29[%arg3, %30] sizes(%14, %c9600) : memref<?x?xf64> -> f64
        %32 = memref.load %alloca[] : memref<f64>
        %33 = arith.addf %32, %31 : f64
        memref.store %33, %alloca[] : memref<f64>
      }
    }
    call @carts_bench_checksum_d(%cst_2) : (f64) -> ()
    call @carts_phase_timer_stop(%20) : (memref<?xi8>) -> ()
    %21 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8>
    call @carts_phase_timer_start(%21, %5) : (memref<?xi8>, memref<?xi8>) -> ()
    call @carts_phase_timer_stop(%21) : (memref<?xi8>) -> ()
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
