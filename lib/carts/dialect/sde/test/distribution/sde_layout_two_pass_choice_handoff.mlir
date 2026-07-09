// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline=sde-planning | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-layout-candidate-choose)' | %FileCheck %s --check-prefix=CHOOSE-NOOP

// The production layout engine runs as the two-pass split
// sde-layout-candidate-choose -> sde-writer-layout-commit. Pass 1 commits the
// chosen logical layout as the transient `sde.layout_choice` module fact; pass 2
// consumes that committed fact, realizes the per-SU writer layout facts, and
// erases the choice. The realized layout facts must appear and the transient
// choice fact must NOT survive the pair.

// CHECK-NOT: sde.layout_choice
// CHECK-LABEL: func.func @two_pass_block_parallel_writer
// CHECK: sde.su_distribute <blocked>
// CHECK: sde.array_layout write array_id({{[0-9]+}}) owner
// CHECK: sde.array_layout_root write %{{.*}} array_id({{[0-9]+}})
// CHECK: role = "write"

// Without a cost model (textual single-pass invocation) sde-layout-candidate-choose
// resolves and is a clean no-op: it commits no choice fact and leaves the SU intact.
// CHOOSE-NOOP-NOT: sde.layout_choice
// CHOOSE-NOOP-LABEL: func.func @two_pass_block_parallel_writer
// CHOOSE-NOOP: sde.su_iterate

func.func @two_pass_block_parallel_writer() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024x1024xf32>
  sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <parallel> {
      memref.store %cst, %A[%i, %j] : memref<1024x1024xf32>
      sde.yield
    }
    sde.yield
  }
  return
}
