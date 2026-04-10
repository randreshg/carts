// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that the blocked-neighbor dep-pointer cache rewrites the outlined
// rhs4sg tap loop to use block-start-relative local indices instead of a
// per-tap remainder. The hot reduction should use hoisted dep loads and
// local-index candidates without rebuilding a quotient/remainder pair in the
// inner 5-tap loop.
// CHECK-LABEL: func.func private @__arts_edt_1
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: %{{.+}} = llvm.load %{{.+}} : !llvm.ptr -> !llvm.ptr
// CHECK: scf.for %[[TAP:.+]] = %c-2 to %c3 step %c1 {
// CHECK: %[[GLOBAL:.+]] = arith.addi %{{.+}}, %{{.+}} : i32
// CHECK: %{{.+}} = arith.index_cast %[[GLOBAL]] : i32 to index
// CHECK-NOT: arith.remui
// CHECK: polygeist.load %{{.+}}[%c0, %{{.+}}, %{{.+}}, %{{.+}}] sizes
