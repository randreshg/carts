--- ConvertARTSToFuncsPass START ---
 - Datablock: %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
 - Datablock: %8 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
 - Datablock: %20 = arts.datablock "out", %7 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
 - Datablock: %29 = arts.datablock "in", %7 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
 - Datablock: %34 = arts.datablock "in", %7 : memref<100xf64>[%27] [%c1] [%c1] {isLoad} : memref<1xf64>
 - Datablock: %39 = arts.datablock "out", %13 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.parallel
arts.parallel parameters(%1) : (i32), constants(%c1, %c0, %c-1_i32, %c0_i32, %cst, %c100) : (index, index, i32, i32, f64, index), dependencies(%7, %13) : (memref<100xf64>, memref<100xf64>) {
  arts.barrier
  arts.single {
    %18 = arith.sitofp %1 : i32 to f64
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %19 = arith.index_cast %arg0 : index to i32
      %c9_i32_7 = arith.constant 9 : i32
      %c1_8 = arith.constant 1 : index
      %20 = "polygeist.memref2pointer"(%7) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %21 = "polygeist.pointer2memref"(%20) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %22 = arith.index_cast %c1_8 : index to i32
      %23 = func.call @artsDbCreatePtr(%21, %6, %c9_i32_7) : (memref<?xi8>, i32, i32) -> i64
      %24 = arts.datablock "out", %7 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
      arts.edt parameters(%19, %18, %arg0) : (i32, f64, index), constants(%c0) : (index), dependencies(%24) : (memref<1xf64>) {
        %44 = arith.sitofp %19 : i32 to f64
        %45 = arith.addf %44, %18 : f64
        memref.store %45, %24[%c0] : memref<1xf64>
      }
      %25 = memref.load %7[%arg0] : memref<100xf64>
      %26 = arith.addi %19, %c-1_i32 : i32
      %27 = arith.index_cast %26 : i32 to index
      %28 = memref.load %7[%27] : memref<100xf64>
      %c7_i32 = arith.constant 7 : i32
      %c1_9 = arith.constant 1 : index
      %29 = "polygeist.memref2pointer"(%7) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %31 = arith.index_cast %c1_9 : index to i32
      %32 = func.call @artsDbCreatePtr(%30, %6, %c7_i32) : (memref<?xi8>, i32, i32) -> i64
      %33 = arts.datablock "in", %7 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
      %c7_i32_10 = arith.constant 7 : i32
      %c1_11 = arith.constant 1 : index
      %34 = "polygeist.memref2pointer"(%7) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %35 = "polygeist.pointer2memref"(%34) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %36 = arith.index_cast %c1_11 : index to i32
      %37 = func.call @artsDbCreatePtr(%35, %6, %c7_i32_10) : (memref<?xi8>, i32, i32) -> i64
      %38 = arts.datablock "in", %7 : memref<100xf64>[%27] [%c1] [%c1] {isLoad} : memref<1xf64>
      %c9_i32_12 = arith.constant 9 : i32
      %c1_13 = arith.constant 1 : index
      %39 = "polygeist.memref2pointer"(%13) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %40 = "polygeist.pointer2memref"(%39) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %41 = arith.index_cast %c1_13 : index to i32
      %42 = func.call @artsDbCreatePtr(%40, %12, %c9_i32_12) : (memref<?xi8>, i32, i32) -> i64
      %43 = arts.datablock "out", %13 : memref<100xf64>[%arg0] [%c1] [%c1] {isLoad} : memref<1xf64>
      arts.edt parameters(%19, %28, %25, %arg0) : (i32, f64, f64, index), constants(%c0_i32, %cst, %c0) : (i32, f64, index), dependencies(%33, %38, %43) : (memref<1xf64>, memref<1xf64>, memref<1xf64>) {
        %44 = arith.cmpi sgt, %19, %c0_i32 : i32
        %45 = arith.select %44, %28, %cst : f64
        %46 = arith.addf %25, %45 : f64
        memref.store %46, %43[%c0] : memref<1xf64>
      }
    }
  }
  arts.barrier
}
Entry block:
^bb1:  // no predecessors
  %13 = "arith.sitofp"(%7) : (i64) -> f64
  "scf.for"(%1, %5, %0) ({
  ^bb0(%arg4: index):
    %14 = "arith.index_cast"(%arg4) : (index) -> i32
    %15 = "arith.constant"() <{value = 9 : i32}> : () -> i32
    %16 = "arith.constant"() <{value = 1 : index}> : () -> index
    %17 = "polygeist.memref2pointer"(%20) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %19 = "arith.index_cast"(%16) : (index) -> i32
    %20 = "func.call"(%18, %19, %15) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
    %21 = "arts.datablock"(%20, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
    "arts.edt"(%14, %13, %arg4, %1, %21) <{operandSegmentSizes = array<i32: 3, 1, 1>}> ({
      %47 = "arith.sitofp"(%14) : (i32) -> f64
      %48 = "arith.addf"(%47, %13) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
      "memref.store"(%48, %21, %1) : (f64, memref<1xf64>, index) -> ()
      "arts.yield"() : () -> ()
    }) : (i32, f64, index, index, memref<1xf64>) -> ()
    %22 = "memref.load"(%20, %arg4) : (memref<100xf64>, index) -> f64
    %23 = "arith.addi"(%14, %2) : (i32, i32) -> i32
    %24 = "arith.index_cast"(%23) : (i32) -> index
    %25 = "memref.load"(%20, %24) : (memref<100xf64>, index) -> f64
    %26 = "arith.constant"() <{value = 7 : i32}> : () -> i32
    %27 = "arith.constant"() <{value = 1 : index}> : () -> index
    %28 = "polygeist.memref2pointer"(%20) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %30 = "arith.index_cast"(%27) : (index) -> i32
    %31 = "func.call"(%29, %19, %26) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
    %32 = "arts.datablock"(%20, %arg4, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
    %33 = "arith.constant"() <{value = 7 : i32}> : () -> i32
    %34 = "arith.constant"() <{value = 1 : index}> : () -> index
    %35 = "polygeist.memref2pointer"(%20) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %36 = "polygeist.pointer2memref"(%35) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %37 = "arith.index_cast"(%34) : (index) -> i32
    %38 = "func.call"(%36, %19, %33) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
    %39 = "arts.datablock"(%20, %24, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
    %40 = "arith.constant"() <{value = 9 : i32}> : () -> i32
    %41 = "arith.constant"() <{value = 1 : index}> : () -> index
    %42 = "polygeist.memref2pointer"(%30) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %43 = "polygeist.pointer2memref"(%42) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %44 = "arith.index_cast"(%41) : (index) -> i32
    %45 = "func.call"(%43, %29, %40) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
    %46 = "arts.datablock"(%30, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
    "arts.edt"(%14, %25, %22, %arg4, %3, %4, %1, %32, %39, %46) <{operandSegmentSizes = array<i32: 4, 3, 3>}> ({
      %47 = "arith.cmpi"(%14, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1
      %48 = "arith.select"(%47, %25, %4) : (i1, f64, f64) -> f64
      %49 = "arith.addf"(%22, %48) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
      "memref.store"(%49, %46, %1) : (f64, memref<1xf64>, index) -> ()
      "arts.yield"() : () -> ()
    }) : (i32, f64, f64, index, i32, f64, index, memref<1xf64>, memref<1xf64>, memref<1xf64>) -> ()
    "scf.yield"() : () -> ()
  }) : (index, index, index) -> ()
  "arts.yield"() : () -> ()
=== ConvertARTSToFuncsPass COMPLETE ===
"builtin.module"() ({
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64, sym_name = "artsEdtCreateWithEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64, i32) -> i64, sym_name = "artsInitializeAndStartEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64, sym_name = "artsEdtCreate", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "artsGetCurrentNode", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi8>, i32, i32) -> i64, sym_name = "artsDbCreatePtr", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> (), sym_name = "artsDbCreateArray", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<11 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str0", unnamed_addr = 0 : i64, value = "A[0] = %f\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.func"() <{CConv = #llvm.cconv<ccc>, function_type = !llvm.func<i32 (ptr, ...)>, linkage = #llvm.linkage<external>, sym_name = "printf", unnamed_addr = 0 : i64, visibility_ = 0 : i64}> ({
  }) : () -> ()
  "func.func"() <{function_type = () -> (), sym_name = "compute"}> ({
    %0 = "arith.constant"() <{value = 100 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.constant"() <{value = 0 : index}> : () -> index
    %3 = "arith.constant"() <{value = -1 : i32}> : () -> i32
    %4 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %5 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %6 = "arith.constant"() <{value = 100 : i32}> : () -> i32
    %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
    %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
    %9 = "func.call"() <{callee = @rand}> : () -> i32
    %10 = "arith.remsi"(%9, %6) : (i32, i32) -> i32
    %11 = "arith.constant"() <{value = 9 : i32}> : () -> i32
    %12 = "arith.constant"() <{value = 1 : index}> : () -> index
    %13 = "polygeist.memref2pointer"(%8) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %14 = "polygeist.pointer2memref"(%13) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %15 = "arith.constant"() <{value = 8 : i32}> : () -> i32
    %16 = "arith.muli"(%12, %0) : (index, index) -> index
    %17 = "memref.alloca"(%16) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %18 = "arith.index_cast"(%16) : (index) -> i32
    %19 = "arith.muli"(%18, %15) : (i32, i32) -> i32
    "func.call"(%17, %15, %11, %18, %14) <{callee = @artsDbCreateArray}> : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %20 = "arts.datablock"(%8, %2, %0, %1) <{mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<100xf64>
    %21 = "arith.constant"() <{value = 9 : i32}> : () -> i32
    %22 = "arith.constant"() <{value = 1 : index}> : () -> index
    %23 = "polygeist.memref2pointer"(%7) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %25 = "arith.constant"() <{value = 8 : i32}> : () -> i32
    %26 = "arith.muli"(%22, %0) : (index, index) -> index
    %27 = "memref.alloca"(%26) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %28 = "arith.index_cast"(%26) : (index) -> i32
    %29 = "arith.muli"(%28, %25) : (i32, i32) -> i32
    "func.call"(%27, %25, %21, %28, %24) <{callee = @artsDbCreateArray}> : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %30 = "arts.datablock"(%7, %2, %0, %1) <{mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<100xf64>
    %31 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %32 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %33 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<0xi64>
    %34 = "memref.cast"(%33) : (memref<0xi64>) -> memref<?xi64>
    %35 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %36 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %37 = "polygeist.pointer2memref"(%36) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %38 = "func.call"(%37, %31, %32, %34, %35) <{callee = @artsEdtCreate}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %39 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %40 = "func.call"(%38, %39) <{callee = @artsInitializeAndStartEpoch}> : (i64, i32) -> i64
    %41 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %42 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %43 = "arith.addi"(%42, %18) : (i32, i32) -> i32
    %44 = "arith.addi"(%43, %28) : (i32, i32) -> i32
    %45 = "arith.constant"() <{value = 3 : i32}> : () -> i32
    %46 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<3xi64>
    %47 = "arith.extsi"(%10) : (i32) -> i64
    %48 = "arith.constant"() <{value = 0 : index}> : () -> index
    "affine.store"(%47, %46, %48) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<3xi64>, index) -> ()
    %49 = "arith.extsi"(%18) : (i32) -> i64
    %50 = "arith.constant"() <{value = 1 : index}> : () -> index
    "affine.store"(%49, %46, %50) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<3xi64>, index) -> ()
    %51 = "arith.extsi"(%28) : (i32) -> i64
    %52 = "arith.constant"() <{value = 2 : index}> : () -> index
    "affine.store"(%51, %46, %52) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<3xi64>, index) -> ()
    %53 = "memref.cast"(%46) : (memref<3xi64>) -> memref<?xi64>
    %54 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %55 = "polygeist.pointer2memref"(%54) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %56 = "func.call"(%55, %41, %45, %53, %44, %40) <{callee = @artsEdtCreateWithEpoch}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %57 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr
    %58 = "llvm.getelementptr"(%57) <{elem_type = !llvm.array<11 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %59 = "affine.load"(%8) <{map = affine_map<() -> (0)>}> : (memref<100xf64>) -> f64
    %60 = "llvm.call"(%58, %59) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, f64) -> i32
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "rand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> (), sym_name = "__arts_edt_1", sym_visibility = "private"}> ({
  }) : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> (), sym_name = "__arts_edt_2", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>):
    %0 = "arith.constant"() <{value = 1 : index}> : () -> index
    %1 = "arith.constant"() <{value = 0 : index}> : () -> index
    %2 = "arith.constant"() <{value = -1 : i32}> : () -> i32
    %3 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %4 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %5 = "arith.constant"() <{value = 100 : index}> : () -> index
    %6 = "arith.constant"() <{value = 0 : index}> : () -> index
    %7 = "memref.load"(%arg1, %6) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %8 = "arith.constant"() <{value = 1 : index}> : () -> index
    %9 = "memref.load"(%arg1, %8) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %10 = "arith.constant"() <{value = 2 : index}> : () -> index
    %11 = "memref.load"(%arg1, %10) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %12 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %13 = "arith.sitofp"(%7) : (i64) -> f64
    "scf.for"(%1, %5, %0) ({
    ^bb0(%arg4: index):
      %14 = "arith.index_cast"(%arg4) : (index) -> i32
      %15 = "arith.constant"() <{value = 9 : i32}> : () -> i32
      %16 = "arith.constant"() <{value = 1 : index}> : () -> index
      %17 = "polygeist.memref2pointer"(%20) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %19 = "arith.index_cast"(%16) : (index) -> i32
      %20 = "func.call"(%18, %19, %15) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
      %21 = "arts.datablock"(%20, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
      "arts.edt"(%14, %13, %arg4, %1, %21) <{operandSegmentSizes = array<i32: 3, 1, 1>}> ({
        %47 = "arith.sitofp"(%14) : (i32) -> f64
        %48 = "arith.addf"(%47, %13) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
        "memref.store"(%48, %21, %1) : (f64, memref<1xf64>, index) -> ()
        "arts.yield"() : () -> ()
      }) : (i32, f64, index, index, memref<1xf64>) -> ()
      %22 = "memref.load"(%20, %arg4) : (memref<100xf64>, index) -> f64
      %23 = "arith.addi"(%14, %2) : (i32, i32) -> i32
      %24 = "arith.index_cast"(%23) : (i32) -> index
      %25 = "memref.load"(%20, %24) : (memref<100xf64>, index) -> f64
      %26 = "arith.constant"() <{value = 7 : i32}> : () -> i32
      %27 = "arith.constant"() <{value = 1 : index}> : () -> index
      %28 = "polygeist.memref2pointer"(%20) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %30 = "arith.index_cast"(%27) : (index) -> i32
      %31 = "func.call"(%29, %19, %26) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
      %32 = "arts.datablock"(%20, %arg4, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
      %33 = "arith.constant"() <{value = 7 : i32}> : () -> i32
      %34 = "arith.constant"() <{value = 1 : index}> : () -> index
      %35 = "polygeist.memref2pointer"(%20) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %36 = "polygeist.pointer2memref"(%35) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %37 = "arith.index_cast"(%34) : (index) -> i32
      %38 = "func.call"(%36, %19, %33) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
      %39 = "arts.datablock"(%20, %24, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
      %40 = "arith.constant"() <{value = 9 : i32}> : () -> i32
      %41 = "arith.constant"() <{value = 1 : index}> : () -> index
      %42 = "polygeist.memref2pointer"(%30) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
      %43 = "polygeist.pointer2memref"(%42) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
      %44 = "arith.index_cast"(%41) : (index) -> i32
      %45 = "func.call"(%43, %29, %40) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
      %46 = "arts.datablock"(%30, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
      "arts.edt"(%14, %25, %22, %arg4, %3, %4, %1, %32, %39, %46) <{operandSegmentSizes = array<i32: 4, 3, 3>}> ({
        %47 = "arith.cmpi"(%14, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1
        %48 = "arith.select"(%47, %25, %4) : (i1, f64, f64) -> f64
        %49 = "arith.addf"(%22, %48) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
        "memref.store"(%49, %46, %1) : (f64, memref<1xf64>, index) -> ()
        "arts.yield"() : () -> ()
      }) : (i32, f64, f64, index, i32, f64, index, memref<1xf64>, memref<1xf64>, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
  }) : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
taskwithdeps-arts.mlir:22:9: error: block with no terminator, has 
"scf.for"(%1, %5, %0) ({
^bb0(%arg4: index):
  %14 = "arith.index_cast"(%arg4) : (index) -> i32
  %15 = "arith.constant"() <{value = 9 : i32}> : () -> i32
  %16 = "arith.constant"() <{value = 1 : index}> : () -> index
  %17 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %19 = "arith.index_cast"(%16) : (index) -> i32
  %20 = "func.call"(%18, <<UNKNOWN SSA VALUE>>, %15) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %21 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  "arts.edt"(%14, %13, %arg4, %1, %21) <{operandSegmentSizes = array<i32: 3, 1, 1>}> ({
    %47 = "arith.sitofp"(%14) : (i32) -> f64
    %48 = "arith.addf"(%47, %13) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    "memref.store"(%48, %21, %1) : (f64, memref<1xf64>, index) -> ()
    "arts.yield"() : () -> ()
  }) : (i32, f64, index, index, memref<1xf64>) -> ()
  %22 = "memref.load"(<<UNKNOWN SSA VALUE>>, %arg4) : (memref<100xf64>, index) -> f64
  %23 = "arith.addi"(%14, %2) : (i32, i32) -> i32
  %24 = "arith.index_cast"(%23) : (i32) -> index
  %25 = "memref.load"(<<UNKNOWN SSA VALUE>>, %24) : (memref<100xf64>, index) -> f64
  %26 = "arith.constant"() <{value = 7 : i32}> : () -> i32
  %27 = "arith.constant"() <{value = 1 : index}> : () -> index
  %28 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %30 = "arith.index_cast"(%27) : (index) -> i32
  %31 = "func.call"(%29, <<UNKNOWN SSA VALUE>>, %26) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %32 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %arg4, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  %33 = "arith.constant"() <{value = 7 : i32}> : () -> i32
  %34 = "arith.constant"() <{value = 1 : index}> : () -> index
  %35 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %36 = "polygeist.pointer2memref"(%35) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %37 = "arith.index_cast"(%34) : (index) -> i32
  %38 = "func.call"(%36, <<UNKNOWN SSA VALUE>>, %33) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %39 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %24, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  %40 = "arith.constant"() <{value = 9 : i32}> : () -> i32
  %41 = "arith.constant"() <{value = 1 : index}> : () -> index
  %42 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %43 = "polygeist.pointer2memref"(%42) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %44 = "arith.index_cast"(%41) : (index) -> i32
  %45 = "func.call"(%43, <<UNKNOWN SSA VALUE>>, %40) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %46 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  "arts.edt"(%14, %25, %22, %arg4, %3, %4, %1, %32, %39, %46) <{operandSegmentSizes = array<i32: 4, 3, 3>}> ({
    %47 = "arith.cmpi"(%14, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1
    %48 = "arith.select"(%47, %25, %4) : (i1, f64, f64) -> f64
    %49 = "arith.addf"(%22, %48) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    "memref.store"(%49, %46, %1) : (f64, memref<1xf64>, index) -> ()
    "arts.yield"() : () -> ()
  }) : (i32, f64, f64, index, i32, f64, index, memref<1xf64>, memref<1xf64>, memref<1xf64>) -> ()
  "scf.yield"() : () -> ()
}) : (index, index, index) -> ()
        scf.for %arg0 = %c0 to %c100 step %c1 {
        ^
taskwithdeps-arts.mlir:22:9: note: see current operation: 
"scf.for"(%1, %5, %0) ({
^bb0(%arg4: index):
  %14 = "arith.index_cast"(%arg4) : (index) -> i32
  %15 = "arith.constant"() <{value = 9 : i32}> : () -> i32
  %16 = "arith.constant"() <{value = 1 : index}> : () -> index
  %17 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %19 = "arith.index_cast"(%16) : (index) -> i32
  %20 = "func.call"(%18, <<UNKNOWN SSA VALUE>>, %15) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %21 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  "arts.edt"(%14, %13, %arg4, %1, %21) <{operandSegmentSizes = array<i32: 3, 1, 1>}> ({
    %47 = "arith.sitofp"(%14) : (i32) -> f64
    %48 = "arith.addf"(%47, %13) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    "memref.store"(%48, %21, %1) : (f64, memref<1xf64>, index) -> ()
    "arts.yield"() : () -> ()
  }) : (i32, f64, index, index, memref<1xf64>) -> ()
  %22 = "memref.load"(<<UNKNOWN SSA VALUE>>, %arg4) : (memref<100xf64>, index) -> f64
  %23 = "arith.addi"(%14, %2) : (i32, i32) -> i32
  %24 = "arith.index_cast"(%23) : (i32) -> index
  %25 = "memref.load"(<<UNKNOWN SSA VALUE>>, %24) : (memref<100xf64>, index) -> f64
  %26 = "arith.constant"() <{value = 7 : i32}> : () -> i32
  %27 = "arith.constant"() <{value = 1 : index}> : () -> index
  %28 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %30 = "arith.index_cast"(%27) : (index) -> i32
  %31 = "func.call"(%29, <<UNKNOWN SSA VALUE>>, %26) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %32 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %arg4, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  %33 = "arith.constant"() <{value = 7 : i32}> : () -> i32
  %34 = "arith.constant"() <{value = 1 : index}> : () -> index
  %35 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %36 = "polygeist.pointer2memref"(%35) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %37 = "arith.index_cast"(%34) : (index) -> i32
  %38 = "func.call"(%36, <<UNKNOWN SSA VALUE>>, %33) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %39 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %24, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  %40 = "arith.constant"() <{value = 9 : i32}> : () -> i32
  %41 = "arith.constant"() <{value = 1 : index}> : () -> index
  %42 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
  %43 = "polygeist.pointer2memref"(%42) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
  %44 = "arith.index_cast"(%41) : (index) -> i32
  %45 = "func.call"(%43, <<UNKNOWN SSA VALUE>>, %40) <{callee = @artsDbCreatePtr}> : (memref<?xi8>, i32, i32) -> i64
  %46 = "arts.datablock"(<<UNKNOWN SSA VALUE>>, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<100xf64>, index, index, index) -> memref<1xf64>
  "arts.edt"(%14, %25, %22, %arg4, %3, %4, %1, %32, %39, %46) <{operandSegmentSizes = array<i32: 4, 3, 3>}> ({
    %47 = "arith.cmpi"(%14, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1
    %48 = "arith.select"(%47, %25, %4) : (i1, f64, f64) -> f64
    %49 = "arith.addf"(%22, %48) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    "memref.store"(%49, %46, %1) : (f64, memref<1xf64>, index) -> ()
    "arts.yield"() : () -> ()
  }) : (i32, f64, f64, index, i32, f64, index, memref<1xf64>, memref<1xf64>, memref<1xf64>) -> ()
  "scf.yield"() : () -> ()
}) : (index, index, index) -> ()
