// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline edt-distribution | %FileCheck %s

// Test that plan attributes survive through the edt-distribution stage.
// Both plan attrs and distribution attrs should be present on the output.

// CHECK-DAG: arts.plan.kernel_family = "stencil"
// CHECK-DAG: distribution_kind

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c63) step (%c1) {
        scf.for %j = %c1 to %c63 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %n = memref.load %A[%im1, %j] : memref<64x64xf64>
          %s = memref.load %A[%ip1, %j] : memref<64x64xf64>
          %c = memref.load %A[%i, %j] : memref<64x64xf64>
          %s0 = arith.addf %n, %s : f64
          %sum = arith.addf %s0, %c : f64
          memref.store %sum, %B[%i, %j] : memref<64x64xf64>
        }
        omp.yield
      }
      }
      omp.terminator
    }
    return
  }
}
