// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline concurrency --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.concurrency.mlir' | %FileCheck %s

// Wavefront_2d loops already expose a dependency-limited frontier. The generic
// intranode stencil owned-span clamp must not add an arts.partition_hint block
// size here, or ForLowering will collapse the frontier before task creation.
// CHECK: arts.edt <parallel>
// CHECK: depPattern = #arts.dep_pattern<wavefront_2d>
// CHECK-NOT: arts.partition_hint = {blockSize =

module {
}
