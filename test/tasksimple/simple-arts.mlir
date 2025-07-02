-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str4("EDT 3: The final number is %d - %d.\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("EDT 2: The number is %d/%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("EDT 1: The number is %d/%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("EDT 0: The initial number is %d/%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Main EDT: Starting...\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %c10_i32 = arith.constant 10 : i32
    %c1_i32 = arith.constant 1 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
    %2 = "polygeist.memref2pointer"(%1) : (memref<memref<?xi8>>) -> !llvm.ptr
    %3 = llvm.load %2 : !llvm.ptr -> !llvm.ptr
    llvm.store %0, %3 : i32, !llvm.ptr
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi64>
    %6 = call @time(%5) : (memref<?xi64>) -> i64
    %7 = arith.trunci %6 : i64 to i32
    call @srand(%7) : (i32) -> ()
    %8 = llvm.mlir.addressof @str0 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %10 = llvm.call @printf(%9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %11 = call @rand() : () -> i32
    %12 = arith.remsi %11, %c100_i32 : i32
    %13 = arith.addi %12, %c1_i32 : i32
    %14 = "polygeist.memref2pointer"(%1) : (memref<memref<?xi8>>) -> !llvm.ptr
    %15 = llvm.load %14 : !llvm.ptr -> !llvm.ptr
    llvm.store %13, %15 : i32, !llvm.ptr
    %16 = call @rand() : () -> i32
    %17 = arith.remsi %16, %c10_i32 : i32
    %18 = arith.addi %17, %c1_i32 : i32
    %19 = llvm.mlir.addressof @str1 : !llvm.ptr
    %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %21 = llvm.call @printf(%20, %13, %18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %22 = arts.db_dep "in"(%1 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
    arts.epoch {
      arts.edt dependencies(%22) : (memref<memref<?xi8>>) attributes {task} {
        %32 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
        %33 = "polygeist.memref2pointer"(%32) : (memref<memref<?xi8>>) -> !llvm.ptr
        %34 = llvm.load %33 : !llvm.ptr -> !llvm.ptr
        llvm.store %0, %34 : i32, !llvm.ptr
        %35 = llvm.mlir.addressof @str2 : !llvm.ptr
        %36 = llvm.getelementptr %35[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
        %37 = "polygeist.memref2pointer"(%22) : (memref<memref<?xi8>>) -> !llvm.ptr
        %38 = llvm.load %37 : !llvm.ptr -> i32
        %39 = llvm.call @printf(%36, %38, %18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        %40 = "polygeist.memref2pointer"(%32) : (memref<memref<?xi8>>) -> !llvm.ptr
        %41 = llvm.load %40 : !llvm.ptr -> !llvm.ptr
        llvm.store %18, %41 : i32, !llvm.ptr
        %42 = arts.db_dep "inout"(%22 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
        %43 = arts.db_dep "inout"(%32 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
        arts.edt dependencies(%42, %43) : (memref<memref<?xi8>>, memref<memref<?xi8>>) attributes {task} {
          %44 = "polygeist.memref2pointer"(%42) : (memref<memref<?xi8>>) -> !llvm.ptr
          %45 = llvm.load %44 : !llvm.ptr -> i32
          %46 = arith.addi %45, %c1_i32 : i32
          %47 = "polygeist.memref2pointer"(%42) : (memref<memref<?xi8>>) -> !llvm.ptr
          llvm.store %46, %47 : i32, !llvm.ptr
          %48 = "polygeist.memref2pointer"(%43) : (memref<memref<?xi8>>) -> !llvm.ptr
          %49 = llvm.load %48 : !llvm.ptr -> i32
          %50 = arith.addi %49, %c1_i32 : i32
          %51 = "polygeist.memref2pointer"(%43) : (memref<memref<?xi8>>) -> !llvm.ptr
          llvm.store %50, %51 : i32, !llvm.ptr
          %52 = llvm.mlir.addressof @str3 : !llvm.ptr
          %53 = llvm.getelementptr %52[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
          %54 = llvm.call @printf(%53, %46, %50) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
          arts.yield
        }
        arts.yield
      }
    }
    %23 = "polygeist.memref2pointer"(%1) : (memref<memref<?xi8>>) -> !llvm.ptr
    %24 = llvm.load %23 : !llvm.ptr -> !llvm.ptr
    %25 = llvm.load %24 : !llvm.ptr -> i32
    %26 = arith.addi %25, %c1_i32 : i32
    %27 = "polygeist.memref2pointer"(%1) : (memref<memref<?xi8>>) -> !llvm.ptr
    %28 = llvm.load %27 : !llvm.ptr -> !llvm.ptr
    llvm.store %26, %28 : i32, !llvm.ptr
    %29 = llvm.mlir.addressof @str4 : !llvm.ptr
    %30 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %31 = llvm.call @printf(%30, %26, %18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-llvm] ArtsCodegen initialized successfully
[convert-arts-to-llvm] Starting operation processing
-----------------------------------------
1st pass - Runtime info operations
[convert-arts-to-llvm] Processing top-level DbAlloc: %1 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
[convert-arts-to-llvm] DbAlloc processing completed successfully
[convert-arts-to-llvm] Processing top-level DbAlloc: %35 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
[convert-arts-to-llvm] DbAlloc processing completed successfully
-----------------------------------------
2nd pass - Sync operations
[convert-arts-to-llvm] Processing Epoch: arts.epoch {
  arts.edt dependencies(%25) : (memref<memref<?xi8>>) attributes {task} {
    %35 = func.call @artsGetCurrentNode() : () -> i32
    %c1_i64_0 = arith.constant 1 : i64
    %c8_i32_1 = arith.constant 8 : i32
    %36 = func.call @artsReserveGuidRoute(%c8_i32_1, %35) : (i32, i32) -> i64
    %37 = func.call @artsDbCreateWithGuid(%36, %c1_i64_0) : (i64, i64) -> memref<?xi8>
    %38 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
    %39 = "polygeist.memref2pointer"(%38) : (memref<memref<?xi8>>) -> !llvm.ptr
    %40 = llvm.load %39 : !llvm.ptr -> !llvm.ptr
    llvm.store %0, %40 : i32, !llvm.ptr
    %41 = llvm.mlir.addressof @str2 : !llvm.ptr
    %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
    %43 = "polygeist.memref2pointer"(%25) : (memref<memref<?xi8>>) -> !llvm.ptr
    %44 = llvm.load %43 : !llvm.ptr -> i32
    %45 = llvm.call @printf(%42, %44, %21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %46 = "polygeist.memref2pointer"(%38) : (memref<memref<?xi8>>) -> !llvm.ptr
    %47 = llvm.load %46 : !llvm.ptr -> !llvm.ptr
    llvm.store %21, %47 : i32, !llvm.ptr
    %48 = arts.db_dep "inout"(%25 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
    %49 = arts.db_dep "inout"(%38 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
    arts.edt dependencies(%48, %49) : (memref<memref<?xi8>>, memref<memref<?xi8>>) attributes {task} {
      %50 = "polygeist.memref2pointer"(%48) : (memref<memref<?xi8>>) -> !llvm.ptr
      %51 = llvm.load %50 : !llvm.ptr -> i32
      %52 = arith.addi %51, %c1_i32 : i32
      %53 = "polygeist.memref2pointer"(%48) : (memref<memref<?xi8>>) -> !llvm.ptr
      llvm.store %52, %53 : i32, !llvm.ptr
      %54 = "polygeist.memref2pointer"(%49) : (memref<memref<?xi8>>) -> !llvm.ptr
      %55 = llvm.load %54 : !llvm.ptr -> i32
      %56 = arith.addi %55, %c1_i32 : i32
      %57 = "polygeist.memref2pointer"(%49) : (memref<memref<?xi8>>) -> !llvm.ptr
      llvm.store %56, %57 : i32, !llvm.ptr
      %58 = llvm.mlir.addressof @str3 : !llvm.ptr
      %59 = llvm.getelementptr %58[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      %60 = llvm.call @printf(%59, %52, %56) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      arts.yield
    }
    arts.yield
  }
}
[convert-arts-to-llvm] Mapped 1 child EDTs to epoch
-----------------------------------------
3rd pass - EDT operations
[convert-arts-to-llvm] Processing EDT: arts.edt dependencies(%25) : (memref<memref<?xi8>>) attributes {task} {
  %45 = func.call @artsGetCurrentNode() : () -> i32
  %c1_i64_5 = arith.constant 1 : i64
  %c8_i32_6 = arith.constant 8 : i32
  %46 = func.call @artsReserveGuidRoute(%c8_i32_6, %45) : (i32, i32) -> i64
  %47 = func.call @artsDbCreateWithGuid(%46, %c1_i64_5) : (i64, i64) -> memref<?xi8>
  %48 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
  %49 = "polygeist.memref2pointer"(%48) : (memref<memref<?xi8>>) -> !llvm.ptr
  %50 = llvm.load %49 : !llvm.ptr -> !llvm.ptr
  llvm.store %0, %50 : i32, !llvm.ptr
  %51 = llvm.mlir.addressof @str2 : !llvm.ptr
  %52 = llvm.getelementptr %51[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
  %53 = "polygeist.memref2pointer"(%25) : (memref<memref<?xi8>>) -> !llvm.ptr
  %54 = llvm.load %53 : !llvm.ptr -> i32
  %55 = llvm.call @printf(%52, %54, %21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
  %56 = "polygeist.memref2pointer"(%48) : (memref<memref<?xi8>>) -> !llvm.ptr
  %57 = llvm.load %56 : !llvm.ptr -> !llvm.ptr
  llvm.store %21, %57 : i32, !llvm.ptr
  %58 = arts.db_dep "inout"(%25 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
  %59 = arts.db_dep "inout"(%48 : memref<memref<?xi8>>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
  arts.edt dependencies(%58, %59) : (memref<memref<?xi8>>, memref<memref<?xi8>>) attributes {task} {
    %60 = "polygeist.memref2pointer"(%58) : (memref<memref<?xi8>>) -> !llvm.ptr
    %61 = llvm.load %60 : !llvm.ptr -> i32
    %62 = arith.addi %61, %c1_i32 : i32
    %63 = "polygeist.memref2pointer"(%58) : (memref<memref<?xi8>>) -> !llvm.ptr
    llvm.store %62, %63 : i32, !llvm.ptr
    %64 = "polygeist.memref2pointer"(%59) : (memref<memref<?xi8>>) -> !llvm.ptr
    %65 = llvm.load %64 : !llvm.ptr -> i32
    %66 = arith.addi %65, %c1_i32 : i32
    %67 = "polygeist.memref2pointer"(%59) : (memref<memref<?xi8>>) -> !llvm.ptr
    llvm.store %66, %67 : i32, !llvm.ptr
    %68 = llvm.mlir.addressof @str3 : !llvm.ptr
    %69 = llvm.getelementptr %68[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
    %70 = llvm.call @printf(%69, %62, %66) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    arts.yield
  }
  arts.yield
}
- Recording in-mode dependencies
- Replacing EDT dependency uses
[convert-arts-to-llvm] Processing EDT: "arts.edt"(%40, %41) ({
  %42 = "polygeist.memref2pointer"(%40) : (memref<memref<?xi8>>) -> !llvm.ptr
  %43 = "llvm.load"(%42) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %44 = "arith.addi"(%43, %1) : (i32, i32) -> i32
  %45 = "polygeist.memref2pointer"(%40) : (memref<memref<?xi8>>) -> !llvm.ptr
  "llvm.store"(%44, %45) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %46 = "polygeist.memref2pointer"(%41) : (memref<memref<?xi8>>) -> !llvm.ptr
  %47 = "llvm.load"(%46) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %48 = "arith.addi"(%47, %1) : (i32, i32) -> i32
  %49 = "polygeist.memref2pointer"(%41) : (memref<memref<?xi8>>) -> !llvm.ptr
  "llvm.store"(%48, %49) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %50 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
  %51 = "llvm.getelementptr"(%50) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %52 = "llvm.call"(%51, %44, %48) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
  "arts.yield"() : () -> ()
}) {task} : (memref<memref<?xi8>>, memref<memref<?xi8>>) -> ()
- Recording in-mode dependencies
- Incrementing latch counts for out-mode dependencies
- Replacing EDT dependency uses
-----------------------------------------
Operation processing completed successfully
[arts-codegen] Applying 3 delayed replacements
[convert-arts-to-llvm] Cleaning up 5 operations

-----------------------------------------
ARTS to LLVM Conversion Statistics
-----------------------------------------
EDT operations: 2
Epoch operations: 1
Barrier operations: 0
Alloc operations: 0
DbAlloc operations: 2
DbDep operations: 0
Runtime Info operations: 0
-----------------------------------------
-----------------------------------------
ConvertArtsToLLVMPass COMPLETED
-----------------------------------------
"builtin.module"() ({
  "func.func"() <{function_type = (i32, memref<?xmemref<?xi8>>) -> i32, sym_name = "artsRT", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xmemref<?xi8>>) -> i32, sym_name = "main"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xmemref<?xi8>>):
    %0 = "func.call"(%arg0, %arg1) <{callee = @artsRT}> : (i32, memref<?xmemref<?xi8>>) -> i32
    %1 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    "func.return"(%1) : (i32) -> ()
  }) : () -> ()
  "func.func"() <{function_type = () -> (), sym_name = "artsShutdown", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xmemref<?xi8>>) -> (), sym_name = "artsMain"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xmemref<?xi8>>):
    %0 = "func.call"() <{callee = @mainBody}> : () -> i32
    "func.call"() <{callee = @artsShutdown}> : () -> ()
    "func.return"() : () -> ()
  }) : () -> ()
  "func.func"() <{function_type = (i64) -> (), sym_name = "artsDbIncrementLatch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>) -> (), sym_name = "__arts_edt_3", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>):
    %0 = "func.call"() <{callee = @artsGetCurrentGuid}> : () -> i64
    %1 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %2 = "arith.constant"() <{value = 0 : index}> : () -> index
    %3 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    "memref.store"(%2, %3) : (index, memref<index>) -> ()
    %4 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %5 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %6 = "arith.constant"() <{value = 1 : index}> : () -> index
    %7 = "memref.load"(%3) : (memref<index>) -> index
    %8 = "arith.muli"(%7, %4) : (index, index) -> index
    %9 = "arith.index_cast"(%8) : (index) -> i32
    %10 = "llvm.getelementptr"(%5, %9) <{elem_type = i8, rawConstantIndices = array<i32: -2147483648>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %11 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %12 = "llvm.getelementptr"(%10, %11) <{elem_type = !llvm.struct<(i64, i32, ptr)>, rawConstantIndices = array<i32: -2147483648, 0>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %13 = "llvm.load"(%12) <{ordering = 0 : i64}> : (!llvm.ptr) -> i64
    %14 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %15 = "arith.constant"() <{value = 2 : i32}> : () -> i32
    %16 = "llvm.getelementptr"(%10, %14) <{elem_type = !llvm.struct<(i64, i32, ptr)>, rawConstantIndices = array<i32: -2147483648, 2>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %17 = "llvm.load"(%16) <{ordering = 0 : i64}> : (!llvm.ptr) -> memref<?xi8>
    %18 = "arith.addi"(%7, %6) : (index, index) -> index
    "memref.store"(%18, %3) : (index, memref<index>) -> ()
    %19 = "arith.constant"() <{value = 1 : index}> : () -> index
    %20 = "memref.load"(%3) : (memref<index>) -> index
    %21 = "arith.muli"(%20, %4) : (index, index) -> index
    %22 = "arith.index_cast"(%21) : (index) -> i32
    %23 = "llvm.getelementptr"(%5, %22) <{elem_type = i8, rawConstantIndices = array<i32: -2147483648>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %24 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %25 = "llvm.getelementptr"(%23, %24) <{elem_type = !llvm.struct<(i64, i32, ptr)>, rawConstantIndices = array<i32: -2147483648, 0>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %26 = "llvm.load"(%25) <{ordering = 0 : i64}> : (!llvm.ptr) -> i64
    %27 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %28 = "arith.constant"() <{value = 2 : i32}> : () -> i32
    %29 = "llvm.getelementptr"(%23, %27) <{elem_type = !llvm.struct<(i64, i32, ptr)>, rawConstantIndices = array<i32: -2147483648, 2>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %30 = "llvm.load"(%29) <{ordering = 0 : i64}> : (!llvm.ptr) -> memref<?xi8>
    %31 = "arith.addi"(%20, %19) : (index, index) -> index
    "memref.store"(%31, %3) : (index, memref<index>) -> ()
    %32 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %33 = "llvm.load"(%32) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %34 = "arith.addi"(%33, %1) : (i32, i32) -> i32
    %35 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    "llvm.store"(%34, %35) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %36 = "polygeist.memref2pointer"(%29) : (memref<?xi8>) -> !llvm.ptr
    %37 = "llvm.load"(%36) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %38 = "arith.addi"(%37, %1) : (i32, i32) -> i32
    %39 = "polygeist.memref2pointer"(%29) : (memref<?xi8>) -> !llvm.ptr
    "llvm.store"(%38, %39) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %40 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
    %41 = "llvm.getelementptr"(%40) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %42 = "llvm.call"(%41, %34, %38) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "func.return"() : () -> ()
  }) : () -> ()
  "func.func"() <{function_type = () -> i64, sym_name = "artsGetCurrentEpochGuid", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64, i64, i32) -> (), sym_name = "artsDbAddDependence", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i64, sym_name = "artsGetCurrentGuid", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64, sym_name = "artsEdtCreateWithEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>) -> (), sym_name = "__arts_edt_2", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>):
    %0 = "func.call"() <{callee = @artsGetCurrentGuid}> : () -> i64
    %1 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %2 = "arith.constant"() <{value = 0 : index}> : () -> index
    %3 = "memref.load"(%arg1, %2) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %4 = "arith.trunci"(%3) : (i64) -> i32
    %5 = "arith.constant"() <{value = 1 : index}> : () -> index
    %6 = "memref.load"(%arg1, %5) <{nontemporal = false}> : (memref<?xi64>, index) -> i64
    %7 = "arith.trunci"(%6) : (i64) -> i32
    %8 = "arith.constant"() <{value = 0 : index}> : () -> index
    %9 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    "memref.store"(%8, %9) : (index, memref<index>) -> ()
    %10 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %11 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %12 = "arith.constant"() <{value = 1 : index}> : () -> index
    %13 = "memref.load"(%9) : (memref<index>) -> index
    %14 = "arith.muli"(%13, %10) : (index, index) -> index
    %15 = "arith.index_cast"(%14) : (index) -> i32
    %16 = "llvm.getelementptr"(%11, %15) <{elem_type = i8, rawConstantIndices = array<i32: -2147483648>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %17 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %18 = "llvm.getelementptr"(%16, %17) <{elem_type = !llvm.struct<(i64, i32, ptr)>, rawConstantIndices = array<i32: -2147483648, 0>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %19 = "llvm.load"(%18) <{ordering = 0 : i64}> : (!llvm.ptr) -> i64
    %20 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %21 = "arith.constant"() <{value = 2 : i32}> : () -> i32
    %22 = "llvm.getelementptr"(%16, %20) <{elem_type = !llvm.struct<(i64, i32, ptr)>, rawConstantIndices = array<i32: -2147483648, 2>}> : (!llvm.ptr, i32) -> !llvm.ptr
    %23 = "llvm.load"(%22) <{ordering = 0 : i64}> : (!llvm.ptr) -> memref<?xi8>
    %24 = "arith.addi"(%13, %12) : (index, index) -> index
    "memref.store"(%24, %9) : (index, memref<index>) -> ()
    %25 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %26 = "arith.constant"() <{value = 1 : i64}> : () -> i64
    %27 = "arith.constant"() <{value = 8 : i32}> : () -> i32
    %28 = "func.call"(%27, %25) <{callee = @artsReserveGuidRoute}> : (i32, i32) -> i64
    %29 = "func.call"(%28, %26) <{callee = @artsDbCreateWithGuid}> : (i64, i64) -> memref<?xi8>
    %30 = "polygeist.memref2pointer"(%29) : (memref<?xi8>) -> !llvm.ptr
    %31 = "llvm.load"(%30) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    "llvm.store"(%4, %31) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %32 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
    %33 = "llvm.getelementptr"(%32) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %34 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %35 = "llvm.load"(%34) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %36 = "llvm.call"(%33, %35, %7) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    %37 = "polygeist.memref2pointer"(%29) : (memref<?xi8>) -> !llvm.ptr
    %38 = "llvm.load"(%37) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    "llvm.store"(%7, %38) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %39 = "arts.db_dep"(%9) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<?xi8>) -> memref<memref<?xi8>>
    %40 = "arts.db_dep"(%29) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<?xi8>) -> memref<memref<?xi8>>
    %41 = "func.call"() <{callee = @artsGetCurrentEpochGuid}> : () -> i64
    %42 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %43 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %44 = "arith.constant"() <{value = 0 : index}> : () -> index
    "memref.store"(%44, %43) : (index, memref<index>) -> ()
    %45 = "memref.load"(%43) : (memref<index>) -> index
    %46 = "arith.constant"() <{value = 1 : index}> : () -> index
    %47 = "memref.load"(%43) : (memref<index>) -> index
    %48 = "arith.addi"(%47, %46) : (index, index) -> index
    "memref.store"(%48, %43) : (index, memref<index>) -> ()
    %49 = "memref.load"(%43) : (memref<index>) -> index
    %50 = "arith.constant"() <{value = 1 : index}> : () -> index
    %51 = "memref.load"(%43) : (memref<index>) -> index
    %52 = "arith.addi"(%51, %50) : (index, index) -> index
    "memref.store"(%52, %43) : (index, memref<index>) -> ()
    %53 = "memref.load"(%43) : (memref<index>) -> index
    %54 = "arith.index_cast"(%53) : (index) -> i32
    %55 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %56 = "arith.constant"() <{value = 0 : index}> : () -> index
    "memref.store"(%56, %55) : (index, memref<index>) -> ()
    %57 = "memref.load"(%55) : (memref<index>) -> index
    %58 = "arith.index_cast"(%57) : (index) -> i32
    %59 = "memref.alloca"(%57) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xi64>
    %60 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %61 = "polygeist.pointer2memref"(%60) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %62 = "func.call"(%61, %42, %58, %59, %54, %41) <{callee = @artsEdtCreateWithEpoch}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %63 = "arith.index_cast"(%45) : (index) -> i32
    "func.call"(%8, %62, %63) <{callee = @artsDbAddDependence}> : (i64, i64, i32) -> ()
    %64 = "arith.index_cast"(%49) : (index) -> i32
    "func.call"(%28, %62, %64) <{callee = @artsDbAddDependence}> : (i64, i64, i32) -> ()
    "func.call"(%8) <{callee = @artsDbIncrementLatch}> : (i64) -> ()
    "func.call"(%28) <{callee = @artsDbIncrementLatch}> : (i64) -> ()
    "func.return"() : () -> ()
  }) : () -> ()
  "func.func"() <{function_type = (i64) -> i1, sym_name = "artsWaitOnHandle", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64, i32) -> i64, sym_name = "artsInitializeAndStartEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64, sym_name = "artsEdtCreate", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>) -> (), sym_name = "__arts_edt_1", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>):
    "func.return"() : () -> ()
  }) : () -> ()
  "func.func"() <{function_type = (i64, i64) -> memref<?xi8>, sym_name = "artsDbCreateWithGuid", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, i32) -> i64, sym_name = "artsReserveGuidRoute", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "artsGetCurrentNode", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<37 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str4", unnamed_addr = 0 : i64, value = "EDT 3: The final number is %d - %d.\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<28 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str3", unnamed_addr = 0 : i64, value = "EDT 2: The number is %d/%d\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<28 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str2", unnamed_addr = 0 : i64, value = "EDT 1: The number is %d/%d\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<36 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str1", unnamed_addr = 0 : i64, value = "EDT 0: The initial number is %d/%d\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<23 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str0", unnamed_addr = 0 : i64, value = "Main EDT: Starting...\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.func"() <{CConv = #llvm.cconv<ccc>, function_type = !llvm.func<i32 (ptr, ...)>, linkage = #llvm.linkage<external>, sym_name = "printf", unnamed_addr = 0 : i64, visibility_ = 0 : i64}> ({
  }) : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "mainBody"}> ({
    %0 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %1 = "arith.constant"() <{value = 10 : i32}> : () -> i32
    %2 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %3 = "arith.constant"() <{value = 100 : i32}> : () -> i32
    %4 = "llvm.mlir.undef"() : () -> i32
    %5 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %6 = "arith.constant"() <{value = 1 : i64}> : () -> i64
    %7 = "arith.constant"() <{value = 8 : i32}> : () -> i32
    %8 = "func.call"(%7, %5) <{callee = @artsReserveGuidRoute}> : (i32, i32) -> i64
    %9 = "func.call"(%8, %6) <{callee = @artsDbCreateWithGuid}> : (i64, i64) -> memref<?xi8>
    %10 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %11 = "llvm.load"(%10) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    "llvm.store"(%4, %11) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %12 = "llvm.mlir.zero"() : () -> !llvm.ptr
    %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr) -> memref<?xi64>
    %14 = "func.call"(%13) <{callee = @time}> : (memref<?xi64>) -> i64
    %15 = "arith.trunci"(%14) : (i64) -> i32
    "func.call"(%15) <{callee = @srand}> : (i32) -> ()
    %16 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr
    %17 = "llvm.getelementptr"(%16) <{elem_type = !llvm.array<23 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %18 = "llvm.call"(%17) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr) -> i32
    %19 = "func.call"() <{callee = @rand}> : () -> i32
    %20 = "arith.remsi"(%19, %3) : (i32, i32) -> i32
    %21 = "arith.addi"(%20, %2) : (i32, i32) -> i32
    %22 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %23 = "llvm.load"(%22) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    "llvm.store"(%21, %23) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %24 = "func.call"() <{callee = @rand}> : () -> i32
    %25 = "arith.remsi"(%24, %1) : (i32, i32) -> i32
    %26 = "arith.addi"(%25, %2) : (i32, i32) -> i32
    %27 = "llvm.mlir.addressof"() <{global_name = @str1}> : () -> !llvm.ptr
    %28 = "llvm.getelementptr"(%27) <{elem_type = !llvm.array<36 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %29 = "llvm.call"(%28, %21, %26) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    %30 = "arts.db_dep"(%9) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<?xi8>) -> memref<memref<?xi8>>
    %31 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %32 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %33 = "arith.constant"() <{value = 0 : index}> : () -> index
    "memref.store"(%33, %32) : (index, memref<index>) -> ()
    %34 = "memref.load"(%32) : (memref<index>) -> index
    %35 = "arith.index_cast"(%34) : (index) -> i32
    %36 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %37 = "arith.constant"() <{value = 0 : index}> : () -> index
    "memref.store"(%37, %36) : (index, memref<index>) -> ()
    %38 = "memref.load"(%36) : (memref<index>) -> index
    %39 = "arith.index_cast"(%38) : (index) -> i32
    %40 = "memref.alloca"(%38) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xi64>
    %41 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %42 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %43 = "polygeist.pointer2memref"(%42) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %44 = "func.call"(%43, %31, %39, %40, %41) <{callee = @artsEdtCreate}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %45 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %46 = "func.call"(%44, %45) <{callee = @artsInitializeAndStartEpoch}> : (i64, i32) -> i64
    %47 = "func.call"(%46) <{callee = @artsWaitOnHandle}> : (i64) -> i1
    %48 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %49 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %50 = "arith.constant"() <{value = 0 : index}> : () -> index
    "memref.store"(%50, %49) : (index, memref<index>) -> ()
    %51 = "memref.load"(%49) : (memref<index>) -> index
    %52 = "arith.constant"() <{value = 1 : index}> : () -> index
    %53 = "memref.load"(%49) : (memref<index>) -> index
    %54 = "arith.addi"(%53, %52) : (index, index) -> index
    "memref.store"(%54, %49) : (index, memref<index>) -> ()
    %55 = "memref.load"(%49) : (memref<index>) -> index
    %56 = "arith.index_cast"(%55) : (index) -> i32
    %57 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %58 = "arith.constant"() <{value = 2 : index}> : () -> index
    "memref.store"(%58, %57) : (index, memref<index>) -> ()
    %59 = "memref.load"(%57) : (memref<index>) -> index
    %60 = "arith.index_cast"(%59) : (index) -> i32
    %61 = "memref.alloca"(%59) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xi64>
    %62 = "arith.constant"() <{value = 0 : index}> : () -> index
    %63 = "arith.extsi"(%4) : (i32) -> i64
    "memref.store"(%63, %61, %62) <{nontemporal = false}> : (i64, memref<?xi64>, index) -> ()
    %64 = "arith.constant"() <{value = 1 : index}> : () -> index
    %65 = "arith.extsi"(%26) : (i32) -> i64
    "memref.store"(%65, %61, %64) <{nontemporal = false}> : (i64, memref<?xi64>, index) -> ()
    %66 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %67 = "polygeist.pointer2memref"(%66) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %68 = "func.call"(%67, %48, %60, %61, %56, %46) <{callee = @artsEdtCreateWithEpoch}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %69 = "arith.index_cast"(%51) : (index) -> i32
    "func.call"(%8, %68, %69) <{callee = @artsDbAddDependence}> : (i64, i64, i32) -> ()
    %70 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %71 = "llvm.load"(%70) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    %72 = "llvm.load"(%71) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %73 = "arith.addi"(%72, %2) : (i32, i32) -> i32
    %74 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %75 = "llvm.load"(%74) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    "llvm.store"(%73, %75) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %76 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
    %77 = "llvm.getelementptr"(%76) <{elem_type = !llvm.array<37 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %78 = "llvm.call"(%77, %73, %26) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "func.return"(%0) : (i32) -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32) -> (), sym_name = "srand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi64>) -> i64, sym_name = "time", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "rand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
-----------------------------------------
-----------------------------------------
simple.mlir:45:17: error: 'polygeist.memref2pointer' op using value defined outside the region
          %26 = affine.load %alloca[] : memref<i32>
                ^
simple.mlir:45:17: note: see current operation: %32 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<?xi8>) -> !llvm.ptr
<unknown>:0: note: required by region isolation constraints
simple.mlir:41:15: error: 'polygeist.memref2pointer' op using value defined outside the region
        %24 = affine.load %alloca[] : memref<i32>
              ^
simple.mlir:41:15: note: see current operation: %34 = "polygeist.memref2pointer"(<<UNKNOWN SSA VALUE>>) : (memref<?xi8>) -> !llvm.ptr
<unknown>:0: note: required by region isolation constraints
