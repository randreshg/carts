// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at loop-reordering | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.edt <parallel>
// CHECK-COUNT-2: arts.for(%c0) to(%c127) step(%c1)
// CHECK-NOT: depPattern = #arts.dep_pattern<elementwise_pipeline>

module {
  func.func @main(%arg0: memref<128xf32>, %arg1: memref<128xf32>,
                  %arg2: memref<128xf32>) {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c127 = arith.constant 127 : index
    arts.edt <parallel> <internode> route(%c0_i32) {
      arts.for(%c0) to(%c127) step(%c1) {{
      ^bb0(%iv: index):
        %0 = memref.load %arg0[%iv] : memref<128xf32>
        memref.store %0, %arg1[%iv] : memref<128xf32>
        arts.yield
      }}
      arts.for(%c0) to(%c127) step(%c1) {{
      ^bb0(%iv: index):
        %0 = arith.addi %iv, %c1 : index
        %1 = memref.load %arg0[%0] : memref<128xf32>
        memref.store %1, %arg2[%iv] : memref<128xf32>
        arts.yield
      }}
      arts.yield
    }
    return
  }
}
