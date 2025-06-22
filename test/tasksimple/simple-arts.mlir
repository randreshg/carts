
-----------------------------------------
DbPass STARTED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %1 = arts.db_alloc "inout"(%alloca : memref<i32>) [] -> : memref<i32>
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
    arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {sync} {
      %alloca_0 = memref.alloca() : memref<i32>
      %23 = arts.db_alloc "inout"(%alloca_0 : memref<i32>) [] -> : memref<i32>
      memref.store %0, %23[] : memref<i32>
      %24 = llvm.mlir.addressof @str2 : !llvm.ptr
      %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      %26 = memref.load %subview[] : memref<i32, strided<[]>>
      %27 = llvm.call @printf(%25, %26, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      memref.store %14, %23[] : memref<i32>
      %subview_1 = memref.subview %23[] [] [] : memref<i32> to memref<i32, strided<[]>>
      %subview_2 = memref.subview %1[] [] [] : memref<i32> to memref<i32, strided<[]>>
      arts.edt dependencies(%subview_1, %subview_2) : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) attributes {task} {
        %28 = memref.load %subview_2[] : memref<i32, strided<[]>>
        %29 = arith.addi %28, %c1_i32 : i32
        memref.store %29, %subview_2[] : memref<i32, strided<[]>>
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
[db-analysis] Initializing DbAnalysis for module
Getting or creating DbGraph for function: main
Creating new DbGraph for function: main
[db-dataflow-analysis] - Starting Dataflow analysis for function: main
  - Processing CallOp (ignoring for now)
    Merging environments
    - Final merged environment: {}
  - Processing CallOp (ignoring for now)
    Merging environments
    - Final merged environment: {}
  - Processing CallOp (ignoring for now)
    Merging environments
    - Final merged environment: {}
  - Processing CallOp (ignoring for now)
    Merging environments
    - Final merged environment: {}
  - Processing EDT #0
   Initial environment: {}
    Processing EDT inputs
    - Examining DB #A.1 as input
      Searching for definitions for DB #A.1
      - No previous definition for DB #A.1, add edge from entry node
    Processing EDT outputs
    Processing EDT body region
      - Processing EDT #1
       Initial environment: {}
        Processing EDT inputs
        - Examining DB #B.1 as input
          Searching for definitions for DB #B.1
          - No previous definition for DB #B.1, add edge from entry node
        - Examining DB #A.2 as input
          Searching for definitions for DB #A.2
          - No previous definition for DB #A.2, add edge from entry node
        Processing EDT outputs
        - Examining DB #B.1 as output
          Searching for definitions for DB #B.1
          - No previous definition for DB #B.1, updating environment with new definition
        - Examining DB #A.2 as output
          Searching for definitions for DB #A.2
[db-alias-analysis] Analyzing alias between DB #4 and DB #2
  -> Both are depes.
    -> Could not determine parents. Falling back to effects.
[db-alias-analysis] Analyzing memory effects overlap
  Effects A: 0, Effects B: 0
  -> No overlapping effects found -> no alias
  -> Final result: no alias
          - No previous definition for DB #A.2, updating environment with new definition
        Processing EDT body region
         - Finished processing region. Environment changed: false
      Finished processing EDT. Environment changed: true
        Merging environments
        - Adding DB #A.2 to merged environment
        - Adding DB #B.1 to merged environment
        - Final merged environment: { #A.2 -> #A.2, #B.1 -> #B.1,}
     - Finished processing region. Environment changed: true
  Finished processing EDT. Environment changed: false
    Merging environments
    - Final merged environment: {}
 - Finished processing region. Environment changed: false
[db-dataflow-analysis] Finished Dataflow analysis for function: main
===============================================
DbGraph for function: main
===============================================
Summary:
  Allocations: 2
  Dep nodes: 3
  Dependence edges: 0
  Allocation edges: 0

Allocation Hierarchy:
=====================
Allocation [A]: DbAllocNode 0 (A)
  Dep type: ReadWrite
  Dep nodes: 2
  In alloc edges: 0
  Out alloc edges: 0

   └── Dep nodes (2):
       ├── [A.1]: DbDepNode 1 (A.1)
  Dep type: Read
  In dep edges: 0
  Out dep edges: 0

       └── [A.2]: DbDepNode 2 (A.2)
  Dep type: ReadWrite
  In dep edges: 0
  Out dep edges: 0


Allocation [B]: DbAllocNode 3 (B)
  Dep type: Write
  Dep nodes: 1
  In alloc edges: 0
  Out alloc edges: 0

   └── Dep nodes (1):
       └── [B.1]: DbDepNode 4 (B.1)
  Dep type: ReadWrite
  In dep edges: 0
  Out dep edges: 0


===============================================
[db] Exported dot graph to: DbGraph_main.dot
Getting or creating DbGraph for function: srand
Creating new DbGraph for function: srand
[db-dataflow-analysis] - Starting Dataflow analysis for function: srand
 - Finished processing region. Environment changed: false
[db-dataflow-analysis] Finished Dataflow analysis for function: srand
===============================================
DbGraph for function: srand
===============================================
Summary:
  Allocations: 0
  Dep nodes: 0
  Dependence edges: 0
  Allocation edges: 0

Allocation Hierarchy:
=====================
===============================================
[db] Exported dot graph to: DbGraph_srand.dot
Getting or creating DbGraph for function: time
Creating new DbGraph for function: time
[db-dataflow-analysis] - Starting Dataflow analysis for function: time
 - Finished processing region. Environment changed: false
[db-dataflow-analysis] Finished Dataflow analysis for function: time
===============================================
DbGraph for function: time
===============================================
Summary:
  Allocations: 0
  Dep nodes: 0
  Dependence edges: 0
  Allocation edges: 0

Allocation Hierarchy:
=====================
===============================================
[db] Exported dot graph to: DbGraph_time.dot
Getting or creating DbGraph for function: rand
Creating new DbGraph for function: rand
[db-dataflow-analysis] - Starting Dataflow analysis for function: rand
 - Finished processing region. Environment changed: false
[db-dataflow-analysis] Finished Dataflow analysis for function: rand
===============================================
DbGraph for function: rand
===============================================
Summary:
  Allocations: 0
  Dep nodes: 0
  Dependence edges: 0
  Allocation edges: 0

Allocation Hierarchy:
=====================
===============================================
[db] Exported dot graph to: DbGraph_rand.dot
[db] No changes made to the module
-----------------------------------------
DbPass FINISHED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %1 = arts.db_alloc "inout"(%alloca : memref<i32>) [] -> : memref<i32>
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
    arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {sync} {
      %alloca_0 = memref.alloca() : memref<i32>
      %23 = arts.db_alloc "inout"(%alloca_0 : memref<i32>) [] -> : memref<i32>
      memref.store %0, %23[] : memref<i32>
      %24 = llvm.mlir.addressof @str2 : !llvm.ptr
      %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<28 x i8>
      %26 = memref.load %subview[] : memref<i32, strided<[]>>
      %27 = llvm.call @printf(%25, %26, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
      memref.store %14, %23[] : memref<i32>
      %subview_1 = memref.subview %23[] [] [] : memref<i32> to memref<i32, strided<[]>>
      %subview_2 = memref.subview %1[] [] [] : memref<i32> to memref<i32, strided<[]>>
      arts.edt dependencies(%subview_1, %subview_2) : (memref<i32, strided<[]>>, memref<i32, strided<[]>>) attributes {task} {
        %28 = memref.load %subview_2[] : memref<i32, strided<[]>>
        %29 = arith.addi %28, %c1_i32 : i32
        memref.store %29, %subview_2[] : memref<i32, strided<[]>>
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
[db-analysis] Destroying DbAnalysis
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
    %1 = arts.db_alloc "inout"(%alloca : memref<i32>) [] -> : memref<i32>
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
    arts.edt dependencies(%subview) : (memref<i32, strided<[]>>) attributes {sync} {
      %alloca_0 = memref.alloca() : memref<i32>
      %23 = arts.db_alloc "inout"(%alloca_0 : memref<i32>) [] -> : memref<i32>
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

