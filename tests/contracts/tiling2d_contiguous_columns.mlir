// RUN: %carts-compile %S/../../external/carts-benchmarks/polybench/gemm/gemm.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.epoch attributes {distribution_kind = #arts.distribution_kind<tiling_2d>, distribution_pattern = #arts.distribution_pattern<matmul>
// CHECK: arts.edt <task> <internode>{{.*}}distribution_kind = #arts.distribution_kind<tiling_2d>
// CHECK: scf.for %[[ROW:.*]] = %{{.*}} to %{{.*}} step %c1 {
// CHECK: scf.for %[[TILE:.*]] = %c0 to %c9600 step %c64 {
// CHECK: %[[TILE_END:.*]] = arith.addi %[[TILE]], %c64 : index
// CHECK: %[[TILE_CLAMP:.*]] = arith.minui %[[TILE_END]], %c9600 : index
// CHECK: %[[LANE_CLAMP:.*]] = arith.minui %{{.*}}, %c9600 : index
// CHECK: %[[SLICE_LOW:.*]] = arith.maxui %[[TILE]], %{{.*}} : index
// CHECK: %[[SLICE_HIGH:.*]] = arith.minui %[[TILE_CLAMP]], %[[LANE_CLAMP]] : index
// CHECK: scf.for %[[COL:.*]] = %[[SLICE_LOW]] to %[[SLICE_HIGH]] step %c1 {

module {
  // RUN/FileCheck contract only.
}
