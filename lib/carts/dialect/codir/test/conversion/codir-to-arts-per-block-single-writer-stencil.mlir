// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db | %FileCheck %s

// `halo` lowers to a distributed per-block DB plus owner-routed neighbor
// exchange EDTs.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @per_block_single_writer_stencil() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %A = memref.alloc() : memref<24x4xf32>

    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%A : memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      emit_block_native_stencil,
                      halo_shape = [1, 0],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c23 = arith.constant 23 : index
        %cst = arith.constant 2.000000e-01 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c23 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c1 to %inner_c4 step %inner_c1 {
            %up = arith.subi %row, %inner_c1 : index
            %dn = arith.addi %row, %inner_c1 : index
            %a0 = memref.load %arg0[%row, %col] : memref<24x4xf32>
            %a1 = memref.load %arg0[%up, %col] : memref<24x4xf32>
            %a2 = memref.load %arg0[%dn, %col] : memref<24x4xf32>
            %s0 = arith.addf %a0, %a1 : f32
            %s1 = arith.addf %s0, %a2 : f32
            %r = arith.mulf %s1, %cst : f32
            memref.store %r, %arg0[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    %resultA = memref.load %A[%c0, %c0] : memref<24x4xf32>
    func.call @use(%resultA) : (f32) -> ()
    memref.dealloc %A : memref<24x4xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @per_block_single_writer_stencil
// CHECK-DAG: %[[C0:.*]] = arith.constant 0 : index
// CHECK-DAG: %[[C1:.*]] = arith.constant 1 : index
// CHECK-DAG: %[[C8:.*]] = arith.constant 8 : index
// CHECK-DAG: %[[C9:.*]] = arith.constant 9 : index
// CHECK-DAG: %[[C10:.*]] = arith.constant 10 : index
// CHECK-DAG: %[[C4:.*]] = arith.constant 4 : index

// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: elementSizes[%[[C10]], %[[C4]]]
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planHaloShape = [1, 0]
// CHECK-SAME: stencil_supported_block_halo

// CHECK: scf.for
// CHECK: arts.db_acquire[<out>] {{.*}} partitioning(<block>)
// CHECK: %[[LOWER_OK:.*]] = arith.cmpi ugt
// CHECK: %[[UPPER_OK:.*]] = arith.cmpi ult
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}bounds_valid(%[[LOWER_OK]])
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}bounds_valid(%[[UPPER_OK]])
// CHECK: %[[TOTAL_NODES:.*]] = arts.runtime_query <total_nodes> -> i32
// CHECK: %[[NODES_IDX:.*]] = arith.index_cast %[[TOTAL_NODES]] : i32 to index
// CHECK: %[[SCALED:.*]] = arith.muli %{{.*}}, %[[NODES_IDX]] : index
// CHECK: %[[ROUTE_IDX:.*]] = arith.divui %[[SCALED]], %{{.*}} : index
// CHECK: %[[ROUTE:.*]] = arith.index_cast %[[ROUTE_IDX]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE]])
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: scf.if
// CHECK: memref.load %{{.*}}[%[[C8]], %{{.*}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[C0]], %{{.*}}]
// CHECK: scf.if
// CHECK: memref.load %{{.*}}[%[[C1]], %{{.*}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[C9]], %{{.*}}]
// CHECK: arts.barrier

// CHECK: arts.edt <task> <internode> route{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK: %[[COMPUTED:.*]] = arith.mulf
// CHECK: %[[STORE_ORIGIN_OK:.*]] = arith.cmpi uge
// CHECK: %[[STORE_ORIGIN_RAW:.*]] = arith.subi
// CHECK: %[[STORE_ORIGIN:.*]] = arith.select %[[STORE_ORIGIN_OK]], %[[STORE_ORIGIN_RAW]], %[[C0]]
// CHECK: %[[STORE_ROW:.*]] = arith.subi %{{.*}}, %[[STORE_ORIGIN]]
// CHECK: memref.store %[[COMPUTED]], %{{.*}}[%[[STORE_ROW]], %{{.*}}]

// func.func private @use
