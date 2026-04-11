// RUN: %carts-compile %s --runtime-static-workers --arts-config %S/../inputs/arts_1t.cfg --start-from concurrency --pipeline edt-distribution | %FileCheck %s

// A single effective dispatch lane must not invent compiler-generated block
// acquires or outline a one-worker task EDT. When the runtime topology
// collapses to one worker, ForLowering should preserve coarse acquires and
// inline the lowered loop directly in the dispatch path unless the user
// explicitly asked for a partitioned dependence.
//
// CHECK-LABEL: func.func @main
// CHECK-DAG: arts.db_acquire[<in>] {{.*}}partitioning(<coarse>)
// CHECK-DAG: arts.db_acquire[<out>] {{.*}}partitioning(<coarse>)
// CHECK: scf.if
// CHECK: scf.for
// CHECK-NOT: arts.edt <task>
// CHECK-NOT: arts.epoch
// CHECK-NOT: partitioning(<block>

module attributes {arts.runtime_static_workers = true, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 1 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 2.000000e+00 : f64

    %guidIn, %ptrIn = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guidOut, %ptrOut = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %guidInAcq, %ptrInAcq = arts.db_acquire[<in>] (%guidIn : memref<?xi64>, %ptrIn : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guidOutAcq, %ptrOutAcq = arts.db_acquire[<out>] (%guidOut : memref<?xi64>, %ptrOut : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <parallel> <intranode> route(%c-1_i32) (%ptrInAcq, %ptrOutAcq) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%arg0: memref<?xmemref<?xf64>>, %arg1: memref<?xmemref<?xf64>>):
      arts.for(%c0) to(%c16) step(%c1) {
      ^bb0(%iv: index):
        %src = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %val = memref.load %src[%iv] : memref<?xf64>
        %scaled = arith.mulf %val, %cst : f64
        %dst = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %scaled, %dst[%iv] : memref<?xf64>
      }
      arts.db_release(%arg0) : memref<?xmemref<?xf64>>
      arts.db_release(%arg1) : memref<?xmemref<?xf64>>
    }

    arts.db_release(%ptrInAcq) : memref<?xmemref<?xf64>>
    arts.db_release(%ptrOutAcq) : memref<?xmemref<?xf64>>
    arts.db_free(%guidOut) : memref<?xi64>
    arts.db_free(%ptrOut) : memref<?xmemref<?xf64>>
    arts.db_free(%guidIn) : memref<?xi64>
    arts.db_free(%ptrIn) : memref<?xmemref<?xf64>>
    %ret = arith.constant 0 : i32
    return %ret : i32
  }
}
