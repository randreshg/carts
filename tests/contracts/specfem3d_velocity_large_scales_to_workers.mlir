// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/specfem3d/velocity/velocity_update.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -O3 -- -DNX=288 -DNY=288 -DNZ=384 -DNREPS=10 -I%S/../../external/carts-benchmarks/specfem3d/common >/dev/null && cat %t.compile/velocity_update.pre-lowering.mlir' | %FileCheck %s

// The large velocity benchmark has 382 outer k-iterations and 64 worker
// threads. Lowering should expose one worker lane per available worker instead
// of collapsing the loop into a smaller fixed chunk count.
// CHECK: scf.for %{{.+}} = %c0 to %c10 step %c1
// CHECK: scf.for %{{.+}} = %c0 to %c64 step %c1

module {
}
