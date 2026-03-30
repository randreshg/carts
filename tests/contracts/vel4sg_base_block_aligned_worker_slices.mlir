// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/sw4lite/vel4sg-base/vel4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=448 -DNREPS=10 >/dev/null && cat %t.compile/vel4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that vel4sg-base lowers its large 64-thread worker slices in the
// same 7-element owner span chosen for the block DB layout. This keeps the
// stencil workers block-aligned instead of reusing an older 9-element
// coarsening hint that would straddle DB ownership.
// CHECK-LABEL: func.func @main
// CHECK: %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>, <uniform>] {{.*}} elementSizes[%c320, %c320, %c7]
// CHECK: scf.for %[[WORKER:.+]] = %c0 to %c64 step %c1 {
// CHECK: %[[CHUNK_BASE:.+]] = arith.muli %[[WORKER]], %c7 : index
// CHECK: arts.db_acquire[<inout>] {{.*}}partitioning(<block>, offsets[%[[CHUNK_BASE]]], sizes[%c7]), offsets[%{{.+}}], sizes[%c1] {{.*}}stencil_owner_dims = [2]

module {
}
