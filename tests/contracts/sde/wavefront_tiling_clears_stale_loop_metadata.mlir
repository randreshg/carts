// RUN: %carts-compile %S/../inputs/snapshots/seidel_2d_openmp_to_arts.mlir --pipeline pattern-pipeline --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// The Seidel wavefront transform creates a new frontier-loop iteration space.
// It must keep the wavefront/stencil contract, but it must not inherit the
// source row-loop metadata because that metadata no longer describes the new
// loop bounds or dependence structure.
// CHECK: arts.edt <parallel>
// CHECK: arts.for(
// CHECK-NOT: arts.loop = #arts.loop_metadata<
// CHECK: }} {arts.pattern_revision = 1 : i64, arts.skip_loop_metadata_recovery, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [