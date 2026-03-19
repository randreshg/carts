// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=arts-to-llvm --pipeline=arts-to-llvm | %FileCheck %s

// CHECK-LABEL: func.func @hoist_runtime_queries(
// CHECK: %[[W:.*]] = {{(func[.])?call}} @arts_get_total_workers() : () -> i32
// CHECK: %[[N:.*]] = {{(func[.])?call}} @arts_get_total_nodes() : () -> i32
// CHECK: %[[FOR:.*]] = scf.for
// CHECK: %[[WN:.*]] = arith.addi %[[W]], %[[N]] : i32
// CHECK: %[[ACC2:.*]] = arith.addi %{{.*}}, %[[WN]] : i32

// CHECK-LABEL: func.func @dedup_runtime_queries()
// CHECK: %[[W0:.*]] = {{(func[.])?call}} @arts_get_total_workers() : () -> i32
// CHECK-NOT: {{(func[.])?call}} @arts_get_total_workers
// CHECK: %[[SUM:.*]] = arith.addi %[[W0]], %[[W0]] : i32
// CHECK: return %[[SUM]] : i32

// CHECK-LABEL: func.func @do_not_optimize_guid(
// CHECK: %[[G0:.*]] = {{(func[.])?call}} @arts_guid_reserve(%{{.*}}, %arg0) : (i32, i32) -> i64
// CHECK: %[[G1:.*]] = {{(func[.])?call}} @arts_guid_reserve(%{{.*}}, %arg0) : (i32, i32) -> i64

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func private @arts_get_total_workers() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @arts_get_total_nodes() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @arts_guid_reserve(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}

  func.func @hoist_runtime_queries(%N: index) -> i32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %zero = arith.constant 0 : i32
    %sum = scf.for %i = %c0 to %N step %c1 iter_args(%acc = %zero) -> (i32) {
      %w = func.call @arts_get_total_workers() : () -> i32
      %n = func.call @arts_get_total_nodes() : () -> i32
      %wn = arith.addi %w, %n : i32
      %acc2 = arith.addi %acc, %wn : i32
      scf.yield %acc2 : i32
    }
    return %sum : i32
  }

  func.func @dedup_runtime_queries() -> i32 {
    %a = func.call @arts_get_total_workers() : () -> i32
    %b = func.call @arts_get_total_workers() : () -> i32
    %sum = arith.addi %a, %b : i32
    return %sum : i32
  }

  func.func @do_not_optimize_guid(%route: i32) -> i64 {
    %t = arith.constant 6 : i32
    %g0 = func.call @arts_guid_reserve(%t, %route) : (i32, i32) -> i64
    %g1 = func.call @arts_guid_reserve(%t, %route) : (i32, i32) -> i64
    %sum = arith.addi %g0, %g1 : i64
    return %sum : i64
  }
}
