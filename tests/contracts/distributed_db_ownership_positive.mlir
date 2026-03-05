// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --distributed-db --stop-at concurrency-opt --debug-only=distributed_db_ownership 2>&1 | %FileCheck %s

// CHECK: DistributedDbOwnership marked 2 / {{[1-9][0-9]*}} DbAlloc operations
// CHECK-DAG: arts.db_alloc{{.*}}{arts.id = 189 : i64, distributed}
// CHECK-DAG: arts.db_alloc{{.*<coarse>, <uniform>.*arts.id = 190 : i64.*}}
// CHECK-DAG: arts.db_alloc{{.*}}{arts.id = 191 : i64, distributed}
// CHECK-DAG: arts.epoch attributes {distribution_kind = #arts.distribution_kind<tiling_2d>, distribution_pattern = #arts.distribution_pattern<matmul>
// CHECK-DAG: %[[MATMUL_IN1_GUID:.*]], %[[MATMUL_IN1_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?xf32>>) partitioning(<coarse>)
// CHECK-DAG: %[[MATMUL_OUT_GUID:.*]], %[[MATMUL_OUT_PTR:.*]] = arts.db_acquire[<inout>] (%[[OUT_GUID:.*]] : memref<?xi64>, %[[OUT_PTR:.*]] : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[
// CHECK-DAG: ], sizes[
// CHECK-DAG: -> (memref<?x?xi64>, memref<?xmemref<?x?xf32>>)
// CHECK-DAG: %[[MATMUL_IN0_GUID:.*]], %[[MATMUL_IN0_PTR:.*]] = arts.db_acquire[<in>] (%{{.*}} : memref<?xi64>, %{{.*}} : memref<?xmemref<?x?xf32>>) partitioning(<block>, offsets[
// CHECK-DAG: ], sizes[
// CHECK-DAG: -> (memref<?xi64>, memref<?xmemref<?x?xf32>>)

module {
  // RUN/FileCheck contract only.
}
