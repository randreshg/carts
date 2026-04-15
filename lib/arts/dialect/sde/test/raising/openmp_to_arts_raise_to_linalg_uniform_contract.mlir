// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify that a raiseable elementwise OpenMP loop is classified and gets a
// carrier-authoritative linalg.generic carrier at the SDE layer during
// RaiseToLinalg. The scalar body is erased and the carrier is the sole
// representation. mu_memref_to_tensor converts memrefs at the SDE boundary.

// CHECK-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// CHECK: func.func @main
// CHECK: arts_sde.cu_region <parallel> {
// CHECK: arts_sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
// CHECK: %[[IN:.+]] = arts_sde.mu_memref_to_tensor %arg0 : memref<128xf64> -> tensor<128xf64>
// CHECK: %[[OUT:.+]] = arts_sde.mu_memref_to_tensor %arg1 : memref<128xf64> -> tensor<128xf64>
// CHECK: linalg.generic
// CHECK-SAME: iterator_types = ["parallel"]
// CHECK-SAME: ins(%[[IN]] : tensor<128xf64>)
// CHECK-SAME: outs(%[[OUT]] : tensor<128xf64>)
// CHECK-NOT: memref.load
// CHECK-NOT: memref.store
// CHECK-NOT: arts.for
// CHECK: // -----// IR Dump After LoopInterchange

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf64>, %B: memref<128xf64>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %v = memref.load %A[%i] : memref<128xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %B[%i] : memref<128xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
