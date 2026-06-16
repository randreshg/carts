// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --implicit-check-not="names more physical owner dimensions than loop dimensions" --implicit-check-not=arts.db_access_window

// A rank-expanded MU type may contain more physical owner slots than the SU
// loop uses after SDE committed a narrower explicit write layout. ARTS must
// consume the explicit SDE write fact instead of recovering a broader shape from
// the type and rejecting the owner-slot mapping.

// CHECK-LABEL: func.func @prefers_explicit_write_layout
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: elementSizes[%c1{{(_[0-9]+)?}}, %c2{{(_[0-9]+)?}}, %c64{{(_[0-9]+)?}}, %c1024{{(_[0-9]+)?}}]
// CHECK: arts.edt

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @prefers_explicit_write_layout() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c64 = arith.constant 64 : index
    %c1024 = arith.constant 1024 : index
    %zero = arith.constant 0.0 : f32
    %A = sde.mu_alloc : memref<2x2x64x1024xf32>

    sde.su_iterate (%c0) to (%c2) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<2x2x64x1024xf32> array_id(0)
      sde.cu_region <parallel> {
        memref.store %zero, %A[%i, %c0, %c0, %c0] : memref<2x2x64x1024xf32>
        sde.yield
      } {groupBlockCount = [2]}
      sde.yield
    } {arrayLayout = [{arrayId = 0 : i64, blockShape = [2, 64, 1024], kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}]}
    return
  }
}
