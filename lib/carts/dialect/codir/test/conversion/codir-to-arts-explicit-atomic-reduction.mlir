// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s

module {
  func.func @explicit_atomic_reduction(%out: memref<1xi32>) {
    codir.codelet deps(%out : memref<1xi32>)
        attributes {dep_collectives = [#codir.collective<none>],
                    dep_modes = [#codir.access_mode<readwrite>],
                    dep_owner_dims = [[]],
                    dep_storage_views = [#codir.storage_view<host_whole>],
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

// CHECK-LABEL: func.func @explicit_atomic_reduction
// CHECK: arts.atomic_add(%{{.*}}, %{{.*}} : memref<?xi32>, i32)
