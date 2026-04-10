// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline pattern-pipeline --arts-config %S/../inputs/arts_1t.cfg -- -DNREPS=1 >/dev/null && grep -n "locationKey = \"activations.c:196:5\"" %t.compile/activations.pattern-pipeline.mlir' | %FileCheck %s

// The elementwise/pattern pipeline must keep the transformed tanh loop tied to
// its original full-size iteration space metadata rather than reattaching the
// nearby softmax loop metadata.
// CHECK: tripCount = 1048576 : i64
// CHECK-NOT: tripCount = 99 : i64
// CHECK: locationKey = "activations.c:196:5"

module {
}
