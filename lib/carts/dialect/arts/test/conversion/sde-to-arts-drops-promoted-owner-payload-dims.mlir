// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde. --implicit-check-not=memref.expand_shape --implicit-check-not="elementSizes[%c1"

// A rank-expanded SDE MU uses leading owner dimensions as the DB grid. ARTS
// must not duplicate those promoted owner dimensions inside each DB payload.

// CHECK-LABEL: func.func @promoted_owner_dims_become_db_grid_not_payload
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: sizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: elementSizes[%c4{{(_[0-9]+)?}}, %c4{{(_[0-9]+)?}}]
// CHECK: arts.db_ref %{{.*}}[%c0{{(_[0-9]+)?}}, %c0{{(_[0-9]+)?}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{[^,]+}}, %{{[^]]+}}] : memref<?x?xf32>

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @promoted_owner_dims_become_db_grid_not_payload() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f32
    %C = sde.mu_alloc {arrayId = 0 : i64} : memref<2x2x4x4xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %C : memref<2x2x4x4xf32> array_id(0)
      sde.cu_region <parallel> {
        %ib = arith.divui %i, %c4 : index
        %jb = arith.divui %j, %c4 : index
        %il = arith.remui %i, %c4 : index
        %jl = arith.remui %j, %c4 : index
        memref.store %zero, %C[%ib, %jb, %il, %jl] : memref<2x2x4x4xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}]}
    return
  }
}
