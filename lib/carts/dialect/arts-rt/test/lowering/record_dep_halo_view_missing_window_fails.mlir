// RUN: not %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: 'arts_rt.rec_dep' op HALO_VIEW dependency #0 requires byte_offsets and byte_sizes

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @record_dep_halo_view_missing_window_fails(%edt: i64, %db: i64) {
    %c0 = arith.constant 0 : index
    %storage = memref.alloca() : memref<1xi64>
    memref.store %db, %storage[%c0] : memref<1xi64>
    arts_rt.rec_dep %edt(%storage : memref<1xi64>) {acquire_modes = array<i32: 1>, dep_flags = array<i32: 4>}
    return
  }
}
