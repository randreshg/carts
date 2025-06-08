
-----------------------------------------
CreateDbsPass STARTED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %alloca = memref.alloca() : memref<i32>
    memref.store %0, %alloca[] : memref<i32>
    %1 = llvm.mlir.zero : !llvm.ptr
    %2 = "polygeist.pointer2memref"(%1) : (!llvm.ptr) -> memref<?xi64>
    %3 = call @time(%2) : (memref<?xi64>) -> i64
    %4 = arith.trunci %3 : i64 to i32
    call @srand(%4) : (i32) -> ()
    %5 = llvm.mlir.addressof @str0 : !llvm.ptr
    %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %7 = llvm.call @printf(%6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %8 = call @rand() : () -> i32
    %9 = arith.remsi %8, %c100_i32 : i32
    %10 = arith.addi %9, %c1_i32 : i32
    memref.store %10, %alloca[] : memref<i32>
    %11 = call @rand() : () -> i32
    %12 = arith.remsi %11, %c10_i32 : i32
    %13 = arith.addi %12, %c1_i32 : i32
    %14 = llvm.mlir.addressof @str1 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %16 = llvm.call @printf(%15, %10, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    arts.edt attributes {sync, task} {
      %alloca_0 = memref.alloca() : memref<i32>
      memref.store %0, %alloca_0[] : memref<i32>
      %22 = llvm.mlir.addressof @str2 : !llvm.ptr
      %23 = llvm.getelementptr %22[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      %24 = memref.load %alloca[] : memref<i32>
      %25 = llvm.call @printf(%23, %24, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      memref.store %13, %alloca_0[] : memref<i32>
      arts.edt attributes {task} {
        %26 = memref.load %alloca[] : memref<i32>
        %27 = arith.addi %26, %c1_i32 : i32
        memref.store %27, %alloca[] : memref<i32>
        %28 = memref.load %alloca_0[] : memref<i32>
        %29 = arith.addi %28, %c1_i32 : i32
        memref.store %29, %alloca_0[] : memref<i32>
        %30 = llvm.mlir.addressof @str3 : !llvm.ptr
        %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
        %32 = llvm.call @printf(%31, %27, %29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        arts.yield
      }
      arts.yield
    }
    %17 = memref.load %alloca[] : memref<i32>
    %18 = arith.addi %17, %c1_i32 : i32
    memref.store %18, %alloca[] : memref<i32>
    %19 = llvm.mlir.addressof @str4 : !llvm.ptr
    %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %21 = llvm.call @printf(%20, %18, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[create-dbs] Candidate datablocks in function: main
------------------
[create-dbs] EDT region #0:
  - Found value: %alloca = memref.alloca() : memref<i32>
  - Found value: %c1_i32 = arith.constant 1 : i32
  - Found value: %alloca_0 = memref.alloca() : memref<i32>
  - Candidate Db #0:
    Memref: %alloca = memref.alloca() : memref<i32>
    IsString: false
    Access Type: inout
    Pinned Indices:  none
    Uses:
      - memref.store %27, %alloca[] : memref<i32>
      - %26 = memref.load %alloca[] : memref<i32>
  - Candidate Db #1:
    Memref: %alloca_0 = memref.alloca() : memref<i32>
    IsString: false
    Access Type: inout
    Pinned Indices:  none
    Uses:
      - memref.store %29, %alloca_0[] : memref<i32>
      - %28 = memref.load %alloca_0[] : memref<i32>
------------------
[create-dbs] EDT region #1:
  - Found value: %0 = llvm.mlir.undef : i32
  - Found value: %alloca = memref.alloca() : memref<i32>
  - Found value: %13 = arith.addi %12, %c1_i32 : i32
  - Found value: %c1_i32 = arith.constant 1 : i32
  - Candidate Db #0:
    Memref: %alloca = memref.alloca() : memref<i32>
    IsString: false
    Access Type: inout
    Pinned Indices:  none
    Uses:
      - memref.store %27, %alloca[] : memref<i32>
      - %26 = memref.load %alloca[] : memref<i32>
      - %24 = memref.load %alloca[] : memref<i32>
[create-dbs] Candidate datablocks in function: srand
[create-dbs] Candidate datablocks in function: time
[create-dbs] Candidate datablocks in function: rand
-----------------------------------------
[create-dbs] Creating 2 datablocks for EDT #0
[create-dbs] Created EDT #0
-----------------------------------------
[create-dbs] Creating 1 datablocks for EDT #1
[create-dbs] Created EDT #1
Removing old datablocks

-----------------------------------------
CreateDbsPass FINISHED
-----------------------------------------
"builtin.module"() ({
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
  "func.func"() <{function_type = () -> i32, sym_name = "main"}> ({
    %0 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %1 = "arith.constant"() <{value = 10 : i32}> : () -> i32
    %2 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %3 = "arith.constant"() <{value = 100 : i32}> : () -> i32
    %4 = "llvm.mlir.undef"() : () -> i32
    %5 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<i32>
    %6 = "memref.subview"(%35#0) <{operandSegmentSizes = array<i32: 1, 0, 0, 0>, static_offsets = array<i64>, static_sizes = array<i64>, static_strides = array<i64>}> : (memref<i32>) -> memref<i32, strided<[]>>
    "memref.store"(%4, %5) <{nontemporal = false}> : (i32, memref<i32>) -> ()
    %7 = "llvm.mlir.zero"() : () -> !llvm.ptr
    %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr) -> memref<?xi64>
    %9 = "func.call"(%8) <{callee = @time}> : (memref<?xi64>) -> i64
    %10 = "arith.trunci"(%9) : (i64) -> i32
    "func.call"(%10) <{callee = @srand}> : (i32) -> ()
    %11 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr
    %12 = "llvm.getelementptr"(%11) <{elem_type = !llvm.array<23 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %13 = "llvm.call"(%12) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr) -> i32
    %14 = "func.call"() <{callee = @rand}> : () -> i32
    %15 = "arith.remsi"(%14, %3) : (i32, i32) -> i32
    %16 = "arith.addi"(%15, %2) : (i32, i32) -> i32
    "memref.store"(%16, %5) <{nontemporal = false}> : (i32, memref<i32>) -> ()
    %17 = "func.call"() <{callee = @rand}> : () -> i32
    %18 = "arith.remsi"(%17, %1) : (i32, i32) -> i32
    %19 = "arith.addi"(%18, %2) : (i32, i32) -> i32
    %20 = "llvm.mlir.addressof"() <{global_name = @str1}> : () -> !llvm.ptr
    %21 = "llvm.getelementptr"(%20) <{elem_type = !llvm.array<36 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %22 = "llvm.call"(%21, %16, %19) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "arts.edt"(%6) ({
      %28 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<i32>
      %29:2 = "arts.db_create"(%28) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0>}> : (memref<i32>) -> (memref<i32>, i64)
      %30 = "memref.subview"(%29#0) <{operandSegmentSizes = array<i32: 1, 0, 0, 0>, static_offsets = array<i64>, static_sizes = array<i64>, static_strides = array<i64>}> : (memref<i32>) -> memref<i32, strided<[]>>
      "memref.store"(%4, %28) <{nontemporal = false}> : (i32, memref<i32>) -> ()
      %31 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
      %32 = "llvm.getelementptr"(%31) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
      %33 = "memref.load"(%6) <{nontemporal = false}> : (memref<i32, strided<[]>>) -> i32
      %34 = "llvm.call"(%32, %33, %19) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
      "memref.store"(%19, %28) <{nontemporal = false}> : (i32, memref<i32>) -> ()
      %35:2 = "arts.db_create"(%5) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0>}> : (memref<i32>) -> (memref<i32>, i64)
      %36 = "memref.subview"(%35#0) <{operandSegmentSizes = array<i32: 1, 0, 0, 0>, static_offsets = array<i64>, static_sizes = array<i64>, static_strides = array<i64>}> : (memref<i32>) -> memref<i32, strided<[]>>
      "arts.edt"(%36, %30) ({
        %37 = "memref.load"(%36) <{nontemporal = false}> : (memref<i32, strided<[]>>) -> i32
        %38 = "arith.addi"(%37, %2) : (i32, i32) -> i32
        "memref.store"(%38, %36) <{nontemporal = false}> : (i32, memref<i32, strided<[]>>) -> ()
        %39 = "memref.load"(%30) <{nontemporal = false}> : (memref<i32, strided<[]>>) -> i32
        %40 = "arith.addi"(%39, %2) : (i32, i32) -> i32
        "memref.store"(%40, %30) <{nontemporal = false}> : (i32, memref<i32, strided<[]>>) -> ()
        %41 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
        %42 = "llvm.getelementptr"(%41) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
        %43 = "llvm.call"(%42, %38, %40) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
        "arts.yield"() : () -> ()
      }) {task} : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) -> ()
      "arts.yield"() : () -> ()
    }) {sync} : (memref<i32, strided<[]>>) -> ()
    %23 = "memref.load"(%5) <{nontemporal = false}> : (memref<i32>) -> i32
    %24 = "arith.addi"(%23, %2) : (i32, i32) -> i32
    "memref.store"(%24, %5) <{nontemporal = false}> : (i32, memref<i32>) -> ()
    %25 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
    %26 = "llvm.getelementptr"(%25) <{elem_type = !llvm.array<37 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %27 = "llvm.call"(%26, %24, %19) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "func.return"(%0) : (i32) -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32) -> (), sym_name = "srand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi64>) -> i64, sym_name = "time", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "rand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
<unknown>:0: error: operand #0 does not dominate this use
<unknown>:0: note: see current operation: %6 = "memref.subview"(%35#0) <{operandSegmentSizes = array<i32: 1, 0, 0, 0>, static_offsets = array<i64>, static_sizes = array<i64>, static_strides = array<i64>}> : (memref<i32>) -> memref<i32, strided<[]>>
<unknown>:0: note: operand defined here (op in a child region)
