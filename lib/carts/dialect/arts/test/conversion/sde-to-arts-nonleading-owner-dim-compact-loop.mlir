// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not="owner dim exceeds loop rank" --implicit-check-not=sde.mu_access_window --implicit-check-not=arts.db_access_plan

// A compact owner-rank SU loop may carry a non-leading physical owner dim. The
// ARTS DB slot is dense slot 0, while the physical block extent comes from
// physicalBlockShape[2].

// CHECK-LABEL: func.func @nonleading_owner_dim_compact_loop
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [1, 8, 8, 4]
// CHECK: arts.db_acquire
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt

func.func @nonleading_owner_dim_compact_loop() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %c64 = arith.constant 64 : index
  %zero = arith.constant 0.000000e+00 : f32
  %A = sde.mu_alloc : memref<8x8x64xf32>
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
  ^bb0(%k: index):
    sde.cu_region <parallel> {
      scf.for %i = %c0 to %c8 step %c1 {
        scf.for %j = %c0 to %c8 step %c1 {
          memref.store %zero, %A[%i, %j, %k] : memref<8x8x64xf32>
        }
      }
    }
  } {physicalOwnerDims = [2], physicalBlockShape = [8, 8, 4]}
  return
}
