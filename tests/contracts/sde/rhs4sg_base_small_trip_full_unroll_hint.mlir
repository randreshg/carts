// RUN: %carts-compile %S/../inputs/snapshots/rhs4sg_base_openmp_to_arts.mlir --emit-llvm --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s

// Verify that loop-local hinting can override the generic per-function unroll
// count for tiny fixed-trip innermost loops. rhs4sg's 5-tap reducer should
// carry a full-unroll metadata marker while larger loops still keep the
// default count-based hint.
// CHECK-DAG: !{!"llvm.loop.unroll.full"}
// CHECK-DAG: !{!"llvm.loop.unroll.count", i32 2}
