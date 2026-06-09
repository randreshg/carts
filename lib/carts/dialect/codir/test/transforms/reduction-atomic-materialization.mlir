// RUN: %carts-compile %s --pass-pipeline='builtin.module(reduction-atomic-materialization)' \
// RUN:   | %FileCheck %s --implicit-check-not=memref.store

module {
  func.func @atomic_reduction_shape(%out: memref<1xi32>) {
    codir.codelet deps(%out : memref<1xi32>)
        attributes {dep_modes = [#codir.access_mode<readwrite>],
                    reduction_strategy = #codir.reduction_strategy<atomic>} {
    ^bb0(%dep: memref<1xi32>):
      %c0 = arith.constant 0 : index
      %one = arith.constant 1 : i32
      %old = memref.load %dep[%c0] : memref<1xi32>
      %next = arith.addi %old, %one : i32
      memref.store %next, %dep[%c0] : memref<1xi32>
      codir.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @atomic_reduction_shape
// CHECK: codir.atomic_add(%{{.*}}, %{{.*}} : memref<1xi32>, i32)
