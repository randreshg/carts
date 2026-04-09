// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.pre-lowering.mlir' | %FileCheck %s

// Verify that the outlined Seidel EDT splits the hot column loop into
// left/interior/right bands.  Each band loads from hoisted pointer2memref
// views and stores back through the mid-row view.  The three sequential
// scf.for loops (left, interior, right) replace a single loop with
// per-element boundary selects.
//
// The left band uses two memref views (top and mid) loaded before the band.
// The interior band re-loads the mid pointer for its own view.
// The right band introduces a third dep_gep (bottom) and re-loads both mid
// and bottom pointers.
// CHECK-LABEL: func.func private @__arts_edt_{{[0-9]+}}
// --- left band: two dep_gep, two pointer2memref, then loop with store ---
// CHECK: arts_rt.dep_gep
// CHECK: arts_rt.dep_gep
// CHECK: polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?xf64>
// CHECK: polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?xf64>
// CHECK: scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:   scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:     polygeist.store %{{.+}}, %{{.+}}[%{{.+}}, %{{.+}}] sizes
// --- interior band: re-loads mid pointer, then loop with store ---
// CHECK: polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?xf64>
// CHECK: scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:   scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:     polygeist.store %{{.+}}, %{{.+}}[%{{.+}}, %{{.+}}] sizes
// --- right band: new dep_gep + two pointer2memref (mid, bot), loop with store ---
// CHECK: arts_rt.dep_gep
// CHECK: polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?xf64>
// CHECK: polygeist.pointer2memref %{{.+}} : !llvm.ptr to memref<?x?xf64>
// CHECK: scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:   scf.for %{{.+}} = %{{.+}} to %{{.+}} step %c1 {
// CHECK:     polygeist.store %{{.+}}, %{{.+}}[%{{.+}}, %{{.+}}] sizes

module {
}
