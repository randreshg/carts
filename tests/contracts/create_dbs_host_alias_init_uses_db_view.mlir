// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --pipeline=create-dbs | %FileCheck %s

// CHECK-LABEL: func.func @host_alias_init_uses_db_view
// CHECK: %[[GUID:.*]], %[[PTR:.*]] = arts.db_alloc[<in>
// CHECK: %[[VIEW:.*]] = arts.db_ref %[[PTR]][%{{.*}}] : memref<?xmemref<?xf32>> -> memref<?xf32>
// CHECK: call @init_data(%[[VIEW]], %{{.*}}) : (memref<?xf32>, i32) -> ()

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu", "polygeist.target-cpu" = "generic", "polygeist.target-features" = "+fp-armv8,+neon,+outline-atomics,+v8a,-fmv"} {
  func.func private @init_data(memref<?xf32>, i32)

  func.func @host_alias_init_uses_db_view() -> f32 {
    %c0 = arith.constant 0 : index
    %c8_i32 = arith.constant 8 : i32
    %buf = memref.alloc() : memref<8xf32>
    %out = memref.alloc() : memref<8xf32>
    %buf_dyn = memref.cast %buf : memref<8xf32> to memref<?xf32>
    call @init_data(%buf_dyn, %c8_i32) : (memref<?xf32>, i32) -> ()
    omp.parallel {
      %value = memref.load %buf[%c0] : memref<8xf32>
      memref.store %value, %out[%c0] : memref<8xf32>
      omp.terminator
    }
    %result = memref.load %out[%c0] : memref<8xf32>
    memref.dealloc %out : memref<8xf32>
    memref.dealloc %buf : memref<8xf32>
    return %result : f32
  }
}
