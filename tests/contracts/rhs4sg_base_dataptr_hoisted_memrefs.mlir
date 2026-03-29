// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/../examples/arts.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=20 -DNY=20 -DNZ=20 -DNREPS=1 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// Check that DataPtrHoisting treats invariant pointer2memref materializations
// as first-class hoist candidates so the EDT body reuses memrefs instead of
// rebuilding them inside the hot inner stencil loops.

// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %[[CENTER_PTR:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %[[CENTER_MEM:.+]] = polygeist.pointer2memref %[[CENTER_PTR]] : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: scf.for %[[OUTER:.+]] =
// CHECK:   %[[MU_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?xf32>
// CHECK:   %[[LA_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?xf32>
// CHECK:   %[[RHS_MEM:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK:   scf.for %[[MID:.+]] =
// CHECK:     scf.for %[[INNER:.+]] =
// CHECK:       %{{.+}} = polygeist.load %[[MU_MEM]][
// CHECK:       %{{.+}} = polygeist.load %[[LA_MEM]][
// CHECK:       %{{.+}} = polygeist.load %[[CENTER_MEM]][
// CHECK:       polygeist.store %{{.+}}, %[[RHS_MEM]][
