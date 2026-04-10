// RUN: %carts-compile %S/../inputs/snapshots/activations_openmp_to_arts.mlir --pipeline pattern-pipeline --arts-config %S/../inputs/arts_1t.cfg | grep -n 'locationKey = "activations.c:196:5"' | %FileCheck %s

// The elementwise/pattern pipeline must keep the transformed tanh loop tied to
// its original full-size iteration space metadata rather than reattaching the
// nearby softmax loop metadata.
// CHECK: tripCount = 1048576 : i64
// CHECK-NOT: tripCount = 99 : i64
// CHECK: locationKey = "activations.c:196:5"
