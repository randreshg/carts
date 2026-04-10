// RUN: %carts-compile %S/../inputs/snapshots/specfem3d_velocity_288x_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// The outlined velocity worker should keep source-level scalar temporaries in
// SSA form instead of carrying rank-0 scratch memrefs through pre-lowering.
// CHECK-LABEL: func.func private @__arts_edt_1(
// CHECK-NOT: memref.alloca
// CHECK-LABEL: func.func @main()
