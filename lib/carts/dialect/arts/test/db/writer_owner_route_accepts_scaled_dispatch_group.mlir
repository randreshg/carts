// RUN: %carts-compile %s --pass-pipeline='builtin.module(writer-owner-route)' | %FileCheck %s --implicit-check-not="writes a distributed DB range that may span multiple owners"

// WriterOwnerRoute must recognize owner-local grouped dispatch when a 1-D DB
// block offset is reconstructed as blockStart = (iv / physicalBlockSize) *
// physicalBlockSize. This is the shape produced by SDE-to-ARTS for row-grain
// writers co-dispatched with rank-expanded block writers.

// CHECK-LABEL: func.func @scaled_dispatch_group_is_owner_local
// CHECK: scf.for
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: offsets[%{{[A-Za-z0-9_]+}}]
// CHECK-SAME: sizes[%{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task> <internode> route
// CHECK-LABEL: func.func @zero_clamped_scaled_dispatch_group_is_owner_local
// CHECK: scf.for
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: offsets[%{{[A-Za-z0-9_]+}}]
// CHECK-SAME: sizes[%{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task> <internode> route
// CHECK-LABEL: func.func @clamped_projected_dispatch_group_is_owner_local
// CHECK: scf.for
// CHECK: scf.for
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: offsets[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
// CHECK-SAME: sizes[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task> <internode> route

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @scaled_dispatch_group_is_owner_local() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c128 = arith.constant 128 : index
    %c256 = arith.constant 256 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 0.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c256] elementType(f64) elementSizes[%c1, %c256] {distributed} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    scf.for %i = %c0 to %c256 step %c128 {
      %block = arith.divui %i, %c128 : index
      %offset = arith.muli %block, %c128 : index
      %nextBlock = arith.addi %block, %c1 : index
      %blockEnd = arith.muli %nextBlock, %c128 : index
      %clampedEnd = arith.minui %blockEnd, %c256 : index
      %remaining = arith.subi %c256, %offset : index
      %span = arith.subi %clampedEnd, %offset : index
      %boundedSpan = arith.minui %span, %c128 : index
      %size = arith.minui %remaining, %boundedSpan : index
      %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%offset], sizes[%size] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
      arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?x?xf64>> {
      ^bb0(%out: memref<?xmemref<?x?xf64>>):
        %payload = arts.db_ref %out[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
        arts.db_release(%out) : memref<?xmemref<?x?xf64>>
        arts.yield
      }
      arts.db_release(%write_ptr) : memref<?xmemref<?x?xf64>>
    }
    return
  }

  func.func @zero_clamped_scaled_dispatch_group_is_owner_local() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c32 = arith.constant 32 : index
    %c256 = arith.constant 256 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 0.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c256] elementType(f64) elementSizes[%c1, %c256] {distributed} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    scf.for %i = %c0 to %c256 step %c32 {
      %block = arith.divui %i, %c32 : index
      %blockStart = arith.muli %block, %c32 : index
      %nextBlock = arith.addi %block, %c1 : index
      %blockEnd = arith.muli %nextBlock, %c32 : index
      %clampedEnd = arith.minui %blockEnd, %c256 : index
      %offset = arith.maxsi %blockStart, %c0 : index
      %end = arith.maxsi %clampedEnd, %c0 : index
      %remaining = arith.subi %c256, %offset : index
      %span = arith.subi %end, %offset : index
      %boundedSpan = arith.minui %span, %c32 : index
      %size = arith.minui %remaining, %boundedSpan : index
      %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%offset], sizes[%size] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
      arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?x?xf64>> {
      ^bb0(%out: memref<?xmemref<?x?xf64>>):
        %payload = arts.db_ref %out[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
        arts.db_release(%out) : memref<?xmemref<?x?xf64>>
        arts.yield
      }
      arts.db_release(%write_ptr) : memref<?xmemref<?x?xf64>>
    }
    return
  }

  func.func @clamped_projected_dispatch_group_is_owner_local() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5 = arith.constant 5 : index
    %c20 = arith.constant 20 : index
    %c512 = arith.constant 512 : index
    %c2560 = arith.constant 2560 : index
    %c10240 = arith.constant 10240 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 0.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c20, %c20] elementType(f64) elementSizes[%c1, %c1, %c512, %c512] {distributed} : (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf64>>)
    scf.for %iElem = %c0 to %c10240 step %c2560 {
      scf.for %jElem = %c0 to %c10240 step %c2560 {
        %iBlockGroup = arith.divui %iElem, %c2560 : index
        %jBlockGroup = arith.divui %jElem, %c2560 : index
        %iStartElem = arith.muli %iBlockGroup, %c2560 : index
        %jStartElem = arith.muli %jBlockGroup, %c2560 : index
        %iClamped = arith.maxsi %iStartElem, %c0 : index
        %jClamped = arith.maxsi %jStartElem, %c0 : index
        %iOffset = arith.divui %iClamped, %c512 : index
        %jOffset = arith.divui %jClamped, %c512 : index
        %iRemaining = arith.subi %c20, %iOffset : index
        %jRemaining = arith.subi %c20, %jOffset : index
        %iSize = arith.minui %iRemaining, %c5 : index
        %jSize = arith.minui %jRemaining, %c5 : index
        %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?x?xi64>, %ptr : memref<?x?xmemref<?x?x?x?xf64>>) partitioning(<block>), indices[], offsets[%iOffset, %jOffset], sizes[%iSize, %jSize] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf64>>)
        arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?x?xmemref<?x?x?x?xf64>> {
        ^bb0(%out: memref<?x?xmemref<?x?x?x?xf64>>):
          %payload = arts.db_ref %out[%c0, %c0] : memref<?x?xmemref<?x?x?x?xf64>> -> memref<?x?x?x?xf64>
          memref.store %value, %payload[%c0, %c0, %c0, %c0] : memref<?x?x?x?xf64>
          arts.db_release(%out) : memref<?x?xmemref<?x?x?x?xf64>>
          arts.yield
        }
        arts.db_release(%write_ptr) : memref<?x?xmemref<?x?x?x?xf64>>
      }
    }
    return
  }
}
