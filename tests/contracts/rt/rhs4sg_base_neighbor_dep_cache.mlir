// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg -- -I%S/../../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that blocked neighbor pointer caching handles the rhs4sg z-stencil
// tap loop. The hot 5-tap reduction should hoist the dep pointer materialization
// out of the loop and avoid per-tap dep_gep/divui chains.

// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_db_acquire(%arg3) offset[%c0 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_db_acquire(%arg3) offset[%c1 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_db_acquire(%arg3) offset[%c2 : index] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> memref<?xi64>, memref<?x!llvm.ptr>
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%c3 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK-NEXT: %[[NEG_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK-NEXT: %[[PTR3D0_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK-NEXT: %{{.+}} = polygeist.pointer2memref %[[PTR3D0_RAW]] : !llvm.ptr to memref<?x?x?xf32>
// CHECK-NEXT: %[[PTR3D1_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK-NEXT: %{{.+}} = polygeist.pointer2memref %[[PTR3D1_RAW]] : !llvm.ptr to memref<?x?x?xf32>
// CHECK-NEXT: %[[NEG_SRC_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK-NEXT: %[[NEG_MEMREF:.+]] = polygeist.pointer2memref %[[NEG_SRC_RAW]] : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK-NEXT: %{{.+}} = polygeist.pointer2memref %[[NEG_RAW]] : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: memref.store %cst_1, %{{.+}}[] : memref<f32>
// CHECK-NEXT: scf.for %{{.+}} = %c-2 to %c3 step %c1 {
// CHECK:   %{{.+}} = polygeist.load %[[NEG_MEMREF]][%c0, %{{.+}}, %{{.+}}, %{{.+}}] sizes
// CHECK-NOT: arts_rt.dep_gep
// CHECK-NOT: arith.divui
// CHECK: }

module {
}
