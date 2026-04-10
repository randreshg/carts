// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --start-from=epochs --pipeline=epochs | %FileCheck %s

// When two consecutive epochs carry the same redundant structural acquire set
// and only perform disjoint effective writes, epoch fusion should win over
// outlining a tiny continuation tail.

// CHECK-LABEL: func.func @test_epoch_fusion_prefers_redundant_acquire_collapse
// CHECK-COUNT-1: arts.epoch
// CHECK-NOT: arts.continuation_for_epoch

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @tail() -> ()

  func.func @test_epoch_fusion_prefers_redundant_acquire_collapse() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 1.000000e+00 : f32
    %cst_0 = arith.constant 2.000000e+00 : f32

    %guid_a, %ptr_a = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c4] : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guid_b, %ptr_b = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c0_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c4] : (memref<?xi64>, memref<?xmemref<?xf32>>)

    %e0 = arts.epoch {
      %acq_guid_a, %acq_ptr_a = arts.db_acquire[<out>] (%guid_a : memref<?xi64>, %ptr_a : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      %acq_guid_b, %acq_ptr_b = arts.db_acquire[<out>] (%guid_b : memref<?xi64>, %ptr_b : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      %ref_a = arts.db_ref %acq_ptr_a[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      memref.store %cst, %ref_a[%c0] : memref<?xf32>
      arts.db_release(%acq_ptr_a) : memref<?xmemref<?xf32>>
      arts.db_release(%acq_ptr_b) : memref<?xmemref<?xf32>>
      arts.yield
    } : i64

    %e1 = arts.epoch {
      %acq_guid_a, %acq_ptr_a = arts.db_acquire[<out>] (%guid_a : memref<?xi64>, %ptr_a : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      %acq_guid_b, %acq_ptr_b = arts.db_acquire[<out>] (%guid_b : memref<?xi64>, %ptr_b : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      %ref_b = arts.db_ref %acq_ptr_b[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
      memref.store %cst_0, %ref_b[%c0] : memref<?xf32>
      arts.db_release(%acq_ptr_a) : memref<?xmemref<?xf32>>
      arts.db_release(%acq_ptr_b) : memref<?xmemref<?xf32>>
      arts.yield
    } : i64

    func.call @tail() : () -> ()
    return
  }
}
