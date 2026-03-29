// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %S/../../tools/carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.pre-lowering.mlir' | %FileCheck %s

// Verify that generic DataPtrHoisting exposes invariant memref views before
// loop-window versioning. The outlined Seidel EDT should reuse hoisted
// pointer2memref views and split the hot column loop into left/interior/right
// bands instead of rebuilding memrefs or keeping per-element boundary selects.
// CHECK-LABEL: func.func private @__arts_edt_{{[0-9]+}}
// CHECK: %[[TOP_GUID:.+]], %[[TOP_ADDR:.+]] = arts.dep_gep
// CHECK: %[[MID_GUID:.+]], %[[MID_ADDR:.+]] = arts.dep_gep
// CHECK: %[[BOT_GUID:.+]], %[[BOT_ADDR:.+]] = arts.dep_gep
// CHECK: %[[TOP_RAW:.+]] = llvm.load %[[TOP_ADDR]] : !llvm.ptr -> !llvm.ptr
// CHECK: %[[MID_RAW:.+]] = llvm.load %[[MID_ADDR]] : !llvm.ptr -> !llvm.ptr
// CHECK: %[[BOT_RAW:.+]] = llvm.load %[[BOT_ADDR]] : !llvm.ptr -> !llvm.ptr
// CHECK: %[[TOP_MEM:.+]] = polygeist.pointer2memref %[[TOP_RAW]] : !llvm.ptr to memref<?x?xf64>
// CHECK: %[[MID_MEM:.+]] = polygeist.pointer2memref %[[MID_RAW]] : !llvm.ptr to memref<?x?xf64>
// CHECK: %[[BOT_MEM:.+]] = polygeist.pointer2memref %[[BOT_RAW]] : !llvm.ptr to memref<?x?xf64>
// CHECK: scf.for %[[LEFT_IV:.+]] = %{{.+}} to %{{.+}} step %c1 {
// CHECK-NOT: arith.select
// CHECK-NOT: polygeist.pointer2memref
// CHECK: %[[LEFT_BASE:.+]] = arith.addi %{{.+}}, %c127 : index
// CHECK: %{{.+}} = polygeist.load %[[TOP_MEM]][%{{.+}}, %[[LEFT_BASE]]] sizes
// CHECK: %[[LEFT_RIGHT:.+]] = arith.addi %{{.+}}, %c1 : index
// CHECK: %{{.+}} = polygeist.load %[[TOP_MEM]][%{{.+}}, %[[LEFT_RIGHT]]] sizes
// CHECK: polygeist.store %{{.+}}, %[[MID_MEM]][%{{.+}}, %{{.+}}] sizes
// CHECK: scf.for %[[MID_IV:.+]] = %{{.+}} to %{{.+}} step %c1 {
// CHECK-NOT: arith.select
// CHECK-NOT: polygeist.pointer2memref
// CHECK: %[[MID_LEFT:.+]] = arith.subi %{{.+}}, %c1 : index
// CHECK: %{{.+}} = polygeist.load %[[TOP_MEM]][%{{.+}}, %[[MID_LEFT]]] sizes
// CHECK: %[[MID_RIGHT:.+]] = arith.addi %{{.+}}, %c1 : index
// CHECK: %{{.+}} = polygeist.load %[[TOP_MEM]][%{{.+}}, %[[MID_RIGHT]]] sizes
// CHECK: polygeist.store %{{.+}}, %[[MID_MEM]][%{{.+}}, %{{.+}}] sizes
// CHECK: scf.for %[[RIGHT_IV:.+]] = %{{.+}} to %{{.+}} step %c1 {
// CHECK-NOT: arith.select
// CHECK-NOT: polygeist.pointer2memref
// CHECK: %[[RIGHT_LEFT:.+]] = arith.subi %{{.+}}, %c1 : index
// CHECK: %{{.+}} = polygeist.load %[[TOP_MEM]][%{{.+}}, %[[RIGHT_LEFT]]] sizes
// CHECK: %[[RIGHT_WRAP:.+]] = arith.addi %{{.+}}, %c-127 : index
// CHECK: %{{.+}} = polygeist.load %[[TOP_MEM]][%{{.+}}, %[[RIGHT_WRAP]]] sizes
// CHECK: polygeist.store %{{.+}}, %[[MID_MEM]][%{{.+}}, %{{.+}}] sizes

module {
}
