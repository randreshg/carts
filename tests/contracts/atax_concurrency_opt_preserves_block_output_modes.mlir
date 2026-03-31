// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../atax.mlir --pipeline concurrency-opt --arts-config %S/inputs/arts_2t.cfg -o %t.compile/atax.concurrency-opt.mlir >/dev/null && cat %t.compile/atax.concurrency-opt.mlir' | %FileCheck %s

// The blocked `tmp` and `y` outputs initialize a worker-local slice and then
// reload only values produced earlier in the same EDT. Keep both acquires in
// `<out>` mode and preserve the single-block owned slice instead of widening
// them back to `<inout>`.

// CHECK: %[[OUT0_GUID:[^,]+]], %[[OUT0_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <block>
// CHECK: %[[OUT1_GUID:[^,]+]], %[[OUT1_PTR:[^ ]+]] = arts.db_alloc[<inout>, <heap>, <write>, <block>
// CHECK-DAG: arts.db_acquire[<out>] (%[[OUT0_GUID]] : memref<?xi64>, %[[OUT0_PTR]] : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[{{.*}}], sizes[{{.*}}]), offsets[{{.*}}], sizes[%c1] {{.*}}stencil_owner_dims = [0]
// CHECK-DAG: arts.db_acquire[<out>] (%[[OUT1_GUID]] : memref<?xi64>, %[[OUT1_PTR]] : memref<?xmemref<?xf64>>) partitioning(<block>, offsets[{{.*}}], sizes[{{.*}}]), offsets[{{.*}}], sizes[%c1] {{.*}}stencil_owner_dims = [0]

module {
}
