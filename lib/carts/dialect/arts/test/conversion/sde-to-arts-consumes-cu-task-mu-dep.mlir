// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.cu_task --implicit-check-not=sde.mu_dep

// SDE task depend declarations are consumed into ARTS DB-backed storage,
// explicit acquires, and an EDT. The task slice remains attached to the acquire
// as partition metadata rather than surviving as an SDE carrier.

// CHECK-LABEL: func.func @consume_task_dep_slice
// CHECK: arts.db_alloc
// CHECK: arts.db_alloc
// CHECK: arts.db_acquire
// CHECK-SAME: partitioning(<block>, offsets
// CHECK-SAME: sizes
// CHECK-SAME: preserve_dep_edge
// CHECK: arts.db_acquire
// CHECK-SAME: partitioning(<block>, offsets
// CHECK-SAME: sizes
// CHECK-SAME: preserve_dep_edge
// CHECK: arts.edt
// CHECK: memref.load
// CHECK: memref.store

func.func @consume_task_dep_slice(%n: index) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %A = memref.alloc(%n) : memref<?xf32>
  %B = memref.alloc(%n) : memref<?xf32>
  scf.for %i = %c0 to %n step %c4 {
    sde.cu_task {
      %r = sde.mu_dep <read> %A[%i] size[%c4] : memref<?xf32> -> !sde.dep
      %w = sde.mu_dep <write> %B[%i] size[%c4] : memref<?xf32> -> !sde.dep
      %v = memref.load %A[%i] : memref<?xf32>
      memref.store %v, %B[%i] : memref<?xf32>
    }
  }
  return
}
