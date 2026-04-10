// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --pipeline=create-dbs | %FileCheck %s

// CHECK-LABEL: func.func @current_node_route_defaults_create_dbs
// CHECK: %[[CUR:.*]] = arith.constant -1 : i32
// CHECK: arts.db_alloc
// CHECK-SAME: route(%[[CUR]] : i32)
// CHECK: arts.edt <parallel> <internode> route(%[[CUR]])

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func @current_node_route_defaults_create_dbs() -> i32 {
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %buf = memref.alloc() : memref<4xi32>
    omp.parallel {
      memref.store %c1_i32, %buf[%c0] : memref<4xi32>
      omp.terminator
    }
    %value = memref.load %buf[%c0] : memref<4xi32>
    memref.dealloc %buf : memref<4xi32>
    return %value : i32
  }
}
