module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1024 = arith.constant 1024 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %cst = arith.constant 0.000000e+00 : f64
    %cst_0 = arith.constant 2.000000e+00 : f64
    %alloc = memref.alloc() : memref<1024xf64>
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%arg0) : index = (%c0) to (%c1024) step (%c1) {
          %2 = arith.index_cast %arg0 : index to i32
          %3 = arith.sitofp %2 : i32 to f64
          %4 = arith.mulf %3, %cst_0 : f64
          memref.store %4, %alloc[%arg0] : memref<1024xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    %0 = affine.for %arg0 = 0 to 1024 iter_args(%arg1 = %cst) -> (f64) {
      %2 = affine.load %alloc[%arg0] : memref<1024xf64>
      %3 = arith.addf %arg1, %2 : f64
      affine.yield %3 : f64
    }
    memref.dealloc %alloc : memref<1024xf64>
    %1 = arith.fptosi %0 : f64 to i32
    return %1 : i32
  }
}
