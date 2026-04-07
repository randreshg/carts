// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline edt-distribution | %FileCheck %s

// Test that irregular/indirect access patterns do NOT produce a structured
// kernel plan. The plan analysis should skip these kernels.

// CHECK-NOT: arts.plan.kernel_family
// CHECK-NOT: arts.plan.iteration_topology

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf64>, %idx: memref<128xi32>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %j32 = memref.load %idx[%i] : memref<128xi32>
          %j = arith.index_cast %j32 : i32 to index
          %v = memref.load %A[%j] : memref<128xf64>
          %r = arith.addf %v, %v : f64
          memref.store %r, %A[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
