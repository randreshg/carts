// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-atomic-reduction-materialization)' 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}has atomic reduction strategy without reduction accumulators

func.func @atomic_strategy_requires_accumulator() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  sde.su_iterate (%c0) to (%c8) step (%c1)
      reduction_strategy(<atomic>)
      classification(<reduction>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      sde.yield
    }
  }
  return
}
