// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline openmp-to-arts | %FileCheck %s

// Verify that a raiseable elementwise OpenMP loop is classified during the
// openmp-to-arts stage, before the later pattern pipeline runs. If these
// attrs appear here, they came from RaiseToLinalg + ConvertSdeToArts.

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel> <intranode> route(%{{.*}}) attributes {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>
// CHECK: arts.for(%c0) to(%c128) step(%c1)
// CHECK: ^bb0(%[[IV:.*]]: index):
// CHECK: %[[VAL:.+]] = memref.load %arg0[%[[IV]]] : memref<128xf64>
// CHECK: %[[MUL:.+]] = arith.mulf %[[VAL]], %cst : f64
// CHECK: memref.store %[[MUL]], %arg1[%[[IV]]] : memref<128xf64>
// CHECK: } {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<uniform>, distribution_pattern = #arts.distribution_pattern<uniform>}
// CHECK-NOT: linalg.generic
// CHECK-NOT: arts_sde.

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
