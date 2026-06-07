// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'

// PASS: an SU body may hold schedule/index plumbing directly (index arithmetic
// produces only index values), as long as source compute stays inside a CU.
module {
  func.func @su_index_plumbing(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      %next = arith.addi %i, %c1 : index
      %scaled = arith.muli %i, %c8 : index
      sde.cu_region <parallel> {
        scf.for %j = %c0 to %c8 step %c1 {
          %v = memref.load %A[%j] : memref<8xf32>
          memref.store %v, %A[%j] : memref<8xf32>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }
}
