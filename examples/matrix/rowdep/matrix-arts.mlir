
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
[create-datablocks] Candidate datablocks in function: compute
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: out
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %22, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: in
    Pinned Indices:
      - %c0 = arith.constant 0 : index
    Uses:
    - %17 = memref.load %alloca_0[%c0, %arg0] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<100x100xf64>
    Access Type: out
    Pinned Indices:
      - %c0 = arith.constant 0 : index
    Uses:
    - memref.store %17, %alloca[%c0, %arg0] : memref<100x100xf64>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: in
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - %29 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<100x100xf64>
    Access Type: out
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %31, %alloca[%arg0, %arg1] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: in
    Pinned Indices:
      - %22 = arith.index_cast %21 : i32 to index
    Uses:
    - %30 = memref.load %alloca_0[%22, %arg1] : memref<100x100xf64>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<100x100xf64>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - memref.store %31, %alloca[%arg0, %arg1] : memref<100x100xf64>
    - %26 = memref.load %alloca[%arg0, %c0] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - %30 = memref.load %alloca_0[%22, %arg1] : memref<100x100xf64>
    - %29 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
    - %23 = memref.load %alloca_0[%22, %c0] : memref<100x100xf64>
    - %18 = memref.load %alloca_0[%arg0, %c0] : memref<100x100xf64>
    - memref.store %22, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
    - %18 = memref.load %alloca_0[%arg0, %c0] : memref<100x100xf64>
[create-datablocks] Candidate datablocks in function: rand
-----------------------------------------
Rewiring uses of:
  %22 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0], sizes[%c100_1], type[f64], typeSize[%21] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %18 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%c0], sizes[%c100_1], type[f64], typeSize[%17] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %20 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%c0], sizes[%c100_2], type[f64], typeSize[%19] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %32 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0], sizes[%c100_3], type[f64], typeSize[%31] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %34 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%arg0], sizes[%c100_4], type[f64], typeSize[%33] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %36 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%24], sizes[%c100_5], type[f64], typeSize[%35] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %3 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100_1, %c100_2], type[f64], typeSize[%2] -> memref<100x100xf64>
-----------------------------------------
Rewiring uses of:
  %5 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100_3, %c100_4], type[f64], typeSize[%4] -> memref<100x100xf64>
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
Events:
  Event
    Producer: 2, Consumer: 3
    Producer: 2, Consumer: 5
    Producer: 2, Consumer: 7
[create-events] Processing grouped event
-----------------------------------------
CreateEventsPass FINISHED
-----------------------------------------
-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    arts.edt dependencies(%2, %3) : (memref<100x100xf64>, memref<100x100xf64>) attributes {sync} {
      %10 = arts.event[%c100, %c100] -> : memref<100x100xi64>
      %11 = arith.sitofp %1 : i32 to f64
      scf.for %arg0 = %c0 to %c100 step %c1 {
        %14 = arith.index_cast %arg0 : index to i32
        %15 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
        arts.edt dependencies(%15) : (memref<100xf64>) attributes {task} {
          %16 = arith.sitofp %14 : i32 to f64
          %17 = arith.addf %16, %11 : f64
          scf.for %arg1 = %c0 to %c100 step %c1 {
            memref.store %17, %15[%arg1] : memref<100xf64>
          }
          arts.yield
        }
      }
      %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
      %13 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
      arts.edt dependencies(%12, %13) : (memref<100xf64>, memref<100xf64>) attributes {task} {
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %14 = memref.load %12[%arg0] : memref<100xf64>
          memref.store %14, %13[%arg0] : memref<100xf64>
        }
        arts.yield
      }
      scf.for %arg0 = %c1 to %c100 step %c1 {
        %14 = arith.index_cast %arg0 : index to i32
        %15 = arith.addi %14, %c-1_i32 : i32
        %16 = arith.index_cast %15 : i32 to index
        %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
        %18 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
        %19 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%16], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
        arts.edt dependencies(%17, %18, %19) : (memref<100xf64>, memref<100xf64>, memref<100xf64>) attributes {task} {
          scf.for %arg1 = %c0 to %c100 step %c1 {
            %20 = memref.load %17[%arg1] : memref<100xf64>
            %21 = memref.load %19[%arg1] : memref<100xf64>
            %22 = arith.addf %20, %21 : f64
            memref.store %22, %18[%arg1] : memref<100xf64>
          }
          arts.yield
        }
      }
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %3[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%5, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %8 = llvm.mlir.addressof @str2 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %2[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%9, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-llvm] Iterate over all the functions
[convert-arts-to-llvm] Lowering arts.datablock
Creating Datablock: %alloca_1 = memref.alloca(%c100, %c100) : memref<?x?xi64>
[convert-arts-to-llvm] Lowering arts.datablock
Creating Datablock: %alloca_6 = memref.alloca(%c100, %c100) : memref<?x?xi64>
[convert-arts-to-llvm] Lowering arts.edt sync
-----------------------------------------
[convert-arts-to-llvm] Sync done EDT created
[convert-arts-to-llvm] Parallel epoch created
[convert-arts-to-llvm] Parallel EDT created
[convert-arts-to-llvm] Parallel op lowered

[convert-arts-to-llvm] Lowering arts.event
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt
Processing dependencies to satisfy
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt
-----------------------------------------
ConvertArtsToLLVMPass FINISHED 
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = call @artsGetCurrentNode() : () -> i32
    %c9_i32 = arith.constant 9 : i32
    %alloca = memref.alloca(%c100, %c100) : memref<?x?xi64>
    %alloca_0 = memref.alloca(%c100, %c100) : memref<?x?xmemref<?xi8>>
    %c0_1 = arith.constant 0 : index
    %c1_2 = arith.constant 1 : index
    scf.for %arg0 = %c0_1 to %c100 step %c1_2 {
      %c0_24 = arith.constant 0 : index
      %c1_25 = arith.constant 1 : index
      scf.for %arg1 = %c0_24 to %c100 step %c1_25 {
        %45 = func.call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
        %46 = arith.index_cast %c8 : index to i64
        %47 = func.call @artsDbCreateWithGuid(%45, %46) : (i64, i64) -> memref<?xi8>
        memref.store %45, %alloca[%arg0, %arg1] : memref<?x?xi64>
        memref.store %47, %alloca_0[%arg0, %arg1] : memref<?x?xmemref<?xi8>>
      }
    }
    %3 = call @artsGetCurrentNode() : () -> i32
    %c9_i32_3 = arith.constant 9 : i32
    %alloca_4 = memref.alloca(%c100, %c100) : memref<?x?xi64>
    %alloca_5 = memref.alloca(%c100, %c100) : memref<?x?xmemref<?xi8>>
    %c0_6 = arith.constant 0 : index
    %c1_7 = arith.constant 1 : index
    scf.for %arg0 = %c0_6 to %c100 step %c1_7 {
      %c0_24 = arith.constant 0 : index
      %c1_25 = arith.constant 1 : index
      scf.for %arg1 = %c0_24 to %c100 step %c1_25 {
        %45 = func.call @artsReserveGuidRoute(%c9_i32_3, %3) : (i32, i32) -> i64
        %46 = arith.index_cast %c8 : index to i64
        %47 = func.call @artsDbCreateWithGuid(%45, %46) : (i64, i64) -> memref<?xi8>
        memref.store %45, %alloca_4[%arg0, %arg1] : memref<?x?xi64>
        memref.store %47, %alloca_5[%arg0, %arg1] : memref<?x?xmemref<?xi8>>
      }
    }
    %4 = call @artsGetCurrentNode() : () -> i32
    %c0_i32 = arith.constant 0 : i32
    %alloca_8 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_8[] : memref<i32>
    %alloca_9 = memref.alloca() : memref<i32>
    %c0_i32_10 = arith.constant 0 : i32
    memref.store %c0_i32_10, %alloca_9[] : memref<i32>
    %5 = memref.load %alloca_9[] : memref<i32>
    %6 = arith.index_cast %5 : i32 to index
    %alloca_11 = memref.alloca(%6) : memref<?xi64>
    %c1_i32 = arith.constant 1 : i32
    %7 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %9 = call @artsEdtCreate(%8, %4, %5, %alloca_11, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_12 = arith.constant 0 : i32
    %10 = call @artsInitializeAndStartEpoch(%9, %c0_i32_12) : (i64, i32) -> i64
    %11 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_13 = arith.constant 0 : i32
    %alloca_14 = memref.alloca() : memref<i32>
    memref.store %c0_i32_13, %alloca_14[] : memref<i32>
    %alloca_15 = memref.alloca() : memref<i32>
    memref.store %c0_i32_13, %alloca_15[] : memref<i32>
    %12 = memref.load %alloca_15[] : memref<i32>
    %alloca_16 = memref.alloca() : memref<i32>
    %c1_i32_17 = arith.constant 1 : i32
    memref.store %c1_i32_17, %alloca_16[] : memref<i32>
    %13 = memref.load %alloca_16[] : memref<i32>
    %14 = arith.index_cast %c100 : index to i32
    %15 = arith.muli %13, %14 : i32
    memref.store %15, %alloca_16[] : memref<i32>
    %16 = memref.load %alloca_16[] : memref<i32>
    %17 = arith.index_cast %c100 : index to i32
    %18 = arith.muli %16, %17 : i32
    memref.store %18, %alloca_16[] : memref<i32>
    %19 = memref.load %alloca_16[] : memref<i32>
    %20 = memref.load %alloca_15[] : memref<i32>
    %21 = arith.addi %20, %19 : i32
    memref.store %21, %alloca_15[] : memref<i32>
    %22 = memref.load %alloca_15[] : memref<i32>
    %alloca_18 = memref.alloca() : memref<i32>
    %c1_i32_19 = arith.constant 1 : i32
    memref.store %c1_i32_19, %alloca_18[] : memref<i32>
    %23 = memref.load %alloca_18[] : memref<i32>
    %24 = arith.index_cast %c100 : index to i32
    %25 = arith.muli %23, %24 : i32
    memref.store %25, %alloca_18[] : memref<i32>
    %26 = memref.load %alloca_18[] : memref<i32>
    %27 = arith.index_cast %c100 : index to i32
    %28 = arith.muli %26, %27 : i32
    memref.store %28, %alloca_18[] : memref<i32>
    %29 = memref.load %alloca_18[] : memref<i32>
    %30 = memref.load %alloca_15[] : memref<i32>
    %31 = arith.addi %30, %29 : i32
    memref.store %31, %alloca_15[] : memref<i32>
    %32 = memref.load %alloca_15[] : memref<i32>
    %alloca_20 = memref.alloca() : memref<i32>
    %c1_i32_21 = arith.constant 1 : i32
    memref.store %c1_i32_21, %alloca_20[] : memref<i32>
    %33 = memref.load %alloca_20[] : memref<i32>
    %34 = arith.index_cast %33 : i32 to index
    %alloca_22 = memref.alloca(%34) : memref<?xi64>
    %35 = arith.extsi %1 : i32 to i64
    %c0_23 = arith.constant 0 : index
    affine.store %35, %alloca_22[%c0_23] : memref<?xi64>
    %36 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %37 = "polygeist.pointer2memref"(%36) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %38 = call @artsEdtCreateWithEpoch(%37, %11, %33, %alloca_22, %32, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %39 = llvm.mlir.addressof @str0 : !llvm.ptr
    %40 = llvm.getelementptr %39[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %41 = llvm.mlir.addressof @str1 : !llvm.ptr
    %42 = llvm.getelementptr %41[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %45 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %47 = arith.index_cast %arg1 : index to i32
        %48 = arith.index_cast %arg0 : index to i64
        %49 = arith.index_cast %arg1 : index to i64
      }
      %46 = llvm.call @printf(%42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %43 = llvm.mlir.addressof @str2 : !llvm.ptr
    %44 = llvm.getelementptr %43[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %45 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %47 = arith.index_cast %arg1 : index to i32
        %48 = arith.index_cast %arg0 : index to i64
        %49 = arith.index_cast %arg1 : index to i64
      }
      %46 = llvm.call @printf(%42) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c100 = arith.constant 100 : index
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    %c0_1 = arith.constant 0 : index
    memref.store %c0_1, %alloca[] : memref<index>
    %c1_2 = arith.constant 1 : index
    %c100_3 = arith.constant 100 : index
    %c100_4 = arith.constant 100 : index
    %alloca_5 = memref.alloca(%c100_3, %c100_4) : memref<?x?xi64>
    %c0_6 = arith.constant 0 : index
    scf.for %arg4 = %c0_6 to %c100_3 step %c1_2 {
      %c0_22 = arith.constant 0 : index
      scf.for %arg5 = %c0_22 to %c100_4 step %c1_2 {
        %26 = memref.load %alloca[] : memref<index>
        %27 = memref.load %arg3[%26] : memref<?x!llvm.struct<(i64, i32, ptr)>>
        %28 = llvm.extractvalue %27[0] : !llvm.struct<(i64, i32, ptr)> 
        %29 = llvm.extractvalue %27[2] : !llvm.struct<(i64, i32, ptr)> 
        %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr) -> memref<?xi8>
        memref.store %28, %alloca_5[%arg4, %arg5] : memref<?x?xi64>
        %31 = arith.addi %26, %c1_2 : index
        memref.store %31, %alloca[] : memref<index>
      }
    }
    %c100_7 = arith.constant 100 : index
    %c100_8 = arith.constant 100 : index
    %alloca_9 = memref.alloca(%c100_7, %c100_8) : memref<?x?xi64>
    %c0_10 = arith.constant 0 : index
    scf.for %arg4 = %c0_10 to %c100_7 step %c1_2 {
      %c0_22 = arith.constant 0 : index
      scf.for %arg5 = %c0_22 to %c100_8 step %c1_2 {
        %26 = memref.load %alloca[] : memref<index>
        %27 = memref.load %arg3[%26] : memref<?x!llvm.struct<(i64, i32, ptr)>>
        %28 = llvm.extractvalue %27[0] : !llvm.struct<(i64, i32, ptr)> 
        %29 = llvm.extractvalue %27[2] : !llvm.struct<(i64, i32, ptr)> 
        %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr) -> memref<?xi8>
        memref.store %28, %alloca_9[%arg4, %arg5] : memref<?x?xi64>
        %31 = arith.addi %26, %c1_2 : index
        memref.store %31, %alloca[] : memref<index>
      }
    }
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca_11 = memref.alloca(%c100, %c100) : memref<?x?xi64>
    %c0_12 = arith.constant 0 : index
    %c1_13 = arith.constant 1 : index
    scf.for %arg4 = %c0_12 to %c100 step %c1_13 {
      %c0_22 = arith.constant 0 : index
      %c1_23 = arith.constant 1 : index
      scf.for %arg5 = %c0_22 to %c100 step %c1_23 {
        %c1_i32_24 = arith.constant 1 : i32
        %26 = func.call @artsEventCreate(%2, %c1_i32_24) : (i32, i32) -> i64
        memref.store %26, %alloca_11[%arg4, %arg5] : memref<?x?xi64>
      }
    }
    %3 = arith.sitofp %1 : i32 to f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %26 = arith.index_cast %arg4 : index to i32
      %27 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %28 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32_22 = arith.constant 0 : i32
      %alloca_23 = memref.alloca() : memref<i32>
      memref.store %c0_i32_22, %alloca_23[] : memref<i32>
      %alloca_24 = memref.alloca() : memref<i32>
      memref.store %c0_i32_22, %alloca_24[] : memref<i32>
      %29 = memref.load %alloca_24[] : memref<i32>
      %alloca_25 = memref.alloca() : memref<i32>
      %c1_i32_26 = arith.constant 1 : i32
      memref.store %c1_i32_26, %alloca_25[] : memref<i32>
      %30 = memref.load %alloca_25[] : memref<i32>
      %31 = arith.index_cast %c100 : index to i32
      %32 = arith.muli %30, %31 : i32
      memref.store %32, %alloca_25[] : memref<i32>
      %33 = memref.load %alloca_25[] : memref<i32>
      %34 = memref.load %alloca_24[] : memref<i32>
      %35 = arith.addi %34, %33 : i32
      memref.store %35, %alloca_24[] : memref<i32>
      %36 = memref.load %alloca_23[] : memref<i32>
      %37 = arith.addi %36, %33 : i32
      memref.store %37, %alloca_23[] : memref<i32>
      %38 = memref.load %alloca_24[] : memref<i32>
      %alloca_27 = memref.alloca() : memref<i32>
      %c2_i32 = arith.constant 2 : i32
      memref.store %c2_i32, %alloca_27[] : memref<i32>
      %39 = memref.load %alloca_27[] : memref<i32>
      %40 = memref.load %alloca_23[] : memref<i32>
      %41 = arith.addi %39, %40 : i32
      memref.store %41, %alloca_27[] : memref<i32>
      %42 = memref.load %alloca_27[] : memref<i32>
      %43 = arith.index_cast %42 : i32 to index
      %alloca_28 = memref.alloca(%43) : memref<?xi64>
      %44 = arith.extsi %26 : i32 to i64
      %c0_29 = arith.constant 0 : index
      affine.store %44, %alloca_28[%c0_29] : memref<?xi64>
      %45 = arith.fptosi %3 : f64 to i64
      %c1_30 = arith.constant 1 : index
      affine.store %45, %alloca_28[%c1_30] : memref<?xi64>
      %alloca_31 = memref.alloca() : memref<i32>
      %c2_i32_32 = arith.constant 2 : i32
      memref.store %c2_i32_32, %alloca_31[] : memref<i32>
      %c0_33 = arith.constant 0 : index
      %c1_34 = arith.constant 1 : index
      scf.for %arg5 = %c0_33 to %c100 step %c1_34 {
        %49 = memref.load %alloca_11[%arg4, %arg5] : memref<?x?xi64>
        %50 = memref.load %alloca_31[] : memref<i32>
        %51 = arith.index_cast %50 : i32 to index
        memref.store %49, %alloca_28[%51] : memref<?xi64>
        %c1_i32_35 = arith.constant 1 : i32
        %52 = arith.addi %50, %c1_i32_35 : i32
        memref.store %52, %alloca_31[] : memref<i32>
      }
      %46 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %47 = "polygeist.pointer2memref"(%46) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %48 = func.call @artsEdtCreateWithEpoch(%47, %28, %42, %alloca_28, %38, %27) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = call @artsGetCurrentNode() : () -> i32
    %c0_i32 = arith.constant 0 : i32
    %alloca_14 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_14[] : memref<i32>
    %alloca_15 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_15[] : memref<i32>
    %6 = memref.load %alloca_15[] : memref<i32>
    %alloca_16 = memref.alloca() : memref<i32>
    %c1_i32 = arith.constant 1 : i32
    memref.store %c1_i32, %alloca_16[] : memref<i32>
    %7 = memref.load %alloca_16[] : memref<i32>
    %8 = arith.index_cast %c100 : index to i32
    %9 = arith.muli %7, %8 : i32
    memref.store %9, %alloca_16[] : memref<i32>
    %10 = memref.load %alloca_16[] : memref<i32>
    %11 = memref.load %alloca_15[] : memref<i32>
    %12 = arith.addi %11, %10 : i32
    memref.store %12, %alloca_15[] : memref<i32>
    %13 = memref.load %alloca_15[] : memref<i32>
    %alloca_17 = memref.alloca() : memref<i32>
    %c1_i32_18 = arith.constant 1 : i32
    memref.store %c1_i32_18, %alloca_17[] : memref<i32>
    %14 = memref.load %alloca_17[] : memref<i32>
    %15 = arith.index_cast %c100 : index to i32
    %16 = arith.muli %14, %15 : i32
    memref.store %16, %alloca_17[] : memref<i32>
    %17 = memref.load %alloca_17[] : memref<i32>
    %18 = memref.load %alloca_15[] : memref<i32>
    %19 = arith.addi %18, %17 : i32
    memref.store %19, %alloca_15[] : memref<i32>
    %20 = memref.load %alloca_15[] : memref<i32>
    %alloca_19 = memref.alloca() : memref<i32>
    %c0_i32_20 = arith.constant 0 : i32
    memref.store %c0_i32_20, %alloca_19[] : memref<i32>
    %21 = memref.load %alloca_19[] : memref<i32>
    %22 = arith.index_cast %21 : i32 to index
    %alloca_21 = memref.alloca(%22) : memref<?xi64>
    %23 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %25 = call @artsEdtCreateWithEpoch(%24, %5, %21, %alloca_21, %20, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    scf.for %arg4 = %c1 to %c100 step %c1 {
      %26 = arith.index_cast %arg4 : index to i32
      %27 = arith.addi %26, %c-1_i32 : i32
      %28 = arith.index_cast %27 : i32 to index
      %29 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %30 = func.call @artsGetCurrentNode() : () -> i32
      %c0_i32_22 = arith.constant 0 : i32
      %alloca_23 = memref.alloca() : memref<i32>
      memref.store %c0_i32_22, %alloca_23[] : memref<i32>
      %alloca_24 = memref.alloca() : memref<i32>
      memref.store %c0_i32_22, %alloca_24[] : memref<i32>
      %31 = memref.load %alloca_24[] : memref<i32>
      %alloca_25 = memref.alloca() : memref<i32>
      %c1_i32_26 = arith.constant 1 : i32
      memref.store %c1_i32_26, %alloca_25[] : memref<i32>
      %32 = memref.load %alloca_25[] : memref<i32>
      %33 = arith.index_cast %c100 : index to i32
      %34 = arith.muli %32, %33 : i32
      memref.store %34, %alloca_25[] : memref<i32>
      %35 = memref.load %alloca_25[] : memref<i32>
      %36 = memref.load %alloca_24[] : memref<i32>
      %37 = arith.addi %36, %35 : i32
      memref.store %37, %alloca_24[] : memref<i32>
      %38 = memref.load %alloca_24[] : memref<i32>
      %alloca_27 = memref.alloca() : memref<i32>
      %c1_i32_28 = arith.constant 1 : i32
      memref.store %c1_i32_28, %alloca_27[] : memref<i32>
      %39 = memref.load %alloca_27[] : memref<i32>
      %40 = arith.index_cast %c100 : index to i32
      %41 = arith.muli %39, %40 : i32
      memref.store %41, %alloca_27[] : memref<i32>
      %42 = memref.load %alloca_27[] : memref<i32>
      %43 = memref.load %alloca_24[] : memref<i32>
      %44 = arith.addi %43, %42 : i32
      memref.store %44, %alloca_24[] : memref<i32>
      %45 = memref.load %alloca_24[] : memref<i32>
      %alloca_29 = memref.alloca() : memref<i32>
      %c1_i32_30 = arith.constant 1 : i32
      memref.store %c1_i32_30, %alloca_29[] : memref<i32>
      %46 = memref.load %alloca_29[] : memref<i32>
      %47 = arith.index_cast %c100 : index to i32
      %48 = arith.muli %46, %47 : i32
      memref.store %48, %alloca_29[] : memref<i32>
      %49 = memref.load %alloca_29[] : memref<i32>
      %50 = memref.load %alloca_24[] : memref<i32>
      %51 = arith.addi %50, %49 : i32
      memref.store %51, %alloca_24[] : memref<i32>
      %52 = memref.load %alloca_24[] : memref<i32>
      %alloca_31 = memref.alloca() : memref<i32>
      %c0_i32_32 = arith.constant 0 : i32
      memref.store %c0_i32_32, %alloca_31[] : memref<i32>
      %53 = memref.load %alloca_31[] : memref<i32>
      %54 = arith.index_cast %53 : i32 to index
      %alloca_33 = memref.alloca(%54) : memref<?xi64>
      %55 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %56 = "polygeist.pointer2memref"(%55) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %57 = func.call @artsEdtCreateWithEpoch(%56, %30, %53, %alloca_33, %52, %29) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0_0 = arith.constant 0 : index
    %0 = memref.load %arg1[%c0_0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %c1_1 = arith.constant 1 : index
    %2 = memref.load %arg1[%c1_1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %alloca = memref.alloca() : memref<index>
    %c0_2 = arith.constant 0 : index
    memref.store %c0_2, %alloca[] : memref<index>
    %c1_3 = arith.constant 1 : index
    %c100_4 = arith.constant 100 : index
    %alloca_5 = memref.alloca(%c100_4) : memref<?xi64>
    %alloca_6 = memref.alloca(%c100_4) : memref<?xmemref<?xi8>>
    %c0_7 = arith.constant 0 : index
    scf.for %arg4 = %c0_7 to %c100_4 step %c1_3 {
      %6 = memref.load %alloca[] : memref<index>
      %7 = memref.load %arg3[%6] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %8 = llvm.extractvalue %7[0] : !llvm.struct<(i64, i32, ptr)> 
      %9 = llvm.extractvalue %7[2] : !llvm.struct<(i64, i32, ptr)> 
      %10 = "polygeist.pointer2memref"(%9) : (!llvm.ptr) -> memref<?xi8>
      memref.store %8, %alloca_5[%arg4] : memref<?xi64>
      memref.store %10, %alloca_6[%arg4] : memref<?xmemref<?xi8>>
      %11 = arith.addi %6, %c1_3 : index
      memref.store %11, %alloca[] : memref<index>
    }
    %4 = arith.sitofp %1 : i32 to f64
    %5 = arith.addf %4, %3 : f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %6 = "polygeist.memref2pointer"(%alloca_6) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %7 = arith.index_cast %arg4 : index to i64
      %8 = llvm.getelementptr %6[%7] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %5, %8 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    %c0_0 = arith.constant 0 : index
    memref.store %c0_0, %alloca[] : memref<index>
    %c1_1 = arith.constant 1 : index
    %c100_2 = arith.constant 100 : index
    %alloca_3 = memref.alloca(%c100_2) : memref<?xi64>
    %alloca_4 = memref.alloca(%c100_2) : memref<?xmemref<?xi8>>
    %c0_5 = arith.constant 0 : index
    scf.for %arg4 = %c0_5 to %c100_2 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_3[%arg4] : memref<?xi64>
      memref.store %4, %alloca_4[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    %c100_6 = arith.constant 100 : index
    %alloca_7 = memref.alloca(%c100_6) : memref<?xi64>
    %alloca_8 = memref.alloca(%c100_6) : memref<?xmemref<?xi8>>
    %c0_9 = arith.constant 0 : index
    scf.for %arg4 = %c0_9 to %c100_6 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_7[%arg4] : memref<?xi64>
      memref.store %4, %alloca_8[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = "polygeist.memref2pointer"(%alloca_4) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %1 = arith.index_cast %arg4 : index to i64
      %2 = llvm.getelementptr %0[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %3 = llvm.load %2 : !llvm.ptr -> f64
      %4 = "polygeist.memref2pointer"(%alloca_8) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %5 = arith.index_cast %arg4 : index to i64
      %6 = llvm.getelementptr %4[%5] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %3, %6 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    %c0_0 = arith.constant 0 : index
    memref.store %c0_0, %alloca[] : memref<index>
    %c1_1 = arith.constant 1 : index
    %c100_2 = arith.constant 100 : index
    %alloca_3 = memref.alloca(%c100_2) : memref<?xi64>
    %alloca_4 = memref.alloca(%c100_2) : memref<?xmemref<?xi8>>
    %c0_5 = arith.constant 0 : index
    scf.for %arg4 = %c0_5 to %c100_2 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_3[%arg4] : memref<?xi64>
      memref.store %4, %alloca_4[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    %c100_6 = arith.constant 100 : index
    %alloca_7 = memref.alloca(%c100_6) : memref<?xi64>
    %alloca_8 = memref.alloca(%c100_6) : memref<?xmemref<?xi8>>
    %c0_9 = arith.constant 0 : index
    scf.for %arg4 = %c0_9 to %c100_6 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_7[%arg4] : memref<?xi64>
      memref.store %4, %alloca_8[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    %c100_10 = arith.constant 100 : index
    %alloca_11 = memref.alloca(%c100_10) : memref<?xi64>
    %alloca_12 = memref.alloca(%c100_10) : memref<?xmemref<?xi8>>
    %c0_13 = arith.constant 0 : index
    scf.for %arg4 = %c0_13 to %c100_10 step %c1_1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
      %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
      memref.store %2, %alloca_11[%arg4] : memref<?xi64>
      memref.store %4, %alloca_12[%arg4] : memref<?xmemref<?xi8>>
      %5 = arith.addi %0, %c1_1 : index
      memref.store %5, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = "polygeist.memref2pointer"(%alloca_4) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %1 = arith.index_cast %arg4 : index to i64
      %2 = llvm.getelementptr %0[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %3 = llvm.load %2 : !llvm.ptr -> f64
      %4 = "polygeist.memref2pointer"(%alloca_12) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %5 = arith.index_cast %arg4 : index to i64
      %6 = llvm.getelementptr %4[%5] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %7 = llvm.load %6 : !llvm.ptr -> f64
      %8 = arith.addf %3, %7 : f64
      %9 = "polygeist.memref2pointer"(%alloca_8) : (memref<?xmemref<?xi8>>) -> !llvm.ptr
      %10 = arith.index_cast %arg4 : index to i64
      %11 = llvm.getelementptr %9[%10] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %8, %11 : f64, !llvm.ptr
    }
    return
  }
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuid(i64, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1 = arith.constant 1 : index
    %c20000_i32 = arith.constant 20000 : i32
    %c100_i32 = arith.constant 100 : i32
    %c0 = arith.constant 0 : index
    %c8_i64 = arith.constant 8 : i64
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %c9_i32 = arith.constant 9 : i32
    %c100 = arith.constant 100 : index
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = call @artsGetCurrentNode() : () -> i32
    scf.for %arg0 = %c0 to %c100 step %c1 {
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %16 = func.call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
        %17 = func.call @artsDbCreateWithGuid(%16, %c8_i64) : (i64, i64) -> memref<?xi8>
      }
    }
    %3 = call @artsGetCurrentNode() : () -> i32
    scf.for %arg0 = %c0 to %c100 step %c1 {
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %16 = func.call @artsReserveGuidRoute(%c9_i32, %3) : (i32, i32) -> i64
        %17 = func.call @artsDbCreateWithGuid(%16, %c8_i64) : (i64, i64) -> memref<?xi8>
      }
    }
    %4 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca : memref<0xi64> to memref<?xi64>
    %5 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %7 = call @artsEdtCreate(%6, %4, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %8 = call @artsInitializeAndStartEpoch(%7, %c0_i32) : (i64, i32) -> i64
    %9 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<1xi64>
    %cast_1 = memref.cast %alloca_0 : memref<1xi64> to memref<?xi64>
    %10 = arith.extsi %1 : i32 to i64
    affine.store %10, %alloca_0[0] : memref<1xi64>
    %11 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %13 = call @artsEdtCreateWithEpoch(%12, %9, %c1_i32, %cast_1, %c20000_i32, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %14 = llvm.mlir.addressof @str1 : !llvm.ptr
    %15 = llvm.getelementptr %14[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %16 = llvm.call @printf(%15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %16 = llvm.call @printf(%15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c200_i32 = arith.constant 200 : i32
    %c100_i32 = arith.constant 100 : i32
    %c102_i32 = arith.constant 102 : i32
    %c2_i32 = arith.constant 2 : i32
    %c0_i32 = arith.constant 0 : i32
    %c1_i32 = arith.constant 1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      scf.for %arg5 = %c0 to %c100 step %c1 {
        %9 = memref.load %alloca[] : memref<index>
        %10 = arith.addi %9, %c1 : index
        memref.store %10, %alloca[] : memref<index>
      }
    }
    scf.for %arg4 = %c0 to %c100 step %c1 {
      scf.for %arg5 = %c0 to %c100 step %c1 {
        %9 = memref.load %alloca[] : memref<index>
        %10 = arith.addi %9, %c1 : index
        memref.store %10, %alloca[] : memref<index>
      }
    }
    %2 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<100x100xi64>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      scf.for %arg5 = %c0 to %c100 step %c1 {
        %9 = func.call @artsEventCreate(%2, %c1_i32) : (i32, i32) -> i64
        memref.store %9, %alloca_0[%arg4, %arg5] : memref<100x100xi64>
      }
    }
    %3 = arith.sitofp %1 : i32 to f64
    %alloca_1 = memref.alloca() : memref<i32>
    %alloca_2 = memref.alloca() : memref<102xi64>
    %cast = memref.cast %alloca_2 : memref<102xi64> to memref<?xi64>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %9 = arith.index_cast %arg4 : index to i32
      %10 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %11 = func.call @artsGetCurrentNode() : () -> i32
      %12 = arith.extsi %9 : i32 to i64
      affine.store %12, %alloca_2[0] : memref<102xi64>
      %13 = arith.fptosi %3 : f64 to i64
      affine.store %13, %alloca_2[1] : memref<102xi64>
      memref.store %c2_i32, %alloca_1[] : memref<i32>
      scf.for %arg5 = %c0 to %c100 step %c1 {
        %17 = memref.load %alloca_0[%arg4, %arg5] : memref<100x100xi64>
        %18 = memref.load %alloca_1[] : memref<i32>
        %19 = arith.index_cast %18 : i32 to index
        memref.store %17, %alloca_2[%19] : memref<102xi64>
        %20 = arith.addi %18, %c1_i32 : i32
        memref.store %20, %alloca_1[] : memref<i32>
      }
      %14 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %16 = func.call @artsEdtCreateWithEpoch(%15, %11, %c102_i32, %cast, %c100_i32, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = call @artsGetCurrentNode() : () -> i32
    %alloca_3 = memref.alloca() : memref<0xi64>
    %cast_4 = memref.cast %alloca_3 : memref<0xi64> to memref<?xi64>
    %6 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %7 = "polygeist.pointer2memref"(%6) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %8 = call @artsEdtCreateWithEpoch(%7, %5, %c0_i32, %cast_4, %c200_i32, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %alloca_5 = memref.alloca() : memref<i32>
    %alloca_6 = memref.alloca() : memref<i32>
    %alloca_7 = memref.alloca() : memref<i32>
    %alloca_8 = memref.alloca() : memref<i32>
    %alloca_9 = memref.alloca() : memref<i32>
    scf.for %arg4 = %c1 to %c100 step %c1 {
      %9 = func.call @artsGetCurrentEpochGuid() : () -> i64
      %10 = func.call @artsGetCurrentNode() : () -> i32
      memref.store %c0_i32, %alloca_5[] : memref<i32>
      memref.store %c1_i32, %alloca_6[] : memref<i32>
      %11 = memref.load %alloca_6[] : memref<i32>
      %12 = arith.muli %11, %c100_i32 : i32
      memref.store %12, %alloca_6[] : memref<i32>
      %13 = memref.load %alloca_6[] : memref<i32>
      %14 = memref.load %alloca_5[] : memref<i32>
      %15 = arith.addi %14, %13 : i32
      memref.store %15, %alloca_5[] : memref<i32>
      memref.store %c1_i32, %alloca_7[] : memref<i32>
      %16 = memref.load %alloca_7[] : memref<i32>
      %17 = arith.muli %16, %c100_i32 : i32
      memref.store %17, %alloca_7[] : memref<i32>
      %18 = memref.load %alloca_7[] : memref<i32>
      %19 = memref.load %alloca_5[] : memref<i32>
      %20 = arith.addi %19, %18 : i32
      memref.store %20, %alloca_5[] : memref<i32>
      memref.store %c1_i32, %alloca_8[] : memref<i32>
      %21 = memref.load %alloca_8[] : memref<i32>
      %22 = arith.muli %21, %c100_i32 : i32
      memref.store %22, %alloca_8[] : memref<i32>
      %23 = memref.load %alloca_8[] : memref<i32>
      %24 = memref.load %alloca_5[] : memref<i32>
      %25 = arith.addi %24, %23 : i32
      memref.store %25, %alloca_5[] : memref<i32>
      %26 = memref.load %alloca_5[] : memref<i32>
      memref.store %c0_i32, %alloca_9[] : memref<i32>
      %27 = memref.load %alloca_9[] : memref<i32>
      %28 = arith.index_cast %27 : i32 to index
      %alloca_10 = memref.alloca(%28) : memref<?xi64>
      %29 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
      %31 = func.call @artsEdtCreateWithEpoch(%30, %10, %27, %alloca_10, %26, %9) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    }
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %0 = memref.load %arg1[%c0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = memref.load %arg1[%c1] : memref<?xi64>
    %3 = arith.sitofp %2 : i64 to f64
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %alloca_0 = memref.alloca() : memref<100xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %6 = memref.load %alloca[] : memref<index>
      %7 = memref.load %arg3[%6] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %8 = llvm.extractvalue %7[2] : !llvm.struct<(i64, i32, ptr)> 
      %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi8>
      memref.store %9, %alloca_0[%arg4] : memref<100xmemref<?xi8>>
      %10 = arith.addi %6, %c1 : index
      memref.store %10, %alloca[] : memref<index>
    }
    %4 = arith.sitofp %1 : i32 to f64
    %5 = arith.addf %4, %3 : f64
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %6 = "polygeist.memref2pointer"(%alloca_0) : (memref<100xmemref<?xi8>>) -> !llvm.ptr
      %7 = arith.index_cast %arg4 : index to i64
      %8 = llvm.getelementptr %6[%7] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %5, %8 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %alloca_0 = memref.alloca() : memref<100xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi8>
      memref.store %3, %alloca_0[%arg4] : memref<100xmemref<?xi8>>
      %4 = arith.addi %0, %c1 : index
      memref.store %4, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<100xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi8>
      memref.store %3, %alloca_1[%arg4] : memref<100xmemref<?xi8>>
      %4 = arith.addi %0, %c1 : index
      memref.store %4, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = "polygeist.memref2pointer"(%alloca_0) : (memref<100xmemref<?xi8>>) -> !llvm.ptr
      %1 = arith.index_cast %arg4 : index to i64
      %2 = llvm.getelementptr %0[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %3 = llvm.load %2 : !llvm.ptr -> f64
      %4 = "polygeist.memref2pointer"(%alloca_1) : (memref<100xmemref<?xi8>>) -> !llvm.ptr
      %5 = llvm.getelementptr %4[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %3, %5 : f64, !llvm.ptr
    }
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %alloca = memref.alloca() : memref<index>
    memref.store %c0, %alloca[] : memref<index>
    %alloca_0 = memref.alloca() : memref<100xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi8>
      memref.store %3, %alloca_0[%arg4] : memref<100xmemref<?xi8>>
      %4 = arith.addi %0, %c1 : index
      memref.store %4, %alloca[] : memref<index>
    }
    %alloca_1 = memref.alloca() : memref<100xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi8>
      memref.store %3, %alloca_1[%arg4] : memref<100xmemref<?xi8>>
      %4 = arith.addi %0, %c1 : index
      memref.store %4, %alloca[] : memref<index>
    }
    %alloca_2 = memref.alloca() : memref<100xmemref<?xi8>>
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = memref.load %alloca[] : memref<index>
      %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
      %2 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
      %3 = "polygeist.pointer2memref"(%2) : (!llvm.ptr) -> memref<?xi8>
      memref.store %3, %alloca_2[%arg4] : memref<100xmemref<?xi8>>
      %4 = arith.addi %0, %c1 : index
      memref.store %4, %alloca[] : memref<index>
    }
    scf.for %arg4 = %c0 to %c100 step %c1 {
      %0 = "polygeist.memref2pointer"(%alloca_0) : (memref<100xmemref<?xi8>>) -> !llvm.ptr
      %1 = arith.index_cast %arg4 : index to i64
      %2 = llvm.getelementptr %0[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %3 = llvm.load %2 : !llvm.ptr -> f64
      %4 = "polygeist.memref2pointer"(%alloca_2) : (memref<100xmemref<?xi8>>) -> !llvm.ptr
      %5 = llvm.getelementptr %4[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      %6 = llvm.load %5 : !llvm.ptr -> f64
      %7 = arith.addf %3, %6 : f64
      %8 = "polygeist.memref2pointer"(%alloca_1) : (memref<100xmemref<?xi8>>) -> !llvm.ptr
      %9 = llvm.getelementptr %8[%1] : (!llvm.ptr, i64) -> !llvm.ptr, f64
      llvm.store %7, %9 : f64, !llvm.ptr
    }
    return
  }
}

