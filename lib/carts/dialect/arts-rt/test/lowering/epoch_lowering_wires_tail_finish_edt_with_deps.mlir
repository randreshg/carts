// RUN: %carts-compile %s --pass-pipeline='builtin.module(epoch-lowering)' | %FileCheck %s --implicit-check-not=arts_rt.wait_on_epoch

// A continuation EDT with ordinary DB deps reserves one extra dep slot for the
// epoch finish signal. ARTS-RT may move only the continuation create/setup
// before the epoch; dependency recording stays after the epoch body has been
// launched.

// CHECK-LABEL: func.func @main
// CHECK: %[[C1:.*]] = arith.constant 1 : i32
// CHECK: %[[PACK:.*]] = arts_rt.edt_param_pack
// CHECK: %[[ONE:.*]] = arith.constant 1 : i32
// CHECK: %[[DEP_COUNT:.*]] = arith.addi %[[C1]], %[[ONE]] : i32
// CHECK: %[[CONT:.*]] = arts_rt.edt_create(%[[PACK]] : memref<1xi64>) depCount(%[[DEP_COUNT]]) route
// CHECK: %[[EPOCH:.*]] = arts_rt.create_epoch finish(%[[CONT]] : i64, %[[C1]] : i32) : i64
// CHECK: %[[TASK:.*]] = arts_rt.edt_create
// CHECK-SAME: epoch(%[[EPOCH]] : i64)
// CHECK: arts_rt.rec_dep %[[TASK]]
// CHECK: arts.db_acquire
// CHECK: arts_rt.rec_dep %[[CONT]]
// CHECK: return

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64,
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:32-i64:64-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @main() {
    %route = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %true = arith.constant true

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.epoch {
      %task_pack = arts_rt.edt_param_pack(%c0_i32) : i32 : memref<1xi64>
      %task = arts_rt.edt_create(%task_pack : memref<1xi64>) depCount(%c0_i32) route(%route) {arts.outlined_func = "__task"}
      arts_rt.rec_dep %task(%guid : memref<?xi64>) bounds_valids(%true) {acquire_modes = array<i32: 2>}
      arts.yield
    } : i64

    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] {runtime_db_mode = #arts.runtime_db_mode<ro>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %cont_pack = arts_rt.edt_param_pack(%c1_i32) : i32 : memref<1xi64>
    %cont = arts_rt.edt_create(%cont_pack : memref<1xi64>) depCount(%c1_i32) route(%route) {arts.outlined_func = "__cont"}
    arts_rt.rec_dep %cont(%acq_guid : memref<?xi64>) bounds_valids(%true) {acquire_modes = array<i32: 1>}
    return
  }

  func.func private @__cont(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    arts.shutdown
    return
  }
}
