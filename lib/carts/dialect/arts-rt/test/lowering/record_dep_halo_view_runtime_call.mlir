// RUN: %carts-compile %s --arts-config %inputs_dir/arts_multinode.cfg --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm \
// RUN:   | %FileCheck %s
// RUN: %carts-compile %s --arts-config %inputs_dir/arts_1t.cfg --start-from arts-rt-to-llvm --pipeline arts-rt-to-llvm \
// RUN:   | %FileCheck %s

// rec_dep slots marked HALO_VIEW plus explicit byte windows lower to the
// halo-view runtime API in both multinode and single-node configs. Extra
// non-halo flags select the _ex variant. Neither path may use the legacy
// DB_MODE_PTR slice API.

// CHECK-LABEL: func.func @record_dep_halo_view_runtime_call
// CHECK: {{func[.]call|call}} @arts_add_halo_dependence(
// CHECK-NOT: {{func[.]call|call}} @arts_add_dependence_at
// CHECK-LABEL: func.func @record_dep_halo_view_runtime_call_ex
// CHECK: {{func[.]call|call}} @arts_add_halo_dependence_ex(
// CHECK-NOT: {{func[.]call|call}} @arts_add_dependence_at

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @record_dep_halo_view_runtime_call(%edt: i64, %db: i64) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %storage = memref.alloca() : memref<1xi64>
    memref.store %db, %storage[%c0] : memref<1xi64>
    arts_rt.rec_dep %edt(%storage : memref<1xi64>) byte_offsets(%c0) byte_sizes(%c8) {acquire_modes = array<i32: 1>, dep_flags = array<i32: 4>}
    return
  }

  func.func @record_dep_halo_view_runtime_call_ex(%edt: i64, %db: i64) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %storage = memref.alloca() : memref<1xi64>
    memref.store %db, %storage[%c0] : memref<1xi64>
    arts_rt.rec_dep %edt(%storage : memref<1xi64>) byte_offsets(%c0) byte_sizes(%c8) {acquire_modes = array<i32: 1>, dep_flags = array<i32: 5>}
    return
  }
}
