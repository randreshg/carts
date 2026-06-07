// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'

// PASS: two SU bands ordered by a plain sde.su_barrier; each band wraps its
// scf.for in an inner CU. Distinct SU bands are explicit ordering structure, not
// hidden textual sequencing.
module {
  func.func @distinct_su_bands(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
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
    sde.su_barrier
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%k: index):
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
