// RUN: %carts-compile %s --pass-pipeline='builtin.module(storage-planning,verify-codir)' \
// RUN:   | %FileCheck %s

// CODIR joins SDE per-array layout evidence to dependencies by `dep_array_ids`,
// not by dependency slot. This keeps the committed SDE layout fact stable when
// dependency collection order differs from array-layout order.

module {
  func.func @layout_order_uses_dep_array_ids(%A: memref<128xf32>, %B: memref<128xf32>) {
    codir.codelet deps(%A, %B : memref<128xf32>, memref<128xf32>)
        attributes {array_layout = [{arrayId = 42 : i64,
                                     blockShape = [16],
                                     commVolumeBytes = 4096 : i64,
                                     kind = "block_contraction",
                                     muBlockCount = 8 : i64,
                                     ownerDims = [0],
                                     role = "write"},
                                    {arrayId = 7 : i64,
                                     blockShape = [16],
                                     commVolumeBytes = 0 : i64,
                                     kind = "block_parallel",
                                     muBlockCount = 8 : i64,
                                     ownerDims = [0],
                                     role = "write"}],
                    dep_array_ids = [7, 42],
                    dep_modes = [#codir.access_mode<write>, #codir.access_mode<write>],
                    dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                    layouts_disagree = [42],
                    pattern = #codir.pattern<elementwise_pipeline>} {
    ^bb0(%a: memref<128xf32>, %b: memref<128xf32>):
      codir.yield
    }
    return
  }

  func.func @partition_graph_filters_by_dep_array_id(%A: memref<128xf32>, %B: memref<128xf32>) {
    codir.codelet deps(%A, %B : memref<128xf32>, memref<128xf32>)
        attributes {array_layout = [{arrayId = 7 : i64,
                                     blockShape = [16],
                                     commVolumeBytes = 0 : i64,
                                     kind = "block_parallel",
                                     muBlockCount = 8 : i64,
                                     ownerDims = [0],
                                     role = "write"},
                                    {arrayId = 42 : i64,
                                     blockShape = [16],
                                     commVolumeBytes = 0 : i64,
                                     kind = "block_parallel",
                                     muBlockCount = 8 : i64,
                                     ownerDims = [0],
                                     role = "write"}],
                    dep_array_ids = [7, 42],
                    dep_modes = [#codir.access_mode<write>, #codir.access_mode<write>],
                    dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                    partition_graph = [{cuGroupCount = 2 : i64,
                                        cuGroupSize = 4 : i64,
                                        edgeClass = "layout_mismatch",
                                        edgeCommBytes = 4096 : i64,
                                        layoutKind = "block_contraction",
                                        muId = 42 : i64,
                                        muBlockCount = 8 : i64,
                                        role = "write"}],
                    pattern = #codir.pattern<elementwise_pipeline>} {
    ^bb0(%a: memref<128xf32>, %b: memref<128xf32>):
      codir.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @layout_order_uses_dep_array_ids
// CHECK: codir.codelet
// CHECK-SAME: dep_array_ids = [7, 42]
// CHECK-SAME: dep_collectives = [#codir.collective<none>, #codir.collective<all_gather>]

// CHECK-LABEL: func.func @partition_graph_filters_by_dep_array_id
// CHECK: codir.codelet
// CHECK-SAME: dep_array_ids = [7, 42]
// CHECK-SAME: dep_collectives = [#codir.collective<none>, #codir.collective<all_gather>]
