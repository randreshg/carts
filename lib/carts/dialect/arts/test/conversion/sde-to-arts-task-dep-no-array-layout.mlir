// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db)' 2>&1 | %FileCheck %s

// A write layout root without arrayLayout facts is malformed committed
// provenance. Existing SDE verification rejects it before ARTS can drop to
// coarse.

// CHECK: commits array root provenance but the enclosing sde.su_iterate has no arrayLayout

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @task_dep_no_array_layout() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %value = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<64xf32>

    sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<64xf32> array_id(0)
      sde.yield
    }

    sde.cu_task {
      sde.mu_dep <write> %A[%c0] size[%c64] : memref<64xf32> -> !sde.dep
      memref.store %value, %A[%c0] : memref<64xf32>
    }
    return
  }
}
