// RUN: %carts-compile %S/../inputs/snapshots/atax_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_2t.cfg | %FileCheck %s

// ATAX reuses the read-only matrix `A` under conflicting owner dimensions:
// the first phase is row-owned while the second phase is column-owned. Keep
// the allocation blocked and preserve both owner-dim contracts, while both
// read acquires stay block-partitioned under their respective owner-dimension
// metadata.

// CHECK: %[[A_GUID:[^,]+]], %[[A_PTR:[^ ]+]] = arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>]
// CHECK-DAG: arts.db_acquire[<in>] (%[[A_GUID]] : memref<?xi64>, %[[A_PTR]] : memref<?xmemref<?x?xf64>>) partitioning(<block>{{.*}}stencil_owner_dims = [0]
// CHECK-DAG: arts.db_acquire[<in>] (%[[A_GUID]] : memref<?xi64>, %[[A_PTR]] : memref<?xmemref<?x?xf64>>) partitioning(<block>{{.*}}stencil_owner_dims = [1]
