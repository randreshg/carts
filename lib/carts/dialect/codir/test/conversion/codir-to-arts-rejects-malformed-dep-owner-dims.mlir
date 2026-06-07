// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @malformed_dep_owner_dims(%A: memref<8xf32>) {
    codir.codelet deps(%A : memref<8xf32>)
        attributes {dep_collectives = [#codir.collective<none>],
                    dep_modes = [#codir.access_mode<read>],
                    dep_owner_dims = [#codir.collective<none>],
                    dep_storage_views = [#codir.storage_view<host_whole>]} {
    ^bb0(%arg0: memref<8xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: dep_owner_dims entry #0 must be an array attribute
