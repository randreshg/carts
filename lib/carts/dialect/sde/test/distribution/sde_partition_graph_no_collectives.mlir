// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline sde-to-codir \
// RUN:   --min-distributed-tile-bytes=4194304 \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s

// SDE owns the CU/MU graph partition as geometry, abstract cost, and
// concurrency evidence. It must not name collectives; CODIR is the first layer
// allowed to choose all_gather/reduce_scatter/etc.

// CHECK-LABEL: // -----// IR Dump After DistributionPlanning (distribution-planning) //----- //
// CHECK: func.func @partition_graph_direct_row_matmul
// CHECK: iterationTopology = #sde.iteration_topology<owner_strip>
// CHECK-SAME: logicalWorkerSlice = [32, 1024]
// CHECK-SAME: partitionGraph = [
// CHECK-SAME: blockShape = [32, 1024]
// CHECK-SAME: cuGroupCount = 16 : i64
// CHECK-SAME: cuGroupSize = 2 : i64
// CHECK-SAME: edgeClass = "aligned"
// CHECK-SAME: layoutKind = "owner_block"
// CHECK-SAME: muBlockCount = 32 : i64
// CHECK-SAME: ownerDims = [0]
// CHECK-NOT: all_gather
// CHECK-NOT: reduce_scatter
// CHECK-NOT: allreduce
// CHECK-NOT: broadcast
// CHECK: partitionScore = {
// CHECK-SAME: chosenCuCount = 32 : i64
// CHECK-SAME: chosenTileBytes = 262144 : i64
// CHECK-SAME: cuGroupCount = 16 : i64
// CHECK-SAME: cuGroupSize = 2 : i64
// CHECK-SAME: exposedCuCount = 16 : i64
// CHECK-SAME: minTileBytes = 4194304 : i64
// CHECK-SAME: muBlockCount = 32 : i64
// CHECK-SAME: objective = "max_concurrency_comm_aware"
// CHECK-SAME: targetLogicalWorkers = 16 : i64
// CHECK-SAME: physicalOwnerDims = [0]

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @partition_graph_direct_row_matmul(%A: memref<1024x1024xf64>, %B: memref<1024x1024xf64>, %C: memref<1024x1024xf64>) {
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
