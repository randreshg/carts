// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that blocked neighbor pointer caching handles the rhs4sg z-stencil
// tap loop. The hot 5-tap reduction should hoist the dep pointer materialization
// out of the loop and avoid per-tap dep_gep/divui chains.

// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%1 : index] indices[%{{.+}} : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %{{.+}} = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?xf32>
// CHECK: %{{.+}} = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?xf32>
// CHECK: %[[NEG_MEMREF:.+]] = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: %{{.+}} = polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: memref.store %cst_1, %{{.+}}[] : memref<f32>
// CHECK-NEXT: scf.for %{{.+}} = %c-2 to %c3 step %c1 {
// CHECK:   %{{.+}} = polygeist.load %[[NEG_MEMREF]][%c0, %{{.+}}, %{{.+}}, %{{.+}}] sizes
// CHECK-NOT: arts_rt.dep_gep
// CHECK-NOT: arith.divui
// CHECK: }

module {
}
