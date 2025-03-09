module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str12("Node %u initialized.\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str11("---------- Main EDT finished ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str10("---------- Main EDT ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str9("The program has finished its execution\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str8("--------------------------------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str7("---------- Compute EDT finished ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str6("---------- Compute EDT ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str5("---------- Parallel EDT finished ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str4("---------- Parallel EDT started ----------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str3("B[%d]: %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str2("A[%d]: %f\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("------------------------\0A--- Compute B[%u] = %f\0A------------------------\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("------------------------\0A--- Compute A[%u] = %f - Guid: %lu\0A------------------------\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  llvm.func @test(%arg0: i32) {
    llvm.return
  }
  llvm.func @computeA(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.mlir.constant(2.000000e+00 : f64) : f64
    %2 = llvm.load %arg1 : !llvm.ptr -> i64
    %3 = llvm.trunc %2 : i64 to i32
    %4 = llvm.getelementptr %arg1[1] : (!llvm.ptr) -> !llvm.ptr, i64
    %5 = llvm.load %4 : !llvm.ptr -> i64
    %6 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %7 = llvm.load %6 : !llvm.ptr -> !llvm.ptr
    %8 = llvm.load %arg3 : !llvm.ptr -> i64
    %9 = llvm.uitofp %3 : i32 to f64
    %10 = llvm.fmul %9, %1  : f64
    llvm.store %10, %7 : f64, !llvm.ptr
    %11 = llvm.mlir.addressof @str0 : !llvm.ptr
    %12 = llvm.getelementptr %11[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<86 x i8>
    %13 = llvm.load %7 : !llvm.ptr -> f64
    %14 = llvm.call @printf(%12, %3, %13, %8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64, i64) -> i32
    llvm.call @artsEventSatisfySlot(%5, %8, %0) : (i64, i64, i32) -> ()
    llvm.return
  }
  llvm.func @artsEventSatisfySlot(i64, i64, i32) attributes {sym_visibility = "private"}
  llvm.func @computeB(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.constant(24 : i64) : i64
    %1 = llvm.mlir.constant(0 : i32) : i32
    %2 = llvm.mlir.constant(0.000000e+00 : f64) : f64
    %3 = llvm.mlir.constant(2 : index) : i64
    %4 = llvm.load %arg1 : !llvm.ptr -> i64
    %5 = llvm.trunc %4 : i64 to i32
    %6 = llvm.getelementptr %arg3[24] : (!llvm.ptr) -> !llvm.ptr, i8
    %7 = llvm.getelementptr %6[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %8 = llvm.load %7 : !llvm.ptr -> !llvm.ptr
    %9 = llvm.icmp "ugt" %5, %1 : i32
    llvm.cond_br %9, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    %10 = llvm.mul %0, %3  : i64
    %11 = llvm.getelementptr %arg3[%10] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %12 = llvm.getelementptr %11[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %13 = llvm.load %12 : !llvm.ptr -> !llvm.ptr
    %14 = llvm.load %13 : !llvm.ptr -> f64
    llvm.br ^bb3(%14 : f64)
  ^bb2:  // pred: ^bb0
    llvm.br ^bb3(%2 : f64)
  ^bb3(%15: f64):  // 2 preds: ^bb1, ^bb2
    llvm.br ^bb4
  ^bb4:  // pred: ^bb3
    %16 = llvm.getelementptr %arg3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %17 = llvm.load %16 : !llvm.ptr -> !llvm.ptr
    %18 = llvm.load %8 : !llvm.ptr -> f64
    %19 = llvm.fadd %18, %15  : f64
    llvm.store %19, %17 : f64, !llvm.ptr
    %20 = llvm.mlir.addressof @str1 : !llvm.ptr
    %21 = llvm.getelementptr %20[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<74 x i8>
    %22 = llvm.load %17 : !llvm.ptr -> f64
    %23 = llvm.call @printf(%21, %5, %22) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    llvm.return
  }
  llvm.func @printDataBlockA(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.constant(1 : i32) : i32
    %1 = llvm.mlir.constant(0 : i32) : i32
    %2 = llvm.mlir.constant(24 : i64) : i64
    %3 = llvm.mlir.addressof @str2 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    llvm.br ^bb1(%1 : i32)
  ^bb1(%5: i32):  // 2 preds: ^bb0, ^bb2
    %6 = llvm.sext %5 : i32 to i64
    %7 = llvm.load %arg1 : !llvm.ptr -> i64
    %8 = llvm.icmp "slt" %6, %7 : i64
    llvm.cond_br %8, ^bb2(%5 : i32), ^bb3
  ^bb2(%9: i32):  // pred: ^bb1
    %10 = llvm.sext %9 : i32 to i64
    %11 = llvm.mul %10, %2  : i64
    %12 = llvm.getelementptr %arg3[%11] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %13 = llvm.getelementptr %12[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %14 = llvm.load %13 : !llvm.ptr -> !llvm.ptr
    %15 = llvm.load %14 : !llvm.ptr -> f64
    %16 = llvm.call @printf(%4, %9, %15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    %17 = llvm.add %9, %0  : i32
    llvm.br ^bb1(%17 : i32)
  ^bb3:  // pred: ^bb1
    llvm.return
  }
  llvm.func @printDataBlockB(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.constant(1 : i32) : i32
    %1 = llvm.mlir.constant(0 : i32) : i32
    %2 = llvm.mlir.constant(24 : i64) : i64
    %3 = llvm.mlir.addressof @str3 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    llvm.br ^bb1(%1 : i32)
  ^bb1(%5: i32):  // 2 preds: ^bb0, ^bb2
    %6 = llvm.sext %5 : i32 to i64
    %7 = llvm.load %arg1 : !llvm.ptr -> i64
    %8 = llvm.icmp "slt" %6, %7 : i64
    llvm.cond_br %8, ^bb2(%5 : i32), ^bb3
  ^bb2(%9: i32):  // pred: ^bb1
    %10 = llvm.sext %9 : i32 to i64
    %11 = llvm.mul %10, %2  : i64
    %12 = llvm.getelementptr %arg3[%11] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %13 = llvm.getelementptr %12[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, ptr)>
    %14 = llvm.load %13 : !llvm.ptr -> !llvm.ptr
    %15 = llvm.load %14 : !llvm.ptr -> f64
    %16 = llvm.call @printf(%4, %9, %15) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    %17 = llvm.add %9, %0  : i32
    llvm.br ^bb1(%17 : i32)
  ^bb3:  // pred: ^bb1
    llvm.return
  }
  llvm.func @parallelEdt(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.constant(2 : index) : i64
    %1 = llvm.mlir.constant(0 : index) : i64
    %2 = llvm.mlir.constant(1 : index) : i64
    %3 = llvm.mlir.constant(-1 : i32) : i32
    %4 = llvm.mlir.constant(8 : i64) : i64
    %5 = llvm.mlir.constant(3 : i32) : i32
    %6 = llvm.mlir.constant(2 : i32) : i32
    %7 = llvm.mlir.constant(2.000000e+00 : f64) : f64
    %8 = llvm.mlir.constant(1 : i32) : i32
    %9 = llvm.mlir.constant(0 : i32) : i32
    %10 = llvm.alloca %2 x i64 : (i64) -> !llvm.ptr
    %11 = llvm.alloca %0 x i64 : (i64) -> !llvm.ptr
    %12 = llvm.mlir.addressof @str4 : !llvm.ptr
    %13 = llvm.getelementptr %12[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<44 x i8>
    %14 = llvm.call @printf(%13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %15 = llvm.call @artsGetCurrentNode() : () -> i32
    %16 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %17 = llvm.load %arg1 : !llvm.ptr -> i64
    %18 = llvm.trunc %17 : i64 to i32
    %19 = llvm.sext %18 : i32 to i64
    %20 = llvm.alloca %19 x !llvm.ptr : (i64) -> !llvm.ptr
    %21 = llvm.alloca %19 x !llvm.ptr : (i64) -> !llvm.ptr
    %22 = llvm.alloca %19 x i64 : (i64) -> !llvm.ptr
    %23 = llvm.alloca %19 x i64 : (i64) -> !llvm.ptr
    llvm.call @artsDbCreatePtrAndGuidArrayFromDeps(%20, %22, %18, %arg3, %9) : (!llvm.ptr, !llvm.ptr, i32, !llvm.ptr, i32) -> ()
    llvm.call @artsDbCreatePtrAndGuidArrayFromDeps(%21, %23, %18, %arg3, %18) : (!llvm.ptr, !llvm.ptr, i32, !llvm.ptr, i32) -> ()
    %24 = llvm.mlir.addressof @str2 : !llvm.ptr
    %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %26 = llvm.mlir.addressof @str3 : !llvm.ptr
    %27 = llvm.getelementptr %26[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    llvm.br ^bb1(%1 : i64)
  ^bb1(%28: i64):  // 2 preds: ^bb0, ^bb2
    %29 = llvm.icmp "slt" %28, %19 : i64
    llvm.cond_br %29, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %30 = llvm.trunc %28 : i64 to i32
    %31 = llvm.getelementptr %20[%28] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    %32 = llvm.load %31 : !llvm.ptr -> !llvm.ptr
    %33 = llvm.load %32 : !llvm.ptr -> f32
    %34 = llvm.fpext %33 : f32 to f64
    %35 = llvm.call @printf(%25, %30, %34) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    %36 = llvm.getelementptr %21[%28] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    %37 = llvm.load %36 : !llvm.ptr -> !llvm.ptr
    %38 = llvm.load %37 : !llvm.ptr -> f32
    %39 = llvm.fpext %38 : f32 to f64
    %40 = llvm.call @printf(%27, %30, %39) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    %41 = llvm.add %28, %2  : i64
    llvm.br ^bb1(%41 : i64)
  ^bb3:  // pred: ^bb1
    llvm.br ^bb4(%1 : i64)
  ^bb4(%42: i64):  // 2 preds: ^bb3, ^bb5
    %43 = llvm.icmp "slt" %42, %19 : i64
    llvm.cond_br %43, ^bb5, ^bb6
  ^bb5:  // pred: ^bb4
    %44 = llvm.trunc %42 : i64 to i32
    %45 = llvm.getelementptr %20[%42] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    %46 = llvm.load %45 : !llvm.ptr -> !llvm.ptr
    %47 = llvm.sitofp %44 : i32 to f64
    %48 = llvm.fmul %47, %7  : f64
    %49 = llvm.fptrunc %48 : f64 to f32
    llvm.store %49, %46 : f32, !llvm.ptr
    %50 = llvm.getelementptr %21[%42] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    %51 = llvm.load %50 : !llvm.ptr -> !llvm.ptr
    llvm.store %49, %51 : f32, !llvm.ptr
    %52 = llvm.add %42, %2  : i64
    llvm.br ^bb4(%52 : i64)
  ^bb6:  // pred: ^bb4
    %53 = llvm.mul %19, %4  : i64
    %54 = llvm.call @artsMalloc(%53) : (i64) -> !llvm.ptr
    llvm.br ^bb7(%1 : i64)
  ^bb7(%55: i64):  // 2 preds: ^bb6, ^bb8
    %56 = llvm.icmp "slt" %55, %19 : i64
    llvm.cond_br %56, ^bb8, ^bb9
  ^bb8:  // pred: ^bb7
    %57 = llvm.trunc %55 : i64 to i32
    %58 = llvm.call @artsEventCreate(%15, %8) : (i32, i32) -> i64
    %59 = llvm.getelementptr %54[%57] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    llvm.store %58, %59 : i64, !llvm.ptr
    %60 = llvm.add %55, %2  : i64
    llvm.br ^bb7(%60 : i64)
  ^bb9:  // pred: ^bb7
    %61 = llvm.mlir.addressof @computeA : !llvm.ptr
    %62 = llvm.mlir.addressof @computeB : !llvm.ptr
    llvm.br ^bb10(%1 : i64)
  ^bb10(%63: i64):  // 2 preds: ^bb9, ^bb13
    %64 = llvm.icmp "slt" %63, %19 : i64
    llvm.cond_br %64, ^bb11, ^bb14
  ^bb11:  // pred: ^bb10
    %65 = llvm.trunc %63 : i64 to i32
    %66 = llvm.sext %65 : i32 to i64
    llvm.store %66, %11 : i64, !llvm.ptr
    %67 = llvm.getelementptr %54[%65] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %68 = llvm.load %67 : !llvm.ptr -> i64
    %69 = llvm.getelementptr %11[1] : (!llvm.ptr) -> !llvm.ptr, i64
    llvm.store %68, %69 : i64, !llvm.ptr
    %70 = llvm.call @artsEdtCreateWithEpoch(%61, %15, %6, %11, %8, %16) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.store %66, %10 : i64, !llvm.ptr
    %71 = llvm.icmp "sgt" %65, %9 : i32
    %72 = llvm.select %71, %5, %6 : i1, i32
    %73 = llvm.call @artsEdtCreateWithEpoch(%62, %15, %6, %10, %72, %16) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    %74 = llvm.load %67 : !llvm.ptr -> i64
    llvm.call @artsAddDependence(%74, %73, %8) : (i64, i64, i32) -> ()
    llvm.cond_br %71, ^bb12, ^bb13
  ^bb12:  // pred: ^bb11
    %75 = llvm.add %65, %3  : i32
    %76 = llvm.getelementptr %54[%75] : (!llvm.ptr, i32) -> !llvm.ptr, i64
    %77 = llvm.load %76 : !llvm.ptr -> i64
    llvm.call @artsAddDependence(%77, %73, %6) : (i64, i64, i32) -> ()
    llvm.br ^bb13
  ^bb13:  // 2 preds: ^bb11, ^bb12
    %78 = llvm.getelementptr %22[%63] : (!llvm.ptr, i64) -> !llvm.ptr, i64
    %79 = llvm.load %78 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%70, %9, %79) : (i64, i32, i64) -> ()
    %80 = llvm.load %78 : !llvm.ptr -> i64
    llvm.call @artsSignalEdt(%73, %9, %80) : (i64, i32, i64) -> ()
    %81 = llvm.add %63, %2  : i64
    llvm.br ^bb10(%81 : i64)
  ^bb14:  // pred: ^bb10
    llvm.return
  }
  llvm.func @artsGetCurrentNode() -> i32 attributes {sym_visibility = "private"}
  llvm.func @artsGetCurrentEpochGuid() -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsDbCreatePtrAndGuidArrayFromDeps(!llvm.ptr, !llvm.ptr, i32, !llvm.ptr, i32) attributes {sym_visibility = "private"}
  llvm.func @artsMalloc(i64) -> !llvm.ptr attributes {sym_visibility = "private"}
  llvm.func @artsEventCreate(i32, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsEdtCreateWithEpoch(!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsAddDependence(i64, i64, i32) attributes {sym_visibility = "private"}
  llvm.func @artsSignalEdt(i64, i32, i64) attributes {sym_visibility = "private"}
  llvm.func @parallelDoneEdt(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.addressof @str5 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<45 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.return
  }
  llvm.func @computeEDT(%arg0: i32, %arg1: !llvm.ptr, %arg2: !llvm.ptr) {
    %0 = llvm.mlir.constant(2 : i32) : i32
    %1 = llvm.mlir.constant(1 : i32) : i32
    %2 = llvm.mlir.constant(0 : i32) : i32
    %3 = llvm.mlir.constant(1 : index) : i64
    %4 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %5 = llvm.alloca %3 x i64 : (i64) -> !llvm.ptr
    %6 = llvm.mlir.addressof @str6 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %8 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %9 = llvm.call @artsGetCurrentNode() : () -> i32
    %10 = llvm.call @artsGetCurrentEpochGuid() : () -> i64
    %11 = llvm.mlir.addressof @parallelDoneEdt : !llvm.ptr
    %12 = llvm.mlir.zero : !llvm.ptr
    %13 = llvm.call @artsEdtCreate(%11, %9, %2, %12, %1) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %14 = llvm.call @artsInitializeAndStartEpoch(%13, %2) : (i64, i32) -> i64
    %15 = llvm.sext %arg0 : i32 to i64
    llvm.store %15, %5 : i64, !llvm.ptr
    %16 = llvm.mul %arg0, %0  : i32
    %17 = llvm.mlir.addressof @parallelEdt : !llvm.ptr
    %18 = llvm.call @artsEdtCreateWithEpoch(%17, %9, %1, %5, %16, %14) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.call @artsSignalDbs(%arg1, %18, %2, %arg0) : (!llvm.ptr, i64, i32, i32) -> ()
    llvm.call @artsSignalDbs(%arg2, %18, %arg0, %arg0) : (!llvm.ptr, i64, i32, i32) -> ()
    %19 = llvm.call @artsWaitOnHandle(%14) : (i64) -> i8
    llvm.store %15, %4 : i64, !llvm.ptr
    %20 = llvm.mlir.addressof @printDataBlockA : !llvm.ptr
    %21 = llvm.call @artsEdtCreateWithEpoch(%20, %9, %1, %4, %arg0, %10) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.call @artsSignalDbs(%arg1, %21, %2, %arg0) : (!llvm.ptr, i64, i32, i32) -> ()
    %22 = llvm.mlir.addressof @printDataBlockB : !llvm.ptr
    %23 = llvm.call @artsEdtCreateWithEpoch(%22, %9, %1, %4, %arg0, %10) : (!llvm.ptr, i32, i32, !llvm.ptr, i32, i64) -> i64
    llvm.call @artsSignalDbs(%arg2, %23, %2, %arg0) : (!llvm.ptr, i64, i32, i32) -> ()
    %24 = llvm.mlir.addressof @str7 : !llvm.ptr
    %25 = llvm.getelementptr %24[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<44 x i8>
    %26 = llvm.call @printf(%25) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.return
  }
  llvm.func @artsEdtCreate(!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {sym_visibility = "private"}
  llvm.func @artsSignalDbs(!llvm.ptr, i64, i32, i32) attributes {sym_visibility = "private"}
  llvm.func @artsWaitOnHandle(i64) -> i8 attributes {sym_visibility = "private"}
  llvm.func @finishMainEdt(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.addressof @str8 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = llvm.mlir.addressof @str9 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
    %5 = llvm.call @printf(%4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %6 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.call @artsShutdown() : () -> ()
    llvm.return
  }
  llvm.func @artsShutdown() attributes {sym_visibility = "private"}
  llvm.func @mainEdt(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %0 = llvm.mlir.constant(5 : index) : i64
    %1 = llvm.mlir.constant(0 : index) : i64
    %2 = llvm.mlir.constant(1 : index) : i64
    %3 = llvm.mlir.constant(8 : i64) : i64
    %4 = llvm.mlir.constant(9 : i32) : i32
    %5 = llvm.mlir.constant(0.000000e+00 : f64) : f64
    %6 = llvm.mlir.constant(5 : i32) : i32
    %7 = llvm.mlir.constant(1 : i32) : i32
    %8 = llvm.mlir.constant(0 : i32) : i32
    %9 = llvm.mlir.addressof @str10 : !llvm.ptr
    %10 = llvm.getelementptr %9[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<32 x i8>
    %11 = llvm.call @printf(%10) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %12 = llvm.call @artsGetCurrentNode() : () -> i32
    %13 = llvm.mlir.addressof @finishMainEdt : !llvm.ptr
    %14 = llvm.mlir.zero : !llvm.ptr
    %15 = llvm.call @artsEdtCreate(%13, %12, %8, %14, %7) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %16 = llvm.call @artsInitializeAndStartEpoch(%15, %8) : (i64, i32) -> i64
    %17 = llvm.alloca %0 x f64 : (i64) -> !llvm.ptr
    %18 = llvm.alloca %0 x f64 : (i64) -> !llvm.ptr
    llvm.br ^bb1(%1 : i64)
  ^bb1(%19: i64):  // 2 preds: ^bb0, ^bb2
    %20 = llvm.icmp "slt" %19, %0 : i64
    llvm.cond_br %20, ^bb2, ^bb3
  ^bb2:  // pred: ^bb1
    %21 = llvm.trunc %19 : i64 to i32
    %22 = llvm.sitofp %21 : i32 to f64
    %23 = llvm.getelementptr %17[%19] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    llvm.store %22, %23 : f64, !llvm.ptr
    %24 = llvm.getelementptr %18[%19] : (!llvm.ptr, i64) -> !llvm.ptr, f64
    llvm.store %5, %24 : f64, !llvm.ptr
    %25 = llvm.add %19, %2  : i64
    llvm.br ^bb1(%25 : i64)
  ^bb3:  // pred: ^bb1
    %26 = llvm.alloca %0 x !llvm.struct<(i64, ptr)> : (i64) -> !llvm.ptr
    %27 = llvm.alloca %0 x !llvm.struct<(i64, ptr)> : (i64) -> !llvm.ptr
    llvm.call @artsDbCreateArray(%26, %3, %4, %6, %17) : (!llvm.ptr, i64, i32, i32, !llvm.ptr) -> ()
    llvm.call @artsDbCreateArray(%27, %3, %4, %6, %18) : (!llvm.ptr, i64, i32, i32, !llvm.ptr) -> ()
    llvm.call @computeEDT(%6, %26, %27) : (i32, !llvm.ptr, !llvm.ptr) -> ()
    %28 = llvm.call @artsWaitOnHandle(%16) : (i64) -> i8
    %29 = llvm.mlir.addressof @str11 : !llvm.ptr
    %30 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<41 x i8>
    %31 = llvm.call @printf(%30) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    llvm.return
  }
  llvm.func @artsDbCreateArray(!llvm.ptr, i64, i32, i32, !llvm.ptr) attributes {sym_visibility = "private"}
  llvm.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: !llvm.ptr) {
    %0 = llvm.mlir.constant(1 : i32) : i32
    %1 = llvm.mlir.constant(0 : i32) : i32
    %2 = llvm.mlir.constant(0 : index) : i64
    %3 = llvm.alloca %2 x i64 : (i64) -> !llvm.ptr
    %4 = llvm.icmp "eq" %arg0, %1 : i32
    llvm.cond_br %4, ^bb1, ^bb2
  ^bb1:  // pred: ^bb0
    %5 = llvm.call @artsGetCurrentNode() : () -> i32
    %6 = llvm.mlir.addressof @mainEdt : !llvm.ptr
    %7 = llvm.call @artsEdtCreate(%6, %5, %0, %3, %1) : (!llvm.ptr, i32, i32, !llvm.ptr, i32) -> i64
    %8 = llvm.mlir.addressof @str12 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
    %10 = llvm.call @printf(%9, %arg0) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    llvm.br ^bb2
  ^bb2:  // 2 preds: ^bb0, ^bb1
    llvm.return
  }
  llvm.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: i32, %arg3: !llvm.ptr) {
    llvm.return
  }
  llvm.func @main(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
    %0 = llvm.mlir.constant(0 : i32) : i32
    %1 = llvm.call @artsRT(%arg0, %arg1) : (i32, !llvm.ptr) -> i32
    llvm.return %0 : i32
  }
  llvm.func @artsRT(i32, !llvm.ptr) -> i32 attributes {sym_visibility = "private"}
}

