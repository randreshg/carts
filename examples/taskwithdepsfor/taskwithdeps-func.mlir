--- ConvertArtsToFuncsPass START ---
[convert-arts-to-funcs] Iterate over all the functions
[convert-arts-to-funcs] Lowering arts.datablock: %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %9 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %23 = arts.datablock "out", %7 : memref<?xmemref<?xf64>>[%arg0] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %31 = arts.datablock "in", %7 : memref<?xmemref<?xf64>>[%arg0] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %36 = arts.datablock "in", %7 : memref<?xmemref<?xf64>>[%30] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.datablock: %41 = arts.datablock "out", %14 : memref<?xmemref<?xf64>>[%arg0] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
[convert-arts-to-funcs] Lowering arts.edt parallel

-----------------------------------------
Naive collection of parameters and dependencies: 
  Adding parameter: %4 = arith.muli %c1_1, %c100 : index
  Adding parameter: %1 = arith.remsi %0, %c100_i32 : i32
  Adding parameter: %6 = arith.muli %5, %c8_i32 : i32
  Adding parameter: %14 = arith.muli %13, %c8_i32_5 : i32
-----------------------------------------
-----------------------------------------
EDT Environment:
- Parameters:
   %4 = arith.muli %c1_1, %c100 : index
   %1 = arith.remsi %0, %c100_i32 : i32
   %6 = arith.muli %5, %c8_i32 : i32
   %14 = arith.muli %13, %c8_i32_5 : i32

- Constants:
   %c1 = arith.constant 1 : index
   %c0 = arith.constant 0 : index
   %c-1_i32 = arith.constant -1 : i32
   %c0_i32 = arith.constant 0 : i32
   %cst = arith.constant 0.000000e+00 : f64
   %c100 = arith.constant 100 : index

- Dependencies:
-----------------------------------------
 - Array of Datablocks: %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
 - Array of Datablocks: %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
[convert-arts-to-funcs] Parallel op lowered

[convert-arts-to-funcs] New function: func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
  %c1 = arith.constant 1 : index
  %c0 = arith.constant 0 : index
  %c-1_i32 = arith.constant -1 : i32
  %c0_i32 = arith.constant 0 : i32
  %cst = arith.constant 0.000000e+00 : f64
  %c100 = arith.constant 100 : index
  %c0_0 = arith.constant 0 : index
  %0 = memref.load %arg1[%c0_0] : memref<?xi64>
  %1 = arith.index_cast %0 : i64 to index
  %c1_1 = arith.constant 1 : index
  %2 = memref.load %arg1[%c1_1] : memref<?xi64>
  %3 = arith.trunci %2 : i64 to i32
  %c2 = arith.constant 2 : index
  %4 = memref.load %arg1[%c2] : memref<?xi64>
  %5 = arith.trunci %4 : i64 to i32
  %c3 = arith.constant 3 : index
  %6 = memref.load %arg1[%c3] : memref<?xi64>
  %7 = arith.trunci %6 : i64 to i32
  %c4 = arith.constant 4 : index
  %8 = memref.load %arg1[%c4] : memref<?xi64>
  %9 = arith.index_cast %8 : i64 to index
  %c5 = arith.constant 5 : index
  %10 = memref.load %arg1[%c5] : memref<?xi64>
  %11 = arith.index_cast %10 : i64 to index
  %c0_2 = arith.constant 0 : index
  %c1_3 = arith.constant 1 : index
  %alloca = memref.alloca(%9) : memref<?xmemref<?xf64>>
  %alloca_4 = memref.alloca(%9) : memref<?xi64>
  %12 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
  %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
  %14 = arith.index_cast %9 : index to i32
  %15 = arith.index_cast %c0_2 : index to i32
  call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_4, %13, %14, %arg3, %15) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
  %16 = arith.addi %c0_2, %9 : index
  %alloca_5 = memref.alloca(%11) : memref<?xmemref<?xf64>>
  %alloca_6 = memref.alloca(%11) : memref<?xi64>
  %17 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
  %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
  %19 = arith.index_cast %11 : index to i32
  %20 = arith.index_cast %16 : index to i32
  call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_6, %18, %19, %arg3, %20) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
  %21 = arith.addi %16, %11 : index
  %22 = arts.event %9 {grouped} : memref<100xi64>
  %23 = arith.sitofp %3 : i32 to f64
  scf.for %arg4 = %c0 to %c100 step %c1 {
    %24 = arith.index_cast %arg4 : index to i32
    %c9_i32 = arith.constant 9 : i32
    %c1_7 = arith.constant 1 : index
    %25 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %26 = "polygeist.pointer2memref"(%25) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
    %27 = func.call @artsDbCreatePtr(%26, %5, %c9_i32) : (memref<?xi8>, i32, i32) -> i64
    %28 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
    %29 = "arts.undef"() : () -> memref<1xf64>
    %30 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
    %31 = memref.load %22[%arg4] : memref<100xi64>
    arts.edt dependencies(%28) : (memref<?xmemref<?xf64>>), events(%31) : (i64) {
      %53 = arith.sitofp %24 : i32 to f64
      %54 = arith.addf %53, %23 : f64
      %55 = memref.load %28[%c0] : memref<?xmemref<?xf64>>
      %c0_13 = arith.constant 0 : index
      memref.store %54, %55[%c0_13] : memref<?xf64>
      arts.yield
    }
    %32 = arith.addi %24, %c-1_i32 : i32
    %33 = arith.index_cast %32 : i32 to index
    %c7_i32 = arith.constant 7 : i32
    %c1_8 = arith.constant 1 : index
    %34 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %35 = "polygeist.pointer2memref"(%34) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
    %36 = func.call @artsDbCreatePtr(%35, %5, %c7_i32) : (memref<?xi8>, i32, i32) -> i64
    %37 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
    %38 = "arts.undef"() : () -> memref<1xf64>
    %39 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
    %c7_i32_9 = arith.constant 7 : i32
    %c1_10 = arith.constant 1 : index
    %40 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %41 = "polygeist.pointer2memref"(%40) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
    %42 = func.call @artsDbCreatePtr(%41, %5, %c7_i32_9) : (memref<?xi8>, i32, i32) -> i64
    %43 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%33] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
    %44 = "arts.undef"() : () -> memref<1xf64>
    %45 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%33] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
    %c9_i32_11 = arith.constant 9 : i32
    %c1_12 = arith.constant 1 : index
    %46 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %47 = "polygeist.pointer2memref"(%46) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
    %48 = func.call @artsDbCreatePtr(%47, %7, %c9_i32_11) : (memref<?xi8>, i32, i32) -> i64
    %49 = arts.datablock "out", %alloca_5 : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
    %50 = "arts.undef"() : () -> memref<1xf64>
    %51 = arts.datablock "out", %alloca_5 : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
    %52 = memref.load %22[%33] : memref<100xi64>
    arts.edt dependencies(%37, %43, %49) : (memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>), events(%31, %52, %c-1_i32) : (i64, i64, i32) {
      %53 = memref.load %37[%c0] : memref<?xmemref<?xf64>>
      %c0_13 = arith.constant 0 : index
      %54 = memref.load %53[%c0_13] : memref<?xf64>
      %55 = memref.load %43[%c0] : memref<?xmemref<?xf64>>
      %c0_14 = arith.constant 0 : index
      %56 = memref.load %55[%c0_14] : memref<?xf64>
      %57 = arith.cmpi sgt, %24, %c0_i32 : i32
      %58 = arith.select %57, %56, %cst : f64
      %59 = arith.addf %54, %58 : f64
      %60 = memref.load %49[%c0] : memref<?xmemref<?xf64>>
      %c0_15 = arith.constant 0 : index
      memref.store %59, %60[%c0_15] : memref<?xf64>
      arts.yield
    }
  }
  return
}
[convert-arts-to-funcs] Skipping arts.event: %22 = arts.event %9 {grouped} : memref<100xi64>
[convert-arts-to-funcs] Lowering arts.edt: arts.edt dependencies(%28) : (memref<?xmemref<?xf64>>), events(%31) : (i64) {
  %53 = arith.sitofp %24 : i32 to f64
  %54 = arith.addf %53, %23 : f64
  %55 = memref.load %28[%c0] : memref<?xmemref<?xf64>>
  %c0_13 = arith.constant 0 : index
  memref.store %54, %55[%c0_13] : memref<?xf64>
  arts.yield
}
-----------------------------------------
Naive collection of parameters and dependencies: 
  Adding parameter: %42 = "arith.index_cast"(%arg4) : (index) -> i32
  Adding parameter: %41 = "arith.sitofp"(%11) : (i32) -> f64
-----------------------------------------
-----------------------------------------
EDT Environment:
- Parameters:
   %42 = "arith.index_cast"(%arg4) : (index) -> i32
   %41 = "arith.sitofp"(%11) : (i32) -> f64

- Constants:
   %1 = "arith.constant"() <{value = 0 : index}> : () -> index

- Dependencies:
-----------------------------------------
[convert-arts-to-funcs] New function: func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
  %c0 = arith.constant 0 : index
  %c0_0 = arith.constant 0 : index
  %0 = memref.load %arg1[%c0_0] : memref<?xi64>
  %1 = arith.trunci %0 : i64 to i32
  %c1 = arith.constant 1 : index
  %2 = memref.load %arg1[%c1] : memref<?xi64>
  %3 = arith.sitofp %2 : i64 to f64
  %c0_1 = arith.constant 0 : index
  %c1_2 = arith.constant 1 : index
  %4 = memref.load %arg3[%c0_1] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
  %5 = call @artsGetPtrFromEdtDep(%4) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
  %6 = arith.addi %c0_1, %c1_2 : index
  %7 = "polygeist.memref2pointer"(%5) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
  %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
  %9 = arith.sitofp %1 : i32 to f64
  %10 = arith.addf %9, %3 : f64
  %11 = memref.load %8[%c0] : memref<?xmemref<?xf64>>
  %c0_3 = arith.constant 0 : index
  memref.store %10, %11[%c0_3] : memref<?xf64>
  return
}
[convert-arts-to-funcs] Lowering arts.edt: "arts.edt"(%74, %82, %90, %51, %93, %2) <{operandSegmentSizes = array<i32: 3, 3>}> ({
  %94 = "memref.load"(%74, %1) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>
  %95 = "arith.constant"() <{value = 0 : index}> : () -> index
  %96 = "memref.load"(%94, %95) <{nontemporal = false}> : (memref<?xf64>, index) -> f64
  %97 = "memref.load"(%82, %1) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>
  %98 = "arith.constant"() <{value = 0 : index}> : () -> index
  %99 = "memref.load"(%97, %98) <{nontemporal = false}> : (memref<?xf64>, index) -> f64
  %100 = "arith.cmpi"(%42, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1
  %101 = "arith.select"(%100, %99, %4) : (i1, f64, f64) -> f64
  %102 = "arith.addf"(%96, %101) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
  %103 = "memref.load"(%90, %1) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>
  %104 = "arith.constant"() <{value = 0 : index}> : () -> index
  "memref.store"(%102, %103, %104) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()
  "arts.yield"() : () -> ()
}) : (memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, i64, i64, i32) -> ()
-----------------------------------------
Naive collection of parameters and dependencies: 
  Adding parameter: %42 = "arith.index_cast"(%arg4) : (index) -> i32
-----------------------------------------
-----------------------------------------
EDT Environment:
- Parameters:
   %42 = "arith.index_cast"(%arg4) : (index) -> i32

- Constants:
   %1 = "arith.constant"() <{value = 0 : index}> : () -> index
   %3 = "arith.constant"() <{value = 0 : i32}> : () -> i32
   %4 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64

- Dependencies:
-----------------------------------------
[convert-arts-to-funcs] New function: func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
  %c0 = arith.constant 0 : index
  %c0_i32 = arith.constant 0 : i32
  %cst = arith.constant 0.000000e+00 : f64
  %c0_0 = arith.constant 0 : index
  %0 = memref.load %arg1[%c0_0] : memref<?xi64>
  %1 = arith.trunci %0 : i64 to i32
  %c0_1 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %2 = memref.load %arg3[%c0_1] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
  %3 = call @artsGetPtrFromEdtDep(%2) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
  %4 = arith.addi %c0_1, %c1 : index
  %5 = "polygeist.memref2pointer"(%3) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
  %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
  %7 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
  %8 = call @artsGetPtrFromEdtDep(%7) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
  %9 = arith.addi %4, %c1 : index
  %10 = "polygeist.memref2pointer"(%8) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
  %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
  %12 = memref.load %arg3[%9] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
  %13 = call @artsGetPtrFromEdtDep(%12) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
  %14 = arith.addi %9, %c1 : index
  %15 = "polygeist.memref2pointer"(%13) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
  %16 = "polygeist.pointer2memref"(%15) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
  %17 = memref.load %6[%c0] : memref<?xmemref<?xf64>>
  %c0_2 = arith.constant 0 : index
  %18 = memref.load %17[%c0_2] : memref<?xf64>
  %19 = memref.load %11[%c0] : memref<?xmemref<?xf64>>
  %c0_3 = arith.constant 0 : index
  %20 = memref.load %19[%c0_3] : memref<?xf64>
  %21 = arith.cmpi sgt, %1, %c0_i32 : i32
  %22 = arith.select %21, %20, %cst : f64
  %23 = arith.addf %18, %22 : f64
  %24 = memref.load %16[%c0] : memref<?xmemref<?xf64>>
  %c0_4 = arith.constant 0 : index
  memref.store %23, %24[%c0_4] : memref<?xf64>
  return
}
[convert-arts-to-funcs] Skipping arts.event: %40 = "arts.event"(%20) {grouped} : (index) -> memref<100xi64>
=== ConvertArtsToFuncsPass COMPLETE ===
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsGetPtrFromEdtDep(!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreatePtrAndGuidArrayFromDeps(memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreatePtr(memref<?xi8>, i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateArray(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100xf64>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %c9_i32 = arith.constant 9 : i32
    %c1_1 = arith.constant 1 : index
    %2 = "polygeist.memref2pointer"(%alloca_0) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %c8_i32 = arith.constant 8 : i32
    %4 = arith.muli %c1_1, %c100 : index
    %alloca_2 = memref.alloca(%4) : memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %5 = arith.index_cast %4 : index to i32
    %6 = arith.muli %5, %c8_i32 : i32
    call @artsDbCreateArray(%alloca_2, %c8_i32, %c9_i32, %5, %3) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %7 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    %8 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
    %c9_i32_3 = arith.constant 9 : i32
    %c1_4 = arith.constant 1 : index
    %9 = "polygeist.memref2pointer"(%alloca) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %10 = "polygeist.pointer2memref"(%9) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %c8_i32_5 = arith.constant 8 : i32
    %11 = arith.muli %c1_4, %c100 : index
    %alloca_6 = memref.alloca(%11) : memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %12 = arith.index_cast %11 : index to i32
    %13 = arith.muli %12, %c8_i32_5 : i32
    call @artsDbCreateArray(%alloca_6, %c8_i32_5, %c9_i32_3, %12, %10) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %14 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    %15 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<100xf64>
    %16 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_7 = arith.constant 0 : i32
    %alloca_8 = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca_8 : memref<0xi64> to memref<?xi64>
    %c1_i32 = arith.constant 1 : i32
    %17 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %19 = call @artsEdtCreate(%18, %16, %c0_i32_7, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_9 = arith.constant 0 : i32
    %20 = call @artsInitializeAndStartEpoch(%19, %c0_i32_9) : (i64, i32) -> i64
    %21 = call @artsGetCurrentNode() : () -> i32
    %c0_10 = arith.constant 0 : index
    %22 = arith.addi %c0_10, %4 : index
    %23 = arith.addi %22, %11 : index
    %24 = arith.index_cast %23 : index to i32
    %c6_i32 = arith.constant 6 : i32
    %alloca_11 = memref.alloca() : memref<6xi64>
    %25 = arith.index_cast %4 : index to i64
    %c0_12 = arith.constant 0 : index
    affine.store %25, %alloca_11[%c0_12] : memref<6xi64>
    %26 = arith.extsi %1 : i32 to i64
    %c1_13 = arith.constant 1 : index
    affine.store %26, %alloca_11[%c1_13] : memref<6xi64>
    %27 = arith.extsi %6 : i32 to i64
    %c2 = arith.constant 2 : index
    affine.store %27, %alloca_11[%c2] : memref<6xi64>
    %28 = arith.extsi %13 : i32 to i64
    %c3 = arith.constant 3 : index
    affine.store %28, %alloca_11[%c3] : memref<6xi64>
    %29 = arith.index_cast %4 : index to i64
    %c4 = arith.constant 4 : index
    affine.store %29, %alloca_11[%c4] : memref<6xi64>
    %30 = arith.index_cast %11 : index to i64
    %c5 = arith.constant 5 : index
    affine.store %30, %alloca_11[%c5] : memref<6xi64>
    %cast_14 = memref.cast %alloca_11 : memref<6xi64> to memref<?xi64>
    %31 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %32 = "polygeist.pointer2memref"(%31) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %33 = call @artsEdtCreateWithEpoch(%32, %21, %c6_i32, %cast_14, %24, %20) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %34 = llvm.mlir.addressof @str0 : !llvm.ptr
    %35 = llvm.getelementptr %34[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %36 = affine.load %alloca_0[0] : memref<100xf64>
    %37 = llvm.call @printf(%35, %36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c100 = arith.constant 100 : index
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %c1_1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_1] : memref<?xi64>
    %3 = arith.trunci %2 : i64 to i32
    %c2 = arith.constant 2 : index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.trunci %4 : i64 to i32
    %c3 = arith.constant 3 : index
    %6 = memref.load %arg1[%c3] : memref<?xi64>
    %7 = arith.trunci %6 : i64 to i32
    %c4 = arith.constant 4 : index
    %8 = memref.load %arg1[%c4] : memref<?xi64>
    %9 = arith.index_cast %8 : i64 to index
    %c5 = arith.constant 5 : index
    %10 = memref.load %arg1[%c5] : memref<?xi64>
    %11 = arith.index_cast %10 : i64 to index
    %c0_2 = arith.constant 0 : index
    %c1_3 = arith.constant 1 : index
    %alloca = memref.alloca(%9) : memref<?xmemref<?xf64>>
    %alloca_4 = memref.alloca(%9) : memref<?xi64>
    %12 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    %14 = arith.index_cast %9 : index to i32
    %15 = arith.index_cast %c0_2 : index to i32
    call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_4, %13, %14, %arg3, %15) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %16 = arith.addi %c0_2, %9 : index
    %alloca_5 = memref.alloca(%11) : memref<?xmemref<?xf64>>
    %alloca_6 = memref.alloca(%11) : memref<?xi64>
    %17 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    %19 = arith.index_cast %11 : index to i32
    %20 = arith.index_cast %16 : index to i32
    call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_6, %18, %19, %arg3, %20) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %21 = arith.addi %16, %11 : index
    %22 = arts.event %9 {grouped} : memref<100xi64>
    %23 = arith.sitofp %3 : i32 to f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %24 = arith.index_cast %arg4 : index to i32
      %c9_i32 = arith.constant 9 : i32
      %c1_7 = arith.constant 1 : index
      %25 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
      %26 = "polygeist.pointer2memref"(%25) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
      %27 = func.call @artsDbCreatePtr(%26, %5, %c9_i32) : (memref<?xi8>, i32, i32) -> i64
      %28 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %29 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
      %30 = memref.load %22[%arg4] : memref<100xi64>
      %31 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %32 = func.call @artsGetCurrentNode() : () -> i32
      %c0_8 = arith.constant 0 : index
      %33 = arith.addi %c0_8, %c1_7 : index
      %34 = arith.index_cast %33 : index to i32
      %c2_i32 = arith.constant 2 : i32
      %alloca_9 = memref.alloca() : memref<2xi64>
      %35 = arith.extsi %24 : i32 to i64
      %c0_10 = arith.constant 0 : index
      affine.store %35, %alloca_9[%c0_10] : memref<2xi64>
      %36 = arith.fptosi %23 : f64 to i64
      %c1_11 = arith.constant 1 : index
      affine.store %36, %alloca_9[%c1_11] : memref<2xi64>
      %cast = memref.cast %alloca_9 : memref<2xi64> to memref<?xi64>
      %37 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %38 = "polygeist.pointer2memref"(%37) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %39 = func.call @artsEdtCreateWithEpoch(%38, %32, %c2_i32, %cast, %34, %31) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %40 = arith.addi %24, %c-1_i32 : i32
      %41 = arith.index_cast %40 : i32 to index
      %c7_i32 = arith.constant 7 : i32
      %c1_12 = arith.constant 1 : index
      %42 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
      %43 = "polygeist.pointer2memref"(%42) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
      %44 = func.call @artsDbCreatePtr(%43, %5, %c7_i32) : (memref<?xi8>, i32, i32) -> i64
      %45 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %46 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
      %c7_i32_13 = arith.constant 7 : i32
      %c1_14 = arith.constant 1 : index
      %47 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
      %48 = "polygeist.pointer2memref"(%47) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
      %49 = func.call @artsDbCreatePtr(%48, %5, %c7_i32_13) : (memref<?xi8>, i32, i32) -> i64
      %50 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%41] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %51 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%41] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
      %c9_i32_15 = arith.constant 9 : i32
      %c1_16 = arith.constant 1 : index
      %52 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
      %53 = "polygeist.pointer2memref"(%52) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xi8>
      %54 = func.call @artsDbCreatePtr(%53, %7, %c9_i32_15) : (memref<?xi8>, i32, i32) -> i64
      %55 = arts.datablock "out", %alloca_5 : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %56 = arts.datablock "out", %alloca_5 : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<1xf64>
      %57 = memref.load %22[%41] : memref<100xi64>
      %58 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %59 = func.call @artsGetCurrentNode() : () -> i32
      %c0_17 = arith.constant 0 : index
      %60 = arith.addi %c0_17, %c1_12 : index
      %61 = arith.addi %60, %c1_14 : index
      %62 = arith.addi %61, %c1_16 : index
      %63 = arith.index_cast %62 : index to i32
      %c1_i32 = arith.constant 1 : i32
      %alloca_18 = memref.alloca() : memref<1xi64>
      %64 = arith.extsi %24 : i32 to i64
      %c0_19 = arith.constant 0 : index
      affine.store %64, %alloca_18[%c0_19] : memref<1xi64>
      %cast_20 = memref.cast %alloca_18 : memref<1xi64> to memref<?xi64>
      %65 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %66 = "polygeist.pointer2memref"(%65) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %67 = func.call @artsEdtCreateWithEpoch(%66, %59, %c1_i32, %cast_20, %63, %58) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c0 = arith.constant 0 : index
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %c1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %c0_1 = arith.constant 0 : index
    %c1_2 = arith.constant 1 : index
    %4 = memref.load %arg3[%c0_1] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %5 = call @artsGetPtrFromEdtDep(%4) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %6 = arith.addi %c0_1, %c1_2 : index
    %7 = "polygeist.memref2pointer"(%5) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
    %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
    %9 = arith.sitofp %1 : i32 to f64
    %10 = arith.addf %9, %3 : f64
    %11 = memref.load %8[%c0] : memref<?xmemref<?xf64>>
    %c0_3 = arith.constant 0 : index
    memref.store %10, %11[%c0_3] : memref<?xf64>
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %c0_1 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %2 = memref.load %arg3[%c0_1] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %3 = call @artsGetPtrFromEdtDep(%2) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %4 = arith.addi %c0_1, %c1 : index
    %5 = "polygeist.memref2pointer"(%3) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
    %7 = memref.load %arg3[%4] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %8 = call @artsGetPtrFromEdtDep(%7) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %9 = arith.addi %4, %c1 : index
    %10 = "polygeist.memref2pointer"(%8) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
    %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
    %12 = memref.load %arg3[%9] : memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
    %13 = call @artsGetPtrFromEdtDep(%12) : (!llvm.struct<(i64, i32, memref<?xi8>)>) -> memref<?xi8>
    %14 = arith.addi %9, %c1 : index
    %15 = "polygeist.memref2pointer"(%13) : (memref<?xi8>) -> !llvm.ptr<memref<?xi8>>
    %16 = "polygeist.pointer2memref"(%15) : (!llvm.ptr<memref<?xi8>>) -> memref<?xmemref<?xf64>>
    %17 = memref.load %6[%c0] : memref<?xmemref<?xf64>>
    %c0_2 = arith.constant 0 : index
    %18 = memref.load %17[%c0_2] : memref<?xf64>
    %19 = memref.load %11[%c0] : memref<?xmemref<?xf64>>
    %c0_3 = arith.constant 0 : index
    %20 = memref.load %19[%c0_3] : memref<?xf64>
    %21 = arith.cmpi sgt, %1, %c0_i32 : i32
    %22 = arith.select %21, %20, %cst : f64
    %23 = arith.addf %18, %22 : f64
    %24 = memref.load %16[%c0] : memref<?xmemref<?xf64>>
    %c0_4 = arith.constant 0 : index
    memref.store %23, %24[%c0_4] : memref<?xf64>
    return
  }
}
carts-opt: /home/randres/projects/carts/external/Polygeist/lib/polygeist/Ops.cpp:1957: virtual mlir::LogicalResult MetaPointer2Memref<mlir::memref::LoadOp>::matchAndRewrite(Op, mlir::PatternRewriter &) const [Op = mlir::memref::LoadOp]: Assertion `val.getType().cast<LLVM::LLVMPointerType>().isOpaque()' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt taskwithdeps-events.mlir --convert-arts-to-funcs --cse --canonicalize -debug-only=convert-arts-to-funcs,arts-codegen,edt-analysis
 #0 0x000055ad141b6097 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xab3097)
 #1 0x000055ad141b3c6e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xab0c6e)
 #2 0x000055ad141b674a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fd32a6bf520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007fd32a7139fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007fd32a6bf476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007fd32a6a57f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007fd32a6a571b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007fd32a6b6e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x000055ad13d93e9d MetaPointer2Memref<mlir::memref::LoadOp>::matchAndRewrite(mlir::memref::LoadOp, mlir::PatternRewriter&) const (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x690e9d)
#10 0x000055ad13edac97 void llvm::function_ref<void ()>::callback_fn<mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>)::$_2>(long) PatternApplicator.cpp:0:0
#11 0x000055ad13ed75af mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7d45af)
#12 0x000055ad13ec5f1d (anonymous namespace)::GreedyPatternRewriteDriver::processWorklist() GreedyPatternRewriteDriver.cpp:0:0
#13 0x000055ad13ec30fc mlir::applyPatternsAndFoldGreedily(mlir::Region&, mlir::FrozenRewritePatternSet const&, mlir::GreedyRewriteConfig, bool*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7c00fc)
#14 0x000055ad13e8cebb (anonymous namespace)::Canonicalizer::runOnOperation() Canonicalizer.cpp:0:0
#15 0x000055ad14050214 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x94d214)
#16 0x000055ad14050841 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x94d841)
#17 0x000055ad14052cf2 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x94fcf2)
#18 0x000055ad13d537e4 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#19 0x000055ad13d52a54 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#20 0x000055ad14153b48 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xa50b48)
#21 0x000055ad13d4d03a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x64a03a)
#22 0x000055ad13d4d504 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x64a504)
#23 0x000055ad1378b441 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x88441)
#24 0x00007fd32a6a6d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#25 0x00007fd32a6a6e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#26 0x000055ad1378aa95 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x87a95)
