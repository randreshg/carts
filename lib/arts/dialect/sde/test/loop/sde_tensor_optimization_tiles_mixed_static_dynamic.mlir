// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Verify that Tiling handles elementwise loops with dynamic
// upper bounds. When the trip count is not statically known, the pass emits
// runtime tile computation (CeilDivSI, MaxUI, MinUI) instead of a static
// constant tile size.

// CHECK-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// CHECK: func.func @main
// Dynamic tile computation: trip count derived from runtime bound
// CHECK: arith.subi
// CHECK: arith.ceildivsi
// CHECK: arith.maxui
// CHECK: arts_sde.su_iterate
// CHECK-SAME: classification(<elementwise>)
// CHECK: scf.for
// CHECK: memref.load
// CHECK: arith.mulf
// CHECK: memref.store

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<?xf64>, %B: memref<?xf64>, %N: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%N) step (%c1) {
          %v = memref.load %A[%i] : memref<?xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %B[%i] : memref<?xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
