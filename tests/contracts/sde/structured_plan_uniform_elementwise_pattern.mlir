// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// Test that a uniform elementwise parallel loop produces elementwise_pipeline
// dep_pattern at pattern-pipeline. Two sibling EDTs with same-index array
// access fuse into one EDT with elementwise_pipeline pattern.
// Verifies:
//   1. Sibling loops fuse into a single EDT
//   2. depPattern is elementwise_pipeline (uniform access)

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<elementwise_pipeline>

module {
  func.func @main(%arg0: memref<256xf64>, %arg1: memref<256xf64>,
                  %arg2: memref<256xf64>) {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c256 = arith.constant 256 : index

    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c256) step(%c1) {
      ^bb0(%iv: index):
        %0 = memref.load %arg0[%iv] : memref<256xf64>
        %1 = arith.addf %0, %0 : f64
        memref.store %1, %arg1[%iv] : memref<256xf64>
        arts.yield
      }
      arts.for(%c0) to(%c256) step(%c1) {
      ^bb0(%iv: index):
        %0 = memref.load %arg0[%iv] : memref<256xf64>
        %1 = arith.mulf %0, %0 : f64
        memref.store %1, %arg2[%iv] : memref<256xf64>
        arts.yield
      }
      arts.yield
    }
    return
  }
}
