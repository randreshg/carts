// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/vel4sg-base/vel4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=448 -DNREPS=10 >/dev/null && cat %t.compile/vel4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that vel4sg-base lowers its worker slices in the 9-element owner span
// chosen for the block DB layout, keeping the stencil workers block-aligned.
// CHECK-LABEL: func.func @main
// CHECK: %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] {{.*}} elementSizes[%c320, %c320, %c7]
// CHECK: arts.lowering_contract(%ptr : memref<?x!llvm.ptr>) block_shape[%c7] contract(<ownerDims = [2], postDbRefined = true>)
// CHECK: scf.for %{{.+}} = %c0 to %c64 step %c1 {
// CHECK: arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%{{.+}}], sizes[%c7]), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}stencil_owner_dims = [2]

module {
}
