--- ConvertARTSToFuncsPass START ---
Preprocessing datablocks

//===-------------------------------------------===//
Legalizing operation : 'builtin.module'(0x562d5eb7c690) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'llvm.mlir.global'(0x562d5eb55fa0) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'llvm.func'(0x562d5eb4fd10) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'func.func'(0x562d5eb48950) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb631e0) {
  %0 = "arith.constant"() <{value = 100 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb63d70) {
  %1 = "arith.constant"() <{value = 1 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb63e40) {
  %2 = "arith.constant"() <{value = 0 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb64390) {
  %3 = "arith.constant"() <{value = -1 : i32}> : () -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb648e0) {
  %4 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb64e30) {
  %5 = "arith.constant"() <{value = 0 : i32}> : () -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb65380) {
  %6 = "arith.constant"() <{value = 100 : i32}> : () -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.alloca'(0x562d5eb2c380) {
  %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.alloca'(0x562d5eb2c460) {
  %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'func.call'(0x562d5eb6dc50) {
  %9 = "func.call"() <{callee = @rand}> : () -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.remsi'(0x562d5eb6dd00) {
  %10 = "arith.remsi"(%9, %6) : (i32, i32) -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.datablock'(0x562d5eb72f80) {
  %11 = "arts.datablock"(%8, %2, %0, %1) <{mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<?xmemref<?xf64>>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.datablock'(0x562d5eb2ec20) {
  %12 = "arts.datablock"(%7, %2, %0, %1) <{mode = "inout", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> : (memref<100xf64>, index, index, index) -> memref<?xmemref<?xf64>>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.parallel'(0x562d5eb6f200) {
  * Fold {
  } -> FAILURE : unable to fold

  * Pattern : 'arts.parallel -> ()' {
[convert-arts-to-funcs] Lowering arts.parallel

 - Datablock: %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
 - Datablock: %8 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
 - Array of Datablocks: %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
 - Array of Datablocks: %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
    ** Erase   : 'arts.parallel'(0x562d5eb6f200)
  } -> SUCCESS : pattern applied successfully
} -> SUCCESS
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.barrier'(0x562d5eb6f790) {
  "arts.barrier"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.single'(0x562d5eb524b0) {
} -> SUCCESS : operation marked 'ignored' during conversion
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.sitofp'(0x562d5eb6f8b0) {
  %31 = "arith.sitofp"(%8) : (i32) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'scf.for'(0x562d5eb722b0) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.index_cast'(0x562d5eb72410) {
  %32 = "arith.index_cast"(%arg4) : (index) -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.datablock'(0x562d5eb6f0d0) {
  %33 = "arts.datablock"(%17, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {baseIsDb, isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<?xmemref<?xf64>>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.edt'(0x562d5eb73830) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.sitofp'(0x562d5eb730d0) {
  %45 = "arith.sitofp"(%32) : (i32) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.addf'(0x562d5eb62af0) {
  %46 = "arith.addf"(%45, %31) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.load'(0x562d5eb62a40) {
  %47 = "memref.load"(%33, %1) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb8a250) {
  %48 = "arith.constant"() <{value = 0 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.store'(0x562d5eb8a2b0) {
  "memref.store"(%46, %47, %48) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.yield'(0x562d5eb73070) {
  "arts.yield"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.load'(0x562d5eb73a50) {
  %34 = "memref.load"(%17, %arg4) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb8a130) {
  %35 = "arith.constant"() <{value = 0 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.load'(0x562d5eb8a1a0) {
  %36 = "memref.load"(%34, %35) <{nontemporal = false}> : (memref<?xf64>, index) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.addi'(0x562d5eb73960) {
  %37 = "arith.addi"(%32, %2) : (i32, i32) -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.index_cast'(0x562d5eb731f0) {
  %38 = "arith.index_cast"(%37) : (i32) -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.load'(0x562d5eb831e0) {
  %39 = "memref.load"(%17, %38) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb83290) {
  %40 = "arith.constant"() <{value = 0 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.load'(0x562d5eb83300) {
  %41 = "memref.load"(%39, %40) <{nontemporal = false}> : (memref<?xf64>, index) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.datablock'(0x562d5eb72db0) {
  %42 = "arts.datablock"(%17, %arg4, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {baseIsDb, isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<?xmemref<?xf64>>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.datablock'(0x562d5eb73b60) {
  %43 = "arts.datablock"(%17, %38, %0, %0) <{mode = "in", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {baseIsDb, isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<?xmemref<?xf64>>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.datablock'(0x562d5eb73cc0) {
  %44 = "arts.datablock"(%24, %arg4, %0, %0) <{mode = "out", operandSegmentSizes = array<i32: 1, 1, 1, 1>}> {baseIsDb, isLoad} : (memref<?xmemref<?xf64>>, index, index, index) -> memref<?xmemref<?xf64>>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.edt'(0x562d5eb691e0) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.cmpi'(0x562d5eb74070) {
  %45 = "arith.cmpi"(%32, %3) <{predicate = 4 : i64}> : (i32, i32) -> i1

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.select'(0x562d5eb74560) {
  %46 = "arith.select"(%45, %41, %4) : (i1, f64, f64) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.addf'(0x562d5eb74690) {
  %47 = "arith.addf"(%36, %46) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.load'(0x562d5eb8a380) {
  %48 = "memref.load"(%44, %1) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arith.constant'(0x562d5eb8a430) {
  %49 = "arith.constant"() <{value = 0 : index}> : () -> index

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'memref.store'(0x562d5eb73770) {
  "memref.store"(%47, %48, %49) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.yield'(0x562d5eb73ff0) {
  "arts.yield"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'scf.yield'(0x562d5eb57860) {
  "scf.yield"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.yield'(0x562d5eb6f850) {
  "arts.yield"() : () -> ()


} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.barrier'(0x562d5eb723b0) {
  "arts.barrier"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'arts.yield'(0x562d5eb6f740) {
  "arts.yield"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'llvm.mlir.addressof'(0x562d5eb73dc0) {
  %58 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'llvm.getelementptr'(0x562d5eb74480) {
  %59 = "llvm.getelementptr"(%58) <{elem_type = !llvm.array<11 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'affine.load'(0x562d5eb47620) {
  %60 = "affine.load"(%8) <{map = affine_map<() -> (0)>}> : (memref<100xf64>) -> f64

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'llvm.call'(0x562d5eb59a50) {
  %61 = "llvm.call"(%59, %60) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, f64) -> i32

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'func.return'(0x562d5eb73710) {
  "func.return"() : () -> ()

} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//

//===-------------------------------------------===//
Legalizing operation : 'func.func'(0x562d5eb7c580) {
} -> SUCCESS : operation marked legal by the target
//===-------------------------------------------===//
=== ConvertARTSToFuncsPass COMPLETE ===
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsDbCreatePtrAndGuidArrayFromDeps(memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateArray(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
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
    %c9_i32_3 = arith.constant 9 : i32
    %c1_4 = arith.constant 1 : index
    %8 = "polygeist.memref2pointer"(%alloca) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %c8_i32_5 = arith.constant 8 : i32
    %10 = arith.muli %c1_4, %c100 : index
    %alloca_6 = memref.alloca(%10) : memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %11 = arith.index_cast %10 : index to i32
    %12 = arith.muli %11, %c8_i32_5 : i32
    call @artsDbCreateArray(%alloca_6, %c8_i32_5, %c9_i32_3, %11, %9) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %13 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    %14 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_7 = arith.constant 0 : i32
    %alloca_8 = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca_8 : memref<0xi64> to memref<?xi64>
    %c1_i32 = arith.constant 1 : i32
    %15 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %16 = "polygeist.pointer2memref"(%15) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %17 = call @artsEdtCreate(%16, %14, %c0_i32_7, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_9 = arith.constant 0 : i32
    %18 = call @artsInitializeAndStartEpoch(%17, %c0_i32_9) : (i64, i32) -> i64
    %19 = call @artsGetCurrentNode() : () -> i32
    %c0_10 = arith.constant 0 : index
    %20 = arith.addi %c0_10, %4 : index
    %21 = arith.addi %20, %10 : index
    %22 = arith.index_cast %21 : index to i32
    %c3_i32 = arith.constant 3 : i32
    %alloca_11 = memref.alloca() : memref<3xi64>
    %23 = arith.extsi %1 : i32 to i64
    %c0_12 = arith.constant 0 : index
    affine.store %23, %alloca_11[%c0_12] : memref<3xi64>
    %24 = arith.index_cast %4 : index to i64
    %c1_13 = arith.constant 1 : index
    affine.store %24, %alloca_11[%c1_13] : memref<3xi64>
    %25 = arith.index_cast %10 : index to i64
    %c2 = arith.constant 2 : index
    affine.store %25, %alloca_11[%c2] : memref<3xi64>
    %cast_14 = memref.cast %alloca_11 : memref<3xi64> to memref<?xi64>
    %26 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %27 = "polygeist.pointer2memref"(%26) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %28 = call @artsEdtCreateWithEpoch(%27, %19, %c3_i32, %cast_14, %22, %18) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %29 = llvm.mlir.addressof @str0 : !llvm.ptr
    %30 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %31 = affine.load %alloca_0[0] : memref<100xf64>
    %32 = llvm.call @printf(%30, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
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
    %1 = arith.trunci %0 : i64 to i32
    %c1_1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %c2 = arith.constant 2 : index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %c0_2 = arith.constant 0 : index
    %c1_3 = arith.constant 1 : index
    %alloca = memref.alloca(%3) : memref<?xmemref<?xf64>>
    %alloca_4 = memref.alloca(%3) : memref<?xi64>
    %6 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %7 = "polygeist.pointer2memref"(%6) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    %8 = arith.index_cast %3 : index to i32
    %9 = arith.index_cast %c0_2 : index to i32
    call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_4, %7, %8, %arg3, %9) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %10 = arith.addi %c0_2, %3 : index
    %alloca_5 = memref.alloca(%5) : memref<?xmemref<?xf64>>
    %alloca_6 = memref.alloca(%5) : memref<?xi64>
    %11 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    %13 = arith.index_cast %5 : index to i32
    %14 = arith.index_cast %10 : index to i32
    call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_6, %12, %13, %arg3, %14) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %15 = arith.addi %10, %5 : index
    %16 = arith.sitofp %1 : i32 to f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %17 = arith.index_cast %arg4 : index to i32
      %18 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      arts.edt parameters(%17, %16, %arg4) : (i32, f64, index), constants(%c0) : (index), dependencies(%18) : (memref<?xmemref<?xf64>>) {
        %28 = arith.sitofp %17 : i32 to f64
        %29 = arith.addf %28, %16 : f64
        %30 = memref.load %18[%c0] : memref<?xmemref<?xf64>>
        %c0_9 = arith.constant 0 : index
        memref.store %29, %30[%c0_9] : memref<?xf64>
      }
      %19 = memref.load %alloca[%arg4] : memref<?xmemref<?xf64>>
      %c0_7 = arith.constant 0 : index
      %20 = memref.load %19[%c0_7] : memref<?xf64>
      %21 = arith.addi %17, %c-1_i32 : i32
      %22 = arith.index_cast %21 : i32 to index
      %23 = memref.load %alloca[%22] : memref<?xmemref<?xf64>>
      %c0_8 = arith.constant 0 : index
      %24 = memref.load %23[%c0_8] : memref<?xf64>
      %25 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %26 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%22] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %27 = arts.datablock "out", %alloca_5 : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      arts.edt parameters(%17, %24, %20, %arg4) : (i32, f64, f64, index), constants(%c0_i32, %cst, %c0) : (i32, f64, index), dependencies(%25, %26, %27) : (memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) {
        %28 = arith.cmpi sgt, %17, %c0_i32 : i32
        %29 = arith.select %28, %24, %cst : f64
        %30 = arith.addf %20, %29 : f64
        %31 = memref.load %27[%c0] : memref<?xmemref<?xf64>>
        %c0_9 = arith.constant 0 : index
        memref.store %30, %31[%c0_9] : memref<?xf64>
      }
    }
    return
  }
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsDbCreatePtrAndGuidArrayFromDeps(memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateArray(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100_i64 = arith.constant 100 : i64
    %c200_i32 = arith.constant 200 : i32
    %c100 = arith.constant 100 : index
    %c100_i32 = arith.constant 100 : i32
    %c3_i32 = arith.constant 3 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c8_i32 = arith.constant 8 : i32
    %c9_i32 = arith.constant 9 : i32
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %alloca = memref.alloca() : memref<100xf64>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = "polygeist.memref2pointer"(%alloca_0) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %alloca_1 = memref.alloca() : memref<100x!llvm.struct<(i64, memref<?xi8>)>>
    %cast = memref.cast %alloca_1 : memref<100x!llvm.struct<(i64, memref<?xi8>)>> to memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    call @artsDbCreateArray(%cast, %c8_i32, %c9_i32, %c100_i32, %3) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %4 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    %5 = "polygeist.memref2pointer"(%alloca) : (memref<100xf64>) -> !llvm.ptr<memref<100xf64>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<memref<100xf64>>) -> memref<?xi8>
    %alloca_2 = memref.alloca() : memref<100x!llvm.struct<(i64, memref<?xi8>)>>
    %cast_3 = memref.cast %alloca_2 : memref<100x!llvm.struct<(i64, memref<?xi8>)>> to memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    call @artsDbCreateArray(%cast_3, %c8_i32, %c9_i32, %c100_i32, %6) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, i32, i32, memref<?xi8>) -> ()
    %7 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    %8 = call @artsGetCurrentNode() : () -> i32
    %alloca_4 = memref.alloca() : memref<0xi64>
    %cast_5 = memref.cast %alloca_4 : memref<0xi64> to memref<?xi64>
    %9 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %10 = "polygeist.pointer2memref"(%9) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %11 = call @artsEdtCreate(%10, %8, %c0_i32, %cast_5, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %12 = call @artsInitializeAndStartEpoch(%11, %c0_i32) : (i64, i32) -> i64
    %13 = call @artsGetCurrentNode() : () -> i32
    %alloca_6 = memref.alloca() : memref<3xi64>
    %14 = arith.extsi %1 : i32 to i64
    affine.store %14, %alloca_6[0] : memref<3xi64>
    affine.store %c100_i64, %alloca_6[1] : memref<3xi64>
    affine.store %c100_i64, %alloca_6[2] : memref<3xi64>
    %cast_7 = memref.cast %alloca_6 : memref<3xi64> to memref<?xi64>
    %15 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %16 = "polygeist.pointer2memref"(%15) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %17 = call @artsEdtCreateWithEpoch(%16, %13, %c3_i32, %cast_7, %c200_i32, %12) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %18 = llvm.mlir.addressof @str0 : !llvm.ptr
    %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %20 = affine.load %alloca_0[0] : memref<100xf64>
    %21 = llvm.call @printf(%19, %20) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) {
    %c0_i32 = arith.constant 0 : i32
    %c2 = arith.constant 2 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c100 = arith.constant 100 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %alloca = memref.alloca(%3) : memref<?xmemref<?xf64>>
    %alloca_0 = memref.alloca(%3) : memref<?xi64>
    %6 = "polygeist.memref2pointer"(%alloca) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %7 = "polygeist.pointer2memref"(%6) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    %8 = arith.index_cast %3 : index to i32
    call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_0, %7, %8, %arg3, %c0_i32) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %alloca_1 = memref.alloca(%5) : memref<?xmemref<?xf64>>
    %alloca_2 = memref.alloca(%5) : memref<?xi64>
    %9 = "polygeist.memref2pointer"(%alloca_1) : (memref<?xmemref<?xf64>>) -> !llvm.ptr<memref<?xmemref<?xf64>>>
    %10 = "polygeist.pointer2memref"(%9) : (!llvm.ptr<memref<?xmemref<?xf64>>>) -> memref<?xmemref<?xi8>>
    %11 = arith.index_cast %5 : index to i32
    %12 = arith.index_cast %3 : index to i32
    call @artsDbCreatePtrAndGuidArrayFromDeps(%alloca_2, %10, %11, %arg3, %12) : (memref<?xi64>, memref<?xmemref<?xi8>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %13 = arith.sitofp %1 : i32 to f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %14 = arith.index_cast %arg4 : index to i32
      %15 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      arts.edt parameters(%14, %13, %arg4) : (i32, f64, index), constants(%c0) : (index), dependencies(%15) : (memref<?xmemref<?xf64>>) {
        %25 = arith.sitofp %14 : i32 to f64
        %26 = arith.addf %25, %13 : f64
        %27 = memref.load %15[%c0] : memref<?xmemref<?xf64>>
        memref.store %26, %27[%c0] : memref<?xf64>
      }
      %16 = memref.load %alloca[%arg4] : memref<?xmemref<?xf64>>
      %17 = memref.load %16[%c0] : memref<?xf64>
      %18 = arith.addi %14, %c-1_i32 : i32
      %19 = arith.index_cast %18 : i32 to index
      %20 = memref.load %alloca[%19] : memref<?xmemref<?xf64>>
      %21 = memref.load %20[%c0] : memref<?xf64>
      %22 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %23 = arts.datablock "in", %alloca : memref<?xmemref<?xf64>>[%19] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      %24 = arts.datablock "out", %alloca_1 : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {baseIsDb, isLoad} : memref<?xmemref<?xf64>>
      arts.edt parameters(%14, %21, %17, %arg4) : (i32, f64, f64, index), constants(%c0_i32, %cst, %c0) : (i32, f64, index), dependencies(%22, %23, %24) : (memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) {
        %25 = arith.cmpi sgt, %14, %c0_i32 : i32
        %26 = arith.select %25, %21, %cst : f64
        %27 = arith.addf %17, %26 : f64
        %28 = memref.load %24[%c0] : memref<?xmemref<?xf64>>
        memref.store %27, %28[%c0] : memref<?xf64>
      }
    }
    return
  }
}

