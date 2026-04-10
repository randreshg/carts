#loc5 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":71:5)
#loc31 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":85:3)
#loc37 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":94:3)
#loc38 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":95:5)
#loc43 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":103:3)
#loc51 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":113:3)
#loc59 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":121:3)
#loc70 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":28:13)
#loc73 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":31:3)
#loc75 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":32:5)
#loc83 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":55:13)
#loc96 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":61:5)
#map = affine_map<()[s0] -> (s0 - 1)>
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("convolution-2d\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":71:5), %arg1: memref<?xmemref<?xi8>> loc("benchmarks/polybench/convolution-2d/convolution-2d.c":71:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %false = arith.constant false loc(#loc)
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %cst_0 = arith.constant 0.000000e+00 : f32 loc(#loc)
    %c1024_i32 = arith.constant 1024 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc6)
    %4 = "polygeist.undef"() : () -> i32 loc(#loc7)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc8)
    %5 = "polygeist.undef"() : () -> f64 loc(#loc8)
    affine.store %5, %alloca[0] : memref<1xf64> loc(#loc8)
    %alloca_1 = memref.alloca() : memref<1xmemref<?xmemref<?xf32>>> loc(#loc9)
    %alloca_2 = memref.alloca() : memref<1xmemref<?xmemref<?xf32>>> loc(#loc9)
    scf.if %true {
      scf.execute_region {
        func.call @carts_benchmarks_start() : () -> () loc(#loc10)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc11)
        func.call @carts_e2e_timer_start(%11) : (memref<?xi8>) -> () loc(#loc11)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc12)
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc12)
        func.call @carts_phase_timer_start(%11, %12) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc12)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %6 = scf.if %true -> (i32) {
      %11 = scf.execute_region -> i32 {
        %12 = scf.if %true -> (i32) {
          %13 = scf.execute_region -> i32 {
            scf.yield %c1024_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %13 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %12 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %11 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %7 = arith.index_cast %6 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %11 = scf.execute_region -> memref<?xmemref<?xf32>> {
            %12 = polygeist.typeSize memref<?xf32> : index loc(#loc13)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc14)
            %14 = arith.index_cast %12 : index to i64 loc(#loc15)
            %15 = arith.muli %13, %14 : i64 loc(#loc16)
            %16 = arith.index_cast %15 : i64 to index loc(#loc13)
            %17 = arith.divui %16, %12 : index loc(#loc13)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf32>> loc(#loc13)
            affine.store %alloc, %alloca_2[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf32>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %11 = scf.execute_region -> memref<?xmemref<?xf32>> {
            %12 = polygeist.typeSize memref<?xf32> : index loc(#loc17)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc18)
            %14 = arith.index_cast %12 : index to i64 loc(#loc19)
            %15 = arith.muli %13, %14 : i64 loc(#loc20)
            %16 = arith.index_cast %15 : i64 to index loc(#loc17)
            %17 = arith.divui %16, %12 : index loc(#loc17)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf32>> loc(#loc17)
            affine.store %alloc, %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf32>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc21)
            %12 = polygeist.typeSize f32 : index loc(#loc22)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc23)
            %14 = arith.index_cast %12 : index to i64 loc(#loc24)
            %15 = arith.muli %13, %14 : i64 loc(#loc25)
            %16 = arith.index_cast %15 : i64 to index loc(#loc22)
            %17 = arith.divui %16, %12 : index loc(#loc22)
            %18 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc26)
            %19 = polygeist.typeSize f32 : index loc(#loc27)
            %20 = arith.extsi %6 : i32 to i64 loc(#loc28)
            %21 = arith.index_cast %19 : index to i64 loc(#loc29)
            %22 = arith.muli %20, %21 : i64 loc(#loc30)
            %23 = arith.index_cast %22 : i64 to index loc(#loc27)
            %24 = arith.divui %23, %19 : index loc(#loc27)
            affine.for %arg2 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":85:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %25 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc21)
                  %alloc = memref.alloc(%17) : memref<?xf32> loc(#loc22)
                  affine.store %alloc, %25[0] : memref<?xmemref<?xf32>> loc(#loc32)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %25 = polygeist.subindex %18[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc26)
                  %alloc = memref.alloc(%24) : memref<?xf32> loc(#loc27)
                  affine.store %alloc, %25[0] : memref<?xmemref<?xf32>> loc(#loc33)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc31)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc34)
        func.call @init_array(%6, %6, %11) : (i32, i32, memref<?xmemref<?xf32>>) -> () loc(#loc35)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc36)
            affine.for %arg2 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":94:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  scf.if %true {
                    scf.execute_region {
                      %12 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc36)
                      affine.for %arg3 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":95:5) = 0 to %7 {
                        scf.if %true {
                          scf.execute_region {
                            %13 = affine.load %12[0] : memref<?xmemref<?xf32>> loc(#loc36)
                            %14 = polygeist.subindex %13[%arg3] () : memref<?xf32> -> memref<?xf32> loc(#loc36)
                            affine.store %cst_0, %14[0] : memref<?xf32> loc(#loc39)
                            scf.yield loc(#loc)
                          } loc(#loc)
                        } loc(#loc)
                      } loc(#loc38)
                      scf.yield loc(#loc)
                    } loc(#loc)
                  } loc(#loc)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc37)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc40)
        func.call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc40)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc41)
        func.call @carts_kernel_timer_start(%11) : (memref<?xi8>) -> () loc(#loc41)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc42)
            affine.for %arg2 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":103:3) = 0 to 1 {
              scf.if %true {
                scf.execute_region {
                  %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc44)
                  %13 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc45)
                  func.call @kernel_conv2d(%6, %6, %12, %13) : (i32, i32, memref<?xmemref<?xf32>>, memref<?xmemref<?xf32>>) -> () loc(#loc46)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  func.call @carts_kernel_timer_accum(%11) : (memref<?xi8>) -> () loc(#loc42)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc43)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc47)
        func.call @carts_kernel_timer_print(%11) : (memref<?xi8>) -> () loc(#loc47)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc48)
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc48)
        func.call @carts_phase_timer_start(%11, %12) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc48)
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
    %8 = scf.if %true -> (i32) {
      %11 = scf.execute_region -> i32 {
        %12 = scf.if %true -> (i32) {
          %13 = scf.execute_region -> i32 {
            %14 = scf.if %false -> (i32) {
              scf.yield %6 : i32 loc(#loc49)
            } else {
              scf.yield %6 : i32 loc(#loc49)
            } loc(#loc49)
            scf.yield %14 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %13 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %12 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %11 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    %9 = arith.index_cast %8 : i32 to index loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc50)
            affine.for %arg2 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":113:3) = 0 to %9 {
              scf.if %true {
                scf.execute_region {
                  %12 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc50)
                  %13 = affine.load %12[0] : memref<?xmemref<?xf32>> loc(#loc50)
                  %14 = polygeist.subindex %13[%arg2] () : memref<?xf32> -> memref<?xf32> loc(#loc50)
                  %15 = affine.load %14[0] : memref<?xf32> loc(#loc50)
                  %16 = arith.extf %15 : f32 to f64 loc(#loc50)
                  %17 = affine.load %alloca[0] : memref<1xf64> loc(#loc52)
                  %18 = arith.addf %17, %16 : f64 loc(#loc52)
                  affine.store %18, %alloca[0] : memref<1xf64> loc(#loc52)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc51)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca[0] : memref<1xf64> loc(#loc53)
        func.call @carts_bench_checksum_d(%11) : (f64) -> () loc(#loc54)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc55)
        func.call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc55)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc56)
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc56)
        func.call @carts_phase_timer_start(%11, %12) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc56)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc57)
            %12 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc58)
            affine.for %arg2 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":121:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %13 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc57)
                  %14 = affine.load %13[0] : memref<?xmemref<?xf32>> loc(#loc60)
                  memref.dealloc %14 : memref<?xf32> loc(#loc60)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %13 = polygeist.subindex %12[%arg2] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc58)
                  %14 = affine.load %13[0] : memref<?xmemref<?xf32>> loc(#loc61)
                  memref.dealloc %14 : memref<?xf32> loc(#loc61)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc59)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc62)
        memref.dealloc %11 : memref<?xmemref<?xf32>> loc(#loc62)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf32>>> loc(#loc63)
        memref.dealloc %11 : memref<?xmemref<?xf32>> loc(#loc63)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc64)
        func.call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc64)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc65)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    %10 = scf.if %true -> (i32) {
      %11 = scf.execute_region -> i32 {
        %12 = scf.if %true -> (i32) {
          %13 = scf.execute_region -> i32 {
            scf.yield %c0_i32 : i32 loc(#loc)
          } loc(#loc)
          scf.yield %13 : i32 loc(#loc)
        } else {
          scf.yield %4 : i32 loc(#loc)
        } loc(#loc)
        scf.yield %12 : i32 loc(#loc)
      } loc(#loc)
      scf.yield %11 : i32 loc(#loc)
    } else {
      scf.yield %4 : i32 loc(#loc)
    } loc(#loc)
    return %10 : i32 loc(#loc66)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc67)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc68)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc69)
  func.func private @init_array(%arg0: i32 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":28:13), %arg1: i32 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":28:13), %arg2: memref<?xmemref<?xf32>> loc("benchmarks/polybench/convolution-2d/convolution-2d.c":28:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc71)
    %0 = arith.index_cast %arg0 : i32 to index loc(#loc70)
    %1 = arith.index_cast %arg1 : i32 to index loc(#loc70)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %2 = arith.sitofp %arg1 : i32 to f32 loc(#loc72)
            affine.for %arg3 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":31:3) = 0 to %0 {
              %3 = arith.index_cast %arg3 : index to i32 loc(#loc73)
              scf.if %true {
                scf.execute_region {
                  %4 = polygeist.subindex %arg2[%arg3] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc74)
                  affine.for %arg4 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":32:5) = 0 to %1 {
                    %5 = arith.index_cast %arg4 : index to i32 loc(#loc75)
                    scf.if %true {
                      scf.execute_region {
                        %6 = affine.load %4[0] : memref<?xmemref<?xf32>> loc(#loc74)
                        %7 = polygeist.subindex %6[%arg4] () : memref<?xf32> -> memref<?xf32> loc(#loc74)
                        %8 = arith.addi %3, %5 : i32 loc(#loc76)
                        %9 = arith.sitofp %8 : i32 to f32 loc(#loc77)
                        %10 = arith.divf %9, %2 : f32 loc(#loc78)
                        affine.store %10, %7[0] : memref<?xf32> loc(#loc79)
                        scf.yield loc(#loc)
                      } loc(#loc)
                    } loc(#loc)
                  } loc(#loc75)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc73)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc80)
  } loc(#loc70)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc81)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc82)
  func.func private @kernel_conv2d(%arg0: i32 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":55:13), %arg1: i32 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":55:13), %arg2: memref<?xmemref<?xf32>> loc("benchmarks/polybench/convolution-2d/convolution-2d.c":55:13), %arg3: memref<?xmemref<?xf32>> loc("benchmarks/polybench/convolution-2d/convolution-2d.c":55:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %c-1_i32 = arith.constant -1 : i32 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %true = arith.constant true loc(#loc84)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %cst = arith.constant 2.000000e-01 : f64 loc(#loc)
    %cst_0 = arith.constant 5.000000e-01 : f64 loc(#loc)
    %cst_1 = arith.constant -8.000000e-01 : f64 loc(#loc)
    %cst_2 = arith.constant -3.000000e-01 : f64 loc(#loc)
    %cst_3 = arith.constant 6.000000e-01 : f64 loc(#loc)
    %cst_4 = arith.constant -9.000000e-01 : f64 loc(#loc)
    %cst_5 = arith.constant 4.000000e-01 : f64 loc(#loc)
    %cst_6 = arith.constant 0.69999999999999996 : f64 loc(#loc)
    %cst_7 = arith.constant 1.000000e-01 : f64 loc(#loc)
    %0 = arith.index_cast %arg1 : i32 to index loc(#loc83)
    %1 = "polygeist.undef"() : () -> i32 loc(#loc85)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %2 = scf.if %true -> (i32) {
              %4 = scf.execute_region -> i32 {
                %5 = arith.addi %arg0, %c-1_i32 : i32 loc(#loc86)
                scf.yield %5 : i32 loc(#loc)
              } loc(#loc)
              scf.yield %4 : i32 loc(#loc)
            } else {
              scf.yield %1 : i32 loc(#loc)
            } loc(#loc)
            %3 = arith.index_cast %2 : i32 to index loc(#loc87)
            omp.parallel {
              scf.execute_region {
                omp.wsloop schedule(static) {
                  omp.loop_nest (%arg4) : index = (%c1) to (%3) step (%c1) {
                    scf.execute_region {
                      %4 = arith.index_cast %arg4 : index to i32 loc(#loc87)
                      scf.if %true {
                        scf.execute_region {
                          scf.if %true {
                            scf.execute_region {
                              %5 = polygeist.subindex %arg3[%arg4] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc88)
                              %6 = arith.addi %4, %c-1_i32 : i32 loc(#loc89)
                              %7 = arith.index_cast %6 : i32 to index loc(#loc90)
                              %8 = polygeist.subindex %arg2[%7] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc91)
                              %9 = polygeist.subindex %arg2[%arg4] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc92)
                              %10 = arith.addi %4, %c1_i32 : i32 loc(#loc93)
                              %11 = arith.index_cast %10 : i32 to index loc(#loc94)
                              %12 = polygeist.subindex %arg2[%11] () : memref<?xmemref<?xf32>> -> memref<?xmemref<?xf32>> loc(#loc95)
                              affine.for %arg5 loc("benchmarks/polybench/convolution-2d/convolution-2d.c":61:5) = 1 to #map()[%0] {
                                scf.execute_region {
                                  %13 = arith.index_cast %arg5 : index to i32 loc(#loc96)
                                  scf.if %true {
                                    scf.execute_region {
                                      %14 = affine.load %5[0] : memref<?xmemref<?xf32>> loc(#loc88)
                                      %15 = polygeist.subindex %14[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc88)
                                      %16 = affine.load %8[0] : memref<?xmemref<?xf32>> loc(#loc91)
                                      %17 = arith.addi %13, %c-1_i32 : i32 loc(#loc97)
                                      %18 = arith.index_cast %17 : i32 to index loc(#loc98)
                                      %19 = polygeist.subindex %16[%18] () : memref<?xf32> -> memref<?xf32> loc(#loc91)
                                      %20 = affine.load %19[0] : memref<?xf32> loc(#loc91)
                                      %21 = arith.extf %20 : f32 to f64 loc(#loc91)
                                      %22 = arith.mulf %21, %cst : f64 loc(#loc99)
                                      %23 = polygeist.subindex %16[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc100)
                                      %24 = affine.load %23[0] : memref<?xf32> loc(#loc100)
                                      %25 = arith.extf %24 : f32 to f64 loc(#loc100)
                                      %26 = arith.mulf %25, %cst_0 : f64 loc(#loc101)
                                      %27 = arith.addf %22, %26 : f64 loc(#loc102)
                                      %28 = arith.addi %13, %c1_i32 : i32 loc(#loc103)
                                      %29 = arith.index_cast %28 : i32 to index loc(#loc104)
                                      %30 = polygeist.subindex %16[%29] () : memref<?xf32> -> memref<?xf32> loc(#loc105)
                                      %31 = affine.load %30[0] : memref<?xf32> loc(#loc105)
                                      %32 = arith.extf %31 : f32 to f64 loc(#loc105)
                                      %33 = arith.mulf %32, %cst_1 : f64 loc(#loc106)
                                      %34 = arith.addf %27, %33 : f64 loc(#loc107)
                                      %35 = affine.load %9[0] : memref<?xmemref<?xf32>> loc(#loc92)
                                      %36 = polygeist.subindex %35[%18] () : memref<?xf32> -> memref<?xf32> loc(#loc92)
                                      %37 = affine.load %36[0] : memref<?xf32> loc(#loc92)
                                      %38 = arith.extf %37 : f32 to f64 loc(#loc92)
                                      %39 = arith.mulf %38, %cst_2 : f64 loc(#loc108)
                                      %40 = arith.addf %34, %39 : f64 loc(#loc109)
                                      %41 = polygeist.subindex %35[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc110)
                                      %42 = affine.load %41[0] : memref<?xf32> loc(#loc110)
                                      %43 = arith.extf %42 : f32 to f64 loc(#loc110)
                                      %44 = arith.mulf %43, %cst_3 : f64 loc(#loc111)
                                      %45 = arith.addf %40, %44 : f64 loc(#loc112)
                                      %46 = polygeist.subindex %35[%29] () : memref<?xf32> -> memref<?xf32> loc(#loc113)
                                      %47 = affine.load %46[0] : memref<?xf32> loc(#loc113)
                                      %48 = arith.extf %47 : f32 to f64 loc(#loc113)
                                      %49 = arith.mulf %48, %cst_4 : f64 loc(#loc114)
                                      %50 = arith.addf %45, %49 : f64 loc(#loc115)
                                      %51 = affine.load %12[0] : memref<?xmemref<?xf32>> loc(#loc95)
                                      %52 = polygeist.subindex %51[%18] () : memref<?xf32> -> memref<?xf32> loc(#loc95)
                                      %53 = affine.load %52[0] : memref<?xf32> loc(#loc95)
                                      %54 = arith.extf %53 : f32 to f64 loc(#loc95)
                                      %55 = arith.mulf %54, %cst_5 : f64 loc(#loc116)
                                      %56 = arith.addf %50, %55 : f64 loc(#loc117)
                                      %57 = polygeist.subindex %51[%arg5] () : memref<?xf32> -> memref<?xf32> loc(#loc118)
                                      %58 = affine.load %57[0] : memref<?xf32> loc(#loc118)
                                      %59 = arith.extf %58 : f32 to f64 loc(#loc118)
                                      %60 = arith.mulf %59, %cst_6 : f64 loc(#loc119)
                                      %61 = arith.addf %56, %60 : f64 loc(#loc120)
                                      %62 = polygeist.subindex %51[%29] () : memref<?xf32> -> memref<?xf32> loc(#loc121)
                                      %63 = affine.load %62[0] : memref<?xf32> loc(#loc121)
                                      %64 = arith.extf %63 : f32 to f64 loc(#loc121)
                                      %65 = arith.mulf %64, %cst_7 : f64 loc(#loc122)
                                      %66 = arith.addf %61, %65 : f64 loc(#loc123)
                                      %67 = arith.truncf %66 : f64 to f32 loc(#loc124)
                                      affine.store %67, %15[0] : memref<?xf32> loc(#loc125)
                                      scf.yield loc(#loc)
                                    } loc(#loc)
                                  } loc(#loc)
                                  scf.yield loc(#loc96)
                                } loc(#loc96)
                              } loc(#loc96)
                              scf.yield loc(#loc)
                            } loc(#loc)
                          } loc(#loc)
                          scf.yield loc(#loc)
                        } loc(#loc)
                      } loc(#loc)
                      scf.yield loc(#loc87)
                    } loc(#loc87)
                    omp.yield loc(#loc87)
                  } loc(#loc87)
                } loc(#loc87)
                scf.yield loc(#loc87)
              } loc(#loc87)
              omp.terminator loc(#loc87)
            } loc(#loc87)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc126)
  } loc(#loc83)
  func.func private @carts_kernel_timer_accum(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc127)
  func.func private @carts_kernel_timer_print(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc128)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc129)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc130)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":74:25)
#loc6 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":71:1)
#loc7 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":121:8)
#loc8 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":111:3)
#loc9 = loc("benchmarks/polybench/convolution-2d/convolution-2d.h":49:21)
#loc10 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc11 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc12 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc13 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":82:19)
#loc14 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":82:40)
#loc15 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":82:45)
#loc16 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":82:43)
#loc17 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":83:19)
#loc18 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":83:40)
#loc19 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":83:45)
#loc20 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":83:43)
#loc21 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":86:5)
#loc22 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":86:12)
#loc23 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":86:32)
#loc24 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":86:37)
#loc25 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":86:35)
#loc26 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":87:5)
#loc27 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":87:12)
#loc28 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":87:32)
#loc29 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":87:37)
#loc30 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":87:35)
#loc32 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":86:10)
#loc33 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":87:10)
#loc34 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":91:22)
#loc35 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":91:3)
#loc36 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":96:7)
#loc39 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":96:15)
#loc40 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc41 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc42 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":141:40)
#loc44 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":104:27)
#loc45 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":104:30)
#loc46 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":104:5)
#loc47 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":142:40)
#loc48 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc49 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":112:14)
#loc50 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":114:17)
#loc52 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":114:14)
#loc53 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":116:24)
#loc54 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc55 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc56 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc57 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":122:10)
#loc58 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":123:10)
#loc60 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":122:5)
#loc61 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":123:5)
#loc62 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":125:3)
#loc63 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":126:3)
#loc64 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc65 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc66 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":133:1)
#loc67 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc68 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc69 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc71 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":28:1)
#loc72 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":33:39)
#loc74 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":33:7)
#loc76 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":33:32)
#loc77 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":33:18)
#loc78 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":33:37)
#loc79 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":33:15)
#loc80 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":35:1)
#loc81 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc82 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc84 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":55:1)
#loc85 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":60:3)
#loc86 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":60:26)
#loc87 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":59:1)
#loc88 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:7)
#loc89 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:27)
#loc90 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:30)
#loc91 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:23)
#loc92 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:49)
#loc93 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:48)
#loc94 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:51)
#loc95 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:44)
#loc97 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:34)
#loc98 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:37)
#loc99 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:21)
#loc100 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:47)
#loc101 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:45)
#loc102 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:39)
#loc103 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:35)
#loc104 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:38)
#loc105 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:24)
#loc106 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:22)
#loc107 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:59)
#loc108 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:47)
#loc109 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:40)
#loc110 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:69)
#loc111 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:67)
#loc112 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:61)
#loc113 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:24)
#loc114 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:22)
#loc115 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":63:77)
#loc116 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:42)
#loc117 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:36)
#loc118 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:68)
#loc119 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:66)
#loc120 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:60)
#loc121 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":65:23)
#loc122 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":65:21)
#loc123 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":64:80)
#loc124 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:17)
#loc125 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":62:15)
#loc126 = loc("benchmarks/polybench/convolution-2d/convolution-2d.c":69:1)
#loc127 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":134:6)
#loc128 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":137:6)
#loc129 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc130 = loc("/home/raherrer/projects/carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
