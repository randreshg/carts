// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @cu_region_contains_su_under_scf(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %cond = arith.cmpi slt, %c0, %c8 : index

    sde.cu_region <single> {
      scf.if %cond {
        sde.su_iterate (%c0) to (%c8) step (%c1) {
        ^bb0(%i: index):
          sde.cu_region <single> {
            %v = memref.load %A[%i] : memref<8xf32>
            memref.store %v, %A[%i] : memref<8xf32>
            sde.yield
          }
          sde.yield
        }
      }
      sde.yield
    }
    return
  }
}

// CHECK: 'sde.su_iterate' op is nested inside an sde.cu_region body
// CHECK: compute units are executable leaves and SU scheduling must be represented outside the CU
