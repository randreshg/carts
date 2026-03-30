// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s
// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile.llvm %S/../../tools/carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline arts-to-llvm --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile.llvm/rhs4sg_base.arts-to-llvm.mlir' | %FileCheck %s --check-prefix=LLVM

// Verify that rhs4sg-base aligns worker chunking to the 9-element DB ownership
// grid on k, keeps the writer on the owner-local block slice, and stamps the
// read-only U acquire with the radius-2 stencil halo contract that later
// lowering uses for widened block-local transport. The physical block is still
// 4-D, so the U halo face is strided in memory and must stay on the whole-DB
// dependence path instead of becoming a byte slice.
// The outlined task still clamps the aligned block slice back to the real
// interior domain [4, 572), so correctness is preserved while ownership stays
// block-aligned.
// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %[[LOWER_GAP:.+]] = arith.subi %c4, %0#0 : index
// CHECK: %[[LOWER_CLAMP:.+]] = arith.select {{.+}}, %c0, %[[LOWER_GAP]] : index
// CHECK: scf.for %{{.+}} = %[[LOWER_CLAMP]] to %{{.+}} step %c1 {

// CHECK-LABEL: func.func @main
// CHECK: %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>, <stencil>] {{.*}} elementSizes[%c3, %c320, %c320, %{{.+}}]
// CHECK: arts.lowering_contract(%ptr : memref<?xmemref<?x?x?x?xf32>>) block_shape[%{{.+}}] {owner_dims = array<i64: 3>, post_db_refined}
// CHECK: %{{.+}} = arts.create_epoch : i64
// CHECK: scf.for %[[WORKER:.+]] = %c0 to %c64 step %c1 {
// CHECK: %[[CHUNK_BASE:.+]] = arith.muli %[[WORKER]], %c9 : index
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>, offsets[%{{.+}}], sizes[%{{.+}}]), offsets[%{{.+}}], sizes[%{{.+}}] element_offsets[%c0, %c0, %c0, %{{.+}}] element_sizes[%c3, %c320, %c320, %{{.+}}] {{.*}}distribution_pattern = #arts.distribution_pattern<stencil>{{.*}}stencil_center_offset = 2 : i64, stencil_max_offsets = [2], stencil_min_offsets = [-2], stencil_owner_dims = [3]
// CHECK: arts.lowering_contract({{.*}}) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%{{.+}}] min_offsets[%c-2] max_offsets[%c2] {{.*}}owner_dims = array<i64: 3>
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>, offsets[%[[CHUNK_BASE]]], sizes[%c9]), offsets[%{{.+}}], sizes[%c1] {{.*}}stencil_owner_dims = [3]

// LLVM-NOT: func.call @arts_add_dependence_at(

module {
}
