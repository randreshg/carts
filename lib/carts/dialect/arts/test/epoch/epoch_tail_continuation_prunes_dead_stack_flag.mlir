// RUN: %carts-compile %s --pass-pipeline='builtin.module(arts-epoch-tail-continuation)' | %FileCheck %s

// Dead final writes to stack-local flags do not block continuation outlining,
// but the stack alloca itself is not captured into the async tail.

// CHECK-LABEL: func.func @main
// CHECK: arts.epoch
// CHECK: arts.db_acquire[<in>]
// CHECK: arts.edt
// CHECK-NOT: memref.store %true
// CHECK: func.call @checksum
// CHECK: arts.shutdown
// CHECK: return %{{.*}} : i32

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:32-i64:64-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @main() -> i32 {
    %route = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.0 : f64
    %true = arith.constant true
    %flag = memref.alloca() : memref<i1>
    %sum = memref.alloca() : memref<f64>
    memref.store %zero, %sum[] : memref<f64>

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c8, %c8] {local_only} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %payload = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>

    arts.epoch {
      arts.yield
    } : i64

    memref.store %true, %flag[] : memref<i1>
    memref.store %zero, %sum[] : memref<f64>
    scf.for %i = %c0 to %c8 step %c1 {
      %v = memref.load %payload[%i, %i] : memref<?x?xf64>
      %old = memref.load %sum[] : memref<f64>
      %next = arith.addf %old, %v : f64
      memref.store %next, %sum[] : memref<f64>
    }
    %result = memref.load %sum[] : memref<f64>
    func.call @checksum(%result) : (f64) -> ()
    return %c0_i32 : i32
  }

  func.func private @checksum(f64)
}
