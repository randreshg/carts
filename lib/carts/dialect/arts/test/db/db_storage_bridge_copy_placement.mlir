// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @copy_out_sinks_to_host_observation() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %value = arith.constant 1.0 : f64
    %true = arith.constant true

    %host_guid, %host_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1, %c4] {local_only} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %host = arts.db_ref %host_ptr[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
    %block_guid, %block_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c1, %c4] {planOwnerDims = [0], planPhysicalBlockShape = [1, 4], storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)

    scf.for %t = %c0 to %c2 step %c1 {
      %writer_guid, %writer_ptr = arts.db_acquire[<out>] (%block_guid : memref<?xi64>, %block_ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
      arts.edt <task> <intranode> route(%route) (%writer_ptr) : memref<?xmemref<?x?xf64>> {
      ^bb0(%block: memref<?xmemref<?x?xf64>>):
        %payload = arts.db_ref %block[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
        memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
        arts.yield
      }
      arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}

      scf.for %i = %c0 to %c1 step %c1 {
        %host_acq_guid, %host_acq_ptr = arts.db_acquire[<inout>] (%host_guid : memref<?xi64>, %host_ptr : memref<?xmemref<?x?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
        %block_acq_guid, %block_acq_ptr = arts.db_acquire[<in>] (%block_guid : memref<?xi64>, %block_ptr : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
        arts.edt <task> <intranode> route(%route) (%host_acq_ptr, %block_acq_ptr) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> attributes {storageBridgeCopy} {
        ^bb0(%host_dep: memref<?xmemref<?x?xf64>>, %block_dep: memref<?xmemref<?x?xf64>>):
          %host_payload = arts.db_ref %host_dep[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          %block_payload = arts.db_ref %block_dep[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
          %loaded = memref.load %block_payload[%c0, %c0] : memref<?x?xf64>
          memref.store %loaded, %host_payload[%c0, %c0] : memref<?x?xf64>
          arts.yield
        }
      }
      arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    }

    func.call @timer_stop() : () -> ()
    scf.if %true {
      %observed = memref.load %host[%c0, %c0] : memref<?x?xf64>
      func.call @use(%observed) : (f64) -> ()
    }
    return
  }

  func.func private @timer_stop() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @use(f64) attributes {llvm.linkage = #llvm.linkage<external>}
}

// CHECK-LABEL: func.func @copy_out_sinks_to_host_observation
// CHECK: scf.for
// CHECK-NOT: storageBridgeCopy
// CHECK: call @timer_stop
// CHECK: storageBridgeCopy
// CHECK: memref.load
// CHECK: call @use
