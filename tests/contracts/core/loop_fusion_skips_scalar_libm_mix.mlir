// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --start-from edt-opt --pipeline edt-opt | %FileCheck %s

// Mixing arithmetic-only pointwise loops with scalar libm-heavy loops blocks
// vectorization on the simple stage after fusion. Keep those loops separate.
// CHECK-LABEL: func.func @main
// CHECK: arts.for(%c0) to(%c16) step(%c1)
// CHECK: arith.addf
// CHECK: arts.for(%c0) to(%c16) step(%c1)
// CHECK: math.exp

module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c-1_i32 = arith.constant -1 : i32
    %bias = arith.constant 1.000000e+00 : f32

    %guidIn, %ptrIn = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guidOut0, %ptrOut0 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guidOut1, %ptrOut1 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f32) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf32>>)

    %guidInAcq, %ptrInAcq = arts.db_acquire[<in>] (%guidIn : memref<?xi64>, %ptrIn : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guidOut0Acq, %ptrOut0Acq = arts.db_acquire[<out>] (%guidOut0 : memref<?xi64>, %ptrOut0 : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)
    %guidOut1Acq, %ptrOut1Acq = arts.db_acquire[<out>] (%guidOut1 : memref<?xi64>, %ptrOut1 : memref<?xmemref<?xf32>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf32>>)

    arts.edt <parallel> <intranode> route(%c-1_i32) (%ptrInAcq, %ptrOut0Acq, %ptrOut1Acq) : memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>> {
    ^bb0(%arg0: memref<?xmemref<?xf32>>, %arg1: memref<?xmemref<?xf32>>, %arg2: memref<?xmemref<?xf32>>):
      arts.for(%c0) to(%c16) step(%c1) {
      ^bb0(%iv: index):
        %src = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        %val = memref.load %src[%iv] : memref<?xf32>
        %biased = arith.addf %val, %bias : f32
        %dst = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %biased, %dst[%iv] : memref<?xf32>
      }
      arts.for(%c0) to(%c16) step(%c1) {
      ^bb0(%iv: index):
        %src = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        %val = memref.load %src[%iv] : memref<?xf32>
        %exp = math.exp %val : f32
        %dst = arts.db_ref %arg2[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        memref.store %exp, %dst[%iv] : memref<?xf32>
      }
      arts.db_release(%arg0) : memref<?xmemref<?xf32>>
      arts.db_release(%arg1) : memref<?xmemref<?xf32>>
      arts.db_release(%arg2) : memref<?xmemref<?xf32>>
    }

    arts.db_release(%ptrOut1Acq) : memref<?xmemref<?xf32>>
    arts.db_release(%ptrOut0Acq) : memref<?xmemref<?xf32>>
    arts.db_release(%ptrInAcq) : memref<?xmemref<?xf32>>
    arts.db_free(%guidOut1) : memref<?xi64>
    arts.db_free(%ptrOut1) : memref<?xmemref<?xf32>>
    arts.db_free(%guidOut0) : memref<?xi64>
    arts.db_free(%ptrOut0) : memref<?xmemref<?xf32>>
    arts.db_free(%guidIn) : memref<?xi64>
    arts.db_free(%ptrIn) : memref<?xmemref<?xf32>>
    %ret = arith.constant 0 : i32
    return %ret : i32
  }
}
