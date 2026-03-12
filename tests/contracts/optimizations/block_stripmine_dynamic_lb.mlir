// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: %[[LB:.*]] = arith.muli %arg0, %c16 : index
// CHECK: %[[OUTER:.*]] = scf.for %{{.*}} = %c0 to %{{.*}} step %c1
// CHECK: %[[BLOCK_START:.*]] = arith.addi %[[LB]], %{{.*}} : index
// CHECK: %[[BLOCK_REF:.*]] = arts.db_ref %{{.*}}[%{{.*}}] : memref<?xmemref<?xf32>> -> memref<?xf32>
// CHECK: %[[INNER:.*]] = scf.for %[[INNER_IV:.*]] = %c0 to %{{.*}} step %c1
// CHECK-NOT: arith.divui
// CHECK-NOT: arith.remui
// CHECK-NOT: arts.db_ref
// CHECK: memref.load %[[BLOCK_REF]][%[[INNER_IV]]] : memref<?xf32>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main(%arg0: index, %arg1: index) -> f32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c0_i32 = arith.constant 0 : i32
    %cst_0 = arith.constant 0.000000e+00 : f32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] route(%c0_i32 : i32) sizes[%c4] elementType(f32) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %lb = arith.muli %arg0, %c16 : index
    %sum = scf.for %i = %lb to %arg1 step %c1 iter_args(%acc = %cst_0) -> (f32) {
      %blk = arith.divui %i, %c16 : index
      %off = arith.remui %i, %c16 : index
      %ref = arts.db_ref %ptr[%blk] : memref<?xmemref<?xf32>> -> memref<?xf32>
      %val = memref.load %ref[%off] : memref<?xf32>
      %next = arith.addf %acc, %val : f32
      scf.yield %next : f32
    }
    return %sum : f32
  }
}
