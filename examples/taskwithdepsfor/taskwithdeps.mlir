module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = "polygeist.memref2pointer"(%arg0) : (memref<?xi8>) -> !llvm.ptr
    %1 = llvm.getelementptr %0[0, 3] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i32, memref<?xf64>, memref<?xf64>, f64)>
    %2 = call @rand() : () -> i32
    %3 = arith.remsi %2, %c100_i32 : i32
    %4 = arith.sitofp %3 : i32 to f64
    llvm.store %4, %1 : f64, !llvm.ptr
    omp.parallel   {
      omp.barrier
      omp.master {
        %5 = llvm.getelementptr %0[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i32, memref<?xf64>, memref<?xf64>, f64)>
        %6 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i32, memref<?xf64>, memref<?xf64>, f64)>
        %7 = scf.while (%arg1 = %c0_i32) : (i32) -> i32 {
          %8 = llvm.load %0 : !llvm.ptr -> i32
          %9 = arith.cmpi slt, %arg1, %8 : i32
          scf.condition(%9) %arg1 : i32
        } do {
        ^bb0(%arg1: i32):
          %8 = llvm.load %5 : !llvm.ptr -> memref<?xf64>
          %9 = arith.index_cast %arg1 : i32 to index
          %10 = memref.load %8[%9] : memref<?xf64>
          %alloca = memref.alloca() : memref<f64>
          affine.store %10, %alloca[] : memref<f64>
          omp.task   depend(taskdependout -> %alloca : memref<f64>) {
            %20 = llvm.load %5 : !llvm.ptr -> memref<?xf64>
            %21 = arith.sitofp %arg1 : i32 to f64
            %22 = llvm.load %1 : !llvm.ptr -> f64
            %23 = arith.addf %21, %22 : f64
            memref.store %23, %20[%9] : memref<?xf64>
            omp.terminator
          }
          %11 = llvm.load %5 : !llvm.ptr -> memref<?xf64>
          %12 = memref.load %11[%9] : memref<?xf64>
          %alloca_0 = memref.alloca() : memref<f64>
          affine.store %12, %alloca_0[] : memref<f64>
          %13 = llvm.load %5 : !llvm.ptr -> memref<?xf64>
          %14 = arith.addi %arg1, %c-1_i32 : i32
          %15 = arith.index_cast %14 : i32 to index
          %16 = memref.load %13[%15] : memref<?xf64>
          %alloca_1 = memref.alloca() : memref<f64>
          affine.store %16, %alloca_1[] : memref<f64>
          %17 = llvm.load %6 : !llvm.ptr -> memref<?xf64>
          %18 = memref.load %17[%9] : memref<?xf64>
          %alloca_2 = memref.alloca() : memref<f64>
          affine.store %18, %alloca_2[] : memref<f64>
          omp.task   depend(taskdependin -> %alloca_0 : memref<f64>, taskdependin -> %alloca_1 : memref<f64>, taskdependout -> %alloca_2 : memref<f64>) {
            %20 = llvm.load %6 : !llvm.ptr -> memref<?xf64>
            %21 = llvm.load %5 : !llvm.ptr -> memref<?xf64>
            %22 = memref.load %21[%9] : memref<?xf64>
            %23 = arith.cmpi sgt, %arg1, %c0_i32 : i32
            %24 = scf.if %23 -> (f64) {
              %26 = llvm.load %5 : !llvm.ptr -> memref<?xf64>
              %27 = memref.load %26[%15] : memref<?xf64>
              scf.yield %27 : f64
            } else {
              scf.yield %cst : f64
            }
            %25 = arith.addf %22, %24 : f64
            memref.store %25, %20[%9] : memref<?xf64>
            omp.terminator
          }
          %19 = arith.addi %arg1, %c1_i32 : i32
          scf.yield %19 : i32
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
