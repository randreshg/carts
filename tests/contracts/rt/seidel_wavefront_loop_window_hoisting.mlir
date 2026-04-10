// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

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