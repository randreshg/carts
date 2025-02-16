cholesky.c:51:13: warning: incompatible pointer types initializing 'double (*)[NB][NB]' with an expression of type 'double *(*)[NB]' [-Wincompatible-pointer-types]
   51 |     double(*AhDep)[NB][NB] = Ah;
      |             ^                ~~
- Depend clause: 2
depExpr:
ArraySubscriptExpr 0x559429af8938 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429af8908 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429af8898 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429af8868 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429af8828 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429af8880 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429af8848 'int' lvalue Var 0x559429af86c0 'k' 'int'
`-ImplicitCastExpr 0x559429af8920 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429af88b8 'int' lvalue Var 0x559429af86c0 'k' 'int'
- Depend: %94 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- TaskOp: "omp.task"(%100) <{depends = [#omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 1, 0, 0>}> ({
}) : (memref<f64>) -> ()
- Depend clause: 0
depExpr:
ArraySubscriptExpr 0x559429af9d58 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429af9d28 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429af9ce8 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429af9cb8 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429af9c78 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429af9cd0 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429af9c98 'int' lvalue Var 0x559429af86c0 'k' 'int'
`-ImplicitCastExpr 0x559429af9d40 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429af9d08 'int' lvalue Var 0x559429af86c0 'k' 'int'
- Depend: %124 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- Depend clause: 2
depExpr:
ArraySubscriptExpr 0x559429af9ea8 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429af9e78 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429af9e38 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429af9e08 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429af9dc8 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429af9e20 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429af9de8 'int' lvalue Var 0x559429af86c0 'k' 'int'
`-ImplicitCastExpr 0x559429af9e90 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429af9e58 'int' lvalue Var 0x559429af9ab8 'i' 'int'
- Depend: %140 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- TaskOp: "omp.task"(%134, %150) <{depends = [#omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 2, 0, 0>}> ({
}) : (memref<f64>, memref<f64>) -> ()
- Depend clause: 0
depExpr:
ArraySubscriptExpr 0x559429afed28 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429afecf8 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429afecb8 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429afec88 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429afec48 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429afeca0 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429afec68 'int' lvalue Var 0x559429af86c0 'k' 'int'
`-ImplicitCastExpr 0x559429afed10 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429afecd8 'int' lvalue Var 0x559429afe8b0 'l' 'int'
- Depend: %160 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- Depend clause: 0
depExpr:
ArraySubscriptExpr 0x559429afee78 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429afee48 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429afee08 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429afedd8 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429afed98 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429afedf0 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429afedb8 'int' lvalue Var 0x559429af86c0 'k' 'int'
`-ImplicitCastExpr 0x559429afee60 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429afee28 'int' lvalue Var 0x559429afea88 'j' 'int'
- Depend: %176 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- Depend clause: 2
depExpr:
ArraySubscriptExpr 0x559429afefc8 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429afef98 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429afef58 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429afef28 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429afeee8 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429afef40 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429afef08 'int' lvalue Var 0x559429afea88 'j' 'int'
`-ImplicitCastExpr 0x559429afefb0 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429afef78 'int' lvalue Var 0x559429afe8b0 'l' 'int'
- Depend: %192 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- TaskOp: "omp.task"(%174, %190, %206) <{depends = [#omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 3, 0, 0>}> ({
}) : (memref<f64>, memref<f64>, memref<f64>) -> ()
- Depend clause: 0
depExpr:
ArraySubscriptExpr 0x559429b025b0 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429b02580 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429b02540 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429b02510 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429b024d0 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429b02528 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429b024f0 'int' lvalue Var 0x559429af86c0 'k' 'int'
`-ImplicitCastExpr 0x559429b02598 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429b02560 'int' lvalue Var 0x559429afe8b0 'l' 'int'
- Depend: %158 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- Depend clause: 2
depExpr:
ArraySubscriptExpr 0x559429b02700 'double[NB]' lvalue
|-ImplicitCastExpr 0x559429b026d0 'double (*)[NB]' <ArrayToPointerDecay>
| `-ArraySubscriptExpr 0x559429b02690 'double[NB][NB]':'double[NB][NB]' lvalue
|   |-ImplicitCastExpr 0x559429b02660 'double (*)[NB][NB]' <LValueToRValue>
|   | `-DeclRefExpr 0x559429b02620 'double (*)[NB][NB]' lvalue Var 0x559429af7bb0 'AhDep' 'double (*)[NB][NB]'
|   `-ImplicitCastExpr 0x559429b02678 'int' <LValueToRValue>
|     `-DeclRefExpr 0x559429b02640 'int' lvalue Var 0x559429afe8b0 'l' 'int'
`-ImplicitCastExpr 0x559429b026e8 'int' <LValueToRValue>
  `-DeclRefExpr 0x559429b026b0 'int' lvalue Var 0x559429afe8b0 'l' 'int'
- Depend: %174 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
- TaskOp: "omp.task"(%168, %184) <{depends = [#omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 2, 0, 0>}> ({
}) : (memref<f64>, memref<f64>) -> ()
VarDecl 0x559429af29e8 <cholesky.c:18:1, col:46> col:15 used original_matrix 'double[256]' static cinit
`-InitListExpr 0x559429af2ac8 <col:44, col:46> 'double[256]'
  |-array_filler: ImplicitValueInitExpr 0x559429af2b28 <<invalid sloc>> 'double'
  `-ImplicitCastExpr 0x559429af2b08 <col:45> 'double' <IntegralToFloating>
    `-IntegerLiteral 0x559429af2a50 <col:45> 'int' 0
InitListExpr 0x559429af2ac8 'double[256]'
|-array_filler: ImplicitValueInitExpr 0x559429af2b28 'double'
`-ImplicitCastExpr 0x559429af2b08 'double' <IntegralToFloating>
  `-IntegerLiteral 0x559429af2a50 'int' 0
 warning not initializing global: original_matrix
VarDecl 0x559429af27f0 <cholesky.c:17:1, col:37> col:15 used matrix 'double[256]' static cinit
|-InitListExpr 0x559429af28d0 <col:35, col:37> 'double[256]'
| |-array_filler: ImplicitValueInitExpr 0x559429af2930 <<invalid sloc>> 'double'
| `-ImplicitCastExpr 0x559429af2910 <col:36> 'double' <IntegralToFloating>
|   `-IntegerLiteral 0x559429af2858 <col:36> 'int' 0
`-FullComment 0x559429b08f70 <line:15:4, col:98>
  `-ParagraphComment 0x559429b08f40 <col:4, col:98>
    |-TextComment 0x559429b08ec0 <col:4, col:82> Text=" cgeist cholesky.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include "
    |-TextComment 0x559429b08ee0 <col:83> Text="&"
    `-TextComment 0x559429b08f00 <col:84, col:98> Text="> cholesky.mlir"
InitListExpr 0x559429af28d0 'double[256]'
|-array_filler: ImplicitValueInitExpr 0x559429af2930 'double'
`-ImplicitCastExpr 0x559429af2910 'double' <IntegralToFloating>
  `-IntegerLiteral 0x559429af2858 'int' 0
 warning not initializing global: matrix
loc("cholesky.c":121:3): error: 'memref.alloca' op dimension operand count does not equal memref dynamic dimension count
"builtin.module"() ({
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<13 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str6", value = "%s %d %d %d\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<13 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str5", value = "BLAS_dfpinfo\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "memref.global"() <{initial_value, sym_name = "dummy", type = memref<1xmemref<?xmemref<?xmemref<?xf64>>>>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "matrix", sym_visibility = "private", type = memref<256xf64>}> : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<9 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str4", value = "ts = %d\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<27 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str3", value = "num_iter must be positive\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<48 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str2", value = "NB = %d, DIM = %d, NB must be smaller than DIM\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<41 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str1", value = "Usage: <block number> <num_iterations> \0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, global_type = memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>, linkage = #llvm.linkage<external>, sym_name = "stderr", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.func"() <{CConv = #llvm.cconv<ccc>, function_type = !llvm.func<i32 (ptr, ptr, ...)>, linkage = #llvm.linkage<external>, sym_name = "fprintf", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.mlir.global"() <{addr_space = 0 : i32, constant, global_type = !llvm.array<14 x i8>, linkage = #llvm.linkage<internal>, sym_name = "str0", value = "%g ms passed\0A\00", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "llvm.func"() <{CConv = #llvm.cconv<ccc>, function_type = !llvm.func<i32 (ptr, ...)>, linkage = #llvm.linkage<external>, sym_name = "printf", visibility_ = 0 : i64}> ({
  }) : () -> ()
  "memref.global"() <{initial_value, sym_name = "original_matrix", sym_visibility = "private", type = memref<256xf64>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "NB", type = memref<1xi32>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_gemm@static@DMONE@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_gemm@static@DMONE", sym_visibility = "private", type = memref<1xf64>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_gemm@static@DONE@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_gemm@static@DONE", sym_visibility = "private", type = memref<1xf64>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_gemm@static@NT@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_gemm@static@NT", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_gemm@static@TR@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_gemm@static@TR", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_syrk@static@DMONE@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_syrk@static@DMONE", sym_visibility = "private", type = memref<1xf64>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_syrk@static@DONE@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_syrk@static@DONE", sym_visibility = "private", type = memref<1xf64>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_syrk@static@NT@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_syrk@static@NT", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_syrk@static@LO@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_syrk@static@LO", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_trsm@static@DONE@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_trsm@static@DONE", sym_visibility = "private", type = memref<1xf64>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_trsm@static@RI@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_trsm@static@RI", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_trsm@static@NU@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_trsm@static@NU", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_trsm@static@TR@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_trsm@static@TR", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_trsm@static@LO@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_trsm@static@LO", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "omp_potrf@static@L@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_potrf@static@L", sym_visibility = "private", type = memref<1xi8>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "omp_potrf@static@INFO", sym_visibility = "private", type = memref<1xi32>}> : () -> ()
  "memref.global"() <{initial_value = dense<true> : tensor<1xi1>, sym_name = "get_time@static@gtod_ref_time_sec@init", sym_visibility = "private", type = memref<1xi1>}> : () -> ()
  "memref.global"() <{initial_value, sym_name = "get_time@static@gtod_ref_time_sec", sym_visibility = "private", type = memref<1xf64>}> : () -> ()
  "func.func"() <{function_type = (memref<?xmemref<?xf64>>, i32, i32, f32) -> (), sym_name = "add_to_diag_hierarchical"}> ({
  ^bb0(%arg0: memref<?xmemref<?xf64>>, %arg1: i32, %arg2: i32, %arg3: f32):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.muli"(%arg2, %arg1) : (i32, i32) -> i32
    %3 = "arith.index_cast"(%2) : (i32) -> index
    %4 = "arith.extf"(%arg3) : (f32) -> f64
    "scf.for"(%0, %3, %1) ({
    ^bb0(%arg4: index):
      %5 = "arith.index_cast"(%arg4) : (index) -> i32
      %6 = "arith.divsi"(%5, %arg1) : (i32, i32) -> i32
      %7 = "arith.muli"(%6, %arg2) : (i32, i32) -> i32
      %8 = "arith.addi"(%7, %6) : (i32, i32) -> i32
      %9 = "arith.index_cast"(%8) : (i32) -> index
      %10 = "memref.load"(%arg0, %9) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>
      %11 = "arith.remsi"(%5, %arg1) : (i32, i32) -> i32
      %12 = "arith.muli"(%11, %arg1) : (i32, i32) -> i32
      %13 = "arith.addi"(%12, %11) : (i32, i32) -> i32
      %14 = "arith.index_cast"(%13) : (i32) -> index
      %15 = "memref.load"(%10, %14) <{nontemporal = false}> : (memref<?xf64>, index) -> f64
      %16 = "arith.addf"(%15, %4) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
      "memref.store"(%16, %10, %14) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xf64>, i32, f64) -> (), sym_name = "add_to_diag"}> ({
  ^bb0(%arg0: memref<?xf64>, %arg1: i32, %arg2: f64):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.index_cast"(%arg1) : (i32) -> index
    "scf.for"(%0, %2, %1) ({
    ^bb0(%arg3: index):
      %3 = "arith.index_cast"(%arg3) : (index) -> i32
      %4 = "arith.muli"(%3, %arg1) : (i32, i32) -> i32
      %5 = "arith.addi"(%3, %4) : (i32, i32) -> i32
      %6 = "arith.index_cast"(%5) : (i32) -> index
      %7 = "memref.load"(%arg0, %6) <{nontemporal = false}> : (memref<?xf64>, index) -> f64
      %8 = "arith.addf"(%7, %arg2) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
      "memref.store"(%8, %arg0, %6) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = () -> f32, sym_name = "get_time"}> ({
    %0 = "arith.constant"() <{value = 9.9999999999999995E-7 : f64}> : () -> f64
    %1 = "arith.constant"() <{value = false}> : () -> i1
    %2 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %3 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1x2xi64>
    %4 = "memref.get_global"() <{name = @"get_time@static@gtod_ref_time_sec"}> : () -> memref<1xf64>
    %5 = "memref.get_global"() <{name = @"get_time@static@gtod_ref_time_sec@init"}> : () -> memref<1xi1>
    %6 = "affine.load"(%5) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%6) ({
      "affine.store"(%1, %5) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%2, %4) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %7 = "memref.cast"(%3) : (memref<1x2xi64>) -> memref<?x2xi64>
    %8 = "llvm.mlir.zero"() : () -> !llvm.ptr
    %9 = "polygeist.pointer2memref"(%8) : (!llvm.ptr) -> memref<?xi8>
    %10 = "func.call"(%7, %9) <{callee = @gettimeofday}> : (memref<?x2xi64>, memref<?xi8>) -> i32
    %11 = "affine.load"(%4) <{map = affine_map<() -> (0)>}> : (memref<1xf64>) -> f64
    %12 = "arith.cmpf"(%11, %2) <{predicate = 1 : i64}> : (f64, f64) -> i1
    %13 = "scf.if"(%12) ({
      %22 = "affine.load"(%3) <{map = affine_map<() -> (0, 0)>}> : (memref<1x2xi64>) -> i64
      %23 = "arith.sitofp"(%22) : (i64) -> f64
      "affine.store"(%23, %4) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"(%23) : (f64) -> ()
    }, {
      "scf.yield"(%11) : (f64) -> ()
    }) : (i1) -> f64
    %14 = "affine.load"(%3) <{map = affine_map<() -> (0, 0)>}> : (memref<1x2xi64>) -> i64
    %15 = "arith.sitofp"(%14) : (i64) -> f64
    %16 = "arith.subf"(%15, %13) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    %17 = "affine.load"(%3) <{map = affine_map<() -> (0, 1)>}> : (memref<1x2xi64>) -> i64
    %18 = "arith.sitofp"(%17) : (i64) -> f64
    %19 = "arith.mulf"(%18, %0) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    %20 = "arith.addf"(%16, %19) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
    %21 = "arith.truncf"(%20) : (f64) -> f32
    "func.return"(%21) : (f32) -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?x2xi64>, memref<?xi8>) -> i32, sym_name = "gettimeofday", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, i32, memref<?xf64>) -> (), sym_name = "initialize_matrix"}> ({
  ^bb0(%arg0: i32, %arg1: i32, %arg2: memref<?xf64>):
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xf64>, i32, i32, memref<?x?xf64>) -> (), sym_name = "omp_potrf"}> ({
  ^bb0(%arg0: memref<?xf64>, %arg1: i32, %arg2: i32, %arg3: memref<?x?xf64>):
    %0 = "arith.constant"() <{value = false}> : () -> i1
    %1 = "arith.constant"() <{value = 76 : i8}> : () -> i8
    %2 = "memref.get_global"() <{name = @"omp_potrf@static@L"}> : () -> memref<1xi8>
    %3 = "memref.get_global"() <{name = @"omp_potrf@static@INFO"}> : () -> memref<1xi32>
    %4 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    %5 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    "affine.store"(%arg1, %5) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    "affine.store"(%arg2, %4) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    %6 = "memref.get_global"() <{name = @"omp_potrf@static@L@init"}> : () -> memref<1xi1>
    %7 = "affine.load"(%6) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%7) ({
      "affine.store"(%0, %6) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%1, %2) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %8 = "memref.cast"(%2) : (memref<1xi8>) -> memref<?xi8>
    %9 = "memref.cast"(%5) : (memref<1xi32>) -> memref<?xi32>
    %10 = "memref.cast"(%4) : (memref<1xi32>) -> memref<?xi32>
    %11 = "memref.cast"(%3) : (memref<1xi32>) -> memref<?xi32>
    "func.call"(%8, %9, %arg0, %10, %11) <{callee = @dpotrf_}> : (memref<?xi8>, memref<?xi32>, memref<?xf64>, memref<?xi32>, memref<?xi32>) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi8>, memref<?xi32>, memref<?xf64>, memref<?xi32>, memref<?xi32>) -> (), sym_name = "dpotrf_", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> (), sym_name = "omp_trsm"}> ({
  ^bb0(%arg0: memref<?xf64>, %arg1: memref<?xf64>, %arg2: i32, %arg3: i32, %arg4: memref<?x?xf64>):
    %0 = "arith.constant"() <{value = 1.000000e+00 : f64}> : () -> f64
    %1 = "arith.constant"() <{value = 82 : i8}> : () -> i8
    %2 = "arith.constant"() <{value = 78 : i8}> : () -> i8
    %3 = "arith.constant"() <{value = 84 : i8}> : () -> i8
    %4 = "arith.constant"() <{value = false}> : () -> i1
    %5 = "arith.constant"() <{value = 76 : i8}> : () -> i8
    %6 = "memref.get_global"() <{name = @"omp_trsm@static@DONE"}> : () -> memref<1xf64>
    %7 = "memref.get_global"() <{name = @"omp_trsm@static@RI"}> : () -> memref<1xi8>
    %8 = "memref.get_global"() <{name = @"omp_trsm@static@NU"}> : () -> memref<1xi8>
    %9 = "memref.get_global"() <{name = @"omp_trsm@static@TR"}> : () -> memref<1xi8>
    %10 = "memref.get_global"() <{name = @"omp_trsm@static@LO"}> : () -> memref<1xi8>
    %11 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    %12 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    "affine.store"(%arg2, %12) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    "affine.store"(%arg3, %11) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    %13 = "memref.get_global"() <{name = @"omp_trsm@static@LO@init"}> : () -> memref<1xi1>
    %14 = "affine.load"(%13) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%14) ({
      "affine.store"(%4, %13) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%5, %10) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %15 = "memref.get_global"() <{name = @"omp_trsm@static@TR@init"}> : () -> memref<1xi1>
    %16 = "affine.load"(%15) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%16) ({
      "affine.store"(%4, %15) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%3, %9) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %17 = "memref.get_global"() <{name = @"omp_trsm@static@NU@init"}> : () -> memref<1xi1>
    %18 = "affine.load"(%17) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%18) ({
      "affine.store"(%4, %17) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%2, %8) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %19 = "memref.get_global"() <{name = @"omp_trsm@static@RI@init"}> : () -> memref<1xi1>
    %20 = "affine.load"(%19) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%20) ({
      "affine.store"(%4, %19) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%1, %7) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %21 = "memref.get_global"() <{name = @"omp_trsm@static@DONE@init"}> : () -> memref<1xi1>
    %22 = "affine.load"(%21) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%22) ({
      "affine.store"(%4, %21) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%0, %6) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %23 = "memref.cast"(%7) : (memref<1xi8>) -> memref<?xi8>
    %24 = "memref.cast"(%10) : (memref<1xi8>) -> memref<?xi8>
    %25 = "memref.cast"(%9) : (memref<1xi8>) -> memref<?xi8>
    %26 = "memref.cast"(%8) : (memref<1xi8>) -> memref<?xi8>
    %27 = "memref.cast"(%12) : (memref<1xi32>) -> memref<?xi32>
    %28 = "memref.cast"(%6) : (memref<1xf64>) -> memref<?xf64>
    %29 = "memref.cast"(%11) : (memref<1xi32>) -> memref<?xi32>
    "func.call"(%23, %24, %25, %26, %27, %27, %28, %arg0, %29, %arg1, %29) <{callee = @dtrsm_}> : (memref<?xi8>, memref<?xi8>, memref<?xi8>, memref<?xi8>, memref<?xi32>, memref<?xi32>, memref<?xf64>, memref<?xf64>, memref<?xi32>, memref<?xf64>, memref<?xi32>) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi8>, memref<?xi8>, memref<?xi8>, memref<?xi8>, memref<?xi32>, memref<?xi32>, memref<?xf64>, memref<?xf64>, memref<?xi32>, memref<?xf64>, memref<?xi32>) -> (), sym_name = "dtrsm_", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> (), sym_name = "omp_syrk"}> ({
  ^bb0(%arg0: memref<?xf64>, %arg1: memref<?xf64>, %arg2: i32, %arg3: i32, %arg4: memref<?x?xf64>):
    %0 = "arith.constant"() <{value = -1.000000e+00 : f64}> : () -> f64
    %1 = "arith.constant"() <{value = 1.000000e+00 : f64}> : () -> f64
    %2 = "arith.constant"() <{value = 78 : i8}> : () -> i8
    %3 = "arith.constant"() <{value = false}> : () -> i1
    %4 = "arith.constant"() <{value = 76 : i8}> : () -> i8
    %5 = "memref.get_global"() <{name = @"omp_syrk@static@DMONE"}> : () -> memref<1xf64>
    %6 = "memref.get_global"() <{name = @"omp_syrk@static@DONE"}> : () -> memref<1xf64>
    %7 = "memref.get_global"() <{name = @"omp_syrk@static@NT"}> : () -> memref<1xi8>
    %8 = "memref.get_global"() <{name = @"omp_syrk@static@LO"}> : () -> memref<1xi8>
    %9 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    %10 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    "affine.store"(%arg2, %10) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    "affine.store"(%arg3, %9) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    %11 = "memref.get_global"() <{name = @"omp_syrk@static@LO@init"}> : () -> memref<1xi1>
    %12 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%12) ({
      "affine.store"(%3, %11) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%4, %8) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %13 = "memref.get_global"() <{name = @"omp_syrk@static@NT@init"}> : () -> memref<1xi1>
    %14 = "affine.load"(%13) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%14) ({
      "affine.store"(%3, %13) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%2, %7) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %15 = "memref.get_global"() <{name = @"omp_syrk@static@DONE@init"}> : () -> memref<1xi1>
    %16 = "affine.load"(%15) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%16) ({
      "affine.store"(%3, %15) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%1, %6) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %17 = "memref.get_global"() <{name = @"omp_syrk@static@DMONE@init"}> : () -> memref<1xi1>
    %18 = "affine.load"(%17) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%18) ({
      "affine.store"(%3, %17) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%0, %5) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %19 = "memref.cast"(%8) : (memref<1xi8>) -> memref<?xi8>
    %20 = "memref.cast"(%7) : (memref<1xi8>) -> memref<?xi8>
    %21 = "memref.cast"(%10) : (memref<1xi32>) -> memref<?xi32>
    %22 = "memref.cast"(%5) : (memref<1xf64>) -> memref<?xf64>
    %23 = "memref.cast"(%9) : (memref<1xi32>) -> memref<?xi32>
    %24 = "memref.cast"(%6) : (memref<1xf64>) -> memref<?xf64>
    "func.call"(%19, %20, %21, %21, %22, %arg0, %23, %24, %arg1, %23) <{callee = @dsyrk_}> : (memref<?xi8>, memref<?xi8>, memref<?xi32>, memref<?xi32>, memref<?xf64>, memref<?xf64>, memref<?xi32>, memref<?xf64>, memref<?xf64>, memref<?xi32>) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi8>, memref<?xi8>, memref<?xi32>, memref<?xi32>, memref<?xf64>, memref<?xf64>, memref<?xi32>, memref<?xf64>, memref<?xf64>, memref<?xi32>) -> (), sym_name = "dsyrk_", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xf64>, memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> (), sym_name = "omp_gemm"}> ({
  ^bb0(%arg0: memref<?xf64>, %arg1: memref<?xf64>, %arg2: memref<?xf64>, %arg3: i32, %arg4: i32, %arg5: memref<?x?xf64>):
    %0 = "arith.constant"() <{value = -1.000000e+00 : f64}> : () -> f64
    %1 = "arith.constant"() <{value = 1.000000e+00 : f64}> : () -> f64
    %2 = "arith.constant"() <{value = 78 : i8}> : () -> i8
    %3 = "arith.constant"() <{value = false}> : () -> i1
    %4 = "arith.constant"() <{value = 84 : i8}> : () -> i8
    %5 = "memref.get_global"() <{name = @"omp_gemm@static@DMONE"}> : () -> memref<1xf64>
    %6 = "memref.get_global"() <{name = @"omp_gemm@static@DONE"}> : () -> memref<1xf64>
    %7 = "memref.get_global"() <{name = @"omp_gemm@static@NT"}> : () -> memref<1xi8>
    %8 = "memref.get_global"() <{name = @"omp_gemm@static@TR"}> : () -> memref<1xi8>
    %9 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    %10 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1xi32>
    "affine.store"(%arg3, %10) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    "affine.store"(%arg4, %9) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    %11 = "memref.get_global"() <{name = @"omp_gemm@static@TR@init"}> : () -> memref<1xi1>
    %12 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%12) ({
      "affine.store"(%3, %11) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%4, %8) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %13 = "memref.get_global"() <{name = @"omp_gemm@static@NT@init"}> : () -> memref<1xi1>
    %14 = "affine.load"(%13) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%14) ({
      "affine.store"(%3, %13) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%2, %7) <{map = affine_map<() -> (0)>}> : (i8, memref<1xi8>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %15 = "memref.get_global"() <{name = @"omp_gemm@static@DONE@init"}> : () -> memref<1xi1>
    %16 = "affine.load"(%15) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%16) ({
      "affine.store"(%3, %15) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%1, %6) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %17 = "memref.get_global"() <{name = @"omp_gemm@static@DMONE@init"}> : () -> memref<1xi1>
    %18 = "affine.load"(%17) <{map = affine_map<() -> (0)>}> : (memref<1xi1>) -> i1
    "scf.if"(%18) ({
      "affine.store"(%3, %17) <{map = affine_map<() -> (0)>}> : (i1, memref<1xi1>) -> ()
      "affine.store"(%0, %5) <{map = affine_map<() -> (0)>}> : (f64, memref<1xf64>) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %19 = "memref.cast"(%7) : (memref<1xi8>) -> memref<?xi8>
    %20 = "memref.cast"(%8) : (memref<1xi8>) -> memref<?xi8>
    %21 = "memref.cast"(%10) : (memref<1xi32>) -> memref<?xi32>
    %22 = "memref.cast"(%5) : (memref<1xf64>) -> memref<?xf64>
    %23 = "polygeist.memref2pointer"(%arg0) : (memref<?xf64>) -> !llvm.ptr
    %24 = "polygeist.pointer2memref"(%23) : (!llvm.ptr) -> memref<?xi8>
    %25 = "memref.cast"(%9) : (memref<1xi32>) -> memref<?xi32>
    %26 = "polygeist.memref2pointer"(%arg1) : (memref<?xf64>) -> !llvm.ptr
    %27 = "polygeist.pointer2memref"(%26) : (!llvm.ptr) -> memref<?xi8>
    %28 = "memref.cast"(%6) : (memref<1xf64>) -> memref<?xf64>
    %29 = "polygeist.memref2pointer"(%arg2) : (memref<?xf64>) -> !llvm.ptr
    %30 = "polygeist.pointer2memref"(%29) : (!llvm.ptr) -> memref<?xi8>
    "func.call"(%19, %20, %21, %21, %21, %22, %24, %25, %27, %25, %28, %30, %25) <{callee = @dgemm_}> : (memref<?xi8>, memref<?xi8>, memref<?xi32>, memref<?xi32>, memref<?xi32>, memref<?xf64>, memref<?xi8>, memref<?xi32>, memref<?xi8>, memref<?xi32>, memref<?xf64>, memref<?xi8>, memref<?xi32>) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi8>, memref<?xi8>, memref<?xi32>, memref<?xi32>, memref<?xi32>, memref<?xf64>, memref<?xi8>, memref<?xi32>, memref<?xi8>, memref<?xi32>, memref<?xf64>, memref<?xi8>, memref<?xi32>) -> (), sym_name = "dgemm_", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?x?xmemref<?xf64>>, i32) -> (), sym_name = "cholesky_blocked"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?x?xmemref<?xf64>>, %arg2: i32):
    %0 = "arith.constant"() <{value = 1 : index}> : () -> index
    %1 = "arith.constant"() <{value = 16 : i32}> : () -> i32
    %2 = "arith.constant"() <{value = 1.000000e+06 : f64}> : () -> f64
    %3 = "arith.constant"() <{value = 1.000000e+03 : f64}> : () -> f64
    %4 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %5 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %6 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %7 = "arith.constant"() <{value = 0 : index}> : () -> index
    %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
    "affine.store"(%6, %8) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
    "omp.parallel"() <{operandSegmentSizes = array<i32: 0, 0, 0, 0, 0>}> ({
      "omp.barrier"() : () -> ()
      %9 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %10 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %11 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %12 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %13 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %14 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %15 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      %16 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<f64>
      "omp.master"() ({
        %17 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1x2xi64>
        %18 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<1x2xi64>
        %19 = "polygeist.memref2pointer"(%arg1) : (memref<?x?xmemref<?xf64>>) -> !llvm.ptr
        %20 = "polygeist.pointer2memref"(%19) : (!llvm.ptr) -> memref<?x?xf64>
        %21 = "arith.index_cast"(%arg2) : (i32) -> index
        %22 = "memref.cast"(%18) : (memref<1x2xi64>) -> memref<?x2xi64>
        %23 = "memref.get_global"() <{name = @NB}> : () -> memref<1xi32>
        %24 = "memref.cast"(%17) : (memref<1x2xi64>) -> memref<?x2xi64>
        %25 = "memref.get_global"() <{name = @original_matrix}> : () -> memref<256xf64>
        %26 = "memref.cast"(%25) : (memref<256xf64>) -> memref<?xf64>
        %27 = "polygeist.subindex"(%arg1, %7) : (memref<?x?xmemref<?xf64>>, index) -> memref<?xmemref<?xf64>>
        "scf.for"(%7, %21, %0) ({
        ^bb0(%arg3: index):
          %32 = "func.call"(%4, %22) <{callee = @clock_gettime}> : (i32, memref<?x2xi64>) -> i32
          %33 = "scf.while"(%5) ({
          ^bb0(%arg4: i32):
            %49 = "affine.load"(%23) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
            %50 = "arith.cmpi"(%arg4, %49) <{predicate = 2 : i64}> : (i32, i32) -> i1
            "scf.condition"(%50, %arg4) : (i1, i32) -> ()
          }, {
          ^bb0(%arg4: i32):
            %49 = "arith.index_cast"(%arg4) : (i32) -> index
            %50 = "memref.load"(%20, %49, %49) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
            "affine.store"(%50, %12) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
            "omp.task"(%12) <{depends = [#omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 1, 0, 0>}> ({
              %55 = "memref.load"(%arg1, %49, %49) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
              "func.call"(%55, %arg0, %arg0, %20) <{callee = @omp_potrf}> : (memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
              "omp.terminator"() : () -> ()
            }) : (memref<f64>) -> ()
            %51 = "arith.addi"(%arg4, %4) : (i32, i32) -> i32
            %52 = "scf.while"(%51) ({
            ^bb0(%arg5: i32):
              %55 = "affine.load"(%23) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
              %56 = "arith.cmpi"(%arg5, %55) <{predicate = 2 : i64}> : (i32, i32) -> i1
              "scf.condition"(%56, %arg5) : (i1, i32) -> ()
            }, {
            ^bb0(%arg5: i32):
              %55 = "memref.load"(%20, %49, %49) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
              "affine.store"(%55, %13) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
              %56 = "arith.index_cast"(%arg5) : (i32) -> index
              %57 = "memref.load"(%20, %49, %56) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
              "affine.store"(%57, %14) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
              "omp.task"(%13, %14) <{depends = [#omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 2, 0, 0>}> ({
                %59 = "memref.load"(%arg1, %49, %49) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                %60 = "memref.load"(%arg1, %49, %56) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                "func.call"(%59, %60, %arg0, %arg0, %20) <{callee = @omp_trsm}> : (memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
                "omp.terminator"() : () -> ()
              }) : (memref<f64>, memref<f64>) -> ()
              %58 = "arith.addi"(%arg5, %4) : (i32, i32) -> i32
              "scf.yield"(%58) : (i32) -> ()
            }) : (i32) -> i32
            %53 = "arith.index_cast"(%51) : (i32) -> index
            %54 = "scf.while"(%51) ({
            ^bb0(%arg5: i32):
              %55 = "affine.load"(%23) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
              %56 = "arith.cmpi"(%arg5, %55) <{predicate = 2 : i64}> : (i32, i32) -> i1
              "scf.condition"(%56, %arg5) : (i1, i32) -> ()
            }, {
            ^bb0(%arg5: i32):
              %55 = "arith.index_cast"(%arg5) : (i32) -> index
              "scf.for"(%53, %55, %0) ({
              ^bb0(%arg6: index):
                %59 = "memref.load"(%20, %49, %55) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
                "affine.store"(%59, %9) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
                %60 = "memref.load"(%20, %49, %arg6) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
                "affine.store"(%60, %10) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
                %61 = "memref.load"(%20, %arg6, %55) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
                "affine.store"(%61, %11) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
                "omp.task"(%9, %10, %11) <{depends = [#omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 3, 0, 0>}> ({
                  %62 = "memref.load"(%arg1, %49, %55) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                  %63 = "memref.load"(%arg1, %49, %arg6) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                  %64 = "memref.load"(%arg1, %arg6, %55) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                  "func.call"(%62, %63, %64, %arg0, %arg0, %20) <{callee = @omp_gemm}> : (memref<?xf64>, memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
                  "omp.terminator"() : () -> ()
                }) : (memref<f64>, memref<f64>, memref<f64>) -> ()
                "scf.yield"() : () -> ()
              }) : (index, index, index) -> ()
              %56 = "memref.load"(%20, %49, %55) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
              "affine.store"(%56, %15) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
              %57 = "memref.load"(%20, %55, %55) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
              "affine.store"(%57, %16) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
              "omp.task"(%15, %16) <{depends = [#omp<clause_task_depend(taskdependin)>, #omp<clause_task_depend(taskdependinout)>], operandSegmentSizes = array<i32: 0, 0, 0, 0, 2, 0, 0>}> ({
                %59 = "memref.load"(%arg1, %49, %55) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                %60 = "memref.load"(%arg1, %55, %55) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
                "func.call"(%59, %60, %arg0, %arg0, %20) <{callee = @omp_syrk}> : (memref<?xf64>, memref<?xf64>, i32, i32, memref<?x?xf64>) -> ()
                "omp.terminator"() : () -> ()
              }) : (memref<f64>, memref<f64>) -> ()
              %58 = "arith.addi"(%arg5, %4) : (i32, i32) -> i32
              "scf.yield"(%58) : (i32) -> ()
            }) : (i32) -> i32
            "scf.yield"(%51) : (i32) -> ()
          }) : (i32) -> i32
          "omp.taskwait"() : () -> ()
          %34 = "func.call"(%4, %24) <{callee = @clock_gettime}> : (i32, memref<?x2xi64>) -> i32
          %35 = "affine.load"(%17) <{map = affine_map<() -> (0, 0)>}> : (memref<1x2xi64>) -> i64
          %36 = "affine.load"(%18) <{map = affine_map<() -> (0, 0)>}> : (memref<1x2xi64>) -> i64
          %37 = "arith.subi"(%35, %36) : (i64, i64) -> i64
          %38 = "arith.sitofp"(%37) : (i64) -> f64
          %39 = "arith.mulf"(%38, %3) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
          %40 = "affine.load"(%17) <{map = affine_map<() -> (0, 1)>}> : (memref<1x2xi64>) -> i64
          %41 = "affine.load"(%18) <{map = affine_map<() -> (0, 1)>}> : (memref<1x2xi64>) -> i64
          %42 = "arith.subi"(%40, %41) : (i64, i64) -> i64
          %43 = "arith.sitofp"(%42) : (i64) -> f64
          %44 = "arith.divf"(%43, %2) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
          %45 = "arith.addf"(%39, %44) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
          %46 = "affine.load"(%8) <{map = affine_map<() -> ()>}> : (memref<f64>) -> f64
          %47 = "arith.addf"(%46, %45) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
          "affine.store"(%47, %8) <{map = affine_map<() -> ()>}> : (f64, memref<f64>) -> ()
          %48 = "affine.load"(%23) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
          "func.call"(%arg0, %48, %1, %26, %27) <{callee = @convert_to_blocks}> : (i32, i32, i32, memref<?xf64>, memref<?xmemref<?xf64>>) -> ()
          "scf.yield"() : () -> ()
        }) : (index, index, index) -> ()
        %28 = "llvm.mlir.addressof"() <{global_name = @str0}> : () -> !llvm.ptr
        %29 = "llvm.getelementptr"(%28) <{elem_type = !llvm.array<14 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
        %30 = "affine.load"(%8) <{map = affine_map<() -> ()>}> : (memref<f64>) -> f64
        %31 = "llvm.call"(%29, %30) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, f64) -> i32
        "omp.terminator"() : () -> ()
      }) : () -> ()
      "omp.barrier"() : () -> ()
      "omp.terminator"() : () -> ()
    }) : () -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?x2xi64>) -> i32, sym_name = "clock_gettime", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, i32, i32, memref<?x?xf64>, memref<?x?xmemref<?xf64>>) -> (), sym_name = "convert_to_blocks", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: i32, %arg2: i32, %arg3: memref<?x?xf64>, %arg4: memref<?x?xmemref<?xf64>>):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.index_cast"(%arg1) : (i32) -> index
    %3 = "arith.index_cast"(%arg0) : (i32) -> index
    "scf.for"(%0, %2, %1) ({
    ^bb0(%arg5: index):
      %4 = "arith.index_cast"(%arg5) : (index) -> i32
      %5 = "arith.muli"(%4, %arg0) : (i32, i32) -> i32
      %6 = "arith.index_cast"(%5) : (i32) -> index
      "scf.for"(%0, %2, %1) ({
      ^bb0(%arg6: index):
        %7 = "arith.index_cast"(%arg6) : (index) -> i32
        %8 = "arith.muli"(%7, %arg0) : (i32, i32) -> i32
        %9 = "arith.index_cast"(%8) : (i32) -> index
        %10 = "memref.load"(%arg4, %arg5, %arg6) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
        "scf.for"(%0, %3, %1) ({
        ^bb0(%arg7: index):
          %11 = "arith.index_cast"(%arg7) : (index) -> i32
          %12 = "arith.muli"(%11, %arg0) : (i32, i32) -> i32
          %13 = "arith.muli"(%11, %arg2) : (i32, i32) -> i32
          "scf.for"(%0, %3, %1) ({
          ^bb0(%arg8: index):
            %14 = "arith.index_cast"(%arg8) : (index) -> i32
            %15 = "arith.addi"(%12, %14) : (i32, i32) -> i32
            %16 = "arith.index_cast"(%15) : (i32) -> index
            %17 = "arith.addi"(%13, %14) : (i32, i32) -> i32
            %18 = "arith.index_cast"(%17) : (i32) -> index
            %19 = "arith.addi"(%18, %9) : (index, index) -> index
            %20 = "memref.load"(%arg3, %6, %19) <{nontemporal = false}> : (memref<?x?xf64>, index, index) -> f64
            "memref.store"(%20, %10, %16) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()
            "scf.yield"() : () -> ()
          }) : (index, index, index) -> ()
          "scf.yield"() : () -> ()
        }) : (index, index, index) -> ()
        "scf.yield"() : () -> ()
      }) : (index, index, index) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<internal>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xmemref<?xi8>>) -> i32, sym_name = "main"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xmemref<?xi8>>):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.constant"() <{value = 2048 : index}> : () -> index
    %3 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %4 = "arith.constant"() <{value = 8 : index}> : () -> index
    %5 = "arith.constant"() <{value = 0 : i32}> : () -> i32
    %6 = "arith.constant"() <{value = -1 : i32}> : () -> i32
    %7 = "arith.constant"() <{value = 16 : i32}> : () -> i32
    %8 = "arith.constant"() <{value = 1 : i32}> : () -> i32
    %9 = "arith.constant"() <{value = 3 : i32}> : () -> i32
    %10 = "arith.cmpi"(%arg0, %9) <{predicate = 1 : i64}> : (i32, i32) -> i1
    "scf.if"(%10) ({
      %47 = "llvm.mlir.addressof"() <{global_name = @stderr}> : () -> !llvm.ptr
      %48 = "llvm.load"(%47) <{ordering = 0 : i64}> : (!llvm.ptr) -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %49 = "polygeist.memref2pointer"(%48) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %50 = "llvm.mlir.addressof"() <{global_name = @str1}> : () -> !llvm.ptr
      %51 = "llvm.getelementptr"(%50) <{elem_type = !llvm.array<41 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
      %52 = "llvm.call"(%49, %51) <{callee = @fprintf, callee_type = !llvm.func<i32 (ptr, ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, !llvm.ptr) -> i32
      "func.call"(%8) <{callee = @exit}> : (i32) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %11 = "memref.get_global"() <{name = @NB}> : () -> memref<1xi32>
    %12 = "affine.load"(%arg1) <{map = affine_map<() -> (1)>}> : (memref<?xmemref<?xi8>>) -> memref<?xi8>
    %13 = "func.call"(%12) <{callee = @atoi}> : (memref<?xi8>) -> i32
    "affine.store"(%13, %11) <{map = affine_map<() -> (0)>}> : (i32, memref<1xi32>) -> ()
    %14 = "arith.cmpi"(%13, %7) <{predicate = 4 : i64}> : (i32, i32) -> i1
    "scf.if"(%14) ({
      %47 = "llvm.mlir.addressof"() <{global_name = @stderr}> : () -> !llvm.ptr
      %48 = "llvm.load"(%47) <{ordering = 0 : i64}> : (!llvm.ptr) -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %49 = "polygeist.memref2pointer"(%48) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %50 = "llvm.mlir.addressof"() <{global_name = @str2}> : () -> !llvm.ptr
      %51 = "llvm.getelementptr"(%50) <{elem_type = !llvm.array<48 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
      %52 = "llvm.call"(%49, %51, %13, %7) <{callee = @fprintf, callee_type = !llvm.func<i32 (ptr, ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, !llvm.ptr, i32, i32) -> i32
      "func.call"(%6) <{callee = @exit}> : (i32) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %15 = "affine.load"(%arg1) <{map = affine_map<() -> (2)>}> : (memref<?xmemref<?xi8>>) -> memref<?xi8>
    %16 = "func.call"(%15) <{callee = @atoi}> : (memref<?xi8>) -> i32
    %17 = "arith.cmpi"(%16, %5) <{predicate = 2 : i64}> : (i32, i32) -> i1
    "scf.if"(%17) ({
      %47 = "llvm.mlir.addressof"() <{global_name = @stderr}> : () -> !llvm.ptr
      %48 = "llvm.load"(%47) <{ordering = 0 : i64}> : (!llvm.ptr) -> memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>
      %49 = "polygeist.memref2pointer"(%48) : (memref<?x!llvm.struct<(i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, i32, i32, i64, i16, i8, array<1 x i8>, ptr, i64, ptr, ptr, ptr, ptr, i64, i32, array<20 x i8>)>>) -> !llvm.ptr
      %50 = "llvm.mlir.addressof"() <{global_name = @str3}> : () -> !llvm.ptr
      %51 = "llvm.getelementptr"(%50) <{elem_type = !llvm.array<27 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
      %52 = "llvm.call"(%49, %51) <{callee = @fprintf, callee_type = !llvm.func<i32 (ptr, ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, !llvm.ptr) -> i32
      "func.call"(%8) <{callee = @exit}> : (i32) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %18 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
    %19 = "arith.divsi"(%7, %18) : (i32, i32) -> i32
    %20 = "llvm.mlir.addressof"() <{global_name = @str4}> : () -> !llvm.ptr
    %21 = "llvm.getelementptr"(%20) <{elem_type = !llvm.array<9 x i8>, rawConstantIndices = array<i32: 0, 0>}> : (!llvm.ptr) -> !llvm.ptr
    %22 = "llvm.call"(%21, %19) <{callee = @printf, callee_type = !llvm.func<i32 (ptr, ...)>, fastmathFlags = #llvm.fastmath<none>}> : (!llvm.ptr, i32) -> i32
    %23 = "memref.get_global"() <{name = @matrix}> : () -> memref<256xf64>
    %24 = "memref.cast"(%23) : (memref<256xf64>) -> memref<?xf64>
    %25 = "memref.get_global"() <{name = @dummy}> : () -> memref<1xmemref<?xmemref<?xmemref<?xf64>>>>
    %26 = "polygeist.typeSize"() <{source = memref<?xmemref<?xf64>>}> : () -> index
    %27 = "arith.index_cast"(%26) : (index) -> i64
    %28 = "arith.extsi"(%18) : (i32) -> i64
    %29 = "arith.muli"(%27, %28) : (i64, i64) -> i64
    %30 = "arith.index_cast"(%29) : (i64) -> index
    %31 = "arith.divui"(%30, %26) : (index, index) -> index
    %32 = "memref.alloc"(%31) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xmemref<?xmemref<?xf64>>>
    "affine.store"(%32, %25) <{map = affine_map<() -> (0)>}> : (memref<?xmemref<?xmemref<?xf64>>>, memref<1xmemref<?xmemref<?xmemref<?xf64>>>>) -> ()
    %33 = "polygeist.typeSize"() <{source = memref<?xf64>}> : () -> index
    %34 = "arith.index_cast"(%33) : (index) -> i64
    %35:2 = "scf.while"(%5) ({
    ^bb0(%arg2: i32):
      %47 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
      %48 = "arith.cmpi"(%arg2, %47) <{predicate = 2 : i64}> : (i32, i32) -> i1
      "scf.condition"(%48, %47, %arg2) : (i1, i32, i32) -> ()
    }, {
    ^bb0(%arg2: i32, %arg3: i32):
      %47 = "affine.load"(%25) <{map = affine_map<() -> (0)>}> : (memref<1xmemref<?xmemref<?xmemref<?xf64>>>>) -> memref<?xmemref<?xmemref<?xf64>>>
      %48 = "arith.index_cast"(%arg3) : (i32) -> index
      %49 = "arith.extsi"(%arg2) : (i32) -> i64
      %50 = "arith.muli"(%34, %49) : (i64, i64) -> i64
      %51 = "arith.index_cast"(%50) : (i64) -> index
      %52 = "arith.divui"(%51, %33) : (index, index) -> index
      %53 = "memref.alloc"(%52) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xmemref<?xf64>>
      "memref.store"(%53, %47, %48) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, memref<?xmemref<?xmemref<?xf64>>>, index) -> ()
      %54 = "arith.addi"(%arg3, %8) : (i32, i32) -> i32
      "scf.yield"(%54) : (i32) -> ()
    }) : (i32) -> (i32, i32)
    %36 = "scf.while"(%5) ({
    ^bb0(%arg2: i32):
      %47 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
      %48 = "arith.cmpi"(%arg2, %47) <{predicate = 2 : i64}> : (i32, i32) -> i1
      "scf.condition"(%48, %arg2) : (i1, i32) -> ()
    }, {
    ^bb0(%arg2: i32):
      %47 = "arith.index_cast"(%arg2) : (i32) -> index
      %48:2 = "scf.while"(%5) ({
      ^bb0(%arg3: i32):
        %50 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
        %51 = "arith.cmpi"(%arg3, %50) <{predicate = 2 : i64}> : (i32, i32) -> i1
        "scf.condition"(%51, %50, %arg3) : (i1, i32, i32) -> ()
      }, {
      ^bb0(%arg3: i32, %arg4: i32):
        %50 = "affine.load"(%25) <{map = affine_map<() -> (0)>}> : (memref<1xmemref<?xmemref<?xmemref<?xf64>>>>) -> memref<?xmemref<?xmemref<?xf64>>>
        %51 = "memref.load"(%50, %47) <{nontemporal = false}> : (memref<?xmemref<?xmemref<?xf64>>>, index) -> memref<?xmemref<?xf64>>
        %52 = "arith.index_cast"(%arg4) : (i32) -> index
        %53 = "arith.divsi"(%7, %arg3) : (i32, i32) -> i32
        %54 = "arith.muli"(%53, %53) : (i32, i32) -> i32
        %55 = "arith.index_cast"(%54) : (i32) -> index
        %56 = "arith.muli"(%55, %4) : (index, index) -> index
        %57 = "arith.divui"(%56, %4) : (index, index) -> index
        %58 = "memref.alloc"(%57) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?xf64>
        "scf.for"(%0, %57, %1) ({
        ^bb0(%arg5: index):
          "memref.store"(%3, %58, %arg5) <{nontemporal = false}> : (f64, memref<?xf64>, index) -> ()
          "scf.yield"() : () -> ()
        }) : (index, index, index) -> ()
        "memref.store"(%58, %51, %52) <{nontemporal = false}> : (memref<?xf64>, memref<?xmemref<?xf64>>, index) -> ()
        %59 = "arith.addi"(%arg4, %8) : (i32, i32) -> i32
        "scf.yield"(%59) : (i32) -> ()
      }) : (i32) -> (i32, i32)
      %49 = "arith.addi"(%arg2, %8) : (i32, i32) -> i32
      "scf.yield"(%49) : (i32) -> ()
    }) : (i32) -> i32
    %37 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
    %38 = "arith.index_cast"(%37) : (i32) -> index
    %39 = "memref.alloca"(%38) <{operandSegmentSizes = array<i32: 1, 0>}> : (index) -> memref<?x?xmemref<?xf64>>
    %40 = "arith.cmpi"(%38, %0) <{predicate = 4 : i64}> : (index, index) -> i1
    "scf.if"(%40) ({
      %47 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
      %48 = "arith.index_cast"(%47) : (i32) -> index
      %49 = "arith.cmpi"(%48, %0) <{predicate = 4 : i64}> : (index, index) -> i1
      "scf.for"(%0, %38, %1) ({
      ^bb0(%arg2: index):
        "scf.if"(%49) ({
          %50 = "affine.load"(%25) <{map = affine_map<() -> (0)>}> : (memref<1xmemref<?xmemref<?xmemref<?xf64>>>>) -> memref<?xmemref<?xmemref<?xf64>>>
          "scf.for"(%0, %48, %1) ({
          ^bb0(%arg3: index):
            %51 = "memref.load"(%50, %arg2) <{nontemporal = false}> : (memref<?xmemref<?xmemref<?xf64>>>, index) -> memref<?xmemref<?xf64>>
            %52 = "memref.load"(%51, %arg3) <{nontemporal = false}> : (memref<?xmemref<?xf64>>, index) -> memref<?xf64>
            "memref.store"(%52, %39, %arg2, %arg3) <{nontemporal = false}> : (memref<?xf64>, memref<?x?xmemref<?xf64>>, index, index) -> ()
            "scf.yield"() : () -> ()
          }) : (index, index, index) -> ()
          "scf.yield"() : () -> ()
        }, {
        }) : (i1) -> ()
        "scf.yield"() : () -> ()
      }) : (index, index, index) -> ()
      "scf.yield"() : () -> ()
    }, {
    }) : (i1) -> ()
    %41 = "memref.get_global"() <{name = @original_matrix}> : () -> memref<256xf64>
    %42 = "polygeist.memref2pointer"(%41) : (memref<256xf64>) -> !llvm.ptr
    %43 = "polygeist.memref2pointer"(%23) : (memref<256xf64>) -> !llvm.ptr
    "scf.for"(%0, %2, %1) ({
    ^bb0(%arg2: index):
      %47 = "arith.index_cast"(%arg2) : (index) -> i32
      %48 = "llvm.getelementptr"(%43, %47) <{elem_type = i8, rawConstantIndices = array<i32: -2147483648>}> : (!llvm.ptr, i32) -> !llvm.ptr
      %49 = "llvm.load"(%48) <{ordering = 0 : i64}> : (!llvm.ptr) -> i8
      %50 = "llvm.getelementptr"(%42, %47) <{elem_type = i8, rawConstantIndices = array<i32: -2147483648>}> : (!llvm.ptr, i32) -> !llvm.ptr
      "llvm.store"(%49, %50) <{ordering = 0 : i64}> : (i8, !llvm.ptr) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    %44 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
    %45 = "polygeist.subindex"(%39, %0) : (memref<?x?xmemref<?xf64>>, index) -> memref<?xmemref<?xf64>>
    "func.call"(%19, %44, %7, %24, %45) <{callee = @convert_to_blocks}> : (i32, i32, i32, memref<?xf64>, memref<?xmemref<?xf64>>) -> ()
    "func.call"(%19, %45, %16) <{callee = @cholesky_blocked}> : (i32, memref<?xmemref<?xf64>>, i32) -> ()
    %46 = "affine.load"(%11) <{map = affine_map<() -> (0)>}> : (memref<1xi32>) -> i32
    "func.call"(%19, %46, %7, %45, %24) <{callee = @convert_to_linear}> : (i32, i32, i32, memref<?xmemref<?xf64>>, memref<?xf64>) -> ()
    "func.return"(%5) : (i32) -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32) -> (), sym_name = "exit", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (memref<?xi8>) -> i32, sym_name = "atoi", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, i32, i32, memref<?x?xmemref<?xf64>>, memref<?x?xf64>) -> (), sym_name = "convert_to_linear", sym_visibility = "private"}> ({
  ^bb0(%arg0: i32, %arg1: i32, %arg2: i32, %arg3: memref<?x?xmemref<?xf64>>, %arg4: memref<?x?xf64>):
    %0 = "arith.constant"() <{value = 0 : index}> : () -> index
    %1 = "arith.constant"() <{value = 1 : index}> : () -> index
    %2 = "arith.index_cast"(%arg1) : (i32) -> index
    %3 = "arith.index_cast"(%arg0) : (i32) -> index
    "scf.for"(%0, %2, %1) ({
    ^bb0(%arg5: index):
      %4 = "arith.index_cast"(%arg5) : (index) -> i32
      %5 = "arith.muli"(%4, %arg0) : (i32, i32) -> i32
      %6 = "arith.index_cast"(%5) : (i32) -> index
      "scf.for"(%0, %2, %1) ({
      ^bb0(%arg6: index):
        %7 = "arith.index_cast"(%arg6) : (index) -> i32
        %8 = "memref.load"(%arg3, %arg5, %arg6) <{nontemporal = false}> : (memref<?x?xmemref<?xf64>>, index, index) -> memref<?xf64>
        %9 = "arith.muli"(%7, %arg0) : (i32, i32) -> i32
        %10 = "arith.index_cast"(%9) : (i32) -> index
        "scf.for"(%0, %3, %1) ({
        ^bb0(%arg7: index):
          %11 = "arith.index_cast"(%arg7) : (index) -> i32
          %12 = "arith.muli"(%11, %arg2) : (i32, i32) -> i32
          %13 = "arith.muli"(%11, %arg0) : (i32, i32) -> i32
          "scf.for"(%0, %3, %1) ({
          ^bb0(%arg8: index):
            %14 = "arith.index_cast"(%arg8) : (index) -> i32
            %15 = "arith.addi"(%12, %14) : (i32, i32) -> i32
            %16 = "arith.index_cast"(%15) : (i32) -> index
            %17 = "arith.addi"(%13, %14) : (i32, i32) -> i32
            %18 = "arith.index_cast"(%17) : (i32) -> index
            %19 = "memref.load"(%8, %18) <{nontemporal = false}> : (memref<?xf64>, index) -> f64
            %20 = "arith.addi"(%16, %10) : (index, index) -> index
            "memref.store"(%19, %arg4, %6, %20) <{nontemporal = false}> : (f64, memref<?x?xf64>, index, index) -> ()
            "scf.yield"() : () -> ()
          }) : (index, index, index) -> ()
          "scf.yield"() : () -> ()
        }) : (index, index, index) -> ()
        "scf.yield"() : () -> ()
      }) : (index, index, index) -> ()
      "scf.yield"() : () -> ()
    }) : (index, index, index) -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<internal>} : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} : () -> ()