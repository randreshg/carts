// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @missing_dep_storage_views(%A: memref<8xf32>) {
    codir.codelet deps(%A : memref<8xf32>)
        attributes {dep_modes = [#codir.access_mode<read>],
                    dep_owner_dims = [[]],
                    dep_collectives = [#codir.collective<none>]} {
    ^bb0(%arg0: memref<8xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: requires one dep_storage_views entry per dependency before CODIR-to-ARTS materialization
