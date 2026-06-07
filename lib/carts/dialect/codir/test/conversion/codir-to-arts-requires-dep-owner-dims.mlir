// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @empty_dep_owner_dims_for_compute_block(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    codir.codelet deps(%A : memref<8xf32>) params(%c0 : index)
        attributes {dep_modes = [#codir.access_mode<read>],
                    dep_storage_views = [#codir.storage_view<compute_block>],
                    dep_owner_dims = [[]],
                    dep_collectives = [#codir.collective<none>],
                    distribution_kind = #codir.distribution_kind<blocked>,
                    iteration_topology = #codir.iteration_topology<owner_strip>,
                    logical_worker_slice = [8],
                    tile_owner_dims = [0],
                    tile_shape = [8]} {
    ^bb0(%arg0: memref<8xf32>, %base: index):
      %value = memref.load %arg0[%base] : memref<8xf32>
      codir.yield
    }
    return
  }
}

// CHECK: dependency #0 requires finalized non-empty dep_owner_dims for block/stencil/compute materialization
