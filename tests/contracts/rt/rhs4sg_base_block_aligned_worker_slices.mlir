// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s
// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --pipeline arts-to-llvm --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s --check-prefix=LLVM

// Verify that rhs4sg-base aligns worker chunking to the 9-element DB ownership
// grid on k, keeps the writer on the owner-local block slice, and preserves
// the worker-local owner metadata even though the backing read DB and read
// acquires remain coarse until outlining.
// The outlined task still clamps the aligned block slice back to the real
// interior domain [4, 572), so correctness is preserved while ownership stays
// block-aligned.
// CHECK-LABEL: func.func @main
// CHECK: %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>] {{.*}} elementSizes[%c3, %c320, %c320, %{{.+}}]
// CHECK: %[[WRITE_GUID:.+]], %[[WRITE_PTR:.+]] = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] {{.*}} elementSizes[%c3, %c320, %c320, %c576]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<coarse>), offsets[%c0], sizes[%c1] {{.*}}distribution_pattern = #arts.distribution_pattern<uniform>{{.*}}stencil_owner_dims = [3]
// CHECK: arts.lowering_contract({{.*}}) pattern(<{{.*}}distributionKind = <block>, distributionPattern = <uniform>{{.*}}) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)
// CHECK: %{{.+}} = arts_rt.create_epoch : i64
// CHECK: scf.for %[[WORKER:.+]] = %c0 to %c64 step %c1 {
// CHECK: %[[WORK_BASE:.+]] = arith.muli %[[WORKER]], %c9 : index
// CHECK: %[[CHUNK_ELEMS:.+]] = arith.minui %{{.+}}, %c9 : index
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}stencil_owner_dims = [3]
// CHECK: arts.lowering_contract({{.*}}) pattern(<{{.*}}distributionKind = <block>, distributionPattern = <uniform>{{.*}}) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)

// LLVM: func.call @arts_add_dependence(
