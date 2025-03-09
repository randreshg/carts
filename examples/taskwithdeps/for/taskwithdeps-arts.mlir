
-----------------------------------------
ConvertOpenMPToArtsPass STARTED
-----------------------------------------
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.parallel to arts.parallel
-----------------------------------------
ConvertOpenMPToArtsPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str6("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arith.cmpi slt, %arg0, %c2_i32 : i32
    %2 = arith.cmpi sge, %arg0, %c2_i32 : i32
    %3 = arith.select %1, %c1_i32, %0 : i32
    scf.if %1 {
      %5 = llvm.mlir.addressof @stderr : !llvm.ptr
      %6 = llvm.load %5 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %7 = "polygeist.memref2pointer"(%6) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %8 = llvm.mlir.addressof @str0 : !llvm.ptr
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
      %10 = memref.load %arg1[%c0] : memref<?xmemref<?xi8>>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.call @fprintf(%7, %9, %11) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    }
    %4 = arith.select %2, %c0_i32, %3 : i32
    scf.if %2 {
      %5 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
      %6 = func.call @atoi(%5) : (memref<?xi8>) -> i32
      %7 = arith.index_cast %6 : i32 to index
      %alloca = memref.alloca(%7) : memref<?xi32>
      %alloca_0 = memref.alloca(%7) : memref<?xi32>
      %8 = llvm.mlir.zero : !llvm.ptr
      %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi64>
      %10 = func.call @time(%9) : (memref<?xi64>) -> i64
      %11 = arith.trunci %10 : i64 to i32
      func.call @srand(%11) : (i32) -> ()
      %12 = llvm.mlir.addressof @str1 : !llvm.ptr
      %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
      %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %15 = llvm.mlir.addressof @str2 : !llvm.ptr
      %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
      %17 = llvm.call @printf(%16, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
      arts.edt attributes {parallel} {
        arts.barrier
        arts.edt attributes {single} {
          %25 = arith.index_cast %6 : i32 to index
          scf.for %arg2 = %c0 to %25 step %c1 {
            %26 = arith.index_cast %arg2 : index to i32
            %27 = memref.load %alloca[%arg2] : memref<?xi32>
            %28 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %29 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%28] -> memref<i32>
            arts.edt dependencies(%29) : (memref<i32>) attributes {task} {
              %30 = func.call @rand() : () -> i32
              %31 = arith.remsi %30, %c100_i32 : i32
              memref.store %31, %alloca[%arg2] : memref<?xi32>
              %32 = llvm.mlir.addressof @str3 : !llvm.ptr
              %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
              %34 = llvm.call @printf(%33, %26, %26, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              arts.yield
            }
          }
          arts.yield
        }
        arts.barrier
        arts.yield
      }
      %18 = llvm.mlir.addressof @str4 : !llvm.ptr
      %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %20 = llvm.call @printf(%19) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %21 = arith.index_cast %6 : i32 to index
      scf.for %arg2 = %c0 to %21 step %c1 {
        %25 = arith.index_cast %arg2 : index to i32
        %26 = llvm.mlir.addressof @str5 : !llvm.ptr
        %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %28 = memref.load %alloca[%arg2] : memref<?xi32>
        %29 = memref.load %alloca_0[%arg2] : memref<?xi32>
        %30 = llvm.call @printf(%27, %25, %28, %25, %29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %22 = llvm.mlir.addressof @str6 : !llvm.ptr
      %23 = llvm.getelementptr %22[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %24 = llvm.call @printf(%23) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}

-----------------------------------------
EdtPass STARTED
-----------------------------------------
[edt] Converting parallel EDT into single EDT
-----------------------------------------
EdtPass FINISHED
-----------------------------------------

-----------------------------------------
CreateDatablocksPass STARTED
-----------------------------------------
[create-datablocks] Candidate datablocks in function: main
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca(%7) : memref<?xi32>
    Access Type: inout
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %31, %alloca[%arg2] : memref<?xi32>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca(%7) : memref<?xi32>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - memref.store %31, %alloca[%arg2] : memref<?xi32>
    - %27 = memref.load %alloca[%arg2] : memref<?xi32>
[create-datablocks] Candidate datablocks in function: atoi
[create-datablocks] Candidate datablocks in function: srand
[create-datablocks] Candidate datablocks in function: time
[create-datablocks] Candidate datablocks in function: rand
-----------------------------------------
Rewiring uses of:
  %31 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%30] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %19 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[], sizes[%dim], type[i32], typeSize[%18] -> memref<?xi32>
-----------------------------------------
CreateDatablocksPass FINISHED
-----------------------------------------

-----------------------------------------
DatablockPass STARTED
-----------------------------------------
-----------------------------------------
DatablockPass FINISHED
-----------------------------------------
-----------------------------------------
CreateEventsPass STARTED
-----------------------------------------
-----------------------------------------
CreateEventsPass FINISHED
-----------------------------------------
-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str6("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arith.cmpi slt, %arg0, %c2_i32 : i32
    %2 = arith.cmpi sge, %arg0, %c2_i32 : i32
    %3 = arith.select %1, %c1_i32, %0 : i32
    scf.if %1 {
      %5 = llvm.mlir.addressof @stderr : !llvm.ptr
      %6 = llvm.load %5 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %7 = "polygeist.memref2pointer"(%6) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %8 = llvm.mlir.addressof @str0 : !llvm.ptr
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
      %10 = memref.load %arg1[%c0] : memref<?xmemref<?xi8>>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.call @fprintf(%7, %9, %11) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    }
    %4 = arith.select %2, %c0_i32, %3 : i32
    scf.if %2 {
      %5 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
      %6 = func.call @atoi(%5) : (memref<?xi8>) -> i32
      %7 = arith.index_cast %6 : i32 to index
      %alloca = memref.alloca(%7) : memref<?xi32>
      %alloca_0 = memref.alloca(%7) : memref<?xi32>
      %8 = llvm.mlir.zero : !llvm.ptr
      %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi64>
      %10 = func.call @time(%9) : (memref<?xi64>) -> i64
      %11 = arith.trunci %10 : i64 to i32
      func.call @srand(%11) : (i32) -> ()
      %12 = llvm.mlir.addressof @str1 : !llvm.ptr
      %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
      %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %15 = llvm.mlir.addressof @str2 : !llvm.ptr
      %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
      %17 = llvm.call @printf(%16, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
      %18 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xi32>
      arts.edt dependencies(%18) : (memref<?xi32>) attributes {sync} {
        scf.for %arg2 = %c0 to %7 step %c1 {
          %25 = arith.index_cast %arg2 : index to i32
          %26 = arts.datablock "inout" ptr[%18 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          arts.edt dependencies(%26) : (memref<i32>) attributes {task} {
            %27 = func.call @rand() : () -> i32
            %28 = arith.remsi %27, %c100_i32 : i32
            memref.store %28, %26[] : memref<i32>
            %29 = llvm.mlir.addressof @str3 : !llvm.ptr
            %30 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
            %31 = llvm.call @printf(%30, %25, %25, %28) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
            arts.yield
          }
        }
        arts.yield
      }
      %19 = llvm.mlir.addressof @str4 : !llvm.ptr
      %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %21 = llvm.call @printf(%20) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %25 = arith.index_cast %arg2 : index to i32
        %26 = llvm.mlir.addressof @str5 : !llvm.ptr
        %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %28 = memref.load %18[%arg2] : memref<?xi32>
        %29 = memref.load %alloca_0[%arg2] : memref<?xi32>
        %30 = llvm.call @printf(%27, %25, %28, %25, %29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %22 = llvm.mlir.addressof @str6 : !llvm.ptr
      %23 = llvm.getelementptr %22[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %24 = llvm.call @printf(%23) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-llvm] Iterate over all the functions
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] Created array of datablocks: %alloca_2 = memref.alloca(%7) : memref<?xmemref<?xi8>>
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] Creating datablock that has a pointer to another datablock
    %28 = arts.datablock "inout" ptr[%20 : memref<?xmemref<?xi8>>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<?xi8>
[convert-arts-to-llvm] Lowering arts.edt sync
-----------------------------------------
[convert-arts-to-llvm] Sync done EDT created
[convert-arts-to-llvm] Parallel epoch created
[convert-arts-to-llvm] Parallel EDT created
[convert-arts-to-llvm] Parallel op lowered

[convert-arts-to-llvm] Lowering arts.edt
-----------------------------------------
ConvertArtsToLLVMPass FINISHED 
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str6("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c4 = arith.constant 4 : index
    %c100_i32 = arith.constant 100 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %c1_1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %c0_2 = arith.constant 0 : index
    %c1_3 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0_2, %alloca[] : memref<index>
    %4 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %5 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_4 = memref.alloca(%3) : memref<?xi64>
    %alloca_5 = memref.alloca(%3) : memref<?xmemref<?xi8>>
    %c0_6 = arith.constant 0 : index
    scf.for %arg4 = %c0_6 to %3 step %c1_3 {
      %6 = memref.load %alloca[] : memref<index>
      %7 = arith.muli %6, %4 : index
      %8 = arith.index_cast %7 : index to i32
      %9 = llvm.getelementptr %5[%8] : (!llvm.ptr, i32) -> !llvm.ptr, i8
      %c0_i32 = arith.constant 0 : i32
      %10 = llvm.getelementptr %9[%c0_i32, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %11 = llvm.load %10 : !llvm.ptr -> i64
      %c0_i32_7 = arith.constant 0 : i32
      %c2_i32 = arith.constant 2 : i32
      %12 = llvm.getelementptr %9[%c0_i32_7, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %13 = llvm.load %12 : !llvm.ptr -> memref<?xi8>
      memref.store %11, %alloca_4[%arg4] : memref<?xi64>
      memref.store %13, %alloca_5[%arg4] : memref<?xmemref<?xi8>>
      %14 = arith.addi %6, %c1_3 : index
      memref.store %14, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %3 step %c1 {
      %6 = arith.index_cast %arg4 : index to i32
      %7 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %8 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32 = arith.constant 0 : i32
      %alloca_7 = memref.alloca() : memref<i32>
      memref.store %c0_i32, %alloca_7[] : memref<i32>
      %alloca_8 = memref.alloca() : memref<i32>
      memref.store %c0_i32, %alloca_8[] : memref<i32>
      %9 = memref.load %alloca_8[] : memref<i32>
      %c1_i32 = arith.constant 1 : i32
      %10 = memref.load %alloca_8[] : memref<i32>
      %11 = arith.addi %10, %c1_i32 : i32
      memref.store %11, %alloca_8[] : memref<i32>
      %12 = memref.load %alloca_8[] : memref<i32>
      %alloca_9 = memref.alloca() : memref<i32>
      %c1_i32_10 = arith.constant 1 : i32
      memref.store %c1_i32_10, %alloca_9[] : memref<i32>
      %13 = memref.load %alloca_9[] : memref<i32>
      %14 = arith.index_cast %13 : i32 to index
      %alloca_11 = memref.alloca(%14) : memref<?xi64>
      %15 = arith.extsi %6 : i32 to i64
      %c0_12 = arith.constant 0 : index
      affine.store %15, %alloca_11[%c0_12] : memref<?xi64>
      %16 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %17 = "polygeist.pointer2memref"(%16) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %18 = func.call @artsEdtCreateWithEpoch(%17, %8, %13, %alloca_11, %12, %7) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %19 = memref.load %alloca_4[%arg4] : memref<?xi64>
      func.call @artsSignalEdt(%18, %9, %19) : (i64, i32, i64) -> ()
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c100_i32 = arith.constant 100 : i32
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %c0_0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0_0, %alloca[] : memref<index>
    %2 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %3 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %4 = memref.load %alloca[] : memref<index>
    %5 = arith.muli %4, %2 : index
    %6 = arith.index_cast %5 : index to i32
    %7 = llvm.getelementptr %3[%6] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32 = arith.constant 0 : i32
    %8 = llvm.getelementptr %7[%c0_i32, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %9 = llvm.load %8 : !llvm.ptr -> i64
    %c0_i32_1 = arith.constant 0 : i32
    %c2_i32 = arith.constant 2 : i32
    %10 = llvm.getelementptr %7[%c0_i32_1, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %11 = llvm.load %10 : !llvm.ptr -> memref<?xi8>
    %12 = arith.addi %4, %c1 : index
    memref.store %12, %alloca[] : memref<index>
    %13 = call @rand() : () -> i32
    %14 = arith.remsi %13, %c100_i32 : i32
    %15 = "polygeist.memref2pointer"(%11) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %14, %15 : i32, !llvm.ptr
    %16 = llvm.mlir.addressof @str3 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
    %18 = llvm.call @printf(%17, %1, %1, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    return
  }
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arith.cmpi slt, %arg0, %c2_i32 : i32
    %2 = arith.cmpi sge, %arg0, %c2_i32 : i32
    %3 = arith.select %1, %c1_i32, %0 : i32
    scf.if %1 {
      %5 = llvm.mlir.addressof @stderr : !llvm.ptr
      %6 = llvm.load %5 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %7 = "polygeist.memref2pointer"(%6) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %8 = llvm.mlir.addressof @str0 : !llvm.ptr
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
      %10 = memref.load %arg1[%c0] : memref<?xmemref<?xi8>>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.call @fprintf(%7, %9, %11) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    }
    %4 = arith.select %2, %c0_i32, %3 : i32
    scf.if %2 {
      %5 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
      %6 = func.call @atoi(%5) : (memref<?xi8>) -> i32
      %7 = arith.index_cast %6 : i32 to index
      %alloca = memref.alloca(%7) : memref<?xi32>
      %8 = llvm.mlir.zero : !llvm.ptr
      %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi64>
      %10 = func.call @time(%9) : (memref<?xi64>) -> i64
      %11 = arith.trunci %10 : i64 to i32
      func.call @srand(%11) : (i32) -> ()
      %12 = llvm.mlir.addressof @str1 : !llvm.ptr
      %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
      %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %15 = llvm.mlir.addressof @str2 : !llvm.ptr
      %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
      %17 = llvm.call @printf(%16, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
      %18 = func.call @artsGetCurrentNode() : () -> i32
      %19 = arith.index_cast %c4 : index to i64
      %c9_i32 = arith.constant 9 : i32
      %alloca_0 = memref.alloca(%7) : memref<?xi64>
      %alloca_1 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      %c0_2 = arith.constant 0 : index
      %c1_3 = arith.constant 1 : index
      scf.for %arg2 = %c0_2 to %7 step %c1_3 {
        %51 = func.call @artsReserveGuidRoute(%c9_i32, %18) : (i32, i32) -> i64
        %52 = func.call @artsDbCreateWithGuid(%51, %19) : (i64, i64) -> memref<?xi8>
        memref.store %51, %alloca_0[%arg2] : memref<?xi64>
        memref.store %52, %alloca_1[%arg2] : memref<?xmemref<?xi8>>
      }
      %20 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32_4 = arith.constant 0 : i32
      %alloca_5 = memref.alloca() : memref<i32>
      memref.store %c0_i32_4, %alloca_5[] : memref<i32>
      %alloca_6 = memref.alloca() : memref<i32>
      %c0_i32_7 = arith.constant 0 : i32
      memref.store %c0_i32_7, %alloca_6[] : memref<i32>
      %21 = memref.load %alloca_6[] : memref<i32>
      %22 = arith.index_cast %21 : i32 to index
      %alloca_8 = memref.alloca(%22) : memref<?xi64>
      %c1_i32_9 = arith.constant 1 : i32
      %23 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %25 = func.call @artsEdtCreate(%24, %20, %21, %alloca_8, %c1_i32_9) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
      %c0_i32_10 = arith.constant 0 : i32
      %26 = func.call @artsInitializeAndStartEpoch(%25, %c0_i32_10) : (i64, i32) -> i64
      %27 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32_11 = arith.constant 0 : i32
      %alloca_12 = memref.alloca() : memref<i32>
      memref.store %c0_i32_11, %alloca_12[] : memref<i32>
      %alloca_13 = memref.alloca() : memref<i32>
      memref.store %c0_i32_11, %alloca_13[] : memref<i32>
      %28 = memref.load %alloca_13[] : memref<i32>
      %alloca_14 = memref.alloca() : memref<i32>
      %c1_i32_15 = arith.constant 1 : i32
      memref.store %c1_i32_15, %alloca_14[] : memref<i32>
      %29 = memref.load %alloca_14[] : memref<i32>
      %30 = arith.index_cast %7 : index to i32
      %31 = arith.muli %29, %30 : i32
      memref.store %31, %alloca_14[] : memref<i32>
      %32 = memref.load %alloca_14[] : memref<i32>
      %33 = memref.load %alloca_13[] : memref<i32>
      %34 = arith.addi %33, %32 : i32
      memref.store %34, %alloca_13[] : memref<i32>
      %35 = memref.load %alloca_13[] : memref<i32>
      %alloca_16 = memref.alloca() : memref<i32>
      %c2_i32_17 = arith.constant 2 : i32
      memref.store %c2_i32_17, %alloca_16[] : memref<i32>
      %36 = memref.load %alloca_16[] : memref<i32>
      %37 = arith.index_cast %36 : i32 to index
      %alloca_18 = memref.alloca(%37) : memref<?xi64>
      %38 = arith.index_cast %7 : index to i64
      %c0_19 = arith.constant 0 : index
      affine.store %38, %alloca_18[%c0_19] : memref<?xi64>
      %39 = arith.index_cast %7 : index to i64
      %c1_20 = arith.constant 1 : index
      affine.store %39, %alloca_18[%c1_20] : memref<?xi64>
      %40 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %41 = "polygeist.pointer2memref"(%40) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %42 = func.call @artsEdtCreateWithEpoch(%41, %27, %36, %alloca_18, %35, %26) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %alloc = memref.alloc() : memref<i32>
      memref.store %28, %alloc[] : memref<i32>
      %c0_21 = arith.constant 0 : index
      %c1_22 = arith.constant 1 : index
      %43 = scf.for %arg2 = %c0_21 to %7 step %c1_22 iter_args(%arg3 = %alloc) -> (memref<i32>) {
        %51 = memref.load %alloca_0[%arg2] : memref<?xi64>
        %52 = memref.load %arg3[] : memref<i32>
        func.call @artsSignalEdt(%42, %52, %51) : (i64, i32, i64) -> ()
        scf.yield %arg3 : memref<i32>
      }
      %44 = func.call @artsWaitOnHandle(%26) : (i64) -> i1
      %45 = llvm.mlir.addressof @str4 : !llvm.ptr
      %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %47 = llvm.call @printf(%46) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %51 = arith.index_cast %arg2 : index to i32
        %52 = llvm.mlir.addressof @str5 : !llvm.ptr
        %53 = llvm.getelementptr %52[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %54 = "polygeist.memref2pointer"(%alloca_1) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %55 = arith.index_cast %arg2 : index to i64
        %56 = llvm.getelementptr %54[%55] : (!llvm.ptr, i64) -> !llvm.ptr, i32
        %57 = llvm.load %56 : !llvm.ptr -> i32
        %58 = memref.load %alloca[%arg2] : memref<?xi32>
        %59 = llvm.call @printf(%53, %51, %57, %51, %58) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %48 = llvm.mlir.addressof @str6 : !llvm.ptr
      %49 = llvm.getelementptr %48[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %50 = llvm.call @printf(%49) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    return
  }
  func.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    %0 = arith.index_cast %arg0 : i32 to index
    %c1 = arith.constant 1 : index
    %1 = arith.cmpi uge, %0, %c1 : index
    cf.cond_br %1, ^bb1, ^bb2(%arg1, %arg2 : i32, memref<?xmemref<?xi8>>)
  ^bb1:  // pred: ^bb0
    return
  ^bb2(%2: i32, %3: memref<?xmemref<?xi8>>):  // pred: ^bb0
    %4 = call @mainBody(%arg1, %arg2) : (i32, memref<?xmemref<?xi8>>) -> i32
    call @artsShutdown() : () -> ()
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 {
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    %c0_i32 = arith.constant 0 : i32
    return %c0_i32 : i32
  }
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str6("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c24 = arith.constant 24 : index
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c1] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_0 = memref.alloca(%1) : memref<?xi64>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %3 = memref.load %alloca[] : memref<index>
      %4 = arith.muli %3, %c24 : index
      %5 = arith.index_cast %4 : index to i32
      %6 = llvm.getelementptr %2[%5] : (!llvm.ptr, i32) -> !llvm.ptr, i8
      %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %8 = llvm.load %7 : !llvm.ptr -> i64
      memref.store %8, %alloca_0[%arg4] : memref<?xi64>
      %9 = arith.addi %3, %c1 : index
      memref.store %9, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<i32>
    %alloca_2 = memref.alloca() : memref<i32>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %3 = arith.index_cast %arg4 : index to i32
      %4 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %5 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0_i32, %alloca_1[] : memref<i32>
      %6 = memref.load %alloca_1[] : memref<i32>
      %7 = arith.addi %6, %c1_i32 : i32
      memref.store %7, %alloca_1[] : memref<i32>
      %8 = memref.load %alloca_1[] : memref<i32>
      memref.store %c1_i32, %alloca_2[] : memref<i32>
      %9 = memref.load %alloca_2[] : memref<i32>
      %10 = arith.index_cast %9 : i32 to index
      %alloca_3 = memref.alloca(%10) : memref<?xi64>
      %11 = arith.extsi %3 : i32 to i64
      affine.store %11, %alloca_3[0] : memref<?xi64>
      %12 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %14 = func.call @artsEdtCreateWithEpoch(%13, %5, %9, %alloca_3, %8, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %15 = memref.load %alloca_0[%arg4] : memref<?xi64>
      func.call @artsSignalEdt(%14, %6, %15) : (i64, i32, i64) -> ()
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100_i32 = arith.constant 100 : i32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %3 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> memref<?xi8>
    %5 = call @rand() : () -> i32
    %6 = arith.remsi %5, %c100_i32 : i32
    %7 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %6, %7 : i32, !llvm.ptr
    %8 = llvm.mlir.addressof @str3 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
    %10 = llvm.call @printf(%9, %1, %1, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    return
  }
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4_i64 = arith.constant 4 : i64
    %c9_i32 = arith.constant 9 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %0 = llvm.mlir.undef : i32
    %1 = arith.cmpi slt, %arg0, %c2_i32 : i32
    %2 = arith.cmpi sge, %arg0, %c2_i32 : i32
    %3 = arith.select %1, %c1_i32, %0 : i32
    scf.if %1 {
      %5 = llvm.mlir.addressof @stderr : !llvm.ptr
      %6 = llvm.load %5 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %7 = "polygeist.memref2pointer"(%6) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %8 = llvm.mlir.addressof @str0 : !llvm.ptr
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
      %10 = memref.load %arg1[%c0] : memref<?xmemref<?xi8>>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.call @fprintf(%7, %9, %11) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr, !llvm.ptr) -> i32
    }
    %4 = arith.select %2, %c0_i32, %3 : i32
    scf.if %2 {
      %5 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
      %6 = func.call @atoi(%5) : (memref<?xi8>) -> i32
      %7 = arith.index_cast %6 : i32 to index
      %alloca = memref.alloca(%7) : memref<?xi32>
      %8 = llvm.mlir.zero : !llvm.ptr
      %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi64>
      %10 = func.call @time(%9) : (memref<?xi64>) -> i64
      %11 = arith.trunci %10 : i64 to i32
      func.call @srand(%11) : (i32) -> ()
      %12 = llvm.mlir.addressof @str1 : !llvm.ptr
      %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
      %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %15 = llvm.mlir.addressof @str2 : !llvm.ptr
      %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<42 x i8>
      %17 = llvm.call @printf(%16, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
      %18 = func.call @artsGetCurrentNode() : () -> i32
      %alloca_0 = memref.alloca(%7) : memref<?xi64>
      %alloca_1 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      scf.for %arg2 = %c0 to %7 step %c1 {
        %36 = func.call @artsReserveGuidRoute(%c9_i32, %18) : (i32, i32) -> i64
        %37 = func.call @artsDbCreateWithGuid(%36, %c4_i64) : (i64, i64) -> memref<?xi8>
        memref.store %36, %alloca_0[%arg2] : memref<?xi64>
        memref.store %37, %alloca_1[%arg2] : memref<?xmemref<?xi8>>
      }
      %19 = func.call @artsGetCurrentNode() : () -> i32
      %alloca_2 = memref.alloca() : memref<0xi64>
      %cast = memref.cast %alloca_2 : memref<0xi64> to memref<?xi64>
      %20 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %21 = "polygeist.pointer2memref"(%20) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %22 = func.call @artsEdtCreate(%21, %19, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
      %23 = func.call @artsInitializeAndStartEpoch(%22, %c0_i32) : (i64, i32) -> i64
      %24 = func.call @artsGetCurrentNode() : () -> i32
      %alloca_3 = memref.alloca() : memref<2xi64>
      %cast_4 = memref.cast %alloca_3 : memref<2xi64> to memref<?xi64>
      %25 = arith.index_cast %7 : index to i64
      affine.store %25, %alloca_3[0] : memref<2xi64>
      affine.store %25, %alloca_3[1] : memref<2xi64>
      %26 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %27 = "polygeist.pointer2memref"(%26) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %28 = func.call @artsEdtCreateWithEpoch(%27, %24, %c2_i32, %cast_4, %6, %23) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      scf.for %arg2 = %c0 to %7 step %c1 {
        %36 = memref.load %alloca_0[%arg2] : memref<?xi64>
        func.call @artsSignalEdt(%28, %c0_i32, %36) : (i64, i32, i64) -> ()
      }
      %29 = func.call @artsWaitOnHandle(%23) : (i64) -> i1
      %30 = llvm.mlir.addressof @str4 : !llvm.ptr
      %31 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %32 = llvm.call @printf(%31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %36 = arith.index_cast %arg2 : index to i32
        %37 = llvm.mlir.addressof @str5 : !llvm.ptr
        %38 = llvm.getelementptr %37[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %39 = "polygeist.memref2pointer"(%alloca_1) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %40 = arith.index_cast %arg2 : index to i64
        %41 = llvm.getelementptr %39[%40] : (!llvm.ptr, i64) -> !llvm.ptr, i32
        %42 = llvm.load %41 : !llvm.ptr -> i32
        %43 = memref.load %alloca[%arg2] : memref<?xi32>
        %44 = llvm.call @printf(%38, %36, %42, %36, %43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %33 = llvm.mlir.addressof @str6 : !llvm.ptr
      %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %35 = llvm.call @printf(%34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    return
  }
  func.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) {
    %c1 = arith.constant 1 : index
    %0 = arith.index_cast %arg0 : i32 to index
    %1 = arith.cmpi uge, %0, %c1 : index
    cf.cond_br %1, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    return
  ^bb2:  // pred: ^bb0
    %2 = call @mainBody(%arg1, %arg2) : (i32, memref<?xmemref<?xi8>>) -> i32
    call @artsShutdown() : () -> ()
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 {
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    return %c0_i32 : i32
  }
}

