// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/specfem3d/velocity/velocity_update.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -O3 -- -DNX=288 -DNY=288 -DNZ=384 -DNREPS=10 -I%S/../../external/carts-benchmarks/specfem3d/common >/dev/null && cat %t.compile/velocity_update.pre-lowering.mlir' | %FileCheck %s

// The outlined velocity worker should keep source-level scalar temporaries in
// SSA form instead of carrying rank-0 scratch memrefs through pre-lowering.
// CHECK-LABEL: func.func private @__arts_edt_1(
// CHECK-NOT: memref.alloca
// CHECK-LABEL: func.func @mainBody()

module {
}
