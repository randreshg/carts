// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @malformed_dep_modes(%A: memref<8xf32>) {
    codir.codelet deps(%A : memref<8xf32>)
        attributes {dep_collectives = [#codir.collective<none>],
                    dep_modes = [#codir.storage_view<host_whole>],
                    dep_owner_dims = [[]],
                    dep_storage_views = [#codir.storage_view<host_whole>]} {
    ^bb0(%arg0: memref<8xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: dep_modes entry #0 must be a CODIR access_mode attribute
