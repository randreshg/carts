// RUN: %carts-compile %S/../inputs/snapshots/specfem3d_velocity_288x_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Velocity worker dependencies carry block-partitioned acquires through
// pre-lowering with the owner-dimension contract, and rec_dep lowers without
// byte-slice transport.
// CHECK: arts.db_acquire[<in>] ({{.*}}) partitioning(<block>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract({{.*}}contract(<ownerDims = [2], postDbRefined = true
// CHECK: arts_rt.rec_dep
// CHECK-NOT: byte_offsets(
// CHECK-NOT: byte_sizes(
