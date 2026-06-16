// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db)' 2>&1 | %FileCheck %s

// Rank-expanded block-grid storage carries committed SDE owner structure in its
// MU type. ARTS must realize per-block DBs from committed arrayLayout facts or
// fail closed; it must not silently funnel them through a coarse DB.

// CHECK: rank-expanded block-grid storage reached ARTS boundary without a committed SDE block layout
// CHECK-NOT: arts.db_alloc{{.*}}<coarse>

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @block_grid_task_dep_without_layout() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %value = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<4x16xf32>

    sde.cu_task {
      sde.mu_dep <write> %A[%c0, %c0] size[%c1, %c1] : memref<4x16xf32> -> !sde.dep
      memref.store %value, %A[%c0, %c0] : memref<4x16xf32>
    }
    return
  }
}
