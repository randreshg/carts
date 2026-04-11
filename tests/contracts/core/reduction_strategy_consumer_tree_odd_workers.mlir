// RUN: %carts-compile %s --runtime-static-workers --arts-config %S/../../examples/arts.cfg --start-from concurrency --pipeline edt-distribution | %FileCheck %s

// Verify that the tree consumer still uses the pairwise step-2 fold when the
// explicit worker count is odd and the lowered loop actually materializes
// three chunks.

// CHECK-LABEL: func.func @main
// CHECK: arts.db_acquire[<in>] (%{{.+}} : memref<?xi64>, %{{.+}} : memref<?xmemref<?xi32>>) partitioning(<coarse>)
// CHECK: arts.edt <task> <intranode>
// CHECK: scf.for {{.+}} step %c2 iter_args(%{{.+}} = %{{.+}}) -> (i32)
// CHECK: scf.if %{{.+}} -> (i32)
// CHECK-NOT: arts.atomic_add

module attributes {arts.runtime_static_workers = true, arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 3 : i64, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main(%A: memref<27xi32>) {
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c3 = arith.constant 3 : index
    %c27 = arith.constant 27 : index
    %c0_i32 = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <stack>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(i32) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xi32>>)
    %guid_acq, %ptr_acq = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xi32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xi32>>)

    arts.edt <parallel> <intranode> route(%c-1_i32) (%ptr_acq) : memref<?xmemref<?xi32>> attributes {arts.reduction_strategy = "tree", workers = #arts.workers<3>} {
    ^bb0(%arg0: memref<?xmemref<?xi32>>):
      %sum = memref.alloca() : memref<1xi32>
      memref.store %c0_i32, %sum[%c0] : memref<1xi32>
      %sum_cast = memref.cast %sum : memref<1xi32> to memref<?xi32>
      %priv = memref.alloca() : memref<i32>
      arts.for(%c0) to(%c27) step(%c1) reduction(%sum_cast : memref<?xi32>) {
      ^bb0(%iv: index):
        %val = memref.load %A[%iv] : memref<27xi32>
        %acc = memref.load %priv[] : memref<i32>
        %next = arith.addi %acc, %val : i32
        memref.store %next, %priv[] : memref<i32>
      } {arts.reduction_strategy = "tree"}
      arts.db_release(%arg0) : memref<?xmemref<?xi32>>
    }

    arts.db_release(%ptr_acq) : memref<?xmemref<?xi32>>
    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xi32>>
    return
  }
}
