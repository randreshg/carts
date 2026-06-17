// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not='cannot map SDE access-window block coordinate'

// Unit stencil offsets may reach the access-window mapper through frontend
// integer casts. ARTS should map the owner block coordinate back to the SDE
// loop dimension while SDE's committed access offsets carry the halo extent.

// CHECK-LABEL: func.func @casted_unit_offset_read_window
// CHECK: arts.edt
// CHECK: arts.db_acquire[<in>]

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @casted_unit_offset_read_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c15 = arith.constant 15 : index
    %c16 = arith.constant 16 : index
    %c1_i32 = arith.constant 1 : i32
    %zero = arith.constant 0.0 : f32
    %P = sde.mu_alloc {arrayId = 0 : i64} : memref<2x8xf32>
    %Q = sde.mu_alloc {arrayId = 1 : i64} : memref<2x8xf32>

    sde.su_iterate (%c0) to (%c16) step (%c1) {
    ^bb0(%i: index):
      sde.array_layout_root write %P : memref<2x8xf32> array_id(0)
      sde.array_layout_root write %Q : memref<2x8xf32> array_id(1)
      sde.cu_region <parallel> {
        %bi = arith.divui %i, %c8 : index
        %li = arith.remui %i, %c8 : index
        memref.store %zero, %P[%bi, %li] : memref<2x8xf32>
        memref.store %zero, %Q[%bi, %li] : memref<2x8xf32>
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [8], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "write"},
      {arrayId = 1 : i64, blockShape = [8], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}

    sde.su_iterate (%c1) to (%c15) step (%c1) classification(<stencil>) {
    ^bb0(%i: index):
      sde.array_layout_root read %P : memref<2x8xf32> array_id(0)
      sde.array_layout_root write %Q : memref<2x8xf32> array_id(1)
      sde.cu_region <parallel> {
        %i32 = arith.index_cast %i : index to i32
        %ip1_i32 = arith.addi %i32, %c1_i32 : i32
        %ip1 = arith.index_cast %ip1_i32 : i32 to index
        %rb = arith.divui %ip1, %c8 : index
        %rl = arith.remui %ip1, %c8 : index
        %value = memref.load %P[%rb, %rl] : memref<2x8xf32>
        %wb = arith.divui %i, %c8 : index
        %wl = arith.remui %i, %c8 : index
        %out = arith.addf %value, %zero : f32
        memref.store %out, %Q[%wb, %wl] : memref<2x8xf32>
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [8], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "read"},
      {arrayId = 1 : i64, blockShape = [8], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [0], role = "write"}],
       accessMinOffsets = [-1], accessMaxOffsets = [1],
       ownerDims = [0], spatialDims = [0], writeFootprint = [0]}
    return
  }
}
