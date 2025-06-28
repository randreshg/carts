-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
[convert-arts-to-llvm] Starting ARTS operation iteration
[convert-arts-to-llvm] Processing runtime information operations
[convert-arts-to-llvm] Processing epoch operations
[convert-arts-to-llvm] Lowering arts.epoch: arts.epoch {
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
[convert-arts-to-llvm] Creating epoch done EDT using EdtCodegen
[convert-arts-to-llvm] Created epoch done EDT with GUID
[convert-arts-to-llvm] Created epoch with handle
[convert-arts-to-llvm] Collecting child EDTs for epoch mapping
[convert-arts-to-llvm] Found 1 child EDTs
[convert-arts-to-llvm] Adding epoch wait
[convert-arts-to-llvm] Epoch processing completed successfully
[convert-arts-to-llvm] Processing allocation operations
[convert-arts-to-llvm] Processing top-level DB operations
[convert-arts-to-llvm] Found top-level DbAlloc: %1 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
[convert-arts-to-llvm] Lowering top-level arts.db_alloc (outside EDT): %1 = arts.db_alloc "inout" [] alloc_type = "stack" ->  attributes {newDb} : memref<memref<?xi8>>
[convert-arts-to-llvm] Replacing DbAlloc uses with allocated pointer
[convert-arts-to-llvm] DbAlloc processing completed successfully
[convert-arts-to-llvm] Found top-level DbDep: %25 = arts.db_dep "in"(%3 : memref<?xi8>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
[convert-arts-to-llvm] Processing top-level arts.db_dep (will be resolved in EDT context): %25 = arts.db_dep "in"(%3 : memref<?xi8>) indices[] offsets[] sizes[] {newDb} : memref<memref<?xi8>>
[db-codegen] WARNING: Could not find or create source DbCodegen for DbDep
[convert-arts-to-llvm] DbDep processing completed - will be resolved in EDT context
[convert-arts-to-llvm] Processing EDT operations
[convert-arts-to-llvm] Processing EDT: "arts.edt"(%32) ({
  %59 = "arts.db_alloc"() <{allocType = "stack", mode = "inout", operandSegmentSizes = array<i32: 0, 0>}> {newDb} : () -> memref<memref<?xi8>>
  %60 = "polygeist.memref2pointer"(%59) : (memref<memref<?xi8>>) -> !llvm.ptr
  %61 = "llvm.load"(%60) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
  "llvm.store"(%4, %61) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %62 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
  %63 = "llvm.getelementptr"(%62) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %64 = "polygeist.memref2pointer"(%32) : (memref<memref<?xi8>>) -> !llvm.ptr
  %65 = "llvm.load"(%64) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %66 = "llvm.call"(%63, %65, %27) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
  %67 = "polygeist.memref2pointer"(%59) : (memref<memref<?xi8>>) -> !llvm.ptr
  %68 = "llvm.load"(%67) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
  "llvm.store"(%27, %68) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %69 = "arts.db_dep"(%32) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<memref<?xi8>>) -> memref<memref<?xi8>>
  %70 = "arts.db_dep"(%59) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<memref<?xi8>>) -> memref<memref<?xi8>>
  "arts.edt"(%69, %70) ({
    %71 = "polygeist.memref2pointer"(%69) : (memref<memref<?xi8>>) -> !llvm.ptr
    %72 = "llvm.load"(%71) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %73 = "arith.addi"(%72, %2) : (i32, i32) -> i32
    %74 = "polygeist.memref2pointer"(%69) : (memref<memref<?xi8>>) -> !llvm.ptr
    "llvm.store"(%73, %74) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %75 = "polygeist.memref2pointer"(%70) : (memref<memref<?xi8>>) -> !llvm.ptr
    %76 = "llvm.load"(%75) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %77 = "arith.addi"(%76, %2) : (i32, i32) -> i32
    %78 = "polygeist.memref2pointer"(%70) : (memref<memref<?xi8>>) -> !llvm.ptr
    "llvm.store"(%77, %78) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %79 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
    %80 = "llvm.getelementptr"(%79) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %81 = "llvm.call"(%80, %73, %77) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "arts.yield"() : () -> ()
  }) {task} : (memref<memref<?xi8>>, memref<memref<?xi8>>) -> ()
  "arts.yield"() : () -> ()
}) {task} : (memref<memref<?xi8>>) -> ()
[convert-arts-to-llvm] Lowering arts.edt: "arts.edt"(%32) ({
  %59 = "arts.db_alloc"() <{allocType = "stack", mode = "inout", operandSegmentSizes = array<i32: 0, 0>}> {newDb} : () -> memref<memref<?xi8>>
  %60 = "polygeist.memref2pointer"(%59) : (memref<memref<?xi8>>) -> !llvm.ptr
  %61 = "llvm.load"(%60) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
  "llvm.store"(%4, %61) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %62 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
  %63 = "llvm.getelementptr"(%62) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %64 = "polygeist.memref2pointer"(%32) : (memref<memref<?xi8>>) -> !llvm.ptr
  %65 = "llvm.load"(%64) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %66 = "llvm.call"(%63, %65, %27) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
  %67 = "polygeist.memref2pointer"(%59) : (memref<memref<?xi8>>) -> !llvm.ptr
  %68 = "llvm.load"(%67) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
  "llvm.store"(%27, %68) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %69 = "arts.db_dep"(%32) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<memref<?xi8>>) -> memref<memref<?xi8>>
  %70 = "arts.db_dep"(%59) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<memref<?xi8>>) -> memref<memref<?xi8>>
  "arts.edt"(%69, %70) ({
    %71 = "polygeist.memref2pointer"(%69) : (memref<memref<?xi8>>) -> !llvm.ptr
    %72 = "llvm.load"(%71) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %73 = "arith.addi"(%72, %2) : (i32, i32) -> i32
    %74 = "polygeist.memref2pointer"(%69) : (memref<memref<?xi8>>) -> !llvm.ptr
    "llvm.store"(%73, %74) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %75 = "polygeist.memref2pointer"(%70) : (memref<memref<?xi8>>) -> !llvm.ptr
    %76 = "llvm.load"(%75) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %77 = "arith.addi"(%76, %2) : (i32, i32) -> i32
    %78 = "polygeist.memref2pointer"(%70) : (memref<memref<?xi8>>) -> !llvm.ptr
    "llvm.store"(%77, %78) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %79 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
    %80 = "llvm.getelementptr"(%79) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %81 = "llvm.call"(%80, %73, %77) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "arts.yield"() : () -> ()
  }) {task} : (memref<memref<?xi8>>, memref<memref<?xi8>>) -> ()
  "arts.yield"() : () -> ()
}) {task} : (memref<memref<?xi8>>) -> ()
[convert-arts-to-llvm] Created DbAlloc within EDT: %64 = "arts.db_alloc"() <{allocType = "stack", mode = "inout", operandSegmentSizes = array<i32: 0, 0>}> {newDb} : () -> memref<memref<?xi8>>
[convert-arts-to-llvm] Created DbDep within EDT: %74 = "arts.db_dep"(%32) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<memref<?xi8>>) -> memref<memref<?xi8>>
[convert-arts-to-llvm] Created DbDep within EDT: %75 = "arts.db_dep"(%64) <{mode = "inout", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<memref<?xi8>>) -> memref<memref<?xi8>>
[convert-arts-to-llvm] Processing EDT: "arts.edt"(%92, %93) ({
  %94 = "polygeist.memref2pointer"(%92) : (memref<memref<?xi8>>) -> !llvm.ptr
  %95 = "llvm.load"(%94) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %96 = "arith.addi"(%95, %2) : (i32, i32) -> i32
  %97 = "polygeist.memref2pointer"(%92) : (memref<memref<?xi8>>) -> !llvm.ptr
  "llvm.store"(%96, %97) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %98 = "polygeist.memref2pointer"(%93) : (memref<memref<?xi8>>) -> !llvm.ptr
  %99 = "llvm.load"(%98) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %100 = "arith.addi"(%99, %2) : (i32, i32) -> i32
  %101 = "polygeist.memref2pointer"(%93) : (memref<memref<?xi8>>) -> !llvm.ptr
  "llvm.store"(%100, %101) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %102 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
  %103 = "llvm.getelementptr"(%102) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %104 = "llvm.call"(%103, %96, %100) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
  "arts.yield"() : () -> ()
}) {task} : (memref<memref<?xi8>>, memref<memref<?xi8>>) -> ()
[convert-arts-to-llvm] Lowering arts.edt: "arts.edt"(%92, %93) ({
  %94 = "polygeist.memref2pointer"(%92) : (memref<memref<?xi8>>) -> !llvm.ptr
  %95 = "llvm.load"(%94) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %96 = "arith.addi"(%95, %2) : (i32, i32) -> i32
  %97 = "polygeist.memref2pointer"(%92) : (memref<memref<?xi8>>) -> !llvm.ptr
  "llvm.store"(%96, %97) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %98 = "polygeist.memref2pointer"(%93) : (memref<memref<?xi8>>) -> !llvm.ptr
  %99 = "llvm.load"(%98) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
  %100 = "arith.addi"(%99, %2) : (i32, i32) -> i32
  %101 = "polygeist.memref2pointer"(%93) : (memref<memref<?xi8>>) -> !llvm.ptr
  "llvm.store"(%100, %101) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
  %102 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
  %103 = "llvm.getelementptr"(%102) <{elem_type = !llvm.array<28 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
  %104 = "llvm.call"(%103, %96, %100) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
  "arts.yield"() : () -> ()
}) {task} : (memref<memref<?xi8>>, memref<memref<?xi8>>) -> ()
[convert-arts-to-llvm] Processing barrier operations
[convert-arts-to-llvm] Cleaning up removed operations
[convert-arts-to-llvm] Cleaning up unused operations
[convert-arts-to-llvm] Module after processing all ARTS operations:
"builtin.module"() ({
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>) -> (), sym_name = "__arts_edt_3", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>):
    "func.return"() : () -> ()
  }) : () -> ()
  "func.func"() <{function_type = () -> i64, sym_name = "artsGetCurrentEpochGuid", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>) -> (), sym_name = "__arts_edt_2", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>):
    "func.return"() : () -> ()
  }) : () -> ()
  "func.func"() <{function_type = (i64, i64) -> memref<?xi8>, sym_name = "artsDbCreateWithGuid", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, i32) -> i64, sym_name = "artsReserveGuidRoute", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64) -> i1, sym_name = "artsWaitOnHandle", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i64, i32) -> i64, sym_name = "artsInitializeAndStartEpoch", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64, sym_name = "artsEdtCreate", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "artsGetCurrentNode", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>) -> (), sym_name = "__arts_edt_1", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>):
    "func.return"() : () -> ()
  }) : () -> ()
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
    %30 = "memref.cast"(%9) : (memref<?xi8>) -> memref<memref<?xi8>>
    %31 = "arts.db_dep"(%9) <{mode = "in", operandSegmentSizes = array<i32: 1, 0, 0, 0>}> {newDb} : (memref<?xi8>) -> memref<memref<?xi8>>
    %32 = "func.call"() <{callee = @artsGetCurrentNode}> : () -> i32
    %33 = "arith.constant"() <{value = 0 : index}> : () -> index
    %34 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    "memref.store"(%33, %34) : (index, memref<index>) -> ()
    %35 = "memref.load"(%34) : (memref<index>) -> index
    %36 = "arith.index_cast"(%35) : (index) -> i32
    %37 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<index>
    %38 = "arith.constant"() <{value = 0 : index}> : () -> index
    "memref.store"(%38, %37) : (index, memref<index>) -> ()
    %39 = "memref.load"(%37) : (memref<index>) -> index
    %40 = "arith.index_cast"(%39) : (index) -> i32
    %41 = "memref.alloca"(%39) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xi64>
    %42 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %43 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %44 = "polygeist.pointer2memref"(%43) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %45 = "func.call"(%44, %32, %40, %41, %42) <{callee = @artsEdtCreate}> : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %46 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %47 = "func.call"(%45, %46) <{callee = @artsInitializeAndStartEpoch}> : (i64, i32) -> i64
    %48 = "func.call"(%47) <{callee = @artsWaitOnHandle}> : (i64) -> i1
    %49 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %50 = "llvm.load"(%49) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    %51 = "llvm.load"(%50) <{ordering = 0 : i64}> : (!llvm.ptr) -> i32
    %52 = "arith.addi"(%51, %2) : (i32, i32) -> i32
    %53 = "polygeist.memref2pointer"(%9) : (memref<?xi8>) -> !llvm.ptr
    %54 = "llvm.load"(%53) <{ordering = 0 : i64}> : (!llvm.ptr) -> !llvm.ptr
    "llvm.store"(%52, %54) <{ordering = 0 : i64}> : (i32, !llvm.ptr) -> ()
    %55 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
    %56 = "llvm.getelementptr"(%55) <{elem_type = !llvm.array<37 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %57 = "llvm.call"(%56, %52, %26) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32, i32) -> i32
    "func.return"(%0) : (i32) -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32) -> (), sym_name = "srand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi64>) -> i64, sym_name = "time", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> i32, sym_name = "rand", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()
-----------------------------------------
carts-opt: /home/randres/projects/carts/external/Polygeist/llvm-project/mlir/lib/IR/Operation.cpp:503: void llvm::ilist_traits<mlir::Operation>::addNodeToList(llvm::ilist_traits<mlir::Operation>::Operation *): Assertion `!op->getBlock() && "already in an operation block!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize --create-dbs --db --canonicalize --cse --polygeist-mem2reg --edt-pointer-rematerialization --create-epochs --canonicalize --preprocess-dbs --convert-arts-to-llvm --cse -debug-only=convert-arts-to-llvm,arts-codegen,edt-codegen,db-codegen
 #0 0x0000556507d41667 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11db667)
 #1 0x0000556507d3f23e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11d923e)
 #2 0x0000556507d41d1a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fe646bac520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007fe646c009fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007fe646bac476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007fe646b927f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007fe646b9271b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007fe646ba3e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x0000556507cad5a8 (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x11475a8)
#10 0x000055650739338c std::enable_if<mlir::ModuleOp::hasTrait<mlir::OpTrait::OneRegion>(), void>::type mlir::OpTrait::SingleBlock<mlir::ModuleOp>::insert<mlir::ModuleOp>(llvm::ilist_iterator<llvm::ilist_detail::node_options<mlir::Operation, true, false, void>, false, false>, mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x82d38c)
#11 0x000055650738ebc0 mlir::arts::ArtsCodegen::insertArtsMainFn(mlir::Location, mlir::func::FuncOp) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x828bc0)
#12 0x000055650738f369 mlir::arts::ArtsCodegen::initRT(mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x829369)
#13 0x000055650737e904 (anonymous namespace)::ConvertArtsToLLVMPass::runOnOperation() ConvertArtsToLLVM.cpp:0:0
#14 0x0000556507b75d64 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x100fd64)
#15 0x0000556507b76391 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1010391)
#16 0x0000556507b78842 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1012842)
#17 0x00005565073cf984 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#18 0x00005565073cebf4 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#19 0x0000556507cd6d48 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1170d48)
#20 0x00005565073c91fa mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x8631fa)
#21 0x00005565073c96c4 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x8636c4)
#22 0x0000556506ca7056 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x141056)
#23 0x00007fe646b93d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#24 0x00007fe646b93e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#25 0x0000556506ca6755 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x140755)
