// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline post-db-refinement | %FileCheck %s

// Test that CPS EDTs with plan attrs use the launch.state_schema / dep_schema
// surface when available, instead of legacy cps_param_perm / cps_dep_routing.
// When plan is present, the split launch state schema should be preferred.

// This test currently validates that plan-bearing EDTs do NOT have mixed
// schema (both legacy and new). The actual split-op emission is Phase 2.

// CHECK-NOT: mixed CPS schema

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<64x64xf64>, %B: memref<64x64xf64>) {
    %c1 = arith.constant 1 : index
    %c63 = arith.constant 63 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c1) to (%c63) step (%c1) {
        scf.for %j = %c1 to %c63 step %c1 {
          %v = memref.load %A[%i, %j] : memref<64x64xf64>
          %r = arith.addf %v, %v : f64
          memref.store %r, %B[%i, %j] : memref<64x64xf64>
        }
        omp.yield
      }
      }
      omp.terminator
    }
    return
  }
}
