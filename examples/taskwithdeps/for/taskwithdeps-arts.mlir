
-----------------------------------------
ConvertOpenMPToArtsPass STARTED
-----------------------------------------
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.parallel to arts.parallel
-----------------------------------------
ConvertOpenMPToArtsPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("A[0] = %d, B[0] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5_i32 = arith.constant 5 : i32
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
          %30 = arith.index_cast %6 : i32 to index
          scf.for %arg2 = %c0 to %30 step %c1 {
            %31 = arith.index_cast %arg2 : index to i32
            %32 = memref.load %alloca[%arg2] : memref<?xi32>
            %33 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %34 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%33] -> memref<i32>
            arts.edt dependencies(%34) : (memref<i32>) attributes {task} {
              memref.store %31, %alloca[%arg2] : memref<?xi32>
              %52 = llvm.mlir.addressof @str3 : !llvm.ptr
              %53 = llvm.getelementptr %52[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
              %54 = llvm.call @printf(%53, %31, %31, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              arts.yield
            }
            %35 = memref.load %alloca[%arg2] : memref<?xi32>
            %36 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %37 = arts.datablock "in" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%36] -> memref<i32>
            %38 = memref.load %alloca_0[%arg2] : memref<?xi32>
            %39 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %40 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%39] -> memref<i32>
            arts.edt dependencies(%37, %40) : (memref<i32>, memref<i32>) attributes {task} {
              %52 = memref.load %alloca[%arg2] : memref<?xi32>
              %53 = arith.addi %52, %c5_i32 : i32
              memref.store %53, %alloca_0[%arg2] : memref<?xi32>
              %54 = llvm.mlir.addressof @str4 : !llvm.ptr
              %55 = llvm.getelementptr %54[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
              %56 = llvm.call @printf(%55, %31, %31, %53) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              arts.yield
            }
            %41 = arith.addi %31, %c-1_i32 : i32
            %42 = arith.index_cast %41 : i32 to index
            %43 = memref.load %alloca_0[%42] : memref<?xi32>
            %44 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %45 = arts.datablock "in" ptr[%alloca_0 : memref<?xi32>], indices[%42], sizes[], type[i32], typeSize[%44] -> memref<i32>
            %46 = memref.load %alloca[%arg2] : memref<?xi32>
            %47 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %48 = arts.datablock "in" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%47] -> memref<i32>
            %49 = memref.load %alloca_0[%arg2] : memref<?xi32>
            %50 = "polygeist.typeSize"() <{source = i32}> : () -> index
            %51 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%50] -> memref<i32>
            arts.edt dependencies(%45, %48, %51) : (memref<i32>, memref<i32>, memref<i32>) attributes {task} {
              %52 = memref.load %alloca[%arg2] : memref<?xi32>
              %53 = memref.load %alloca_0[%42] : memref<?xi32>
              %54 = arith.addi %52, %53 : i32
              %55 = arith.addi %54, %c5_i32 : i32
              memref.store %55, %alloca_0[%arg2] : memref<?xi32>
              %56 = llvm.mlir.addressof @str5 : !llvm.ptr
              %57 = llvm.getelementptr %56[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
              %58 = llvm.call @printf(%57, %31, %31, %55) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
              arts.yield
            }
          }
          arts.yield
        }
        arts.barrier
        arts.yield
      }
      %18 = llvm.mlir.addressof @str6 : !llvm.ptr
      %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %20 = llvm.call @printf(%19) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %21 = llvm.mlir.addressof @str7 : !llvm.ptr
      %22 = llvm.getelementptr %21[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
      %23 = memref.load %alloca[%c0] : memref<?xi32>
      %24 = memref.load %alloca_0[%c0] : memref<?xi32>
      %25 = llvm.call @printf(%22, %23, %24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      %26 = arith.index_cast %6 : i32 to index
      scf.for %arg2 = %c0 to %26 step %c1 {
        %30 = arith.index_cast %arg2 : index to i32
        %31 = llvm.mlir.addressof @str8 : !llvm.ptr
        %32 = llvm.getelementptr %31[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %33 = memref.load %alloca[%arg2] : memref<?xi32>
        %34 = memref.load %alloca_0[%arg2] : memref<?xi32>
        %35 = llvm.call @printf(%32, %30, %33, %30, %34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %27 = llvm.mlir.addressof @str9 : !llvm.ptr
      %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %29 = llvm.call @printf(%28) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
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
    Memref: %alloca_0 = memref.alloca(%7) : memref<?xi32>
    Access Type: inout
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %53, %alloca_0[%arg2] : memref<?xi32>
  - Candidate Datablock
    Memref: %alloca = memref.alloca(%7) : memref<?xi32>
    Access Type: in
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - %52 = memref.load %alloca[%arg2] : memref<?xi32>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca(%7) : memref<?xi32>
    Access Type: in
    Pinned Indices:
      - %42 = arith.index_cast %41 : i32 to index
    Uses:
    - %53 = memref.load %alloca_0[%42] : memref<?xi32>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca(%7) : memref<?xi32>
    Access Type: inout
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %55, %alloca_0[%arg2] : memref<?xi32>
  - Candidate Datablock
    Memref: %alloca = memref.alloca(%7) : memref<?xi32>
    Access Type: in
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - %52 = memref.load %alloca[%arg2] : memref<?xi32>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca(%7) : memref<?xi32>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - memref.store %55, %alloca_0[%arg2] : memref<?xi32>
    - %53 = memref.load %alloca_0[%42] : memref<?xi32>
    - %49 = memref.load %alloca_0[%arg2] : memref<?xi32>
    - %43 = memref.load %alloca_0[%42] : memref<?xi32>
    - memref.store %53, %alloca_0[%arg2] : memref<?xi32>
    - %38 = memref.load %alloca_0[%arg2] : memref<?xi32>
  - Candidate Datablock
    Memref: %alloca = memref.alloca(%7) : memref<?xi32>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - %52 = memref.load %alloca[%arg2] : memref<?xi32>
    - %52 = memref.load %alloca[%arg2] : memref<?xi32>
    - %46 = memref.load %alloca[%arg2] : memref<?xi32>
    - %35 = memref.load %alloca[%arg2] : memref<?xi32>
    - memref.store %31, %alloca[%arg2] : memref<?xi32>
    - %32 = memref.load %alloca[%arg2] : memref<?xi32>
[create-datablocks] Candidate datablocks in function: atoi
[create-datablocks] Candidate datablocks in function: srand
[create-datablocks] Candidate datablocks in function: time
-----------------------------------------
Rewiring uses of:
  %36 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%35] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %43 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%42] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %45 = arts.datablock "in" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%44] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %56 = arts.datablock "in" ptr[%alloca_0 : memref<?xi32>], indices[%45], sizes[], type[i32], typeSize[%55] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %58 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%57] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %60 = arts.datablock "in" ptr[%alloca : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%59] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %19 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[], sizes[%dim], type[i32], typeSize[%18] -> memref<?xi32>
-----------------------------------------
Rewiring uses of:
  %21 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[], sizes[%dim_3], type[i32], typeSize[%20] -> memref<?xi32>
-----------------------------------------
CreateDatablocksPass FINISHED
-----------------------------------------

-----------------------------------------
DatablockPass STARTED
-----------------------------------------
-----------------------------------------
[datablock-analysis] Printing graph for function: main
Nodes:
  #0 inout
    %18 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xi32>
    isLoopDependent=false useCount=6 hasPtrDb=false userEdtPos=0
  #1 inout
    %19 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xi32>
    isLoopDependent=false useCount=6 hasPtrDb=false userEdtPos=1
  #2 inout
    %32 = arts.datablock "inout" ptr[%19 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=0
  #3 inout
    %33 = arts.datablock "inout" ptr[%18 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=0
  #4 in
    %34 = arts.datablock "in" ptr[%19 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=1
  #5 in
    %37 = arts.datablock "in" ptr[%18 : memref<?xi32>], indices[%36], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=0
  #6 inout
    %38 = arts.datablock "inout" ptr[%18 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=1
  #7 in
    %39 = arts.datablock "in" ptr[%19 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=2
Edges:
  #2 -> #4 (direct, loop dependent)
  #2 -> #7 (direct, loop dependent)
  #3 -> #5 (indirect, loop dependent)
  #3 -> #6 (direct, loop dependent)
  #6 -> #5 (indirect, loop dependent)
Total nodes: 8
-----------------------------------------
DatablockPass FINISHED
-----------------------------------------
-----------------------------------------
CreateEventsPass STARTED
-----------------------------------------
Events:
  Event
    Producer: 2, Consumer: 4
    Producer: 2, Consumer: 7
  Event
    Producer: 3, Consumer: 5
    Producer: 3, Consumer: 6
[create-events] Processing grouped event
[create-events] Processing grouped event
-----------------------------------------
CreateEventsPass FINISHED
-----------------------------------------
-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("A[0] = %d, B[0] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5_i32 = arith.constant 5 : i32
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
      %18 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xi32>
      %19 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xi32>
      arts.edt dependencies(%18, %19) : (memref<?xi32>, memref<?xi32>) attributes {sync} {
        %31 = arts.event[%7] -> : memref<?xi64>
        %32 = arts.event[%7] -> : memref<?xi64>
        scf.for %arg2 = %c0 to %7 step %c1 {
          %33 = arith.index_cast %arg2 : index to i32
          // ---------- A[i] -> #2 inout ---------------- //
          %34 = arts.datablock "inout" ptr[%19 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          // EDT 0
          arts.edt dependencies(%34) : (memref<i32>) attributes {task} {
            memref.store %33, %34[] : memref<i32>
            %42 = llvm.mlir.addressof @str3 : !llvm.ptr
            %43 = llvm.getelementptr %42[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
            %44 = llvm.call @printf(%43, %33, %33, %33) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
            arts.yield
          }
          // ---------- B[i] -> #3 inout ---------------- //
          %35 = arts.datablock "inout" ptr[%18 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          // ---------- A[i] -> #4 in ---------------- //
          %36 = arts.datablock "in" ptr[%19 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          // EDT 1
          arts.edt dependencies(%35, %36) : (memref<i32>, memref<i32>) attributes {task} {
          }
          %37 = arith.addi %33, %c-1_i32 : i32
          %38 = arith.index_cast %37 : i32 to index
          // ---------- B[i] -> #6 inout ---------------- //
          %39 = arts.datablock "in" ptr[%18 : memref<?xi32>], indices[%38], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          // ---------- B[i-1] -> #5 in ---------------- //
          %40 = arts.datablock "inout" ptr[%18 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          // ---------- A[i] -> #7 in ---------------- //
          %41 = arts.datablock "in" ptr[%19 : memref<?xi32>], indices[%arg2], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
          // EDT 2
          arts.edt dependencies(%39, %40, %41) : (memref<i32>, memref<i32>, memref<i32>) attributes {task} {
          }
        }
        arts.yield
      }
      %20 = llvm.mlir.addressof @str6 : !llvm.ptr
      %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %22 = llvm.call @printf(%21) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %23 = llvm.mlir.addressof @str7 : !llvm.ptr
      %24 = llvm.getelementptr %23[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
      %25 = memref.load %19[%c0] : memref<?xi32>
      %26 = memref.load %18[%c0] : memref<?xi32>
      %27 = llvm.call @printf(%24, %25, %26) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %31 = arith.index_cast %arg2 : index to i32
        %32 = llvm.mlir.addressof @str8 : !llvm.ptr
        %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %34 = memref.load %19[%arg2] : memref<?xi32>
        %35 = memref.load %18[%arg2] : memref<?xi32>
        %36 = llvm.call @printf(%33, %31, %34, %31, %35) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %28 = llvm.mlir.addressof @str9 : !llvm.ptr
      %29 = llvm.getelementptr %28[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %30 = llvm.call @printf(%29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-llvm] Iterate over all the functions
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] Created array of datablocks: %alloca_2 = memref.alloca(%7) : memref<?xmemref<?xi8>>
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] Created array of datablocks: %alloca_7 = memref.alloca(%7) : memref<?xmemref<?xi8>>
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] DB has a pointer to another datablock...
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] DB has a pointer to another datablock...
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] DB has a pointer to another datablock...
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] DB has a pointer to another datablock...
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] DB has a pointer to another datablock...
[convert-arts-to-llvm] Lowering arts.datablock
[arts-codegen] DB has a pointer to another datablock...
Module after iterating Datablocks:
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("A[0] = %d, B[0] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5_i32 = arith.constant 5 : i32
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
      %18 = func.call @artsGetCurrentNode() : () -> i32
      %19 = arith.index_cast %c4 : index to i64
      %c9_i32 = arith.constant 9 : i32
      %alloca_1 = memref.alloca(%7) : memref<?xi64>
      %alloca_2 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      %c0_3 = arith.constant 0 : index
      %c1_4 = arith.constant 1 : index
      scf.for %arg2 = %c0_3 to %7 step %c1_4 {
        %43 = func.call @artsReserveGuidRoute(%c9_i32, %18) : (i32, i32) -> i64
        %44 = func.call @artsDbCreateWithGuid(%43, %19) : (i64, i64) -> memref<?xi8>
        memref.store %43, %alloca_1[%arg2] : memref<?xi64>
        memref.store %44, %alloca_2[%arg2] : memref<?xmemref<?xi8>>
      }
      %20 = arts.datablock "inout" ptr[%alloca_0 : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xmemref<?xi8>>
      %21 = func.call @artsGetCurrentNode() : () -> i32
      %22 = arith.index_cast %c4 : index to i64
      %c9_i32_5 = arith.constant 9 : i32
      %alloca_6 = memref.alloca(%7) : memref<?xi64>
      %alloca_7 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      %c0_8 = arith.constant 0 : index
      %c1_9 = arith.constant 1 : index
      scf.for %arg2 = %c0_8 to %7 step %c1_9 {
        %43 = func.call @artsReserveGuidRoute(%c9_i32_5, %21) : (i32, i32) -> i64
        %44 = func.call @artsDbCreateWithGuid(%43, %22) : (i64, i64) -> memref<?xi8>
        memref.store %43, %alloca_6[%arg2] : memref<?xi64>
        memref.store %44, %alloca_7[%arg2] : memref<?xmemref<?xi8>>
      }
      %23 = arts.datablock "inout" ptr[%alloca : memref<?xi32>], indices[], sizes[%7], type[i32], typeSize[%c4] -> memref<?xmemref<?xi8>>
      arts.edt dependencies(%20, %23) : (memref<?xmemref<?xi8>>, memref<?xmemref<?xi8>>) attributes {sync} {
        %43 = arts.event[%7] -> : memref<?xi64>
        %44 = arts.event[%7] -> : memref<?xi64>
        scf.for %arg2 = %c0 to %7 step %c1 {
          %45 = arith.index_cast %arg2 : index to i32
          %46 = arts.datablock "inout" ptr[%23 : memref<?xmemref<?xi8>>], indices[%arg2], sizes[], type[i32], typeSize[%c4], outEvent[%44] {hasPtrDb, single} -> memref<?xi8>
          arts.edt dependencies(%46) : (memref<?xi8>) attributes {task} {
            %54 = "polygeist.memref2pointer"(%46) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %45, %54 : i32, !llvm.ptr
            %55 = llvm.mlir.addressof @str3 : !llvm.ptr
            %56 = llvm.getelementptr %55[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
            %57 = llvm.call @printf(%56, %45, %45, %45) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
            arts.yield
          }
          %47 = arts.datablock "inout" ptr[%20 : memref<?xmemref<?xi8>>], indices[%arg2], sizes[], type[i32], typeSize[%c4], outEvent[%43] {hasPtrDb, single} -> memref<?xi8>
          %48 = arts.datablock "in" ptr[%23 : memref<?xmemref<?xi8>>], indices[%arg2], sizes[], type[i32], typeSize[%c4], inEvent[%44] {hasPtrDb, single} -> memref<?xi8>
          arts.edt dependencies(%47, %48) : (memref<?xi8>, memref<?xi8>) attributes {task} {
            %54 = "polygeist.memref2pointer"(%48) : (memref<?xi8>) -> !llvm.ptr
            %55 = llvm.load %54 : !llvm.ptr -> i32
            %56 = arith.addi %55, %c5_i32 : i32
            %57 = "polygeist.memref2pointer"(%47) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %56, %57 : i32, !llvm.ptr
            %58 = llvm.mlir.addressof @str4 : !llvm.ptr
            %59 = llvm.getelementptr %58[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
            %60 = llvm.call @printf(%59, %45, %45, %56) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
            arts.yield
          }
          %49 = arith.addi %45, %c-1_i32 : i32
          %50 = arith.index_cast %49 : i32 to index
          %51 = arts.datablock "in" ptr[%20 : memref<?xmemref<?xi8>>], indices[%50], sizes[], type[i32], typeSize[%c4], inEvent[%43] {hasPtrDb, single} -> memref<?xi8>
          %52 = arts.datablock "inout" ptr[%20 : memref<?xmemref<?xi8>>], indices[%arg2], sizes[], type[i32], typeSize[%c4], inEvent[%43] {hasPtrDb, single} -> memref<?xi8>
          %53 = arts.datablock "in" ptr[%23 : memref<?xmemref<?xi8>>], indices[%arg2], sizes[], type[i32], typeSize[%c4], inEvent[%44] {hasPtrDb, single} -> memref<?xi8>
          arts.edt dependencies(%51, %52, %53) : (memref<?xi8>, memref<?xi8>, memref<?xi8>) attributes {task} {
            %54 = "polygeist.memref2pointer"(%53) : (memref<?xi8>) -> !llvm.ptr
            %55 = llvm.load %54 : !llvm.ptr -> i32
            %56 = "polygeist.memref2pointer"(%51) : (memref<?xi8>) -> !llvm.ptr
            %57 = llvm.load %56 : !llvm.ptr -> i32
            %58 = arith.addi %55, %57 : i32
            %59 = arith.addi %58, %c5_i32 : i32
            %60 = "polygeist.memref2pointer"(%52) : (memref<?xi8>) -> !llvm.ptr
            llvm.store %59, %60 : i32, !llvm.ptr
            %61 = llvm.mlir.addressof @str5 : !llvm.ptr
            %62 = llvm.getelementptr %61[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
            %63 = llvm.call @printf(%62, %45, %45, %59) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
            arts.yield
          }
        }
        arts.yield
      }
      %24 = llvm.mlir.addressof @str6 : !llvm.ptr
      %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %26 = llvm.call @printf(%25) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %27 = llvm.mlir.addressof @str7 : !llvm.ptr
      %28 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
      %29 = "polygeist.memref2pointer"(%23) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %30 = arith.index_cast %c0 : index to i64
      %31 = llvm.getelementptr %29[%30] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %32 = llvm.load %31 : !llvm.ptr -> !llvm.ptr
      %33 = llvm.load %32 : !llvm.ptr -> i32
      %34 = "polygeist.memref2pointer"(%20) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %35 = arith.index_cast %c0 : index to i64
      %36 = llvm.getelementptr %34[%35] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %37 = llvm.load %36 : !llvm.ptr -> !llvm.ptr
      %38 = llvm.load %37 : !llvm.ptr -> i32
      %39 = llvm.call @printf(%28, %33, %38) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %43 = arith.index_cast %arg2 : index to i32
        %44 = llvm.mlir.addressof @str8 : !llvm.ptr
        %45 = llvm.getelementptr %44[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %46 = "polygeist.memref2pointer"(%23) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %47 = arith.index_cast %arg2 : index to i64
        %48 = llvm.getelementptr %46[%47] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
        %49 = llvm.load %48 : !llvm.ptr -> !llvm.ptr
        %50 = llvm.load %49 : !llvm.ptr -> i32
        %51 = "polygeist.memref2pointer"(%20) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %52 = arith.index_cast %arg2 : index to i64
        %53 = llvm.getelementptr %51[%52] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
        %54 = llvm.load %53 : !llvm.ptr -> !llvm.ptr
        %55 = llvm.load %54 : !llvm.ptr -> i32
        %56 = llvm.call @printf(%45, %43, %50, %43, %55) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %40 = llvm.mlir.addressof @str9 : !llvm.ptr
      %41 = llvm.getelementptr %40[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %42 = llvm.call @printf(%41) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return %4 : i32
  }
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-llvm] Lowering arts.edt sync
-----------------------------------------
[convert-arts-to-llvm] Sync done EDT created
[convert-arts-to-llvm] Parallel epoch created
[convert-arts-to-llvm] Parallel EDT created
[convert-arts-to-llvm] Parallel op lowered

[convert-arts-to-llvm] Lowering arts.event
[convert-arts-to-llvm] Lowering arts.event
[convert-arts-to-llvm] Lowering arts.edt
[convert-arts-to-llvm] Lowering arts.edt
[convert-arts-to-llvm] Lowering arts.edt
-----------------------------------------
ConvertArtsToLLVMPass FINISHED 
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("A[0] = %d, B[0] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c4 = arith.constant 4 : index
    %c5_i32 = arith.constant 5 : i32
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %c1_1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_1] : memref<?xi64>
    %3 = arith.index_cast %2 : i64 to index
    %c2 = arith.constant 2 : index
    %4 = memref.load %arg1[%c2] : memref<?xi64>
    %5 = arith.index_cast %4 : i64 to index
    %c0_2 = arith.constant 0 : index
    %c1_3 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0_2, %alloca[] : memref<index>
    %6 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, ptr)>}> : () -> index
    %7 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_4 = memref.alloca(%5) : memref<?xi64>
    %alloca_5 = memref.alloca(%5) : memref<?xmemref<?xi8>>
    %c0_6 = arith.constant 0 : index
    scf.for %arg4 = %c0_6 to %5 step %c1_3 {
      %10 = memref.load %alloca[] : memref<index>
      %11 = arith.muli %10, %6 : index
      %12 = arith.index_cast %11 : index to i32
      %13 = llvm.getelementptr %7[%12] : (!llvm.ptr, i32) -> !llvm.ptr, i8
      %c0_i32 = arith.constant 0 : i32
      %14 = llvm.getelementptr %13[%c0_i32, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %15 = llvm.load %14 : !llvm.ptr -> i64
      %c0_i32_16 = arith.constant 0 : i32
      %c2_i32 = arith.constant 2 : i32
      %16 = llvm.getelementptr %13[%c0_i32_16, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %17 = llvm.load %16 : !llvm.ptr -> memref<?xi8>
      memref.store %15, %alloca_4[%arg4] : memref<?xi64>
      memref.store %17, %alloca_5[%arg4] : memref<?xmemref<?xi8>>
      %18 = arith.addi %10, %c1_3 : index
      memref.store %18, %alloca[] : memref<index>
    }
    %alloca_7 = memref.alloca(%5) : memref<?xi64>
    %alloca_8 = memref.alloca(%5) : memref<?xmemref<?xi8>>
    %c0_9 = arith.constant 0 : index
    scf.for %arg4 = %c0_9 to %5 step %c1_3 {
      %10 = memref.load %alloca[] : memref<index>
      %11 = arith.muli %10, %6 : index
      %12 = arith.index_cast %11 : index to i32
      %13 = llvm.getelementptr %7[%12] : (!llvm.ptr, i32) -> !llvm.ptr, i8
      %c0_i32 = arith.constant 0 : i32
      %14 = llvm.getelementptr %13[%c0_i32, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %15 = llvm.load %14 : !llvm.ptr -> i64
      %c0_i32_16 = arith.constant 0 : i32
      %c2_i32 = arith.constant 2 : i32
      %16 = llvm.getelementptr %13[%c0_i32_16, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %17 = llvm.load %16 : !llvm.ptr -> memref<?xi8>
      memref.store %15, %alloca_7[%arg4] : memref<?xi64>
      memref.store %17, %alloca_8[%arg4] : memref<?xmemref<?xi8>>
      %18 = arith.addi %10, %c1_3 : index
      memref.store %18, %alloca[] : memref<index>
    }
    %8 = call @artsGetCurrentNode() : () -> i32
    %alloca_10 = memref.alloca(%5) : memref<?xi64>
    %c0_11 = arith.constant 0 : index
    %c1_12 = arith.constant 1 : index
    scf.for %arg4 = %c0_11 to %5 step %c1_12 {
      %c1_i32 = arith.constant 1 : i32
      %10 = func.call @artsEventCreate(%8, %c1_i32) : (i32, i32) -> i64
      memref.store %10, %alloca_10[%arg4] : memref<?xi64>
    }
    %9 = call @artsGetCurrentNode() : () -> i32
    %alloca_13 = memref.alloca(%5) : memref<?xi64>
    %c0_14 = arith.constant 0 : index
    %c1_15 = arith.constant 1 : index
    scf.for %arg4 = %c0_14 to %5 step %c1_15 {
      %c1_i32 = arith.constant 1 : i32
      %10 = func.call @artsEventCreate(%9, %c1_i32) : (i32, i32) -> i64
      memref.store %10, %alloca_13[%arg4] : memref<?xi64>
    }
    scf.for %arg4 = %c0 to %5 step %c1 {
      %10 = arith.index_cast %arg4 : index to i32
      %11 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %12 = func.call @artsGetCurrentNode() : () -> i32
      %c0_16 = arith.constant 0 : index
      %alloca_17 = memref.alloca() : memref<index>
      memref.store %c0_16, %alloca_17[] : memref<index>
      %alloca_18 = memref.alloca() : memref<index>
      memref.store %c0_16, %alloca_18[] : memref<index>
      %13 = memref.load %alloca_18[] : memref<index>
      %c1_19 = arith.constant 1 : index
      %14 = memref.load %alloca_18[] : memref<index>
      %15 = arith.addi %14, %c1_19 : index
      memref.store %15, %alloca_18[] : memref<index>
      %16 = memref.load %alloca_17[] : memref<index>
      %17 = arith.addi %16, %c1_19 : index
      memref.store %17, %alloca_17[] : memref<index>
      %18 = memref.load %alloca_18[] : memref<index>
      %19 = arith.index_cast %18 : index to i32
      %alloca_20 = memref.alloca() : memref<index>
      %c1_21 = arith.constant 1 : index
      memref.store %c1_21, %alloca_20[] : memref<index>
      %20 = memref.load %alloca_20[] : memref<index>
      %21 = memref.load %alloca_17[] : memref<index>
      %22 = arith.addi %20, %21 : index
      memref.store %22, %alloca_20[] : memref<index>
      %23 = memref.load %alloca_20[] : memref<index>
      %24 = arith.index_cast %23 : index to i32
      %alloca_22 = memref.alloca(%23) : memref<?xi64>
      %c0_23 = arith.constant 0 : index
      %25 = arith.extsi %10 : i32 to i64
      memref.store %25, %alloca_22[%c0_23] : memref<?xi64>
      %alloca_24 = memref.alloca() : memref<index>
      memref.store %c1_21, %alloca_24[] : memref<index>
      %26 = memref.load %alloca_13[%arg4] : memref<?xi64>
      %27 = memref.load %alloca_24[] : memref<index>
      memref.store %26, %alloca_22[%27] : memref<?xi64>
      %c1_25 = arith.constant 1 : index
      %28 = arith.addi %27, %c1_25 : index
      memref.store %28, %alloca_24[] : memref<index>
      %29 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %31 = func.call @artsEdtCreateWithEpoch(%30, %12, %24, %alloca_22, %19, %11) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %32 = memref.load %alloca_7[%arg4] : memref<?xi64>
      %33 = arith.index_cast %13 : index to i32
      func.call @artsSignalEdt(%31, %33, %32) : (i64, i32, i64) -> ()
      %34 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %35 = func.call @artsGetCurrentNode() : () -> i32
      %c0_26 = arith.constant 0 : index
      %alloca_27 = memref.alloca() : memref<index>
      memref.store %c0_26, %alloca_27[] : memref<index>
      %alloca_28 = memref.alloca() : memref<index>
      memref.store %c0_26, %alloca_28[] : memref<index>
      %36 = memref.load %alloca_28[] : memref<index>
      %c1_29 = arith.constant 1 : index
      %37 = memref.load %alloca_28[] : memref<index>
      %38 = arith.addi %37, %c1_29 : index
      memref.store %38, %alloca_28[] : memref<index>
      %39 = memref.load %alloca_27[] : memref<index>
      %40 = arith.addi %39, %c1_29 : index
      memref.store %40, %alloca_27[] : memref<index>
      %41 = memref.load %alloca_28[] : memref<index>
      %c1_30 = arith.constant 1 : index
      %42 = memref.load %alloca_28[] : memref<index>
      %43 = arith.addi %42, %c1_30 : index
      memref.store %43, %alloca_28[] : memref<index>
      %44 = memref.load %alloca_28[] : memref<index>
      %45 = arith.index_cast %44 : index to i32
      %alloca_31 = memref.alloca() : memref<index>
      %c1_32 = arith.constant 1 : index
      memref.store %c1_32, %alloca_31[] : memref<index>
      %46 = memref.load %alloca_31[] : memref<index>
      %47 = memref.load %alloca_27[] : memref<index>
      %48 = arith.addi %46, %47 : index
      memref.store %48, %alloca_31[] : memref<index>
      %49 = memref.load %alloca_31[] : memref<index>
      %50 = arith.index_cast %49 : index to i32
      %alloca_33 = memref.alloca(%49) : memref<?xi64>
      %c0_34 = arith.constant 0 : index
      %51 = arith.extsi %10 : i32 to i64
      memref.store %51, %alloca_33[%c0_34] : memref<?xi64>
      %alloca_35 = memref.alloca() : memref<index>
      memref.store %c1_32, %alloca_35[] : memref<index>
      %52 = memref.load %alloca_10[%arg4] : memref<?xi64>
      %53 = memref.load %alloca_35[] : memref<index>
      memref.store %52, %alloca_33[%53] : memref<?xi64>
      %c1_36 = arith.constant 1 : index
      %54 = arith.addi %53, %c1_36 : index
      memref.store %54, %alloca_35[] : memref<index>
      %55 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %56 = "polygeist.pointer2memref"(%55) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %57 = func.call @artsEdtCreateWithEpoch(%56, %35, %50, %alloca_33, %45, %34) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %58 = memref.load %alloca_13[%arg4] : memref<?xi64>
      %59 = arith.index_cast %41 : index to i32
      func.call @artsAddDependence(%58, %57, %59) : (i64, i64, i32) -> ()
      %60 = memref.load %alloca_4[%arg4] : memref<?xi64>
      %61 = arith.index_cast %36 : index to i32
      func.call @artsSignalEdt(%57, %61, %60) : (i64, i32, i64) -> ()
      %62 = arith.addi %10, %c-1_i32 : i32
      %63 = arith.index_cast %62 : i32 to index
      %64 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %65 = func.call @artsGetCurrentNode() : () -> i32
      %c0_37 = arith.constant 0 : index
      %alloca_38 = memref.alloca() : memref<index>
      memref.store %c0_37, %alloca_38[] : memref<index>
      %alloca_39 = memref.alloca() : memref<index>
      memref.store %c0_37, %alloca_39[] : memref<index>
      %66 = memref.load %alloca_39[] : memref<index>
      %c1_40 = arith.constant 1 : index
      %67 = memref.load %alloca_39[] : memref<index>
      %68 = arith.addi %67, %c1_40 : index
      memref.store %68, %alloca_39[] : memref<index>
      %69 = memref.load %alloca_39[] : memref<index>
      %c1_41 = arith.constant 1 : index
      %70 = memref.load %alloca_39[] : memref<index>
      %71 = arith.addi %70, %c1_41 : index
      memref.store %71, %alloca_39[] : memref<index>
      %72 = memref.load %alloca_39[] : memref<index>
      %c1_42 = arith.constant 1 : index
      %73 = memref.load %alloca_39[] : memref<index>
      %74 = arith.addi %73, %c1_42 : index
      memref.store %74, %alloca_39[] : memref<index>
      %75 = memref.load %alloca_39[] : memref<index>
      %76 = arith.index_cast %75 : index to i32
      %alloca_43 = memref.alloca() : memref<index>
      %c1_44 = arith.constant 1 : index
      memref.store %c1_44, %alloca_43[] : memref<index>
      %77 = memref.load %alloca_43[] : memref<index>
      %78 = arith.index_cast %77 : index to i32
      %alloca_45 = memref.alloca(%77) : memref<?xi64>
      %c0_46 = arith.constant 0 : index
      %79 = arith.extsi %10 : i32 to i64
      memref.store %79, %alloca_45[%c0_46] : memref<?xi64>
      %80 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %81 = "polygeist.pointer2memref"(%80) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %82 = func.call @artsEdtCreateWithEpoch(%81, %65, %78, %alloca_45, %76, %64) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %83 = memref.load %alloca_10[%63] : memref<?xi64>
      %84 = arith.index_cast %66 : index to i32
      func.call @artsAddDependence(%83, %82, %84) : (i64, i64, i32) -> ()
      %85 = memref.load %alloca_10[%arg4] : memref<?xi64>
      %86 = arith.index_cast %69 : index to i32
      func.call @artsAddDependence(%85, %82, %86) : (i64, i64, i32) -> ()
      %87 = memref.load %alloca_13[%arg4] : memref<?xi64>
      %88 = arith.index_cast %72 : index to i32
      func.call @artsAddDependence(%87, %82, %88) : (i64, i64, i32) -> ()
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
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
    %13 = "polygeist.memref2pointer"(%11) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %1, %13 : i32, !llvm.ptr
    %14 = llvm.mlir.addressof @str3 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
    %16 = llvm.call @printf(%15, %1, %1, %1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    %alloca_2 = memref.alloca() : memref<index>
    %c1_3 = arith.constant 1 : index
    memref.store %c1_3, %alloca_2[] : memref<index>
    %17 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %18 = memref.load %alloca_2[] : memref<index>
    %19 = arith.index_cast %18 : index to i32
    %20 = llvm.getelementptr %17[%19] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %21 = llvm.load %20 : !llvm.ptr -> i64
    %c0_i32_4 = arith.constant 0 : i32
    call @artsEventSatisfySlot(%21, %9, %c0_i32_4) : (i64, i64, i32) -> ()
    %c1_5 = arith.constant 1 : index
    %22 = arith.addi %18, %c1_5 : index
    memref.store %22, %alloca_2[] : memref<index>
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c5_i32 = arith.constant 5 : i32
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
    %13 = memref.load %alloca[] : memref<index>
    %14 = arith.muli %13, %2 : index
    %15 = arith.index_cast %14 : index to i32
    %16 = llvm.getelementptr %3[%15] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_2 = arith.constant 0 : i32
    %17 = llvm.getelementptr %16[%c0_i32_2, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %18 = llvm.load %17 : !llvm.ptr -> i64
    %c0_i32_3 = arith.constant 0 : i32
    %c2_i32_4 = arith.constant 2 : i32
    %19 = llvm.getelementptr %16[%c0_i32_3, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %20 = llvm.load %19 : !llvm.ptr -> memref<?xi8>
    %21 = arith.addi %13, %c1 : index
    memref.store %21, %alloca[] : memref<index>
    %22 = "polygeist.memref2pointer"(%20) : (memref<?xi8>) -> !llvm.ptr
    %23 = llvm.load %22 : !llvm.ptr -> i32
    %24 = arith.addi %23, %c5_i32 : i32
    %25 = "polygeist.memref2pointer"(%11) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %24, %25 : i32, !llvm.ptr
    %26 = llvm.mlir.addressof @str4 : !llvm.ptr
    %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %28 = llvm.call @printf(%27, %1, %1, %24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    %alloca_5 = memref.alloca() : memref<index>
    %c1_6 = arith.constant 1 : index
    memref.store %c1_6, %alloca_5[] : memref<index>
    %29 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %30 = memref.load %alloca_5[] : memref<index>
    %31 = arith.index_cast %30 : index to i32
    %32 = llvm.getelementptr %29[%31] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %33 = llvm.load %32 : !llvm.ptr -> i64
    %c0_i32_7 = arith.constant 0 : i32
    call @artsEventSatisfySlot(%33, %9, %c0_i32_7) : (i64, i64, i32) -> ()
    %c1_8 = arith.constant 1 : index
    %34 = arith.addi %30, %c1_8 : index
    memref.store %34, %alloca_5[] : memref<index>
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c5_i32 = arith.constant 5 : i32
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
    %13 = memref.load %alloca[] : memref<index>
    %14 = arith.muli %13, %2 : index
    %15 = arith.index_cast %14 : index to i32
    %16 = llvm.getelementptr %3[%15] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_2 = arith.constant 0 : i32
    %17 = llvm.getelementptr %16[%c0_i32_2, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %18 = llvm.load %17 : !llvm.ptr -> i64
    %c0_i32_3 = arith.constant 0 : i32
    %c2_i32_4 = arith.constant 2 : i32
    %19 = llvm.getelementptr %16[%c0_i32_3, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %20 = llvm.load %19 : !llvm.ptr -> memref<?xi8>
    %21 = arith.addi %13, %c1 : index
    memref.store %21, %alloca[] : memref<index>
    %22 = memref.load %alloca[] : memref<index>
    %23 = arith.muli %22, %2 : index
    %24 = arith.index_cast %23 : index to i32
    %25 = llvm.getelementptr %3[%24] : (!llvm.ptr, i32) -> !llvm.ptr, i8
    %c0_i32_5 = arith.constant 0 : i32
    %26 = llvm.getelementptr %25[%c0_i32_5, 0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %27 = llvm.load %26 : !llvm.ptr -> i64
    %c0_i32_6 = arith.constant 0 : i32
    %c2_i32_7 = arith.constant 2 : i32
    %28 = llvm.getelementptr %25[%c0_i32_6, 2] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %29 = llvm.load %28 : !llvm.ptr -> memref<?xi8>
    %30 = arith.addi %22, %c1 : index
    memref.store %30, %alloca[] : memref<index>
    %31 = "polygeist.memref2pointer"(%29) : (memref<?xi8>) -> !llvm.ptr
    %32 = llvm.load %31 : !llvm.ptr -> i32
    %33 = "polygeist.memref2pointer"(%11) : (memref<?xi8>) -> !llvm.ptr
    %34 = llvm.load %33 : !llvm.ptr -> i32
    %35 = arith.addi %32, %34 : i32
    %36 = arith.addi %35, %c5_i32 : i32
    %37 = "polygeist.memref2pointer"(%20) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %36, %37 : i32, !llvm.ptr
    %38 = llvm.mlir.addressof @str5 : !llvm.ptr
    %39 = llvm.getelementptr %38[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %40 = llvm.call @printf(%39, %1, %1, %36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    return
  }
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c5_i32 = arith.constant 5 : i32
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
      %alloca = memref.alloca(%7) : memref<?xi64>
      %alloca_0 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      %c0_1 = arith.constant 0 : index
      %c1_2 = arith.constant 1 : index
      scf.for %arg2 = %c0_1 to %7 step %c1_2 {
        %72 = func.call @artsReserveGuidRoute(%c9_i32, %18) : (i32, i32) -> i64
        %73 = func.call @artsDbCreateWithGuid(%72, %19) : (i64, i64) -> memref<?xi8>
        memref.store %72, %alloca[%arg2] : memref<?xi64>
        memref.store %73, %alloca_0[%arg2] : memref<?xmemref<?xi8>>
      }
      %20 = func.call @artsGetCurrentNode() : () -> i32
      %21 = arith.index_cast %c4 : index to i64
      %c9_i32_3 = arith.constant 9 : i32
      %alloca_4 = memref.alloca(%7) : memref<?xi64>
      %alloca_5 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      %c0_6 = arith.constant 0 : index
      %c1_7 = arith.constant 1 : index
      scf.for %arg2 = %c0_6 to %7 step %c1_7 {
        %72 = func.call @artsReserveGuidRoute(%c9_i32_3, %20) : (i32, i32) -> i64
        %73 = func.call @artsDbCreateWithGuid(%72, %21) : (i64, i64) -> memref<?xi8>
        memref.store %72, %alloca_4[%arg2] : memref<?xi64>
        memref.store %73, %alloca_5[%arg2] : memref<?xmemref<?xi8>>
      }
      %22 = func.call @artsGetCurrentNode() : () -> i32
      %c0_8 = arith.constant 0 : index
      %alloca_9 = memref.alloca() : memref<index>
      memref.store %c0_8, %alloca_9[] : memref<index>
      %alloca_10 = memref.alloca() : memref<index>
      memref.store %c0_8, %alloca_10[] : memref<index>
      %23 = memref.load %alloca_10[] : memref<index>
      %24 = arith.index_cast %23 : index to i32
      %alloca_11 = memref.alloca() : memref<index>
      %c0_12 = arith.constant 0 : index
      memref.store %c0_12, %alloca_11[] : memref<index>
      %25 = memref.load %alloca_11[] : memref<index>
      %26 = arith.index_cast %25 : index to i32
      %alloca_13 = memref.alloca(%25) : memref<?xi64>
      %c1_i32_14 = arith.constant 1 : i32
      %27 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %28 = "polygeist.pointer2memref"(%27) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %29 = func.call @artsEdtCreate(%28, %22, %26, %alloca_13, %c1_i32_14) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
      %c0_i32_15 = arith.constant 0 : i32
      %30 = func.call @artsInitializeAndStartEpoch(%29, %c0_i32_15) : (i64, i32) -> i64
      %31 = func.call @artsGetCurrentNode() : () -> i32
      %c0_16 = arith.constant 0 : index
      %alloca_17 = memref.alloca() : memref<index>
      memref.store %c0_16, %alloca_17[] : memref<index>
      %alloca_18 = memref.alloca() : memref<index>
      memref.store %c0_16, %alloca_18[] : memref<index>
      %32 = memref.load %alloca_18[] : memref<index>
      %c1_19 = arith.constant 1 : index
      %33 = arith.muli %c1_19, %7 : index
      %34 = memref.load %alloca_18[] : memref<index>
      %35 = arith.addi %34, %33 : index
      memref.store %35, %alloca_18[] : memref<index>
      %36 = memref.load %alloca_18[] : memref<index>
      %c1_20 = arith.constant 1 : index
      %37 = arith.muli %c1_20, %7 : index
      %38 = memref.load %alloca_18[] : memref<index>
      %39 = arith.addi %38, %37 : index
      memref.store %39, %alloca_18[] : memref<index>
      %40 = memref.load %alloca_18[] : memref<index>
      %41 = arith.index_cast %40 : index to i32
      %alloca_21 = memref.alloca() : memref<index>
      %c3 = arith.constant 3 : index
      memref.store %c3, %alloca_21[] : memref<index>
      %42 = memref.load %alloca_21[] : memref<index>
      %43 = arith.index_cast %42 : index to i32
      %alloca_22 = memref.alloca(%42) : memref<?xi64>
      %c0_23 = arith.constant 0 : index
      %44 = arith.index_cast %7 : index to i64
      memref.store %44, %alloca_22[%c0_23] : memref<?xi64>
      %c1_24 = arith.constant 1 : index
      %45 = arith.index_cast %7 : index to i64
      memref.store %45, %alloca_22[%c1_24] : memref<?xi64>
      %c2 = arith.constant 2 : index
      %46 = arith.index_cast %7 : index to i64
      memref.store %46, %alloca_22[%c2] : memref<?xi64>
      %47 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %48 = "polygeist.pointer2memref"(%47) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %49 = func.call @artsEdtCreateWithEpoch(%48, %31, %43, %alloca_22, %41, %30) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %alloc = memref.alloc() : memref<index>
      memref.store %32, %alloc[] : memref<index>
      %c0_25 = arith.constant 0 : index
      %c1_26 = arith.constant 1 : index
      %50 = scf.for %arg2 = %c0_25 to %7 step %c1_26 iter_args(%arg3 = %alloc) -> (memref<index>) {
        %72 = memref.load %alloca[%arg2] : memref<?xi64>
        %73 = memref.load %arg3[] : memref<index>
        %74 = arith.index_cast %73 : index to i32
        func.call @artsSignalEdt(%49, %74, %72) : (i64, i32, i64) -> ()
        %c1_30 = arith.constant 1 : index
        %75 = arith.addi %73, %c1_30 : index
        memref.store %75, %arg3[] : memref<index>
        scf.yield %arg3 : memref<index>
      }
      %alloc_27 = memref.alloc() : memref<index>
      memref.store %36, %alloc_27[] : memref<index>
      %c0_28 = arith.constant 0 : index
      %c1_29 = arith.constant 1 : index
      %51 = scf.for %arg2 = %c0_28 to %7 step %c1_29 iter_args(%arg3 = %alloc_27) -> (memref<index>) {
        %72 = memref.load %alloca_4[%arg2] : memref<?xi64>
        %73 = memref.load %arg3[] : memref<index>
        %74 = arith.index_cast %73 : index to i32
        func.call @artsSignalEdt(%49, %74, %72) : (i64, i32, i64) -> ()
        %c1_30 = arith.constant 1 : index
        %75 = arith.addi %73, %c1_30 : index
        memref.store %75, %arg3[] : memref<index>
        scf.yield %arg3 : memref<index>
      }
      %52 = func.call @artsWaitOnHandle(%30) : (i64) -> i1
      %53 = llvm.mlir.addressof @str6 : !llvm.ptr
      %54 = llvm.getelementptr %53[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %55 = llvm.call @printf(%54) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %56 = llvm.mlir.addressof @str7 : !llvm.ptr
      %57 = llvm.getelementptr %56[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
      %58 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %59 = arith.index_cast %c0 : index to i64
      %60 = llvm.getelementptr %58[%59] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %61 = llvm.load %60 : !llvm.ptr -> !llvm.ptr
      %62 = llvm.load %61 : !llvm.ptr -> i32
      %63 = "polygeist.memref2pointer"(%alloca_0) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %64 = arith.index_cast %c0 : index to i64
      %65 = llvm.getelementptr %63[%64] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
      %66 = llvm.load %65 : !llvm.ptr -> !llvm.ptr
      %67 = llvm.load %66 : !llvm.ptr -> i32
      %68 = llvm.call @printf(%57, %62, %67) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %72 = arith.index_cast %arg2 : index to i32
        %73 = llvm.mlir.addressof @str8 : !llvm.ptr
        %74 = llvm.getelementptr %73[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %75 = "polygeist.memref2pointer"(%alloca_5) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %76 = arith.index_cast %arg2 : index to i64
        %77 = llvm.getelementptr %75[%76] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
        %78 = llvm.load %77 : !llvm.ptr -> !llvm.ptr
        %79 = llvm.load %78 : !llvm.ptr -> i32
        %80 = "polygeist.memref2pointer"(%alloca_0) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
        %81 = arith.index_cast %arg2 : index to i64
        %82 = llvm.getelementptr %80[%81] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
        %83 = llvm.load %82 : !llvm.ptr -> !llvm.ptr
        %84 = llvm.load %83 : !llvm.ptr -> i32
        %85 = llvm.call @printf(%74, %72, %79, %72, %84) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %69 = llvm.mlir.addressof @str9 : !llvm.ptr
      %70 = llvm.getelementptr %69[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %71 = llvm.call @printf(%70) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
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
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i1 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.nounwind, llvm.readnone}
  llvm.mlir.global internal constant @str9("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("A[%d] = %d, B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("A[0] = %d, B[0] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("Final arrays:\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Task %d - 2: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task %d - 1: Computing B[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task %d - 0: Initializing A[%d] = %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Initializing arrays A and B with size %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.mlir.global internal constant @str0("Usage: %s N\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32
  func.func private @atoi(memref<?xi8>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    return
  }
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c24 = arith.constant 24 : index
    %c1_i32 = arith.constant 1 : i32
    %c2 = arith.constant 2 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c2] : memref<?xi64>
    %1 = arith.index_cast %0 : i64 to index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %alloca_0 = memref.alloca(%1) : memref<?xi64>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %5 = memref.load %alloca[] : memref<index>
      %6 = arith.muli %5, %c24 : index
      %7 = arith.index_cast %6 : index to i32
      %8 = llvm.getelementptr %2[%7] : (!llvm.ptr, i32) -> !llvm.ptr, i8
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %10 = llvm.load %9 : !llvm.ptr -> i64
      memref.store %10, %alloca_0[%arg4] : memref<?xi64>
      %11 = arith.addi %5, %c1 : index
      memref.store %11, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca(%1) : memref<?xi64>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %5 = memref.load %alloca[] : memref<index>
      %6 = arith.muli %5, %c24 : index
      %7 = arith.index_cast %6 : index to i32
      %8 = llvm.getelementptr %2[%7] : (!llvm.ptr, i32) -> !llvm.ptr, i8
      %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
      %10 = llvm.load %9 : !llvm.ptr -> i64
      memref.store %10, %alloca_1[%arg4] : memref<?xi64>
      %11 = arith.addi %5, %c1 : index
      memref.store %11, %alloca[] : memref<index>
    }
    %3 = call @artsGetCurrentNode() : () -> i32
    %alloca_2 = memref.alloca(%1) : memref<?xi64>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %5 = func.call @artsEventCreate(%3, %c1_i32) : (i32, i32) -> i64
      memref.store %5, %alloca_2[%arg4] : memref<?xi64>
    }
    %4 = call @artsGetCurrentNode() : () -> i32
    %alloca_3 = memref.alloca(%1) : memref<?xi64>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %5 = func.call @artsEventCreate(%4, %c1_i32) : (i32, i32) -> i64
      memref.store %5, %alloca_3[%arg4] : memref<?xi64>
    }
    %alloca_4 = memref.alloca() : memref<index>
    %alloca_5 = memref.alloca() : memref<index>
    %alloca_6 = memref.alloca() : memref<index>
    %alloca_7 = memref.alloca() : memref<index>
    %alloca_8 = memref.alloca() : memref<index>
    %alloca_9 = memref.alloca() : memref<index>
    %alloca_10 = memref.alloca() : memref<index>
    %alloca_11 = memref.alloca() : memref<index>
    %alloca_12 = memref.alloca() : memref<index>
    %alloca_13 = memref.alloca() : memref<index>
    scf.for %arg4 = %c0 to %1 step %c1 {
      %5 = arith.index_cast %arg4 : index to i32
      %6 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %7 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0, %alloca_4[] : memref<index>
      memref.store %c0, %alloca_5[] : memref<index>
      %8 = memref.load %alloca_5[] : memref<index>
      %9 = arith.addi %8, %c1 : index
      memref.store %9, %alloca_5[] : memref<index>
      %10 = memref.load %alloca_4[] : memref<index>
      %11 = arith.addi %10, %c1 : index
      memref.store %11, %alloca_4[] : memref<index>
      %12 = memref.load %alloca_5[] : memref<index>
      %13 = arith.index_cast %12 : index to i32
      memref.store %c1, %alloca_6[] : memref<index>
      %14 = memref.load %alloca_6[] : memref<index>
      %15 = memref.load %alloca_4[] : memref<index>
      %16 = arith.addi %14, %15 : index
      memref.store %16, %alloca_6[] : memref<index>
      %17 = memref.load %alloca_6[] : memref<index>
      %18 = arith.index_cast %17 : index to i32
      %alloca_14 = memref.alloca(%17) : memref<?xi64>
      %19 = arith.extsi %5 : i32 to i64
      memref.store %19, %alloca_14[%c0] : memref<?xi64>
      memref.store %c1, %alloca_7[] : memref<index>
      %20 = memref.load %alloca_3[%arg4] : memref<?xi64>
      %21 = memref.load %alloca_7[] : memref<index>
      memref.store %20, %alloca_14[%21] : memref<?xi64>
      %22 = arith.addi %21, %c1 : index
      memref.store %22, %alloca_7[] : memref<index>
      %23 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %25 = func.call @artsEdtCreateWithEpoch(%24, %7, %18, %alloca_14, %13, %6) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %26 = memref.load %alloca_1[%arg4] : memref<?xi64>
      %27 = arith.index_cast %8 : index to i32
      func.call @artsSignalEdt(%25, %27, %26) : (i64, i32, i64) -> ()
      %28 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %29 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0, %alloca_8[] : memref<index>
      memref.store %c0, %alloca_9[] : memref<index>
      %30 = memref.load %alloca_9[] : memref<index>
      %31 = arith.addi %30, %c1 : index
      memref.store %31, %alloca_9[] : memref<index>
      %32 = memref.load %alloca_8[] : memref<index>
      %33 = arith.addi %32, %c1 : index
      memref.store %33, %alloca_8[] : memref<index>
      %34 = memref.load %alloca_9[] : memref<index>
      %35 = arith.addi %34, %c1 : index
      memref.store %35, %alloca_9[] : memref<index>
      %36 = memref.load %alloca_9[] : memref<index>
      %37 = arith.index_cast %36 : index to i32
      memref.store %c1, %alloca_10[] : memref<index>
      %38 = memref.load %alloca_10[] : memref<index>
      %39 = memref.load %alloca_8[] : memref<index>
      %40 = arith.addi %38, %39 : index
      memref.store %40, %alloca_10[] : memref<index>
      %41 = memref.load %alloca_10[] : memref<index>
      %42 = arith.index_cast %41 : index to i32
      %alloca_15 = memref.alloca(%41) : memref<?xi64>
      memref.store %19, %alloca_15[%c0] : memref<?xi64>
      memref.store %c1, %alloca_11[] : memref<index>
      %43 = memref.load %alloca_2[%arg4] : memref<?xi64>
      %44 = memref.load %alloca_11[] : memref<index>
      memref.store %43, %alloca_15[%44] : memref<?xi64>
      %45 = arith.addi %44, %c1 : index
      memref.store %45, %alloca_11[] : memref<index>
      %46 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %47 = "polygeist.pointer2memref"(%46) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %48 = func.call @artsEdtCreateWithEpoch(%47, %29, %42, %alloca_15, %37, %28) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %49 = memref.load %alloca_3[%arg4] : memref<?xi64>
      %50 = arith.index_cast %34 : index to i32
      func.call @artsAddDependence(%49, %48, %50) : (i64, i64, i32) -> ()
      %51 = memref.load %alloca_0[%arg4] : memref<?xi64>
      %52 = arith.index_cast %30 : index to i32
      func.call @artsSignalEdt(%48, %52, %51) : (i64, i32, i64) -> ()
      %53 = arith.addi %5, %c-1_i32 : i32
      %54 = arith.index_cast %53 : i32 to index
      %55 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %56 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0, %alloca_12[] : memref<index>
      %57 = memref.load %alloca_12[] : memref<index>
      %58 = arith.addi %57, %c1 : index
      memref.store %58, %alloca_12[] : memref<index>
      %59 = memref.load %alloca_12[] : memref<index>
      %60 = arith.addi %59, %c1 : index
      memref.store %60, %alloca_12[] : memref<index>
      %61 = memref.load %alloca_12[] : memref<index>
      %62 = arith.addi %61, %c1 : index
      memref.store %62, %alloca_12[] : memref<index>
      %63 = memref.load %alloca_12[] : memref<index>
      %64 = arith.index_cast %63 : index to i32
      memref.store %c1, %alloca_13[] : memref<index>
      %65 = memref.load %alloca_13[] : memref<index>
      %66 = arith.index_cast %65 : index to i32
      %alloca_16 = memref.alloca(%65) : memref<?xi64>
      memref.store %19, %alloca_16[%c0] : memref<?xi64>
      %67 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %68 = "polygeist.pointer2memref"(%67) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %69 = func.call @artsEdtCreateWithEpoch(%68, %56, %66, %alloca_16, %64, %55) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %70 = memref.load %alloca_2[%54] : memref<?xi64>
      %71 = arith.index_cast %57 : index to i32
      func.call @artsAddDependence(%70, %69, %71) : (i64, i64, i32) -> ()
      %72 = memref.load %alloca_2[%arg4] : memref<?xi64>
      %73 = arith.index_cast %59 : index to i32
      func.call @artsAddDependence(%72, %69, %73) : (i64, i64, i32) -> ()
      %74 = memref.load %alloca_3[%arg4] : memref<?xi64>
      %75 = arith.index_cast %61 : index to i32
      func.call @artsAddDependence(%74, %69, %75) : (i64, i64, i32) -> ()
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> i64
    %5 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %6 = llvm.load %5 : !llvm.ptr -> memref<?xi8>
    %7 = "polygeist.memref2pointer"(%6) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %1, %7 : i32, !llvm.ptr
    %8 = llvm.mlir.addressof @str3 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
    %10 = llvm.call @printf(%9, %1, %1, %1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    %11 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %12 = llvm.getelementptr %11[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %13 = llvm.load %12 : !llvm.ptr -> i64
    call @artsEventSatisfySlot(%13, %4, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0_i32 = arith.constant 0 : i32
    %c0 = arith.constant 0 : index
    %c5_i32 = arith.constant 5 : i32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> i64
    %5 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %6 = llvm.load %5 : !llvm.ptr -> memref<?xi8>
    %7 = llvm.getelementptr %2[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %8 = llvm.getelementptr %7[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %9 = llvm.load %8 : !llvm.ptr -> memref<?xi8>
    %10 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %11 = llvm.load %10 : !llvm.ptr -> i32
    %12 = arith.addi %11, %c5_i32 : i32
    %13 = "polygeist.memref2pointer"(%6) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %12, %13 : i32, !llvm.ptr
    %14 = llvm.mlir.addressof @str4 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %16 = llvm.call @printf(%15, %1, %1, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    %17 = "polygeist.memref2pointer"(%arg1) : (memref<?xi64>) -> !llvm.ptr
    %18 = llvm.getelementptr %17[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %19 = llvm.load %18 : !llvm.ptr -> i64
    call @artsEventSatisfySlot(%19, %4, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c5_i32 = arith.constant 5 : i32
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, ptr)>>) -> !llvm.ptr
    %3 = llvm.getelementptr %2[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %4 = llvm.load %3 : !llvm.ptr -> memref<?xi8>
    %5 = llvm.getelementptr %2[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> memref<?xi8>
    %8 = llvm.getelementptr %2[48] : (!llvm.ptr) -> !llvm.ptr, i8
    %9 = llvm.getelementptr %8[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %10 = llvm.load %9 : !llvm.ptr -> memref<?xi8>
    %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    %12 = llvm.load %11 : !llvm.ptr -> i32
    %13 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    %14 = llvm.load %13 : !llvm.ptr -> i32
    %15 = arith.addi %12, %14 : i32
    %16 = arith.addi %15, %c5_i32 : i32
    %17 = "polygeist.memref2pointer"(%7) : (memref<?xi8>) -> !llvm.ptr
    llvm.store %16, %17 : i32, !llvm.ptr
    %18 = llvm.mlir.addressof @str5 : !llvm.ptr
    %19 = llvm.getelementptr %18[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %20 = llvm.call @printf(%19, %1, %1, %16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
    return
  }
  func.func @mainBody(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c3_i32 = arith.constant 3 : i32
    %c4_i64 = arith.constant 4 : i64
    %c2 = arith.constant 2 : index
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
      %alloca = memref.alloca(%7) : memref<?xi64>
      %alloca_0 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      scf.for %arg2 = %c0 to %7 step %c1 {
        %48 = func.call @artsReserveGuidRoute(%c9_i32, %18) : (i32, i32) -> i64
        %49 = func.call @artsDbCreateWithGuid(%48, %c4_i64) : (i64, i64) -> memref<?xi8>
        memref.store %48, %alloca[%arg2] : memref<?xi64>
        memref.store %49, %alloca_0[%arg2] : memref<?xmemref<?xi8>>
      }
      %19 = func.call @artsGetCurrentNode() : () -> i32
      %alloca_1 = memref.alloca(%7) : memref<?xi64>
      %alloca_2 = memref.alloca(%7) : memref<?xmemref<?xi8>>
      scf.for %arg2 = %c0 to %7 step %c1 {
        %48 = func.call @artsReserveGuidRoute(%c9_i32, %19) : (i32, i32) -> i64
        %49 = func.call @artsDbCreateWithGuid(%48, %c4_i64) : (i64, i64) -> memref<?xi8>
        memref.store %48, %alloca_1[%arg2] : memref<?xi64>
        memref.store %49, %alloca_2[%arg2] : memref<?xmemref<?xi8>>
      }
      %20 = func.call @artsGetCurrentNode() : () -> i32
      %alloca_3 = memref.alloca() : memref<0xi64>
      %cast = memref.cast %alloca_3 : memref<0xi64> to memref<?xi64>
      %21 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %22 = "polygeist.pointer2memref"(%21) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %23 = func.call @artsEdtCreate(%22, %20, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
      %24 = func.call @artsInitializeAndStartEpoch(%23, %c0_i32) : (i64, i32) -> i64
      %25 = func.call @artsGetCurrentNode() : () -> i32
      %26 = arith.addi %7, %7 : index
      %27 = arith.index_cast %26 : index to i32
      %alloca_4 = memref.alloca() : memref<3xi64>
      %cast_5 = memref.cast %alloca_4 : memref<3xi64> to memref<?xi64>
      %28 = arith.index_cast %7 : index to i64
      memref.store %28, %alloca_4[%c0] : memref<3xi64>
      memref.store %28, %alloca_4[%c1] : memref<3xi64>
      memref.store %28, %alloca_4[%c2] : memref<3xi64>
      %29 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %31 = func.call @artsEdtCreateWithEpoch(%30, %25, %c3_i32, %cast_5, %27, %24) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %alloc = memref.alloc() : memref<index>
      memref.store %c0, %alloc[] : memref<index>
      scf.for %arg2 = %c0 to %7 step %c1 {
        %48 = memref.load %alloca[%arg2] : memref<?xi64>
        %49 = memref.load %alloc[] : memref<index>
        %50 = arith.index_cast %49 : index to i32
        func.call @artsSignalEdt(%31, %50, %48) : (i64, i32, i64) -> ()
        %51 = arith.addi %49, %c1 : index
        memref.store %51, %alloc[] : memref<index>
      }
      %alloc_6 = memref.alloc() : memref<index>
      memref.store %7, %alloc_6[] : memref<index>
      scf.for %arg2 = %c0 to %7 step %c1 {
        %48 = memref.load %alloca_1[%arg2] : memref<?xi64>
        %49 = memref.load %alloc_6[] : memref<index>
        %50 = arith.index_cast %49 : index to i32
        func.call @artsSignalEdt(%31, %50, %48) : (i64, i32, i64) -> ()
        %51 = arith.addi %49, %c1 : index
        memref.store %51, %alloc_6[] : memref<index>
      }
      %32 = func.call @artsWaitOnHandle(%24) : (i64) -> i1
      %33 = llvm.mlir.addressof @str6 : !llvm.ptr
      %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<15 x i8>
      %35 = llvm.call @printf(%34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      %36 = llvm.mlir.addressof @str7 : !llvm.ptr
      %37 = llvm.getelementptr %36[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
      %38 = "polygeist.memref2pointer"(%alloca_2) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %39 = llvm.load %38 : !llvm.ptr -> !llvm.ptr
      %40 = llvm.load %39 : !llvm.ptr -> i32
      %41 = "polygeist.memref2pointer"(%alloca_0) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %42 = llvm.load %41 : !llvm.ptr -> !llvm.ptr
      %43 = llvm.load %42 : !llvm.ptr -> i32
      %44 = llvm.call @printf(%37, %40, %43) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      scf.for %arg2 = %c0 to %7 step %c1 {
        %48 = arith.index_cast %arg2 : index to i32
        %49 = llvm.mlir.addressof @str8 : !llvm.ptr
        %50 = llvm.getelementptr %49[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %51 = arith.index_cast %arg2 : index to i64
        %52 = llvm.getelementptr %38[%51] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
        %53 = llvm.load %52 : !llvm.ptr -> !llvm.ptr
        %54 = llvm.load %53 : !llvm.ptr -> i32
        %55 = llvm.getelementptr %41[%51] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
        %56 = llvm.load %55 : !llvm.ptr -> !llvm.ptr
        %57 = llvm.load %56 : !llvm.ptr -> i32
        %58 = llvm.call @printf(%50, %48, %54, %48, %57) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
      }
      %45 = llvm.mlir.addressof @str9 : !llvm.ptr
      %46 = llvm.getelementptr %45[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
      %47 = llvm.call @printf(%46) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
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

