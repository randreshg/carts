// RUN: %carts-compile %s --pass-pipeline='builtin.module(realize-edt-distribution-plan)' | %FileCheck %s

// ARTS realizes a per-block halo EDT only when the stencil read dependency
// already carries explicit element-window operands. ARTS-RT consumes that
// committed fact; it must not infer halo byte windows from stencil metadata.

// CHECK-LABEL: func.func @marks_committed_per_block_halo
// CHECK: arts.db_acquire[<in>]
// CHECK-NOT: haloViewDependency
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: element_offsets
// CHECK-SAME: haloViewDependency
// CHECK: arts.db_acquire[<out>]
// CHECK-NOT: haloViewDependency
// CHECK: arts.edt
// CHECK-SAME: distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-SAME: distribution_version = 1 : i32
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: stencil_supported_block_halo

module {
  func.func @marks_committed_per_block_halo() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %value = arith.constant 0.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %normal_guid, %normal_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %halo_guid, %halo_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] element_offsets[%c0] element_sizes[%c16] {depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_supported_block_halo} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {depPattern = #arts.dep_pattern<stencil>, distribution_pattern = #arts.distribution_pattern<stencil>, stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_supported_block_halo} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%normal_ptr, %halo_ptr, %write_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {depPattern = #arts.dep_pattern<stencil>, planHaloShape = [1], stencil_max_offsets = [1], stencil_min_offsets = [-1]} {
    ^bb0(%normal: memref<?xmemref<?xf64>>, %halo: memref<?xmemref<?xf64>>, %write: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %write[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%normal) : memref<?xmemref<?xf64>>
      arts.db_release(%halo) : memref<?xmemref<?xf64>>
      arts.db_release(%write) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%normal_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%halo_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%write_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
