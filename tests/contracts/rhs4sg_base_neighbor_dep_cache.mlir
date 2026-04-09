// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that blocked neighbor pointer caching handles the rhs4sg z-stencil
// tap loop. The hot 5-tap reduction should use loop-invariant center/prev/next
// dep pointers plus in-loop pointer selects, not per-tap dep_gep/divui chains.

// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%1 : index] indices[%{{.+}} : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %[[NEG_PTR:.+]] = scf.if %{{.+}} -> (!llvm.ptr) {
// CHECK:   %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK:   scf.yield %{{.+}} : !llvm.ptr
// CHECK: } else {
// CHECK:   scf.yield %{{.+}} : !llvm.ptr
// CHECK: }
// CHECK: %[[POS_PTR:.+]] = scf.if %{{.+}} -> (!llvm.ptr) {
// CHECK:   %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK:   scf.yield %{{.+}} : !llvm.ptr
// CHECK: } else {
// CHECK:   scf.yield %{{.+}} : !llvm.ptr
// CHECK: }
// CHECK: scf.for %{{.+}} = %c-2 to %c3 step %c1 iter_args(%{{.+}} = %cst_1) -> (f32) {
// CHECK:   %{{.+}} = arith.cmpi {{.+}}, %{{.+}}, %{{.+}} : index
// CHECK:   %{{.+}} = arith.cmpi {{.+}}, %{{.+}}, %{{.+}} : index
// CHECK:   %{{.+}} = llvm.select %{{.+}}, %[[NEG_PTR]], %{{.+}} : i1, !llvm.ptr
// CHECK:   %{{.+}} = llvm.select %{{.+}}, %[[POS_PTR]], %{{.+}} : i1, !llvm.ptr
// CHECK:   %{{.+}} = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK-NOT: arts_rt.dep_gep
// CHECK-NOT: arith.divui
// CHECK:   %{{.+}} = polygeist.load %{{.+}}[%{{.+}}, %{{.+}}, %{{.+}}, %{{.+}}] sizes
// CHECK:   scf.yield %{{.+}} : f32

module {
}
