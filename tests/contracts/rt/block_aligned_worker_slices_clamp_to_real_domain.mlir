// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s
// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --pipeline arts-to-llvm --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s --check-prefix=LLVM

// Verify that rhs4sg-base allocates DBs as coarse and distributes the stencil
// kernel with block-partitioned acquires preserving owner dims.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <coarse>] {{.*}} elementSizes[%c3, %c320, %c320, %c576]
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>] {{.*}} elementSizes[%c3, %c320, %c320, %c576]
// CHECK: arts_rt.create_epoch
// CHECK: scf.for %{{.+}} = %c0 to %c64 step %c1 {
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract
// CHECK-SAME: distributionPattern = <stencil>
// CHECK-SAME: contract(<ownerDims = [2], postDbRefined = true

// LLVM: func.call @arts_add_dependence(
