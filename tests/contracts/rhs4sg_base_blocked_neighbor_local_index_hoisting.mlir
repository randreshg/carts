// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && cat %t.compile/rhs4sg_base.pre-lowering.mlir' | %FileCheck %s

// Verify that the blocked-neighbor dep-pointer cache rewrites the outlined
// rhs4sg tap loop to use block-start-relative local indices instead of a
// per-tap remainder. The hot reduction should select among cached neighbor
// pointers and local-index candidates without rebuilding a quotient/remainder
// pair in the inner 5-tap loop.
// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %[[NEG_PTR:.+]] = scf.if {{.+}} -> (!llvm.ptr) {
// CHECK: %[[POS_PTR:.+]] = scf.if {{.+}} -> (!llvm.ptr) {
// CHECK: scf.for %[[TAP:.+]] = %c-2 to %c3 step %c1 iter_args(%{{.+}} = %cst_1) -> (f32) {
// CHECK: %{{.+}} = arith.addf %{{.+}}, %{{.+}} : f32
// CHECK: %[[GLOBAL:.+]] = arith.addi %{{.+}}, %[[TAP]] : index
// CHECK: %[[NEXT_BLOCK:.+]] = arith.addi %{{.+}}, %{{.+}} : index
// CHECK: %[[LOW:.+]] = arith.cmpi ult, %[[GLOBAL]], %{{.+}} : index
// CHECK: %[[HIGH:.+]] = arith.cmpi uge, %[[GLOBAL]], %[[NEXT_BLOCK]] : index
// CHECK: %[[LOCAL:.+]] = arith.subi %[[GLOBAL]], %{{.+}} : index
// CHECK: %[[LOCAL_NEG:.+]] = arith.addi %[[LOCAL]], %{{.+}} : index
// CHECK: %[[LOCAL_POS:.+]] = arith.subi %[[LOCAL]], %{{.+}} : index
// CHECK: %[[SEL_LOCAL0:.+]] = arith.select %[[LOW]], %[[LOCAL_NEG]], %[[LOCAL]] : index
// CHECK: %[[SEL_LOCAL1:.+]] = arith.select %[[HIGH]], %[[LOCAL_POS]], %[[SEL_LOCAL0]] : index
// CHECK-NOT: arith.remui
// CHECK: %[[SEL_PTR0:.+]] = llvm.select %[[LOW]], %[[NEG_PTR]], %{{.+}} : i1, !llvm.ptr
// CHECK: %[[SEL_PTR1:.+]] = llvm.select %[[HIGH]], %[[POS_PTR]], %[[SEL_PTR0]] : i1, !llvm.ptr
// CHECK: %[[VIEW:.+]] = polygeist.pointer2memref %[[SEL_PTR1]] : !llvm.ptr to memref<?x?x?x?xf32>
// CHECK: polygeist.load %[[VIEW]][%c0, %{{.+}}, %{{.+}}, %[[SEL_LOCAL1]]] sizes

module {
}
