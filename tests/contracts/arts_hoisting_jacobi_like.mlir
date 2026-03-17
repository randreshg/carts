// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline concurrency-opt | %FileCheck %s

// Verify that for a jacobi-like 2D stencil pattern:
// 1. An EDT task is created
// 2. Block-ref operations (arts.db_ref) are hoisted to the outer row loop
// 3. The inner column loop contains only loads and stores (no db_ref / block ops)

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <task>
// CHECK: ^bb0(%arg1: memref<?xmemref<?x?xf32>>, %arg2: memref<?xmemref<?x?xf32>>):
// CHECK: scf.for %[[ROW_OUTER:.*]] = %{{.*}} to %{{.*}} step %c1 {
// CHECK: %[[ROW:.*]] = arith.addi %{{.*}}, %[[ROW_OUTER]] : index
// CHECK: arts.db_ref %arg1[%{{.*}}] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
// CHECK: arts.db_ref %arg1[%{{.*}}] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
// CHECK: arts.db_ref %arg1[%{{.*}}] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
// CHECK: arts.db_ref %arg2[%{{.*}}] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
// CHECK: scf.for %[[COL:.*]] = %{{.*}} to %{{.*}} step %c1 {
// CHECK-NOT: arts.db_ref
// CHECK: memref.load {{.*}} : memref<?x?xf32>
// CHECK: memref.load {{.*}} : memref<?x?xf32>
// CHECK: memref.load {{.*}} : memref<?x?xf32>
// CHECK: memref.store {{.*}} : memref<?x?xf32>

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @main() -> f32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c15 = arith.constant 15 : index
    %A = memref.alloc() : memref<16x16xf32>
    %B = memref.alloc() : memref<16x16xf32>
    omp.parallel {
      omp.wsloop for (%i) : index = (%c1) to (%c15) step (%c1) {
        scf.for %j = %c1 to %c15 step %c1 {
          %im1 = arith.subi %i, %c1 : index
          %ip1 = arith.addi %i, %c1 : index
          %up = memref.load %A[%im1, %j] : memref<16x16xf32>
          %mid = memref.load %A[%i, %j] : memref<16x16xf32>
          %down = memref.load %A[%ip1, %j] : memref<16x16xf32>
          %sum = arith.addf %up, %mid : f32
          %out = arith.addf %sum, %down : f32
          memref.store %out, %B[%i, %j] : memref<16x16xf32>
        }
        omp.yield
      }
      omp.terminator
    }
    %result = memref.load %B[%c1, %c1] : memref<16x16xf32>
    memref.dealloc %A : memref<16x16xf32>
    memref.dealloc %B : memref<16x16xf32>
    return %result : f32
  }
}
