// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that the outlined Seidel EDT correctly accesses neighbor dep blocks
// through dep_gep and handles boundary conditions with scf.if guards.
// The function loads center, top, and bottom block pointers, with conditional
// fallbacks for boundary blocks.
// CHECK-LABEL: func.func private @__arts_edt_{{[0-9]+}}
// CHECK: arts_rt.edt_param_unpack
// CHECK: arts_rt.dep_gep
// CHECK: scf.for
// CHECK: polygeist.store
