// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --pipeline edt-distribution | %FileCheck %s

// Verify that EdtTaskLoopLowering unconditionally computes aligned bounds
// (diffLower/diffUpper via arith.subi, ceilDiv via arith.divui, clamping via
// arith.minui) regardless of the useAlignedLowerBound flag.
// CHECK: arith.subi
// CHECK: arith.divui
// CHECK: arith.minui

module {
}
