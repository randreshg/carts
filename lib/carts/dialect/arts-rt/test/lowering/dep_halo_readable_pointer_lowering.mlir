// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm \
// RUN:   | %FileCheck %s

// Depv reads must match arts_dep_halo_ptr semantics. Compact remote halos read
// the depv ptr directly; local borrowed halo views read depv.ptr + slice_offset.
// Pointer-table consumers get a temporary one-slot table for the adjusted local
// borrowed pointer without mutating depv.

// CHECK-LABEL: func.func @dep_gep_halo_readable_slot
// CHECK: llvm.getelementptr %arg0[0, 3]
// CHECK: llvm.getelementptr %arg0[0, 4]
// CHECK: llvm.load
// CHECK: llvm.load
// CHECK: arith.andi {{.*}}, %c4_i32 : i32
// CHECK: arith.andi {{.*}}, %c8_i32 : i32
// CHECK: llvm.getelementptr {{.*}}[{{.*}}] : (!llvm.ptr, i64) -> !llvm.ptr, i8
// CHECK: memref.alloca() : memref<1x!llvm.ptr>
// CHECK: polygeist.memref2pointer
// CHECK: llvm.select
// CHECK: return

// CHECK-LABEL: func.func @dep_db_acquire_halo_pointer_table
// CHECK: llvm.getelementptr %arg0[0, 3]
// CHECK: llvm.getelementptr %arg0[0, 4]
// CHECK: arith.andi {{.*}}, %c4_i32 : i32
// CHECK: arith.andi {{.*}}, %c8_i32 : i32
// CHECK: llvm.getelementptr {{.*}}[{{.*}}] : (!llvm.ptr, i64) -> !llvm.ptr, i8
// CHECK: memref.alloca() : memref<1x!llvm.ptr>
// CHECK: polygeist.memref2pointer
// CHECK: llvm.select
// CHECK: polygeist.pointer2memref {{.*}} : !llvm.ptr to memref<?x!llvm.ptr>

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @dep_gep_halo_readable_slot(%depv: !llvm.ptr) -> (!llvm.ptr, !llvm.ptr) {
    %c0 = arith.constant 0 : index
    %guid, %ptr = arts_rt.dep_gep(%depv) offset[%c0 : index] : !llvm.ptr -> !llvm.ptr, !llvm.ptr
    return %guid, %ptr : !llvm.ptr, !llvm.ptr
  }

  func.func @dep_db_acquire_halo_pointer_table(%depv: !llvm.ptr) -> (memref<?xi64>, memref<?x!llvm.ptr>) {
    %c0 = arith.constant 0 : index
    %guid, %ptr = arts_rt.dep_db_acquire(%depv) offset[%c0 : index] : !llvm.ptr -> memref<?xi64>, memref<?x!llvm.ptr>
    return %guid, %ptr : memref<?xi64>, memref<?x!llvm.ptr>
  }
}
