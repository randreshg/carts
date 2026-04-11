// RUN: %carts-compile %s --start-from arts-to-llvm --pipeline arts-to-llvm --arts-config %S/../inputs/arts_2t.cfg | %FileCheck %s

// Verify that the atomic combine artifact consumed downstream by the
// reduction-strategy path lowers to an LLVM atomic RMW.

// CHECK-LABEL: func.func @main
// CHECK: llvm.atomicrmw add

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main() {
    %c0 = arith.constant 0 : index
    %c7 = arith.constant 7 : i32
    %zero = arith.constant 0 : i32
    %sum = memref.alloca() : memref<1xi32>
    memref.store %zero, %sum[%c0] : memref<1xi32>
    %sum_cast = memref.cast %sum : memref<1xi32> to memref<?xi32>
    arts.atomic_add(%sum_cast, %c7 : memref<?xi32>, i32)
    return
  }
}
