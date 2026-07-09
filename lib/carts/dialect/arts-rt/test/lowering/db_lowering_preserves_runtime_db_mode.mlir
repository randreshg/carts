// RUN: %carts-compile %s --pass-pipeline='builtin.module(db-lowering)' | %FileCheck %s

// runtime_db_mode is the committed RO/EW/RW verdict ARTS-RT consumes. DB lowering
// rebuilds the acquire in opaque-pointer form and must preserve that verdict
// rather than dropping it for a later pass to re-derive.

// CHECK-LABEL: func.func @db_lowering_keeps_runtime_db_mode
// CHECK: arts.db_alloc
// CHECK-SAME: block_layout = #arts.block_layout<
// CHECK-SAME: db_placement = #arts.db_placement<distributed>
// CHECK: arts.db_acquire
// CHECK-SAME: block_layout = #arts.block_layout<
// CHECK-SAME: runtime_db_mode = #arts.runtime_db_mode<ew>
// CHECK-SAME: memref<?x!llvm.ptr>

module attributes {
  arts.runtime_total_nodes = 4 : i64,
  arts.runtime_total_workers = 16 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @db_lowering_keeps_runtime_db_mode() {
    %route = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {
      distributed,
      db_placement = #arts.db_placement<distributed>,
      block_layout = #arts.block_layout<
        owner_dims = [0],
        block_shape = [1],
        distribution_kind = <block>>
    } : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {
      block_layout = #arts.block_layout<
        owner_dims = [0],
        block_shape = [1],
        distribution_kind = <block>>,
      runtime_db_mode = #arts.runtime_db_mode<ew>
    } -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
