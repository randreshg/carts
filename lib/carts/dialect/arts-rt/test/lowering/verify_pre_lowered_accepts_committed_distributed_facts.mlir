// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-lowering,db-distributed-runtime-init,verify-pre-lowered)' | %FileCheck %s

// When a distributed DB grid and runtime DB mode are present, the gate passes
// only after pre-lowering has emitted explicit runtime init callbacks.

// CHECK-LABEL: func.func @distributed_grid_committed
// CHECK: arts.db_alloc
// CHECK-SAME: distributed
// CHECK: arts.db_acquire
// CHECK-SAME: runtime_db_mode = #arts.runtime_db_mode<ew>

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @distributed_grid_committed() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
