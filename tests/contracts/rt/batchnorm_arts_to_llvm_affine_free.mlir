// RUN: %carts-compile %S/../inputs/snapshots/batchnorm_openmp_to_arts.mlir --pipeline arts-to-llvm --arts-config %S/../inputs/arts_2t.cfg | %FileCheck %s

// Verify that late ARTS-to-LLVM lowering for batchnorm remains affine-free and
// expresses N-D index remapping with arithmetic ops.
// CHECK-LABEL: module attributes
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: llvm.getelementptr
// CHECK-NOT: affine.apply
// CHECK-NOT: affine.for
// CHECK-NOT: affine.if
// CHECK-NOT: builtin.unrealized_conversion_cast
