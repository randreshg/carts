// RUN: %carts-compile %s --O3 --arts-config %S/../../examples/arts.cfg --start-from edt-opt --pipeline edt-opt | %FileCheck %s

// Loop fusion should allow sibling loops that share only read-only inputs and
// write to disjoint outputs. Stale per-loop parallel metadata must not block
// that fusion.
//
// CHECK-LABEL: func.func @main
// CHECK-COUNT-1: arts.for(%c0) to(%c16) step(%c1)
// CHECK: memref.load %{{.*}}[%[[IV:.*]]] : memref<?xf64>
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[IV]]] : memref<?xf64>
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[IV]]] : memref<?xf64>
// CHECK-NOT: arts.for(%c0) to(%c16) step(%c1)

module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @main() -> i32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c-1_i32 = arith.constant -1 : i32
    %scale = arith.constant 2.000000e+00 : f64
    %shift = arith.constant 1.000000e+00 : f64

    %guidIn, %ptrIn = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guidOut0, %ptrOut0 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guidOut1, %ptrOut1 = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%c-1_i32 : i32) sizes[%c1] elementType(f64) elementSizes[%c16] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %guidInAcq, %ptrInAcq = arts.db_acquire[<in>] (%guidIn : memref<?xi64>, %ptrIn : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guidOut0Acq, %ptrOut0Acq = arts.db_acquire[<out>] (%guidOut0 : memref<?xi64>, %ptrOut0 : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guidOut1Acq, %ptrOut1Acq = arts.db_acquire[<out>] (%guidOut1 : memref<?xi64>, %ptrOut1 : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <parallel> <intranode> route(%c-1_i32) (%ptrInAcq, %ptrOut0Acq, %ptrOut1Acq) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%arg0: memref<?xmemref<?xf64>>, %arg1: memref<?xmemref<?xf64>>, %arg2: memref<?xmemref<?xf64>>):
      arts.for(%c0) to(%c16) step(%c1) {{
      ^bb0(%iv: index):
        %src = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %val = memref.load %src[%iv] : memref<?xf64>
        %scaled = arith.mulf %val, %scale : f64
        %dst = arts.db_ref %arg1[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %scaled, %dst[%iv] : memref<?xf64>
      }} {arts.loop = #arts.loop_metadata<potentiallyParallel = false, hasReductions = false, tripCount = 1 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = true, memrefsWithLoopCarriedDeps = 1 : i64, parallelClassification = 3 : i64, locationKey = "stale">}
      arts.for(%c0) to(%c16) step(%c1) {{
      ^bb0(%iv: index):
        %src = arts.db_ref %arg0[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %val = memref.load %src[%iv] : memref<?xf64>
        %biased = arith.addf %val, %shift : f64
        %dst = arts.db_ref %arg2[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %biased, %dst[%iv] : memref<?xf64>
      }}
      arts.db_release(%arg0) : memref<?xmemref<?xf64>>
      arts.db_release(%arg1) : memref<?xmemref<?xf64>>
      arts.db_release(%arg2) : memref<?xmemref<?xf64>>
    }

    arts.db_release(%ptrOut1Acq) : memref<?xmemref<?xf64>>
    arts.db_release(%ptrOut0Acq) : memref<?xmemref<?xf64>>
    arts.db_release(%ptrInAcq) : memref<?xmemref<?xf64>>
    arts.db_free(%guidOut1) : memref<?xi64>
    arts.db_free(%ptrOut1) : memref<?xmemref<?xf64>>
    arts.db_free(%guidOut0) : memref<?xi64>
    arts.db_free(%ptrOut0) : memref<?xmemref<?xf64>>
    arts.db_free(%guidIn) : memref<?xi64>
    arts.db_free(%ptrIn) : memref<?xmemref<?xf64>>
    %ret = arith.constant 0 : i32
    return %ret : i32
  }
}
