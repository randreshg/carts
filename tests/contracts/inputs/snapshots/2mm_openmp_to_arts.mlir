#loc5 = loc("benchmarks/polybench/2mm/2mm.c":98:5)
#loc48 = loc("benchmarks/polybench/2mm/2mm.c":119:3)
#loc57 = loc("benchmarks/polybench/2mm/2mm.c":124:3)
#loc64 = loc("benchmarks/polybench/2mm/2mm.c":127:3)
#loc87 = loc("benchmarks/polybench/2mm/2mm.c":144:3)
#loc109 = loc("benchmarks/polybench/2mm/2mm.c":35:13)
#loc114 = loc("benchmarks/polybench/2mm/2mm.c":42:3)
#loc117 = loc("benchmarks/polybench/2mm/2mm.c":43:5)
#loc123 = loc("benchmarks/polybench/2mm/2mm.c":45:3)
#loc126 = loc("benchmarks/polybench/2mm/2mm.c":46:5)
#loc133 = loc("benchmarks/polybench/2mm/2mm.c":48:3)
#loc136 = loc("benchmarks/polybench/2mm/2mm.c":49:5)
#loc143 = loc("benchmarks/polybench/2mm/2mm.c":51:3)
#loc146 = loc("benchmarks/polybench/2mm/2mm.c":52:5)
#loc155 = loc("benchmarks/polybench/2mm/2mm.c":72:13)
#loc163 = loc("benchmarks/polybench/2mm/2mm.c":82:7)
#loc165 = loc("benchmarks/polybench/2mm/2mm.c":84:9)
#loc174 = loc("benchmarks/polybench/2mm/2mm.c":89:7)
#loc176 = loc("benchmarks/polybench/2mm/2mm.c":91:9)
#loc183 = loc("benchmarks/polybench/2mm/2mm.c":24:13)
#loc187 = loc("benchmarks/polybench/2mm/2mm.c":28:3)
module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr<270> = dense<32> : vector<4xi64>, !llvm.ptr<271> = dense<32> : vector<4xi64>, !llvm.ptr<272> = dense<64> : vector<4xi64>, i64 = dense<64> : vector<2xi64>, i128 = dense<128> : vector<2xi64>, f80 = dense<128> : vector<2xi64>, !llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little", "dlti.mangling_mode" = "e", "dlti.legal_int_widths" = array<i32: 8, 16, 32, 64>, "dlti.stack_alignment" = 128 : i64>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str3("cleanup\00") {addr_space = 0 : i32} loc(#loc1)
  llvm.mlir.global internal constant @str2("verification\00") {addr_space = 0 : i32} loc(#loc2)
  llvm.mlir.global internal constant @str1("startup\00") {addr_space = 0 : i32} loc(#loc3)
  llvm.mlir.global internal constant @str0("2mm\00") {addr_space = 0 : i32} loc(#loc4)
  func.func @main(%arg0: i32 loc("benchmarks/polybench/2mm/2mm.c":98:5), %arg1: memref<?xmemref<?xi8>> loc("benchmarks/polybench/2mm/2mm.c":98:5)) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %false = arith.constant false loc(#loc)
    %0 = llvm.mlir.addressof @str3 : !llvm.ptr loc(#loc)
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %1 = llvm.mlir.addressof @str2 : !llvm.ptr loc(#loc)
    %c64_i32 = arith.constant 64 : i32 loc(#loc)
    %2 = llvm.mlir.addressof @str1 : !llvm.ptr loc(#loc)
    %c0_i32 = arith.constant 0 : i32 loc(#loc)
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr loc(#loc)
    %true = arith.constant true loc(#loc6)
    %4 = "polygeist.undef"() : () -> i32 loc(#loc7)
    %alloca = memref.alloca() : memref<1xf64> loc(#loc8)
    %5 = "polygeist.undef"() : () -> f64 loc(#loc8)
    affine.store %5, %alloca[0] : memref<1xf64> loc(#loc8)
    %alloca_0 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    %alloca_1 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    %alloca_2 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    %alloca_3 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    %alloca_4 = memref.alloca() : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
    %alloca_5 = memref.alloca() : memref<1xf64> loc(#loc9)
    affine.store %5, %alloca_5[0] : memref<1xf64> loc(#loc9)
    %alloca_6 = memref.alloca() : memref<1xf64> loc(#loc9)
    affine.store %5, %alloca_6[0] : memref<1xf64> loc(#loc9)
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
            scf.yield %c64_i32 : i32 loc(#loc)
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
          %11 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %12 = polygeist.typeSize memref<?xf64> : index loc(#loc13)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc14)
            %14 = arith.index_cast %12 : index to i64 loc(#loc15)
            %15 = arith.muli %13, %14 : i64 loc(#loc16)
            %16 = arith.index_cast %15 : i64 to index loc(#loc13)
            %17 = arith.divui %16, %12 : index loc(#loc13)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf64>> loc(#loc13)
            affine.store %alloc, %alloca_4[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %11 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %12 = polygeist.typeSize memref<?xf64> : index loc(#loc17)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc18)
            %14 = arith.index_cast %12 : index to i64 loc(#loc19)
            %15 = arith.muli %13, %14 : i64 loc(#loc20)
            %16 = arith.index_cast %15 : i64 to index loc(#loc17)
            %17 = arith.divui %16, %12 : index loc(#loc17)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf64>> loc(#loc17)
            affine.store %alloc, %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %11 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %12 = polygeist.typeSize memref<?xf64> : index loc(#loc21)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc22)
            %14 = arith.index_cast %12 : index to i64 loc(#loc23)
            %15 = arith.muli %13, %14 : i64 loc(#loc24)
            %16 = arith.index_cast %15 : i64 to index loc(#loc21)
            %17 = arith.divui %16, %12 : index loc(#loc21)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf64>> loc(#loc21)
            affine.store %alloc, %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %11 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %12 = polygeist.typeSize memref<?xf64> : index loc(#loc25)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc26)
            %14 = arith.index_cast %12 : index to i64 loc(#loc27)
            %15 = arith.muli %13, %14 : i64 loc(#loc28)
            %16 = arith.index_cast %15 : i64 to index loc(#loc25)
            %17 = arith.divui %16, %12 : index loc(#loc25)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf64>> loc(#loc25)
            affine.store %alloc, %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          %11 = scf.execute_region -> memref<?xmemref<?xf64>> {
            %12 = polygeist.typeSize memref<?xf64> : index loc(#loc29)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc30)
            %14 = arith.index_cast %12 : index to i64 loc(#loc31)
            %15 = arith.muli %13, %14 : i64 loc(#loc32)
            %16 = arith.index_cast %15 : i64 to index loc(#loc29)
            %17 = arith.divui %16, %12 : index loc(#loc29)
            %alloc = memref.alloc(%17) : memref<?xmemref<?xf64>> loc(#loc29)
            affine.store %alloc, %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc9)
            scf.yield %alloc : memref<?xmemref<?xf64>> loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc33)
            %12 = polygeist.typeSize f64 : index loc(#loc34)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc35)
            %14 = arith.index_cast %12 : index to i64 loc(#loc36)
            %15 = arith.muli %13, %14 : i64 loc(#loc37)
            %16 = arith.index_cast %15 : i64 to index loc(#loc34)
            %17 = arith.divui %16, %12 : index loc(#loc34)
            %18 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc38)
            %19 = polygeist.typeSize f64 : index loc(#loc39)
            %20 = arith.extsi %6 : i32 to i64 loc(#loc40)
            %21 = arith.index_cast %19 : index to i64 loc(#loc41)
            %22 = arith.muli %20, %21 : i64 loc(#loc42)
            %23 = arith.index_cast %22 : i64 to index loc(#loc39)
            %24 = arith.divui %23, %19 : index loc(#loc39)
            %25 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc43)
            %26 = polygeist.typeSize f64 : index loc(#loc44)
            %27 = arith.extsi %6 : i32 to i64 loc(#loc45)
            %28 = arith.index_cast %26 : index to i64 loc(#loc46)
            %29 = arith.muli %27, %28 : i64 loc(#loc47)
            %30 = arith.index_cast %29 : i64 to index loc(#loc44)
            %31 = arith.divui %30, %26 : index loc(#loc44)
            affine.for %arg2 loc("benchmarks/polybench/2mm/2mm.c":119:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %32 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc33)
                  %alloc = memref.alloc(%17) : memref<?xf64> loc(#loc34)
                  affine.store %alloc, %32[0] : memref<?xmemref<?xf64>> loc(#loc49)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %32 = polygeist.subindex %18[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc38)
                  %alloc = memref.alloc(%24) : memref<?xf64> loc(#loc39)
                  affine.store %alloc, %32[0] : memref<?xmemref<?xf64>> loc(#loc50)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
              scf.if %true {
                scf.execute_region {
                  %32 = polygeist.subindex %25[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc43)
                  %alloc = memref.alloc(%31) : memref<?xf64> loc(#loc44)
                  affine.store %alloc, %32[0] : memref<?xmemref<?xf64>> loc(#loc51)
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
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc52)
            %12 = polygeist.typeSize f64 : index loc(#loc53)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc54)
            %14 = arith.index_cast %12 : index to i64 loc(#loc55)
            %15 = arith.muli %13, %14 : i64 loc(#loc56)
            %16 = arith.index_cast %15 : i64 to index loc(#loc53)
            %17 = arith.divui %16, %12 : index loc(#loc53)
            affine.for %arg2 loc("benchmarks/polybench/2mm/2mm.c":124:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %18 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc52)
                  %alloc = memref.alloc(%17) : memref<?xf64> loc(#loc53)
                  affine.store %alloc, %18[0] : memref<?xmemref<?xf64>> loc(#loc58)
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
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %11 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc59)
            %12 = polygeist.typeSize f64 : index loc(#loc60)
            %13 = arith.extsi %6 : i32 to i64 loc(#loc61)
            %14 = arith.index_cast %12 : index to i64 loc(#loc62)
            %15 = arith.muli %13, %14 : i64 loc(#loc63)
            %16 = arith.index_cast %15 : i64 to index loc(#loc60)
            %17 = arith.divui %16, %12 : index loc(#loc60)
            affine.for %arg2 loc("benchmarks/polybench/2mm/2mm.c":127:3) = 0 to %7 {
              scf.if %true {
                scf.execute_region {
                  %18 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc59)
                  %alloc = memref.alloc(%17) : memref<?xf64> loc(#loc60)
                  affine.store %alloc, %18[0] : memref<?xmemref<?xf64>> loc(#loc65)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc64)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %cast = memref.cast %alloca_6 : memref<1xf64> to memref<?xf64> loc(#loc66)
        %cast_7 = memref.cast %alloca_5 : memref<1xf64> to memref<?xf64> loc(#loc67)
        %11 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc68)
        %12 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc69)
        %13 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc70)
        %14 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc71)
        func.call @init_array(%6, %6, %6, %6, %cast, %cast_7, %11, %12, %13, %14) : (i32, i32, i32, i32, memref<?xf64>, memref<?xf64>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) -> () loc(#loc72)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %2 : !llvm.ptr to memref<?xi8> loc(#loc73)
        func.call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc73)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc74)
        func.call @carts_kernel_timer_start(%11) : (memref<?xi8>) -> () loc(#loc74)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_6[0] : memref<1xf64> loc(#loc75)
        %12 = affine.load %alloca_5[0] : memref<1xf64> loc(#loc76)
        %13 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc77)
        %14 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc78)
        %15 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc79)
        %16 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc80)
        %17 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc81)
        func.call @kernel_2mm(%6, %6, %6, %6, %11, %12, %13, %14, %15, %16, %17) : (i32, i32, i32, i32, f64, f64, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) -> () loc(#loc82)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc83)
        func.call @carts_kernel_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc83)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc84)
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc84)
        func.call @carts_phase_timer_start(%11, %12) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc84)
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
              scf.yield %6 : i32 loc(#loc85)
            } else {
              scf.yield %6 : i32 loc(#loc85)
            } loc(#loc85)
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
            %11 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc86)
            affine.for %arg2 loc("benchmarks/polybench/2mm/2mm.c":144:3) = 0 to %9 {
              scf.if %true {
                scf.execute_region {
                  %12 = polygeist.subindex %11[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc86)
                  %13 = affine.load %12[0] : memref<?xmemref<?xf64>> loc(#loc86)
                  %14 = polygeist.subindex %13[%arg2] () : memref<?xf64> -> memref<?xf64> loc(#loc86)
                  %15 = affine.load %14[0] : memref<?xf64> loc(#loc86)
                  %16 = affine.load %alloca[0] : memref<1xf64> loc(#loc88)
                  %17 = arith.addf %16, %15 : f64 loc(#loc88)
                  affine.store %17, %alloca[0] : memref<1xf64> loc(#loc88)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc87)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca[0] : memref<1xf64> loc(#loc89)
        func.call @carts_bench_checksum_d(%11) : (f64) -> () loc(#loc90)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %1 : !llvm.ptr to memref<?xi8> loc(#loc91)
        func.call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc91)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc92)
        %12 = polygeist.pointer2memref %3 : !llvm.ptr to memref<?xi8> loc(#loc92)
        func.call @carts_phase_timer_start(%11, %12) : (memref<?xi8>, memref<?xi8>) -> () loc(#loc92)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_4[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc93)
        func.call @free_matrix(%11, %6) : (memref<?xmemref<?xf64>>, i32) -> () loc(#loc94)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_3[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc95)
        func.call @free_matrix(%11, %6) : (memref<?xmemref<?xf64>>, i32) -> () loc(#loc96)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_2[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc97)
        func.call @free_matrix(%11, %6) : (memref<?xmemref<?xf64>>, i32) -> () loc(#loc98)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_1[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc99)
        func.call @free_matrix(%11, %6) : (memref<?xmemref<?xf64>>, i32) -> () loc(#loc100)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = affine.load %alloca_0[0] : memref<1xmemref<?xmemref<?xf64>>> loc(#loc101)
        func.call @free_matrix(%11, %6) : (memref<?xmemref<?xf64>>, i32) -> () loc(#loc102)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        %11 = polygeist.pointer2memref %0 : !llvm.ptr to memref<?xi8> loc(#loc103)
        func.call @carts_phase_timer_stop(%11) : (memref<?xi8>) -> () loc(#loc103)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        func.call @carts_e2e_timer_stop() : () -> () loc(#loc104)
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
    return %10 : i32 loc(#loc105)
  } loc(#loc5)
  func.func private @carts_benchmarks_start() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc106)
  func.func private @carts_e2e_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc107)
  func.func private @carts_phase_timer_start(memref<?xi8>, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc108)
  func.func private @init_array(%arg0: i32 loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg1: i32 loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg2: i32 loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg3: i32 loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg4: memref<?xf64> loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg5: memref<?xf64> loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg6: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg7: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg8: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":35:13), %arg9: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":35:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc110)
    %c1_i32 = arith.constant 1 : i32 loc(#loc)
    %c3_i32 = arith.constant 3 : i32 loc(#loc)
    %c2_i32 = arith.constant 2 : i32 loc(#loc)
    %cst = arith.constant 3.241200e+04 : f64 loc(#loc)
    %cst_0 = arith.constant 2.123000e+03 : f64 loc(#loc)
    %0 = arith.index_cast %arg2 : i32 to index loc(#loc109)
    %1 = arith.index_cast %arg1 : i32 to index loc(#loc109)
    %2 = arith.index_cast %arg0 : i32 to index loc(#loc109)
    %3 = arith.index_cast %arg3 : i32 to index loc(#loc109)
    scf.if %true {
      scf.execute_region {
        affine.store %cst, %arg4[0] : memref<?xf64> loc(#loc111)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        affine.store %cst_0, %arg5[0] : memref<?xf64> loc(#loc112)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            %4 = arith.sitofp %arg0 : i32 to f64 loc(#loc113)
            affine.for %arg10 loc("benchmarks/polybench/2mm/2mm.c":42:3) = 0 to %2 {
              %5 = arith.index_cast %arg10 : index to i32 loc(#loc114)
              scf.if %true {
                scf.execute_region {
                  %6 = polygeist.subindex %arg6[%arg10] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc115)
                  %7 = arith.sitofp %5 : i32 to f64 loc(#loc116)
                  affine.for %arg11 loc("benchmarks/polybench/2mm/2mm.c":43:5) = 0 to %0 {
                    %8 = arith.index_cast %arg11 : index to i32 loc(#loc117)
                    %9 = affine.load %6[0] : memref<?xmemref<?xf64>> loc(#loc115)
                    %10 = polygeist.subindex %9[%arg11] () : memref<?xf64> -> memref<?xf64> loc(#loc115)
                    %11 = arith.sitofp %8 : i32 to f64 loc(#loc118)
                    %12 = arith.mulf %7, %11 : f64 loc(#loc119)
                    %13 = arith.divf %12, %4 : f64 loc(#loc120)
                    affine.store %13, %10[0] : memref<?xf64> loc(#loc121)
                  } loc(#loc117)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc114)
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
            %4 = arith.sitofp %arg1 : i32 to f64 loc(#loc122)
            affine.for %arg10 loc("benchmarks/polybench/2mm/2mm.c":45:3) = 0 to %0 {
              %5 = arith.index_cast %arg10 : index to i32 loc(#loc123)
              scf.if %true {
                scf.execute_region {
                  %6 = polygeist.subindex %arg7[%arg10] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc124)
                  %7 = arith.sitofp %5 : i32 to f64 loc(#loc125)
                  affine.for %arg11 loc("benchmarks/polybench/2mm/2mm.c":46:5) = 0 to %1 {
                    %8 = arith.index_cast %arg11 : index to i32 loc(#loc126)
                    %9 = affine.load %6[0] : memref<?xmemref<?xf64>> loc(#loc124)
                    %10 = polygeist.subindex %9[%arg11] () : memref<?xf64> -> memref<?xf64> loc(#loc124)
                    %11 = arith.addi %8, %c1_i32 : i32 loc(#loc127)
                    %12 = arith.sitofp %11 : i32 to f64 loc(#loc128)
                    %13 = arith.mulf %7, %12 : f64 loc(#loc129)
                    %14 = arith.divf %13, %4 : f64 loc(#loc130)
                    affine.store %14, %10[0] : memref<?xf64> loc(#loc131)
                  } loc(#loc126)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc123)
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
            %4 = arith.sitofp %arg3 : i32 to f64 loc(#loc132)
            affine.for %arg10 loc("benchmarks/polybench/2mm/2mm.c":48:3) = 0 to %3 {
              %5 = arith.index_cast %arg10 : index to i32 loc(#loc133)
              scf.if %true {
                scf.execute_region {
                  %6 = polygeist.subindex %arg8[%arg10] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc134)
                  %7 = arith.sitofp %5 : i32 to f64 loc(#loc135)
                  affine.for %arg11 loc("benchmarks/polybench/2mm/2mm.c":49:5) = 0 to %1 {
                    %8 = arith.index_cast %arg11 : index to i32 loc(#loc136)
                    %9 = affine.load %6[0] : memref<?xmemref<?xf64>> loc(#loc134)
                    %10 = polygeist.subindex %9[%arg11] () : memref<?xf64> -> memref<?xf64> loc(#loc134)
                    %11 = arith.addi %8, %c3_i32 : i32 loc(#loc137)
                    %12 = arith.sitofp %11 : i32 to f64 loc(#loc138)
                    %13 = arith.mulf %7, %12 : f64 loc(#loc139)
                    %14 = arith.divf %13, %4 : f64 loc(#loc140)
                    affine.store %14, %10[0] : memref<?xf64> loc(#loc141)
                  } loc(#loc136)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc133)
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
            %4 = arith.sitofp %arg2 : i32 to f64 loc(#loc142)
            affine.for %arg10 loc("benchmarks/polybench/2mm/2mm.c":51:3) = 0 to %2 {
              %5 = arith.index_cast %arg10 : index to i32 loc(#loc143)
              scf.if %true {
                scf.execute_region {
                  %6 = polygeist.subindex %arg9[%arg10] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc144)
                  %7 = arith.sitofp %5 : i32 to f64 loc(#loc145)
                  affine.for %arg11 loc("benchmarks/polybench/2mm/2mm.c":52:5) = 0 to %3 {
                    %8 = arith.index_cast %arg11 : index to i32 loc(#loc146)
                    %9 = affine.load %6[0] : memref<?xmemref<?xf64>> loc(#loc144)
                    %10 = polygeist.subindex %9[%arg11] () : memref<?xf64> -> memref<?xf64> loc(#loc144)
                    %11 = arith.addi %8, %c2_i32 : i32 loc(#loc147)
                    %12 = arith.sitofp %11 : i32 to f64 loc(#loc148)
                    %13 = arith.mulf %7, %12 : f64 loc(#loc149)
                    %14 = arith.divf %13, %4 : f64 loc(#loc150)
                    affine.store %14, %10[0] : memref<?xf64> loc(#loc151)
                  } loc(#loc146)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc143)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc152)
  } loc(#loc109)
  func.func private @carts_phase_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc153)
  func.func private @carts_kernel_timer_start(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc154)
  func.func private @kernel_2mm(%arg0: i32 loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg1: i32 loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg2: i32 loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg3: i32 loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg4: f64 loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg5: f64 loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg6: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg7: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg8: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg9: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":72:13), %arg10: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":72:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %cst = arith.constant 0.000000e+00 : f64 loc(#loc)
    %c1 = arith.constant 1 : index loc(#loc)
    %c0 = arith.constant 0 : index loc(#loc)
    %true = arith.constant true loc(#loc156)
    %0 = arith.index_cast %arg2 : i32 to index loc(#loc155)
    %1 = arith.index_cast %arg3 : i32 to index loc(#loc155)
    %2 = arith.index_cast %arg1 : i32 to index loc(#loc155)
    %3 = "polygeist.undef"() : () -> i32 loc(#loc157)
    %alloca = memref.alloca() : memref<i1> loc(#loc156)
    affine.store %true, %alloca[] : memref<i1> loc(#loc156)
    scf.if %true {
      scf.execute_region {
        scf.if %true {
          scf.execute_region {
            omp.parallel {
              scf.execute_region {
                %4 = affine.load %alloca[] : memref<i1> loc(#loc)
                %5 = scf.if %4 -> (i1) {
                  %6 = scf.execute_region -> i1 {
                    %7 = scf.if %4 -> (i1) {
                      scf.execute_region {
                        %8 = scf.if %4 -> (i32) {
                          scf.yield %arg0 : i32 loc(#loc)
                        } else {
                          scf.yield %3 : i32 loc(#loc)
                        } loc(#loc)
                        %9 = arith.index_cast %8 : i32 to index loc(#loc159)
                        omp.wsloop {
                          omp.loop_nest (%arg11) : index = (%c0) to (%9) step (%c1) {
                            scf.execute_region {
                              affine.store %true, %alloca[] : memref<i1> loc(#loc159)
                              scf.if %true {
                                scf.execute_region {
                                  %10 = polygeist.subindex %arg6[%arg11] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc160)
                                  %11 = polygeist.subindex %arg6[%arg11] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc161)
                                  %12 = polygeist.subindex %arg7[%arg11] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc162)
                                  affine.for %arg12 loc("benchmarks/polybench/2mm/2mm.c":82:7) = 0 to %2 {
                                    scf.execute_region {
                                      scf.if %true {
                                        scf.execute_region {
                                          %13 = affine.load %10[0] : memref<?xmemref<?xf64>> loc(#loc160)
                                          %14 = polygeist.subindex %13[%arg12] () : memref<?xf64> -> memref<?xf64> loc(#loc160)
                                          affine.store %cst, %14[0] : memref<?xf64> loc(#loc164)
                                          scf.yield loc(#loc)
                                        } loc(#loc)
                                      } loc(#loc)
                                      scf.if %true {
                                        scf.execute_region {
                                          scf.if %true {
                                            scf.execute_region {
                                              affine.for %arg13 loc("benchmarks/polybench/2mm/2mm.c":84:9) = 0 to %0 {
                                                scf.execute_region {
                                                  %13 = affine.load %11[0] : memref<?xmemref<?xf64>> loc(#loc161)
                                                  %14 = polygeist.subindex %13[%arg12] () : memref<?xf64> -> memref<?xf64> loc(#loc161)
                                                  %15 = affine.load %12[0] : memref<?xmemref<?xf64>> loc(#loc162)
                                                  %16 = polygeist.subindex %15[%arg13] () : memref<?xf64> -> memref<?xf64> loc(#loc162)
                                                  %17 = affine.load %16[0] : memref<?xf64> loc(#loc162)
                                                  %18 = arith.mulf %arg4, %17 : f64 loc(#loc166)
                                                  %19 = polygeist.subindex %arg8[%arg13] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc167)
                                                  %20 = affine.load %19[0] : memref<?xmemref<?xf64>> loc(#loc167)
                                                  %21 = polygeist.subindex %20[%arg12] () : memref<?xf64> -> memref<?xf64> loc(#loc167)
                                                  %22 = affine.load %21[0] : memref<?xf64> loc(#loc167)
                                                  %23 = arith.mulf %18, %22 : f64 loc(#loc168)
                                                  %24 = affine.load %14[0] : memref<?xf64> loc(#loc169)
                                                  %25 = arith.addf %24, %23 : f64 loc(#loc169)
                                                  affine.store %25, %14[0] : memref<?xf64> loc(#loc169)
                                                  scf.yield loc(#loc165)
                                                } loc(#loc165)
                                              } loc(#loc165)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                          scf.yield loc(#loc)
                                        } loc(#loc)
                                      } loc(#loc)
                                      scf.yield loc(#loc163)
                                    } loc(#loc163)
                                  } loc(#loc163)
                                  scf.yield loc(#loc)
                                } loc(#loc)
                              } loc(#loc)
                              scf.yield loc(#loc159)
                            } loc(#loc159)
                            omp.yield loc(#loc159)
                          } loc(#loc159)
                        } loc(#loc159)
                        affine.store %true, %alloca[] : memref<i1> loc(#loc159)
                        scf.yield loc(#loc)
                      } loc(#loc)
                      scf.yield %true : i1 loc(#loc)
                    } else {
                      scf.yield %4 : i1 loc(#loc)
                    } loc(#loc)
                    scf.yield %7 : i1 loc(#loc)
                  } loc(#loc)
                  scf.yield %6 : i1 loc(#loc)
                } else {
                  scf.yield %4 : i1 loc(#loc)
                } loc(#loc)
                scf.if %5 {
                  scf.execute_region {
                    scf.if %5 {
                      scf.execute_region {
                        %6 = scf.if %5 -> (i32) {
                          scf.yield %arg0 : i32 loc(#loc)
                        } else {
                          scf.yield %3 : i32 loc(#loc)
                        } loc(#loc)
                        %7 = arith.index_cast %6 : i32 to index loc(#loc170)
                        omp.wsloop {
                          omp.loop_nest (%arg11) : index = (%c0) to (%7) step (%c1) {
                            scf.execute_region {
                              affine.store %true, %alloca[] : memref<i1> loc(#loc170)
                              scf.if %true {
                                scf.execute_region {
                                  %8 = polygeist.subindex %arg10[%arg11] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc171)
                                  %9 = polygeist.subindex %arg10[%arg11] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc172)
                                  %10 = polygeist.subindex %arg6[%arg11] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc173)
                                  affine.for %arg12 loc("benchmarks/polybench/2mm/2mm.c":89:7) = 0 to %1 {
                                    scf.execute_region {
                                      scf.if %true {
                                        scf.execute_region {
                                          %11 = affine.load %8[0] : memref<?xmemref<?xf64>> loc(#loc171)
                                          %12 = polygeist.subindex %11[%arg12] () : memref<?xf64> -> memref<?xf64> loc(#loc171)
                                          %13 = affine.load %12[0] : memref<?xf64> loc(#loc175)
                                          %14 = arith.mulf %13, %arg5 : f64 loc(#loc175)
                                          affine.store %14, %12[0] : memref<?xf64> loc(#loc175)
                                          scf.yield loc(#loc)
                                        } loc(#loc)
                                      } loc(#loc)
                                      scf.if %true {
                                        scf.execute_region {
                                          scf.if %true {
                                            scf.execute_region {
                                              affine.for %arg13 loc("benchmarks/polybench/2mm/2mm.c":91:9) = 0 to %2 {
                                                scf.execute_region {
                                                  %11 = affine.load %9[0] : memref<?xmemref<?xf64>> loc(#loc172)
                                                  %12 = polygeist.subindex %11[%arg12] () : memref<?xf64> -> memref<?xf64> loc(#loc172)
                                                  %13 = affine.load %10[0] : memref<?xmemref<?xf64>> loc(#loc173)
                                                  %14 = polygeist.subindex %13[%arg13] () : memref<?xf64> -> memref<?xf64> loc(#loc173)
                                                  %15 = affine.load %14[0] : memref<?xf64> loc(#loc173)
                                                  %16 = polygeist.subindex %arg9[%arg13] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc177)
                                                  %17 = affine.load %16[0] : memref<?xmemref<?xf64>> loc(#loc177)
                                                  %18 = polygeist.subindex %17[%arg12] () : memref<?xf64> -> memref<?xf64> loc(#loc177)
                                                  %19 = affine.load %18[0] : memref<?xf64> loc(#loc177)
                                                  %20 = arith.mulf %15, %19 : f64 loc(#loc178)
                                                  %21 = affine.load %12[0] : memref<?xf64> loc(#loc179)
                                                  %22 = arith.addf %21, %20 : f64 loc(#loc179)
                                                  affine.store %22, %12[0] : memref<?xf64> loc(#loc179)
                                                  scf.yield loc(#loc176)
                                                } loc(#loc176)
                                              } loc(#loc176)
                                              scf.yield loc(#loc)
                                            } loc(#loc)
                                          } loc(#loc)
                                          scf.yield loc(#loc)
                                        } loc(#loc)
                                      } loc(#loc)
                                      scf.yield loc(#loc174)
                                    } loc(#loc174)
                                  } loc(#loc174)
                                  scf.yield loc(#loc)
                                } loc(#loc)
                              } loc(#loc)
                              scf.yield loc(#loc170)
                            } loc(#loc170)
                            omp.yield loc(#loc170)
                          } loc(#loc170)
                        } loc(#loc170)
                        affine.store %true, %alloca[] : memref<i1> loc(#loc170)
                        scf.yield loc(#loc)
                      } loc(#loc)
                    } loc(#loc)
                    scf.yield loc(#loc)
                  } loc(#loc)
                } loc(#loc)
                scf.yield loc(#loc158)
              } loc(#loc158)
              omp.terminator loc(#loc158)
            } loc(#loc158)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc180)
  } loc(#loc155)
  func.func private @carts_kernel_timer_stop(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc181)
  func.func private @carts_bench_checksum_d(f64) attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc182)
  func.func private @free_matrix(%arg0: memref<?xmemref<?xf64>> loc("benchmarks/polybench/2mm/2mm.c":24:13), %arg1: i32 loc("benchmarks/polybench/2mm/2mm.c":24:13)) attributes {llvm.linkage = #llvm.linkage<internal>} {
    %true = arith.constant true loc(#loc184)
    %0 = llvm.mlir.zero : !llvm.ptr loc(#loc)
    %false = arith.constant false loc(#loc)
    %1 = arith.index_cast %arg1 : i32 to index loc(#loc183)
    %2 = scf.if %true -> (i1) {
      %3 = scf.execute_region -> i1 {
        %4 = scf.if %true -> (i1) {
          %5 = scf.execute_region -> i1 {
            %6 = polygeist.memref2pointer %arg0 : memref<?xmemref<?xf64>> to !llvm.ptr loc(#loc185)
            %7 = llvm.icmp "eq" %6, %0 : !llvm.ptr loc(#loc185)
            %8 = scf.if %7 -> (i1) {
              %9 = scf.if %true -> (i1) {
                %10 = scf.execute_region -> i1 {
                  %11 = scf.if %true -> (i1) {
                    %12 = scf.execute_region -> i1 {
                      scf.yield %false : i1 loc(#loc)
                    } loc(#loc)
                    scf.yield %12 : i1 loc(#loc)
                  } else {
                    scf.yield %true : i1 loc(#loc)
                  } loc(#loc)
                  scf.yield %11 : i1 loc(#loc)
                } loc(#loc)
                scf.yield %10 : i1 loc(#loc)
              } else {
                scf.yield %true : i1 loc(#loc)
              } loc(#loc)
              scf.yield %9 : i1 loc(#loc186)
            } else {
              scf.yield %true : i1 loc(#loc186)
            } loc(#loc186)
            scf.yield %8 : i1 loc(#loc)
          } loc(#loc)
          scf.yield %5 : i1 loc(#loc)
        } else {
          scf.yield %true : i1 loc(#loc)
        } loc(#loc)
        scf.yield %4 : i1 loc(#loc)
      } loc(#loc)
      scf.yield %3 : i1 loc(#loc)
    } else {
      scf.yield %true : i1 loc(#loc)
    } loc(#loc)
    scf.if %2 {
      scf.execute_region {
        scf.if %2 {
          scf.execute_region {
            affine.for %arg2 loc("benchmarks/polybench/2mm/2mm.c":28:3) = 0 to %1 {
              scf.if %2 {
                scf.execute_region {
                  %3 = polygeist.subindex %arg0[%arg2] () : memref<?xmemref<?xf64>> -> memref<?xmemref<?xf64>> loc(#loc188)
                  %4 = affine.load %3[0] : memref<?xmemref<?xf64>> loc(#loc189)
                  memref.dealloc %4 : memref<?xf64> loc(#loc189)
                  scf.yield loc(#loc)
                } loc(#loc)
              } loc(#loc)
            } loc(#loc187)
            scf.yield loc(#loc)
          } loc(#loc)
        } loc(#loc)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    scf.if %2 {
      scf.execute_region {
        memref.dealloc %arg0 : memref<?xmemref<?xf64>> loc(#loc190)
        scf.yield loc(#loc)
      } loc(#loc)
    } loc(#loc)
    return loc(#loc191)
  } loc(#loc183)
  func.func private @carts_e2e_timer_stop() attributes {llvm.linkage = #llvm.linkage<external>} loc(#loc192)
} loc(#loc)
#loc = loc(unknown)
#loc1 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:65)
#loc2 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:27)
#loc3 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:65)
#loc4 = loc("benchmarks/polybench/2mm/2mm.c":101:25)
#loc6 = loc("benchmarks/polybench/2mm/2mm.c":98:1)
#loc7 = loc("benchmarks/polybench/2mm/2mm.c":144:8)
#loc8 = loc("benchmarks/polybench/2mm/2mm.c":142:3)
#loc9 = loc("benchmarks/polybench/2mm/2mm.h":61:21)
#loc10 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":54:34)
#loc11 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":79:37)
#loc12 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":95:41)
#loc13 = loc("benchmarks/polybench/2mm/2mm.c":113:21)
#loc14 = loc("benchmarks/polybench/2mm/2mm.c":113:42)
#loc15 = loc("benchmarks/polybench/2mm/2mm.c":113:47)
#loc16 = loc("benchmarks/polybench/2mm/2mm.c":113:45)
#loc17 = loc("benchmarks/polybench/2mm/2mm.c":114:19)
#loc18 = loc("benchmarks/polybench/2mm/2mm.c":114:40)
#loc19 = loc("benchmarks/polybench/2mm/2mm.c":114:45)
#loc20 = loc("benchmarks/polybench/2mm/2mm.c":114:43)
#loc21 = loc("benchmarks/polybench/2mm/2mm.c":115:19)
#loc22 = loc("benchmarks/polybench/2mm/2mm.c":115:40)
#loc23 = loc("benchmarks/polybench/2mm/2mm.c":115:45)
#loc24 = loc("benchmarks/polybench/2mm/2mm.c":115:43)
#loc25 = loc("benchmarks/polybench/2mm/2mm.c":116:19)
#loc26 = loc("benchmarks/polybench/2mm/2mm.c":116:40)
#loc27 = loc("benchmarks/polybench/2mm/2mm.c":116:45)
#loc28 = loc("benchmarks/polybench/2mm/2mm.c":116:43)
#loc29 = loc("benchmarks/polybench/2mm/2mm.c":117:19)
#loc30 = loc("benchmarks/polybench/2mm/2mm.c":117:40)
#loc31 = loc("benchmarks/polybench/2mm/2mm.c":117:45)
#loc32 = loc("benchmarks/polybench/2mm/2mm.c":117:43)
#loc33 = loc("benchmarks/polybench/2mm/2mm.c":120:5)
#loc34 = loc("benchmarks/polybench/2mm/2mm.c":120:14)
#loc35 = loc("benchmarks/polybench/2mm/2mm.c":120:34)
#loc36 = loc("benchmarks/polybench/2mm/2mm.c":120:39)
#loc37 = loc("benchmarks/polybench/2mm/2mm.c":120:37)
#loc38 = loc("benchmarks/polybench/2mm/2mm.c":121:5)
#loc39 = loc("benchmarks/polybench/2mm/2mm.c":121:12)
#loc40 = loc("benchmarks/polybench/2mm/2mm.c":121:32)
#loc41 = loc("benchmarks/polybench/2mm/2mm.c":121:37)
#loc42 = loc("benchmarks/polybench/2mm/2mm.c":121:35)
#loc43 = loc("benchmarks/polybench/2mm/2mm.c":122:5)
#loc44 = loc("benchmarks/polybench/2mm/2mm.c":122:12)
#loc45 = loc("benchmarks/polybench/2mm/2mm.c":122:32)
#loc46 = loc("benchmarks/polybench/2mm/2mm.c":122:37)
#loc47 = loc("benchmarks/polybench/2mm/2mm.c":122:35)
#loc49 = loc("benchmarks/polybench/2mm/2mm.c":120:12)
#loc50 = loc("benchmarks/polybench/2mm/2mm.c":121:10)
#loc51 = loc("benchmarks/polybench/2mm/2mm.c":122:10)
#loc52 = loc("benchmarks/polybench/2mm/2mm.c":125:5)
#loc53 = loc("benchmarks/polybench/2mm/2mm.c":125:12)
#loc54 = loc("benchmarks/polybench/2mm/2mm.c":125:32)
#loc55 = loc("benchmarks/polybench/2mm/2mm.c":125:37)
#loc56 = loc("benchmarks/polybench/2mm/2mm.c":125:35)
#loc58 = loc("benchmarks/polybench/2mm/2mm.c":125:10)
#loc59 = loc("benchmarks/polybench/2mm/2mm.c":128:5)
#loc60 = loc("benchmarks/polybench/2mm/2mm.c":128:12)
#loc61 = loc("benchmarks/polybench/2mm/2mm.c":128:32)
#loc62 = loc("benchmarks/polybench/2mm/2mm.c":128:37)
#loc63 = loc("benchmarks/polybench/2mm/2mm.c":128:35)
#loc65 = loc("benchmarks/polybench/2mm/2mm.c":128:10)
#loc66 = loc("benchmarks/polybench/2mm/2mm.c":132:30)
#loc67 = loc("benchmarks/polybench/2mm/2mm.c":132:38)
#loc68 = loc("benchmarks/polybench/2mm/2mm.c":132:45)
#loc69 = loc("benchmarks/polybench/2mm/2mm.c":132:48)
#loc70 = loc("benchmarks/polybench/2mm/2mm.c":132:51)
#loc71 = loc("benchmarks/polybench/2mm/2mm.c":132:54)
#loc72 = loc("benchmarks/polybench/2mm/2mm.c":132:3)
#loc73 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":96:36)
#loc74 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":139:40)
#loc75 = loc("benchmarks/polybench/2mm/2mm.c":137:30)
#loc76 = loc("benchmarks/polybench/2mm/2mm.c":137:37)
#loc77 = loc("benchmarks/polybench/2mm/2mm.c":137:43)
#loc78 = loc("benchmarks/polybench/2mm/2mm.c":137:48)
#loc79 = loc("benchmarks/polybench/2mm/2mm.c":137:51)
#loc80 = loc("benchmarks/polybench/2mm/2mm.c":137:54)
#loc81 = loc("benchmarks/polybench/2mm/2mm.c":137:57)
#loc82 = loc("benchmarks/polybench/2mm/2mm.c":137:3)
#loc83 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":140:39)
#loc84 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":98:3)
#loc85 = loc("benchmarks/polybench/2mm/2mm.c":143:14)
#loc86 = loc("benchmarks/polybench/2mm/2mm.c":145:17)
#loc88 = loc("benchmarks/polybench/2mm/2mm.c":145:14)
#loc89 = loc("benchmarks/polybench/2mm/2mm.c":147:24)
#loc90 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":251:3)
#loc91 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":99:41)
#loc92 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":100:41)
#loc93 = loc("benchmarks/polybench/2mm/2mm.c":152:15)
#loc94 = loc("benchmarks/polybench/2mm/2mm.c":152:3)
#loc95 = loc("benchmarks/polybench/2mm/2mm.c":153:15)
#loc96 = loc("benchmarks/polybench/2mm/2mm.c":153:3)
#loc97 = loc("benchmarks/polybench/2mm/2mm.c":154:15)
#loc98 = loc("benchmarks/polybench/2mm/2mm.c":154:3)
#loc99 = loc("benchmarks/polybench/2mm/2mm.c":155:15)
#loc100 = loc("benchmarks/polybench/2mm/2mm.c":155:3)
#loc101 = loc("benchmarks/polybench/2mm/2mm.c":156:15)
#loc102 = loc("benchmarks/polybench/2mm/2mm.c":156:3)
#loc103 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":101:36)
#loc104 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":80:32)
#loc105 = loc("benchmarks/polybench/2mm/2mm.c":162:1)
#loc106 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":50:6)
#loc107 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":74:6)
#loc108 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":90:6)
#loc110 = loc("benchmarks/polybench/2mm/2mm.c":35:1)
#loc111 = loc("benchmarks/polybench/2mm/2mm.c":40:10)
#loc112 = loc("benchmarks/polybench/2mm/2mm.c":41:9)
#loc113 = loc("benchmarks/polybench/2mm/2mm.c":44:38)
#loc115 = loc("benchmarks/polybench/2mm/2mm.c":44:7)
#loc116 = loc("benchmarks/polybench/2mm/2mm.c":44:18)
#loc118 = loc("benchmarks/polybench/2mm/2mm.c":44:33)
#loc119 = loc("benchmarks/polybench/2mm/2mm.c":44:31)
#loc120 = loc("benchmarks/polybench/2mm/2mm.c":44:36)
#loc121 = loc("benchmarks/polybench/2mm/2mm.c":44:15)
#loc122 = loc("benchmarks/polybench/2mm/2mm.c":47:44)
#loc124 = loc("benchmarks/polybench/2mm/2mm.c":47:7)
#loc125 = loc("benchmarks/polybench/2mm/2mm.c":47:18)
#loc127 = loc("benchmarks/polybench/2mm/2mm.c":47:36)
#loc128 = loc("benchmarks/polybench/2mm/2mm.c":47:33)
#loc129 = loc("benchmarks/polybench/2mm/2mm.c":47:31)
#loc130 = loc("benchmarks/polybench/2mm/2mm.c":47:42)
#loc131 = loc("benchmarks/polybench/2mm/2mm.c":47:15)
#loc132 = loc("benchmarks/polybench/2mm/2mm.c":50:44)
#loc134 = loc("benchmarks/polybench/2mm/2mm.c":50:7)
#loc135 = loc("benchmarks/polybench/2mm/2mm.c":50:18)
#loc137 = loc("benchmarks/polybench/2mm/2mm.c":50:36)
#loc138 = loc("benchmarks/polybench/2mm/2mm.c":50:33)
#loc139 = loc("benchmarks/polybench/2mm/2mm.c":50:31)
#loc140 = loc("benchmarks/polybench/2mm/2mm.c":50:42)
#loc141 = loc("benchmarks/polybench/2mm/2mm.c":50:15)
#loc142 = loc("benchmarks/polybench/2mm/2mm.c":53:44)
#loc144 = loc("benchmarks/polybench/2mm/2mm.c":53:7)
#loc145 = loc("benchmarks/polybench/2mm/2mm.c":53:18)
#loc147 = loc("benchmarks/polybench/2mm/2mm.c":53:36)
#loc148 = loc("benchmarks/polybench/2mm/2mm.c":53:33)
#loc149 = loc("benchmarks/polybench/2mm/2mm.c":53:31)
#loc150 = loc("benchmarks/polybench/2mm/2mm.c":53:42)
#loc151 = loc("benchmarks/polybench/2mm/2mm.c":53:15)
#loc152 = loc("benchmarks/polybench/2mm/2mm.c":54:1)
#loc153 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":93:6)
#loc154 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":128:6)
#loc156 = loc("benchmarks/polybench/2mm/2mm.c":72:1)
#loc157 = loc("benchmarks/polybench/2mm/2mm.c":75:3)
#loc158 = loc("benchmarks/polybench/2mm/2mm.c":78:1)
#loc159 = loc("benchmarks/polybench/2mm/2mm.c":80:1)
#loc160 = loc("benchmarks/polybench/2mm/2mm.c":83:9)
#loc161 = loc("benchmarks/polybench/2mm/2mm.c":85:11)
#loc162 = loc("benchmarks/polybench/2mm/2mm.c":85:32)
#loc164 = loc("benchmarks/polybench/2mm/2mm.c":83:19)
#loc166 = loc("benchmarks/polybench/2mm/2mm.c":85:30)
#loc167 = loc("benchmarks/polybench/2mm/2mm.c":85:42)
#loc168 = loc("benchmarks/polybench/2mm/2mm.c":85:40)
#loc169 = loc("benchmarks/polybench/2mm/2mm.c":85:21)
#loc170 = loc("benchmarks/polybench/2mm/2mm.c":87:1)
#loc171 = loc("benchmarks/polybench/2mm/2mm.c":90:9)
#loc172 = loc("benchmarks/polybench/2mm/2mm.c":92:11)
#loc173 = loc("benchmarks/polybench/2mm/2mm.c":92:22)
#loc175 = loc("benchmarks/polybench/2mm/2mm.c":90:17)
#loc177 = loc("benchmarks/polybench/2mm/2mm.c":92:34)
#loc178 = loc("benchmarks/polybench/2mm/2mm.c":92:32)
#loc179 = loc("benchmarks/polybench/2mm/2mm.c":92:19)
#loc180 = loc("benchmarks/polybench/2mm/2mm.c":96:1)
#loc181 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":131:6)
#loc182 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":207:6)
#loc184 = loc("benchmarks/polybench/2mm/2mm.c":24:1)
#loc185 = loc("benchmarks/polybench/2mm/2mm.c":25:7)
#loc186 = loc("benchmarks/polybench/2mm/2mm.c":25:3)
#loc188 = loc("benchmarks/polybench/2mm/2mm.c":29:10)
#loc189 = loc("benchmarks/polybench/2mm/2mm.c":29:5)
#loc190 = loc("benchmarks/polybench/2mm/2mm.c":31:3)
#loc191 = loc("benchmarks/polybench/2mm/2mm.c":32:1)
#loc192 = loc("carts/.install/carts/include/arts/utils/benchmarks/CartsBenchmarks.h":77:6)
