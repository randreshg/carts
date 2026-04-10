#loc5 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":244:5)
#loc61 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":263:3)
#loc66 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":266:5)
#loc76 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":279:3)
#loc91 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":292:3)
#loc101 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":300:3)
#loc102 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":301:5)
#loc121 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13)
#loc128 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":228:3)
#loc130 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":229:5)
#loc139 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":238:3)
#loc147 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13)
#loc168 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:13)
#loc188 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13)
#loc214 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13)
#loc232 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:13)
#loc244 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:13)
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("batchnorm\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":244:5), %arg1: memref<?xmemref<?xi8>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":244:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %c32_i32 = arith.constant 32 : i32 loc(#loc)
    %c64_i32 = arith.constant 64 : i32 loc(#loc)
    %c4_i32 = arith.constant 4 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc6)
    %4 = "polygeist.undef"() : () -> i32 loc(#loc7)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc8)
    %5 = "polygeist.undef"() : () -> f64 loc(#loc8)
    affine.store %5, %alloca[0] : memref<1xf64> loc(#loc8)
    %alloca_0 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc9)
    %alloca_1 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc10)
    %alloca_2 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc11)
    %alloca_3 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc12)
    %alloca_4 = memref.alloca() : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc13)
    %alloca_5 = memref.alloca() : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc14)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc15)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc16)
        func.call @carts_e2e_timer_start(%17) : (memref<?xi8>) -> () loc(#loc16)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc17)
        %18 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc17)
        func.call @carts_phase_timer_start(%17, %18) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc17)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %6 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            scf.yield %c4_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %7 = arith.index_cast %6 : i32 to index loc(#loc)
    %8 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            scf.yield %c64_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %9 = arith.index_cast %8 : i32 to index loc(#loc)
    %10 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            scf.yield %c32_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %11 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            %20 = arith.muli %10, %10 : i32 loc(#loc18)
            scf.yield %20 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %17 = scf.execute_region -> memref<?xmemref<?xmemref<?xf32>>> {
            %18 = polygeist.typeSize memref<?xmemref<?xf32>> : index loc(#loc19)
            %19 = arith.extsi %6 : i32 to i64 loc(#loc20)
            %20 = arith.index_cast %18 : index to i64 loc(#loc21)
            %21 = arith.muli %19, %20 : i64 loc(#loc22)
            %22 = arith.index_cast %21 : i64 to index loc(#loc19)
            %23 = arith.divui %22, %18 : index loc(#loc19)
            %alloc = memref.alloc(%23) : memref<?xmemref<?xmemref<?xf32>>> loc(#loc19)
            affine.store %alloc, %alloca_5[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc14)
            scf.yield %alloc : memref<?xmemref<?xmemref<?xf32>>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %17 = scf.execute_region -> memref<?xmemref<?xmemref<?xf32>>> {
            %18 = polygeist.typeSize memref<?xmemref<?xf32>> : index loc(#loc23)
            %19 = arith.extsi %6 : i32 to i64 loc(#loc24)
            %20 = arith.index_cast %18 : index to i64 loc(#loc25)
            %21 = arith.muli %19, %20 : i64 loc(#loc26)
            %22 = arith.index_cast %21 : i64 to index loc(#loc23)
            %23 = arith.divui %22, %18 : index loc(#loc23)
            %alloc = memref.alloc(%23) : memref<?xmemref<?xmemref<?xf32>>> loc(#loc23)
            affine.store %alloc, %alloca_4[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc13)
            scf.yield %alloc : memref<?xmemref<?xmemref<?xf32>>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %17 = scf.execute_region -> memref<?xf32> {
            %18 = polygeist.typeSize f32 : index loc(#loc27)
            %19 = arith.extsi %8 : i32 to i64 loc(#loc28)
            %20 = arith.index_cast %18 : index to i64 loc(#loc29)
            %21 = arith.muli %19, %20 : i64 loc(#loc30)
            %22 = arith.index_cast %21 : i64 to index loc(#loc27)
            %23 = arith.divui %22, %18 : index loc(#loc27)
            %alloc = memref.alloc(%23) : memref<?xf32> loc(#loc27)
            affine.store %alloc, %alloca_3[0] : memref<1xmemref<?xf32>> loc(#loc12)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %17 = scf.execute_region -> memref<?xf32> {
            %18 = polygeist.typeSize f32 : index loc(#loc31)
            %19 = arith.extsi %8 : i32 to i64 loc(#loc32)
            %20 = arith.index_cast %18 : index to i64 loc(#loc33)
            %21 = arith.muli %19, %20 : i64 loc(#loc34)
            %22 = arith.index_cast %21 : i64 to index loc(#loc31)
            %23 = arith.divui %22, %18 : index loc(#loc31)
            %alloc = memref.alloc(%23) : memref<?xf32> loc(#loc31)
            affine.store %alloc, %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc11)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %17 = scf.execute_region -> memref<?xf32> {
            %18 = polygeist.typeSize f32 : index loc(#loc35)
            %19 = arith.extsi %8 : i32 to i64 loc(#loc36)
            %20 = arith.index_cast %18 : index to i64 loc(#loc37)
            %21 = arith.muli %19, %20 : i64 loc(#loc38)
            %22 = arith.index_cast %21 : i64 to index loc(#loc35)
            %23 = arith.divui %22, %18 : index loc(#loc35)
            %alloc = memref.alloc(%23) : memref<?xf32> loc(#loc35)
            affine.store %alloc, %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc10)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %17 = scf.execute_region -> memref<?xf32> {
            %18 = polygeist.typeSize f32 : index loc(#loc39)
            %19 = arith.extsi %8 : i32 to i64 loc(#loc40)
            %20 = arith.index_cast %18 : index to i64 loc(#loc41)
            %21 = arith.muli %19, %20 : i64 loc(#loc42)
            %22 = arith.index_cast %21 : i64 to index loc(#loc39)
            %23 = arith.divui %22, %18 : index loc(#loc39)
            %alloc = memref.alloc(%23) : memref<?xf32> loc(#loc39)
            affine.store %alloc, %alloca_0[0] : memref<1xmemref<?xf32>> loc(#loc9)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %17 = affine.load %alloca_5[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc43)
            %18 = polygeist.typeSize memref<?xf32> : index loc(#loc44)
            %19 = arith.extsi %8 : i32 to i64 loc(#loc45)
            %20 = arith.index_cast %18 : index to i64 loc(#loc46)
            %21 = arith.muli %19, %20 : i64 loc(#loc47)
            %22 = arith.index_cast %21 : i64 to index loc(#loc44)
            %23 = arith.divui %22, %18 : index loc(#loc44)
            %24 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc48)
            %25 = polygeist.typeSize memref<?xf32> : index loc(#loc49)
            %26 = arith.extsi %8 : i32 to i64 loc(#loc50)
            %27 = arith.index_cast %25 : index to i64 loc(#loc51)
            %28 = arith.muli %26, %27 : i64 loc(#loc52)
            %29 = arith.index_cast %28 : i64 to index loc(#loc49)
            %30 = arith.divui %29, %25 : index loc(#loc49)
            %31 = polygeist.typeSize f32 : index loc(#loc53)
            %32 = arith.extsi %11 : i32 to i64 loc(#loc54)
            %33 = arith.index_cast %31 : index to i64 loc(#loc55)
            %34 = arith.muli %32, %33 : i64 loc(#loc56)
            %35 = arith.index_cast %34 : i64 to index loc(#loc53)
            %36 = arith.divui %35, %31 : index loc(#loc53)
            %37 = polygeist.typeSize f32 : index loc(#loc57)
            %38 = arith.extsi %11 : i32 to i64 loc(#loc58)
            %39 = arith.index_cast %37 : index to i64 loc(#loc59)
            %40 = arith.muli %38, %39 : i64 loc(#loc60)
            %41 = arith.index_cast %40 : i64 to index loc(#loc57)
            %42 = arith.divui %41, %37 : index loc(#loc57)
            affine.for %arg2 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":263:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %43 = polygeist.subindex %17[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc43)
                  %alloc = memref.alloc(%23) : memref<?xmemref<?xf32>> loc(#loc44)
                  affine.store %alloc, %43[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc62)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %43 = polygeist.subindex %24[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc48)
                  %alloc = memref.alloc(%30) : memref<?xmemref<?xf32>> loc(#loc49)
                  affine.store %alloc, %43[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc63)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %43 = polygeist.subindex %17[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc64)
                      %44 = polygeist.subindex %24[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc65)
                      affine.for %arg3 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":266:5) = 0 to %9 {
                        scf.if %true {
                          scf.execute_region {
                            %45 = affine.load %43[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc64)
                            %46 = polygeist.subindex %45[%arg3] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc64)
                            %alloc = memref.alloc(%36) : memref<?xf32> loc(#loc53)
                            affine.store %alloc, %46[0] : memref<?xmemref<?xf32>> loc(#loc67)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                        scf.if %true {
                          scf.execute_region {
                            %45 = affine.load %44[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc65)
                            %46 = polygeist.subindex %45[%arg3] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc65)
                            %alloc = memref.alloc(%42) : memref<?xf32> loc(#loc57)
                            affine.store %alloc, %46[0] : memref<?xmemref<?xf32>> loc(#loc68)
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
            } loc(#loc61)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_5[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc69)
        %18 = affine.load %alloca_3[0] : memref<1xmemref<?xf32>> loc(#loc70)
        %19 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc71)
        func.call @init_data(%17, %18, %19, %6, %8, %11) : (memref<?xmemref<?xmemref<?xf32>>>, memref<?xf32>, memref<?xf32>, i32, i32, i32) -> () loc(#loc72)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc73)
        func.call @carts_phase_timer_stop(%17) : (memref<?xi8>) -> () loc(#loc73)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc74)
        func.call @carts_kernel_timer_start(%17) : (memref<?xi8>) -> () loc(#loc74)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %17 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc75)
            affine.for %arg2 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":279:3) = 0 to 1 {
              scf.if %true {
                scf.execute_region {
                  %18 = affine.load %alloca_5[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc77)
                  %19 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc78)
                  %20 = affine.load %alloca_3[0] : memref<1xmemref<?xf32>> loc(#loc79)
                  %21 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc80)
                  %22 = affine.load %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc81)
                  %23 = affine.load %alloca_0[0] : memref<1xmemref<?xf32>> loc(#loc82)
                  func.call @batchnorm_forward(%18, %19, %20, %21, %6, %8, %11, %22, %23) : (memref<?xmemref<?xmemref<?xf32>>>, memref<?xmemref<?xmemref<?xf32>>>, memref<?xf32>, memref<?xf32>, i32, i32, i32, memref<?xf32>, memref<?xf32>) -> () loc(#loc83)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  func.call @carts_kernel_timer_accum(%17) : (memref<?xi8>) -> () loc(#loc75)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc76)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc84)
        func.call @carts_kernel_timer_print(%17) : (memref<?xi8>) -> () loc(#loc84)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc85)
        %18 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc85)
        func.call @carts_phase_timer_start(%17, %18) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc85)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.store %cst, %alloca[0] : memref<1xf64> loc(#loc8)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %12 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            scf.yield %6 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %13 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            %20 = arith.cmpi slt, %8, %12 : i32 loc(#loc86)
            %21 = scf.if %20 -> (i32) {
              scf.yield %8 : i32 loc(#loc87)
            } else {
              scf.yield %12 : i32 loc(#loc87)
            } loc(#loc87)
            scf.yield %21 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %12 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %12 : i32 loc(#loc)
    } loc(#loc)
    %14 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            %20 = arith.cmpi slt, %11, %13 : i32 loc(#loc88)
            %21 = scf.if %20 -> (i32) {
              scf.yield %11 : i32 loc(#loc89)
            } else {
              scf.yield %13 : i32 loc(#loc89)
            } loc(#loc89)
            scf.yield %21 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %13 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %13 : i32 loc(#loc)
    } loc(#loc)
    %15 = arith.index_cast %14 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %17 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc90)
            affine.for %arg2 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":292:3) = 0 to %15 {
              scf.if %true {
                scf.execute_region {
                  %18 = polygeist.subindex %17[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc90)
                  %19 = affine.load %18[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc90)
                  %20 = polygeist.subindex %19[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc90)
                  %21 = affine.load %20[0] : memref<?xmemref<?xf32>> loc(#loc90)
                  %22 = polygeist.subindex %21[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc90)
                  %23 = affine.load %22[0] : memref<?xf32> loc(#loc90)
                  %24 = arith.extf %23 : f32 to f64 loc(#loc92)
                  %25 = math.absf %24 : f64 loc(#loc93)
                  %26 = affine.load %alloca[0] : memref<1xf64> loc(#loc94)
                  %27 = arith.addf %26, %25 : f64 loc(#loc94)
                  affine.store %27, %alloca[0] : memref<1xf64> loc(#loc94)
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
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca[0] : memref<1xf64> loc(#loc95)
        func.call @carts_bench_checksum_d(%17) : (f64) -> () loc(#loc96)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc97)
        func.call @carts_phase_timer_stop(%17) : (memref<?xi8>) -> () loc(#loc97)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc98)
        %18 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc98)
        func.call @carts_phase_timer_start(%17, %18) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc98)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %17 = affine.load %alloca_5[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc99)
            %18 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc100)
            affine.for %arg2 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":300:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %19 = polygeist.subindex %17[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc99)
                      %20 = polygeist.subindex %18[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc100)
                      affine.for %arg3 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":301:5) = 0 to %9 {
                        scf.if %true {
                          scf.execute_region {
                            %21 = affine.load %19[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc99)
                            %22 = polygeist.subindex %21[%arg3] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc99)
                            %23 = affine.load %22[0] : memref<?xmemref<?xf32>> loc(#loc103)
                            memref.dealloc %23 : memref<?xf32> loc(#loc103)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                        scf.if %true {
                          scf.execute_region {
                            %21 = affine.load %20[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc100)
                            %22 = polygeist.subindex %21[%arg3] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc100)
                            %23 = affine.load %22[0] : memref<?xmemref<?xf32>> loc(#loc104)
                            memref.dealloc %23 : memref<?xf32> loc(#loc104)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc102)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %19 = polygeist.subindex %17[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc105)
                  %20 = affine.load %19[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc106)
                  memref.dealloc %20 : memref<?xmemref<?xf32>> loc(#loc106)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %19 = polygeist.subindex %18[%arg2] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc107)
                  %20 = affine.load %19[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc108)
                  memref.dealloc %20 : memref<?xmemref<?xf32>> loc(#loc108)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc101)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_5[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc109)
        memref.dealloc %17 : memref<?xmemref<?xmemref<?xf32>>> loc(#loc109)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xmemref<?xf32>>>> loc(#loc110)
        memref.dealloc %17 : memref<?xmemref<?xmemref<?xf32>>> loc(#loc110)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_3[0] : memref<1xmemref<?xf32>> loc(#loc111)
        memref.dealloc %17 : memref<?xf32> loc(#loc111)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_2[0] : memref<1xmemref<?xf32>> loc(#loc112)
        memref.dealloc %17 : memref<?xf32> loc(#loc112)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_1[0] : memref<1xmemref<?xf32>> loc(#loc113)
        memref.dealloc %17 : memref<?xf32> loc(#loc113)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = affine.load %alloca_0[0] : memref<1xmemref<?xf32>> loc(#loc114)
        memref.dealloc %17 : memref<?xf32> loc(#loc114)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %17 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc115)
        func.call @carts_phase_timer_stop(%17) : (memref<?xi8>) -> () loc(#loc115)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc116)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %16 = scf.if %true -> (i32) {
      %17 = scf.execute_region -> i32 {
        %18 = scf.if %true -> (i32) {
          %19 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %19 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %18 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %17 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    return %16 : i32 loc(#loc117)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc118)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc119)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc120)
  func.func private @init_data(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13), %arg2: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13), %arg3: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13), %arg4: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13), %arg5: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc122)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %cst = arith.constant 2.000000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 1.000000e+00 : f32 loc(#loc)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %cst_1 = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %0 = arith.index_cast %arg3 : i32 to index loc(#loc121)
    %1 = arith.index_cast %arg4 : i32 to index loc(#loc121)
    %alloca = memref.alloca() : memref<1xi32> loc(#loc123)
    %2 = "polygeist.undef"() : () -> i32 loc(#loc123)
    affine.store %2, %alloca[0] : memref<1xi32> loc(#loc123)
    %3 = scf.if %true -> (i32) {
      %4 = scf.execute_region -> i32 {
        %5 = scf.if %true -> (i32) {
          %6 = scf.execute_region -> i32 {
            %7 = arith.muli %arg3, %arg4 : i32 loc(#loc124)
            %8 = arith.muli %7, %arg5 : i32 loc(#loc125)
            scf.yield %8 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %6 : i32 loc(#loc)
        } else {
          scf.yield %2 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %5 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %4 : i32 loc(#loc)
    } else {
      scf.yield %2 : i32 loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.store %c0_i32, %alloca[0] : memref<1xi32> loc(#loc123)
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
            %4 = arith.index_cast %arg5 : i32 to index loc(#loc126)
            %5 = arith.sitofp %3 : i32 to f32 loc(#loc127)
            affine.for %arg6 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":228:3) = 0 to %0 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %6 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc129)
                      affine.for %arg7 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":229:5) = 0 to %1 {
                        scf.if %true {
                          scf.execute_region {
                            scf.if %true {
                              scf.execute_region {
                                scf.for %arg8 = %c0 to %4 step %c1 {
                                  scf.if %true {
                                    scf.execute_region {
                                      %7 = affine.load %6[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc129)
                                      %8 = polygeist.subindex %7[%arg7] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc129)
                                      %9 = affine.load %8[0] : memref<?xmemref<?xf32>> loc(#loc129)
                                      %10 = polygeist.subindex %9[%arg8] () : memref<?xf32> -> memref<?xf32> loc(#loc129)
                                      %11 = affine.load %alloca[0] : memref<1xi32> loc(#loc132)
                                      %12 = arith.sitofp %11 : i32 to f32 loc(#loc133)
                                      %13 = arith.divf %12, %5 : f32 loc(#loc134)
                                      %14 = arith.mulf %13, %cst : f32 loc(#loc135)
                                      %15 = arith.subf %14, %cst_0 : f32 loc(#loc136)
                                      affine.store %15, %10[0] : memref<?xf32> loc(#loc137)
                                      scf.yield loc(#loc)
                                    } loc(#loc)
                                  } loc(#loc)
                                  scf.if %true {
                                    scf.execute_region {
                                      %7 = affine.load %alloca[0] : memref<1xi32> loc(#loc138)
                                      %8 = arith.addi %7, %c1_i32 : i32 loc(#loc138)
                                      affine.store %8, %alloca[0] : memref<1xi32> loc(#loc138)
                                      scf.yield loc(#loc)
                                    } loc(#loc)
                                  } loc(#loc)
                                } loc(#loc131)
                                scf.yield loc(#loc)
                              } loc(#loc)
                            } loc(#loc)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc130)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc128)
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
            affine.for %arg6 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":238:3) = 0 to %1 {
              scf.if %true {
                scf.execute_region {
                  %4 = polygeist.subindex %arg1[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc140)
                  affine.store %cst_0, %4[0] : memref<?xf32> loc(#loc141)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %4 = polygeist.subindex %arg2[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc142)
                  affine.store %cst_1, %4[0] : memref<?xf32> loc(#loc143)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc139)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc144)
  } loc(#loc121)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc145)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc146)
  func.func private @batchnorm_forward(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg1: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg2: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg3: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg4: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg5: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg6: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg7: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13), %arg8: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc148)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc149)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %1 = scf.if %true -> (i32) {
              %3 = scf.execute_region -> i32 {
                scf.yield %arg4 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %3 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %2 = arith.index_cast %1 : i32 to index loc(#loc150)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg9) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %3 = arith.index_cast %arg5 : i32 to index loc(#loc151)
                              %4 = arith.index_cast %arg6 : i32 to index loc(#loc152)
                              %5 = polygeist.subindex %arg1[%arg9] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc153)
                              %6 = polygeist.subindex %arg0[%arg9] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc154)
                              scf.for %arg10 = %c0 to %3 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        scf.for %arg11 = %c0 to %4 step %c1 {
                                          scf.if %true {
                                            scf.execute_region {
                                              %7 = affine.load %5[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc153)
                                              %8 = polygeist.subindex %7[%arg10] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc153)
                                              %9 = affine.load %8[0] : memref<?xmemref<?xf32>> loc(#loc153)
                                              %10 = polygeist.subindex %9[%arg11] () : memref<?xf32> -> memref<?xf32> loc(#loc153)
                                              %11 = affine.load %6[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc154)
                                              %12 = polygeist.subindex %11[%arg10] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc154)
                                              %13 = affine.load %12[0] : memref<?xmemref<?xf32>> loc(#loc154)
                                              %14 = polygeist.subindex %13[%arg11] () : memref<?xf32> -> memref<?xf32> loc(#loc154)
                                              %15 = affine.load %14[0] : memref<?xf32> loc(#loc154)
                                              affine.store %15, %10[0] : memref<?xf32> loc(#loc157)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc156)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc155)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc150)
                    } loc(#loc150)
                    omp.yield loc(#loc150)
                  } loc(#loc150)
                } loc(#loc150)
                scf.yield loc(#loc150)
              } loc(#loc150)
              omp.terminator loc(#loc150)
            } loc(#loc150)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @mean_cpu(%arg1, %arg4, %arg5, %arg6, %arg7) : (memref<?xmemref<?xmemref<?xf32>>>, i32, i32, i32, memref<?xf32>) -> () loc(#loc158)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @variance_cpu(%arg1, %arg7, %arg4, %arg5, %arg6, %arg8) : (memref<?xmemref<?xmemref<?xf32>>>, memref<?xf32>, i32, i32, i32, memref<?xf32>) -> () loc(#loc159)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @normalize_cpu(%arg1, %arg7, %arg8, %arg4, %arg5, %arg6) : (memref<?xmemref<?xmemref<?xf32>>>, memref<?xf32>, memref<?xf32>, i32, i32, i32) -> () loc(#loc160)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @scale_bias_cpu(%arg1, %arg2, %arg4, %arg5, %arg6) : (memref<?xmemref<?xmemref<?xf32>>>, memref<?xf32>, i32, i32, i32) -> () loc(#loc161)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @add_bias_cpu(%arg1, %arg3, %arg4, %arg5, %arg6) : (memref<?xmemref<?xmemref<?xf32>>>, memref<?xf32>, i32, i32, i32) -> () loc(#loc162)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc163)
  } loc(#loc147)
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc164)
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc165)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc166)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc167)
  func.func private @mean_cpu(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:13), %arg1: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:13), %arg2: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:13), %arg3: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:13), %arg4: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst_0 = arith.constant 1.000000e+00 : f32 loc(#loc)
    %true = arith.constant true loc(#loc169)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc170)
    %1 = "polygeist.undef"() : () -> f32 loc(#loc171)
    %2 = scf.if %true -> (f32) {
      %3 = scf.execute_region -> f32 {
        %4 = scf.if %true -> (f32) {
          %5 = scf.execute_region -> f32 {
            %6 = arith.muli %arg1, %arg3 : i32 loc(#loc172)
            %7 = arith.sitofp %6 : i32 to f32 loc(#loc173)
            %8 = arith.divf %cst_0, %7 : f32 loc(#loc174)
            scf.yield %8 : f32 loc(#loc)
          } loc(#loc)
          scf.yield %5 : f32 loc(#loc)
        } else {
          scf.yield %1 : f32 loc(#loc)
        } loc(#loc)
        scf.yield %4 : f32 loc(#loc)
      } loc(#loc)
      scf.yield %3 : f32 loc(#loc)
    } else {
      scf.yield %1 : f32 loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %3 = scf.if %true -> (i32) {
              %5 = scf.execute_region -> i32 {
                scf.yield %arg2 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %5 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %4 = arith.index_cast %3 : i32 to index loc(#loc175)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg5) : index = (%c0) to (%4) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %5 = polygeist.subindex %arg4[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc176)
                          affine.store %cst, %5[0] : memref<?xf32> loc(#loc177)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = arith.index_cast %arg1 : i32 to index loc(#loc178)
                              %6 = arith.index_cast %arg3 : i32 to index loc(#loc179)
                              %7 = polygeist.subindex %arg4[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc180)
                              scf.for %arg6 = %c0 to %5 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        %8 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc182)
                                        scf.for %arg7 = %c0 to %6 step %c1 {
                                          scf.if %true {
                                            scf.execute_region {
                                              %9 = affine.load %8[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc182)
                                              %10 = polygeist.subindex %9[%arg5] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc182)
                                              %11 = affine.load %10[0] : memref<?xmemref<?xf32>> loc(#loc182)
                                              %12 = polygeist.subindex %11[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc182)
                                              %13 = affine.load %12[0] : memref<?xf32> loc(#loc182)
                                              %14 = affine.load %7[0] : memref<?xf32> loc(#loc184)
                                              %15 = arith.addf %14, %13 : f32 loc(#loc184)
                                              affine.store %15, %7[0] : memref<?xf32> loc(#loc184)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc183)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc181)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          %5 = polygeist.subindex %arg4[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc185)
                          %6 = affine.load %5[0] : memref<?xf32> loc(#loc186)
                          %7 = arith.mulf %6, %2 : f32 loc(#loc186)
                          affine.store %7, %5[0] : memref<?xf32> loc(#loc186)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc175)
                    } loc(#loc175)
                    omp.yield loc(#loc175)
                  } loc(#loc175)
                } loc(#loc175)
                scf.yield loc(#loc175)
              } loc(#loc175)
              omp.terminator loc(#loc175)
            } loc(#loc175)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc187)
  } loc(#loc168)
  func.func private @variance_cpu(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13), %arg2: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13), %arg3: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13), %arg4: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13), %arg5: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c-1_i32 = arith.constant -1 : i32 loc(#loc)
    %cst = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst_0 = arith.constant 1.000000e+00 : f32 loc(#loc)
    %true = arith.constant true loc(#loc189)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc190)
    %1 = "polygeist.undef"() : () -> f32 loc(#loc191)
    %2 = scf.if %true -> (f32) {
      %3 = scf.execute_region -> f32 {
        %4 = scf.if %true -> (f32) {
          %5 = scf.execute_region -> f32 {
            %6 = arith.muli %arg2, %arg4 : i32 loc(#loc192)
            %7 = arith.addi %6, %c-1_i32 : i32 loc(#loc193)
            %8 = arith.sitofp %7 : i32 to f32 loc(#loc194)
            %9 = arith.divf %cst_0, %8 : f32 loc(#loc195)
            scf.yield %9 : f32 loc(#loc)
          } loc(#loc)
          scf.yield %5 : f32 loc(#loc)
        } else {
          scf.yield %1 : f32 loc(#loc)
        } loc(#loc)
        scf.yield %4 : f32 loc(#loc)
      } loc(#loc)
      scf.yield %3 : f32 loc(#loc)
    } else {
      scf.yield %1 : f32 loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %3 = scf.if %true -> (i32) {
              %5 = scf.execute_region -> i32 {
                scf.yield %arg3 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %5 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %4 = arith.index_cast %3 : i32 to index loc(#loc196)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg6) : index = (%c0) to (%4) step (%c1) {
                    scf.execute_region {
                      %alloca = memref.alloca() : memref<1xf32> loc(#loc197)
                      affine.store %1, %alloca[0] : memref<1xf32> loc(#loc197)
                      scf.if %true {
                        scf.execute_region {
                          %5 = polygeist.subindex %arg5[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc198)
                          affine.store %cst, %5[0] : memref<?xf32> loc(#loc199)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = arith.index_cast %arg2 : i32 to index loc(#loc200)
                              %6 = arith.index_cast %arg4 : i32 to index loc(#loc201)
                              %7 = polygeist.subindex %arg1[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc202)
                              %8 = polygeist.subindex %arg5[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc203)
                              scf.for %arg7 = %c0 to %5 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        %9 = polygeist.subindex %arg0[%arg7] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc205)
                                        scf.for %arg8 = %c0 to %6 step %c1 {
                                          scf.if %true {
                                            scf.execute_region {
                                              scf.if %true {
                                                %10 = scf.execute_region -> f32 {
                                                  %11 = affine.load %9[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc205)
                                                  %12 = polygeist.subindex %11[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc205)
                                                  %13 = affine.load %12[0] : memref<?xmemref<?xf32>> loc(#loc205)
                                                  %14 = polygeist.subindex %13[%arg8] () : memref<?xf32> -> memref<?xf32> loc(#loc205)
                                                  %15 = affine.load %14[0] : memref<?xf32> loc(#loc205)
                                                  %16 = affine.load %7[0] : memref<?xf32> loc(#loc202)
                                                  %17 = arith.subf %15, %16 : f32 loc(#loc207)
                                                  affine.store %17, %alloca[0] : memref<1xf32> loc(#loc197)
                                                  scf.yield %17 : f32 loc(#loc)
                                                } loc(#loc)
                                              } loc(#loc)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                          scf.if %true {
                                            scf.execute_region {
                                              %10 = affine.load %alloca[0] : memref<1xf32> loc(#loc208)
                                              %11 = arith.mulf %10, %10 : f32 loc(#loc209)
                                              %12 = affine.load %8[0] : memref<?xf32> loc(#loc210)
                                              %13 = arith.addf %12, %11 : f32 loc(#loc210)
                                              affine.store %13, %8[0] : memref<?xf32> loc(#loc210)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc206)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc204)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          %5 = polygeist.subindex %arg5[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc211)
                          %6 = affine.load %5[0] : memref<?xf32> loc(#loc212)
                          %7 = arith.mulf %6, %2 : f32 loc(#loc212)
                          affine.store %7, %5[0] : memref<?xf32> loc(#loc212)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc196)
                    } loc(#loc196)
                    omp.yield loc(#loc196)
                  } loc(#loc196)
                } loc(#loc196)
                scf.yield loc(#loc196)
              } loc(#loc196)
              omp.terminator loc(#loc196)
            } loc(#loc196)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc213)
  } loc(#loc188)
  func.func private @normalize_cpu(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13), %arg2: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13), %arg3: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13), %arg4: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13), %arg5: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst = arith.constant 9.99999974E-6 : f32 loc(#loc)
    %true = arith.constant true loc(#loc215)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc216)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %1 = scf.if %true -> (i32) {
              %3 = scf.execute_region -> i32 {
                scf.yield %arg3 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %3 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %2 = arith.index_cast %1 : i32 to index loc(#loc217)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg6) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %3 = arith.index_cast %arg4 : i32 to index loc(#loc218)
                              %4 = arith.index_cast %arg5 : i32 to index loc(#loc219)
                              %5 = polygeist.subindex %arg0[%arg6] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc220)
                              scf.for %arg7 = %c0 to %3 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        %6 = polygeist.subindex %arg1[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc222)
                                        %7 = polygeist.subindex %arg2[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc223)
                                        scf.for %arg8 = %c0 to %4 step %c1 {
                                          scf.if %true {
                                            scf.execute_region {
                                              %8 = affine.load %5[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc220)
                                              %9 = polygeist.subindex %8[%arg7] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc220)
                                              %10 = affine.load %9[0] : memref<?xmemref<?xf32>> loc(#loc220)
                                              %11 = polygeist.subindex %10[%arg8] () : memref<?xf32> -> memref<?xf32> loc(#loc220)
                                              %12 = affine.load %11[0] : memref<?xf32> loc(#loc225)
                                              %13 = affine.load %6[0] : memref<?xf32> loc(#loc222)
                                              %14 = arith.subf %12, %13 : f32 loc(#loc226)
                                              %15 = affine.load %7[0] : memref<?xf32> loc(#loc223)
                                              %16 = arith.addf %15, %cst : f32 loc(#loc227)
                                              %17 = math.sqrt %16 : f32 loc(#loc228)
                                              %18 = arith.divf %14, %17 : f32 loc(#loc229)
                                              affine.store %18, %11[0] : memref<?xf32> loc(#loc230)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc224)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc221)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc217)
                    } loc(#loc217)
                    omp.yield loc(#loc217)
                  } loc(#loc217)
                } loc(#loc217)
                scf.yield loc(#loc217)
              } loc(#loc217)
              omp.terminator loc(#loc217)
            } loc(#loc217)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc231)
  } loc(#loc214)
  func.func private @scale_bias_cpu(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:13), %arg2: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:13), %arg3: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:13), %arg4: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc233)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc234)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %1 = scf.if %true -> (i32) {
              %3 = scf.execute_region -> i32 {
                scf.yield %arg2 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %3 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %2 = arith.index_cast %1 : i32 to index loc(#loc235)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg5) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %3 = arith.index_cast %arg3 : i32 to index loc(#loc236)
                              %4 = arith.index_cast %arg4 : i32 to index loc(#loc237)
                              %5 = polygeist.subindex %arg0[%arg5] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc238)
                              scf.for %arg6 = %c0 to %3 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        %6 = polygeist.subindex %arg1[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc240)
                                        scf.for %arg7 = %c0 to %4 step %c1 {
                                          scf.if %true {
                                            scf.execute_region {
                                              %7 = affine.load %5[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc238)
                                              %8 = polygeist.subindex %7[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc238)
                                              %9 = affine.load %8[0] : memref<?xmemref<?xf32>> loc(#loc238)
                                              %10 = polygeist.subindex %9[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc238)
                                              %11 = affine.load %6[0] : memref<?xf32> loc(#loc240)
                                              %12 = affine.load %10[0] : memref<?xf32> loc(#loc242)
                                              %13 = arith.mulf %12, %11 : f32 loc(#loc242)
                                              affine.store %13, %10[0] : memref<?xf32> loc(#loc242)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc241)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc239)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc235)
                    } loc(#loc235)
                    omp.yield loc(#loc235)
                  } loc(#loc235)
                } loc(#loc235)
                scf.yield loc(#loc235)
              } loc(#loc235)
              omp.terminator loc(#loc235)
            } loc(#loc235)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc243)
  } loc(#loc232)
  func.func private @add_bias_cpu(%arg0: memref<?xmemref<?xmemref<?xf32>>> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:13), %arg1: memref<?xf32> loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:13), %arg2: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:13), %arg3: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:13), %arg4: i32 loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc245)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc246)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %1 = scf.if %true -> (i32) {
              %3 = scf.execute_region -> i32 {
                scf.yield %arg2 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %3 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %2 = arith.index_cast %1 : i32 to index loc(#loc247)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg5) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %3 = arith.index_cast %arg3 : i32 to index loc(#loc248)
                              %4 = arith.index_cast %arg4 : i32 to index loc(#loc249)
                              %5 = polygeist.subindex %arg0[%arg5] () : memref<?xmemref<?xmemref<?xf32>>> -> memref<?xmemref<?xmemref<?xf32>>> loc(#loc250)
                              scf.for %arg6 = %c0 to %3 step %c1 {
                                scf.if %true {
                                  scf.execute_region {
                                    scf.if %true {
                                      scf.execute_region {
                                        %6 = polygeist.subindex %arg1[%arg6] () : memref<?xf32> -> memref<?xf32> loc(#loc252)
                                        scf.for %arg7 = %c0 to %4 step %c1 {
                                          scf.if %true {
                                            scf.execute_region {
                                              %7 = affine.load %5[0] : memref<?xmemref<?xmemref<?xf32>>> loc(#loc250)
                                              %8 = polygeist.subindex %7[%arg6] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc250)
                                              %9 = affine.load %8[0] : memref<?xmemref<?xf32>> loc(#loc250)
                                              %10 = polygeist.subindex %9[%arg7] () : memref<?xf32> -> memref<?xf32> loc(#loc250)
                                              %11 = affine.load %6[0] : memref<?xf32> loc(#loc252)
                                              %12 = affine.load %10[0] : memref<?xf32> loc(#loc254)
                                              %13 = arith.addf %12, %11 : f32 loc(#loc254)
                                              affine.store %13, %10[0] : memref<?xf32> loc(#loc254)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                        } loc(#loc253)
                                        scf.yield loc(#loc)
                                      } loc(#loc)
                                    } loc(#loc)
                                    scf.yield loc(#loc)
                                  } loc(#loc)
                                } loc(#loc)
                              } loc(#loc251)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc247)
                    } loc(#loc247)
                    omp.yield loc(#loc247)
                  } loc(#loc247)
                } loc(#loc247)
                scf.yield loc(#loc247)
              } loc(#loc247)
              omp.terminator loc(#loc247)
            } loc(#loc247)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc255)
  } loc(#loc244)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":246:25)
#loc6 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":244:1)
#loc7 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":301:10)
#loc8 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":288:3)
#loc9 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":261:3)
#loc10 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":260:3)
#loc11 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":259:3)
#loc12 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":258:3)
#loc13 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":257:3)
#loc14 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":256:3)
#loc15 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc16 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc17 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc18 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":253:24)
#loc19 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":256:16)
#loc20 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":256:34)
#loc21 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":256:42)
#loc22 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":256:40)
#loc23 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":257:21)
#loc24 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":257:39)
#loc25 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":257:47)
#loc26 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":257:45)
#loc27 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":258:19)
#loc28 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":258:35)
#loc29 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":258:46)
#loc30 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":258:44)
#loc31 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":259:19)
#loc32 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":259:35)
#loc33 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":259:46)
#loc34 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":259:44)
#loc35 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":260:17)
#loc36 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":260:33)
#loc37 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":260:44)
#loc38 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":260:42)
#loc39 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":261:21)
#loc40 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":261:37)
#loc41 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":261:48)
#loc42 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":261:46)
#loc43 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":264:5)
#loc44 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":264:12)
#loc45 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":264:29)
#loc46 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":264:40)
#loc47 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":264:38)
#loc48 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":265:5)
#loc49 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":265:17)
#loc50 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":265:34)
#loc51 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":265:45)
#loc52 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":265:43)
#loc53 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":267:17)
#loc54 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":267:33)
#loc55 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":267:43)
#loc56 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":267:41)
#loc57 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":268:22)
#loc58 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":268:38)
#loc59 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":268:48)
#loc60 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":268:46)
#loc62 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":264:10)
#loc63 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":265:15)
#loc64 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":267:7)
#loc65 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":268:7)
#loc67 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":267:15)
#loc68 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":268:20)
#loc69 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":273:13)
#loc70 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":273:16)
#loc71 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":273:24)
#loc72 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":273:3)
#loc73 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc74 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc75 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":141:40)
#loc77 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":280:23)
#loc78 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":280:26)
#loc79 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":280:34)
#loc80 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":280:42)
#loc81 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":281:23)
#loc82 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":281:29)
#loc83 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":280:5)
#loc84 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":142:40)
#loc85 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc86 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":290:16)
#loc87 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":290:3)
#loc88 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":291:15)
#loc89 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":291:3)
#loc90 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":293:30)
#loc92 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":293:22)
#loc93 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":293:17)
#loc94 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":293:14)
#loc95 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":295:24)
#loc96 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc97 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc98 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc99 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":302:12)
#loc100 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":303:12)
#loc103 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":302:7)
#loc104 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":303:7)
#loc105 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":305:10)
#loc106 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":305:5)
#loc107 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":306:10)
#loc108 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":306:5)
#loc109 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":308:3)
#loc110 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":309:3)
#loc111 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":310:3)
#loc112 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":311:3)
#loc113 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":312:3)
#loc114 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":313:3)
#loc115 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc116 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc117 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":320:1)
#loc118 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc119 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc120 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc122 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":221:1)
#loc123 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":225:3)
#loc124 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":224:21)
#loc125 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":224:32)
#loc126 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":230:21)
#loc127 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:36)
#loc129 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:9)
#loc131 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":230:7)
#loc132 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:30)
#loc133 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:23)
#loc134 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:34)
#loc135 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:43)
#loc136 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:50)
#loc137 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":231:20)
#loc138 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":232:12)
#loc140 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":239:5)
#loc141 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":239:15)
#loc142 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":240:5)
#loc143 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":240:15)
#loc144 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":242:1)
#loc145 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc146 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc148 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":191:1)
#loc149 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":198:3)
#loc150 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":197:1)
#loc151 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":199:19)
#loc152 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":200:21)
#loc153 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":201:9)
#loc154 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":201:27)
#loc155 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":199:5)
#loc156 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":200:7)
#loc157 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":201:25)
#loc158 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":207:3)
#loc159 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":208:3)
#loc160 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":211:3)
#loc161 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":214:3)
#loc162 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":215:3)
#loc163 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":216:1)
#loc164 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":134:6)
#loc165 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":137:6)
#loc166 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc167 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
#loc169 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":61:1)
#loc170 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":66:3)
#loc171 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":62:3)
#loc172 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":62:31)
#loc173 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":62:24)
#loc174 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":62:22)
#loc175 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":65:1)
#loc176 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":67:5)
#loc177 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":67:13)
#loc178 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":68:19)
#loc179 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":69:21)
#loc180 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":70:9)
#loc181 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":68:5)
#loc182 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":70:20)
#loc183 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":69:7)
#loc184 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":70:17)
#loc185 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":73:5)
#loc186 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":73:13)
#loc187 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":75:1)
#loc189 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":88:1)
#loc190 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":94:3)
#loc191 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":90:3)
#loc192 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":90:31)
#loc193 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":90:41)
#loc194 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":90:24)
#loc195 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":90:22)
#loc196 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":93:1)
#loc197 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":98:9)
#loc198 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":95:5)
#loc199 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":95:17)
#loc200 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":96:19)
#loc201 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":97:21)
#loc202 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":98:35)
#loc203 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":99:9)
#loc204 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":96:5)
#loc205 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":98:22)
#loc206 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":97:7)
#loc207 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":98:33)
#loc208 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":99:24)
#loc209 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":99:29)
#loc210 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":99:21)
#loc211 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":102:5)
#loc212 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":102:17)
#loc213 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":104:1)
#loc215 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":117:1)
#loc216 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":122:3)
#loc217 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":121:1)
#loc218 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":123:19)
#loc219 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":124:21)
#loc220 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:9)
#loc221 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":123:5)
#loc222 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:36)
#loc223 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:54)
#loc224 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":124:7)
#loc225 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:23)
#loc226 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:34)
#loc227 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:66)
#loc228 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:48)
#loc229 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:45)
#loc230 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":125:20)
#loc231 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":129:1)
#loc233 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":141:1)
#loc234 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":145:3)
#loc235 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":144:1)
#loc236 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":146:19)
#loc237 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":147:21)
#loc238 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":148:9)
#loc239 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":146:5)
#loc240 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":148:28)
#loc241 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":147:7)
#loc242 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":148:25)
#loc243 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":152:1)
#loc245 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":164:1)
#loc246 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":168:3)
#loc247 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":167:1)
#loc248 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":169:19)
#loc249 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":170:21)
#loc250 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":171:9)
#loc251 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":169:5)
#loc252 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":171:28)
#loc253 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":170:7)
#loc254 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":171:25)
#loc255 = loc("benchmarks/ml-kernels/batchnorm/batchnorm.c":175:1)
