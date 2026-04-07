// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline post-db-refinement | %FileCheck %s

// Test that when no plan attrs are present (e.g. irregular kernel), the legacy
// CPS schema path remains functional and produces valid IR without errors.
// The generic lowering path must continue to work unchanged.

// CHECK-NOT: arts.plan.kernel_family
// The key assertion: compilation completes without error through post-db-refinement.

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64xf64>) {
    %c0 = arith.constant 0 : index
    %c64 = arith.constant 64 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 1.5 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c64) step (%c1) {
          %v = memref.load %A[%i] : memref<64xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %A[%i] : memref<64xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
