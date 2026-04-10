// RUN: %carts-compile %S/../inputs/snapshots/specfem3d_velocity_288x_openmp_to_arts.mlir --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// The large velocity benchmark has 382 outer k-iterations and 64 worker
// threads. Lowering should expose one worker lane per available worker instead
// of collapsing the loop into a smaller fixed chunk count.
// CHECK: scf.for %{{.+}} = %c0 to %c10 step %c1
// CHECK: scf.for %{{.+}} = %c0 to %c64 step %c1
