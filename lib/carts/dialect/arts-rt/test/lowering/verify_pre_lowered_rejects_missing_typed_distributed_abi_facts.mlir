// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-pre-lowered)' 2>&1 | %FileCheck %s

// A legacy distributed marker without the typed placement/layout contract must
// not reach ARTS-RT ABI lowering.

// CHECK: distributed DB reached ABI lowering without typed db_placement
// CHECK: distributed DB reached ABI lowering without typed block_layout

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @missing_typed_distributed_abi_facts() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
        {distributed}
        : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
