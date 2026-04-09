// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/specfem3d/velocity/velocity_update.c --pipeline pre-lowering --arts-config %S/inputs/arts_64t.cfg -O3 -- -DNX=288 -DNY=288 -DNZ=384 -DNREPS=10 -I%S/../../external/carts-benchmarks/specfem3d/common >/dev/null && cat %t.compile/velocity_update.pre-lowering.mlir' | %FileCheck %s

// Worker-local velocity dependencies must stay slice-aware through
// pre-lowering so rec_dep can emit precise byte-range dependencies instead of
// whole-DB edges.
// CHECK: arts.lowering_contract({{.*}}contract(<ownerDims = [2], narrowableDep = true, postDbRefined = true
// CHECK: arts.rec_dep
// CHECK-SAME: byte_offsets(
// CHECK-SAME: byte_sizes(

module {
}
