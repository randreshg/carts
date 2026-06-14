// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --implicit-check-not='arts.db_alloc{{.*}}<coarse>' --implicit-check-not=sde.mu_alloc --implicit-check-not=local_only

// ARTS must consume the committed writer block layout for task dependencies.
// A task dep over a block-partitioned MU becomes a per-block DB with a
// derivable owner route; it must not fall through the raw coarse bridge.

// CHECK-LABEL: func.func @task_dep_committed_block_layout
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: elementSizes[%c16{{(_[0-9]+)?}}]
// CHECK: sde.mu_dep <write> %{{.*}}[%c0] size[%c64]

// CHECK-LABEL: func.func @task_dep_selects_layout_for_exact_root
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: elementSizes[%c16{{(_[0-9]+)?}}]
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: elementSizes[%c8{{(_[0-9]+)?}}]

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @task_dep_committed_block_layout() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    %value = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<64xf32>
    %A_view = memref.cast %A : memref<64xf32> to memref<?xf32>

    sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A_view : memref<?xf32> array_id(0)
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [16], muBlockCount = 4 : i64, role = "write"}]}

    sde.cu_task {
      sde.mu_dep <write> %A[%c0] size[%c64] : memref<64xf32> -> !sde.dep
      memref.store %value, %A[%c0] : memref<64xf32>
    }
    return
  }

  func.func @task_dep_selects_layout_for_exact_root() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %value = arith.constant 1.0 : f32
    %A = sde.mu_alloc {arrayId = 10 : i64} : memref<64xf32>
    %B = sde.mu_alloc {arrayId = 11 : i64} : memref<64xf32>

    sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<64xf32> array_id(10)
      sde.array_layout_root write %B : memref<64xf32> array_id(11)
      sde.yield
    } {arrayLayout = [{arrayId = 10 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [16], muBlockCount = 4 : i64, role = "write"}, {arrayId = 11 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [8], muBlockCount = 8 : i64, role = "write"}]}

    sde.cu_task {
      sde.mu_dep <write> %A[%c0] size[%c64] : memref<64xf32> -> !sde.dep
      sde.mu_dep <write> %B[%c0] size[%c64] : memref<64xf32> -> !sde.dep
      memref.store %value, %A[%c0] : memref<64xf32>
      memref.store %value, %B[%c0] : memref<64xf32>
    }
    return
  }

}
