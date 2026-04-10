// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/vel4sg-base/vel4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=448 -DNREPS=10 >/dev/null && cat %t.compile/vel4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that vel4sg-base lowers its worker slices in the owner-local 7-element
// span used by worker lowering, keeping the blocked worker acquires aligned
// even though the backing DB allocation now remains coarse.
// CHECK-LABEL: func.func @main
// CHECK: %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] {{.*}} elementSizes[%c320, %c320, %c448]
// CHECK: scf.for %{{.+}} = %c0 to %c64 step %c1 {
// CHECK: arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?x!llvm.ptr>) partitioning(<block>, offsets[%{{.+}}], sizes[%c7]), offsets[%{{.+}}], sizes[%{{.+}}] {{.*}}stencil_owner_dims = [2]

module {
}
