--- ConvertARTSToFuncsPass START ---
[arts-codegen] Initializing ArtsCodegen
[arts-codegen] ArtsCodegen initialized
[convert-arts-to-funcs] Lowering arts.parallel
arts.parallel parameters(%arg0, %6) : (i32, i32), constants(%c0, %c1) : (index, index), dependencies(%7) : (!arts.dep) {
  arts.barrier
  arts.single {
    %8 = arith.index_cast %arg0 : i32 to index
    %9 = arith.sitofp %6 : i32 to f64
    scf.for %arg3 = %c0 to %8 step %c1 {
      %10 = arith.index_cast %arg3 : index to i32
      %11 = arith.sitofp %10 : i32 to f64
      %12 = arith.addf %11, %9 : f64
      memref.store %12, %4[%arg3] : memref<?xf64>
    }
  }
  arts.barrier
}
Cannot cast memref of type memref<?xi8> to type memref<?xf64>


Moving ops to the end of the function
"func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> (), sym_name = "__arts_edt_2", sym_visibility = "private"}> ({
^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>):
  %0 = "arith.constant"() <{value = 0 : index}> : () -> index
  %1 = "arith.constant"() <{value = 1 : index}> : () -> index
  %2 = "arith.constant"() <{value = 0 : index}> : () -> index
  %3 = "memref.load"(%arg1, %2) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
  %4 = "arith.constant"() <{value = 1 : index}> : () -> index
  %5 = "memref.load"(%arg1, %4) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
  %6 = "arith.constant"() <{value = 0 : index}> : () -> index
  %7 = "memref.load"(%arg3, %6) <{nontemporal = false}> : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, index) -> !llvm.struct<(i64, i32, memref<?xi8>)>
  %8 = "func.call"(%7) <{callee = @artsGetPtrFromEdtDep}> : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
  %9 = "arith.index_cast"(%3) : (i64) -> index
  %10 = "arith.sitofp"(%5) : (i64) -> f64
  "scf.for"(%0, %9, %1) ({
  ^bb0(%arg4: index):
    %11 = "arith.index_cast"(%arg4) : (index) -> i32
    %12 = "arith.sitofp"(%11) : (i32) -> f64
    %13 = "arith.addf"(%12, %10) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    "memref.store"(%13, %8, %arg4) : (f64, memref<?xi8>, index) -> ()
    "scf.yield"() : () -> ()
  }) : (index, index, index) -> ()
  "arts.yield"() : () -> ()
}) : () -> ()
=== ConvertARTSToFuncsPass COMPLETE ===
"builtin.module"() ({
  "func.func"() <{function_type = (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>, sym_name = "artsGetPtrFromEdtDep", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64, sym_name = "artsEdtCreateWithEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64, i32) -> i64, sym_name = "artsInitializeAndStartEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64, sym_name = "artsEdtCreate", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "artsGetCurrentNode", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> (), sym_name = "artsDbCreateArray", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<11 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str0", unnamed_addr = 0 : i64, value = "A[0] = %f\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.func"() <{CConv = #llvm.cconv<ccc>, function_type = !llvm.func<i32 (ptr, ...)>, linkage = #llvm.linkage<external>, sym_name = "printf", unnamed_addr = 0 : i64, visibility_ = 0 : i64}> ({
  }) : () -> ()
  "func.func"() <{function_type = (i32, memref<?xf64>, memref<?xf64>) -> (), sym_name = "compute"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.constant"() <{value = 8 : index}> : () -> index
    %3 = "arith.constant"() <{value = 8 : i64}> : () -> i64
    %4 = "arith.constant"() <{value = 100 : i32}> : () -> i32
    %5 = "arith.extsi"(%arg0) : (i32) -> i64
    %6 = "arith.muli"(%5, %3) : (i64, i64) -> i64
    %7 = "arith.index_cast"(%6) : (i64) -> index
    %8 = "arith.divui"(%7, %2) : (index, index) -> index
    %9 = "arts.alloc"(%8) : (index) -> memref<?xf64>
    %10 = "func.call"() <{callee = @rand}> : () -> i32
    %11 = "arith.remsi"(%10, %4) : (i32, i32) -> i32
    %12 = "arith.constant"() <{value = 1 : index}> : () -> index
    %13 = "arith.constant"() <{value = 0 : index}> : () -> index
    %14 = "memref.dim"(%9, %13) : (memref<?xf64>, index) -> index
    %15 = "arith.muli"(%12, %14) : (index, index) -> index
    %16 = "memref.alloca"(%15) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %17 = "arith.constant"() <{value = 8 : i64}> : () -> i64
    %18 = "arith.constant"() <{value = 9 : i32}> : () -> i32
    %19 = "arith.index_cast"(%15) : (index) -> i32
    %20 = "polygeist.memref2pointer"(%9) : (memref<?xf64>) -> !llvm.ptr<memref<?xf64>>
    %21 = "polygeist.pointer2memref"(%20) : (!llvm.ptr<memref<?xf64>>) -> memref<?xi8>
    "func.call"(%16, %17, %18, %19, %21) <{callee = @artsDbCreateArray}> : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> ()
    %22 = "arts.datablock"(%9) <{mode = "inout"}> : (memref<?xf64>) -> !arts.dep
    %23 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %24 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %25 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %26 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<0xi64>
    %27 = "memref.cast"(%26) : (memref<0xi64>) -> memref<?xi64>
    %28 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %30 = "func.call"(%29, %23, %25, %27, %24) <{callee = @artsEdtCreate}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %31 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %32 = "func.call"(%30, %31) <{callee = @artsInitializeAndStartEpoch}> : (i64, i32) -> i64
    %33 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %34 = "arith.index_cast"(%15) : (index) -> i32
    %35 = "arith.addi"(%33, %34) : (i32, i32) -> i32
    %36 = "arith.constant"() <{value = 2 : i32}> : () -> i32
    %37 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<2xi64>
    %38 = "arith.extsi"(%arg0) : (i32) -> i64
    %39 = "arith.constant"() <{value = 0 : index}> : () -> index
    "affine.store"(%38, %37, %39) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<2xi64>, index) -> ()
    %40 = "arith.extsi"(%11) : (i32) -> i64
    %41 = "arith.constant"() <{value = 1 : index}> : () -> index
    "affine.store"(%40, %37, %41) <{map = affine_map<(d0) -> (d0)>}> : (i64, memref<2xi64>, index) -> ()
    %42 = "memref.cast"(%37) : (memref<2xi64>) -> memref<?xi64>
    %43 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %44 = "polygeist.pointer2memref"(%43) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %45 = "func.call"(%44, %23, %36, %42, %35, %32) <{callee = @artsEdtCreateWithEpoch}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "rand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> (), sym_name = "__arts_edt_1", sym_visibility = "private"}> ({
  }) : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> (), sym_name = "__arts_edt_2", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.constant"() <{value = 0 : index}> : () -> index
    %3 = "memref.load"(%arg1, %2) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %4 = "arith.constant"() <{value = 1 : index}> : () -> index
    %5 = "memref.load"(%arg1, %4) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %6 = "arith.constant"() <{value = 0 : index}> : () -> index
    %7 = "memref.load"(%arg3, %6) <{nontemporal = false}> : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, index) -> !llvm.struct<(i64, i32, memref<?xi8>)>
    %8 = "func.call"(%7) <{callee = @artsGetPtrFromEdtDep}> : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %9 = "arith.index_cast"(%3) : (i64) -> index
    %10 = "arith.sitofp"(%5) : (i64) -> f64
    "scf.for"(%0, %9, %1) ({
    ^bb0(%arg4: index):
      %11 = "arith.index_cast"(%arg4) : (index) -> i32
      %12 = "arith.sitofp"(%11) : (i32) -> f64
      %13 = "arith.addf"(%12, %10) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
      "memref.store"(%13, %8, %arg4) : (f64, memref<?xi8>, index) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    "arts.yield"() : () -> ()
  }) : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
taskwithdeps-arts.mlir:27:11: error: 'memref.store' op failed to verify that type of 'value' matches element type of 'memref'
          memref.store %12, %4[%arg3] : memref<?xf64>
          ^
taskwithdeps-arts.mlir:27:11: note: see current operation: "memref.store"(%13, %8, %arg4) : (f64, memref<?xi8>, index) -> ()
