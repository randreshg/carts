// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from=arts-to-llvm --pipeline=arts-to-llvm | %FileCheck %s

// CHECK-LABEL: func.func @post_call_rewrite
// CHECK: %[[RANGE:.*]] = {{(func[.])?call}} @arts_guid_reserve_range(
// CHECK: %[[IDX:.*]] = arith.index_cast %{{.*}} : index to i32
// CHECK: %[[G:.*]] = {{(func[.])?call}} @arts_guid_from_index(%[[RANGE]], %[[IDX]]) : (i64, i32) -> i64
// CHECK-NOT: call @arts_guid_reserve(
//
// CHECK-LABEL: func.func @post_call_rewrite_dynamic
// CHECK: %[[FITS:.*]] = arith.cmpi ule
// CHECK: %[[GT1:.*]] = arith.cmpi ugt
// CHECK: %[[CAN_USE:.*]] = arith.andi %[[FITS]], %[[GT1]] : i1
// CHECK: %[[RANGE2:.*]] = scf.if %[[CAN_USE]] -> (i64) {
// CHECK:   %[[N32:.*]] = arith.index_cast %arg0 : index to i32
// CHECK:   %[[RR:.*]] = {{(func[.])?call}} @arts_guid_reserve_range(
// CHECK: } else {
// CHECK:   scf.yield %{{.*}} : i64
// CHECK: }
// CHECK: scf.for
// CHECK:   scf.if %[[CAN_USE]] {
// CHECK:   %[[IDX2:.*]] = arith.index_cast %{{.*}} : index to i32
// CHECK:   %[[FROM:.*]] = {{(func[.])?call}} @arts_guid_from_index(%[[RANGE2]], %[[IDX2]]) : (i64, i32) -> i64
// CHECK: } else {
// CHECK:   %[[RES:.*]] = {{(func[.])?call}} @arts_guid_reserve(

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func private @arts_guid_reserve(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}

  func.func @post_call_rewrite() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c6_i32 = arith.constant 6 : i32
    %c0_i32 = arith.constant 0 : i32
    %buf = memref.alloc() : memref<8xi64>
    scf.for %i = %c0 to %c8 step %c1 {
      %g = func.call @arts_guid_reserve(%c6_i32, %c0_i32) : (i32, i32) -> i64
      memref.store %g, %buf[%i] : memref<8xi64>
    }
    memref.dealloc %buf : memref<8xi64>
    return
  }

  func.func @post_call_rewrite_dynamic(%N: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c6_i32 = arith.constant 6 : i32
    %c0_i32 = arith.constant 0 : i32
    %buf = memref.alloc(%N) : memref<?xi64>
    scf.for %i = %c0 to %N step %c1 {
      %g = func.call @arts_guid_reserve(%c6_i32, %c0_i32) : (i32, i32) -> i64
      memref.store %g, %buf[%i] : memref<?xi64>
    }
    memref.dealloc %buf : memref<?xi64>
    return
  }
}
