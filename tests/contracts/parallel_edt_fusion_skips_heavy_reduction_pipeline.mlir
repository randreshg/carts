// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --start-from edt-opt --pipeline edt-opt | %FileCheck %s

// Keep sibling parallel EDTs separate when they form independent
// reduction-like pipelines that already amortize launch overhead and only
// share a read-only input.
//
// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK: arts.for(%c0) to(%c64) step(%c1)
// CHECK: arts.edt <parallel>
// CHECK: arts.for(%c0) to(%c64) step(%c1)

module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main(%arg0: memref<64xf64>, %arg1: memref<64xf64>,
                  %arg2: memref<64xf64>) -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %zero = arith.constant 0.000000e+00 : f64

    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c64) step(%c1) {{
      ^bb0(%iv: index):
        %sum = scf.for %k = %c0 to %c16 step %c1 iter_args(%acc = %zero) -> (f64) {
          %val = memref.load %arg0[%iv] : memref<64xf64>
          %next = arith.addf %acc, %val : f64
          scf.yield %next : f64
        }
        memref.store %sum, %arg1[%iv] : memref<64xf64>
        arts.yield
      }} {arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = true, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "bicg_like_left">}
      arts.yield
    }

    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c64) step(%c1) {{
      ^bb0(%iv: index):
        %sum = scf.for %k = %c0 to %c16 step %c1 iter_args(%acc = %zero) -> (f64) {
          %val = memref.load %arg0[%iv] : memref<64xf64>
          %next = arith.addf %acc, %val : f64
          scf.yield %next : f64
        }
        memref.store %sum, %arg2[%iv] : memref<64xf64>
        arts.yield
      }} {arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = true, tripCount = 64 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "bicg_like_right">}
      arts.yield
    }

    %ret = arith.constant 0 : i32
    return %ret : i32
  }
}
