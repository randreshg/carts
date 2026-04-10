// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/atax/atax.c --pipeline db-partitioning --arts-config %S/inputs/arts_2t.cfg -- -I%S/../../external/carts-benchmarks/polybench/atax -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -D_GNU_SOURCE -lm >/dev/null && cat %t.compile/atax.db-partitioning.mlir' | %FileCheck %s

// ATAX reuses the read-only matrix `A` under conflicting owner dimensions:
// the first phase is row-owned while the second phase is column-owned. Keep
// the allocation blocked and preserve both owner-dim contracts, while both
// read acquires widen to coarse full-range views under their respective
// owner-dimension metadata.

// CHECK: %[[A_GUID:[^,]+]], %[[A_PTR:[^ ]+]] = arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>]
// CHECK-DAG: arts.db_acquire[<in>] (%[[A_GUID]] : memref<?xi64>, %[[A_PTR]] : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%{{.*}}], sizes[%{{.*}}] {{.*}}stencil_owner_dims = [0]
// CHECK-DAG: arts.db_acquire[<in>] (%[[A_GUID]] : memref<?xi64>, %[[A_PTR]] : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), offsets[%{{.*}}], sizes[%{{.*}}] {{.*}}stencil_owner_dims = [1]

module {
}
