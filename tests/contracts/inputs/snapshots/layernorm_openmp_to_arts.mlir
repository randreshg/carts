#loc39 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":74:3)
#loc48 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":83:3)
#loc57 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":92:3)
#loc66 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":99:3)
#loc77 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":22:13)
#loc81 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":24:3)
#loc83 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":25:5)
#loc91 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":30:3)
#loc99 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13)
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str4("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str3("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str2("allocation failure\0A\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global external @stderr() {addr_space = 0 : i32} : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>> loc(#loc)
  llvm.func @fprintf(!llvm.ptr, !llvm.ptr, ...) -> i32 loc(#loc)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc4)
  llvm.mlir.global internal constant @str0("layernorm\00") {addr_space = 0 : i32} loc(#loc5)
  func.func @main() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str4 : !llvm.ptr loc(#loc)
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst_0 = arith.constant 9.99999974E-6 : f32 loc(#loc)
    %c1024_i32 = arith.constant 1024 : i32 loc(#loc)
    %c16_i32 = arith.constant 16 : i32 loc(#loc)
    %false = arith.constant false loc(#loc)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %3 = llvm.mlir.addressof @stderr : !llvm.ptr loc(#loc)
    %4 = llvm.mlir.zero : !llvm.ptr loc(#loc)
    %c1024_i64 = arith.constant 1024 : i64 loc(#loc)
    %c16_i64 = arith.constant 16 : i64 loc(#loc)
    %5 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc)
    %7 = "polygeist.undef"() : () -> i32 loc(#loc7)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc8)
    %8 = "polygeist.undef"() : () -> f64 loc(#loc8)
    affine.store %8, %alloca[0] : memref<1xf64> loc(#loc8)
    %alloca_1 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc9)
    %alloca_2 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc10)
    %alloca_3 = memref.alloca() : memref<1xmemref<?xmemref<?xf32>>> loc(#loc11)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc12)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %13 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc13)
        func.call @carts_e2e_timer_start(%13) : (memref<?xi8>) -> () loc(#loc13)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %13 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8> loc(#loc14)
        %14 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc14)
        func.call @carts_phase_timer_start(%13, %14) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc14)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %13 = scf.execute_region -> memref<?xmemref<?xf32>> {
            %14 = polygeist.typeSize memref<?xf32> : index loc(#loc15)
            %15 = arith.index_cast %14 : index to i64 loc(#loc16)
            %16 = arith.muli %15, %c16_i64 : i64 loc(#loc17)
            %17 = arith.index_cast %16 : i64 to index loc(#loc15)
            %18 = arith.divui %17, %14 : index loc(#loc15)
            %alloc = memref.alloc(%18) : memref<?xmemref<?xf32>> loc(#loc15)
            affine.store %alloc, %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc11)
            scf.yield %alloc : memref<?xmemref<?xf32>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %13 = scf.execute_region -> memref<?xf32> {
            %14 = polygeist.typeSize f32 : index loc(#loc18)
            %15 = arith.index_cast %14 : index to i64 loc(#loc19)
            %16 = arith.muli %15, %c1024_i64 : i64 loc(#loc20)
            %17 = arith.index_cast %16 : i64 to index loc(#loc18)
            %18 = arith.divui %17, %14 : index loc(#loc18)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc18)
            affine.store %alloc, %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc10)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %13 = scf.execute_region -> memref<?xf32> {
            %14 = polygeist.typeSize f32 : index loc(#loc21)
            %15 = arith.index_cast %14 : index to i64 loc(#loc22)
            %16 = arith.muli %15, %c1024_i64 : i64 loc(#loc23)
            %17 = arith.index_cast %16 : i64 to index loc(#loc21)
            %18 = arith.divui %17, %14 : index loc(#loc21)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc21)
            affine.store %alloc, %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc9)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %9:2 = scf.if %true -> (i1, i32) {
      %13:2 = scf.execute_region -> (i1, i32) {
        %14:2 = scf.if %true -> (i1, i32) {
          %15:2 = scf.execute_region -> (i1, i32) {
            %16 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc24)
            %17 = polygeist.memref2pointer %16 : memref<?xmemref<?xf32>> to !llvm.ptr loc(#loc25)
            %18 = llvm.icmp "eq" %17, %4 : !llvm.ptr loc(#loc25)
            %19 = scf.if %18 -> (i1) {
              scf.yield %true : i1 loc(#loc26)
            } else {
              %22 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc27)
              %23 = polygeist.memref2pointer %22 : memref<?xf32> to !llvm.ptr loc(#loc28)
              %24 = llvm.icmp "eq" %23, %4 : !llvm.ptr loc(#loc28)
              scf.yield %24 : i1 loc(#loc26)
            } loc(#loc26)
            %20 = scf.if %19 -> (i1) {
              scf.yield %true : i1 loc(#loc29)
            } else {
              %22 = affine.load %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc30)
              %23 = polygeist.memref2pointer %22 : memref<?xf32> to !llvm.ptr loc(#loc31)
              %24 = llvm.icmp "eq" %23, %4 : !llvm.ptr loc(#loc31)
              scf.yield %24 : i1 loc(#loc29)
            } loc(#loc29)
            %21:2 = scf.if %20 -> (i1, i32) {
              scf.if %true {
                scf.execute_region {
                  %23 = llvm.load %3 : !llvm.ptr -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>> loc(#loc33)
                  %24 = polygeist.memref2pointer %23 : memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>> to !llvm.ptr loc(#loc33)
                  %25 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<20 x i8> loc(#loc3)
                  %26 = llvm.call @fprintf(%24, %25) vararg(!llvm.func<i32 (ptr, ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32 loc(#loc34)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              %22:2 = scf.if %true -> (i1, i32) {
                %23:2 = scf.execute_region -> (i1, i32) {
                  %24:2 = scf.if %true -> (i1, i32) {
                    %25:2 = scf.execute_region -> (i1, i32) {
                      scf.yield %false, %c1_i32 : i1, i32 loc(#loc)
                    } loc(#loc)
                    scf.yield %25#0, %25#1 : i1, i32 loc(#loc)
                  } else {
                    scf.yield %true, %7 : i1, i32 loc(#loc)
                  } loc(#loc)
                  scf.yield %24#0, %24#1 : i1, i32 loc(#loc)
                } loc(#loc)
                scf.yield %23#0, %23#1 : i1, i32 loc(#loc)
              } else {
                scf.yield %true, %7 : i1, i32 loc(#loc)
              } loc(#loc)
              scf.yield %22#0, %22#1 : i1, i32 loc(#loc32)
            } else {
              scf.yield %true, %7 : i1, i32 loc(#loc32)
            } loc(#loc32)
            scf.yield %21#0, %21#1 : i1, i32 loc(#loc)
          } loc(#loc)
          scf.yield %15#0, %15#1 : i1, i32 loc(#loc)
        } else {
          scf.yield %true, %7 : i1, i32 loc(#loc)
        } loc(#loc)
        scf.yield %14#0, %14#1 : i1, i32 loc(#loc)
      } loc(#loc)
      scf.yield %13#0, %13#1 : i1, i32 loc(#loc)
    } else {
      scf.yield %true, %7 : i1, i32 loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        scf.if %9#0 {
          scf.execute_region {
            %13 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc35)
            %14 = polygeist.typeSize f32 : index loc(#loc36)
            %15 = arith.index_cast %14 : index to i64 loc(#loc37)
            %16 = arith.muli %15, %c1024_i64 : i64 loc(#loc38)
            %17 = arith.index_cast %16 : i64 to index loc(#loc36)
            %18 = arith.divui %17, %14 : index loc(#loc36)
            affine.for %arg0 loc("benchmarks/ml-kernels/layernorm/layernorm.c":74:3) = 0 to 16 {
              scf.if %9#0 {
                scf.execute_region {
                  %19 = polygeist.subindex %13[%arg0] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc35)
                  %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc36)
                  affine.store %alloc, %19[0] : memref<?xmemref<?xf32>> loc(#loc40)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc39)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc41)
        %14 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc42)
        %15 = affine.load %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc43)
        func.call @init(%13, %14, %15) : (memref<?xmemref<?xf32>>, memref<?xf32>, memref<?xf32>) -> () loc(#loc44)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %5 : !llvm.ptr to memref<?xi8> loc(#loc45)
        func.call @carts_phase_timer_stop(%13) : (memref<?xi8>) -> () loc(#loc45)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc46)
        func.call @carts_kernel_timer_start(%13) : (memref<?xi8>) -> () loc(#loc46)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        scf.if %9#0 {
          scf.execute_region {
            %13 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc47)
            affine.for %arg0 loc("benchmarks/ml-kernels/layernorm/layernorm.c":83:3) = 0 to 1 {
              scf.if %9#0 {
                scf.execute_region {
                  %14 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc49)
                  %15 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc50)
                  %16 = affine.load %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc51)
                  func.call @layernorm_forward(%14, %15, %16, %c16_i32, %c1024_i32, %cst_0) : (memref<?xmemref<?xf32>>, memref<?xf32>, memref<?xf32>, i32, i32, f32) -> () loc(#loc52)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %9#0 {
                scf.execute_region {
                  func.call @carts_kernel_timer_accum(%13) : (memref<?xi8>) -> () loc(#loc47)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc48)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc53)
        func.call @carts_kernel_timer_print(%13) : (memref<?xi8>) -> () loc(#loc53)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc54)
        %14 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc54)
        func.call @carts_phase_timer_start(%13, %14) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc54)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        scf.if %9#0 {
          scf.execute_region {
            affine.store %cst, %alloca[0] : memref<1xf64> loc(#loc8)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %10 = scf.if %9#0 -> (i32) {
      %13 = scf.execute_region -> i32 {
        %14 = scf.if %9#0 -> (i32) {
          %15 = scf.execute_region -> i32 {
            %16 = scf.if %true -> (i32) {
              scf.yield %c16_i32 : i32 loc(#loc55)
            } else {
              scf.yield %c1024_i32 : i32 loc(#loc55)
            } loc(#loc55)
            scf.yield %16 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %15 : i32 loc(#loc)
        } else {
          scf.yield %7 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %14 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %13 : i32 loc(#loc)
    } else {
      scf.yield %7 : i32 loc(#loc)
    } loc(#loc)
    %11 = arith.index_cast %10 : i32 to index loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        scf.if %9#0 {
          scf.execute_region {
            %13 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc56)
            affine.for %arg0 loc("benchmarks/ml-kernels/layernorm/layernorm.c":92:3) = 0 to %11 {
              scf.if %9#0 {
                scf.execute_region {
                  %14 = polygeist.subindex %13[%arg0] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc56)
                  %15 = affine.load %14[0] : memref<?xmemref<?xf32>> loc(#loc56)
                  %16 = polygeist.subindex %15[%arg0] () : memref<?xf32> -> memref<?xf32> loc(#loc56)
                  %17 = affine.load %16[0] : memref<?xf32> loc(#loc56)
                  %18 = arith.extf %17 : f32 to f64 loc(#loc58)
                  %19 = math.absf %18 : f64 loc(#loc59)
                  %20 = affine.load %alloca[0] : memref<1xf64> loc(#loc60)
                  %21 = arith.addf %20, %19 : f64 loc(#loc60)
                  affine.store %21, %alloca[0] : memref<1xf64> loc(#loc60)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc57)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = affine.load %alloca[0] : memref<1xf64> loc(#loc61)
        func.call @carts_bench_checksum_d(%13) : (f64) -> () loc(#loc62)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc63)
        func.call @carts_phase_timer_stop(%13) : (memref<?xi8>) -> () loc(#loc63)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc64)
        %14 = polygeist.pointer2memref %6 : !llvm.ptr to memref<?xi8> loc(#loc64)
        func.call @carts_phase_timer_start(%13, %14) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc64)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        scf.if %9#0 {
          scf.execute_region {
            %13 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc65)
            affine.for %arg0 loc("benchmarks/ml-kernels/layernorm/layernorm.c":99:3) = 0 to 16 {
              scf.if %9#0 {
                scf.execute_region {
                  %14 = polygeist.subindex %13[%arg0] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc65)
                  %15 = affine.load %14[0] : memref<?xmemref<?xf32>> loc(#loc67)
                  memref.dealloc %15 : memref<?xf32> loc(#loc67)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc66)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc68)
        memref.dealloc %13 : memref<?xmemref<?xf32>> loc(#loc68)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc69)
        memref.dealloc %13 : memref<?xf32> loc(#loc69)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = affine.load %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc70)
        memref.dealloc %13 : memref<?xf32> loc(#loc70)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        %13 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc71)
        func.call @carts_phase_timer_stop(%13) : (memref<?xi8>) -> () loc(#loc71)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %9#0 {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc72)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %12 = scf.if %9#0 -> (i32) {
      %13 = scf.execute_region -> i32 {
        %14 = scf.if %9#0 -> (i32) {
          %15 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %15 : i32 loc(#loc)
        } else {
          scf.yield %9#1 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %14 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %13 : i32 loc(#loc)
    } else {
      scf.yield %9#1 : i32 loc(#loc)
    } loc(#loc)
    return %12 : i32 loc(#loc73)
  } loc(#loc6)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc74)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc75)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc76)
  func.func private @init(%arg0: memref<?xmemref<?xf32>> loc("benchmarks/ml-kernels/layernorm/layernorm.c":22:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/layernorm/layernorm.c":22:13), %arg2: memref<?xf32> loc("benchmarks/ml-kernels/layernorm/layernorm.c":22:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 0.000000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 1.000000e+00 : f32 loc(#loc)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %cst_1 = arith.constant 3.125000e-02 : f32 loc(#loc)
    %cst_2 = arith.constant 5.000000e+01 : f32 loc(#loc)
    %c113_i32 = arith.constant 113 : i32 loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %true = arith.constant true loc(#loc78)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc79)
    %alloca = memref.alloca() : memref<1xi32> loc(#loc80)
    affine.store %0, %alloca[0] : memref<1xi32> loc(#loc80)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.store %c0_i32, %alloca[0] : memref<1xi32> loc(#loc80)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.for %arg3 loc("benchmarks/ml-kernels/layernorm/layernorm.c":24:3) = 0 to 16 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %1 = polygeist.subindex %arg0[%arg3] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc82)
                      affine.for %arg4 loc("benchmarks/ml-kernels/layernorm/layernorm.c":25:5) = 0 to 1024 {
                        scf.if %true {
                          scf.execute_region {
                            %2 = affine.load %1[0] : memref<?xmemref<?xf32>> loc(#loc82)
                            %3 = polygeist.subindex %2[%arg4] () : memref<?xf32> -> memref<?xf32> loc(#loc82)
                            %4 = affine.load %alloca[0] : memref<1xi32> loc(#loc84)
                            %5 = arith.remsi %4, %c113_i32 : i32 loc(#loc85)
                            %6 = arith.sitofp %5 : i32 to f32 loc(#loc86)
                            %7 = arith.subf %6, %cst_2 : f32 loc(#loc87)
                            %8 = arith.mulf %7, %cst_1 : f32 loc(#loc88)
                            affine.store %8, %3[0] : memref<?xf32> loc(#loc89)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                        scf.if %true {
                          scf.execute_region {
                            %2 = affine.load %alloca[0] : memref<1xi32> loc(#loc90)
                            %3 = arith.addi %2, %c1_i32 : i32 loc(#loc90)
                            affine.store %3, %alloca[0] : memref<1xi32> loc(#loc90)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc83)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc81)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.for %arg3 loc("benchmarks/ml-kernels/layernorm/layernorm.c":30:3) = 0 to 1024 {
              scf.if %true {
                scf.execute_region {
                  %1 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc92)
                  affine.store %cst_0, %1[0] : memref<?xf32> loc(#loc93)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %1 = polygeist.subindex %arg2[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc94)
                  affine.store %cst, %1[0] : memref<?xf32> loc(#loc95)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc91)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc96)
  } loc(#loc77)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc97)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc98)
  func.func private @layernorm_forward(%arg0: memref<?xmemref<?xf32>> loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13), %arg2: memref<?xf32> loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13), %arg3: i32 loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13), %arg4: i32 loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13), %arg5: f32 loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst = arith.constant 1.000000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 0.000000e+00 : f32 loc(#loc)
    %true = arith.constant true loc(#loc100)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc101)
    %1 = "polygeist.undef"() : () -> f32 loc(#loc102)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %2 = scf.if %true -> (i32) {
              %4 = scf.execute_region -> i32 {
                scf.yield %arg3 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %4 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %3 = arith.index_cast %2 : i32 to index loc(#loc103)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg6) : index = (%c0) to (%3) step (%c1) {
                    scf.execute_region {
                      %alloca = memref.alloca() : memref<1xf32> loc(#loc104)
                      affine.store %1, %alloca[0] : memref<1xf32> loc(#loc104)
                      %alloca_1 = memref.alloca() : memref<1xf32> loc(#loc105)
                      affine.store %1, %alloca_1[0] : memref<1xf32> loc(#loc105)
                      %alloca_2 = memref.alloca() : memref<1xf32> loc(#loc106)
                      affine.store %1, %alloca_2[0] : memref<1xf32> loc(#loc106)
                      %alloca_3 = memref.alloca() : memref<1xf32> loc(#loc107)
                      affine.store %1, %alloca_3[0] : memref<1xf32> loc(#loc107)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              affine.store %cst_0, %alloca_3[0] : memref<1xf32> loc(#loc107)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              affine.store %cst_0, %alloca_2[0] : memref<1xf32> loc(#loc106)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = arith.index_cast %arg4 : i32 to index loc(#loc108)
                              %6 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc109)
                              scf.for %arg7 = %c0 to %5 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    %7 = affine.load %6[0] : memref<?xmemref<?xf32>> loc(#loc109)
                                    %8 = polygeist.subindex %7[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc109)
                                    %9 = affine.load %8[0] : memref<?xf32> loc(#loc109)
                                    %10 = affine.load %alloca_3[0] : memref<1xf32> loc(#loc111)
                                    %11 = arith.addf %10, %9 : f32 loc(#loc111)
                                    affine.store %11, %alloca_3[0] : memref<1xf32> loc(#loc111)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc110)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        %5 = scf.execute_region -> f32 {
                          %6 = arith.sitofp %arg4 : i32 to f32 loc(#loc112)
                          %7 = affine.load %alloca_3[0] : memref<1xf32> loc(#loc113)
                          %8 = arith.divf %7, %6 : f32 loc(#loc113)
                          affine.store %8, %alloca_3[0] : memref<1xf32> loc(#loc113)
                          scf.yield %8 : f32 loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = arith.index_cast %arg4 : i32 to index loc(#loc114)
                              %6 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc115)
                              %7 = affine.load %alloca_3[0] : memref<1xf32> loc(#loc116)
                              scf.for %arg7 = %c0 to %5 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      %8 = scf.execute_region -> f32 {
                                        %9 = affine.load %6[0] : memref<?xmemref<?xf32>> loc(#loc115)
                                        %10 = polygeist.subindex %9[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc115)
                                        %11 = affine.load %10[0] : memref<?xf32> loc(#loc115)
                                        %12 = arith.subf %11, %7 : f32 loc(#loc118)
                                        affine.store %12, %alloca_1[0] : memref<1xf32> loc(#loc105)
                                        scf.yield %12 : f32 loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                                scf.if %true {
                                  scf.execute_region {
                                    %8 = affine.load %alloca_1[0] : memref<1xf32> loc(#loc119)
                                    %9 = arith.mulf %8, %8 : f32 loc(#loc120)
                                    %10 = affine.load %alloca_2[0] : memref<1xf32> loc(#loc121)
                                    %11 = arith.addf %10, %9 : f32 loc(#loc121)
                                    affine.store %11, %alloca_2[0] : memref<1xf32> loc(#loc121)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc117)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        %5 = scf.execute_region -> f32 {
                          %6 = affine.load %alloca_2[0] : memref<1xf32> loc(#loc122)
                          %7 = arith.sitofp %arg4 : i32 to f32 loc(#loc123)
                          %8 = arith.divf %6, %7 : f32 loc(#loc124)
                          affine.store %8, %alloca_2[0] : memref<1xf32> loc(#loc125)
                          scf.yield %8 : f32 loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      %4 = scf.if %true -> (f32) {
                        %5 = scf.execute_region -> f32 {
                          %6 = scf.if %true -> (f32) {
                            %7 = scf.execute_region -> f32 {
                              %8 = affine.load %alloca_2[0] : memref<1xf32> loc(#loc126)
                              %9 = arith.addf %8, %arg5 : f32 loc(#loc127)
                              %10 = math.sqrt %9 : f32 loc(#loc128)
                              %11 = arith.divf %cst, %10 : f32 loc(#loc129)
                              scf.yield %11 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %7 : f32 loc(#loc)
                          } else {
                            scf.yield %1 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %6 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %5 : f32 loc(#loc)
                      } else {
                        scf.yield %1 : f32 loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = arith.index_cast %arg4 : i32 to index loc(#loc130)
                              %6 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc131)
                              %7 = affine.load %alloca_3[0] : memref<1xf32> loc(#loc132)
                              %8 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc133)
                              scf.for %arg7 = %c0 to %5 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      %9 = scf.execute_region -> f32 {
                                        %10 = affine.load %6[0] : memref<?xmemref<?xf32>> loc(#loc131)
                                        %11 = polygeist.subindex %10[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc131)
                                        %12 = affine.load %11[0] : memref<?xf32> loc(#loc131)
                                        %13 = arith.subf %12, %7 : f32 loc(#loc135)
                                        %14 = arith.mulf %13, %4 : f32 loc(#loc136)
                                        affine.store %14, %alloca[0] : memref<1xf32> loc(#loc104)
                                        scf.yield %14 : f32 loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                                scf.if %true {
                                  scf.execute_region {
                                    %9 = affine.load %8[0] : memref<?xmemref<?xf32>> loc(#loc133)
                                    %10 = polygeist.subindex %9[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc133)
                                    %11 = affine.load %alloca[0] : memref<1xf32> loc(#loc137)
                                    %12 = polygeist.subindex %arg1[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc138)
                                    %13 = affine.load %12[0] : memref<?xf32> loc(#loc138)
                                    %14 = arith.mulf %11, %13 : f32 loc(#loc139)
                                    %15 = polygeist.subindex %arg2[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc140)
                                    %16 = affine.load %15[0] : memref<?xf32> loc(#loc140)
                                    %17 = arith.addf %14, %16 : f32 loc(#loc141)
                                    affine.store %17, %10[0] : memref<?xf32> loc(#loc142)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc134)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc103)
                    } loc(#loc103)
                    omp.yield loc(#loc103)
                  } loc(#loc103)
                } loc(#loc103)
                scf.yield loc(#loc103)
              } loc(#loc103)
              omp.terminator loc(#loc103)
            } loc(#loc103)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc143)
  } loc(#loc99)
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc144)
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc145)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc146)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc147)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":70:21)
#loc4 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc5 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":61:25)
#loc6 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":59:5)
#loc7 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":99:8)
#loc8 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":90:3)
#loc9 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":67:3)
#loc10 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":66:3)
#loc11 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":65:3)
#loc12 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc13 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc14 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc15 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":65:15)
#loc16 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":65:40)
#loc17 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":65:38)
#loc18 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":66:18)
#loc19 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":66:34)
#loc20 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":66:48)
#loc21 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":67:17)
#loc22 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":67:33)
#loc23 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":67:47)
#loc24 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:8)
#loc25 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:7)
#loc26 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:10)
#loc27 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:14)
#loc28 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:13)
#loc29 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:20)
#loc30 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:24)
#loc31 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:23)
#loc32 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":69:3)
#loc33 = loc("/usr/include/stdio.h":149:16)
#loc34 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":70:5)
#loc35 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":75:5)
#loc36 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":75:12)
#loc37 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":75:37)
#loc38 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":75:35)
#loc40 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":75:10)
#loc41 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":78:8)
#loc42 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":78:11)
#loc43 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":78:18)
#loc44 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":78:3)
#loc45 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc46 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc47 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":141:40)
#loc49 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":84:23)
#loc50 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":84:26)
#loc51 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":84:33)
#loc52 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":84:5)
#loc53 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":142:40)
#loc54 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc55 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":7:15)
#loc56 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":93:36)
#loc58 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":93:28)
#loc59 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":93:23)
#loc60 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":93:20)
#loc61 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":95:24)
#loc62 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc63 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc64 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc65 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":100:10)
#loc67 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":100:5)
#loc68 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":102:3)
#loc69 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":103:3)
#loc70 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":104:3)
#loc71 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc72 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc73 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":110:1)
#loc74 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc75 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc76 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc78 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":22:1)
#loc79 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":30:8)
#loc80 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":23:3)
#loc82 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:7)
#loc84 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:26)
#loc85 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:30)
#loc86 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:18)
#loc87 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:37)
#loc88 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:46)
#loc89 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":26:15)
#loc90 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":27:10)
#loc92 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":31:5)
#loc93 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":31:14)
#loc94 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":32:5)
#loc95 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":32:13)
#loc96 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":34:1)
#loc97 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc98 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc100 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":36:1)
#loc101 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":39:3)
#loc102 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":37:54)
#loc103 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":38:1)
#loc104 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":53:7)
#loc105 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":47:7)
#loc106 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":41:5)
#loc107 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":40:5)
#loc108 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":42:23)
#loc109 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":43:15)
#loc110 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":42:5)
#loc111 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":43:12)
#loc112 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":45:13)
#loc113 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":45:10)
#loc114 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":46:23)
#loc115 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":47:20)
#loc116 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":47:30)
#loc117 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":46:5)
#loc118 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":47:28)
#loc119 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":48:14)
#loc120 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":48:19)
#loc121 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":48:11)
#loc122 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":50:11)
#loc123 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":50:17)
#loc124 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":50:15)
#loc125 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":50:9)
#loc126 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":51:34)
#loc127 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":51:38)
#loc128 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":51:28)
#loc129 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":51:26)
#loc130 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":52:23)
#loc131 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":53:21)
#loc132 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":53:31)
#loc133 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:7)
#loc134 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":52:5)
#loc135 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":53:29)
#loc136 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":53:37)
#loc137 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:17)
#loc138 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:24)
#loc139 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:22)
#loc140 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:35)
#loc141 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:33)
#loc142 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":54:15)
#loc143 = loc("benchmarks/ml-kernels/layernorm/layernorm.c":57:1)
#loc144 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":134:6)
#loc145 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":137:6)
#loc146 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc147 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
