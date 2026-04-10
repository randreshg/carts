// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline arts-to-llvm --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Seidel's wavefront lowering should keep the owned center block on the
// whole-DB path while lowering neighbor halo reads to split-phase sliced
// dependence registration.
// CHECK-LABEL: func.func @main
// CHECK: func.call @arts_add_dependence_ex(
// CHECK: func.call @arts_add_dependence_at_ex(