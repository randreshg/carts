// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/specfem3d/velocity/velocity_update.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg >/dev/null && cat %t.compile/velocity_update.pre-lowering.mlir' | %FileCheck %s

// Velocity only exposes 46 outer-loop iterations in the source OpenMP region
// because the source loop is `for (k = 1; k < NZ - 1; ++k)` with `NZ=48`.
// The compiler must not let arts-for-opt collapse that natural scaling down to
// three worker lanes via a blockSize=23 hint.
// CHECK: scf.for %{{.+}} = %c0 to %c46 step %c1
// CHECK-NOT: arts.partition_hint = {blockSize = 23 : i64, mode = 1 : i8}

module {
}
