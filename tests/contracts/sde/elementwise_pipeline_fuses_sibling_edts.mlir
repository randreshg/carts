// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// Two sibling single-loop EDTs should collapse all the way to one EDT with one
// fused loop in the pattern pipeline.
//
// CHECK-LABEL: func.func @main
// CHECK-COUNT-1: arts.edt <parallel>
// CHECK-COUNT-1: arts.for(%c0) to(%c128) step(%c1)
// CHECK: memref.store %{{.*}}, %arg1[%[[IV:.*]]] : memref<128xf32>
// CHECK: memref.store %{{.*}}, %arg2[%[[IV]]] : memref<128xf32>
// CHECK: depPattern = #arts.dep_pattern<elementwise_pipeline>

module {
  func.func @main(%arg0: memref<128xf32>, %arg1: memref<128xf32>,
                  %arg2: memref<128xf32>) {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index

    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c128) step(%c1) {{
      ^bb0(%iv: index):
        %0 = memref.load %arg0[%iv] : memref<128xf32>
        %1 = arith.addf %0, %0 : f32
        memref.store %1, %arg1[%iv] : memref<128xf32>
        arts.yield
      }}
      arts.yield
    }

    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c128) step(%c1) {{
      ^bb0(%iv: index):
        %0 = memref.load %arg0[%iv] : memref<128xf32>
        %1 = arith.negf %0 : f32
        memref.store %1, %arg2[%iv] : memref<128xf32>
        arts.yield
      }}
      arts.yield
    }
    return
  }
}
