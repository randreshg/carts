// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/../examples/arts.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=40 -DNY=40 -DNZ=40 -DNREPS=1 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// Check that DataPtrHoisting treats invariant pointer2memref materializations
// as first-class hoist candidates so the EDT body reuses memrefs instead of
// rebuilding them inside the hot inner stencil loops.
//
// The current pre-lowering shape hoists all dependency-backed memref views
// ahead of the worker loops, then reuses them inside the hot stencil nests.

// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %[[MU_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?xf32>
// CHECK-NEXT: %[[LA_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?xf32>
// CHECK-NEXT: %[[CENTER_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK-NEXT: %[[RHS_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK-NEXT: scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:   scf.for %{{.+}} = %c4 to %c36 step %c1 {
// CHECK:     scf.for %{{.+}} = %c4 to %c36 step %c1 {
// CHECK:       %{{.+}} = polygeist.load %[[MU_MEM]][
// CHECK:       %{{.+}} = polygeist.load %[[LA_MEM]][
// CHECK:       %{{.+}} = polygeist.load %[[CENTER_MEM]][
// CHECK:       polygeist.store %{{.+}}, %[[RHS_MEM]][
