// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s
// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile.llvm %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline arts-to-llvm --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile.llvm/rhs4sg_base.arts-to-llvm.mlir' | %FileCheck %s --check-prefix=LLVM

// Verify that rhs4sg-base aligns worker chunking to the 9-element DB ownership
// grid on k, keeps the writer on the owner-local block slice, and stamps the
// read-only U acquire with the radius-2 stencil halo contract that later
// lowering uses for widened block-local transport. The physical block is still
// 4-D, so the U halo face is strided in memory and must stay on the
// block-indexed dependence path instead of becoming a byte slice.
// The outlined task still clamps the aligned block slice back to the real
// interior domain [4, 572), so correctness is preserved while ownership stays
// block-aligned.
// CHECK-LABEL: func.func private @__arts_edt_{{[0-9]+}}
// CHECK: scf.for %{{.+}} = %c0 to %c64 step %c1 {
// CHECK: %[[CHUNK_BASE:.+]] = arith.muli %{{.+}}, %c9 : index
// CHECK: %[[CHUNK_ELEMS:.+]] = arith.minui %{{.+}}, %c9 : index

// CHECK-LABEL: func.func @main
// CHECK: %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>] {{.*}} elementSizes[%c3, %c320, %c320, %{{.+}}]
// CHECK: arts.lowering_contract(%ptr : memref<?x!llvm.ptr>) block_shape[%{{.+}}] contract(<ownerDims = [3], postDbRefined = true>)
// CHECK: %[[WRITE_GUID:.+]], %[[WRITE_PTR:.+]] = arts.db_alloc[<inout>, <heap>, <write>, <block>] {{.*}} elementSizes[%c3, %c320, %c320, %c9]
// CHECK: arts.lowering_contract(%[[WRITE_PTR]] : memref<?x!llvm.ptr>) block_shape[%c9] contract(<ownerDims = [3], postDbRefined = true>)
// CHECK: %{{.+}} = arts.create_epoch : i64
// CHECK: scf.for %[[WORKER:.+]] = %c0 to %c64 step %c1 {
// CHECK: %[[WORK_BASE:.+]] = arith.muli %[[WORKER]], %c9 : index
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}distribution_pattern = #arts.distribution_pattern<stencil>{{.*}}stencil_owner_dims = [3]
// CHECK: arts.lowering_contract({{.*}}) pattern(<{{.*}}distributionKind = <block>, distributionPattern = <stencil>{{.*}}) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}stencil_owner_dims = [3]
// CHECK: arts.lowering_contract({{.*}}) pattern(<{{.*}}distributionKind = <block>, distributionPattern = <stencil>{{.*}}) block_shape[%c3] contract(<ownerDims = [3], postDbRefined = true, criticalPathDistance = 0 : i64>)

// LLVM: func.call @arts_add_dependence(

module {
}
