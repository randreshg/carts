// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/ml-kernels/activations/activations.c --pipeline initial-cleanup --arts-config %S/../inputs/arts_1t.cfg -- -DSIZE=1024 -DNREPS=1 -lm >/dev/null && %carts compile %t.compile/activations.mlir --emit-llvm --arts-config %S/../inputs/arts_1t.cfg' | %FileCheck %s

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

module {
}
