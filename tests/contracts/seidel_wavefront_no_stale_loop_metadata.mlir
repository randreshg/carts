// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline pattern-pipeline --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.pattern-pipeline.mlir' | %FileCheck %s

// The Seidel wavefront transform creates a new frontier-loop iteration space.
// It must keep the wavefront/stencil contract, but it must not inherit the
// source row-loop metadata because that metadata no longer describes the new
// loop bounds or dependence structure.
// CHECK: arts.edt <parallel>
// CHECK: arts.for(
// CHECK-NOT: arts.loop = #arts.loop_metadata<
// CHECK: }} {arts.pattern_revision = 1 : i64, depPattern = #arts.dep_pattern<wavefront_2d>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_block_shape = [

module {
}
