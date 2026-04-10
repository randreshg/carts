// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/specfem3d/velocity/velocity_update.c --pipeline pattern-pipeline --arts-config %S/../inputs/arts_64t.cfg >/dev/null && cat %t.compile/velocity_update.pattern-pipeline.mlir' | %FileCheck %s

// The velocity kernel has loop-IV casts ahead of the innermost loop body. It
// must compile through pattern-pipeline without being misclassified as a
// matmul init/reduction nest.
// CHECK: arts.edt <parallel>
// CHECK: arts.for(
// CHECK: depPattern = #arts.dep_pattern<uniform>

module {
}
