// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'

// PASS: the real CU-outside-SU nesting. The scf.for is accepted because its
// nearest SDE ancestor is the INNER cu_region, even though an outer SU and an
// outer CU also enclose it. Pins that the rule is nearest-SDE-ancestor, not
// "has some CU ancestor".
module {
  func.func @inner_cu_wraps_scf(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c8) step (%c1) {
      ^bb0(%i: index):
        sde.cu_region <parallel> {
          scf.for %j = %c0 to %c8 step %c1 {
            %v = memref.load %A[%j] : memref<8xf32>
            memref.store %v, %A[%j] : memref<8xf32>
          }
          sde.yield
        }
        sde.yield
      }
      sde.yield
    }
    return
  }
}
