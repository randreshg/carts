// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline db-partitioning --arts-config %S/../../../examples/arts.cfg >/dev/null && cat %t.compile/rhs4sg_base.db-partitioning.mlir' | %FileCheck %s

// Canonical owner-dim rhs4sg contracts now keep the read-only input coarse at
// allocation time while preserving worker-local blocked acquires on the
// outlined task slices.
// CHECK-LABEL: func.func @main
// CHECK: %[[UGUID:.*]], %[[UPTR:.*]] = arts.db_alloc[{{.*}}<coarse>, <stencil>{{.*}}] route(%c-1_i32 : i32) sizes[%c1]
// CHECK: %[[UACQ_GUID:.*]], %[[UACQ_PTR:.*]] = arts.db_acquire[<in>] (%[[UGUID]] : memref<?xi64>, %[[UPTR]] : memref<?xmemref<?x?x?x?xf32>>) partitioning(<block>, offsets[
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<uniform>
// CHECK-SAME: stencil_owner_dims = [3]
// CHECK: arts.lowering_contract(%[[UACQ_PTR]] : memref<?xmemref<?x?x?x?xf32>>) pattern(<depPattern = <uniform>, distributionKind = <block>, distributionPattern = <uniform>
// CHECK-SAME: contract(<ownerDims = [3], postDbRefined = true>
