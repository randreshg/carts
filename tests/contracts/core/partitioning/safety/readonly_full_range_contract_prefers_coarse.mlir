// RUN: %carts-compile %S/../../../inputs/snapshots/rhs4sg_base_default_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../inputs/arts_64t.cfg | %FileCheck %s

// Canonical owner-dim rhs4sg contracts keep the read-only input coarse at
// allocation time while preserving the downstream block-distribution contract
// on the outlined task slice.
// CHECK-LABEL: func.func @main
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <stencil>] route(%c-1_i32 : i32) sizes[%c1]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-SAME: stencil_owner_dims = [2]
// CHECK: arts.lowering_contract
// CHECK-SAME: distributionPattern = <stencil>
// CHECK-SAME: contract(<ownerDims = [2], postDbRefined = true>
