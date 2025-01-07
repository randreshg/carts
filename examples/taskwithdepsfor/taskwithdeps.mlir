#set = affine_set<(d0) : (d0 - 1 >= 0)>
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100_i32 = arith.constant 100 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %0 = arith.index_cast %arg0 : i32 to index
    %1 = call @rand() : () -> i32
    %2 = arith.remsi %1, %c100_i32 : i32
    %3 = arith.sitofp %2 : i32 to f64
    omp.parallel   {
      omp.barrier
      %alloca = memref.alloca() : memref<f64>
      %alloca_0 = memref.alloca() : memref<f64>
      %alloca_1 = memref.alloca() : memref<f64>
      %alloca_2 = memref.alloca() : memref<f64>
      omp.master {
        affine.for %arg3 = 0 to %0 {
          %4 = arith.index_cast %arg3 : index to i32
          %5 = affine.load %arg1[%arg3] : memref<?xf64>
          affine.store %5, %alloca[] : memref<f64>
          omp.task   depend(taskdependout -> %alloca : memref<f64>) {
            %9 = arith.sitofp %4 : i32 to f64
            %10 = arith.addf %9, %3 : f64
            affine.store %10, %arg1[%arg3] : memref<?xf64>
            omp.terminator
          }
          %6 = affine.load %arg1[%arg3] : memref<?xf64>
          affine.store %6, %alloca_0[] : memref<f64>
          %7 = affine.load %arg1[%arg3 - 1] : memref<?xf64>
          affine.store %7, %alloca_1[] : memref<f64>
          %8 = affine.load %arg2[%arg3] : memref<?xf64>
          affine.store %8, %alloca_2[] : memref<f64>
          omp.task   depend(taskdependin -> %alloca_0 : memref<f64>, taskdependin -> %alloca_1 : memref<f64>, taskdependout -> %alloca_2 : memref<f64>) {
            %9 = affine.load %arg1[%arg3] : memref<?xf64>
            %10 = affine.if #set(%arg3) -> f64 {
              %12 = affine.load %arg1[%arg3 - 1] : memref<?xf64>
              affine.yield %12 : f64
            } else {
              affine.yield %cst : f64
            }
            %11 = arith.addf %9, %10 : f64
            affine.store %11, %arg2[%arg3] : memref<?xf64>
            omp.terminator
          }
        }
        omp.terminator
      }
      omp.barrier
      omp.terminator
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
