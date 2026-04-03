// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/sw4lite/rhs4sg-base/rhs4sg_base.c --pipeline initial-cleanup --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/sw4lite/common -DNX=320 -DNY=320 -DNZ=576 -DNREPS=10 >/dev/null && %carts compile %t.compile/rhs4sg_base.mlir --emit-llvm --arts-config %S/inputs/arts_64t.cfg' | %FileCheck %s

// Verify that loop-local hinting can override the generic per-function unroll
// count for tiny fixed-trip innermost loops. rhs4sg's 5-tap reducer should
// carry a full-unroll metadata marker while larger loops still keep the
// default count-based hint.
// CHECK-DAG: !{!"llvm.loop.unroll.full"}
// CHECK-DAG: !{!"llvm.loop.unroll.count", i32 2}

module {
}
