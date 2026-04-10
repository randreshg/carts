// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that blocked neighbor pointer caching handles the rhs4sg z-stencil
// tap loop. The hot 5-tap reduction should hoist the dep pointer materialization
// out of the loop and avoid per-tap dep_gep/divui chains.

// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%c0 : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%{{.+}} : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%{{.+}} : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK: %{{.+}}, %{{.+}} = arts_rt.dep_gep(%arg3) offset[%{{.+}} : index] indices[%c0 : index] strides[%c1] : memref<?x!llvm.struct<(i64, ptr, i32, i32, i64, i64)>> -> !llvm.ptr, !llvm.ptr
// CHECK: %[[PTR0_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %[[PTR1_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %[[PTR2_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %[[PTR3_RAW:.+]] = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %{{.+}} = polygeist.pointer2memref %[[PTR0_RAW]] : !llvm.ptr to memref<?x?x?xf32>
// CHECK: %{{.+}} = polygeist.pointer2memref %[[PTR1_RAW]] : !llvm.ptr to memref<?x?x?xf32>
// CHECK: %[[INPUT_MEMREF:.+]] = polygeist.pointer2memref %[[PTR2_RAW]] : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: %{{.+}} = polygeist.pointer2memref %[[PTR3_RAW]] : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: memref.store %cst_1, %{{.+}}[] : memref<f32>
// CHECK-NEXT: scf.for %{{.+}} = %c-2 to %c3 step %c1 {
// CHECK:   %{{.+}} = polygeist.load %[[INPUT_MEMREF]][%c0, %{{.+}}, %{{.+}}, %{{.+}}] sizes
// CHECK-NOT: arts_rt.dep_gep
// CHECK-NOT: arith.divui
// CHECK: }
