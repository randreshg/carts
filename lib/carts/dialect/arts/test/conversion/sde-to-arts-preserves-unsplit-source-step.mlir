// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,sde-storage-to-arts-db,sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window

// Nested source-body loops keep their source chunk step when ARTS has not split
// a grouped writer to an owner-local range.

// CHECK-LABEL: func.func @preserve_unsplit_source_step
// CHECK-DAG: %[[C2:[A-Za-z0-9_]+]] = arith.constant 2 : index
// CHECK-DAG: %[[C4:[A-Za-z0-9_]+]] = arith.constant 4 : index
// CHECK: arts.edt
// CHECK: ^bb0
// CHECK: scf.for %[[LOCAL:[A-Za-z0-9_]+]] = {{.*}} to {{.*}} step %[[STEP:[A-Za-z0-9_]+]]
// CHECK-NOT: arith.addi %[[LOCAL]], %[[C4]]
// CHECK: arith.addi %[[LOCAL]], %[[STEP]]
// CHECK: scf.for %{{.*}} = %[[LOCAL]] to

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @preserve_unsplit_source_step() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %A = sde.mu_alloc {arrayId = 0 : i64} : memref<8xf32>

    sde.su_iterate (%c0) to (%c8) step (%c2) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<8xf32> array_id(0)
      sde.cu_region <parallel> {
        %endRaw = arith.addi %i, %c2 : index
        %end = arith.minui %endRaw, %c8 : index
        scf.for %x = %i to %end step %c1 {
          memref.store %zero, %A[%x] : memref<8xf32>
        }
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4], muBlockCount = 2 : i64, role = "write"}]}
    return
  }
}
