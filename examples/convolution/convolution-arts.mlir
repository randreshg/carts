-----------------------------------------
Canonicalizing dim ops
-----------------------------------------

-----------------------------------------
DatablockPass STARTED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str9("Verification: %d errors\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("convolution C_serial:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("convolution C_parallel:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("convolution Kernel:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("%4.1f \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("convolution A:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Mismatch at (%d,%d): Parallel=%.2f vs Serial=%.2f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Task %d running\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Task %d starting at row %d\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c16 = arith.constant 16 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c15 = arith.constant 15 : index
    %c0_i32 = arith.constant 0 : i32
    %c15_i32 = arith.constant 15 : i32
    %cst = arith.constant 9.9999999999999995E-7 : f64
    %cst_0 = arith.constant 0.000000e+00 : f64
    %c4_i32 = arith.constant 4 : i32
    %cst_1 = arith.constant 1.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c16_i32 = arith.constant 16 : i32
    %alloca = memref.alloca() : memref<15x15xf64>
    %alloca_2 = memref.alloca() : memref<15x15xf64>
    %0 = arts.datablock "inout" ptr[%alloca_2 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] -> memref<15x15xf64>
    %alloca_3 = memref.alloca() : memref<2x2xf64>
    %1 = arts.datablock "in" ptr[%alloca_3 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] -> memref<2x2xf64>
    %alloca_4 = memref.alloca() : memref<16x16xf64>
    %2 = arts.datablock "in" ptr[%alloca_4 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] -> memref<16x16xf64>
    scf.for %arg2 = %c0 to %c16 step %c1 {
      scf.for %arg3 = %c0 to %c16 step %c1 {
        memref.store %cst_1, %2[%arg2, %arg3] : memref<16x16xf64>
      }
    }
    scf.for %arg2 = %c0 to %c2 step %c1 {
      scf.for %arg3 = %c0 to %c2 step %c1 {
        memref.store %cst_1, %1[%arg2, %arg3] : memref<2x2xf64>
      }
    }
    arts.edt dependencies(%1, %0, %2) : (memref<2x2xf64>, memref<15x15xf64>, memref<16x16xf64>) attributes {sync} {
      %37 = llvm.mlir.addressof @str0 : !llvm.ptr
      %38 = llvm.getelementptr %37[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      scf.for %arg2 = %c0 to %c4 step %c1 {
        %39 = arith.index_cast %arg2 : index to i32
        %40 = arith.muli %39, %c4_i32 : i32
        %41 = arith.addi %39, %c1_i32 : i32
        %42 = llvm.call @printf(%38, %41, %40) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        %43 = arts.datablock "in" ptr[%1 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] -> memref<2x2xf64>
        %44 = arts.datablock "inout" ptr[%0 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] -> memref<15x15xf64>
        %45 = arts.datablock "in" ptr[%2 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] -> memref<16x16xf64>
        arts.edt dependencies(%43, %44, %45) : (memref<2x2xf64>, memref<15x15xf64>, memref<16x16xf64>) attributes {task} {
          %46 = llvm.mlir.addressof @str1 : !llvm.ptr
          %47 = llvm.getelementptr %46[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<17 x i8>
          %48 = llvm.call @printf(%47, %41) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
          %49 = arith.addi %40, %c4_i32 : i32
          %50 = scf.while (%arg3 = %40) : (i32) -> i32 {
            %51 = arith.cmpi slt, %arg3, %49 : i32
            %52 = arith.cmpi slt, %arg3, %c15_i32 : i32
            %53 = arith.andi %51, %52 : i1
            scf.condition(%53) %arg3 : i32
          } do {
          ^bb0(%arg3: i32):
            %51 = arith.index_cast %arg3 : i32 to index
            scf.for %arg4 = %c0 to %c15 step %c1 {
              %53 = arith.index_cast %arg4 : index to i32
              %54 = scf.for %arg5 = %c0 to %c2 step %c1 iter_args(%arg6 = %cst_0) -> (f64) {
                %55 = arith.index_cast %arg5 : index to i32
                %56 = arith.addi %arg3, %55 : i32
                %57 = arith.cmpi slt, %56, %c16_i32 : i32
                %58 = scf.for %arg7 = %c0 to %c2 step %c1 iter_args(%arg8 = %arg6) -> (f64) {
                  %59 = arith.index_cast %arg7 : index to i32
                  %60 = arith.addi %53, %59 : i32
                  %61 = arith.cmpi slt, %60, %c16_i32 : i32
                  %62 = arith.andi %57, %61 : i1
                  %63 = scf.if %62 -> (f64) {
                    %64 = arith.index_cast %56 : i32 to index
                    %65 = arith.index_cast %60 : i32 to index
                    %66 = memref.load %45[%64, %65] : memref<16x16xf64>
                    %67 = memref.load %43[%arg5, %arg7] : memref<2x2xf64>
                    %68 = arith.mulf %66, %67 : f64
                    %69 = arith.addf %arg8, %68 : f64
                    scf.yield %69 : f64
                  } else {
                    scf.yield %arg8 : f64
                  }
                  scf.yield %63 : f64
                }
                scf.yield %58 : f64
              }
              memref.store %54, %44[%51, %arg4] : memref<15x15xf64>
            }
            %52 = arith.addi %arg3, %c1_i32 : i32
            scf.yield %52 : i32
          }
          arts.yield
        }
      }
      arts.yield
    }
    scf.for %arg2 = %c0 to %c15 step %c1 {
      %37 = arith.index_cast %arg2 : index to i32
      scf.for %arg3 = %c0 to %c15 step %c1 {
        %38 = arith.index_cast %arg3 : index to i32
        %39 = scf.for %arg4 = %c0 to %c2 step %c1 iter_args(%arg5 = %cst_0) -> (f64) {
          %40 = arith.index_cast %arg4 : index to i32
          %41 = arith.addi %37, %40 : i32
          %42 = arith.cmpi slt, %41, %c16_i32 : i32
          %43 = scf.for %arg6 = %c0 to %c2 step %c1 iter_args(%arg7 = %arg5) -> (f64) {
            %44 = arith.index_cast %arg6 : index to i32
            %45 = arith.addi %38, %44 : i32
            %46 = arith.cmpi slt, %45, %c16_i32 : i32
            %47 = arith.andi %42, %46 : i1
            %48 = scf.if %47 -> (f64) {
              %49 = arith.index_cast %41 : i32 to index
              %50 = arith.index_cast %45 : i32 to index
              %51 = memref.load %2[%49, %50] : memref<16x16xf64>
              %52 = memref.load %1[%arg4, %arg6] : memref<2x2xf64>
              %53 = arith.mulf %51, %52 : f64
              %54 = arith.addf %arg7, %53 : f64
              scf.yield %54 : f64
            } else {
              scf.yield %arg7 : f64
            }
            scf.yield %48 : f64
          }
          scf.yield %43 : f64
        }
        memref.store %39, %alloca[%arg2, %arg3] : memref<15x15xf64>
      }
    }
    %3 = scf.for %arg2 = %c0 to %c15 step %c1 iter_args(%arg3 = %c0_i32) -> (i32) {
      %37 = arith.index_cast %arg2 : index to i32
      %38 = scf.for %arg4 = %c0 to %c15 step %c1 iter_args(%arg5 = %arg3) -> (i32) {
        %39 = arith.index_cast %arg4 : index to i32
        %40 = memref.load %0[%arg2, %arg4] : memref<15x15xf64>
        %41 = memref.load %alloca[%arg2, %arg4] : memref<15x15xf64>
        %42 = arith.subf %40, %41 : f64
        %43 = math.absf %42 : f64
        %44 = arith.cmpf ogt, %43, %cst : f64
        %45 = scf.if %44 -> (i32) {
          %46 = arith.addi %arg5, %c1_i32 : i32
          %47 = llvm.mlir.addressof @str2 : !llvm.ptr
          %48 = llvm.getelementptr %47[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
          %49 = llvm.call @printf(%48, %37, %39, %40, %41) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64, f64) -> i32
          scf.yield %46 : i32
        } else {
          scf.yield %arg5 : i32
        }
        scf.yield %45 : i32
      }
      scf.yield %38 : i32
    }
    %4 = arith.cmpi sgt, %3, %c0_i32 : i32
    %5 = llvm.mlir.addressof @str3 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
    %7 = llvm.call @printf(%6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %8 = llvm.mlir.addressof @str4 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
    %10 = llvm.mlir.addressof @str5 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg2 = %c0 to %c16 step %c1 {
      scf.for %arg3 = %c0 to %c16 step %c1 {
        %38 = memref.load %2[%arg2, %arg3] : memref<16x16xf64>
        %39 = llvm.call @printf(%9, %38) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %37 = llvm.call @printf(%11) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %12 = llvm.mlir.addressof @str5 : !llvm.ptr
    %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %15 = llvm.mlir.addressof @str6 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<21 x i8>
    %17 = llvm.call @printf(%16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %18 = llvm.mlir.addressof @str4 : !llvm.ptr
    %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
    scf.for %arg2 = %c0 to %c2 step %c1 {
      scf.for %arg3 = %c0 to %c2 step %c1 {
        %38 = memref.load %1[%arg2, %arg3] : memref<2x2xf64>
        %39 = llvm.call @printf(%19, %38) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %37 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %20 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %21 = llvm.mlir.addressof @str7 : !llvm.ptr
    %22 = llvm.getelementptr %21[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<25 x i8>
    %23 = llvm.call @printf(%22) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %24 = llvm.mlir.addressof @str4 : !llvm.ptr
    %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
    scf.for %arg2 = %c0 to %c15 step %c1 {
      scf.for %arg3 = %c0 to %c15 step %c1 {
        %38 = memref.load %0[%arg2, %arg3] : memref<15x15xf64>
        %39 = llvm.call @printf(%25, %38) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %37 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %26 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %27 = llvm.mlir.addressof @str8 : !llvm.ptr
    %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %29 = llvm.call @printf(%28) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %30 = llvm.mlir.addressof @str4 : !llvm.ptr
    %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
    scf.for %arg2 = %c0 to %c15 step %c1 {
      scf.for %arg3 = %c0 to %c15 step %c1 {
        %38 = memref.load %alloca[%arg2, %arg3] : memref<15x15xf64>
        %39 = llvm.call @printf(%31, %38) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %37 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %32 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %33 = llvm.mlir.addressof @str9 : !llvm.ptr
    %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<25 x i8>
    %35 = llvm.call @printf(%34, %3) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %36 = arith.extui %4 : i1 to i32
    return %36 : i32
  }
}
Datablock node #1: %0 = arts.datablock "inout" ptr[%alloca_2 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] -> memref<15x15xf64>
DB #1 - Computing access region (rank=2).
  Processing user: %40 = memref.load %0[%arg2, %arg4] : memref<15x15xf64>
  Processing user: %38 = memref.load %0[%arg2, %arg3] : memref<15x15xf64>
DB #1:
  Dim 0:
    mins:
     0
    maxs:
     15
  Dim 1:
    mins:
     0
    maxs:
     15
-----------------------------------------
Datablock node #2: %1 = arts.datablock "in" ptr[%alloca_3 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] -> memref<2x2xf64>
DB #2 - Computing access region (rank=2).
  Processing user: memref.store %cst_1, %1[%arg2, %arg3] : memref<2x2xf64>
  Processing user: %52 = memref.load %1[%arg4, %arg6] : memref<2x2xf64>
  Processing user: %38 = memref.load %1[%arg2, %arg3] : memref<2x2xf64>
DB #2:
  Dim 0:
    mins:
     0
    maxs:
     2
  Dim 1:
    mins:
     0
    maxs:
     2
-----------------------------------------
Datablock node #3: %2 = arts.datablock "in" ptr[%alloca_4 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] -> memref<16x16xf64>
DB #3 - Computing access region (rank=2).
  Processing user: memref.store %cst_1, %2[%arg2, %arg3] : memref<16x16xf64>
  Processing user: %51 = memref.load %2[%49, %50] : memref<16x16xf64>
  Processing user: %38 = memref.load %2[%arg2, %arg3] : memref<16x16xf64>
DB #3:
  Dim 0:
    mins:
     0
     %41 = arith.addi %37, %40 : i32>
    maxs:
     16
     %41 = arith.addi %37, %40 : i32>
  Dim 1:
    mins:
     0
     %45 = arith.addi %38, %44 : i32>
    maxs:
     16
     %45 = arith.addi %38, %44 : i32>
-----------------------------------------
DB #3 - Skipping region refinement for dim 0 (symbolic bounds for both min and max).
DB #3 - Skipping region refinement for dim 1 (symbolic bounds for both min and max).
Datablock node #4: %43 = arts.datablock "in" ptr[%1 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] -> memref<2x2xf64>
DB #4 - Computing access region (rank=2).
  Processing user: %67 = memref.load %43[%arg5, %arg7] : memref<2x2xf64>
DB #4:
  Dim 0:
    mins:
     0
    maxs:
     2
  Dim 1:
    mins:
     0
    maxs:
     2
-----------------------------------------
Datablock node #5: %44 = arts.datablock "inout" ptr[%0 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] -> memref<15x15xf64>
DB #5 - Computing access region (rank=2).
  Processing user: memref.store %54, %44[%51, %arg4] : memref<15x15xf64>
DB #5:
  Dim 0:
    mins:
     <block argument> of type 'i32' at index: 0>
    maxs:
     <block argument> of type 'i32' at index: 0>
  Dim 1:
    mins:
     0
    maxs:
     15
-----------------------------------------
Datablock node #6: %45 = arts.datablock "in" ptr[%2 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] -> memref<16x16xf64>
DB #6 - Computing access region (rank=2).
  Processing user: %66 = memref.load %45[%64, %65] : memref<16x16xf64>
DB #6:
  Dim 0:
    mins:
     %56 = arith.addi %arg3, %55 : i32>
    maxs:
     %56 = arith.addi %arg3, %55 : i32>
  Dim 1:
    mins:
     %60 = arith.addi %53, %59 : i32>
    maxs:
     %60 = arith.addi %53, %59 : i32>
-----------------------------------------
- Building datablock graph for function: main
  - Processing ForOp loop
    - Processing ForOp loop
     - Finished processing region. Environment changed: false
      Merging environments
      - Final merged environment: {}
      - Iteration 1 completed: edges=0
    - Finished processing ForOp loop
      Merging environments
      - Final merged environment: {}
   - Finished processing region. Environment changed: false
    Merging environments
    - Final merged environment: {}
    - Iteration 1 completed: edges=0
  - Finished processing ForOp loop
    Merging environments
    - Final merged environment: {}
  - Processing ForOp loop
    - Processing ForOp loop
     - Finished processing region. Environment changed: false
      Merging environments
      - Final merged environment: {}
      - Iteration 1 completed: edges=0
    - Finished processing ForOp loop
      Merging environments
      - Final merged environment: {}
   - Finished processing region. Environment changed: false
    Merging environments
    - Final merged environment: {}
    - Iteration 1 completed: edges=0
  - Finished processing ForOp loop
    Merging environments
    - Final merged environment: {}
  - Processing EDT #0
   Initial environment: {}
    Processing EDT inputs
    - Examining DB #2 as input
      Searching for definitions for DB #2
      - No previous definition for DB #2, add edge from entry node
    - Examining DB #1 as input
      Searching for definitions for DB #1
      - No previous definition for DB #1, add edge from entry node
    - Examining DB #3 as input
      Searching for definitions for DB #3
      - No previous definition for DB #3, add edge from entry node
    Processing EDT outputs
    - Examining DB #1 as output
      Searching for definitions for DB #1
      - No previous definition for DB #1, updating environment with new definition
    Processing EDT body region
      - Processing ForOp loop
        - Processing EDT #1
         Initial environment: {}
          Processing EDT inputs
          - Examining DB #4 as input
            Searching for definitions for DB #4
            - No previous definition for DB #4, add edge from entry node
          - Examining DB #5 as input
            Searching for definitions for DB #5
            - No previous definition for DB #5, add edge from entry node
          - Examining DB #6 as input
            Searching for definitions for DB #6
            - No previous definition for DB #6, add edge from entry node
          Processing EDT outputs
          - Examining DB #5 as output
            Searching for definitions for DB #5
            - No previous definition for DB #5, updating environment with new definition
          Processing EDT body region
           - Finished processing region. Environment changed: false
        Finished processing EDT. Environment changed: true
          Merging environments
          - Adding DB #5 to merged environment
          - Final merged environment: { #5 -> #5,}
       - Finished processing region. Environment changed: true
        Merging environments
        - Adding DB #5 to merged environment
        - Final merged environment: { #5 -> #5,}
        - Iteration 1 completed: edges=1
        - Loop environment updated, iterating fixed-point...
        - Processing EDT #2
         Initial environment: {  #5 -> #5,}
          Processing EDT inputs
          - Examining DB #4 as input
            Searching for definitions for DB #4
            - No previous definition for DB #4, add edge from entry node
          - Examining DB #5 as input
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Found 1 definitions for DB #5
        Adding edge from DB #5 to DB #5
        Edge added successfully
          - Examining DB #6 as input
            Searching for definitions for DB #6
            - No previous definition for DB #6, add edge from entry node
          Processing EDT outputs
          - Examining DB #5 as output
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Updating environment: DB #5 now defined from DB #5
          Processing EDT body region
           - Finished processing region. Environment changed: false
        Finished processing EDT. Environment changed: true
          Merging environments
          - DB already exists in merged environment: 5
            - Same EDT parent, updating definition
          - Final merged environment: { #5 -> #5,}
       - Finished processing region. Environment changed: true
        Merging environments
        - DB already exists in merged environment: 5
          - Same EDT parent, updating definition
        - Final merged environment: { #5 -> #5,}
        - Iteration 2 completed: edges=2
        - Loop environment updated, iterating fixed-point...
        - Processing EDT #3
         Initial environment: {  #5 -> #5,}
          Processing EDT inputs
          - Examining DB #4 as input
            Searching for definitions for DB #4
            - No previous definition for DB #4, add edge from entry node
          - Examining DB #5 as input
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Found 1 definitions for DB #5
        Adding edge from DB #5 to DB #5
        Edge already exists, skipping
          - Examining DB #6 as input
            Searching for definitions for DB #6
            - No previous definition for DB #6, add edge from entry node
          Processing EDT outputs
          - Examining DB #5 as output
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Updating environment: DB #5 now defined from DB #5
          Processing EDT body region
           - Finished processing region. Environment changed: false
        Finished processing EDT. Environment changed: true
          Merging environments
          - DB already exists in merged environment: 5
            - Same EDT parent, updating definition
          - Final merged environment: { #5 -> #5,}
       - Finished processing region. Environment changed: true
        Merging environments
        - DB already exists in merged environment: 5
          - Same EDT parent, updating definition
        - Final merged environment: { #5 -> #5,}
        - Iteration 3 completed: edges=2
        - Loop environment updated, iterating fixed-point...
        - Processing EDT #4
         Initial environment: {  #5 -> #5,}
          Processing EDT inputs
          - Examining DB #4 as input
            Searching for definitions for DB #4
            - No previous definition for DB #4, add edge from entry node
          - Examining DB #5 as input
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Found 1 definitions for DB #5
        Adding edge from DB #5 to DB #5
        Edge already exists, skipping
          - Examining DB #6 as input
            Searching for definitions for DB #6
            - No previous definition for DB #6, add edge from entry node
          Processing EDT outputs
          - Examining DB #5 as output
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Updating environment: DB #5 now defined from DB #5
          Processing EDT body region
           - Finished processing region. Environment changed: false
        Finished processing EDT. Environment changed: true
          Merging environments
          - DB already exists in merged environment: 5
            - Same EDT parent, updating definition
          - Final merged environment: { #5 -> #5,}
       - Finished processing region. Environment changed: true
        Merging environments
        - DB already exists in merged environment: 5
          - Same EDT parent, updating definition
        - Final merged environment: { #5 -> #5,}
        - Iteration 4 completed: edges=2
        - Loop environment updated, iterating fixed-point...
        - Processing EDT #5
         Initial environment: {  #5 -> #5,}
          Processing EDT inputs
          - Examining DB #4 as input
            Searching for definitions for DB #4
            - No previous definition for DB #4, add edge from entry node
          - Examining DB #5 as input
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Found 1 definitions for DB #5
        Adding edge from DB #5 to DB #5
        Edge already exists, skipping
          - Examining DB #6 as input
            Searching for definitions for DB #6
            - No previous definition for DB #6, add edge from entry node
          Processing EDT outputs
          - Examining DB #5 as output
            Searching for definitions for DB #5
            - Potential definition found: DB #5
        Updating environment: DB #5 now defined from DB #5
          Processing EDT body region
           - Finished processing region. Environment changed: false
        Finished processing EDT. Environment changed: true
          Merging environments
          - DB already exists in merged environment: 5
            - Same EDT parent, updating definition
          - Final merged environment: { #5 -> #5,}
       - Finished processing region. Environment changed: true
        Merging environments
        - DB already exists in merged environment: 5
          - Same EDT parent, updating definition
        - Final merged environment: { #5 -> #5,}
        - Iteration 5 completed: edges=2
      - Finished processing ForOp loop
        Merging environments
        - Adding DB #5 to merged environment
        - Final merged environment: { #5 -> #5,}
     - Finished processing region. Environment changed: true
  Finished processing EDT. Environment changed: true
    Merging environments
    - Adding DB #1 to merged environment
    - Final merged environment: { #1 -> #1,}
  - Processing ForOp loop
    - Processing ForOp loop
      - Processing ForOp loop
        - Processing ForOp loop
          - Processing IfOp with then and else regions
           - Finished processing region. Environment changed: false
           - Finished processing region. Environment changed: false
            Merging environments
            - DB already exists in merged environment: 1
              - Same EDT parent, updating definition
            - Final merged environment: { #1 -> #1,}
            - IfOp regions merged. Environment changed: false
            Merging environments
            - DB already exists in merged environment: 1
              - Same EDT parent, updating definition
            - Final merged environment: { #1 -> #1,}
         - Finished processing region. Environment changed: false
          Merging environments
          - DB already exists in merged environment: 1
            - Same EDT parent, updating definition
          - Final merged environment: { #1 -> #1,}
          - Iteration 1 completed: edges=2
        - Finished processing ForOp loop
          Merging environments
          - DB already exists in merged environment: 1
            - Same EDT parent, updating definition
          - Final merged environment: { #1 -> #1,}
       - Finished processing region. Environment changed: false
        Merging environments
        - DB already exists in merged environment: 1
          - Same EDT parent, updating definition
        - Final merged environment: { #1 -> #1,}
        - Iteration 1 completed: edges=2
      - Finished processing ForOp loop
        Merging environments
        - DB already exists in merged environment: 1
          - Same EDT parent, updating definition
        - Final merged environment: { #1 -> #1,}
     - Finished processing region. Environment changed: false
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
      - Iteration 1 completed: edges=2
    - Finished processing ForOp loop
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
   - Finished processing region. Environment changed: false
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
    - Iteration 1 completed: edges=2
  - Finished processing ForOp loop
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
  - Processing ForOp loop
    - Processing ForOp loop
      - Processing IfOp with then and else regions
       - Finished processing region. Environment changed: false
       - Finished processing region. Environment changed: false
        Merging environments
        - DB already exists in merged environment: 1
          - Same EDT parent, updating definition
        - Final merged environment: { #1 -> #1,}
        - IfOp regions merged. Environment changed: false
        Merging environments
        - DB already exists in merged environment: 1
          - Same EDT parent, updating definition
        - Final merged environment: { #1 -> #1,}
     - Finished processing region. Environment changed: false
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
      - Iteration 1 completed: edges=2
    - Finished processing ForOp loop
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
   - Finished processing region. Environment changed: false
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
    - Iteration 1 completed: edges=2
  - Finished processing ForOp loop
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
  - Processing ForOp loop
    - Processing ForOp loop
     - Finished processing region. Environment changed: false
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
      - Iteration 1 completed: edges=2
    - Finished processing ForOp loop
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
   - Finished processing region. Environment changed: false
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
    - Iteration 1 completed: edges=2
  - Finished processing ForOp loop
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
  - Processing ForOp loop
    - Processing ForOp loop
     - Finished processing region. Environment changed: false
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
      - Iteration 1 completed: edges=2
    - Finished processing ForOp loop
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
   - Finished processing region. Environment changed: false
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
    - Iteration 1 completed: edges=2
  - Finished processing ForOp loop
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
  - Processing ForOp loop
    - Processing ForOp loop
     - Finished processing region. Environment changed: false
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
      - Iteration 1 completed: edges=2
    - Finished processing ForOp loop
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
   - Finished processing region. Environment changed: false
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
    - Iteration 1 completed: edges=2
  - Finished processing ForOp loop
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
  - Processing ForOp loop
    - Processing ForOp loop
     - Finished processing region. Environment changed: false
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
      - Iteration 1 completed: edges=2
    - Finished processing ForOp loop
      Merging environments
      - DB already exists in merged environment: 1
        - Same EDT parent, updating definition
      - Final merged environment: { #1 -> #1,}
   - Finished processing region. Environment changed: false
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
    - Iteration 1 completed: edges=2
  - Finished processing ForOp loop
    Merging environments
    - DB already exists in merged environment: 1
      - Same EDT parent, updating definition
    - Final merged environment: { #1 -> #1,}
 - Finished processing region. Environment changed: true
Finished building datablock graph for function: main
Converting datablocks to parameters - Analyzing 7 datablocks
 - No datablocks to convert
[datablock] Shrinking datablocks in function: main
Nodes:
  #1 inout
    %0 = arts.datablock "inout" ptr[%alloca_2 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] -> memref<15x15xf64>
     useCount=4 hasPtrDb=false userEdtPos=1
    Dim #0: 0 -> 15
    Dim #1: 0 -> 15
  #2 in
    %1 = arts.datablock "in" ptr[%alloca_3 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] -> memref<2x2xf64>
     useCount=5 hasPtrDb=false userEdtPos=0
    Dim #0: 0 -> 2
    Dim #1: 0 -> 2
  #3 in
    %2 = arts.datablock "in" ptr[%alloca_4 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] -> memref<16x16xf64>
     useCount=5 hasPtrDb=false userEdtPos=2
    Dim #0: 0 -> 16
    Dim #1: 0 -> 16
  #4 in
    %43 = arts.datablock "in" ptr[%1 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] {hasGuid, ptrDb} -> memref<2x2xf64>
     useCount=2 hasPtrDb=true userEdtPos=0 parent=#2
    Dim #0: 0 -> 2
    Dim #1: 0 -> 2
  #5 inout
    %44 = arts.datablock "inout" ptr[%0 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] {hasGuid, ptrDb} -> memref<15x15xf64>
     useCount=2 hasPtrDb=true userEdtPos=1 parent=#1
    Dim #0: <block argument> of type 'i32' at index: 0 -> <block argument> of type 'i32' at index: 0
    Dim #1: 0 -> 15
  #6 in
    %45 = arts.datablock "in" ptr[%2 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] {hasGuid, ptrDb} -> memref<16x16xf64>
     useCount=2 hasPtrDb=true userEdtPos=2 parent=#3
    Dim #0: %56 = arith.addi %arg3, %55 : i32 -> %56 = arith.addi %arg3, %55 : i32
    Dim #1: %60 = arith.addi %53, %59 : i32 -> %60 = arith.addi %53, %59 : i32
Edges:
  #0 -> #2
  #0 -> #1
  #0 -> #3
  #0 -> #4
  #0 -> #5
  #0 -> #6
  #5 -> #5
Total nodes: 7
Analyzing 7 datablocks
Analyzing datablock: %0 = arts.datablock "inout" ptr[%alloca_2 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] -> memref<15x15xf64>
- Shrinking datablock 
Analyzing datablock: %4 = arts.datablock "in" ptr[%alloca_7 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] -> memref<2x2xf64>
- Shrinking datablock 
Analyzing datablock: %8 = arts.datablock "in" ptr[%alloca_12 : memref<16x16xf64>], indices[], offsets[%c0, %c0], sizes[%c16, %c16], type[f64], typeSize[%c8] -> memref<16x16xf64>
- Shrinking datablock 
Analyzing datablock: %52 = arts.datablock "in" ptr[%6 : memref<2x2xf64>], indices[], offsets[%c0, %c0], sizes[%c2, %c2], type[f64], typeSize[%c8] {hasGuid, ptrDb} -> memref<2x2xf64>
- Shrinking datablock 
Analyzing datablock: %56 = arts.datablock "inout" ptr[%2 : memref<15x15xf64>], indices[], offsets[%c0, %c0], sizes[%c15, %c15], type[f64], typeSize[%c8] {hasGuid, ptrDb} -> memref<15x15xf64>
Converting to index: <block argument> of type 'i32' at index: 0
carts-opt: /home/randres/projects/carts/lib/arts/Passes/Datablock.cpp:207: auto (anonymous namespace)::DatablockPass::shrinkDatablock()::(anonymous class)::operator()(ValueOrInt, mlir::Operation *, mlir::OpBuilder &) const: Assertion `op && "Value must have a defining operation"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt convolution.mlir --lower-affine --cse --loop-restructure --canonicalize-scf-for --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --hoist-invariant --create-datablocks --canonicalize-polygeist --canonicalize-scf-for --datablock --cse --canonicalize-scf-for --canonicalize-polygeist --polygeist-mem2reg --create-events --create-epochs -debug-only=datablock,datablock-analysis
 #0 0x000055f55c4e2437 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11c2437)
 #1 0x000055f55c4e000e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11c000e)
 #2 0x000055f55c4e2aea SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f5bb0b4b520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f5bb0b9f9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f5bb0b4b476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f5bb0b317f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f5bb0b3171b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f5bb0b42e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x000055f55bb27bc6 (anonymous namespace)::DatablockPass::shrinkDatablock()::$_4::operator()(ValueOrInt, mlir::Operation*, mlir::OpBuilder&) const Datablock.cpp:0:0
#10 0x000055f55bb26458 void llvm::function_ref<void (mlir::Operation*)>::callback_fn<std::enable_if<!llvm::is_one_of<mlir::func::FuncOp, mlir::Operation*, mlir::Region*, mlir::Block*>::value && std::is_same<void, void>::value, void>::type mlir::detail::walk<(mlir::WalkOrder)1, mlir::ForwardIterator, (anonymous namespace)::DatablockPass::shrinkDatablock()::$_1, mlir::func::FuncOp, void>(mlir::Operation*, (anonymous namespace)::DatablockPass::shrinkDatablock()::$_1&&)::'lambda'(mlir::Operation*)>(long, mlir::Operation*) Datablock.cpp:0:0
#11 0x000055f55b4c4dbe void mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<void (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1a4dbe)
#12 0x000055f55bb22cb9 (anonymous namespace)::DatablockPass::runOnOperation() Datablock.cpp:0:0
#13 0x000055f55c3168f4 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xff68f4)
#14 0x000055f55c316f21 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xff6f21)
#15 0x000055f55c3193d2 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xff93d2)
#16 0x000055f55bb6f924 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#17 0x000055f55bb6eb94 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#18 0x000055f55c477b38 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1157b38)
#19 0x000055f55bb6919a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x84919a)
#20 0x000055f55bb69664 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x849664)
#21 0x000055f55b460056 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x140056)
#22 0x00007f5bb0b32d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#23 0x00007f5bb0b32e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#24 0x000055f55b45f755 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x13f755)
