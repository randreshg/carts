// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not="cannot map SDE access-window block coordinate to a loop dimension" --implicit-check-not=arts.db_access_window

// In-place reduction-classified CUs can read and write the same rank-expanded
// MU through loop-carried accesses. Until SDE emits a real wavefront/skew shape,
// access-window raising must defer instead of producing a direct window that
// ARTS cannot map to the SU loop dimensions.

// CHECK-LABEL: func.func @reduction_readwrite_window_defers
// CHECK: arts.db_alloc
// CHECK-SAME: <coarse>
// CHECK: arts.edt

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @reduction_readwrite_window_defers() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %A = sde.mu_alloc : memref<8x8xf64>

    sde.su_iterate (%c0) to (%c64) step (%c8) classification(<reduction>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<8x8xf64> array_id(0)
      sde.cu_region <parallel> {
        %end = arith.addi %i, %c8 : index
        %hi = arith.minui %end, %c64 : index
        scf.for %j = %i to %hi step %c1 {
          %block = arith.divui %j, %c8 : index
          %elem = arith.remui %j, %c8 : index
          %prev = memref.load %A[%block, %elem] : memref<8x8xf64>
          %nextElem = arith.remui %c16, %c8 : index
          memref.store %prev, %A[%block, %nextElem] : memref<8x8xf64>
        }
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [8], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}]}
    return
  }
}
