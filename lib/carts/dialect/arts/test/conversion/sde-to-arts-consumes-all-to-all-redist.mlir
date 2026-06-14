// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not=sde.su_all_to_all --implicit-check-not=sde.su_distribute --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window

// Tier-1 order-preserving all-to-all regrain on 8x4: source [2,4] (4 blocks) to
// target [4,4] (2 blocks), distinct DBs, reader-pull sync EDTs, su_barrier.

// CHECK-LABEL: func.func @consume_all_to_all_redist
// CHECK: %[[SRC_GUID:.*]], %[[SRC_PTR:.*]] = arts.db_alloc
// CHECK: %[[DST_GUID:.*]], %[[DST_PTR:.*]] = arts.db_alloc
// CHECK-DAG: arts.db_acquire[<in>]
// CHECK-DAG: arts.db_acquire[<out>]
// CHECK-DAG: arts.edt {{.*}}sync

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @consume_all_to_all_redist() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c4 = arith.constant 4 : index
    %zero = arith.constant 0.0 : f32
    %T = sde.mu_alloc : memref<8x4xf32>
    %U = sde.mu_alloc : memref<8x4xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c4) step (%c1, %c1) classification(<elementwise_pipeline>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %T : memref<8x4xf32> array_id(0)
      sde.cu_region <single> {
        memref.store %zero, %T[%i, %j] : memref<8x4xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [2, 4], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}]}

    sde.su_distribute <owner_compute> {
      sde.su_all_to_all %T : memref<8x4xf32> array_id(0) source owner [0] block [2, 4] target owner [0] block [4, 4]
      sde.su_barrier
      sde.su_iterate (%c0, %c0) to (%c8, %c4) step (%c1, %c1) classification(<elementwise_pipeline>) {
      ^bb0(%i: index, %j: index):
        sde.array_layout_root read %T : memref<8x4xf32> array_id(0)
        sde.array_layout_root write %U : memref<8x4xf32> array_id(1)
        sde.cu_region <parallel> {
          %v = memref.load %T[%i, %j] : memref<8x4xf32>
          memref.store %v, %U[%i, %j] : memref<8x4xf32>
        } {groupBlockCount = [2]}
        sde.yield
      } {arrayLayout = [{arrayId = 0 : i64, blockShape = [2, 4], kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "read"}, {arrayId = 1 : i64, blockShape = [4, 4], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}
    }
    return
  }
}
