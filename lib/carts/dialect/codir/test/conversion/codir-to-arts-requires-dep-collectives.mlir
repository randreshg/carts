// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @missing_dep_collectives(%A: memref<8xf32>) {
    codir.codelet deps(%A : memref<8xf32>)
        attributes {dep_modes = [#codir.access_mode<read>],
                    dep_storage_views = [#codir.storage_view<host_whole>],
                    dep_owner_dims = [[]]} {
    ^bb0(%arg0: memref<8xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: requires one dep_collectives entry per dependency before CODIR-to-ARTS materialization
