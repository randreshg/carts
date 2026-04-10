// RUN: %carts-compile %S/../inputs/snapshots/activations_openmp_to_arts.mlir --emit-llvm --arts-config %S/../inputs/arts_1t.cfg | %FileCheck %s

// The scalar-call activations path should not receive forced llvm.loop
// vectorize/unroll hints because LLVM cannot honor them profitably on that
// path. Under the 1-worker direct-lowering path the elementwise loops live in
// main_edt, so check the tanhf loop there while still proving the exp path
// exists in the same lowered function.
// CHECK-LABEL: define void @main_edt(
// CHECK: call float @tanhf
// CHECK-NOT: llvm.loop.vectorize.width
// CHECK-NOT: llvm.loop.unroll.count
// CHECK: call float @llvm.exp.f32
