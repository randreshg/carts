-----------------------------------------
DataBlockPass STARTED
-----------------------------------------
-----------------------------------------
Rewiring uses of:
  %14 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0], sizes[%c1, %c100], type[f64], typeSize[%c8], affineMap=affine_map<(d0) -> (d0, 0)> {isLoad} -> memref<1x100xf64>
    - affine.store %16, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
  Original affine map: (d0, d1) -> (d0, d1)
  New affine map: (d0, d1) -> (0, d1)
  Drop: 1
  Updated: affine.store %16, %14[0, %arg1] : memref<1x100xf64>
-----------------------------------------
Rewiring uses of:
  %12 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8], affineMap=affine_map<() -> (0, 0)> {isLoad} -> memref<100x100xf64>
    - affine.store %13, %alloca[0, %arg0] : memref<100x100xf64>
  Original affine map: (d0) -> (0, d0)
  New affine map: (d0) -> (d0)
  Drop: 0
  Updated: "affine.store"(%19, %18, %arg0) <{map = affine_map<(d0) -> (d0)>}> : (f64, memref<100x100xf64>, index) -> ()
-----------------------------------------
Rewiring uses of:
  %17 = "arts.datablock"(%6, %0, %1, %1) <{affineMap = affine_map<() -> (0, 0)>, elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
    - %19 = "affine.load"(%6, %arg0) <{map = affine_map<(d0) -> (0, d0)>}> : (memref<100x100xf64>, index) -> f64
  Original affine map: (d0) -> (0, d0)
  New affine map: (d0) -> (d0)
  Drop: 0
  Updated: %19 = "affine.load"(%17, %arg0) <{map = affine_map<(d0) -> (d0)>}> : (memref<100x100xf64>, index) -> f64
-----------------------------------------
Rewiring uses of:
  %20 = "arts.datablock"(%6, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0 - 1, 0)>, elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
    - %23 = "affine.load"(%6, %arg0, %arg1) <{map = affine_map<(d0, d1) -> (d0 - 1, d1)>}> : (memref<100x100xf64>, index, index) -> f64
  Original affine map: (d0, d1) -> (d0 - 1, d1)
  New affine map: (d0, d1) -> (0, d1)
  Drop: 1
  Updated: %24 = "affine.load"(%20, %23, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (memref<1x100xf64>, index, index) -> f64
-----------------------------------------
Rewiring uses of:
  %21 = "arts.datablock"(%4, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0, 0)>, elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
    - "affine.store"(%25, %4, %arg0, %arg1) <{map = affine_map<(d0, d1) -> (d0, d1)>}> : (f64, memref<100x100xf64>, index, index) -> ()
  Original affine map: (d0, d1) -> (d0, d1)
  New affine map: (d0, d1) -> (0, d1)
  Drop: 1
  Updated: "affine.store"(%25, %21, %26, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (f64, memref<1x100xf64>, index, index) -> ()
-----------------------------------------
Rewiring uses of:
  %19 = "arts.datablock"(%6, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0, 0)>, elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
    - %22 = "affine.load"(%6, %arg0, %arg1) <{map = affine_map<(d0, d1) -> (d0, d1)>}> : (memref<100x100xf64>, index, index) -> f64
  Original affine map: (d0, d1) -> (d0, d1)
  New affine map: (d0, d1) -> (0, d1)
  Drop: 1
  Updated: %23 = "affine.load"(%19, %22, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (memref<1x100xf64>, index, index) -> f64
-----------------------------------------
Rewiring uses of:
  %5 = "arts.datablock"(%4, %0, %1, %1) <{elementType = f64, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
-----------------------------------------
Rewiring uses of:
  %7 = "arts.datablock"(%6, %0, %1, %1) <{elementType = f64, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
"builtin.module"() ({
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<18 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str2", unnamed_addr = 0 : i64, value = "B[%d][%d] = %f   \00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<2 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str1", unnamed_addr = 0 : i64, value = "\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<18 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str0", unnamed_addr = 0 : i64, value = "A[%d][%d] = %f   \00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.func"() <{CConv = #llvm.cconv<ccc>, function_type = !llvm.func<i32 (ptr, ...)>, linkage = #llvm.linkage<external>, sym_name = "printf", unnamed_addr = 0 : i64, visibility_ = 0 : i64}> ({
  }) : () -> ()
  "func.func"() <{function_type = () -> (), sym_name = "compute"}> ({
    %0 = "arith.constant"() <{value = 8 : index}> : () -> index
    %1 = "arith.constant"() <{value = 100 : index}> : () -> index
    %2 = "arith.constant"() <{value = 1 : index}> : () -> index
    %3 = "arith.constant"() <{value = 100 : i32}> : () -> i32
    %4 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100x100xf64>
    %5 = "arts.datablock"(%4, %0, %1, %1) <{elementType = f64, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
    %6 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100x100xf64>
    %7 = "arts.datablock"(%6, %0, %1, %1) <{elementType = f64, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
    %8 = "func.call"() <{callee = @rand}> : () -> i32
    %9 = "arith.remsi"(%8, %3) : (i32, i32) -> i32
    "arts.edt"(%5, %7) <{operandSegmentSizes = array<i32: 2, 0>}> ({
      "arts.barrier"() : () -> ()
      "arts.edt"() <{operandSegmentSizes = array<i32: 0, 0>}> ({
        %16 = "arith.sitofp"(%9) : (i32) -> f64
        "affine.for"() ({
        ^bb0(%arg0: index):
          %19 = "arith.index_cast"(%arg0) : (index) -> i32
          %20 = "arts.datablock"(%7, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0, 0)>, elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
          "arts.edt"(%20) <{operandSegmentSizes = array<i32: 1, 0>}> ({
            %21 = "arith.sitofp"(%19) : (i32) -> f64
            %22 = "arith.addf"(%21, %16) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
            "affine.for"() ({
            ^bb0(%arg1: index):
              %23 = "arith.constant"() <{value = 0 : index}> : () -> index
              "affine.store"(%22, %20, %23, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (f64, memref<1x100xf64>, index, index) -> ()
              "affine.yield"() : () -> ()
            }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
            "arts.yield"() : () -> ()
          }) : (memref<1x100xf64>) -> ()
          "affine.yield"() : () -> ()
        }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
        %17 = "arts.datablock"(%7, %0, %1, %1) <{affineMap = affine_map<() -> (0, 0)>, elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
        %18 = "arts.datablock"(%5, %0, %1, %1) <{affineMap = affine_map<() -> (0, 0)>, elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 0, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index) -> memref<100x100xf64>
        "arts.edt"(%18, %17) <{operandSegmentSizes = array<i32: 2, 0>}> ({
          "affine.for"() ({
          ^bb0(%arg0: index):
            %19 = "affine.load"(%17, %arg0) <{map = affine_map<(d0) -> (d0)>}> : (memref<100x100xf64>, index) -> f64
            "affine.store"(%19, %18, %arg0) <{map = affine_map<(d0) -> (d0)>}> : (f64, memref<100x100xf64>, index) -> ()
            "affine.yield"() : () -> ()
          }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
          "arts.yield"() : () -> ()
        }) : (memref<100x100xf64>, memref<100x100xf64>) -> ()
        "affine.for"() ({
        ^bb0(%arg0: index):
          %19 = "arts.datablock"(%7, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0, 0)>, elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
          %20 = "arts.datablock"(%7, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0 - 1, 0)>, elementType = f64, mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
          %21 = "arts.datablock"(%5, %0, %arg0, %2, %1) <{affineMap = affine_map<(d0) -> (d0, 0)>, elementType = f64, mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 2>}> {isLoad} : (memref<100x100xf64>, index, index, index, index) -> memref<1x100xf64>
          "arts.edt"(%20, %21, %19) <{operandSegmentSizes = array<i32: 3, 0>}> ({
            "affine.for"() ({
            ^bb0(%arg1: index):
              %22 = "arith.constant"() <{value = 0 : index}> : () -> index
              %23 = "affine.load"(%19, %22, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (memref<1x100xf64>, index, index) -> f64
              %24 = "arith.constant"() <{value = 0 : index}> : () -> index
              %25 = "affine.load"(%20, %24, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (memref<1x100xf64>, index, index) -> f64
              %26 = "arith.addf"(%23, %25) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
              %27 = "arith.constant"() <{value = 0 : index}> : () -> index
              "affine.store"(%26, %21, %27, %arg1) <{map = affine_map<(d0, d1) -> (0, d1)>}> : (f64, memref<1x100xf64>, index, index) -> ()
              "affine.yield"() : () -> ()
            }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
            "arts.yield"() : () -> ()
          }) : (memref<1x100xf64>, memref<1x100xf64>, memref<1x100xf64>) -> ()
          "affine.yield"() : () -> ()
        }) {lower_bound = affine_map<() -> (1)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
        "arts.yield"() : () -> ()
      }) {single} : () -> ()
      "arts.barrier"() : () -> ()
      "arts.yield"() : () -> ()
    }) {parallel} : (memref<100x100xf64>, memref<100x100xf64>) -> ()
    %10 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr
    %11 = "llvm.getelementptr"(%10) <{elem_type = !llvm.array<18 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %12 = "llvm.mlir.addressof"() <{global_name = @str1}> : () -> !llvm.ptr
    %13 = "llvm.getelementptr"(%12) <{elem_type = !llvm.array<2 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    "affine.for"() ({
    ^bb0(%arg0: index):
      %16 = "arith.index_cast"(%arg0) : (index) -> i32
      "affine.for"() ({
      ^bb0(%arg1: index):
        %18 = "arith.index_cast"(%arg1) : (index) -> i32
        %19 = "affine.load"(%7, %arg0, %arg1) <{map = affine_map<(d0, d1) -> (d0, d1)>}> : (memref<100x100xf64>, index, index) -> f64
        %20 = "llvm.call"(%11, %16, %18, %19) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32, f64) -> i32
        "affine.yield"() : () -> ()
      }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
      %17 = "llvm.call"(%13) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr) -> i32
      "affine.yield"() : () -> ()
    }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
    %14 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
    %15 = "llvm.getelementptr"(%14) <{elem_type = !llvm.array<18 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    "affine.for"() ({
    ^bb0(%arg0: index):
      %16 = "arith.index_cast"(%arg0) : (index) -> i32
      "affine.for"() ({
      ^bb0(%arg1: index):
        %18 = "arith.index_cast"(%arg1) : (index) -> i32
        %19 = "affine.load"(%5, %arg0, %arg1) <{map = affine_map<(d0, d1) -> (d0, d1)>}> : (memref<100x100xf64>, index, index) -> f64
        %20 = "llvm.call"(%15, %16, %18, %19) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32, f64) -> i32
        "affine.yield"() : () -> ()
      }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
      %17 = "llvm.call"(%13) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr) -> i32
      "affine.yield"() : () -> ()
    }) {lower_bound = affine_map<() -> (0)>, step = 1 : index, upper_bound = affine_map<() -> (100)>} : () -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "rand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
matrix-arts.mlir:40:19: error: 'affine.load' op affine map num results must equal memref rank
            %13 = affine.load %alloca_0[0, %arg0] : memref<100x100xf64>
                  ^
matrix-arts.mlir:40:19: note: see current operation: %19 = "affine.load"(%17, %arg0) <{map = affine_map<(d0) -> (d0)>}> : (memref<100x100xf64>, index) -> f64
