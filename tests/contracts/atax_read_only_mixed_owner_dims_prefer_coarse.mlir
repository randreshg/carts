// RUN: sh -c 'mkdir -p %t.compile && CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../atax.mlir --pipeline concurrency-opt --arts-config %S/inputs/arts_2t.cfg -o %t.compile/atax.concurrency-opt.mlir >/dev/null && cat %t.compile/atax.concurrency-opt.mlir' | %FileCheck %s

// ATAX reuses the read-only matrix `A` under conflicting owner dimensions:
// the first phase is row-owned while the second phase is column-owned. On a
// single node, a 1-D blocked layout cannot satisfy both contracts, so the
// shared read-only input should stay coarse while the produced vectors remain
// blocked.

// CHECK: %[[A_GUID:[^,]+]], %[[A_PTR:[^ ]+]] = arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// CHECK-DAG: arts.db_acquire[<in>] (%[[A_GUID]] : memref<?xi64>, %[[A_PTR]] : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {{.*}}stencil_owner_dims = [0]
// CHECK-DAG: arts.db_acquire[<in>] (%[[A_GUID]] : memref<?xi64>, %[[A_PTR]] : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%c0], sizes[%c1] {{.*}}stencil_owner_dims = [1]

module {
}
