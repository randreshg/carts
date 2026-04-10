// RUN: %carts-compile %S/../inputs/snapshots/specfem3d_velocity_openmp_to_arts.mlir --pipeline pattern-pipeline --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// The velocity kernel has loop-IV casts ahead of the innermost loop body. It
// must compile through pattern-pipeline without being misclassified as a
// matmul init/reduction nest.
// CHECK: arts.edt <parallel>
// CHECK: arts.for(
// CHECK: depPattern = #arts.dep_pattern<uniform>
