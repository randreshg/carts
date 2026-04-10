// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/ml-kernels/batchnorm/batchnorm.c --pipeline arts-to-llvm --arts-config %S/../inputs/arts_2t.cfg -- -lm >/dev/null && cat %t.compile/batchnorm.arts-to-llvm.mlir' | %FileCheck %s

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

module {
}
