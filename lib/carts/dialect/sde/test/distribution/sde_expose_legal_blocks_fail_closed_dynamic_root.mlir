// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-expose-legal-blocks)' 2>&1 | %FileCheck %s

// A load-bearing committed layout fact cannot be refined to legal blocks when
// its explicit root has dynamic shape. SDE must fail closed instead of leaving a
// downstream layer to infer the block grain from fallback shape guesses.

// CHECK: sde-expose-legal-blocks: cannot ground static logical shape for array 0

module attributes {"carts.sde.max-block-bytes" = 120 : i64} {
  func.func @dynamic_root_refuses_budget(%A: memref<?x30xf32>, %n: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c30 = arith.constant 30 : index
    %zero = arith.constant 0.0 : f32
    sde.su_iterate (%c0, %c0) to (%n, %c30) step (%c1, %c1)
        classification(<elementwise>) {
    ^bb0(%elem: index, %q: index):
      sde.array_layout_root write %A : memref<?x30xf32> array_id(0)
      sde.cu_region <parallel> {
        memref.store %zero, %A[%elem, %q] : memref<?x30xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [5000, 30],
       kind = "block_parallel", muBlockCount = 1 : i64,
       ownerDims = [0], role = "write"}]}
    return
  }
}
