module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    omp.parallel   {
      omp.barrier
      %alloca_1 = memref.alloca() : memref<f64>
      %alloca_2 = memref.alloca() : memref<f64>
      %alloca_3 = memref.alloca() : memref<f64>
      %alloca_4 = memref.alloca() : memref<f64>
      omp.master {
        %10 = arith.sitofp %1 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %13 = arith.index_cast %arg0 : index to i32
          %14 = memref.load %alloca_0[%arg0, %c0] : memref<100x100xf64>
          affine.store %14, %alloca_4[] : memref<f64>
          omp.task   depend(taskdependout -> %alloca_4 : memref<f64>) {
            %15 = arith.sitofp %13 : i32 to f64
            %16 = arith.addf %15, %10 : f64
            scf.for %arg1 = %c0 to %c100 step %c1 {
              memref.store %16, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
            }
            omp.terminator
          }
        }
        %11 = affine.load %alloca_0[0, 0] : memref<100x100xf64>
        %alloca_5 = memref.alloca() : memref<f64>
        affine.store %11, %alloca_5[] : memref<f64>
        %12 = affine.load %alloca[0, 0] : memref<100x100xf64>
        %alloca_6 = memref.alloca() : memref<f64>
        affine.store %12, %alloca_6[] : memref<f64>
        omp.task   depend(taskdependin -> %alloca_5 : memref<f64>, taskdependout -> %alloca_6 : memref<f64>) {
          scf.for %arg0 = %c0 to %c100 step %c1 {
            %13 = memref.load %alloca_0[%c0, %arg0] : memref<100x100xf64>
            memref.store %13, %alloca[%c0, %arg0] : memref<100x100xf64>
          }
          omp.terminator
        }
        scf.for %arg0 = %c1 to %c100 step %c1 {
          %13 = arith.index_cast %arg0 : index to i32
          %14 = memref.load %alloca_0[%arg0, %c0] : memref<100x100xf64>
          affine.store %14, %alloca_1[] : memref<f64>
          %15 = arith.addi %13, %c-1_i32 : i32
          %16 = arith.index_cast %15 : i32 to index
          %17 = memref.load %alloca_0[%16, %c0] : memref<100x100xf64>
          affine.store %17, %alloca_2[] : memref<f64>
          %18 = memref.load %alloca[%arg0, %c0] : memref<100x100xf64>
          affine.store %18, %alloca_3[] : memref<f64>
          omp.task   depend(taskdependin -> %alloca_1 : memref<f64>, taskdependin -> %alloca_2 : memref<f64>, taskdependout -> %alloca_3 : memref<f64>) {
            scf.for %arg1 = %c0 to %c100 step %c1 {
              %19 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
              %20 = memref.load %alloca_0[%16, %arg1] : memref<100x100xf64>
              %21 = arith.addf %19, %20 : f64
              memref.store %21, %alloca[%arg0, %arg1] : memref<100x100xf64>
            }
            omp.terminator
          }
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    %2 = llvm.mlir.addressof @str0 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %4 = llvm.mlir.addressof @str1 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%3, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%5) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %6 = llvm.mlir.addressof @str2 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %8 = llvm.mlir.addressof @str1 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %alloca[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%7, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
