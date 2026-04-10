// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that Seidel keeps the full 2-D stencil tile on the semantic attrs
// while lowering the worker-local acquire to the 1-D row-owned block contract
// used for late halo-slice inference instead of the matmul-oriented tiling_2d
// lane-slicing path.
// CHECK-LABEL: func.func @main
// CHECK-NOT: distribution_kind = #arts.distribution_kind<tiling_2d>
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>]
// CHECK: arts_rt.create_epoch
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>)
// CHECK-SAME: depPattern = #arts.dep_pattern<wavefront_2d>
// CHECK-SAME: distribution_kind = #arts.distribution_kind<block>
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-SAME: stencil_block_shape = [150, 960]
// CHECK-SAME: stencil_owner_dims = [0]
// CHECK-SAME: stencil_spatial_dims = [0, 1]
// CHECK-NEXT: arts.lowering_contract
// CHECK-SAME: depPattern = <wavefront_2d>
// CHECK-SAME: distributionKind = <block>
// CHECK-SAME: distributionPattern = <stencil>
// CHECK-SAME: block_shape[%c150]
// CHECK-SAME: contract(<ownerDims = [0], centerOffset = 1
// CHECK-SAME: spatialDims = [0, 1]
// CHECK-SAME: supportedBlockHalo = true
// CHECK: arts_rt.wait_on_epoch
