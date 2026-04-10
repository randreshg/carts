// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline arts-to-llvm --arts-epoch-finish-continuation --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that continuation lowering keeps Seidel DB-guid dependencies
// on the direct pointer lookup path. This guards the regression where
// record_dep fell back to memref.dim on a rehydrated guid handle after
// outlining broke the original DbAllocOp def-use chain.
// CHECK-LABEL: func.func private @__arts_edt_{{[0-9]+}}
// CHECK-NOT: memref.dim %{{.+}} : memref<?x?xi64>
// CHECK: %[[GUID_TABLE:.+]] = polygeist.memref2pointer %{{.+}} : memref<?xi64> to !llvm.ptr
// CHECK-NEXT: %[[LINEAR_I64:.+]] = arith.index_cast %{{.+}} : index to i64
// CHECK-NEXT: %[[GUID_ADDR:.+]] = llvm.getelementptr %[[GUID_TABLE]][%[[LINEAR_I64]]] : (!llvm.ptr, i64) -> !llvm.ptr, i64
// CHECK-NEXT: %[[GUID_VALUE:.+]] = llvm.load %[[GUID_ADDR]] : !llvm.ptr -> i64
// CHECK: func.call @arts_add_dependence{{.*}}(%[[GUID_VALUE]], %{{.+}}, %{{.+}}, %{{.+}}{{.*}})