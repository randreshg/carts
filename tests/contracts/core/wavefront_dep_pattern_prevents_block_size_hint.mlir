// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline concurrency --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Wavefront_2d loops already expose a dependency-limited frontier. The generic
// intranode stencil owned-span clamp must not add an arts.partition_hint block
// size here, or ForLowering will collapse the frontier before task creation.
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<wavefront_2d>
// CHECK-NOT: arts.partition_hint = {blockSize =