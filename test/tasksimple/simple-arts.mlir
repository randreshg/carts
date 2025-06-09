-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %1 = arts.db_create "inout"(%alloca : memref<i32>) [] -> : memref<i32>
    memref.store %0, %1[] : memref<i32>
    %2 = llvm.mlir.zero : !llvm.ptr
    %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi64>
    %4 = call @time(%3) : (memref<?xi64>) -> i64
    %5 = arith.trunci %4 : i64 to i32
    call @srand(%5) : (i32) -> ()
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %8 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %9 = call @rand() : () -> i32
    %10 = arith.remsi %9, %c100_i32 : i32
    %11 = arith.addi %10, %c1_i32 : i32
    memref.store %11, %1[] : memref<i32>
    %12 = call @rand() : () -> i32
    %13 = arith.remsi %12, %c10_i32 : i32
    %14 = arith.addi %13, %c1_i32 : i32
    %15 = llvm.mlir.addressof @str1 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %17 = llvm.call @printf(%16, %11, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %subview = memref.subview %1[] [] [] : memref<i32> to memref<i32, strided<[]>>
    arts.epoch {
      arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {task} {
        %alloca_0 = memref.alloca() : memref<i32>
        %23 = arts.db_create "inout"(%alloca_0 : memref<i32>) [] -> : memref<i32>
        memref.store %0, %23[] : memref<i32>
        %24 = llvm.mlir.addressof @str2 : !llvm.ptr
        %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
        %26 = memref.load %subview[] : memref<i32, strided<[]>>
        %27 = llvm.call @printf(%25, %26, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        memref.store %14, %23[] : memref<i32>
        %subview_1 = memref.subview %23[] [] [] : memref<i32> to memref<i32, strided<[]>>
        arts.edt dependencies(%subview_1, %subview) : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) attributes {task} {
          %28 = memref.load %subview[] : memref<i32, strided<[]>>
          %29 = arith.addi %28, %c1_i32 : i32
          memref.store %29, %subview[] : memref<i32, strided<[]>>
          %30 = memref.load %subview_1[] : memref<i32, strided<[]>>
          %31 = arith.addi %30, %c1_i32 : i32
          memref.store %31, %subview_1[] : memref<i32, strided<[]>>
          %32 = llvm.mlir.addressof @str3 : !llvm.ptr
          %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
          %34 = llvm.call @printf(%33, %29, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
          arts.yield
        }
        arts.yield
      }
    }
    %18 = memref.load %1[] : memref<i32>
    %19 = arith.addi %18, %c1_i32 : i32
    memref.store %19, %1[] : memref<i32>
    %20 = llvm.mlir.addressof @str4 : !llvm.ptr
    %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %22 = llvm.call @printf(%21, %19, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
Module after preprocessing DataBlocks:
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %1 = arts.db_create "inout"(%alloca : memref<i32>) [] -> : memref<i32>
    memref.store %0, %1[] : memref<i32>
    %2 = llvm.mlir.zero : !llvm.ptr
    %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi64>
    %4 = call @time(%3) : (memref<?xi64>) -> i64
    %5 = arith.trunci %4 : i64 to i32
    call @srand(%5) : (i32) -> ()
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %8 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %9 = call @rand() : () -> i32
    %10 = arith.remsi %9, %c100_i32 : i32
    %11 = arith.addi %10, %c1_i32 : i32
    memref.store %11, %1[] : memref<i32>
    %12 = call @rand() : () -> i32
    %13 = arith.remsi %12, %c10_i32 : i32
    %14 = arith.addi %13, %c1_i32 : i32
    %15 = llvm.mlir.addressof @str1 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %17 = llvm.call @printf(%16, %11, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %subview = memref.subview %1[] [] [] : memref<i32> to memref<i32, strided<[]>>
    arts.epoch {
      arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {task} {
        %alloca_0 = memref.alloca() : memref<i32>
        %23 = arts.db_create "inout"(%alloca_0 : memref<i32>) [] -> : memref<i32>
        memref.store %0, %23[] : memref<i32>
        %24 = llvm.mlir.addressof @str2 : !llvm.ptr
        %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
        %26 = memref.load %subview[] : memref<i32, strided<[]>>
        %27 = llvm.call @printf(%25, %26, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        memref.store %14, %23[] : memref<i32>
        %subview_1 = memref.subview %23[] [] [] : memref<i32> to memref<i32, strided<[]>>
        arts.edt dependencies(%subview_1, %subview) : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) attributes {task} {
          %28 = memref.load %subview[] : memref<i32, strided<[]>>
          %29 = arith.addi %28, %c1_i32 : i32
          memref.store %29, %subview[] : memref<i32, strided<[]>>
          %30 = memref.load %subview_1[] : memref<i32, strided<[]>>
          %31 = arith.addi %30, %c1_i32 : i32
          memref.store %31, %subview_1[] : memref<i32, strided<[]>>
          %32 = llvm.mlir.addressof @str3 : !llvm.ptr
          %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
          %34 = llvm.call @printf(%33, %29, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
          arts.yield
        }
        arts.yield
      }
    }
    %18 = memref.load %1[] : memref<i32>
    %19 = arith.addi %18, %c1_i32 : i32
    memref.store %19, %1[] : memref<i32>
    %20 = llvm.mlir.addressof @str4 : !llvm.ptr
    %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %22 = llvm.call @printf(%21, %19, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
-----------------------------------------
[convert-arts-to-llvm] Lowering arts.epoch: arts.epoch {
  arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {task} {
    %alloca_0 = memref.alloca() : memref<i32>
    %23 = arts.db_create "inout"(%alloca_0 : memref<i32>) [] -> : memref<i32>
    memref.store %0, %23[] : memref<i32>
    %24 = llvm.mlir.addressof @str2 : !llvm.ptr
    %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
    %26 = memref.load %subview[] : memref<i32, strided<[]>>
    %27 = llvm.call @printf(%25, %26, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    memref.store %14, %23[] : memref<i32>
    %subview_1 = memref.subview %23[] [] [] : memref<i32> to memref<i32, strided<[]>>
    arts.edt dependencies(%subview_1, %subview) : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) attributes {task} {
      %28 = memref.load %subview[] : memref<i32, strided<[]>>
      %29 = arith.addi %28, %c1_i32 : i32
      memref.store %29, %subview[] : memref<i32, strided<[]>>
      %30 = memref.load %subview_1[] : memref<i32, strided<[]>>
      %31 = arith.addi %30, %c1_i32 : i32
      memref.store %31, %subview_1[] : memref<i32, strided<[]>>
      %32 = llvm.mlir.addressof @str3 : !llvm.ptr
      %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      %34 = llvm.call @printf(%33, %29, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      arts.yield
    }
    arts.yield
  }
}
[convert-arts-to-llvm] Module after iterating Epochs and Events:
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
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
    %1 = arts.db_create "inout"(%alloca : memref<i32>) [] -> : memref<i32>
    memref.store %0, %1[] : memref<i32>
    %2 = llvm.mlir.zero : !llvm.ptr
    %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi64>
    %4 = call @time(%3) : (memref<?xi64>) -> i64
    %5 = arith.trunci %4 : i64 to i32
    call @srand(%5) : (i32) -> ()
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<23 x i8>
    %8 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %9 = call @rand() : () -> i32
    %10 = arith.remsi %9, %c100_i32 : i32
    %11 = arith.addi %10, %c1_i32 : i32
    memref.store %11, %1[] : memref<i32>
    %12 = call @rand() : () -> i32
    %13 = arith.remsi %12, %c10_i32 : i32
    %14 = arith.addi %13, %c1_i32 : i32
    %15 = llvm.mlir.addressof @str1 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<36 x i8>
    %17 = llvm.call @printf(%16, %11, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %subview = memref.subview %1[] [] [] : memref<i32> to memref<i32, strided<[]>>
    %18 = call @artsGetCurrentNode() : () -> i32
    %c0 = arith.constant 0 : index
    %alloca_0 = memref.alloca() : memref<index>
    memref.store %c0, %alloca_0[] : memref<index>
    %19 = memref.load %alloca_0[] : memref<index>
    %20 = arith.index_cast %19 : index to i32
    %alloca_1 = memref.alloca() : memref<index>
    %c0_2 = arith.constant 0 : index
    memref.store %c0_2, %alloca_1[] : memref<index>
    %21 = memref.load %alloca_1[] : memref<index>
    %22 = arith.index_cast %21 : index to i32
    %alloca_3 = memref.alloca(%21) : memref<?xi64>
    %c1_i32_4 = arith.constant 1 : i32
    %23 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %25 = call @artsEdtCreate(%24, %18, %22, %alloca_3, %c1_i32_4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_5 = arith.constant 0 : i32
    %26 = call @artsInitializeAndStartEpoch(%25, %c0_i32_5) : (i64, i32) -> i64
    arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {task} {
      %alloca_6 = memref.alloca() : memref<i32>
      %33 = arts.db_create "inout"(%alloca_6 : memref<i32>) [] -> : memref<i32>
      memref.store %0, %33[] : memref<i32>
      %34 = llvm.mlir.addressof @str2 : !llvm.ptr
      %35 = llvm.getelementptr %34[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      %36 = memref.load %subview[] : memref<i32, strided<[]>>
      %37 = llvm.call @printf(%35, %36, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      memref.store %14, %33[] : memref<i32>
      %subview_7 = memref.subview %33[] [] [] : memref<i32> to memref<i32, strided<[]>>
      arts.edt dependencies(%subview_7, %subview) : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) attributes {task} {
        %38 = memref.load %subview[] : memref<i32, strided<[]>>
        %39 = arith.addi %38, %c1_i32 : i32
        memref.store %39, %subview[] : memref<i32, strided<[]>>
        %40 = memref.load %subview_7[] : memref<i32, strided<[]>>
        %41 = arith.addi %40, %c1_i32 : i32
        memref.store %41, %subview_7[] : memref<i32, strided<[]>>
        %42 = llvm.mlir.addressof @str3 : !llvm.ptr
        %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
        %44 = llvm.call @printf(%43, %39, %41) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
        arts.yield
      }
      arts.yield
    }
    %27 = call @artsWaitOnHandle(%26) : (i64) -> i1
    %28 = memref.load %1[] : memref<i32>
    %29 = arith.addi %28, %c1_i32 : i32
    memref.store %29, %1[] : memref<i32>
    %30 = llvm.mlir.addressof @str4 : !llvm.ptr
    %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %32 = llvm.call @printf(%31, %29, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
}
-----------------------------------------
[convert-arts-to-llvm] Lowering arts.db_create
[convert-arts-to-llvm] Lowering arts.db_create
[convert-arts-to-llvm] Lowering arts.edt
carts-opt: /home/randres/projects/carts/external/Polygeist/llvm-project/llvm/include/llvm/Support/Casting.h:578: decltype(auto) llvm::cast(From *) [To = mlir::arts::DbControlOp, From = mlir::Operation]: Assertion `isa<To>(Val) && "cast<Ty>() argument of incompatible type!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize --create-dbs --db --cse --polygeist-mem2reg --edt-pointer-rematerialization --create-epochs --convert-arts-to-llvm --cse -debug-only=convert-arts-to-llvm,arts-codegen
 #0 0x00005624e36827a7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11d87a7)
 #1 0x00005624e368037e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11d637e)
 #2 0x00005624e3682e5a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f2f0931c520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f2f093709fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f2f0931c476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f2f093027f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f2f0930271b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f2f09313e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00005624e2ccf050 mlir::arts::EdtCodegen::processSubviewDependency(mlir::Value, mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x825050)
#10 0x00005624e2cca3a7 mlir::arts::EdtCodegen::process(mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x8203a7)
#11 0x00005624e2cc9e92 mlir::arts::EdtCodegen::EdtCodegen(mlir::arts::ArtsCodegen&, llvm::SmallVector<mlir::Value, 6u>*, mlir::Region*, mlir::Value*, mlir::Location*, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x81fe92)
#12 0x00005624e2cd7542 mlir::arts::ArtsCodegen::createEdt(llvm::SmallVector<mlir::Value, 6u>*, mlir::Region*, mlir::Value*, mlir::Location*, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x82d542)
#13 0x00005624e2cc40e6 mlir::WalkResult llvm::function_ref<mlir::WalkResult (mlir::Operation*)>::callback_fn<std::enable_if<!llvm::is_one_of<mlir::arts::EdtOp, mlir::Operation*, mlir::Region*, mlir::Block*>::value && std::is_same<mlir::WalkResult, mlir::WalkResult>::value, mlir::WalkResult>::type mlir::detail::walk<(mlir::WalkOrder)0, mlir::ForwardIterator, (anonymous namespace)::ConvertArtsToLLVMPass::iterateOps()::$_7, mlir::arts::EdtOp, mlir::WalkResult>(mlir::Operation*, (anonymous namespace)::ConvertArtsToLLVMPass::iterateOps()::$_7&&)::'lambda'(mlir::Operation*)>(long, mlir::Operation*) ConvertArtsToLLVM.cpp:0:0
#14 0x00005624e2664db8 mlir::WalkResult mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<mlir::WalkResult (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1badb8)
#15 0x00005624e2664d67 mlir::WalkResult mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<mlir::WalkResult (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1bad67)
#16 0x00005624e2664d67 mlir::WalkResult mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<mlir::WalkResult (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1bad67)
#17 0x00005624e2cbf91c (anonymous namespace)::ConvertArtsToLLVMPass::runOnOperation() ConvertArtsToLLVM.cpp:0:0
#18 0x00005624e34b6ea4 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x100cea4)
#19 0x00005624e34b74d1 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x100d4d1)
#20 0x00005624e34b9982 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x100f982)
#21 0x00005624e2d10d64 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#22 0x00005624e2d0ffd4 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#23 0x00005624e3617e88 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x116de88)
#24 0x00005624e2d0a5da mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x8605da)
#25 0x00005624e2d0aaa4 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x860aa4)
#26 0x00005624e25eb086 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x141086)
#27 0x00007f2f09303d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#28 0x00007f2f09303e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#29 0x00005624e25ea785 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x140785)
