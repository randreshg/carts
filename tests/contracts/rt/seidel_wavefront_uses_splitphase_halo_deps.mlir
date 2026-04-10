// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline arts-to-llvm --arts-config %S/../inputs/arts_64t.cfg -- -I%S/../../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../../external/carts-benchmarks/polybench/common -I%S/../../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.arts-to-llvm.mlir' | %FileCheck %s

// Seidel's wavefront lowering should keep the owned center block on the
// whole-DB path while lowering neighbor halo reads to split-phase sliced
// dependence registration.
// CHECK-LABEL: func.func @main
// CHECK: func.call @arts_add_dependence_ex(
// CHECK: func.call @arts_add_dependence_at_ex(

module {
}
