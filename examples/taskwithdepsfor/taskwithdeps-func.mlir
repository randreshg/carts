--- ConvertARTSToFuncsPass START ---
[convert-arts-to-funcs] Lowering arts.parallel

 - Datablock: %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
 - Datablock: %8 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
 - Array of Datablocks: %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
 - Array of Datablocks: %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
=== ConvertARTSToFuncsPass COMPLETE ===
"builtin.module"() ({
  "func.func"() <{function_type = (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> (), sym_name = "artsDbCreatePtrAndGuidArrayFromDeps", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64, sym_name = "artsEdtCreateWithEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64, i32) -> i64, sym_name = "artsInitializeAndStartEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64, sym_name = "artsEdtCreate", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "artsGetCurrentNode", sym_visibility = "private"}> ({
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
    %14 = "polygeist.pointer2memref"(%13) : (!llvm.ptr<memref<100xf64>>) -> memref<100xmemref<?xi8>>
    %15 = "arith.constant"() <{value = 8 : i32}> : () -> i32
    %16 = "arith.muli"(%12, %0) : (index, index) -> index
    %17 = "memref.alloca"(%16) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %18 = "arith.index_cast"(%16) : (index) -> i32
    %19 = "arith.muli"(%18, %15) : (i32, i32) -> i32
    "func.call"(%17, %15, %11, %18, %14) <{callee = @artsDbCreateArray}> : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<100xmemref<?xi8>>) -> ()
    %20 = "arts.datablock"(%8, %2, %0, %1) <{mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<100xf64>
    %21 = "arith.constant"() <{value = 9 : i32}> : () -> i32
    %22 = "arith.constant"() <{value = 1 : index}> : () -> index
    %23 = "polygeist.memref2pointer"(%7) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<memref<100xf64>>) -> memref<100xmemref<?xi8>>
    %25 = "arith.constant"() <{value = 8 : i32}> : () -> i32
    %26 = "arith.muli"(%22, %0) : (index, index) -> index
    %27 = "memref.alloca"(%26) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %28 = "arith.index_cast"(%26) : (index) -> i32
    %29 = "arith.muli"(%28, %25) : (i32, i32) -> i32
    "func.call"(%27, %25, %21, %28, %24) <{callee = @artsDbCreateArray}> : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<100xmemref<?xi8>>) -> ()
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
    %42 = "arith.constant"() <{value = 0 : index}> : () -> index
    %43 = "arith.addi"(%42, %16) : (index, index) -> index
    %44 = "arith.addi"(%43, %26) : (index, index) -> index
    %45 = "arith.index_cast"(%44) : (index) -> i32
    %46 = "arith.constant"() <{value = 3 : i32}> : () -> i32
    %47 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<3xi64>
    %48 = "arith.extsi"(%10) : (i32) -> i64
    %49 = "arith.constant"() <{value = 0 : index}> : () -> index
    "affine.store"(%48, %47, %49) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<3xi64>, index) -> ()
    %50 = "arith.index_cast"(%16) : (index) -> i64
    %51 = "arith.constant"() <{value = 1 : index}> : () -> index
    "affine.store"(%50, %47, %51) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<3xi64>, index) -> ()
    %52 = "arith.index_cast"(%26) : (index) -> i64
    %53 = "arith.constant"() <{value = 2 : index}> : () -> index
    "affine.store"(%52, %47, %53) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<3xi64>, index) -> ()
    %54 = "memref.cast"(%47) : (memref<3xi64>) -> memref<?xi64>
    %55 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %56 = "polygeist.pointer2memref"(%55) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %57 = "func.call"(%56, %41, %46, %54, %44, %40) <{callee = @artsEdtCreateWithEpoch}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, index, i64) -> i64
    %58 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr
    %59 = "llvm.getelementptr"(%58) <{elem_type = !llvm.array<11 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %60 = "affine.load"(%8) <{map = affine_map<() -> (0)>}> : (memref<100xf64>) -> f64
    %61 = "llvm.call"(%59, %60) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, f64) -> i32
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
    %8 = "arith.trunci"(%7) : (i64) -> i32
    %9 = "arith.constant"() <{value = 1 : index}> : () -> index
    %10 = "memref.load"(%arg1, %9) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %11 = "arith.index_cast"(%10) : (i64) -> index
    %12 = "arith.constant"() <{value = 2 : index}> : () -> index
    %13 = "memref.load"(%arg1, %12) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %14 = "arith.index_cast"(%13) : (i64) -> index
    %15 = "arith.constant"() <{value = 0 : index}> : () -> index
    %16 = "arith.constant"() <{value = 1 : index}> : () -> index
    %17 = "memref.alloca"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xmemref<?xf64>>
    %18 = "memref.alloca"(%11) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xi64>
    %19 = "polygeist.memref2pointer"(%17) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %20 = "polygeist.pointer2memref"(%19) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    "func.call"(%18, %20, %11, %arg3, %15) <{callee = @artsDbCreatePtrAndGuidArrayFromDeps}> : (memref<?xi64>, memref<?xmemref<?xi8>>, index, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, index) -> ()
    %21 = "arith.addi"(%15, %11) : (index, index) -> index
    %22 = "memref.alloca"(%14) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xmemref<?xf64>>
    %23 = "memref.alloca"(%14) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xi64>
    %24 = "polygeist.memref2pointer"(%22) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %25 = "polygeist.pointer2memref"(%24) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    "func.call"(%23, %25, %14, %arg3, %21) <{callee = @artsDbCreatePtrAndGuidArrayFromDeps}> : (memref<?xi64>, memref<?xmemref<?xi8>>, index, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, index) -> ()
    %26 = "arith.addi"(%21, %14) : (index, index) -> index
    %27 = "arith.sitofp"(%8) : (i32) -> f64
    "scf.for"(%1, %5, %0) ({
    ^bb0(%arg4: index):
      %28 = "arith.index_cast"(%arg4) : (index) -> i32
      %29 = "arts.datablock"(%17, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<1xf64>
      "arts.edt"(%28, %27, %arg4, %1, %29) <{operandSegmentSizes = array<i32: 3, 1, 1>}> ({
        %37 = "arith.sitofp"(%28) : (i32) -> f64
        %38 = "arith.addf"(%37, %27) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
        "memref.store"(%38, %29, %1) : (f64, memref<1xf64>, index) -> ()
        "arts.yield"() : () -> ()
      }) : (i32, f64, index, index, memref<1xf64>) -> ()
      %30 = "memref.load"(%17, %arg4) : (memref<?xmemref<?xf64>>, index) -> f64
      %31 = "arith.addi"(%28, %2) : (i32, i32) -> i32
      %32 = "arith.index_cast"(%31) : (i32) -> index
      %33 = "memref.load"(%17, %32) : (memref<?xmemref<?xf64>>, index) -> f64
      %34 = "arts.datablock"(%17, %arg4, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<1xf64>
      %35 = "arts.datablock"(%17, %32, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<1xf64>
      %36 = "arts.datablock"(%22, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<1xf64>
      "arts.edt"(%28, %33, %30, %arg4, %3, %4, %1, %34, %35, %36) <{operandSegmentSizes = array<i32: 4, 3, 3>}> ({
        %37 = "arith.cmpi"(%28, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1
        %38 = "arith.select"(%37, %33, %4) : (i1, f64, f64) -> f64
        %39 = "arith.addf"(%30, %38) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
        "memref.store"(%39, %36, %1) : (f64, memref<1xf64>, index) -> ()
        "arts.yield"() : () -> ()
      }) : (i32, f64, f64, index, i32, f64, index, memref<1xf64>, memref<1xf64>, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    "func.return"() : () -> ()
    "arts.yield"() : () -> ()
  ^bb1:  // no predecessors
  }) : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
taskwithdeps-arts.mlir:30:17: error: 'memref.load' op failed to verify that result type matches element type of 'memref'
          %11 = memref.load %2[%arg0] : memref<100xf64>
                ^
taskwithdeps-arts.mlir:30:17: note: see current operation: %30 = "memref.load"(%17, %arg4) : (memref<?xmemref<?xf64>>, index) -> f64
