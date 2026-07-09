// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-pre-lowered)' 2>&1 | %FileCheck %s

// Typed ABI facts are authoritative, but ARTS-RT still verifies that they are
// self-consistent with the DB handle rank before lowering.

// CHECK: typed block_layout owner_dims must address the DB block grid

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @inconsistent_typed_block_layout() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
        {
          db_placement = #arts.db_placement<distributed>,
          block_layout = #arts.block_layout<
            owner_dims = [1],
            block_shape = [1],
            distribution_kind = <block>>
        }
        : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
