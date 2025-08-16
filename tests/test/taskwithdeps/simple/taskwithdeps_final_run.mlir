
-----------------------------------------
CreateDbsPass STARTED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx15.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
  llvm.mlir.global internal constant @str7("-----------------\0AMain function DONE\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("A: %d, B: %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("Parallel region DONE\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("Task 1: Initializing a with value %d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Main thread: Creating tasks\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("-----------------\0AMain function\0A-----------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5_i32 = arith.constant 5 : i32
    %c100_i32 = arith.constant 100 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<i32>
    %0 = llvm.mlir.undef : i32
    memref.store %0, %alloca[] : memref<i32>
    %alloca_0 = memref.alloca() : memref<i32>
    memref.store %0, %alloca_0[] : memref<i32>
    %1 = llvm.mlir.addressof @str0 : !llvm.ptr
    %2 = llvm.getelementptr %1[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<51 x i8>
    %3 = llvm.call @printf(%2) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    memref.store %c0_i32, %alloca_0[] : memref<i32>
    memref.store %c0_i32, %alloca[] : memref<i32>
    %4 = llvm.mlir.zero : !llvm.ptr
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?xi64>
    %6 = call @time(%5) : (memref<?xi64>) -> i64
    %7 = arith.trunci %6 : i64 to i32
    call @srand(%7) : (i32) -> ()
    arts.edt attributes {sync, task} {
      %19 = llvm.mlir.addressof @str1 : !llvm.ptr
      %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
      %21 = llvm.call @printf(%20) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
      arts.edt attributes {task} {
        %31 = func.call @rand() : () -> i32
        %32 = arith.remsi %31, %c100_i32 : i32
        memref.store %32, %alloca_0[] : memref<i32>
        %33 = llvm.mlir.addressof @str2 : !llvm.ptr
        %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
        %35 = llvm.call @printf(%34, %32) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
        arts.yield
      }
      %22 = memref.load %alloca_0[] : memref<i32>
      %23 = "polygeist.typeSize"() <{source = i32}> : () -> index
      %24 = arts.db_control "in" ptr[%alloca_0 : memref<i32>], indices[], offsets[], sizes[], type[i32], typeSize[%23] -> memref<i32>
      %25 = memref.load %alloca[] : memref<i32>
      %26 = "polygeist.typeSize"() <{source = i32}> : () -> index
      %27 = arts.db_control "inout" ptr[%alloca : memref<i32>], indices[], offsets[], sizes[], type[i32], typeSize[%26] -> memref<i32>
      arts.edt attributes {task} {
        %31 = memref.load %alloca_0[] : memref<i32>
        %32 = llvm.mlir.addressof @str3 : !llvm.ptr
        %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
        %34 = llvm.call @printf(%33, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
        %35 = arith.addi %31, %c5_i32 : i32
        memref.store %35, %alloca[] : memref<i32>
        arts.yield
      }
      %28 = memref.load %alloca[] : memref<i32>
      %29 = "polygeist.typeSize"() <{source = i32}> : () -> index
      %30 = arts.db_control "in" ptr[%alloca : memref<i32>], indices[], offsets[], sizes[], type[i32], typeSize[%29] -> memref<i32>
      arts.edt attributes {task} {
        %31 = memref.load %alloca[] : memref<i32>
        %32 = llvm.mlir.addressof @str4 : !llvm.ptr
        %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
        %34 = llvm.call @printf(%33, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
        arts.yield
      }
      arts.yield
    }
    %8 = llvm.mlir.addressof @str5 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
    %10 = llvm.call @printf(%9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %11 = llvm.mlir.addressof @str6 : !llvm.ptr
    %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<14 x i8>
    %13 = memref.load %alloca_0[] : memref<i32>
    %14 = memref.load %alloca[] : memref<i32>
    %15 = llvm.call @printf(%12, %13, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32) -> i32
    %16 = llvm.mlir.addressof @str7 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<56 x i8>
    %18 = llvm.call @printf(%17) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return %c0_i32 : i32
  }
  func.func private @srand(i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @time(memref<?xi64>) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[create-dbs] Starting CreateDbs pass analysis...
[create-dbs] Processing function: main
[create-dbs] Processing function: srand
[create-dbs] Processing function: time
[create-dbs] Processing function: rand
[create-dbs] Found 4 EDT operations
[create-dbs] Analyzing EDT region
  - Candidate #0: Memref %alloca_0 = memref.alloca() : memref<i32>, Dep inout
[create-dbs] Analyzing EDT region
  - Candidate #0: Memref %alloca = memref.alloca() : memref<i32>, Dep inout
  - Candidate #1: Memref %alloca_0 = memref.alloca() : memref<i32>, Dep in
[create-dbs] Analyzing EDT region
  - Candidate #0: Memref %alloca = memref.alloca() : memref<i32>, Dep in
[create-dbs] Analyzing EDT region
  - Candidate #0: Memref %alloca = memref.alloca() : memref<i32>, Dep in
  - Candidate #1: Memref %alloca_0 = memref.alloca() : memref<i32>, Dep in
[create-dbs] createDbDeps called for EDT: arts.edt attributes {sync, task} {
  %19 = llvm.mlir.addressof @str1 : !llvm.ptr
  %20 = llvm.getelementptr %19[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
  %21 = llvm.call @printf(%20) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
  arts.edt attributes {task} {
    %31 = func.call @rand() : () -> i32
    %32 = arith.remsi %31, %c100_i32 : i32
    memref.store %32, %alloca_0[] : memref<i32>
    %33 = llvm.mlir.addressof @str2 : !llvm.ptr
    %34 = llvm.getelementptr %33[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
    %35 = llvm.call @printf(%34, %32) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    arts.yield
  }
  %22 = memref.load %alloca_0[] : memref<i32>
  %23 = "polygeist.typeSize"() <{source = i32}> : () -> index
  %24 = arts.db_control "in" ptr[%alloca_0 : memref<i32>], indices[], offsets[], sizes[], type[i32], typeSize[%23] -> memref<i32>
  %25 = memref.load %alloca[] : memref<i32>
  %26 = "polygeist.typeSize"() <{source = i32}> : () -> index
  %27 = arts.db_control "inout" ptr[%alloca : memref<i32>], indices[], offsets[], sizes[], type[i32], typeSize[%26] -> memref<i32>
  arts.edt attributes {task} {
    %31 = memref.load %alloca_0[] : memref<i32>
    %32 = llvm.mlir.addressof @str3 : !llvm.ptr
    %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %34 = llvm.call @printf(%33, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %35 = arith.addi %31, %c5_i32 : i32
    memref.store %35, %alloca[] : memref<i32>
    arts.yield
  }
  %28 = memref.load %alloca[] : memref<i32>
  %29 = "polygeist.typeSize"() <{source = i32}> : () -> index
  %30 = arts.db_control "in" ptr[%alloca : memref<i32>], indices[], offsets[], sizes[], type[i32], typeSize[%29] -> memref<i32>
  arts.edt attributes {task} {
    %31 = memref.load %alloca[] : memref<i32>
    %32 = llvm.mlir.addressof @str4 : !llvm.ptr
    %33 = llvm.getelementptr %32[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %34 = llvm.call @printf(%33, %31) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    arts.yield
  }
  arts.yield
}
[create-dbs] Builder has insertion block: 1
[create-dbs] Number of unique ptr groups: 2
[create-dbs] Created DbAllocOp: %0 = arts.db_alloc "inout" (%alloca : memref<i32>) [] alloc_type = "stack" ->  : memref<i32>
[create-dbs] DbAllocOp result: %0 = arts.db_alloc "inout" (%alloca : memref<i32>) [] alloc_type = "stack" ->  : memref<i32>
[create-dbs] Created DbAlloc as sourcePtr for ptr: %alloca = memref.alloca() : memref<i32>
[create-dbs] createDbDep called with sourcePtr: %0 = arts.db_alloc "inout" (%alloca : memref<i32>) [] alloc_type = "stack" ->  : memref<i32>
[create-dbs] sourcePtr type: memref<i32>
[create-dbs] Created new DbDep for EDT dep: %9 = arts.db_dep "inout"(%0 : memref<i32>) indices[] offsets[] sizes[] : memref<i32>
[create-dbs] Created DbAllocOp: %2 = arts.db_alloc "inout" (%alloca_0 : memref<i32>) [] alloc_type = "stack" ->  : memref<i32>
[create-dbs] DbAllocOp result: %2 = arts.db_alloc "inout" (%alloca_0 : memref<i32>) [] alloc_type = "stack" ->  : memref<i32>
[create-dbs] Created DbAlloc as sourcePtr for ptr: %alloca_0 = memref.alloca() : memref<i32>
[create-dbs] createDbDep called with sourcePtr: %2 = arts.db_alloc "inout" (%alloca_0 : memref<i32>) [] alloc_type = "stack" ->  : memref<i32>
[create-dbs] sourcePtr type: memref<i32>
[create-dbs] Created new DbDep for EDT dep: %11 = arts.db_dep "inout"(%2 : memref<i32>) indices[] offsets[] sizes[] : memref<i32>
[create-dbs] createDbDeps called for EDT: arts.edt attributes {task} {
  %35 = func.call @rand() : () -> i32
  %36 = arith.remsi %35, %c100_i32 : i32
  memref.store %36, %alloca_0[] : memref<i32>
  %37 = llvm.mlir.addressof @str2 : !llvm.ptr
  %38 = llvm.getelementptr %37[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<38 x i8>
  %39 = llvm.call @printf(%38, %36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
  arts.yield
}
[create-dbs] Builder has insertion block: 1
[create-dbs] Number of unique ptr groups: 1
[create-dbs] Using parent dep as sourcePtr for ptr: %alloca_0 = memref.alloca() : memref<i32>
[create-dbs] createDbDep called with sourcePtr: %11 = arts.db_dep "inout"(%2 : memref<i32>) indices[] offsets[] sizes[] : memref<i32>
[create-dbs] sourcePtr type: memref<i32>
[create-dbs] Created new DbDep for EDT dep: %26 = arts.db_dep "inout"(%11 : memref<i32>) indices[] offsets[] sizes[] : memref<i32>
[create-dbs] createDbDeps called for EDT: "arts.edt"() ({
  %41 = "memref.load"(%6) <{nontemporal = false}> : (memref<i32>) -> i32
  %42 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
  %43 = "llvm.getelementptr"(%42) <{elem_type = !llvm.array<37 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %44 = "llvm.call"(%43, %41) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
  %45 = "arith.addi"(%41, %0) : (i32, i32) -> i32
  "memref.store"(%45, %3) <{nontemporal = false}> : (i32, memref<i32>) -> ()
  "arts.yield"() : () -> ()
}) {task} : () -> ()
[create-dbs] Builder has insertion block: 1
[create-dbs] Number of unique ptr groups: 2
[create-dbs] Using parent dep as sourcePtr for ptr: %3 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<i32>
[create-dbs] createDbDep called with sourcePtr: %15 = "arts.db_dep"(%4) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
[create-dbs] sourcePtr type: memref<i32>
[create-dbs] Created new DbDep for EDT dep: %38 = "arts.db_dep"(%15) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
[create-dbs] Using parent dep as sourcePtr for ptr: %6 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<i32>
[create-dbs] createDbDep called with sourcePtr: %16 = "arts.db_dep"(%7) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
[create-dbs] sourcePtr type: memref<i32>
[create-dbs] Created new DbDep for EDT dep: %39 = "arts.db_dep"(%16) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
[create-dbs] createDbDeps called for EDT: "arts.edt"() ({
  %43 = "memref.load"(%3) <{nontemporal = false}> : (memref<i32>) -> i32
  %44 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
  %45 = "llvm.getelementptr"(%44) <{elem_type = !llvm.array<29 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %46 = "llvm.call"(%45, %43) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
  "arts.yield"() : () -> ()
}) {task} : () -> ()
[create-dbs] Builder has insertion block: 1
[create-dbs] Number of unique ptr groups: 1
[create-dbs] Using parent dep as sourcePtr for ptr: %3 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<i32>
[create-dbs] createDbDep called with sourcePtr: %15 = "arts.db_dep"(%4) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
[create-dbs] sourcePtr type: memref<i32>
[create-dbs] Created new DbDep for EDT dep: %43 = "arts.db_dep"(%15) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
[create-dbs] Cleaning up 4 operations
[create-dbs] Erasing edt operation: "arts.edt"() ({
  %44 = "func.call"() <{callee = @rand}> : () -> i32
  %45 = "arith.remsi"(%44, %1) : (i32, i32) -> i32
  "memref.store"(%45, %6) <{nontemporal = false}> : (i32, memref<i32>) -> ()
  %46 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
  %47 = "llvm.getelementptr"(%46) <{elem_type = !llvm.array<38 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %48 = "llvm.call"(%47, %45) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
  "arts.yield"() : () -> ()
}) {task} : () -> ()
[create-dbs] Erasing edt operation: "arts.edt"() ({
  %44 = "memref.load"(%6) <{nontemporal = false}> : (memref<i32>) -> i32
  %45 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
  %46 = "llvm.getelementptr"(%45) <{elem_type = !llvm.array<37 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %47 = "llvm.call"(%46, %44) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
  %48 = "arith.addi"(%44, %0) : (i32, i32) -> i32
  "memref.store"(%48, %3) <{nontemporal = false}> : (i32, memref<i32>) -> ()
  "arts.yield"() : () -> ()
}) {task} : () -> ()
[create-dbs] Erasing edt operation: "arts.edt"() ({
  %44 = "memref.load"(%3) <{nontemporal = false}> : (memref<i32>) -> i32
  %45 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
  %46 = "llvm.getelementptr"(%45) <{elem_type = !llvm.array<29 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %47 = "llvm.call"(%46, %44) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
  "arts.yield"() : () -> ()
}) {task} : () -> ()
[create-dbs] Erasing edt operation: "arts.edt"() ({
  %28 = "llvm.mlir.addressof"() <{global_name = @str1}> : () -> !llvm.ptr
  %29 = "llvm.getelementptr"(%28) <{elem_type = !llvm.array<29 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %30 = "llvm.call"(%29) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr) -> i32
  %31 = "arts.db_dep"(%16) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
  %32 = "memref.load"(%6) <{nontemporal = false}> : (memref<i32>) -> i32
  %33 = "polygeist.typeSize"() <{source = i32}> : () -> index
  %34 = "arts.db_control"(%6, %33) <{elementType = i32, mode = "in", operandSegmentSizes = array<i32: 1, 1, 0, 0, 0>}> : (memref<i32>, index) -> memref<i32>
  %35 = "memref.load"(%3) <{nontemporal = false}> : (memref<i32>) -> i32
  %36 = "polygeist.typeSize"() <{source = i32}> : () -> index
  %37 = "arts.db_control"(%3, %36) <{elementType = i32, mode = "inout", operandSegmentSizes = array<i32: 1, 1, 0, 0, 0>}> : (memref<i32>, index) -> memref<i32>
  %38 = "arts.db_dep"(%15) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
  %39 = "arts.db_dep"(%16) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
  %40 = "memref.load"(%3) <{nontemporal = false}> : (memref<i32>) -> i32
  %41 = "polygeist.typeSize"() <{source = i32}> : () -> index
  %42 = "arts.db_control"(%3, %41) <{elementType = i32, mode = "in", operandSegmentSizes = array<i32: 1, 1, 0, 0, 0>}> : (memref<i32>, index) -> memref<i32>
  %43 = "arts.db_dep"(%15) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (memref<i32>) -> memref<i32>
  "arts.yield"() : () -> ()
}) {sync, task} : () -> ()
taskwithdeps.mlir:1:1: error: 'arts.db_dep' op operation destroyed but still has uses
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:o-i64:64-i128:128-n32:64-S128", llvm.target_triple = "arm64-apple-macosx15.0.0", "polygeist.target-cpu" = "apple-m1", "polygeist.target-features" = "+aes,+crc,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz"} {
^
taskwithdeps.mlir:1:1: note: see current operation: %0 = "arts.db_dep"(<<NULL VALUE>>) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> : (<<NULL TYPE>>) -> memref<i32>
taskwithdeps.mlir:1:1: note: - use: 
"arts.edt"(<<UNKNOWN SSA VALUE>>) ({
^bb0(%arg2: memref<i32>):
  %37 = "memref.load"(%arg2) <{nontemporal = false}> : (memref<i32>) -> i32
  %38 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
  %39 = "llvm.getelementptr"(%38) <{elem_type = !llvm.array<29 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %40 = "llvm.call"(%39, %37) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
  "arts.yield"() : () -> ()
}) : (memref<i32>) -> ()

LLVM ERROR: operation destroyed but still has uses
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: /Users/randreshg/carts/.install/carts/bin/carts-opt taskwithdeps.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --arts-inliner --convert-openmp-to-arts --edt --create-dbs --cse --debug-only=create-dbs
Stack dump without symbol names (ensure you have llvm-symbolizer in your PATH or set the environment var `LLVM_SYMBOLIZER_PATH` to point to it):
0  carts-opt                0x0000000101125674 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) + 56
1  carts-opt                0x0000000101123814 llvm::sys::RunSignalHandlers() + 112
2  carts-opt                0x0000000101125d08 SignalHandler(int) + 360
3  libsystem_platform.dylib 0x000000018a1a4624 _sigtramp + 56
4  libsystem_pthread.dylib  0x000000018a16a88c pthread_kill + 296
5  libsystem_c.dylib        0x000000018a073c60 abort + 124
6  carts-opt                0x00000001010e05ec llvm::report_fatal_error(llvm::Twine const&, bool) + 456
7  carts-opt                0x00000001010e0424 llvm::report_fatal_error(llvm::Twine const&, bool) + 0
8  carts-opt                0x000000010108b5c8 mlir::Operation::~Operation() + 232
9  carts-opt                0x000000010108c35c llvm::ilist_traits<mlir::Operation>::deleteNode(mlir::Operation*) + 68
10 carts-opt                0x000000010101e584 mlir::Block::~Block() + 128
11 carts-opt                0x00000001010994a4 llvm::iplist_impl<llvm::simple_ilist<mlir::Block>, llvm::ilist_traits<mlir::Block>>::erase(llvm::ilist_iterator<llvm::ilist_detail::node_options<mlir::Block, true, false, void>, false, false>) + 104
12 carts-opt                0x000000010109853c mlir::Region::~Region() + 76
13 carts-opt                0x000000010108b684 mlir::Operation::~Operation() + 420
14 carts-opt                0x00000001010917d0 llvm::iplist_impl<llvm::simple_ilist<mlir::Operation>, llvm::ilist_traits<mlir::Operation>>::erase(llvm::ilist_iterator<llvm::ilist_detail::node_options<mlir::Operation, true, false, void>, false, false>) + 124
15 carts-opt                0x0000000100943970 (anonymous namespace)::CreateDbsPass::runOnOperation() + 11928
16 carts-opt                0x0000000100f892a0 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) + 588
17 carts-opt                0x0000000100f899a4 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) + 304
18 carts-opt                0x0000000100f8b8d0 mlir::PassManager::run(mlir::Operation*) + 1228
19 carts-opt                0x0000000100a5fbd8 performActions(llvm::raw_ostream&, std::__1::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) + 3016
20 carts-opt                0x0000000100a5ef70 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::__1::unique_ptr<llvm::MemoryBuffer, std::__1::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::__1::unique_ptr<llvm::MemoryBuffer, std::__1::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_0>(long, std::__1::unique_ptr<llvm::MemoryBuffer, std::__1::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) + 472
21 carts-opt                0x00000001010ac52c mlir::splitAndProcessBuffer(std::__1::unique_ptr<llvm::MemoryBuffer, std::__1::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::__1::unique_ptr<llvm::MemoryBuffer, std::__1::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) + 628
22 carts-opt                0x0000000100a5ae9c mlir::MlirOptMain(llvm::raw_ostream&, std::__1::unique_ptr<llvm::MemoryBuffer, std::__1::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) + 344
23 carts-opt                0x0000000100a5b390 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) + 1204
24 carts-opt                0x000000010012a6fc main + 2304
25 dyld                     0x0000000189dcab98 start + 6076
/Users/randreshg/carts/.install/carts/bin/carts: line 121: 50124 Abort trap: 6           "${CARTS_INSTALL_DIR}/bin/carts-opt" "$@"
