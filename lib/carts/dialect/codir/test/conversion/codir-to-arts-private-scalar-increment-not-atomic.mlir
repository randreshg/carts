// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,reduction-atomic-materialization,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=arts.atomic_add

module {
  func.func @private_scalar_increment_not_atomic(%out: memref<1xi32>) {
    codir.codelet deps(%out : memref<1xi32>)
        attributes {dep_collectives = [#codir.collective<none>],
                    dep_modes = [#codir.access_mode<write>],
                    dep_owner_dims = [[]],
                    dep_storage_views = [#codir.storage_view<host_whole>]} {
    ^bb0(%dep: memref<1xi32>):
      %c0 = arith.constant 0 : index
      %zero = arith.constant 0 : i32
      %one = arith.constant 1 : i32
      %count = memref.alloca() : memref<i32>
      memref.store %zero, %count[] : memref<i32>
      %old = memref.load %count[] : memref<i32>
      %next = arith.addi %old, %one : i32
      memref.store %next, %count[] : memref<i32>
      %final = memref.load %count[] : memref<i32>
      memref.store %final, %dep[%c0] : memref<1xi32>
      codir.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @private_scalar_increment_not_atomic
// CHECK: memref.alloca() : memref<i32>
// CHECK: memref.store %{{.*}}, %{{.*}}[] : memref<i32>
