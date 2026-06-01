// RUN: not %carts-compile %s --pass-pipeline='builtin.module(storage-planning)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @finalized_storage_view_mismatch(%A: memref<8xf32>) {
    codir.codelet deps(%A : memref<8xf32>)
        attributes {dep_collectives = [#codir.collective<none>],
                    dep_modes = [#codir.access_mode<read>],
                    dep_owner_dims = [[0]],
                    dep_storage_views = [#codir.storage_view<compute_block>]} {
    ^bb0(%arg0: memref<8xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: existing dep_storage_views does not match recomputed CODIR storage plan
