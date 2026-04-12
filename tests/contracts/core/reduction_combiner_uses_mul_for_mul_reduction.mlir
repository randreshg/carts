// RUN: %carts-compile %s --O3 --arts-config %S/../inputs/arts_2t.cfg --pipeline edt-distribution | %FileCheck %s

// Verify that a multiplication reduction uses arith.muli in the result EDT
// combine loop, not arith.addi. This validates that the SDE reduction kind
// flows through to EdtReductionLowering.

// The result EDT must use arith.muli (not arith.addi) and identity 1 (not 0).
// CHECK-LABEL: func.func @main
// CHECK: arts.edt <task> <intranode> route(%{{.*}}) (%{{.*}}, %{{.*}})
// CHECK: arith.constant 1 : i32
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} iter_args(%{{.*}} = %{{.*}}) -> (i32)
// CHECK: arith.muli %{{.*}}, %{{.*}} : i32
// CHECK: arith.muli %{{.*}}, %{{.*}} : i32

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  omp.declare_reduction @mul_i32 : i32 init {
  ^bb0(%arg0: i32):
    %c1_i32 = arith.constant 1 : i32
    omp.yield(%c1_i32 : i32)
  } combiner {
  ^bb0(%lhs: i32, %rhs: i32):
    %prod = arith.muli %lhs, %rhs : i32
    omp.yield(%prod : i32)
  }

  func.func @main(%A: memref<16xi32>) {
    %c0 = arith.constant 0 : index
    %c16 = arith.constant 16 : index
    %c1 = arith.constant 1 : index
    %prod = memref.alloca() : memref<1xi32>
    %prod_cast = memref.cast %prod : memref<1xi32> to memref<?xi32>
    %c1_i32 = arith.constant 1 : i32
    memref.store %c1_i32, %prod[%c0] : memref<1xi32>
    omp.parallel {
      omp.wsloop reduction(@mul_i32 %prod_cast -> %prv : memref<?xi32>) {
        omp.loop_nest (%i) : index = (%c0) to (%c16) step (%c1) {
          %val = memref.load %A[%i] : memref<16xi32>
          %acc = memref.load %prv[%c0] : memref<?xi32>
          %next = arith.muli %acc, %val : i32
          memref.store %next, %prv[%c0] : memref<?xi32>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
