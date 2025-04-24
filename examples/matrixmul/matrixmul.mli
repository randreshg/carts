module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str12("Verification FAILED\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str11("Verification PASSED\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str10("%s\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str9("----------------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("\0AMatrix C (Sequential):\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("\0AMatrix C (Parallel):\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("\0AMatrix B:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("%6.2f \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Matrix A:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Mismatch at (%d,%d): Parallel=%.2f, Sequential=%.2f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str1("Matrix size and block size must be positive integers.\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Usage: %s <matrix_size> <block_size>\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %false = arith.constant false
    %true = arith.constant true
    %cst = arith.constant 9.9999999999999995E-7 : f64
    %cst_0 = arith.constant 0.000000e+00 : f32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c3_i32 = arith.constant 3 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arith.cmpi ne, %arg0, %c3_i32 : i32
    %2 = arith.cmpi eq, %arg0, %c3_i32 : i32
    %3 = arith.select %1, %c1_i32, %0 : i32
    scf.if %1 {
      %6 = llvm.mlir.addressof @stderr : !llvm.ptr
      %7 = llvm.load %6 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %8 = "polygeist.memref2pointer"(%7) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %9 = llvm.mlir.addressof @str0 : !llvm.ptr
      %10 = llvm.getelementptr %9[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
      %11 = affine.load %arg1[0] : memref<?xmemref<?xi8>>
      %12 = "polygeist.memref2pointer"(%11) : (memref<?xi8>) -> !llvm.ptr
      %13 = llvm.call @fprintf(%8, %10, %12) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    }
    %4:4 = scf.if %2 -> (i32, i32, i1, i32) {
      %6 = affine.load %arg1[1] : memref<?xmemref<?xi8>>
      %7 = func.call @atoi(%6) : (memref<?xi8>) -> i32
      %8 = arith.cmpi sgt, %7, %c0_i32 : i32
      %9 = affine.load %arg1[2] : memref<?xmemref<?xi8>>
      %10 = func.call @atoi(%9) : (memref<?xi8>) -> i32
      %11 = arith.cmpi sle, %10, %c0_i32 : i32
      %12 = arith.cmpi sle, %7, %c0_i32 : i32
      %13 = arith.andi %8, %11 : i1
      %14 = arith.ori %12, %13 : i1
      %15 = arith.xori %14, %true : i1
      %16 = arith.select %14, %c1_i32, %3 : i32
      scf.if %14 {
        %17 = llvm.mlir.addressof @stderr : !llvm.ptr
        %18 = llvm.load %17 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
        %19 = "polygeist.memref2pointer"(%18) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
        %20 = llvm.mlir.addressof @str1 : !llvm.ptr
        %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<55 x i8>
        %22 = llvm.call @fprintf(%19, %21) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
      }
      scf.yield %7, %10, %15, %16 : i32, i32, i1, i32
    } else {
      scf.yield %0, %0, %false, %3 : i32, i32, i1, i32
    }
    %5 = arith.select %4#2, %c0_i32, %4#3 : i32
    scf.if %4#2 {
      %6 = arith.index_cast %4#0 : i32 to index
      %alloca = memref.alloca(%6, %6) : memref<?x?xf32>
      %alloca_1 = memref.alloca(%6, %6) : memref<?x?xf32>
      %alloca_2 = memref.alloca(%6, %6) : memref<?x?xf32>
      %alloca_3 = memref.alloca(%6, %6) : memref<?x?xf32>
      %7 = arith.index_cast %4#0 : i32 to index
      scf.for %arg2 = %c0 to %7 step %c1 {
        %41 = arith.index_cast %arg2 : index to i32
        %42 = arith.index_cast %4#0 : i32 to index
        scf.for %arg3 = %c0 to %42 step %c1 {
          %43 = arith.index_cast %arg3 : index to i32
          %44 = arith.addi %41, %43 : i32
          %45 = arith.sitofp %44 : i32 to f32
          memref.store %45, %alloca[%arg2, %arg3] : memref<?x?xf32>
          %46 = arith.subi %41, %43 : i32
          %47 = arith.sitofp %46 : i32 to f32
          memref.store %47, %alloca_1[%arg2, %arg3] : memref<?x?xf32>
          memref.store %cst_0, %alloca_2[%arg2, %arg3] : memref<?x?xf32>
          memref.store %cst_0, %alloca_3[%arg2, %arg3] : memref<?x?xf32>
        }
      }
      omp.parallel   {
        omp.barrier
        omp.master {
          %41 = arith.index_cast %4#0 : i32 to index
          %42 = arith.index_cast %4#1 : i32 to index
          scf.for %arg2 = %c0 to %41 step %42 {
            %43 = arith.divui %arg2, %42 : index
            %44 = arith.index_cast %4#1 : i32 to index
            %45 = arith.muli %43, %44 : index
            %46 = arith.index_cast %45 : index to i32
            %47 = arith.index_cast %4#0 : i32 to index
            %48 = arith.index_cast %4#1 : i32 to index
            scf.for %arg3 = %c0 to %47 step %48 {
              %49 = arith.divui %arg3, %48 : index
              %50 = arith.index_cast %4#1 : i32 to index
              %51 = arith.muli %49, %50 : index
              %52 = arith.index_cast %51 : index to i32
              %53 = arith.index_cast %4#0 : i32 to index
              %54 = arith.index_cast %4#1 : i32 to index
              scf.for %arg4 = %c0 to %53 step %54 {
                %55 = arith.divui %arg4, %54 : index
                %56 = arith.index_cast %4#1 : i32 to index
                %57 = arith.muli %55, %56 : index
                %58 = arith.index_cast %57 : index to i32
                omp.task   {
                  %59 = arith.addi %46, %4#1 : i32
                  %60 = arith.index_cast %59 : i32 to index
                  scf.for %arg5 = %45 to %60 step %c1 {
                    %61 = arith.addi %52, %4#1 : i32
                    %62 = arith.index_cast %61 : i32 to index
                    scf.for %arg6 = %51 to %62 step %c1 {
                      %63 = arith.addi %58, %4#1 : i32
                      %64 = arith.index_cast %63 : i32 to index
                      scf.for %arg7 = %57 to %64 step %c1 {
                        %65 = memref.load %alloca[%arg5, %arg7] : memref<?x?xf32>
                        %66 = memref.load %alloca_1[%arg7, %arg6] : memref<?x?xf32>
                        %67 = arith.mulf %65, %66 : f32
                        %68 = memref.load %alloca_2[%arg5, %arg6] : memref<?x?xf32>
                        %69 = arith.addf %68, %67 : f32
                        memref.store %69, %alloca_2[%arg5, %arg6] : memref<?x?xf32>
                      }
                    }
                  }
                  omp.terminator
                }
              }
            }
          }
          omp.terminator
        }
        omp.barrier
        omp.terminator
      }
      %8 = arith.index_cast %4#0 : i32 to index
      scf.for %arg2 = %c0 to %8 step %c1 {
        %41 = arith.index_cast %4#0 : i32 to index
        scf.for %arg3 = %c0 to %41 step %c1 {
          %42 = arith.index_cast %4#0 : i32 to index
          %43 = scf.for %arg4 = %c0 to %42 step %c1 iter_args(%arg5 = %cst_0) -> (f32) {
            %44 = memref.load %alloca[%arg2, %arg4] : memref<?x?xf32>
            %45 = memref.load %alloca_1[%arg4, %arg3] : memref<?x?xf32>
            %46 = arith.mulf %44, %45 : f32
            %47 = arith.addf %arg5, %46 : f32
            scf.yield %47 : f32
          }
          memref.store %43, %alloca_3[%arg2, %arg3] : memref<?x?xf32>
        }
      }
      %9 = arith.index_cast %4#0 : i32 to index
      %10 = scf.for %arg2 = %c0 to %9 step %c1 iter_args(%arg3 = %c1_i32) -> (i32) {
        %41 = arith.index_cast %arg2 : index to i32
        %42 = arith.index_cast %4#0 : i32 to index
        %43 = scf.for %arg4 = %c0 to %42 step %c1 iter_args(%arg5 = %arg3) -> (i32) {
          %44 = arith.index_cast %arg4 : index to i32
          %45 = memref.load %alloca_2[%arg2, %arg4] : memref<?x?xf32>
          %46 = memref.load %alloca_3[%arg2, %arg4] : memref<?x?xf32>
          %47 = arith.subf %45, %46 : f32
          %48 = arith.extf %47 : f32 to f64
          %49 = math.absf %48 : f64
          %50 = arith.cmpf ogt, %49, %cst : f64
          %51 = arith.select %50, %c0_i32, %arg5 : i32
          scf.if %50 {
            %52 = llvm.mlir.addressof @str2 : !llvm.ptr
            %53 = llvm.getelementptr %52[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<53 x i8>
            %54 = arith.extf %45 : f32 to f64
            %55 = arith.extf %46 : f32 to f64
            %56 = llvm.call @printf(%53, %41, %44, %54, %55) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64, f64) -> i32
          }
          scf.yield %51 : i32
        }
        scf.yield %43 : i32
      }
      %11 = arith.cmpi ne, %10, %c0_i32 : i32
      %12 = llvm.mlir.addressof @str3 : !llvm.ptr
      %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
      %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %15 = arith.index_cast %4#0 : i32 to index
      scf.for %arg2 = %c0 to %15 step %c1 {
        %41 = arith.index_cast %4#0 : i32 to index
        scf.for %arg3 = %c0 to %41 step %c1 {
          %45 = llvm.mlir.addressof @str4 : !llvm.ptr
          %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
          %47 = memref.load %alloca[%arg2, %arg3] : memref<?x?xf32>
          %48 = arith.extf %47 : f32 to f64
          %49 = llvm.call @printf(%46, %48) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
        }
        %42 = llvm.mlir.addressof @str5 : !llvm.ptr
        %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
        %44 = llvm.call @printf(%43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      }
      %16 = llvm.mlir.addressof @str6 : !llvm.ptr
      %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<12 x i8>
      %18 = llvm.call @printf(%17) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %19 = arith.index_cast %4#0 : i32 to index
      scf.for %arg2 = %c0 to %19 step %c1 {
        %41 = arith.index_cast %4#0 : i32 to index
        scf.for %arg3 = %c0 to %41 step %c1 {
          %45 = llvm.mlir.addressof @str4 : !llvm.ptr
          %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
          %47 = memref.load %alloca_1[%arg2, %arg3] : memref<?x?xf32>
          %48 = arith.extf %47 : f32 to f64
          %49 = llvm.call @printf(%46, %48) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
        }
        %42 = llvm.mlir.addressof @str5 : !llvm.ptr
        %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
        %44 = llvm.call @printf(%43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      }
      %20 = llvm.mlir.addressof @str7 : !llvm.ptr
      %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
      %22 = llvm.call @printf(%21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %23 = arith.index_cast %4#0 : i32 to index
      scf.for %arg2 = %c0 to %23 step %c1 {
        %41 = arith.index_cast %4#0 : i32 to index
        scf.for %arg3 = %c0 to %41 step %c1 {
          %45 = llvm.mlir.addressof @str4 : !llvm.ptr
          %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
          %47 = memref.load %alloca_2[%arg2, %arg3] : memref<?x?xf32>
          %48 = arith.extf %47 : f32 to f64
          %49 = llvm.call @printf(%46, %48) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
        }
        %42 = llvm.mlir.addressof @str5 : !llvm.ptr
        %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
        %44 = llvm.call @printf(%43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      }
      %24 = llvm.mlir.addressof @str8 : !llvm.ptr
      %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<25 x i8>
      %26 = llvm.call @printf(%25) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %27 = arith.index_cast %4#0 : i32 to index
      scf.for %arg2 = %c0 to %27 step %c1 {
        %41 = arith.index_cast %4#0 : i32 to index
        scf.for %arg3 = %c0 to %41 step %c1 {
          %45 = llvm.mlir.addressof @str4 : !llvm.ptr
          %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
          %47 = memref.load %alloca_3[%arg2, %arg3] : memref<?x?xf32>
          %48 = arith.extf %47 : f32 to f64
          %49 = llvm.call @printf(%46, %48) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
        }
        %42 = llvm.mlir.addressof @str5 : !llvm.ptr
        %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
        %44 = llvm.call @printf(%43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      }
      %28 = llvm.mlir.addressof @str5 : !llvm.ptr
      %29 = llvm.getelementptr %28[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %30 = llvm.call @printf(%29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %31 = llvm.mlir.addressof @str9 : !llvm.ptr
      %32 = llvm.getelementptr %31[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
      %33 = llvm.call @printf(%32) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %34 = llvm.mlir.addressof @str10 : !llvm.ptr
      %35 = llvm.getelementptr %34[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<4 x i8>
      %36 = scf.if %11 -> (!llvm.ptr) {
        %41 = llvm.mlir.addressof @str11 : !llvm.ptr
        %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<20 x i8>
        scf.yield %42 : !llvm.ptr
      } else {
        %41 = llvm.mlir.addressof @str12 : !llvm.ptr
        %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<20 x i8>
        scf.yield %42 : !llvm.ptr
      }
      %37 = llvm.call @printf(%35, %36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
      %38 = llvm.mlir.addressof @str9 : !llvm.ptr
      %39 = llvm.getelementptr %38[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
      %40 = llvm.call @printf(%39) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %5 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
