// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.pre-lowering.mlir' | %FileCheck %s

// Verify that Seidel keeps the full 2-D stencil tile on the semantic attrs
// while lowering the worker-local acquire to the 1-D row-owned block contract
// used for late halo-slice inference instead of the matmul-oriented tiling_2d
// lane-slicing path.
// CHECK-LABEL: func.func @main
// CHECK-NOT: distribution_kind = #arts.distribution_kind<tiling_2d>
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <stencil>] {{.*}} elementType(f64) elementSizes[%{{.+}}, %{{.+}}]
// CHECK: %[[EPOCH:.+]] = arts_rt.create_epoch : i64
// CHECK: scf.for %[[WAVE:.+]] = %c0 to %{{.+}} step %c1 {
// CHECK: %[[ACTIVE_ROWS:.+]] = arith.minui %[[NUM_ROWS:.+]], %c64 : index
// CHECK: %[[DISPATCH:.+]] = arith.maxui %{{.+}}, %c1 : index
// CHECK: scf.for %[[WORKER:.+]] = %c0 to %[[DISPATCH]] step %c1 {
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>), offsets[%{{.+}}], sizes[%{{.+}}] {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>, distribution_version = 1 : i32, stencil_block_shape = [150, 960], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]}
// CHECK-NEXT: arts.lowering_contract(%{{.+}} : memref<?x!llvm.ptr>) pattern(<depPattern = <wavefront_2d>, distributionKind = <block>, distributionPattern = <stencil>, distributionVersion = 1 : i64, revision = 1 : i64>) block_shape[%c150] min_offsets[%c-1] max_offsets[%c1] write_footprint[%c0] contract(<ownerDims = [0], centerOffset = 1 : i64, spatialDims = [0, 1], supportedBlockHalo = true{{.*}})
// CHECK: arts_rt.wait_on_epoch %[[EPOCH]] : i64

module {
}
