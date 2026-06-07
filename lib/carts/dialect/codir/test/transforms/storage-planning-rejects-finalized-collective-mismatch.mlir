// RUN: not %carts-compile %s --pass-pipeline='builtin.module(storage-planning)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @finalized_collective_mismatch(%A: memref<8xf32>) {
    codir.codelet deps(%A : memref<8xf32>)
        attributes {dep_collectives = [#codir.collective<all_gather>],
                    dep_modes = [#codir.access_mode<read>],
                    dep_owner_dims = [[]],
                    dep_storage_views = [#codir.storage_view<host_whole>]} {
    ^bb0(%arg0: memref<8xf32>):
      codir.yield
    }
    return
  }
}

// CHECK: existing dep_collectives does not match recomputed CODIR storage plan
