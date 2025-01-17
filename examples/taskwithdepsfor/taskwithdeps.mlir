module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c8 = arith.constant 8 : index
    %c8_i64 = arith.constant 8 : i64
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = arith.extsi %arg0 : i32 to i64
    %1 = arith.muli %0, %c8_i64 : i64
    %2 = arith.index_cast %1 : i64 to index
    %3 = arith.divui %2, %c8 : index
    %alloc = memref.alloc(%3) : memref<?xf64>
    %alloc_0 = memref.alloc(%3) : memref<?xf64>
    %4 = call @rand() : () -> i32
    %5 = arith.remsi %4, %c100_i32 : i32
    omp.parallel   {
      omp.barrier
      %alloca = memref.alloca() : memref<f64>
      %alloca_1 = memref.alloca() : memref<f64>
      %alloca_2 = memref.alloca() : memref<f64>
      %alloca_3 = memref.alloca() : memref<f64>
      omp.master {
        %6 = arith.index_cast %arg0 : i32 to index
        scf.for %arg3 = %c0 to %6 step %c1 {
          %7 = arith.index_cast %arg3 : index to i32
          %8 = memref.load %alloc[%arg3] : memref<?xf64>
          affine.store %8, %alloca[] : memref<f64>
          omp.task   depend(taskdependout -> %alloca : memref<f64>) {
            %14 = arith.sitofp %7 : i32 to f64
            %15 = arith.sitofp %5 : i32 to f64
            %16 = arith.addf %14, %15 : f64
            memref.store %16, %alloc[%arg3] : memref<?xf64>
            omp.terminator
          }
          %9 = memref.load %alloc[%arg3] : memref<?xf64>
          affine.store %9, %alloca_1[] : memref<f64>
          %10 = arith.addi %7, %c-1_i32 : i32
          %11 = arith.index_cast %10 : i32 to index
          %12 = memref.load %alloc[%11] : memref<?xf64>
          affine.store %12, %alloca_2[] : memref<f64>
          %13 = memref.load %alloc_0[%arg3] : memref<?xf64>
          affine.store %13, %alloca_3[] : memref<f64>
          omp.task   depend(taskdependin -> %alloca_1 : memref<f64>, taskdependin -> %alloca_2 : memref<f64>, taskdependout -> %alloca_3 : memref<f64>) {
            %14 = arith.cmpi sgt, %7, %c0_i32 : i32
            %15 = arith.select %14, %12, %cst : f64
            %16 = arith.addf %9, %15 : f64
            memref.store %16, %alloc_0[%arg3] : memref<?xf64>
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
