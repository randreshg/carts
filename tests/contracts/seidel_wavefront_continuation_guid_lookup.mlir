// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline arts-to-llvm --arts-epoch-finish-continuation --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.arts-to-llvm.mlir' | %FileCheck %s

// Verify that continuation lowering keeps Seidel DB-guid dependencies
// on the direct pointer lookup path. This guards the regression where
// record_dep fell back to memref.dim on a rehydrated guid handle after
// outlining broke the original DbAllocOp def-use chain.
// CHECK-LABEL: func.func private @__arts_edt_3
// CHECK: %[[GUID_RAW:.+]] = llvm.inttoptr %{{.+}} : i64 to !llvm.ptr
// CHECK-NOT: memref.dim %{{.+}} : memref<?x?xi64>
// CHECK: arts_edt_create_with_epoch{{.*}}arts.create_id = 117000
// CHECK: %[[LINEAR_I64:.+]] = arith.index_cast %{{.+}} : index to i64
// CHECK-NEXT: %[[GUID_ADDR:.+]] = llvm.getelementptr %[[GUID_RAW]][%[[LINEAR_I64]]] : (!llvm.ptr, i64) -> !llvm.ptr, i64
// CHECK-NEXT: %[[GUID_VALUE:.+]] = llvm.load %[[GUID_ADDR]] : !llvm.ptr -> i64
// CHECK-NEXT: func.call @arts_add_dependence(%[[GUID_VALUE]], %{{.+}}, %{{.+}}, %{{.+}}) : (i64, i64, i32, i32) -> ()

module {
}
