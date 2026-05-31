// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --min-distributed-tile-bytes=4194304 \
// RUN:   | %FileCheck %s --implicit-check-not=partitionGraph \
// RUN:       --implicit-check-not=partitionScore --implicit-check-not=all_gather \
// RUN:       --implicit-check-not=reduce_scatter --implicit-check-not=allreduce \
// RUN:       --implicit-check-not=broadcast

// CODIR forwards SDE's neutral CU/MU graph evidence as generic attrs. The graph
// payload is still layout/cost/concurrency only; storage and collective choices
// remain CODIR decisions after this boundary.

// CHECK-LABEL: func.func @sde_partition_graph_to_codir
// CHECK: codir.codelet
// CHECK-SAME: partition_graph = [
// CHECK-SAME: layoutKind = "block_parallel"
// CHECK-SAME: role = "write"
// CHECK-SAME: role = "read"
// CHECK-SAME: partition_score = {
// CHECK-SAME: chosenCuCount = 16 : i64
// CHECK-SAME: chosenTileBytes = 524288 : i64
// CHECK-SAME: objective = "max_concurrency_comm_aware"
// CHECK-SAME: targetLogicalWorkers = 16 : i64
// CHECK-SAME: tile_owner_dims = [0]
// CHECK-SAME: tile_shape = [64, 1024]

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @sde_partition_graph_to_codir(%A: memref<1024x1024xf64>, %B: memref<1024x1024xf64>, %C: memref<1024x1024xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1024 = arith.constant 1024 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%c1024) step (%c1) {
          scf.for %j = %c0 to %c1024 step %c1 {
            %zero = arith.constant 0.000000e+00 : f64
            memref.store %zero, %C[%i, %j] : memref<1024x1024xf64>
            scf.for %k = %c0 to %c1024 step %c1 {
              %a = memref.load %A[%i, %k] : memref<1024x1024xf64>
              %b = memref.load %B[%k, %j] : memref<1024x1024xf64>
              %old = memref.load %C[%i, %j] : memref<1024x1024xf64>
              %prod = arith.mulf %a, %b : f64
              %next = arith.addf %old, %prod : f64
              memref.store %next, %C[%i, %j] : memref<1024x1024xf64>
            }
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
