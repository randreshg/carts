// RUN: %carts-compile %S/../../../inputs/snapshots/rhs4sg_base_default_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../../../../examples/arts.cfg | %FileCheck %s

// Canonical owner-dim rhs4sg contracts now keep the read-only input coarse at
// allocation time while preserving the downstream block-distribution contract
// on the outlined task slice.
// CHECK-LABEL: func.func @main
// CHECK: %[[UGUID:.*]], %[[UPTR:.*]] = arts.db_alloc[{{.*}}<coarse>, <stencil>{{.*}}] route(%c-1_i32 : i32) sizes[%c1]
// CHECK: %[[UACQ_GUID:.*]], %[[UACQ_PTR:.*]] = arts.db_acquire[<in>] (%[[UGUID]] : memref<?xi64>, %[[UPTR]] : memref<?xmemref<?x?x?x?xf32>>) partitioning(<coarse>), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}distribution_pattern = #arts.distribution_pattern<uniform>{{.*}}stencil_owner_dims = [3]
// CHECK: arts.lowering_contract(%[[UACQ_PTR]] : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>
// CHECK-SAME: contract(<ownerDims = [3], postDbRefined = true>
