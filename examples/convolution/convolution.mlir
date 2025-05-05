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
    %alloca_3 = memref.alloca() : memref<2x2xf64>
    %alloca_4 = memref.alloca() : memref<16x16xf64>
    scf.for %arg2 = %c0 to %c16 step %c1 {
      scf.for %arg3 = %c0 to %c16 step %c1 {
        memref.store %cst_1, %alloca_4[%arg2, %arg3] : memref<16x16xf64>
      }
    }
    scf.for %arg2 = %c0 to %c2 step %c1 {
      scf.for %arg3 = %c0 to %c2 step %c1 {
        memref.store %cst_1, %alloca_3[%arg2, %arg3] : memref<2x2xf64>
      }
    }
    omp.parallel   {
      omp.barrier
      omp.master {
        scf.for %arg2 = %c0 to %c4 step %c1 {
          %30 = arith.index_cast %arg2 : index to i32
          %31 = arith.muli %30, %c4_i32 : i32
          %32 = llvm.mlir.addressof @str0 : !llvm.ptr
          %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
          %34 = arith.addi %30, %c1_i32 : i32
          %35 = llvm.call @printf(%33, %34, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
          omp.task   {
            %36 = llvm.mlir.addressof @str1 : !llvm.ptr
            %37 = llvm.getelementptr %36[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<17 x i8>
            %38 = llvm.call @printf(%37, %34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
            %39 = arith.addi %31, %c4_i32 : i32
            %40 = scf.while (%arg3 = %31) : (i32) -> i32 {
              %41 = arith.cmpi slt, %arg3, %39 : i32
              %42 = arith.cmpi slt, %arg3, %c15_i32 : i32
              %43 = arith.andi %41, %42 : i1
              scf.condition(%43) %arg3 : i32
            } do {
            ^bb0(%arg3: i32):
              scf.for %arg4 = %c0 to %c15 step %c1 {
                %42 = arith.index_cast %arg4 : index to i32
                %43 = scf.for %arg5 = %c0 to %c2 step %c1 iter_args(%arg6 = %cst_0) -> (f64) {
                  %45 = arith.index_cast %arg5 : index to i32
                  %46 = scf.for %arg7 = %c0 to %c2 step %c1 iter_args(%arg8 = %arg6) -> (f64) {
                    %47 = arith.index_cast %arg7 : index to i32
                    %48 = arith.addi %arg3, %45 : i32
                    %49 = arith.addi %42, %47 : i32
                    %50 = arith.cmpi slt, %48, %c16_i32 : i32
                    %51 = arith.cmpi slt, %49, %c16_i32 : i32
                    %52 = arith.andi %50, %51 : i1
                    %53 = scf.if %52 -> (f64) {
                      %54 = arith.index_cast %48 : i32 to index
                      %55 = arith.index_cast %49 : i32 to index
                      %56 = memref.load %alloca_4[%54, %55] : memref<16x16xf64>
                      %57 = memref.load %alloca_3[%arg5, %arg7] : memref<2x2xf64>
                      %58 = arith.mulf %56, %57 : f64
                      %59 = arith.addf %arg8, %58 : f64
                      scf.yield %59 : f64
                    } else {
                      scf.yield %arg8 : f64
                    }
                    scf.yield %53 : f64
                  }
                  scf.yield %46 : f64
                }
                %44 = arith.index_cast %arg3 : i32 to index
                memref.store %43, %alloca_2[%44, %arg4] : memref<15x15xf64>
              }
              %41 = arith.addi %arg3, %c1_i32 : i32
              scf.yield %41 : i32
            }
            omp.terminator
          }
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    scf.for %arg2 = %c0 to %c15 step %c1 {
      %30 = arith.index_cast %arg2 : index to i32
      scf.for %arg3 = %c0 to %c15 step %c1 {
        %31 = arith.index_cast %arg3 : index to i32
        %32 = scf.for %arg4 = %c0 to %c2 step %c1 iter_args(%arg5 = %cst_0) -> (f64) {
          %33 = arith.index_cast %arg4 : index to i32
          %34 = scf.for %arg6 = %c0 to %c2 step %c1 iter_args(%arg7 = %arg5) -> (f64) {
            %35 = arith.index_cast %arg6 : index to i32
            %36 = arith.addi %30, %33 : i32
            %37 = arith.addi %31, %35 : i32
            %38 = arith.cmpi slt, %36, %c16_i32 : i32
            %39 = arith.cmpi slt, %37, %c16_i32 : i32
            %40 = arith.andi %38, %39 : i1
            %41 = scf.if %40 -> (f64) {
              %42 = arith.index_cast %36 : i32 to index
              %43 = arith.index_cast %37 : i32 to index
              %44 = memref.load %alloca_4[%42, %43] : memref<16x16xf64>
              %45 = memref.load %alloca_3[%arg4, %arg6] : memref<2x2xf64>
              %46 = arith.mulf %44, %45 : f64
              %47 = arith.addf %arg7, %46 : f64
              scf.yield %47 : f64
            } else {
              scf.yield %arg7 : f64
            }
            scf.yield %41 : f64
          }
          scf.yield %34 : f64
        }
        memref.store %32, %alloca[%arg2, %arg3] : memref<15x15xf64>
      }
    }
    %0 = scf.for %arg2 = %c0 to %c15 step %c1 iter_args(%arg3 = %c0_i32) -> (i32) {
      %30 = arith.index_cast %arg2 : index to i32
      %31 = scf.for %arg4 = %c0 to %c15 step %c1 iter_args(%arg5 = %arg3) -> (i32) {
        %32 = arith.index_cast %arg4 : index to i32
        %33 = memref.load %alloca_2[%arg2, %arg4] : memref<15x15xf64>
        %34 = memref.load %alloca[%arg2, %arg4] : memref<15x15xf64>
        %35 = arith.subf %33, %34 : f64
        %36 = math.absf %35 : f64
        %37 = arith.cmpf ogt, %36, %cst : f64
        %38 = scf.if %37 -> (i32) {
          %39 = arith.addi %arg5, %c1_i32 : i32
          %40 = llvm.mlir.addressof @str2 : !llvm.ptr
          %41 = llvm.getelementptr %40[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
          %42 = llvm.call @printf(%41, %30, %32, %33, %34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64, f64) -> i32
          scf.yield %39 : i32
        } else {
          scf.yield %arg5 : i32
        }
        scf.yield %38 : i32
      }
      scf.yield %31 : i32
    }
    %1 = arith.cmpi sgt, %0, %c0_i32 : i32
    %2 = llvm.mlir.addressof @str3 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<16 x i8>
    %4 = llvm.call @printf(%3) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %c16 step %c1 {
      scf.for %arg3 = %c0 to %c16 step %c1 {
        %33 = llvm.mlir.addressof @str4 : !llvm.ptr
        %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %35 = memref.load %alloca_4[%arg2, %arg3] : memref<16x16xf64>
        %36 = llvm.call @printf(%34, %35) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %30 = llvm.mlir.addressof @str5 : !llvm.ptr
      %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %32 = llvm.call @printf(%31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %5 = llvm.mlir.addressof @str5 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    %7 = llvm.call @printf(%6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %8 = llvm.mlir.addressof @str6 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<21 x i8>
    %10 = llvm.call @printf(%9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %c2 step %c1 {
      scf.for %arg3 = %c0 to %c2 step %c1 {
        %33 = llvm.mlir.addressof @str4 : !llvm.ptr
        %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %35 = memref.load %alloca_3[%arg2, %arg3] : memref<2x2xf64>
        %36 = llvm.call @printf(%34, %35) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %30 = llvm.mlir.addressof @str5 : !llvm.ptr
      %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %32 = llvm.call @printf(%31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %11 = llvm.mlir.addressof @str5 : !llvm.ptr
    %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    %13 = llvm.call @printf(%12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %14 = llvm.mlir.addressof @str7 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<25 x i8>
    %16 = llvm.call @printf(%15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %c15 step %c1 {
      scf.for %arg3 = %c0 to %c15 step %c1 {
        %33 = llvm.mlir.addressof @str4 : !llvm.ptr
        %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %35 = memref.load %alloca_2[%arg2, %arg3] : memref<15x15xf64>
        %36 = llvm.call @printf(%34, %35) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %30 = llvm.mlir.addressof @str5 : !llvm.ptr
      %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %32 = llvm.call @printf(%31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %17 = llvm.mlir.addressof @str5 : !llvm.ptr
    %18 = llvm.getelementptr %17[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    %19 = llvm.call @printf(%18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %20 = llvm.mlir.addressof @str8 : !llvm.ptr
    %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %22 = llvm.call @printf(%21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    scf.for %arg2 = %c0 to %c15 step %c1 {
      scf.for %arg3 = %c0 to %c15 step %c1 {
        %33 = llvm.mlir.addressof @str4 : !llvm.ptr
        %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<7 x i8>
        %35 = memref.load %alloca[%arg2, %arg3] : memref<15x15xf64>
        %36 = llvm.call @printf(%34, %35) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
      }
      %30 = llvm.mlir.addressof @str5 : !llvm.ptr
      %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
      %32 = llvm.call @printf(%31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %23 = llvm.mlir.addressof @str5 : !llvm.ptr
    %24 = llvm.getelementptr %23[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    %25 = llvm.call @printf(%24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %26 = llvm.mlir.addressof @str9 : !llvm.ptr
    %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<25 x i8>
    %28 = llvm.call @printf(%27, %0) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %29 = arith.extui %1 : i1 to i32
    return %29 : i32
  }
}
