// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_multinode.cfg | %FileCheck %s

// Verify that two-level pre-lowering preserves Seidel's owner-space
// dependency shape. The owner dimension is linearized to a 1-D index space
// after dep_gep lowering, so both dep_gep and db_acquire use rank-1 offsets.
// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%{{.+}} : index] strides[%c1]
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}distribution_kind = #arts.distribution_kind<two_level>