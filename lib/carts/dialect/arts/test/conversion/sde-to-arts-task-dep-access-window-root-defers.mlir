// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --implicit-check-not='arts.db_alloc{{.*}}<coarse>'

// A task-dep root that also has SDE memory accesses may require access-window
// realization. ARTS must fail closed instead of realizing the task dep early
// and erasing the only path that can produce db_access_window facts.

// CHECK: task dependency root also has SDE memory accesses that require access-window realization

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @task_dep_access_window_root_defers() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %value = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8x1x16x1xf32>

    sde.su_iterate (%c0) to (%c8) step (%c1) classification(<elementwise>) {
    ^bb0(%b: index):
      sde.array_layout_root write %A : memref<8x1x16x1xf32> array_id(0)
      sde.cu_region <parallel> {
        scf.for %slot = %c0 to %c16 step %c1 {
          memref.store %value, %A[%b, %c0, %slot, %c0] : memref<8x1x16x1xf32>
        }
      } {groupBlockCount = [4]}
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [1, 16, 1], muBlockCount = 8 : i64, role = "write"}]}

    sde.cu_task {
      sde.mu_dep <write> %A[%c0, %c0, %c0, %c0] size[%c8, %c1, %c16, %c1] : memref<8x1x16x1xf32> -> !sde.dep
      memref.store %value, %A[%c0, %c0, %c0, %c0] : memref<8x1x16x1xf32>
    }
    return
  }
}
