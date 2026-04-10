#loc5 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":161:5)
#loc77 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":189:3)
#loc105 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":206:3)
#loc109 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":209:3)
#loc113 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":212:3)
#loc117 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":215:3)
#loc121 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":218:3)
#loc125 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":221:3)
#loc129 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":224:3)
#loc133 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":227:3)
#loc170 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":153:13)
#loc173 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":155:3)
#loc183 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":32:13)
#loc193 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":43:13)
#loc204 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":54:13)
#loc217 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":70:13)
#loc235 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":89:13)
#loc249 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":102:13)
#loc261 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":113:13)
#loc270 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":126:13)
#loc275 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":131:3)
#loc282 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":139:3)
#loc291 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":145:3)
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("activations\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":161:5), %arg1: memref<?xmemref<?xi8>> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":161:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1048576_i32 = arith.constant 1048576 : i32 loc(#loc)
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %c100_i32 = arith.constant 100 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc6)
    %4 = "polygeist.undef"() : () -> f64 loc(#loc7)
    %5 = "polygeist.undef"() : () -> i32 loc(#loc8)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc9)
    affine.store %4, %alloca[0] : memref<1xf64> loc(#loc9)
    %alloca_0 = memref.alloca() : memref<1xf64> loc(#loc10)
    affine.store %4, %alloca_0[0] : memref<1xf64> loc(#loc10)
    %alloca_1 = memref.alloca() : memref<1xf64> loc(#loc11)
    affine.store %4, %alloca_1[0] : memref<1xf64> loc(#loc11)
    %alloca_2 = memref.alloca() : memref<1xf64> loc(#loc12)
    affine.store %4, %alloca_2[0] : memref<1xf64> loc(#loc12)
    %alloca_3 = memref.alloca() : memref<1xf64> loc(#loc13)
    affine.store %4, %alloca_3[0] : memref<1xf64> loc(#loc13)
    %alloca_4 = memref.alloca() : memref<1xf64> loc(#loc14)
    affine.store %4, %alloca_4[0] : memref<1xf64> loc(#loc14)
    %alloca_5 = memref.alloca() : memref<1xf64> loc(#loc15)
    affine.store %4, %alloca_5[0] : memref<1xf64> loc(#loc15)
    %alloca_6 = memref.alloca() : memref<1xf64> loc(#loc16)
    affine.store %4, %alloca_6[0] : memref<1xf64> loc(#loc16)
    %alloca_7 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc17)
    %alloca_8 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc18)
    %alloca_9 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc19)
    %alloca_10 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc20)
    %alloca_11 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc21)
    %alloca_12 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc22)
    %alloca_13 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc23)
    %alloca_14 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc24)
    %alloca_15 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc25)
    %alloca_16 = memref.alloca() : memref<1xmemref<?xf32>> loc(#loc26)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc27)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc28)
        func.call @carts_e2e_timer_start(%12) : (memref<?xi8>) -> () loc(#loc28)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc29)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc29)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc29)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %6 = scf.if %true -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %true -> (i32) {
          %14 = scf.execute_region -> i32 {
            scf.yield %c1048576_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %14 : i32 loc(#loc)
        } else {
          scf.yield %5 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %13 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %12 : i32 loc(#loc)
    } else {
      scf.yield %5 : i32 loc(#loc)
    } loc(#loc)
    %7 = arith.index_cast %6 : i32 to index loc(#loc)
    %8 = scf.if %true -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %true -> (i32) {
          %14 = scf.execute_region -> i32 {
            scf.yield %c100_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %14 : i32 loc(#loc)
        } else {
          scf.yield %5 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %13 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %12 : i32 loc(#loc)
    } else {
      scf.yield %5 : i32 loc(#loc)
    } loc(#loc)
    %9 = arith.index_cast %8 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc30)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc31)
            %15 = arith.index_cast %13 : index to i64 loc(#loc32)
            %16 = arith.muli %14, %15 : i64 loc(#loc33)
            %17 = arith.index_cast %16 : i64 to index loc(#loc30)
            %18 = arith.divui %17, %13 : index loc(#loc30)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc30)
            affine.store %alloc, %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc26)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc34)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc35)
            %15 = arith.index_cast %13 : index to i64 loc(#loc36)
            %16 = arith.muli %14, %15 : i64 loc(#loc37)
            %17 = arith.index_cast %16 : i64 to index loc(#loc34)
            %18 = arith.divui %17, %13 : index loc(#loc34)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc34)
            affine.store %alloc, %alloca_15[0] : memref<1xmemref<?xf32>> loc(#loc25)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc38)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc39)
            %15 = arith.index_cast %13 : index to i64 loc(#loc40)
            %16 = arith.muli %14, %15 : i64 loc(#loc41)
            %17 = arith.index_cast %16 : i64 to index loc(#loc38)
            %18 = arith.divui %17, %13 : index loc(#loc38)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc38)
            affine.store %alloc, %alloca_14[0] : memref<1xmemref<?xf32>> loc(#loc24)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc42)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc43)
            %15 = arith.index_cast %13 : index to i64 loc(#loc44)
            %16 = arith.muli %14, %15 : i64 loc(#loc45)
            %17 = arith.index_cast %16 : i64 to index loc(#loc42)
            %18 = arith.divui %17, %13 : index loc(#loc42)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc42)
            affine.store %alloc, %alloca_13[0] : memref<1xmemref<?xf32>> loc(#loc23)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc46)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc47)
            %15 = arith.index_cast %13 : index to i64 loc(#loc48)
            %16 = arith.muli %14, %15 : i64 loc(#loc49)
            %17 = arith.index_cast %16 : i64 to index loc(#loc46)
            %18 = arith.divui %17, %13 : index loc(#loc46)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc46)
            affine.store %alloc, %alloca_12[0] : memref<1xmemref<?xf32>> loc(#loc22)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc50)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc51)
            %15 = arith.index_cast %13 : index to i64 loc(#loc52)
            %16 = arith.muli %14, %15 : i64 loc(#loc53)
            %17 = arith.index_cast %16 : i64 to index loc(#loc50)
            %18 = arith.divui %17, %13 : index loc(#loc50)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc50)
            affine.store %alloc, %alloca_11[0] : memref<1xmemref<?xf32>> loc(#loc21)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc54)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc55)
            %15 = arith.index_cast %13 : index to i64 loc(#loc56)
            %16 = arith.muli %14, %15 : i64 loc(#loc57)
            %17 = arith.index_cast %16 : i64 to index loc(#loc54)
            %18 = arith.divui %17, %13 : index loc(#loc54)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc54)
            affine.store %alloc, %alloca_10[0] : memref<1xmemref<?xf32>> loc(#loc20)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc58)
            %14 = arith.extsi %6 : i32 to i64 loc(#loc59)
            %15 = arith.index_cast %13 : index to i64 loc(#loc60)
            %16 = arith.muli %14, %15 : i64 loc(#loc61)
            %17 = arith.index_cast %16 : i64 to index loc(#loc58)
            %18 = arith.divui %17, %13 : index loc(#loc58)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc58)
            affine.store %alloc, %alloca_9[0] : memref<1xmemref<?xf32>> loc(#loc19)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc62)
            %14 = arith.extsi %8 : i32 to i64 loc(#loc63)
            %15 = arith.index_cast %13 : index to i64 loc(#loc64)
            %16 = arith.muli %14, %15 : i64 loc(#loc65)
            %17 = arith.index_cast %16 : i64 to index loc(#loc62)
            %18 = arith.divui %17, %13 : index loc(#loc62)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc62)
            affine.store %alloc, %alloca_8[0] : memref<1xmemref<?xf32>> loc(#loc18)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %12 = scf.execute_region -> memref<?xf32> {
            %13 = polygeist.typeSize f32 : index loc(#loc66)
            %14 = arith.extsi %8 : i32 to i64 loc(#loc67)
            %15 = arith.index_cast %13 : index to i64 loc(#loc68)
            %16 = arith.muli %14, %15 : i64 loc(#loc69)
            %17 = arith.index_cast %16 : i64 to index loc(#loc66)
            %18 = arith.divui %17, %13 : index loc(#loc66)
            %alloc = memref.alloc(%18) : memref<?xf32> loc(#loc66)
            affine.store %alloc, %alloca_7[0] : memref<1xmemref<?xf32>> loc(#loc17)
            scf.yield %alloc : memref<?xf32> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc70)
        func.call @init_data(%12, %6) : (memref<?xf32>, i32) -> () loc(#loc71)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_8[0] : memref<1xmemref<?xf32>> loc(#loc72)
        func.call @init_data(%12, %8) : (memref<?xf32>, i32) -> () loc(#loc73)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc74)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc74)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc75)
        func.call @carts_kernel_timer_start(%12) : (memref<?xi8>) -> () loc(#loc75)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc76)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":189:3) = 0 to 1 {
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc78)
                  %14 = affine.load %alloca_15[0] : memref<1xmemref<?xf32>> loc(#loc79)
                  func.call @activate_relu(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc80)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc81)
                  %14 = affine.load %alloca_14[0] : memref<1xmemref<?xf32>> loc(#loc82)
                  func.call @activate_leaky(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc83)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc84)
                  %14 = affine.load %alloca_13[0] : memref<1xmemref<?xf32>> loc(#loc85)
                  func.call @activate_relu6(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc86)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc87)
                  %14 = affine.load %alloca_12[0] : memref<1xmemref<?xf32>> loc(#loc88)
                  func.call @activate_gelu(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc89)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc90)
                  %14 = affine.load %alloca_11[0] : memref<1xmemref<?xf32>> loc(#loc91)
                  func.call @activate_gelu_fast(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc92)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc93)
                  %14 = affine.load %alloca_10[0] : memref<1xmemref<?xf32>> loc(#loc94)
                  func.call @activate_sigmoid(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc95)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc96)
                  %14 = affine.load %alloca_9[0] : memref<1xmemref<?xf32>> loc(#loc97)
                  func.call @activate_tanh(%13, %14, %6) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc98)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = affine.load %alloca_8[0] : memref<1xmemref<?xf32>> loc(#loc99)
                  %14 = affine.load %alloca_7[0] : memref<1xmemref<?xf32>> loc(#loc100)
                  func.call @softmax(%13, %14, %8) : (memref<?xf32>, memref<?xf32>, i32) -> () loc(#loc101)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  func.call @carts_kernel_timer_accum(%12) : (memref<?xi8>) -> () loc(#loc76)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc77)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc102)
        func.call @carts_kernel_timer_print(%12) : (memref<?xi8>) -> () loc(#loc102)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc103)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc103)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc103)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            affine.store %cst, %alloca_6[0] : memref<1xf64> loc(#loc16)
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
            %12 = affine.load %alloca_15[0] : memref<1xmemref<?xf32>> loc(#loc104)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":206:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc104)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc104)
              %15 = arith.extf %14 : f32 to f64 loc(#loc104)
              %16 = math.absf %15 : f64 loc(#loc106)
              %17 = affine.load %alloca_6[0] : memref<1xf64> loc(#loc107)
              %18 = arith.addf %17, %16 : f64 loc(#loc107)
              affine.store %18, %alloca_6[0] : memref<1xf64> loc(#loc107)
            } loc(#loc105)
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
            affine.store %cst, %alloca_5[0] : memref<1xf64> loc(#loc15)
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
            %12 = affine.load %alloca_14[0] : memref<1xmemref<?xf32>> loc(#loc108)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":209:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc108)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc108)
              %15 = arith.extf %14 : f32 to f64 loc(#loc108)
              %16 = math.absf %15 : f64 loc(#loc110)
              %17 = affine.load %alloca_5[0] : memref<1xf64> loc(#loc111)
              %18 = arith.addf %17, %16 : f64 loc(#loc111)
              affine.store %18, %alloca_5[0] : memref<1xf64> loc(#loc111)
            } loc(#loc109)
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
            affine.store %cst, %alloca_4[0] : memref<1xf64> loc(#loc14)
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
            %12 = affine.load %alloca_13[0] : memref<1xmemref<?xf32>> loc(#loc112)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":212:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc112)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc112)
              %15 = arith.extf %14 : f32 to f64 loc(#loc112)
              %16 = math.absf %15 : f64 loc(#loc114)
              %17 = affine.load %alloca_4[0] : memref<1xf64> loc(#loc115)
              %18 = arith.addf %17, %16 : f64 loc(#loc115)
              affine.store %18, %alloca_4[0] : memref<1xf64> loc(#loc115)
            } loc(#loc113)
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
            affine.store %cst, %alloca_3[0] : memref<1xf64> loc(#loc13)
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
            %12 = affine.load %alloca_12[0] : memref<1xmemref<?xf32>> loc(#loc116)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":215:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc116)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc116)
              %15 = arith.extf %14 : f32 to f64 loc(#loc116)
              %16 = math.absf %15 : f64 loc(#loc118)
              %17 = affine.load %alloca_3[0] : memref<1xf64> loc(#loc119)
              %18 = arith.addf %17, %16 : f64 loc(#loc119)
              affine.store %18, %alloca_3[0] : memref<1xf64> loc(#loc119)
            } loc(#loc117)
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
            affine.store %cst, %alloca_2[0] : memref<1xf64> loc(#loc12)
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
            %12 = affine.load %alloca_11[0] : memref<1xmemref<?xf32>> loc(#loc120)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":218:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc120)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc120)
              %15 = arith.extf %14 : f32 to f64 loc(#loc120)
              %16 = math.absf %15 : f64 loc(#loc122)
              %17 = affine.load %alloca_2[0] : memref<1xf64> loc(#loc123)
              %18 = arith.addf %17, %16 : f64 loc(#loc123)
              affine.store %18, %alloca_2[0] : memref<1xf64> loc(#loc123)
            } loc(#loc121)
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
            affine.store %cst, %alloca_1[0] : memref<1xf64> loc(#loc11)
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
            %12 = affine.load %alloca_10[0] : memref<1xmemref<?xf32>> loc(#loc124)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":221:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc124)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc124)
              %15 = arith.extf %14 : f32 to f64 loc(#loc124)
              %16 = math.absf %15 : f64 loc(#loc126)
              %17 = affine.load %alloca_1[0] : memref<1xf64> loc(#loc127)
              %18 = arith.addf %17, %16 : f64 loc(#loc127)
              affine.store %18, %alloca_1[0] : memref<1xf64> loc(#loc127)
            } loc(#loc125)
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
            affine.store %cst, %alloca_0[0] : memref<1xf64> loc(#loc10)
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
            %12 = affine.load %alloca_9[0] : memref<1xmemref<?xf32>> loc(#loc128)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":224:3) = 0 to %7 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc128)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc128)
              %15 = arith.extf %14 : f32 to f64 loc(#loc128)
              %16 = math.absf %15 : f64 loc(#loc130)
              %17 = affine.load %alloca_0[0] : memref<1xf64> loc(#loc131)
              %18 = arith.addf %17, %16 : f64 loc(#loc131)
              affine.store %18, %alloca_0[0] : memref<1xf64> loc(#loc131)
            } loc(#loc129)
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
            affine.store %cst, %alloca[0] : memref<1xf64> loc(#loc9)
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
            %12 = affine.load %alloca_7[0] : memref<1xmemref<?xf32>> loc(#loc132)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":227:3) = 0 to %9 {
              %13 = polygeist.subindex %12[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc132)
              %14 = affine.load %13[0] : memref<?xf32> loc(#loc132)
              %15 = arith.extf %14 : f32 to f64 loc(#loc132)
              %16 = math.absf %15 : f64 loc(#loc134)
              %17 = affine.load %alloca[0] : memref<1xf64> loc(#loc135)
              %18 = arith.addf %17, %16 : f64 loc(#loc135)
              affine.store %18, %alloca[0] : memref<1xf64> loc(#loc135)
            } loc(#loc133)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %10 = scf.if %true -> (f64) {
      %12 = scf.execute_region -> f64 {
        %13 = scf.if %true -> (f64) {
          %14 = scf.execute_region -> f64 {
            %15 = affine.load %alloca_6[0] : memref<1xf64> loc(#loc136)
            %16 = affine.load %alloca_5[0] : memref<1xf64> loc(#loc137)
            %17 = arith.addf %15, %16 : f64 loc(#loc138)
            %18 = affine.load %alloca_4[0] : memref<1xf64> loc(#loc139)
            %19 = arith.addf %17, %18 : f64 loc(#loc140)
            %20 = affine.load %alloca_3[0] : memref<1xf64> loc(#loc141)
            %21 = arith.addf %19, %20 : f64 loc(#loc142)
            %22 = affine.load %alloca_2[0] : memref<1xf64> loc(#loc143)
            %23 = arith.addf %21, %22 : f64 loc(#loc144)
            %24 = affine.load %alloca_1[0] : memref<1xf64> loc(#loc145)
            %25 = arith.addf %23, %24 : f64 loc(#loc146)
            %26 = affine.load %alloca_0[0] : memref<1xf64> loc(#loc147)
            %27 = arith.addf %25, %26 : f64 loc(#loc148)
            %28 = affine.load %alloca[0] : memref<1xf64> loc(#loc149)
            %29 = arith.addf %27, %28 : f64 loc(#loc150)
            scf.yield %29 : f64 loc(#loc)
          } loc(#loc)
          scf.yield %14 : f64 loc(#loc)
        } else {
          scf.yield %4 : f64 loc(#loc)
        } loc(#loc)
        scf.yield %13 : f64 loc(#loc)
      } loc(#loc)
      scf.yield %12 : f64 loc(#loc)
    } else {
      scf.yield %4 : f64 loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_bench_checksum_d(%10) : (f64) -> () loc(#loc151)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc152)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc152)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc153)
        %13 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc153)
        func.call @carts_phase_timer_start(%12, %13) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc153)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_16[0] : memref<1xmemref<?xf32>> loc(#loc154)
        memref.dealloc %12 : memref<?xf32> loc(#loc154)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_15[0] : memref<1xmemref<?xf32>> loc(#loc155)
        memref.dealloc %12 : memref<?xf32> loc(#loc155)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_14[0] : memref<1xmemref<?xf32>> loc(#loc156)
        memref.dealloc %12 : memref<?xf32> loc(#loc156)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_13[0] : memref<1xmemref<?xf32>> loc(#loc157)
        memref.dealloc %12 : memref<?xf32> loc(#loc157)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_12[0] : memref<1xmemref<?xf32>> loc(#loc158)
        memref.dealloc %12 : memref<?xf32> loc(#loc158)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_11[0] : memref<1xmemref<?xf32>> loc(#loc159)
        memref.dealloc %12 : memref<?xf32> loc(#loc159)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_10[0] : memref<1xmemref<?xf32>> loc(#loc160)
        memref.dealloc %12 : memref<?xf32> loc(#loc160)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_9[0] : memref<1xmemref<?xf32>> loc(#loc161)
        memref.dealloc %12 : memref<?xf32> loc(#loc161)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_8[0] : memref<1xmemref<?xf32>> loc(#loc162)
        memref.dealloc %12 : memref<?xf32> loc(#loc162)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = affine.load %alloca_7[0] : memref<1xmemref<?xf32>> loc(#loc163)
        memref.dealloc %12 : memref<?xf32> loc(#loc163)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %12 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc164)
        func.call @carts_phase_timer_stop(%12) : (memref<?xi8>) -> () loc(#loc164)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc165)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %11 = scf.if %true -> (i32) {
      %12 = scf.execute_region -> i32 {
        %13 = scf.if %true -> (i32) {
          %14 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %14 : i32 loc(#loc)
        } else {
          scf.yield %5 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %13 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %12 : i32 loc(#loc)
    } else {
      scf.yield %5 : i32 loc(#loc)
    } loc(#loc)
    return %11 : i32 loc(#loc166)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc167)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc168)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc169)
  func.func private @init_data(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":153:13), %arg1: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":153:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc171)
    %cst = arith.constant -3.000000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 6.000000e+00 : f32 loc(#loc)
    %0 = arith.index_cast %arg1 : i32 to index loc(#loc170)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %1 = arith.sitofp %arg1 : i32 to f32 loc(#loc172)
            affine.for %arg2 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":155:3) = 0 to %0 {
              %2 = arith.index_cast %arg2 : index to i32 loc(#loc173)
              scf.if %true {
                scf.execute_region {
                  %3 = polygeist.subindex %arg0[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc174)
                  %4 = arith.sitofp %2 : i32 to f32 loc(#loc175)
                  %5 = arith.divf %4, %1 : f32 loc(#loc176)
                  %6 = arith.mulf %5, %cst_0 : f32 loc(#loc177)
                  %7 = arith.addf %6, %cst : f32 loc(#loc178)
                  affine.store %7, %3[0] : memref<?xf32> loc(#loc179)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc173)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc180)
  } loc(#loc170)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc181)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc182)
  func.func private @activate_relu(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":32:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":32:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":32:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc184)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc185)
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
            %2 = arith.index_cast %1 : i32 to index loc(#loc186)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc187)
                          %4 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc188)
                          %5 = affine.load %4[0] : memref<?xf32> loc(#loc188)
                          %6 = arith.cmpf ogt, %5, %cst : f32 loc(#loc189)
                          %7 = scf.if %6 -> (f32) {
                            scf.yield %5 : f32 loc(#loc190)
                          } else {
                            scf.yield %cst : f32 loc(#loc190)
                          } loc(#loc190)
                          affine.store %7, %3[0] : memref<?xf32> loc(#loc191)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc186)
                    } loc(#loc186)
                    omp.yield loc(#loc186)
                  } loc(#loc186)
                } loc(#loc186)
                scf.yield loc(#loc186)
              } loc(#loc186)
              omp.terminator loc(#loc186)
            } loc(#loc186)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc192)
  } loc(#loc183)
  func.func private @activate_leaky(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":43:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":43:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":43:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst_0 = arith.constant 1.000000e-01 : f32 loc(#loc)
    %true = arith.constant true loc(#loc194)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc195)
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
            %2 = arith.index_cast %1 : i32 to index loc(#loc196)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc197)
                          %4 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc198)
                          %5 = affine.load %4[0] : memref<?xf32> loc(#loc198)
                          %6 = arith.cmpf ogt, %5, %cst : f32 loc(#loc199)
                          %7 = scf.if %6 -> (f32) {
                            scf.yield %5 : f32 loc(#loc200)
                          } else {
                            %8 = arith.mulf %5, %cst_0 : f32 loc(#loc201)
                            scf.yield %8 : f32 loc(#loc200)
                          } loc(#loc200)
                          affine.store %7, %3[0] : memref<?xf32> loc(#loc202)
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
    return loc(#loc203)
  } loc(#loc193)
  func.func private @activate_relu6(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":54:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":54:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":54:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 6.000000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc205)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc206)
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
            %2 = arith.index_cast %1 : i32 to index loc(#loc207)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      %3 = "polygeist.undef"() : () -> f32 loc(#loc208)
                      %4 = scf.if %true -> (f32) {
                        %7 = scf.execute_region -> f32 {
                          %8 = scf.if %true -> (f32) {
                            %9 = scf.execute_region -> f32 {
                              %10 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc209)
                              %11 = affine.load %10[0] : memref<?xf32> loc(#loc209)
                              scf.yield %11 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %9 : f32 loc(#loc)
                          } else {
                            scf.yield %3 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %8 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %7 : f32 loc(#loc)
                      } else {
                        scf.yield %3 : f32 loc(#loc)
                      } loc(#loc)
                      %5 = scf.if %true -> (f32) {
                        %7 = scf.execute_region -> f32 {
                          %8 = arith.cmpf ogt, %4, %cst_0 : f32 loc(#loc210)
                          %9 = scf.if %8 -> (f32) {
                            scf.yield %4 : f32 loc(#loc211)
                          } else {
                            scf.yield %cst_0 : f32 loc(#loc211)
                          } loc(#loc211)
                          scf.yield %9 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %7 : f32 loc(#loc)
                      } else {
                        scf.yield %4 : f32 loc(#loc)
                      } loc(#loc)
                      %6 = scf.if %true -> (f32) {
                        %7 = scf.execute_region -> f32 {
                          %8 = arith.cmpf olt, %5, %cst : f32 loc(#loc212)
                          %9 = scf.if %8 -> (f32) {
                            scf.yield %5 : f32 loc(#loc213)
                          } else {
                            scf.yield %cst : f32 loc(#loc213)
                          } loc(#loc213)
                          scf.yield %9 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %7 : f32 loc(#loc)
                      } else {
                        scf.yield %5 : f32 loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          %7 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc214)
                          affine.store %6, %7[0] : memref<?xf32> loc(#loc215)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc207)
                    } loc(#loc207)
                    omp.yield loc(#loc207)
                  } loc(#loc207)
                } loc(#loc207)
                scf.yield loc(#loc207)
              } loc(#loc207)
              omp.terminator loc(#loc207)
            } loc(#loc207)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc216)
  } loc(#loc204)
  func.func private @activate_gelu(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":70:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":70:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":70:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst = arith.constant 1.000000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 5.000000e-01 : f32 loc(#loc)
    %cst_1 = arith.constant 4.471500e-02 : f32 loc(#loc)
    %cst_2 = arith.constant 0.797884583 : f32 loc(#loc)
    %true = arith.constant true loc(#loc218)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc219)
    %1 = "polygeist.undef"() : () -> f32 loc(#loc220)
    %2 = scf.if %true -> (f32) {
      %4 = scf.execute_region -> f32 {
        %5 = scf.if %true -> (f32) {
          %6 = scf.execute_region -> f32 {
            scf.yield %cst_2 : f32 loc(#loc)
          } loc(#loc)
          scf.yield %6 : f32 loc(#loc)
        } else {
          scf.yield %1 : f32 loc(#loc)
        } loc(#loc)
        scf.yield %5 : f32 loc(#loc)
      } loc(#loc)
      scf.yield %4 : f32 loc(#loc)
    } else {
      scf.yield %1 : f32 loc(#loc)
    } loc(#loc)
    %3 = scf.if %true -> (f32) {
      %4 = scf.execute_region -> f32 {
        %5 = scf.if %true -> (f32) {
          %6 = scf.execute_region -> f32 {
            scf.yield %cst_1 : f32 loc(#loc)
          } loc(#loc)
          scf.yield %6 : f32 loc(#loc)
        } else {
          scf.yield %1 : f32 loc(#loc)
        } loc(#loc)
        scf.yield %5 : f32 loc(#loc)
      } loc(#loc)
      scf.yield %4 : f32 loc(#loc)
    } else {
      scf.yield %1 : f32 loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %4 = scf.if %true -> (i32) {
              %6 = scf.execute_region -> i32 {
                scf.yield %arg2 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %6 : i32 loc(#loc)
            } else {
              scf.yield %0 : i32 loc(#loc)
            } loc(#loc)
            %5 = arith.index_cast %4 : i32 to index loc(#loc221)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%5) step (%c1) {
                    scf.execute_region {
                      %6 = scf.if %true -> (f32) {
                        %9 = scf.execute_region -> f32 {
                          %10 = scf.if %true -> (f32) {
                            %11 = scf.execute_region -> f32 {
                              %12 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc222)
                              %13 = affine.load %12[0] : memref<?xf32> loc(#loc222)
                              scf.yield %13 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %11 : f32 loc(#loc)
                          } else {
                            scf.yield %1 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %10 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %9 : f32 loc(#loc)
                      } else {
                        scf.yield %1 : f32 loc(#loc)
                      } loc(#loc)
                      %7 = scf.if %true -> (f32) {
                        %9 = scf.execute_region -> f32 {
                          %10 = scf.if %true -> (f32) {
                            %11 = scf.execute_region -> f32 {
                              %12 = arith.mulf %6, %6 : f32 loc(#loc223)
                              %13 = arith.mulf %12, %6 : f32 loc(#loc224)
                              scf.yield %13 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %11 : f32 loc(#loc)
                          } else {
                            scf.yield %1 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %10 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %9 : f32 loc(#loc)
                      } else {
                        scf.yield %1 : f32 loc(#loc)
                      } loc(#loc)
                      %8 = scf.if %true -> (f32) {
                        %9 = scf.execute_region -> f32 {
                          %10 = scf.if %true -> (f32) {
                            %11 = scf.execute_region -> f32 {
                              %12 = arith.mulf %3, %7 : f32 loc(#loc225)
                              %13 = arith.addf %6, %12 : f32 loc(#loc226)
                              %14 = arith.mulf %2, %13 : f32 loc(#loc227)
                              scf.yield %14 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %11 : f32 loc(#loc)
                          } else {
                            scf.yield %1 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %10 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %9 : f32 loc(#loc)
                      } else {
                        scf.yield %1 : f32 loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          %9 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc228)
                          %10 = arith.mulf %6, %cst_0 : f32 loc(#loc229)
                          %11 = func.call @tanhf(%8) : (f32) -> f32 loc(#loc230)
                          %12 = arith.addf %11, %cst : f32 loc(#loc231)
                          %13 = arith.mulf %10, %12 : f32 loc(#loc232)
                          affine.store %13, %9[0] : memref<?xf32> loc(#loc233)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc221)
                    } loc(#loc221)
                    omp.yield loc(#loc221)
                  } loc(#loc221)
                } loc(#loc221)
                scf.yield loc(#loc221)
              } loc(#loc221)
              omp.terminator loc(#loc221)
            } loc(#loc221)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc234)
  } loc(#loc217)
  func.func private @activate_gelu_fast(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":89:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":89:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":89:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst = arith.constant -1.702000e+00 : f32 loc(#loc)
    %cst_0 = arith.constant 1.000000e+00 : f32 loc(#loc)
    %true = arith.constant true loc(#loc236)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc237)
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
            %2 = arith.index_cast %1 : i32 to index loc(#loc238)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      %3 = "polygeist.undef"() : () -> f32 loc(#loc239)
                      %4 = scf.if %true -> (f32) {
                        %6 = scf.execute_region -> f32 {
                          %7 = scf.if %true -> (f32) {
                            %8 = scf.execute_region -> f32 {
                              %9 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc240)
                              %10 = affine.load %9[0] : memref<?xf32> loc(#loc240)
                              scf.yield %10 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %8 : f32 loc(#loc)
                          } else {
                            scf.yield %3 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %7 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %6 : f32 loc(#loc)
                      } else {
                        scf.yield %3 : f32 loc(#loc)
                      } loc(#loc)
                      %5 = scf.if %true -> (f32) {
                        %6 = scf.execute_region -> f32 {
                          %7 = scf.if %true -> (f32) {
                            %8 = scf.execute_region -> f32 {
                              %9 = arith.mulf %4, %cst : f32 loc(#loc241)
                              %10 = math.exp %9 : f32 loc(#loc242)
                              %11 = arith.addf %10, %cst_0 : f32 loc(#loc243)
                              %12 = arith.divf %cst_0, %11 : f32 loc(#loc244)
                              scf.yield %12 : f32 loc(#loc)
                            } loc(#loc)
                            scf.yield %8 : f32 loc(#loc)
                          } else {
                            scf.yield %3 : f32 loc(#loc)
                          } loc(#loc)
                          scf.yield %7 : f32 loc(#loc)
                        } loc(#loc)
                        scf.yield %6 : f32 loc(#loc)
                      } else {
                        scf.yield %3 : f32 loc(#loc)
                      } loc(#loc)
                      scf.if %true {
                        scf.execute_region {
                          %6 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc245)
                          %7 = arith.mulf %4, %5 : f32 loc(#loc246)
                          affine.store %7, %6[0] : memref<?xf32> loc(#loc247)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc238)
                    } loc(#loc238)
                    omp.yield loc(#loc238)
                  } loc(#loc238)
                } loc(#loc238)
                scf.yield loc(#loc238)
              } loc(#loc238)
              omp.terminator loc(#loc238)
            } loc(#loc238)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc248)
  } loc(#loc235)
  func.func private @activate_sigmoid(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":102:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":102:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":102:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %cst = arith.constant 1.000000e+00 : f32 loc(#loc)
    %true = arith.constant true loc(#loc250)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc251)
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
            %2 = arith.index_cast %1 : i32 to index loc(#loc252)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc253)
                          %4 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc254)
                          %5 = affine.load %4[0] : memref<?xf32> loc(#loc254)
                          %6 = arith.negf %5 : f32 loc(#loc255)
                          %7 = math.exp %6 : f32 loc(#loc256)
                          %8 = arith.addf %7, %cst : f32 loc(#loc257)
                          %9 = arith.divf %cst, %8 : f32 loc(#loc258)
                          affine.store %9, %3[0] : memref<?xf32> loc(#loc259)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc252)
                    } loc(#loc252)
                    omp.yield loc(#loc252)
                  } loc(#loc252)
                } loc(#loc252)
                scf.yield loc(#loc252)
              } loc(#loc252)
              omp.terminator loc(#loc252)
            } loc(#loc252)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc260)
  } loc(#loc249)
  func.func private @activate_tanh(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":113:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":113:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":113:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc262)
    %0 = "polygeist.undef"() : () -> i32 loc(#loc263)
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
            %2 = arith.index_cast %1 : i32 to index loc(#loc264)
            omp.parallel {
              scf.execute_region {
                omp.wsloop {
                  omp.loop_nest (%arg3) : index = (%c0) to (%2) step (%c1) {
                    scf.execute_region {
                      scf.if %true {
                        scf.execute_region {
                          %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc265)
                          %4 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc266)
                          %5 = affine.load %4[0] : memref<?xf32> loc(#loc266)
                          %6 = func.call @tanhf(%5) : (f32) -> f32 loc(#loc267)
                          affine.store %6, %3[0] : memref<?xf32> loc(#loc268)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc264)
                    } loc(#loc264)
                    omp.yield loc(#loc264)
                  } loc(#loc264)
                } loc(#loc264)
                scf.yield loc(#loc264)
              } loc(#loc264)
              omp.terminator loc(#loc264)
            } loc(#loc264)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc269)
  } loc(#loc261)
  func.func private @softmax(%arg0: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":126:13), %arg1: memref<?xf32> loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":126:13), %arg2: i32 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":126:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc271)
    %cst = arith.constant 0.000000e+00 : f32 loc(#loc)
    %0 = arith.index_cast %arg2 : i32 to index loc(#loc270)
    %alloca = memref.alloca() : memref<1xf32> loc(#loc272)
    %1 = "polygeist.undef"() : () -> f32 loc(#loc272)
    affine.store %1, %alloca[0] : memref<1xf32> loc(#loc272)
    %alloca_0 = memref.alloca() : memref<1xf32> loc(#loc273)
    affine.store %1, %alloca_0[0] : memref<1xf32> loc(#loc273)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %2 = affine.load %arg0[0] : memref<?xf32> loc(#loc274)
            affine.store %2, %alloca_0[0] : memref<1xf32> loc(#loc273)
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
            affine.for %arg3 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":131:3) = 1 to %0 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %2 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc276)
                      %3 = affine.load %2[0] : memref<?xf32> loc(#loc276)
                      %4 = affine.load %alloca_0[0] : memref<1xf32> loc(#loc277)
                      %5 = arith.cmpf ogt, %3, %4 : f32 loc(#loc278)
                      scf.if %5 {
                        scf.if %true {
                          scf.execute_region {
                            affine.store %3, %alloca_0[0] : memref<1xf32> loc(#loc280)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc279)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc275)
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
            affine.store %cst, %alloca[0] : memref<1xf32> loc(#loc272)
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
            %2 = affine.load %alloca_0[0] : memref<1xf32> loc(#loc281)
            affine.for %arg3 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":139:3) = 0 to %0 {
              scf.if %true {
                scf.execute_region {
                  %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc283)
                  %4 = polygeist.subindex %arg0[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc284)
                  %5 = affine.load %4[0] : memref<?xf32> loc(#loc284)
                  %6 = arith.subf %5, %2 : f32 loc(#loc285)
                  %7 = math.exp %6 : f32 loc(#loc286)
                  affine.store %7, %3[0] : memref<?xf32> loc(#loc287)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc288)
                  %4 = affine.load %3[0] : memref<?xf32> loc(#loc288)
                  %5 = affine.load %alloca[0] : memref<1xf32> loc(#loc289)
                  %6 = arith.addf %5, %4 : f32 loc(#loc289)
                  affine.store %6, %alloca[0] : memref<1xf32> loc(#loc289)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc282)
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
            %2 = affine.load %alloca[0] : memref<1xf32> loc(#loc290)
            affine.for %arg3 loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":145:3) = 0 to %0 {
              scf.if %true {
                scf.execute_region {
                  %3 = polygeist.subindex %arg1[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc292)
                  %4 = affine.load %3[0] : memref<?xf32> loc(#loc293)
                  %5 = arith.divf %4, %2 : f32 loc(#loc293)
                  affine.store %5, %3[0] : memref<?xf32> loc(#loc293)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc291)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc294)
  } loc(#loc270)
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc295)
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc296)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc297)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc298)
  func.func private @tanhf(f32) -> f32 attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc299)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":163:25)
#loc6 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":161:1)
#loc7 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:3)
#loc8 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":227:8)
#loc9 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":226:3)
#loc10 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":223:3)
#loc11 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":220:3)
#loc12 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":217:3)
#loc13 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":214:3)
#loc14 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":211:3)
#loc15 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":208:3)
#loc16 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":205:3)
#loc17 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":179:3)
#loc18 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":178:3)
#loc19 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":177:3)
#loc20 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":176:3)
#loc21 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":175:3)
#loc22 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":174:3)
#loc23 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":173:3)
#loc24 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":172:3)
#loc25 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":171:3)
#loc26 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":170:3)
#loc27 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc28 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc29 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc30 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":170:18)
#loc31 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":170:34)
#loc32 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":170:41)
#loc33 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":170:39)
#loc34 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":171:21)
#loc35 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":171:37)
#loc36 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":171:44)
#loc37 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":171:42)
#loc38 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":172:22)
#loc39 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":172:38)
#loc40 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":172:45)
#loc41 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":172:43)
#loc42 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":173:22)
#loc43 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":173:38)
#loc44 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":173:45)
#loc45 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":173:43)
#loc46 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":174:21)
#loc47 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":174:37)
#loc48 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":174:44)
#loc49 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":174:42)
#loc50 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":175:26)
#loc51 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":175:42)
#loc52 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":175:49)
#loc53 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":175:47)
#loc54 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":176:24)
#loc55 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":176:40)
#loc56 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":176:47)
#loc57 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":176:45)
#loc58 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":177:21)
#loc59 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":177:37)
#loc60 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":177:44)
#loc61 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":177:42)
#loc62 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":178:26)
#loc63 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":178:42)
#loc64 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":178:57)
#loc65 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":178:55)
#loc66 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":179:27)
#loc67 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":179:43)
#loc68 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":179:58)
#loc69 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":179:56)
#loc70 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":182:13)
#loc71 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":182:3)
#loc72 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":183:13)
#loc73 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":183:3)
#loc74 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc75 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc76 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":141:40)
#loc78 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":190:19)
#loc79 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":190:26)
#loc80 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":190:5)
#loc81 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":191:20)
#loc82 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":191:27)
#loc83 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":191:5)
#loc84 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":192:20)
#loc85 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":192:27)
#loc86 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":192:5)
#loc87 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":193:19)
#loc88 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":193:26)
#loc89 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":193:5)
#loc90 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":194:24)
#loc91 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":194:31)
#loc92 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":194:5)
#loc93 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":195:22)
#loc94 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":195:29)
#loc95 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":195:5)
#loc96 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":196:19)
#loc97 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":196:26)
#loc98 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":196:5)
#loc99 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":197:13)
#loc100 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":197:28)
#loc101 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":197:5)
#loc102 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":142:40)
#loc103 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc104 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":206:56)
#loc106 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":206:51)
#loc107 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":206:48)
#loc108 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":209:57)
#loc110 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":209:52)
#loc111 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":209:49)
#loc112 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":212:57)
#loc114 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":212:52)
#loc115 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":212:49)
#loc116 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":215:56)
#loc118 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":215:51)
#loc119 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":215:48)
#loc120 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":218:61)
#loc122 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":218:56)
#loc123 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":218:53)
#loc124 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":221:59)
#loc126 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":221:54)
#loc127 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":221:51)
#loc128 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":224:56)
#loc130 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":224:51)
#loc131 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":224:48)
#loc132 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":227:67)
#loc134 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":227:62)
#loc135 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":227:59)
#loc136 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:27)
#loc137 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:43)
#loc138 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:41)
#loc139 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:60)
#loc140 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:58)
#loc141 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":230:27)
#loc142 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":229:75)
#loc143 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":230:43)
#loc144 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":230:41)
#loc145 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":230:64)
#loc146 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":230:62)
#loc147 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":231:27)
#loc148 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":230:81)
#loc149 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":231:43)
#loc150 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":231:41)
#loc151 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc152 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc153 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc154 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":237:3)
#loc155 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":238:3)
#loc156 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":239:3)
#loc157 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":240:3)
#loc158 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":241:3)
#loc159 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":242:3)
#loc160 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":243:3)
#loc161 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":244:3)
#loc162 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":245:3)
#loc163 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":246:3)
#loc164 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc165 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc166 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":253:1)
#loc167 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc168 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc169 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc171 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":153:1)
#loc172 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:39)
#loc174 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:5)
#loc175 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:28)
#loc176 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:37)
#loc177 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:25)
#loc178 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:18)
#loc179 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":157:10)
#loc180 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":159:1)
#loc181 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc182 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc184 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":32:1)
#loc185 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":35:3)
#loc186 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":34:1)
#loc187 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":36:5)
#loc188 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":36:18)
#loc189 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":36:27)
#loc190 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":36:17)
#loc191 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":36:15)
#loc192 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":38:1)
#loc194 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":43:1)
#loc195 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":46:3)
#loc196 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":45:1)
#loc197 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":47:5)
#loc198 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":47:18)
#loc199 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":47:27)
#loc200 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":47:17)
#loc201 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":47:50)
#loc202 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":47:15)
#loc203 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":49:1)
#loc205 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":54:1)
#loc206 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":57:3)
#loc207 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":56:1)
#loc208 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":58:5)
#loc209 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":58:17)
#loc210 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":59:16)
#loc211 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":59:11)
#loc212 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":60:16)
#loc213 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":60:11)
#loc214 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":61:5)
#loc215 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":61:15)
#loc216 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":63:1)
#loc218 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":70:1)
#loc219 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":76:3)
#loc220 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":72:3)
#loc221 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":75:1)
#loc222 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":77:17)
#loc223 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":78:22)
#loc224 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":78:28)
#loc225 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":79:49)
#loc226 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":79:41)
#loc227 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":79:34)
#loc228 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":80:5)
#loc229 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":80:22)
#loc230 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":80:38)
#loc231 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":80:36)
#loc232 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":80:28)
#loc233 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":80:15)
#loc234 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":82:1)
#loc236 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":89:1)
#loc237 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":92:3)
#loc238 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":91:1)
#loc239 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":94:5)
#loc240 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":93:17)
#loc241 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":94:49)
#loc242 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":94:36)
#loc243 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":94:34)
#loc244 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":94:26)
#loc245 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":95:5)
#loc246 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":95:21)
#loc247 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":95:15)
#loc248 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":97:1)
#loc250 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":102:1)
#loc251 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":105:3)
#loc252 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":104:1)
#loc253 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:5)
#loc254 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:38)
#loc255 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:37)
#loc256 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:32)
#loc257 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:30)
#loc258 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:22)
#loc259 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":106:15)
#loc260 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":108:1)
#loc262 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":113:1)
#loc263 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":116:3)
#loc264 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":115:1)
#loc265 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":117:5)
#loc266 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":117:23)
#loc267 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":117:17)
#loc268 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":117:15)
#loc269 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":119:1)
#loc271 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":126:1)
#loc272 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":138:3)
#loc273 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":130:3)
#loc274 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":130:19)
#loc276 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":132:9)
#loc277 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":132:20)
#loc278 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":132:18)
#loc279 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":132:5)
#loc280 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":133:15)
#loc281 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":140:33)
#loc283 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":140:5)
#loc284 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":140:22)
#loc285 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":140:31)
#loc286 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":140:17)
#loc287 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":140:15)
#loc288 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":141:12)
#loc289 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":141:9)
#loc290 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":146:18)
#loc292 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":146:5)
#loc293 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":146:15)
#loc294 = loc("/home/raherrer/projects/carts/external/carts-benchmarks/ml-kernels/activations/activations.c":148:1)
#loc295 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":134:6)
#loc296 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":137:6)
#loc297 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc298 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
#loc299 = loc("-":199:1)
