// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../external/carts-benchmarks/specfem3d/stress/stress_update.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/stress_update.db-partitioning.mlir' | %FileCheck %s

// Verify the block fallback keeps the owner slice for inout acquires while
// widening the read-only stencil acquire window by the halo width.
// CHECK-LABEL: func.func @main
// CHECK: %[[DIRECT_GUID:.*]], %[[DIRECT_PTR:.*]] = arts.db_acquire[<in>]{{.*}}partitioning(<block>, offsets[%[[PART_BASE:.*]]], sizes[%[[PART_ELEMS:.*]]]), offsets[%[[OWNER_OFF:.*]]], sizes[%[[OWNER_SZ:.*]]] element_offsets[%c0, %c0, %[[PART_BASE]]] element_sizes[%c40, %c40, %[[PART_ELEMS]]]{{.*}}
// CHECK: %[[HALO_GUID:.*]], %[[HALO_PTR:.*]] = arts.db_acquire[<in>]{{.*}}partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}], sizes[%{{.+}}] element_offsets[%c0, %c0, %{{.+}}] element_sizes[%c40, %c40, %{{.+}}] {{.*}}stencil_center_offset = 1 : i64, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [2]
// CHECK: %[[WRITE_GUID:.*]], %[[WRITE_PTR:.*]] = arts.db_acquire[<inout>]{{.*}}partitioning(<block>, offsets[%[[PART_BASE]]], sizes[%[[PART_ELEMS]]]), offsets[%[[OWNER_OFF]]], sizes[%[[OWNER_SZ]]]{{.*}}
