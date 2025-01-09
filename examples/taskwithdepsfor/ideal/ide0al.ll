Emitting fn: computeA
computeA
CompoundStmt 0x563e00675260
|-DeclStmt 0x563e006748d0
| `-VarDecl 0x563e00674798  used i 'unsigned int' cinit
|   `-CStyleCastExpr 0x563e006748a8 'unsigned int' <IntegralCast>
|     `-ImplicitCastExpr 0x563e00674890 'uint64_t':'unsigned long' <LValueToRValue> part_of_explicit_cast
|       `-ArraySubscriptExpr 0x563e00674858 'uint64_t':'unsigned long' lvalue
|         |-ImplicitCastExpr 0x563e00674840 'uint64_t *' <LValueToRValue>
|         | `-DeclRefExpr 0x563e00674800 'uint64_t *' lvalue ParmVar 0x563e006734c8 'paramv' 'uint64_t *'
|         `-IntegerLiteral 0x563e00674820 'int' 0
|-DeclStmt 0x563e00674a28
| `-VarDecl 0x563e006748f8  used eventGuid 'artsGuid_t':'long' cinit
|   `-CStyleCastExpr 0x563e00674a00 'artsGuid_t':'long' <IntegralCast>
|     `-ImplicitCastExpr 0x563e006749e8 'uint64_t':'unsigned long' <LValueToRValue> part_of_explicit_cast
|       `-ArraySubscriptExpr 0x563e006749b8 'uint64_t':'unsigned long' lvalue
|         |-ImplicitCastExpr 0x563e006749a0 'uint64_t *' <LValueToRValue>
|         | `-DeclRefExpr 0x563e00674960 'uint64_t *' lvalue ParmVar 0x563e006734c8 'paramv' 'uint64_t *'
|         `-IntegerLiteral 0x563e00674980 'int' 1
|-DeclStmt 0x563e00674bc0
| `-VarDecl 0x563e00674a58  used A_i 'double *' cinit
|   `-CStyleCastExpr 0x563e00674b98 'double *' <BitCast>
|     `-ImplicitCastExpr 0x563e00674b80 'void *' <LValueToRValue> part_of_explicit_cast
|       `-MemberExpr 0x563e00674b38 'void *' lvalue .ptr 0x563e0065c388
|         `-ArraySubscriptExpr 0x563e00674b18 'artsEdtDep_t':'artsEdtDep_t' lvalue
|           |-ImplicitCastExpr 0x563e00674b00 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
|           | `-DeclRefExpr 0x563e00674ac0 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e00674610 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
|           `-IntegerLiteral 0x563e00674ae0 'int' 0
|-DeclStmt 0x563e00674d10
| `-VarDecl 0x563e00674be8  used A_i_guid 'artsGuid_t':'long' cinit
|   `-ImplicitCastExpr 0x563e00674cf8 'artsGuid_t':'long' <LValueToRValue>
|     `-MemberExpr 0x563e00674cc8 'artsGuid_t':'long' lvalue .guid 0x563e0065c260
|       `-ArraySubscriptExpr 0x563e00674ca8 'artsEdtDep_t':'artsEdtDep_t' lvalue
|         |-ImplicitCastExpr 0x563e00674c90 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
|         | `-DeclRefExpr 0x563e00674c50 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e00674610 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
|         `-IntegerLiteral 0x563e00674c70 'int' 0
|-BinaryOperator 0x563e00674e30 'double' '='
| |-ArraySubscriptExpr 0x563e00674d80 'double' lvalue
| | |-ImplicitCastExpr 0x563e00674d68 'double *' <LValueToRValue>
| | | `-DeclRefExpr 0x563e00674d28 'double *' lvalue Var 0x563e00674a58 'A_i' 'double *'
| | `-IntegerLiteral 0x563e00674d48 'int' 0
| `-BinaryOperator 0x563e00674e10 'double' '*'
|   |-ImplicitCastExpr 0x563e00674df8 'double' <IntegralToFloating>
|   | `-ImplicitCastExpr 0x563e00674de0 'unsigned int' <LValueToRValue>
|   |   `-DeclRefExpr 0x563e00674da0 'unsigned int' lvalue Var 0x563e00674798 'i' 'unsigned int'
|   `-FloatingLiteral 0x563e00674dc0 'double' 2.000000e+00
|-CallExpr 0x563e00675030 'int'
| |-ImplicitCastExpr 0x563e00675018 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00674e50 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| |-ImplicitCastExpr 0x563e00675088 'const char *' <NoOp>
| | `-ImplicitCastExpr 0x563e00675070 'char *' <ArrayToPointerDecay>
| |   `-StringLiteral 0x563e00674ea8 'char[86]' lvalue "------------------------\n--- Compute A[%u] = %f - Guid: %lu\n------------------------\n"
| |-ImplicitCastExpr 0x563e006750a0 'unsigned int' <LValueToRValue>
| | `-DeclRefExpr 0x563e00674f20 'unsigned int' lvalue Var 0x563e00674798 'i' 'unsigned int'
| |-ImplicitCastExpr 0x563e006750b8 'double' <LValueToRValue>
| | `-ArraySubscriptExpr 0x563e00674f98 'double' lvalue
| |   |-ImplicitCastExpr 0x563e00674f80 'double *' <LValueToRValue>
| |   | `-DeclRefExpr 0x563e00674f40 'double *' lvalue Var 0x563e00674a58 'A_i' 'double *'
| |   `-IntegerLiteral 0x563e00674f60 'int' 0
| `-ImplicitCastExpr 0x563e006750d0 'artsGuid_t':'long' <LValueToRValue>
|   `-DeclRefExpr 0x563e00674fb8 'artsGuid_t':'long' lvalue Var 0x563e00674be8 'A_i_guid' 'artsGuid_t':'long'
`-CallExpr 0x563e006751e0 'void'
  |-ImplicitCastExpr 0x563e006751c8 'void (*)(artsGuid_t, artsGuid_t, uint32_t)' <FunctionToPointerDecay>
  | `-DeclRefExpr 0x563e006750e8 'void (artsGuid_t, artsGuid_t, uint32_t)' Function 0x563e00668b78 'artsEventSatisfySlot' 'void (artsGuid_t, artsGuid_t, uint32_t)'
  |-ImplicitCastExpr 0x563e00675218 'artsGuid_t':'long' <LValueToRValue>
  | `-DeclRefExpr 0x563e00675108 'artsGuid_t':'long' lvalue Var 0x563e006748f8 'eventGuid' 'artsGuid_t':'long'
  |-ImplicitCastExpr 0x563e00675230 'artsGuid_t':'long' <LValueToRValue>
  | `-DeclRefExpr 0x563e00675128 'artsGuid_t':'long' lvalue Var 0x563e00674be8 'A_i_guid' 'artsGuid_t':'long'
  `-ImplicitCastExpr 0x563e00675248 'uint32_t':'unsigned int' <IntegralCast>
    `-DeclRefExpr 0x563e00675148 'int' EnumConstant 0x563e0065d1c0 'ARTS_EVENT_LATCH_DECR_SLOT' 'int'
----------------------
ANALYZING FUNCTION: computeA
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.unsigned int &
.unsigned int
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.double *
.double
.double *&
.double *
.double
.double *
.double
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.void **
.void *
.void *
.double *
.double
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: Emitting fn: computeB
computeB
CompoundStmt 0x563e00676648
|-DeclStmt 0x563e00675b68
| `-VarDecl 0x563e00675a30  used i 'unsigned int' cinit
|   `-CStyleCastExpr 0x563e00675b40 'unsigned int' <IntegralCast>
|     `-ImplicitCastExpr 0x563e00675b28 'uint64_t':'unsigned long' <LValueToRValue> part_of_explicit_cast
|       `-ArraySubscriptExpr 0x563e00675af0 'uint64_t':'unsigned long' lvalue
|         |-ImplicitCastExpr 0x563e00675ad8 'uint64_t *' <LValueToRValue>
|         | `-DeclRefExpr 0x563e00675a98 'uint64_t *' lvalue ParmVar 0x563e00675338 'paramv' 'uint64_t *'
|         `-IntegerLiteral 0x563e00675ab8 'int' 0
|-DeclStmt 0x563e00675d00
| `-VarDecl 0x563e00675b98  used A_i 'double *' cinit
|   `-CStyleCastExpr 0x563e00675cd8 'double *' <BitCast>
|     `-ImplicitCastExpr 0x563e00675cc0 'void *' <LValueToRValue> part_of_explicit_cast
|       `-MemberExpr 0x563e00675c78 'void *' lvalue .ptr 0x563e0065c388
|         `-ArraySubscriptExpr 0x563e00675c58 'artsEdtDep_t':'artsEdtDep_t' lvalue
|           |-ImplicitCastExpr 0x563e00675c40 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
|           | `-DeclRefExpr 0x563e00675c00 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e00675438 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
|           `-IntegerLiteral 0x563e00675c20 'int' 1
|-DeclStmt 0x563e00675db8
| `-VarDecl 0x563e00675d30  used A_im1_val 'double' cinit
|   `-FloatingLiteral 0x563e00675d98 'double' 0.000000e+00
|-IfStmt 0x563e006760e8
| |-BinaryOperator 0x563e00675e40 'int' '>'
| | |-ImplicitCastExpr 0x563e00675e10 'unsigned int' <LValueToRValue>
| | | `-DeclRefExpr 0x563e00675dd0 'unsigned int' lvalue Var 0x563e00675a30 'i' 'unsigned int'
| | `-ImplicitCastExpr 0x563e00675e28 'unsigned int' <IntegralCast>
| |   `-IntegerLiteral 0x563e00675df0 'int' 0
| `-CompoundStmt 0x563e006760c8
|   |-DeclStmt 0x563e00675fe0
|   | `-VarDecl 0x563e00675e78  used A_im1 'double *' cinit
|   |   `-CStyleCastExpr 0x563e00675fb8 'double *' <BitCast>
|   |     `-ImplicitCastExpr 0x563e00675fa0 'void *' <LValueToRValue> part_of_explicit_cast
|   |       `-MemberExpr 0x563e00675f58 'void *' lvalue .ptr 0x563e0065c388
|   |         `-ArraySubscriptExpr 0x563e00675f38 'artsEdtDep_t':'artsEdtDep_t' lvalue
|   |           |-ImplicitCastExpr 0x563e00675f20 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
|   |           | `-DeclRefExpr 0x563e00675ee0 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e00675438 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
|   |           `-IntegerLiteral 0x563e00675f00 'int' 2
|   `-BinaryOperator 0x563e006760a8 'double' '='
|     |-DeclRefExpr 0x563e00675ff8 'double' lvalue Var 0x563e00675d30 'A_im1_val' 'double'
|     `-ImplicitCastExpr 0x563e00676090 'double' <LValueToRValue>
|       `-ArraySubscriptExpr 0x563e00676070 'double' lvalue
|         |-ImplicitCastExpr 0x563e00676058 'double *' <LValueToRValue>
|         | `-DeclRefExpr 0x563e00676018 'double *' lvalue Var 0x563e00675e78 'A_im1' 'double *'
|         `-IntegerLiteral 0x563e00676038 'int' 0
|-DeclStmt 0x563e00676288
| `-VarDecl 0x563e00676120  used B_i 'double *' cinit
|   `-CStyleCastExpr 0x563e00676260 'double *' <BitCast>
|     `-ImplicitCastExpr 0x563e00676248 'void *' <LValueToRValue> part_of_explicit_cast
|       `-MemberExpr 0x563e00676200 'void *' lvalue .ptr 0x563e0065c388
|         `-ArraySubscriptExpr 0x563e006761e0 'artsEdtDep_t':'artsEdtDep_t' lvalue
|           |-ImplicitCastExpr 0x563e006761c8 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
|           | `-DeclRefExpr 0x563e00676188 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e00675438 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
|           `-IntegerLiteral 0x563e006761a8 'int' 0
|-BinaryOperator 0x563e00676400 'double' '='
| |-ArraySubscriptExpr 0x563e006762f8 'double' lvalue
| | |-ImplicitCastExpr 0x563e006762e0 'double *' <LValueToRValue>
| | | `-DeclRefExpr 0x563e006762a0 'double *' lvalue Var 0x563e00676120 'B_i' 'double *'
| | `-IntegerLiteral 0x563e006762c0 'int' 0
| `-BinaryOperator 0x563e006763e0 'double' '+'
|   |-ImplicitCastExpr 0x563e006763b0 'double' <LValueToRValue>
|   | `-ArraySubscriptExpr 0x563e00676370 'double' lvalue
|   |   |-ImplicitCastExpr 0x563e00676358 'double *' <LValueToRValue>
|   |   | `-DeclRefExpr 0x563e00676318 'double *' lvalue Var 0x563e00675b98 'A_i' 'double *'
|   |   `-IntegerLiteral 0x563e00676338 'int' 0
|   `-ImplicitCastExpr 0x563e006763c8 'double' <LValueToRValue>
|     `-DeclRefExpr 0x563e00676390 'double' lvalue Var 0x563e00675d30 'A_im1_val' 'double'
`-CallExpr 0x563e006765b0 'int'
  |-ImplicitCastExpr 0x563e00676598 'int (*)(const char *, ...)' <FunctionToPointerDecay>
  | `-DeclRefExpr 0x563e00676420 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
  |-ImplicitCastExpr 0x563e00676600 'const char *' <NoOp>
  | `-ImplicitCastExpr 0x563e006765e8 'char *' <ArrayToPointerDecay>
  |   `-StringLiteral 0x563e00676478 'char[74]' lvalue "------------------------\n--- Compute B[%u] = %f\n------------------------\n"
  |-ImplicitCastExpr 0x563e00676618 'unsigned int' <LValueToRValue>
  | `-DeclRefExpr 0x563e006764e8 'unsigned int' lvalue Var 0x563e00675a30 'i' 'unsigned int'
  `-ImplicitCastExpr 0x563e00676630 'double' <LValueToRValue>
    `-ArraySubscriptExpr 0x563e00676560 'double' lvalue
      |-ImplicitCastExpr 0x563e00676548 'double *' <LValueToRValue>
      | `-DeclRefExpr 0x563e00676508 'double *' lvalue Var 0x563e00676120 'B_i' 'double *'
      `-IntegerLiteral 0x563e00676528 'int' 0
guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.double *
.double
.int
.double
.unsigned int
.double
.double
.const char *
.const char
.unsigned int
.unsigned int
.double *
.double
.int
.double
.double
.double
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEventSatisfySlot
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: eventGuid type: i64
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: dataGuid type: i64
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: slot type: i32
.void
.void
Creating function: (i64, i64, i32) -> ()
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.void
----------------------
ANALYZING FUNCTION: computeB
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.unsigned int &
.unsigned int
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.unsigned int
.double *
.double
.double *&
.double *
.double
.double *
.double
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    FieldEmitting fn: printDataBlockA
printDataBlockA
: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.void **
.void *
.void *
.double *
.double
.double
.double &
.double
.double
.double
.unsigned int
.int
.unsigned int
.int
.double *
.double
.double *&
.double *
.double
.double *
.double
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.void **
.void *
.void *
.double *
.double
.double *
.double
.int
.double
.double
.double *
.double
.double *&
.double *
.double
.double *
.double
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.void **
.void *
.void *
.double *
.double
.double *
.double
.int
.double
.double *
.double
.int
.double
.double
.double
.const char *
.const char
.unsigned int
.unsigned int
.double *
.double
.int
.double
.double
.double
----------------------
ANALYZING FUNCTION: printDataBlockA
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: CompoundStmt 0x563e00676f50
`-ForStmt 0x563e00676f18
  |-DeclStmt 0x563e00676a40
  | `-VarDecl 0x563e006769a8  used i 'int' cinit
  |   `-IntegerLiteral 0x563e00676a10 'int' 0
  |-<<<NULL>>>
  |-BinaryOperator 0x563e00676b38 'int' '<'
  | |-ImplicitCastExpr 0x563e00676b20 'uint64_t':'unsigned long' <IntegralCast>
  | | `-ImplicitCastExpr 0x563e00676af0 'int' <LValueToRValue>
  | |   `-DeclRefExpr 0x563e00676a58 'int' lvalue Var 0x563e006769a8 'i' 'int'
  | `-ImplicitCastExpr 0x563e00676b08 'uint64_t':'unsigned long' <LValueToRValue>
  |   `-ArraySubscriptExpr 0x563e00676ad0 'uint64_t':'unsigned long' lvalue
  |     |-ImplicitCastExpr 0x563e00676ab8 'uint64_t *' <LValueToRValue>
  |     | `-DeclRefExpr 0x563e00676a78 'uint64_t *' lvalue ParmVar 0x563e00676720 'paramv' 'uint64_t *'
  |     `-IntegerLiteral 0x563e00676a98 'int' 0
  |-UnaryOperator 0x563e00676b78 'int' postfix '++'
  | `-DeclRefExpr 0x563e00676b58 'int' lvalue Var 0x563e006769a8 'i' 'int'
  `-CompoundStmt 0x563e00676ef8
    |-DeclStmt 0x563e00676d28
    | `-VarDecl 0x563e00676ba8  used data 'double *' cinit
    |   `-CStyleCastExpr 0x563e00676d00 'double *' <BitCast>
    |     `-ImplicitCastExpr 0x563e00676ce8 'void *' <LValueToRValue> part_of_explicit_cast
    |       `-MemberExpr 0x563e00676ca0 'void *' lvalue .ptr 0x563e0065c388
    |         `-ArraySubscriptExpr 0x563e00676c80 'artsEdtDep_t':'artsEdtDep_t' lvalue
    |           |-ImplicitCastExpr 0x563e00676c50 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
    |           | `-DeclRefExpr 0x563e00676c10 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e00676820 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
    |           `-ImplicitCastExpr 0x563e00676c68 'int' <LValueToRValue>
    |             `-DeclRefExpr 0x563e00676c30 'int' lvalue Var 0x563e006769a8 'i' 'int'
    `-CallExpr 0x563e00676e60 'int'
      |-ImplicitCastExpr 0x563e00676e48 'int (*)(const char *, ...)' <FunctionToPointerDecay>
      | `-DeclRefExpr 0x563e00676d40 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
      |-ImplicitCastExpr 0x563e00676eb0 'const char *' <NoOp>
      | `-ImplicitCastExpr 0x563e00676e98 'char *' <ArrayToPointerDecay>
      |   `-StringLiteral 0x563e00676d98 'char[11]' lvalue "A[%d]: %f\n"
      |-ImplicitCastExpr 0x563e00676ec8 'int' <LValueToRValue>
      | `-DeclRefExpr 0x563e00676dc0 'int' lvalue Var 0x563e006769a8 'i' 'int'
      `-ImplicitCastExpr 0x563e00676ee0 'double' <LValueToRValue>
        `-UnaryOperator 0x563e00676e18 'double' lvalue prefix '*' cannot overflow
          `-ImplicitCastExpr 0x563e00676e00 'double *' <LValueToRValue>
            `-DeclRefExpr 0x563e00676de0 'double *' lvalue Var 0x563e00676ba8 'data' 'double *'
Emitting fn: printDataBlockB
printDataBlockB
CompoundStmt 0x563e00677808
`-ForStmt 0x563e006777a8
  |-DeclStmt 0x563e00677308
  | `-VarDecl 0x563e00677280  used i 'int' cinit
  |   `-IntegerLiteral 0x563e006772e8 'int' 0
  |-<<<NULL>>>
  |-BinaryOperator 0x563e00677400 'int' '<'
  | |-ImplicitCastExpr 0x563e006773e8 'uint64_t':'unsigned long' <IntegralCast>
  | | `-ImplicitCastExpr 0x563e006773b8 'int' <LValueToRValue>
  | |   `-DeclRefExpr 0x563e00677320 'int' lvalue Var 0x563e00677280 'i' 'int'
  | `-ImplicitCastExpr 0x563e006773d0 'uint64_t':'unsigned long' <LValueToRValue>
  |   `-ArraySubscriptExpr 0x563e00677398 'uint64_t':'unsigned long' lvalue
  |     |-ImplicitCastExpr 0x563e00677380 'uint64_t *' <LValueToRValue>
  |     | `-DeclRefExpr 0x563e00677340 'uint64_t *' lvalue ParmVar 0x563e00676ff8 'paramv' 'uint64_t *'
  |     `-IntegerLiteral 0x563e00677360 'int' 0
  |-UnaryOperator 0x563e00677440 'int' postfix '++'
  | `-DeclRefExpr 0x563e00677420 'int' lvalue Var 0x563e00677280 'i' 'int'
  `-CompoundStmt 0x563e00677788
    |-DeclStmt 0x563e006775f0
    | `-VarDecl 0x563e00677470  used data 'double *' cinit
    |   `-CStyleCastExpr 0x563e006775c8 'double *' <BitCast>
    |     `-ImplicitCastExpr 0x563e006775b0 'void *' <LValueToRValue> part_of_explicit_cast
    |       `-MemberExpr 0x563e00677568 'void *' lvalue .ptr 0x563e0065c388
    |         `-ArraySubscriptExpr 0x563e00677548 'artsEdtDep_t':'artsEdtDep_t' lvalue
    |           |-ImplicitCastExpr 0x563e00677518 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
    |           | `-DeclRefExpr 0x563e006774d8 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e006770f8 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
    |           `-ImplicitCastExpr 0x563e00677530 'int' <LValueToRValue>
    |             `-DeclRefExpr 0x563e006774f8 'int' lvalue Var 0x563e00677280 'i' 'int'
    `-CallExpr 0x563e006776f0 'int'
      |-ImplicitCastExpr 0x563e006776d8 'int (*)(const char *, ...)' <FunctionToPointerDecay>
      | `-DeclRefExpr 0x563e00677608 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
      |-ImplicitCastExpr 0x563e00677740 'const char *' <NoOp>
      | `-ImplicitCastExpr 0x563e00677728 'char *' <ArrayToPointerDecay>
      |   `-StringLiteral 0x563e00677628 'char[11]' lvalue "B[%d]: %f\n"
      |-ImplicitCastExpr 0x563e00677758 'int' <LValueToRValue>
      | `-DeclRefExpr 0x563e00677650 'int' lvalue Var 0x563e00677280 'i' 'int'
      `-ImplicitCastExpr 0x563e00677770 'double' <LValueToRValue>
        `-UnaryOperator 0x563e006776a8 'double' lvalue prefix '*' cannot overflow
          `-ImplicitCastExpr 0x563e00677690 'double *' <LValueToRValue>
            `-DeclRefExpr 0x563e00677670 'double *' lvalue Var 0x563e00677470 'data' 'double *'
guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.int &
.int
.int
.int
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.double *
.double
.double *&
.double *
.double
.double *
.double
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.void **
.void *
.void *
.double *
.double
.const char *
.const char
.int
.int
.double *
.double
.double
.double
----------------------
ANALYZING FUNCTION: printDataBlockB
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.int &
.int
.int
.int
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.double *
.double
.double *&
.double *
.double
.double *
.double
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i6Emitting fn: parallelEdt
parallelEdt
CompoundStmt 0x563e0067ac98
|-CallExpr 0x563e00678450 'int'
| |-ImplicitCastExpr 0x563e00678438 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00678378 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| `-ImplicitCastExpr 0x563e00678490 'const char *' <NoOp>
|   `-ImplicitCastExpr 0x563e00678478 'char *' <ArrayToPointerDecay>
|     `-StringLiteral 0x563e006783d8 'char[44]' lvalue "---------- Parallel EDT started ----------\n"
|-DeclStmt 0x563e006785e0
| `-VarDecl 0x563e006784f0  used currentNode 'unsigned int' cinit
|   |-CallExpr 0x563e006785c0 'unsigned int'
|   | `-ImplicitCastExpr 0x563e006785a8 'unsigned int (*)()' <FunctionToPointerDecay>
|   |   `-DeclRefExpr 0x563e00678558 'unsigned int ()' Function 0x563e00672860 'artsGetCurrentNode' 'unsigned int ()'
|   `-FullComment 0x563e00681af0
|     `-ParagraphComment 0x563e00681ac0
|       `-TextComment 0x563e00681a90 Text=" Context"
|-DeclStmt 0x563e00678748
| `-VarDecl 0x563e00678608  used curentEpoch 'artsGuid_t':'long' cinit
|   `-CallExpr 0x563e00678700 'artsGuid_t':'long'
|     `-ImplicitCastExpr 0x563e006786e8 'artsGuid_t (*)()' <FunctionToPointerDecay>
|       `-DeclRefExpr 0x563e00678670 'artsGuid_t ()' Function 0x563e0066e098 'artsGetCurrentEpochGuid' 'artsGuid_t ()'
|-DeclStmt 0x563e006788b8
| `-VarDecl 0x563e00678778  used N 'int' cinit
|   |-ImplicitCastExpr 0x563e00678870 'int' <IntegralCast>
|   | `-ImplicitCastExpr 0x563e00678858 'uint64_t':'unsigned long' <LValueToRValue>
|   |   `-ArraySubscriptExpr 0x563e00678838 'uint64_t':'unsigned long' lvalue
|   |     |-ImplicitCastExpr 0x563e00678820 'uint64_t *' <LValueToRValue>
|   |     | `-DeclRefExpr 0x563e006787e0 'uint64_t *' lvalue ParmVar 0x563e006778b0 'paramv' 'uint64_t *'
|   |     `-IntegerLiteral 0x563e00678800 'int' 0
|   `-FullComment 0x563e00681bc0
|     `-ParagraphComment 0x563e00681b90
|       `-TextComment 0x563e00681b60 Text=" Parameters"
|-DeclStmt 0x563e00678be8
| |-VarDecl 0x563e00678a18  used A_array 'artsDataBlock[N]'
| | `-FullComment 0x563e00681c90
| |   `-ParagraphComment 0x563e00681c60
| |     `-TextComment 0x563e00681c30 Text=" Convert from depv to DataBlocks"
| `-VarDecl 0x563e00678b68  used B_array 'artsDataBlock[N]'
|   `-FullComment 0x563e00681d60
|     `-ParagraphComment 0x563e00681d30
|       `-TextComment 0x563e00681d00 Text=" Convert from depv to DataBlocks"
|-CallExpr 0x563e00678d10 'void'
| |-ImplicitCastExpr 0x563e00678cf8 'void (*)(artsDataBlock *, unsigned int, artsEdtDep_t *, unsigned int)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00678c00 'void (artsDataBlock *, unsigned int, artsEdtDep_t *, unsigned int)' Function 0x563e0066af98 'artsDbCreateArrayFromDeps' 'void (artsDataBlock *, unsigned int, artsEdtDep_t *, unsigned int)'
| |-ImplicitCastExpr 0x563e00678d50 'artsDataBlock *' <ArrayToPointerDecay>
| | `-DeclRefExpr 0x563e00678c20 'artsDataBlock[N]' lvalue Var 0x563e00678a18 'A_array' 'artsDataBlock[N]'
| |-ImplicitCastExpr 0x563e00678d80 'unsigned int' <IntegralCast>
| | `-ImplicitCastExpr 0x563e00678d68 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e00678c40 'int' lvalue Var 0x563e00678778 'N' 'int'
| |-ImplicitCastExpr 0x563e00678d98 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
| | `-DeclRefExpr 0x563e00678c60 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e006779b0 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
| `-ImplicitCastExpr 0x563e00678db0 'unsigned int' <IntegralCast>
|   `-IntegerLiteral 0x563e00678c80 'int' 0
|-CallExpr 0x563e00678e80 'void'
| |-ImplicitCastExpr 0x563e00678e68 'void (*)(artsDataBlock *, unsigned int, artsEdtDep_t *, unsigned int)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00678dc8 'void (artsDataBlock *, unsigned int, artsEdtDep_t *, unsigned int)' Function 0x563e0066af98 'artsDbCreateArrayFromDeps' 'void (artsDataBlock *, unsigned int, artsEdtDep_t *, unsigned int)'
| |-ImplicitCastExpr 0x563e00678ec0 'artsDataBlock *' <ArrayToPointerDecay>
| | `-DeclRefExpr 0x563e00678de8 'artsDataBlock[N]' lvalue Var 0x563e00678b68 'B_array' 'artsDataBlock[N]'
| |-ImplicitCastExpr 0x563e00678ef0 'unsigned int' <IntegralCast>
| | `-ImplicitCastExpr 0x563e00678ed8 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e00678e08 'int' lvalue Var 0x563e00678778 'N' 'int'
| |-ImplicitCastExpr 0x563e00678f08 'artsEdtDep_t *':'artsEdtDep_t *' <LValueToRValue>
| | `-DeclRefExpr 0x563e00678e28 'artsEdtDep_t *':'artsEdtDep_t *' lvalue ParmVar 0x563e006779b0 'depv' 'artsEdtDep_t *':'artsEdtDep_t *'
| `-ImplicitCastExpr 0x563e00678f38 'unsigned int' <IntegralCast>
|   `-ImplicitCastExpr 0x563e00678f20 'int' <LValueToRValue>
|     `-DeclRefExpr 0x563e00678e48 'int' lvalue Var 0x563e00678778 'N' 'int'
|-DeclStmt 0x563e00679168
| `-VarDecl 0x563e00678f68  used A_event 'artsGuid_t *' cinit
|   `-CStyleCastExpr 0x563e00679140 'artsGuid_t *' <BitCast>
|     `-CallExpr 0x563e00679100 'void *'
|       |-ImplicitCastExpr 0x563e006790e8 'void *(*)(size_t)' <FunctionToPointerDecay>
|       | `-DeclRefExpr 0x563e00678fd0 'void *(size_t)' Function 0x563e006615c8 'artsMalloc' 'void *(size_t)'
|       `-BinaryOperator 0x563e00679070 'unsigned long' '*'
|         |-ImplicitCastExpr 0x563e00679058 'unsigned long' <IntegralCast>
|         | `-ImplicitCastExpr 0x563e00679040 'int' <LValueToRValue>
|         |   `-DeclRefExpr 0x563e00678ff0 'int' lvalue Var 0x563e00678778 'N' 'int'
|         `-UnaryExprOrTypeTraitExpr 0x563e00679020 'unsigned long' sizeof 'artsGuid_t':'long'
|-ForStmt 0x563e00679510
| |-DeclStmt 0x563e00679220
| | `-VarDecl 0x563e00679198  used i 'int' cinit
| |   `-IntegerLiteral 0x563e00679200 'int' 0
| |-<<<NULL>>>
| |-BinaryOperator 0x563e006792d0 'int' '<'
| | |-ImplicitCastExpr 0x563e006792a0 'int' <LValueToRValue>
| | | `-DeclRefExpr 0x563e00679238 'int' lvalue Var 0x563e00679198 'i' 'int'
| | `-ImplicitCastExpr 0x563e006792b8 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e00679280 'int' lvalue Var 0x563e00678778 'N' 'int'
| |-UnaryOperator 0x563e00679310 'int' postfix '++'
| | `-DeclRefExpr 0x563e006792f0 'int' lvalue Var 0x563e00679198 'i' 'int'
| `-BinaryOperator 0x563e006794f0 'artsGuid_t':'long' '='
|   |-ArraySubscriptExpr 0x563e00679398 'artsGuid_t':'long' lvalue
|   | |-ImplicitCastExpr 0x563e00679368 'artsGuid_t *' <LValueToRValue>
|   | | `-DeclRefExpr 0x563e00679328 'artsGuid_t *' lvalue Var 0x563e00678f68 'A_event' 'artsGuid_t *'
|   | `-ImplicitCastExpr 0x563e00679380 'int' <LValueToRValue>
|   |   `-DeclRefExpr 0x563e00679348 'int' lvalue Var 0x563e00679198 'i' 'int'
|   `-CallExpr 0x563e00679490 'artsGuid_t':'long'
|     |-ImplicitCastExpr 0x563e00679478 'artsGuid_t (*)(unsigned int, unsigned int)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e006793b8 'artsGuid_t (unsigned int, unsigned int)' Function 0x563e00668388 'artsEventCreate' 'artsGuid_t (unsigned int, unsigned int)'
|     |-ImplicitCastExpr 0x563e006794c0 'unsigned int' <LValueToRValue>
|     | `-DeclRefExpr 0x563e006793d8 'unsigned int' lvalue Var 0x563e006784f0 'currentNode' 'unsigned int'
|     `-ImplicitCastExpr 0x563e006794d8 'unsigned int' <IntegralCast>
|       `-IntegerLiteral 0x563e006793f8 'int' 1
`-ForStmt 0x563e0067ac60
  |-DeclStmt 0x563e006795e8
  | `-VarDecl 0x563e00679560  used i 'int' cinit
  |   `-IntegerLiteral 0x563e006795c8 'int' 0
  |-<<<NULL>>>
  |-BinaryOperator 0x563e00679670 'int' '<'
  | |-ImplicitCastExpr 0x563e00679640 'int' <LValueToRValue>
  | | `-DeclRefExpr 0x563e00679600 'int' lvalue Var 0x563e00679560 'i' 'int'
  | `-ImplicitCastExpr 0x563e00679658 'int' <LValueToRValue>
  |   `-DeclRefExpr 0x563e00679620 'int' lvalue Var 0x563e00678778 'N' 'int'
  |-UnaryOperator 0x563e006796b0 'int' postfix '++'
  | `-DeclRefExpr 0x563e00679690 'int' lvalue Var 0x563e00679560 'i' 'int'
  `-CompoundStmt 0x563e0067ac08
    |-DeclStmt 0x563e00679a38
    | `-VarDecl 0x563e00679748  used Aparams 'uint64_t[2]' cinit
    |   `-InitListExpr 0x563e00679960 'uint64_t[2]'
    |     |-CStyleCastExpr 0x563e006797f8 'uint64_t':'unsigned long' <IntegralCast>
    |     | `-ImplicitCastExpr 0x563e006797e0 'int' <LValueToRValue> part_of_explicit_cast
    |     |   `-DeclRefExpr 0x563e006797b0 'int' lvalue Var 0x563e00679560 'i' 'int'
    |     `-CStyleCastExpr 0x563e006798d8 'uint64_t':'unsigned long' <IntegralCast>
    |       `-ImplicitCastExpr 0x563e006798c0 'artsGuid_t':'long' <LValueToRValue> part_of_explicit_cast
    |         `-ArraySubscriptExpr 0x563e00679890 'artsGuid_t':'long' lvalue
    |           |-ImplicitCastExpr 0x563e00679860 'artsGuid_t *' <LValueToRValue>
    |           | `-DeclRefExpr 0x563e00679820 'artsGuid_t *' lvalue Var 0x563e00678f68 'A_event' 'artsGuid_t *'
    |           `-ImplicitCastExpr 0x563e00679878 'int' <LValueToRValue>
    |             `-DeclRefExpr 0x563e00679840 'int' lvalue Var 0x563e00679560 'i' 'int'
    |-DeclStmt 0x563e00679d68
    | `-VarDecl 0x563e00679a60  used A_edt 'artsGuid_t':'long' cinit
    |   `-CallExpr 0x563e00679ca0 'artsGuid_t':'long'
    |     |-ImplicitCastExpr 0x563e00679c88 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
    |     | `-DeclRefExpr 0x563e00679ac8 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' Function 0x563e00664938 'artsEdtCreateWithEpoch' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)'
    |     |-CStyleCastExpr 0x563e00679b60 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
    |     | `-ImplicitCastExpr 0x563e00679b48 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
    |     |   `-DeclRefExpr 0x563e00679ae8 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e006746b8 'computeA' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
    |     |-ImplicitCastExpr 0x563e00679cf0 'unsigned int' <LValueToRValue>
    |     | `-DeclRefExpr 0x563e00679b88 'unsigned int' lvalue Var 0x563e006784f0 'currentNode' 'unsigned int'
    |     |-ImplicitCastExpr 0x563e00679d08 'uint32_t':'unsigned int' <IntegralCast>
    |     | `-IntegerLiteral 0x563e00679ba8 'int' 2
    |     |-ImplicitCastExpr 0x563e00679d20 'uint64_t *' <ArrayToPointerDecay>
    |     | `-DeclRefExpr 0x563e00679bc8 'uint64_t[2]' lvalue Var 0x563e00679748 'Aparams' 'uint64_t[2]'
    |     |-ImplicitCastExpr 0x563e00679d38 'uint32_t':'unsigned int' <IntegralCast>
    |     | `-IntegerLiteral 0x563e00679be8 'int' 1
    |     `-ImplicitCastExpr 0x563e00679d50 'artsGuid_t':'long' <LValueToRValue>
    |       `-DeclRefExpr 0x563e00679c08 'artsGuid_t':'long' lvalue Var 0x563e00678608 'curentEpoch' 'artsGuid_t':'long'
    |-DeclStmt 0x563e00679f98
    | `-VarDecl 0x563e00679da0  used Bparams 'uint64_t[1]' cinit
    |   `-InitListExpr 0x563e00679ed0 'uint64_t[1]'
    |     `-CStyleCastExpr 0x563e00679e50 'uint64_t':'unsigned long' <IntegralCast>
    |       `-ImplicitCastExpr 0x563e00679e38 'int' <LValueToRValue> part_of_explicit_cast
    |         `-DeclRefExpr 0x563e00679e08 'int' lvalue Var 0x563e00679560 'i' 'int'
    |-DeclStmt 0x563e0067a138
    | `-VarDecl 0x563e00679fc8  used depCount 'int' cinit
    |   `-ConditionalOperator 0x563e0067a108 'int'
    |     |-ParenExpr 0x563e0067a0a8 'int'
    |     | `-BinaryOperator 0x563e0067a088 'int' '>'
    |     |   |-ImplicitCastExpr 0x563e0067a070 'int' <LValueToRValue>
    |     |   | `-DeclRefExpr 0x563e0067a030 'int' lvalue Var 0x563e00679560 'i' 'int'
    |     |   `-IntegerLiteral 0x563e0067a050 'int' 0
    |     |-IntegerLiteral 0x563e0067a0c8 'int' 3
    |     `-IntegerLiteral 0x563e0067a0e8 'int' 2
    |-DeclStmt 0x563e0067a408
    | `-VarDecl 0x563e0067a160  used B_edt 'artsGuid_t':'long' cinit
    |   `-CallExpr 0x563e0067a328 'artsGuid_t':'long'
    |     |-ImplicitCastExpr 0x563e0067a310 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
    |     | `-DeclRefExpr 0x563e0067a1c8 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' Function 0x563e00664938 'artsEdtCreateWithEpoch' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)'
    |     |-CStyleCastExpr 0x563e0067a230 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
    |     | `-ImplicitCastExpr 0x563e0067a218 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
    |     |   `-DeclRefExpr 0x563e0067a1e8 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e006754e0 'computeB' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
    |     |-ImplicitCastExpr 0x563e0067a378 'unsigned int' <LValueToRValue>
    |     | `-DeclRefExpr 0x563e0067a258 'unsigned int' lvalue Var 0x563e006784f0 'currentNode' 'unsigned int'
    |     |-ImplicitCastExpr 0x563e0067a390 'uint32_t':'unsigned int' <IntegralCast>
    |     | `-IntegerLiteral 0x563e0067a290 'int' 2
    |     |-ImplicitCastExpr 0x563e0067a3a8 'uint64_t *' <ArrayToPointerDecay>
    |     | `-DeclRefExpr 0x563e0067a2b0 'uint64_t[1]' lvalue Var 0x563e00679da0 'Bparams' 'uint64_t[1]'
    |     |-ImplicitCastExpr 0x563e0067a3d8 'uint32_t':'unsigned int' <IntegralCast>
    |     | `-ImplicitCastExpr 0x563e0067a3c0 'int' <LValueToRValue>
    |     |   `-DeclRefExpr 0x563e0067a2d0 'int' lvalue Var 0x563e00679fc8 'depCount' 'int'
    |     `-ImplicitCastExpr 0x563e0067a3f0 'artsGuid_t':'long' <LValueToRValue>
    |       `-DeclRefExpr 0x563e0067a2f0 'artsGuid_t':'long' lvalue Var 0x563e00678608 'curentEpoch' 'artsGuid_t':'long'
    |-CallExpr 0x563e0067a528 'void'
    | |-ImplicitCastExpr 0x563e0067a510 'void (*)(artsGuid_t, artsGuid_t, uint32_t)' <FunctionToPointerDecay>
    | | `-DeclRefExpr 0x563e0067a420 'void (artsGuid_t, artsGuid_t, uint32_t)' Function 0x563e00668dd8 'artsAddDependence' 'void (artsGuid_t, artsGuid_t, uint32_t)'
    | |-ImplicitCastExpr 0x563e0067a560 'artsGuid_t':'long' <LValueToRValue>
    | | `-ArraySubscriptExpr 0x563e0067a4b0 'artsGuid_t':'long' lvalue
    | |   |-ImplicitCastExpr 0x563e0067a480 'artsGuid_t *' <LValueToRValue>
    | |   | `-DeclRefExpr 0x563e0067a440 'artsGuid_t *' lvalue Var 0x563e00678f68 'A_event' 'artsGuid_t *'
    | |   `-ImplicitCastExpr 0x563e0067a498 'int' <LValueToRValue>
    | |     `-DeclRefExpr 0x563e0067a460 'int' lvalue Var 0x563e00679560 'i' 'int'
    | |-ImplicitCastExpr 0x563e0067a578 'artsGuid_t':'long' <LValueToRValue>
    | | `-DeclRefExpr 0x563e0067a4d0 'artsGuid_t':'long' lvalue Var 0x563e0067a160 'B_edt' 'artsGuid_t':'long'
    | `-ImplicitCastExpr 0x563e0067a590 'uint32_t':'unsigned int' <IntegralCast>
    |   `-IntegerLiteral 0x563e0067a4f0 'int' 1
    |-IfStmt 0x563e0067a818
    | |-BinaryOperator 0x563e0067a600 'int' '>'
    | | |-ImplicitCastExpr 0x563e0067a5e8 'int' <LValueToRValue>
    | | | `-DeclRefExpr 0x563e0067a5a8 'int' lvalue Var 0x563e00679560 'i' 'int'
    | | `-IntegerLiteral 0x563e0067a5c8 'int' 0
    | `-CallExpr 0x563e0067a768 'void'
    |   |-ImplicitCastExpr 0x563e0067a750 'void (*)(artsGuid_t, artsGuid_t, uint32_t)' <FunctionToPointerDecay>
    |   | `-DeclRefExpr 0x563e0067a620 'void (artsGuid_t, artsGuid_t, uint32_t)' Function 0x563e00668dd8 'artsAddDependence' 'void (artsGuid_t, artsGuid_t, uint32_t)'
    |   |-ImplicitCastExpr 0x563e0067a7a0 'artsGuid_t':'long' <LValueToRValue>
    |   | `-ArraySubscriptExpr 0x563e0067a6f0 'artsGuid_t':'long' lvalue
    |   |   |-ImplicitCastExpr 0x563e0067a6d8 'artsGuid_t *' <LValueToRValue>
    |   |   | `-DeclRefExpr 0x563e0067a640 'artsGuid_t *' lvalue Var 0x563e00678f68 'A_event' 'artsGuid_t *'
    |   |   `-BinaryOperator 0x563e0067a6b8 'int' '-'
    |   |     |-ImplicitCastExpr 0x563e0067a6a0 'int' <LValueToRValue>
    |   |     | `-DeclRefExpr 0x563e0067a660 'int' lvalue Var 0x563e00679560 'i' 'int'
    |   |     `-IntegerLiteral 0x563e0067a680 'int' 1
    |   |-ImplicitCastExpr 0x563e0067a7b8 'artsGuid_t':'long' <LValueToRValue>
    |   | `-DeclRefExpr 0x563e0067a710 'artsGuid_t':'long' lvalue Var 0x563e0067a160 'B_edt' 'artsGuid_t':'long'
    |   `-ImplicitCastExpr 0x563e0067a7d0 'uint32_t':'unsigned int' <IntegralCast>
    |     `-IntegerLiteral 0x563e0067a730 'int' 2
    |-CallExpr 0x563e0067a9d0 'void'
    | |-ImplicitCastExpr 0x563e0067a9b8 'void (*)(artsGuid_t, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
    | | `-DeclRefExpr 0x563e0067a838 'void (artsGuid_t, uint32_t, artsGuid_t)' Function 0x563e006661d8 'artsSignalEdt' 'void (artsGuid_t, uint32_t, artsGuid_t)'
    | |-ImplicitCastExpr 0x563e0067aa08 'artsGuid_t':'long' <LValueToRValue>
    | | `-DeclRefExpr 0x563e0067a858 'artsGuid_t':'long' lvalue Var 0x563e00679a60 'A_edt' 'artsGuid_t':'long'
    | |-ImplicitCastExpr 0x563e0067aa20 'uint32_t':'unsigned int' <IntegralCast>
    | | `-IntegerLiteral 0x563e0067a878 'int' 0
    | `-ImplicitCastExpr 0x563e0067aa38 'artsGuid_t':'long' <LValueToRValue>
    |   `-MemberExpr 0x563e0067a928 'artsGuid_t':'long' lvalue .guid 0x563e0065c560
    |     `-ArraySubscriptExpr 0x563e0067a908 'artsDataBlock':'artsDataBlock' lvalue
    |       |-ImplicitCastExpr 0x563e0067a8d8 'artsDataBlock *' <ArrayToPointerDecay>
    |       | `-DeclRefExpr 0x563e0067a898 'artsDataBlock[N]' lvalue Var 0x563e00678a18 'A_array' 'artsDataBlock[N]'
    |       `-ImplicitCastExpr 0x563e0067a8f0 'int' <LValueToRValue>
    |         `-DeclRefExpr 0x563e0067a8b8 'int' lvalue Var 0x563e00679560 'i' 'int'
    `-CallExpr 0x563e0067ab88 'void'
      |-ImplicitCastExpr 0x563e0067ab70 'void (*)(artsGuid_t, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
      | `-DeclRefExpr 0x563e0067aa50 'void (artsGuid_t, uint32_t, artsGuid_t)' Function 0x563e006661d8 'artsSignalEdt' 'void (artsGuid_t, uint32_t, artsGuid_t)'
      |-ImplicitCastExpr 0x563e0067abc0 'artsGuid_t':'long' <LValueToRValue>
      | `-DeclRefExpr 0x563e0067aa70 'artsGuid_t':'long' lvalue Var 0x563e0067a160 'B_edt' 'artsGuid_t':'long'
      |-ImplicitCastExpr 0x563e0067abd8 'uint32_t':'unsigned int' <IntegralCast>
      | `-IntegerLiteral 0x563e0067aa90 'int' 0
      `-ImplicitCastExpr 0x563e0067abf0 'artsGuid_t':'long' <LValueToRValue>
        `-MemberExpr 0x563e0067ab40 'artsGuid_t':'long' lvalue .guid 0x563e0065c560
          `-ArraySubscriptExpr 0x563e0067ab20 'artsDataBlock':'artsDataBlock' lvalue
            |-ImplicitCastExpr 0x563e0067aaf0 'artsDataBlock *' <ArrayToPointerDecay>
            | `-DeclRefExpr 0x563e0067aab0 'artsDataBlock[N]' lvalue Var 0x563e00678b68 'B_array' 'artsDataBlock[N]'
            `-ImplicitCastExpr 0x563e0067ab08 'int' <LValueToRValue>
              `-DeclRefExpr 0x563e0067aad0 'int' lvalue Var 0x563e00679560 'i' 'int'
4, i32, memref<?xi8>)>
.int
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.void **
.void *
.void *
.double *
.double
.const char *
.const char
.int
.int
.double *
.double
.double
.double
----------------------
ANALYZING FUNCTION: parallelEdt
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.const char *
.const char
.unsigned int
.unsigned int &
.unsigned int
.unsigned int
----------------------
ANALYZING FUNCTION: artsGetCurrentNode
.unsigned int
.unsigned int
Creating function: () -> i32
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsGetCurrentEpochGuid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
Creating function: () -> i64
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.int &
.int
.int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.int
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.int
----------------------
ANALYZING FUNCTION: artsDbCreateArrayFromDeps
Getting MLIR Type
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>t
 - Parameter: dbArray type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: numElements type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: deps type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: initialSlot type: i32
.void
.void
Creating function: (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
.int
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.unsigned int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.void
----------------------
ANALYZING FUNCTION: artsDbCreateArrayFromDeps
.int
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.int
.unsigned int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.void
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t *&
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsMalloc
Getting MLIR Type
.size_t
.size_t
.unsigned long
- MLIR Type: i64t
 - Parameter: size type: i64
.void *
.void *
Creating function: (i64) -> memref<?xi8>
.int
.unsigned long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned long
.unsigned long
.void *
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.int &
.int
.int
.int
.int
.int
.int
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEventCreate
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: route type: i32
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: latchCount type: i32
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
Creating function: (i32, i32) -> i64
.unsigned int
.int
.unsigned int
.unsigned int
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.int &
.int
.int
.int
.int
.int
.int
.uint64_t[2]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[2]
.uint64_t[2]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t[2]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[2]
.uint64_t[2]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreateWithEpoch
Getting MLIR Type
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>t
 - Parameter: funcPtr type: memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: route type: i32
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: epochGuid type: i64
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
Creating function: (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
----------------------
ANALYZING FUNCTION: computeA
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[1]
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[1]
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.int &
.int
.int
.int
.int
.int
.int
.int
.int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreateWithEpoch
----------------------
ANALYZING FUNCTION: computeB
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsAddDependence
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: source type: i64
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: destination type: i64
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: slot type: i32
.void
.void
CreatEmitting fn: parallelDoneEdt
parallelDoneEdt
ing function: (i64, i64, i32) -> ()
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.void
.int
.int
.int
----------------------
ANALYZING FUNCTION: artsAddDependence
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.void
----------------------
ANALYZING FUNCTION: artsSignalEdt
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: edtGuid type: i64
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: slot type: i32
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: dataGuid type: i64
.void
.void
Creating function: (i64, i32, i64) -> ()
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.void
----------------------
ANALYZING FUNCTION: artsSignalEdt
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t *
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.void
----------------------
ANALYZING FUNCTION: parallelDoneEdt
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_tCompoundStmt 0x563e0067b158
`-CallExpr 0x563e0067b100 'int'
  |-ImplicitCastExpr 0x563e0067b0e8 'int (*)(const char *, ...)' <FunctionToPointerDecay>
  | `-DeclRefExpr 0x563e0067b028 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
  `-ImplicitCastExpr 0x563e0067b140 'const char *' <NoOp>
    `-ImplicitCastExpr 0x563e0067b128 'char *' <ArrayToPointerDecay>
      `-StringLiteral 0x563e0067b088 'char[45]' lvalue "---------- Parallel EDT finished ----------\n"
Emitting fn: computeEDT
computeEDT
CompoundStmt 0x563e0067dba0
|-CallExpr 0x563e0067bdc8 'int'
| |-ImplicitCastExpr 0x563e0067bdb0 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067bcf8 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| `-ImplicitCastExpr 0x563e0067be08 'const char *' <NoOp>
|   `-ImplicitCastExpr 0x563e0067bdf0 'char *' <ArrayToPointerDecay>
|     `-StringLiteral 0x563e0067bd58 'char[35]' lvalue "---------- Compute EDT ----------\n"
|-DeclStmt 0x563e0067bf20
| `-VarDecl 0x563e0067be60  used currentNode 'unsigned int' cinit
|   |-CallExpr 0x563e0067bf00 'unsigned int'
|   | `-ImplicitCastExpr 0x563e0067bee8 'unsigned int (*)()' <FunctionToPointerDecay>
|   |   `-DeclRefExpr 0x563e0067bec8 'unsigned int ()' Function 0x563e00672860 'artsGetCurrentNode' 'unsigned int ()'
|   `-FullComment 0x563e00682070
|     `-ParagraphComment 0x563e00682040
|       `-TextComment 0x563e00682010 Text=" Context"
|-DeclStmt 0x563e0067c038
| `-VarDecl 0x563e0067bf48  used curentEpoch 'artsGuid_t':'long' cinit
|   `-CallExpr 0x563e0067bfe8 'artsGuid_t':'long'
|     `-ImplicitCastExpr 0x563e0067bfd0 'artsGuid_t (*)()' <FunctionToPointerDecay>
|       `-DeclRefExpr 0x563e0067bfb0 'artsGuid_t ()' Function 0x563e0066e098 'artsGetCurrentEpochGuid' 'artsGuid_t ()'
|-DeclStmt 0x563e0067c358
| `-VarDecl 0x563e0067c060  used parallelDoneEdtGuid 'artsGuid_t':'long' cinit
|   |-CallExpr 0x563e0067c2b0 'artsGuid_t':'long'
|   | |-ImplicitCastExpr 0x563e0067c298 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)' <FunctionToPointerDecay>
|   | | `-DeclRefExpr 0x563e0067c0c8 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)' Function 0x563e00663bd8 'artsEdtCreate' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)'
|   | |-CStyleCastExpr 0x563e0067c130 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
|   | | `-ImplicitCastExpr 0x563e0067c118 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
|   | |   `-DeclRefExpr 0x563e0067c0e8 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e0067af60 'parallelDoneEdt' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
|   | |-ImplicitCastExpr 0x563e0067c2f8 'unsigned int' <LValueToRValue>
|   | | `-DeclRefExpr 0x563e0067c158 'unsigned int' lvalue Var 0x563e0067be60 'currentNode' 'unsigned int'
|   | |-ImplicitCastExpr 0x563e0067c310 'uint32_t':'unsigned int' <IntegralCast>
|   | | `-IntegerLiteral 0x563e0067c178 'int' 0
|   | |-ImplicitCastExpr 0x563e0067c328 'uint64_t *' <NullToPointer>
|   | | `-ParenExpr 0x563e0067c1f8 'void *'
|   | |   `-CStyleCastExpr 0x563e0067c1d0 'void *' <NullToPointer>
|   | |     `-IntegerLiteral 0x563e0067c198 'int' 0
|   | `-ImplicitCastExpr 0x563e0067c340 'uint32_t':'unsigned int' <IntegralCast>
|   |   `-IntegerLiteral 0x563e0067c218 'int' 1
|   `-FullComment 0x563e00682140
|     `-ParagraphComment 0x563e00682110
|       `-TextComment 0x563e006820e0 Text=" Create parallel EDT"
|-DeclStmt 0x563e0067c548
| `-VarDecl 0x563e0067c380  used parallelDoneEpochGuid 'artsGuid_t':'long' cinit
|   `-CallExpr 0x563e0067c4c0 'artsGuid_t':'long'
|     |-ImplicitCastExpr 0x563e0067c4a8 'artsGuid_t (*)(artsGuid_t, unsigned int)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e0067c3e8 'artsGuid_t (artsGuid_t, unsigned int)' Function 0x563e0066e4b8 'artsInitializeAndStartEpoch' 'artsGuid_t (artsGuid_t, unsigned int)'
|     |-ImplicitCastExpr 0x563e0067c4f0 'artsGuid_t':'long' <LValueToRValue>
|     | `-DeclRefExpr 0x563e0067c408 'artsGuid_t':'long' lvalue Var 0x563e0067c060 'parallelDoneEdtGuid' 'artsGuid_t':'long'
|     `-ImplicitCastExpr 0x563e0067c508 'unsigned int' <IntegralCast>
|       `-IntegerLiteral 0x563e0067c428 'int' 0
|-DeclStmt 0x563e0067c738
| `-VarDecl 0x563e0067c5c0  used parallelParams 'uint64_t[1]' cinit
|   |-InitListExpr 0x563e0067c6f0 'uint64_t[1]'
|   | `-CStyleCastExpr 0x563e0067c670 'uint64_t':'unsigned long' <IntegralCast>
|   |   `-ImplicitCastExpr 0x563e0067c658 'int' <LValueToRValue> part_of_explicit_cast
|   |     `-DeclRefExpr 0x563e0067c628 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|   `-FullComment 0x563e00682210
|     `-ParagraphComment 0x563e006821e0
|       `-TextComment 0x563e006821b0 Text=" As of now, let's create only one instance of parallelEdt"
|-DeclStmt 0x563e0067c858
| `-VarDecl 0x563e0067c760  used parallelDeps 'uint64_t':'unsigned long' cinit
|   `-ImplicitCastExpr 0x563e0067c840 'uint64_t':'unsigned long' <IntegralCast>
|     `-BinaryOperator 0x563e0067c820 'int' '*'
|       |-IntegerLiteral 0x563e0067c7c8 'int' 2
|       `-ImplicitCastExpr 0x563e0067c808 'int' <LValueToRValue>
|         `-DeclRefExpr 0x563e0067c7e8 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|-DeclStmt 0x563e0067cb48
| `-VarDecl 0x563e0067c880  used parallelEdtGuid 'artsGuid_t':'long' cinit
|   `-CallExpr 0x563e0067ca30 'artsGuid_t':'long'
|     |-ImplicitCastExpr 0x563e0067ca18 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e0067c8e8 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' Function 0x563e00664938 'artsEdtCreateWithEpoch' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)'
|     |-CStyleCastExpr 0x563e0067c950 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
|     | `-ImplicitCastExpr 0x563e0067c938 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
|     |   `-DeclRefExpr 0x563e0067c908 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e006782b0 'parallelEdt' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
|     |-ImplicitCastExpr 0x563e0067ca80 'unsigned int' <LValueToRValue>
|     | `-DeclRefExpr 0x563e0067c978 'unsigned int' lvalue Var 0x563e0067be60 'currentNode' 'unsigned int'
|     |-ImplicitCastExpr 0x563e0067ca98 'uint32_t':'unsigned int' <IntegralCast>
|     | `-IntegerLiteral 0x563e0067c998 'int' 1
|     |-ImplicitCastExpr 0x563e0067cab0 'uint64_t *' <ArrayToPointerDecay>
|     | `-DeclRefExpr 0x563e0067c9b8 'uint64_t[1]' lvalue Var 0x563e0067c5c0 'parallelParams' 'uint64_t[1]'
|     |-ImplicitCastExpr 0x563e0067cae0 'uint32_t':'unsigned int' <IntegralCast>
|     | `-ImplicitCastExpr 0x563e0067cac8 'uint64_t':'unsigned long' <LValueToRValue>
|     |   `-DeclRefExpr 0x563e0067c9d8 'uint64_t':'unsigned long' lvalue Var 0x563e0067c760 'parallelDeps' 'uint64_t':'unsigned long'
|     `-ImplicitCastExpr 0x563e0067caf8 'artsGuid_t':'long' <LValueToRValue>
|       `-DeclRefExpr 0x563e0067c9f8 'artsGuid_t':'long' lvalue Var 0x563e0067c380 'parallelDoneEpochGuid' 'artsGuid_t':'long'
|-CallExpr 0x563e0067cc70 'void'
| |-ImplicitCastExpr 0x563e0067cc58 'void (*)(artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067cb60 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' Function 0x563e0066b338 'artsSignalDbs' 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)'
| |-ImplicitCastExpr 0x563e0067ccb0 'artsDataBlock *' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067cb80 'artsDataBlock *' lvalue ParmVar 0x563e0067b208 'A_array' 'artsDataBlock *'
| |-ImplicitCastExpr 0x563e0067ccc8 'artsGuid_t':'long' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067cba0 'artsGuid_t':'long' lvalue Var 0x563e0067c880 'parallelEdtGuid' 'artsGuid_t':'long'
| |-ImplicitCastExpr 0x563e0067cce0 'unsigned int' <IntegralCast>
| | `-IntegerLiteral 0x563e0067cbc0 'int' 0
| `-ImplicitCastExpr 0x563e0067cd10 'unsigned int' <IntegralCast>
|   `-ImplicitCastExpr 0x563e0067ccf8 'int' <LValueToRValue>
|     `-DeclRefExpr 0x563e0067cbe0 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|-CallExpr 0x563e0067cde0 'void'
| |-ImplicitCastExpr 0x563e0067cdc8 'void (*)(artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067cd28 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' Function 0x563e0066b338 'artsSignalDbs' 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)'
| |-ImplicitCastExpr 0x563e0067ce20 'artsDataBlock *' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067cd48 'artsDataBlock *' lvalue ParmVar 0x563e0067bb10 'B_array' 'artsDataBlock *'
| |-ImplicitCastExpr 0x563e0067ce38 'artsGuid_t':'long' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067cd68 'artsGuid_t':'long' lvalue Var 0x563e0067c880 'parallelEdtGuid' 'artsGuid_t':'long'
| |-ImplicitCastExpr 0x563e0067ce68 'unsigned int' <IntegralCast>
| | `-ImplicitCastExpr 0x563e0067ce50 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e0067cd88 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
| `-ImplicitCastExpr 0x563e0067ce98 'unsigned int' <IntegralCast>
|   `-ImplicitCastExpr 0x563e0067ce80 'int' <LValueToRValue>
|     `-DeclRefExpr 0x563e0067cda8 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|-CallExpr 0x563e0067cf90 'bool':'unsigned char'
| |-ImplicitCastExpr 0x563e0067cf78 'bool (*)(artsGuid_t)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067ced8 'bool (artsGuid_t)' Function 0x563e0066ea78 'artsWaitOnHandle' 'bool (artsGuid_t)'
| `-ImplicitCastExpr 0x563e0067cfb8 'artsGuid_t':'long' <LValueToRValue>
|   `-DeclRefExpr 0x563e0067cef8 'artsGuid_t':'long' lvalue Var 0x563e0067c380 'parallelDoneEpochGuid' 'artsGuid_t':'long'
|-DeclStmt 0x563e0067d1f8
| `-VarDecl 0x563e0067d058  used paramv 'uint64_t[1]' cinit
|   |-InitListExpr 0x563e0067d188 'uint64_t[1]'
|   | `-CStyleCastExpr 0x563e0067d108 'uint64_t':'unsigned long' <IntegralCast>
|   |   `-ImplicitCastExpr 0x563e0067d0f0 'int' <LValueToRValue> part_of_explicit_cast
|   |     `-DeclRefExpr 0x563e0067d0c0 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|   `-FullComment 0x563e006822e0
|     `-ParagraphComment 0x563e006822b0
|       `-TextComment 0x563e00682280 Text=" Print the arrays"
|-DeclStmt 0x563e0067d4b0
| `-VarDecl 0x563e0067d220  used printAEdtGuid 'artsGuid_t':'long' cinit
|   |-CallExpr 0x563e0067d3d0 'artsGuid_t':'long'
|   | |-ImplicitCastExpr 0x563e0067d3b8 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
|   | | `-DeclRefExpr 0x563e0067d288 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' Function 0x563e00664938 'artsEdtCreateWithEpoch' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)'
|   | |-CStyleCastExpr 0x563e0067d2f0 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
|   | | `-ImplicitCastExpr 0x563e0067d2d8 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
|   | |   `-DeclRefExpr 0x563e0067d2a8 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e006768c8 'printDataBlockA' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
|   | |-ImplicitCastExpr 0x563e0067d420 'unsigned int' <LValueToRValue>
|   | | `-DeclRefExpr 0x563e0067d318 'unsigned int' lvalue Var 0x563e0067be60 'currentNode' 'unsigned int'
|   | |-ImplicitCastExpr 0x563e0067d438 'uint32_t':'unsigned int' <IntegralCast>
|   | | `-IntegerLiteral 0x563e0067d338 'int' 1
|   | |-ImplicitCastExpr 0x563e0067d450 'uint64_t *' <ArrayToPointerDecay>
|   | | `-DeclRefExpr 0x563e0067d358 'uint64_t[1]' lvalue Var 0x563e0067d058 'paramv' 'uint64_t[1]'
|   | |-ImplicitCastExpr 0x563e0067d480 'uint32_t':'unsigned int' <IntegralCast>
|   | | `-ImplicitCastExpr 0x563e0067d468 'int' <LValueToRValue>
|   | |   `-DeclRefExpr 0x563e0067d378 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|   | `-ImplicitCastExpr 0x563e0067d498 'artsGuid_t':'long' <LValueToRValue>
|   |   `-DeclRefExpr 0x563e0067d398 'artsGuid_t':'long' lvalue Var 0x563e0067bf48 'curentEpoch' 'artsGuid_t':'long'
|   `-FullComment 0x563e006ce260
|     `-ParagraphComment 0x563e006ce230
|       `-TextComment 0x563e006ce200 Text=" Print A"
|-CallExpr 0x563e0067d580 'void'
| |-ImplicitCastExpr 0x563e0067d568 'void (*)(artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067d4c8 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' Function 0x563e0066b338 'artsSignalDbs' 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)'
| |-ImplicitCastExpr 0x563e0067d5c0 'artsDataBlock *' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067d4e8 'artsDataBlock *' lvalue ParmVar 0x563e0067b208 'A_array' 'artsDataBlock *'
| |-ImplicitCastExpr 0x563e0067d5d8 'artsGuid_t':'long' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067d508 'artsGuid_t':'long' lvalue Var 0x563e0067d220 'printAEdtGuid' 'artsGuid_t':'long'
| |-ImplicitCastExpr 0x563e0067d5f0 'unsigned int' <IntegralCast>
| | `-IntegerLiteral 0x563e0067d528 'int' 0
| `-ImplicitCastExpr 0x563e0067d620 'unsigned int' <IntegralCast>
|   `-ImplicitCastExpr 0x563e0067d608 'int' <LValueToRValue>
|     `-DeclRefExpr 0x563e0067d548 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|-DeclStmt 0x563e0067d908
| `-VarDecl 0x563e0067d678  used printBEdtGuid 'artsGuid_t':'long' cinit
|   |-CallExpr 0x563e0067d828 'artsGuid_t':'long'
|   | |-ImplicitCastExpr 0x563e0067d810 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' <FunctionToPointerDecay>
|   | | `-DeclRefExpr 0x563e0067d6e0 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)' Function 0x563e00664938 'artsEdtCreateWithEpoch' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t, artsGuid_t)'
|   | |-CStyleCastExpr 0x563e0067d748 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
|   | | `-ImplicitCastExpr 0x563e0067d730 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
|   | |   `-DeclRefExpr 0x563e0067d700 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e006771a0 'printDataBlockB' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
|   | |-ImplicitCastExpr 0x563e0067d878 'unsigned int' <LValueToRValue>
|   | | `-DeclRefExpr 0x563e0067d770 'unsigned int' lvalue Var 0x563e0067be60 'currentNode' 'unsigned int'
|   | |-ImplicitCastExpr 0x563e0067d890 'uint32_t':'unsigned int' <IntegralCast>
|   | | `-IntegerLiteral 0x563e0067d790 'int' 1
|   | |-ImplicitCastExpr 0x563e0067d8a8 'uint64_t *' <ArrayToPointerDecay>
|   | | `-DeclRefExpr 0x563e0067d7b0 'uint64_t[1]' lvalue Var 0x563e0067d058 'paramv' 'uint64_t[1]'
|   | |-ImplicitCastExpr 0x563e0067d8d8 'uint32_t':'unsigned int' <IntegralCast>
|   | | `-ImplicitCastExpr 0x563e0067d8c0 'int' <LValueToRValue>
|   | |   `-DeclRefExpr 0x563e0067d7d0 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
|   | `-ImplicitCastExpr 0x563e0067d8f0 'artsGuid_t':'long' <LValueToRValue>
|   |   `-DeclRefExpr 0x563e0067d7f0 'artsGuid_t':'long' lvalue Var 0x563e0067bf48 'curentEpoch' 'artsGuid_t':'long'
|   `-FullComment 0x563e006ce330
|     `-ParagraphComment 0x563e006ce300
|       `-TextComment 0x563e006ce2d0 Text=" Print B"
|-CallExpr 0x563e0067d9d8 'void'
| |-ImplicitCastExpr 0x563e0067d9c0 'void (*)(artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067d920 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)' Function 0x563e0066b338 'artsSignalDbs' 'void (artsDataBlock *, artsGuid_t, unsigned int, unsigned int)'
| |-ImplicitCastExpr 0x563e0067da18 'artsDataBlock *' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067d940 'artsDataBlock *' lvalue ParmVar 0x563e0067bb10 'B_array' 'artsDataBlock *'
| |-ImplicitCastExpr 0x563e0067da30 'artsGuid_t':'long' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067d960 'artsGuid_t':'long' lvalue Var 0x563e0067d678 'printBEdtGuid' 'artsGuid_t':'long'
| |-ImplicitCastExpr 0x563e0067da48 'unsigned int' <IntegralCast>
| | `-IntegerLiteral 0x563e0067d980 'int' 0
| `-ImplicitCastExpr 0x563e0067da78 'unsigned int' <IntegralCast>
|   `-ImplicitCastExpr 0x563e0067da60 'int' <LValueToRValue>
|     `-DeclRefExpr 0x563e0067d9a0 'int' lvalue ParmVar 0x563e0067b188 'N' 'int'
`-CallExpr 0x563e0067db48 'int'
  |-ImplicitCastExpr 0x563e0067db30 'int (*)(const char *, ...)' <FunctionToPointerDecay>
  | `-DeclRefExpr 0x563e0067da90 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
  `-ImplicitCastExpr 0x563e0067db88 'const char *' <NoOp>
    `-ImplicitCastExpr 0x563e0067db70 'char *' <ArrayToPointerDecay>
      `-StringLiteral 0x563e0067dab0 'char[44]' lvalue "---------- Compute EDT finished ----------\n"

.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.const char *
.const char
----------------------
ANALYZING FUNCTION: computeEDT
Getting MLIR Type
.int
- MLIR Type: i32t
 - Parameter: N type: i32
Getting MLIR Type
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>t
 - Parameter: A_array type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>
Getting MLIR Type
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>t
 - Parameter: B_array type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?x!llvm.struct<(i64, memref<?xi8>)>>, memref<?x!llvm.struct<(i64, memref<?xi8>)>>) -> ()
.int *
.int
.int
.artsDataBlock **
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock **
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.const char *
.const char
.unsigned int
.unsigned int &
.unsigned int
.unsigned int
----------------------
ANALYZING FUNCTION: artsGetCurrentNode
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsGetCurrentEpochGuid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreate
Getting MLIR Type
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>t
 - Parameter: funcPtr type: memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: route type: i32
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
Creating function: (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
----------------------
ANALYZING FUNCTION: parallelDoneEdt
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsInitializeAndStartEpoch
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: finishEdtGuid type: i64
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: slot type: i32
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
Creating function: (i64, i32) -> i64
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[1]
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[1]
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t &
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreateWithEpoch
----------------------
ANALYZING FUNCTION: parallelEdt
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsSignalDbs
Getting MLIR Type
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>t
 - Parameter: dbArray type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: edtGuid type: i64
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: initialSlot type: i32
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: numElements type: i32
.void
.void
Creating function: (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.unsigned int
.int
.unsigned int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned int
.unsigned int
.void
----------------------
ANALYZING FUNCTION: artsSignalDbs
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.unsigned int
.int
.unsigned int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned int
.unsigned int
.void
----------------------
ANALYZING FUNCTION: artsWaitOnHandle
Getting MLIR Type
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
- MLIR Type: i64t
 - Parameter: epochGuid type: i64
.bool
.bool
.uint8_t
.uint8_t
.__uint8_t
.__uint8_t
.unsigned char
.bool
.bool
.uint8_t
.uint8_t
.__uint8_t
.__uint8_t
.unsigned char
Creating function: (i64) -> i8
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.bool
.bool
.uint8_t
.uint8_t
.__uint8_t
.__uint8_t
.unsigned char
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[1]
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[1]
.uint64_t[1]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreateWithEpoch
----------------------
ANALYZING FUNCTION: printDataBlockA
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsSignalDbs
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.unsigned int
.int
.unsigned int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned int
.unsigned int
.void
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreateWithEpoch
----------------------
ANALYZING FUNCTION: printDataBlockB
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsSignalDbs
.artsDataBlock *
.arEmitting fn: finishMainEdt
finishMainEdt
CompoundStmt 0x563e0067e270
|-CallExpr 0x563e0067dfc8 'int'
| |-ImplicitCastExpr 0x563e0067dfb0 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067df38 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| `-ImplicitCastExpr 0x563e0067e008 'const char *' <NoOp>
|   `-ImplicitCastExpr 0x563e0067dff0 'char *' <ArrayToPointerDecay>
|     `-StringLiteral 0x563e0067df58 'char[40]' lvalue "--------------------------------------\n"
|-CallExpr 0x563e0067e0b0 'int'
| |-ImplicitCastExpr 0x563e0067e098 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067e020 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| `-ImplicitCastExpr 0x563e0067e0f0 'const char *' <NoOp>
|   `-ImplicitCastExpr 0x563e0067e0d8 'char *' <ArrayToPointerDecay>
|     `-StringLiteral 0x563e0067e040 'char[40]' lvalue "The program has finished its execution\n"
|-CallExpr 0x563e0067e198 'int'
| |-ImplicitCastExpr 0x563e0067e180 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067e108 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| `-ImplicitCastExpr 0x563e0067e1d8 'const char *' <NoOp>
|   `-ImplicitCastExpr 0x563e0067e1c0 'char *' <ArrayToPointerDecay>
|     `-StringLiteral 0x563e0067e128 'char[40]' lvalue "--------------------------------------\n"
`-CallExpr 0x563e0067e250 'void'
  `-ImplicitCastExpr 0x563e0067e238 'void (*)()' <FunctionToPointerDecay>
    `-DeclRefExpr 0x563e0067e1f0 'void ()' Function 0x563e00661478 'artsShutdown' 'void ()'
Emitting fn: mainEdt
mainEdt
tsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.unsigned int
.int
.unsigned int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned int
.unsigned int
.void
.const char *
.const char
----------------------
ANALYZING FUNCTION: finishMainEdt
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.const char *
.const char
.const char *
.const char
.const char *
.const char
----------------------
ANALYZING FUNCTION: artsShutdown
.void
.void
Creating function: () -> ()
.void
----------------------
ANALYZING FUNCTION: mainEdt
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: paramc type: i32
Getting MLIR Type
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: memref<?xi64>t
 - Parameter: paramv type: memref<?xi64>
Getting MLIR Type
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
- MLIR Type: i32t
 - Parameter: depc type: i32
Getting MLIR Type
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>t
 - Parameter: depv type: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>
.void
.void
Creating function: (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> ()
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
CompoundStmt 0x563e00680410
|-CallExpr 0x563e0067e628 'int'
| |-ImplicitCastExpr 0x563e0067e610 'int (*)(const char *, ...)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067e5a0 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
| `-ImplicitCastExpr 0x563e0067e668 'const char *' <NoOp>
|   `-ImplicitCastExpr 0x563e0067e650 'char *' <ArrayToPointerDecay>
|     `-StringLiteral 0x563e0067e5c0 'char[32]' lvalue "---------- Main EDT ----------\n"
|-DeclStmt 0x563e0067e758
| `-VarDecl 0x563e0067e698  used currentNode 'unsigned int' cinit
|   `-CallExpr 0x563e0067e738 'unsigned int'
|     `-ImplicitCastExpr 0x563e0067e720 'unsigned int (*)()' <FunctionToPointerDecay>
|       `-DeclRefExpr 0x563e0067e700 'unsigned int ()' Function 0x563e00672860 'artsGetCurrentNode' 'unsigned int ()'
|-DeclStmt 0x563e0067ea18
| `-VarDecl 0x563e0067e780  used finishMainEdtGuid 'artsGuid_t':'long' cinit
|   `-CallExpr 0x563e0067e970 'artsGuid_t':'long'
|     |-ImplicitCastExpr 0x563e0067e958 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e0067e7e8 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)' Function 0x563e00663bd8 'artsEdtCreate' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)'
|     |-CStyleCastExpr 0x563e0067e850 'artsEdt_t':'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <NoOp>
|     | `-ImplicitCastExpr 0x563e0067e838 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay> part_of_explicit_cast
|     |   `-DeclRefExpr 0x563e0067e808 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e0067de70 'finishMainEdt' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
|     |-ImplicitCastExpr 0x563e0067e9b8 'unsigned int' <LValueToRValue>
|     | `-DeclRefExpr 0x563e0067e878 'unsigned int' lvalue Var 0x563e0067e698 'currentNode' 'unsigned int'
|     |-ImplicitCastExpr 0x563e0067e9d0 'uint32_t':'unsigned int' <IntegralCast>
|     | `-IntegerLiteral 0x563e0067e898 'int' 0
|     |-ImplicitCastExpr 0x563e0067e9e8 'uint64_t *' <NullToPointer>
|     | `-ParenExpr 0x563e0067e918 'void *'
|     |   `-CStyleCastExpr 0x563e0067e8f0 'void *' <NullToPointer>
|     |     `-IntegerLiteral 0x563e0067e8b8 'int' 0
|     `-ImplicitCastExpr 0x563e0067ea00 'uint32_t':'unsigned int' <IntegralCast>
|       `-IntegerLiteral 0x563e0067e938 'int' 1
|-DeclStmt 0x563e0067efd8
| `-VarDecl 0x563e0067ea40  used finishMainEpochGuid 'artsGuid_t':'long' cinit
|   `-CallExpr 0x563e0067ef50 'artsGuid_t':'long'
|     |-ImplicitCastExpr 0x563e0067eb08 'artsGuid_t (*)(artsGuid_t, unsigned int)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e0067eaa8 'artsGuid_t (artsGuid_t, unsigned int)' Function 0x563e0066e4b8 'artsInitializeAndStartEpoch' 'artsGuid_t (artsGuid_t, unsigned int)'
|     |-ImplicitCastExpr 0x563e0067ef80 'artsGuid_t':'long' <LValueToRValue>
|     | `-DeclRefExpr 0x563e0067eac8 'artsGuid_t':'long' lvalue Var 0x563e0067e780 'finishMainEdtGuid' 'artsGuid_t':'long'
|     `-ImplicitCastExpr 0x563e0067ef98 'unsigned int' <IntegralCast>
|       `-IntegerLiteral 0x563e0067eae8 'int' 0
|-DeclStmt 0x563e0067f090
| `-VarDecl 0x563e0067f008  used N 'int' cinit
|   |-IntegerLiteral 0x563e0067f070 'int' 5
|   `-FullComment 0x563e006ce460
|     `-ParagraphComment 0x563e006ce430
|       `-TextComment 0x563e006ce400 Text=" Define the size of the computation"
|-DeclStmt 0x563e0067f270
| `-VarDecl 0x563e0067f0c0  used A 'double *' cinit
|   `-CStyleCastExpr 0x563e0067f248 'double *' <BitCast>
|     `-CallExpr 0x563e0067f208 'void *'
|       |-ImplicitCastExpr 0x563e0067f1f0 'void *(*)(size_t)' <FunctionToPointerDecay>
|       | `-DeclRefExpr 0x563e0067f128 'void *(size_t)' Function 0x563e006615c8 'artsMalloc' 'void *(size_t)'
|       `-BinaryOperator 0x563e0067f1d0 'unsigned long' '*'
|         |-ImplicitCastExpr 0x563e0067f1b8 'unsigned long' <IntegralCast>
|         | `-ImplicitCastExpr 0x563e0067f1a0 'int' <LValueToRValue>
|         |   `-DeclRefExpr 0x563e0067f148 'int' lvalue Var 0x563e0067f008 'N' 'int'
|         `-UnaryExprOrTypeTraitExpr 0x563e0067f180 'unsigned long' sizeof 'double'
|-DeclStmt 0x563e0067f478
| `-VarDecl 0x563e0067f2a0  used B 'double *' cinit
|   `-CStyleCastExpr 0x563e0067f428 'double *' <BitCast>
|     `-CallExpr 0x563e0067f3e8 'void *'
|       |-ImplicitCastExpr 0x563e0067f3d0 'void *(*)(size_t)' <FunctionToPointerDecay>
|       | `-DeclRefExpr 0x563e0067f308 'void *(size_t)' Function 0x563e006615c8 'artsMalloc' 'void *(size_t)'
|       `-BinaryOperator 0x563e0067f3b0 'unsigned long' '*'
|         |-ImplicitCastExpr 0x563e0067f398 'unsigned long' <IntegralCast>
|         | `-ImplicitCastExpr 0x563e0067f380 'int' <LValueToRValue>
|         |   `-DeclRefExpr 0x563e0067f328 'int' lvalue Var 0x563e0067f008 'N' 'int'
|         `-UnaryExprOrTypeTraitExpr 0x563e0067f360 'unsigned long' sizeof 'double'
|-ForStmt 0x563e0067f818
| |-DeclStmt 0x563e0067f530
| | `-VarDecl 0x563e0067f4a8  used i 'int' cinit
| |   |-IntegerLiteral 0x563e0067f510 'int' 0
| |   `-FullComment 0x563e006ce530
| |     `-ParagraphComment 0x563e006ce500
| |       `-TextComment 0x563e006ce4d0 Text=" Initialize A with numbers from 0 to N-1"
| |-<<<NULL>>>
| |-BinaryOperator 0x563e0067f5b8 'int' '<'
| | |-ImplicitCastExpr 0x563e0067f588 'int' <LValueToRValue>
| | | `-DeclRefExpr 0x563e0067f548 'int' lvalue Var 0x563e0067f4a8 'i' 'int'
| | `-ImplicitCastExpr 0x563e0067f5a0 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e0067f568 'int' lvalue Var 0x563e0067f008 'N' 'int'
| |-UnaryOperator 0x563e0067f5f8 'int' postfix '++'
| | `-DeclRefExpr 0x563e0067f5d8 'int' lvalue Var 0x563e0067f4a8 'i' 'int'
| `-CompoundStmt 0x563e0067f7f8
|   |-BinaryOperator 0x563e0067f6f0 'double' '='
|   | |-ArraySubscriptExpr 0x563e0067f680 'double' lvalue
|   | | |-ImplicitCastExpr 0x563e0067f650 'double *' <LValueToRValue>
|   | | | `-DeclRefExpr 0x563e0067f610 'double *' lvalue Var 0x563e0067f0c0 'A' 'double *'
|   | | `-ImplicitCastExpr 0x563e0067f668 'int' <LValueToRValue>
|   | |   `-DeclRefExpr 0x563e0067f630 'int' lvalue Var 0x563e0067f4a8 'i' 'int'
|   | `-ImplicitCastExpr 0x563e0067f6d8 'double' <IntegralToFloating>
|   |   `-ImplicitCastExpr 0x563e0067f6c0 'int' <LValueToRValue>
|   |     `-DeclRefExpr 0x563e0067f6a0 'int' lvalue Var 0x563e0067f4a8 'i' 'int'
|   `-BinaryOperator 0x563e0067f7d8 'double' '='
|     |-ArraySubscriptExpr 0x563e0067f780 'double' lvalue
|     | |-ImplicitCastExpr 0x563e0067f750 'double *' <LValueToRValue>
|     | | `-DeclRefExpr 0x563e0067f710 'double *' lvalue Var 0x563e0067f2a0 'B' 'double *'
|     | `-ImplicitCastExpr 0x563e0067f768 'int' <LValueToRValue>
|     |   `-DeclRefExpr 0x563e0067f730 'int' lvalue Var 0x563e0067f4a8 'i' 'int'
|     `-ImplicitCastExpr 0x563e0067f7c0 'double' <IntegralToFloating>
|       `-IntegerLiteral 0x563e0067f7a0 'int' 0
|-DeclStmt 0x563e0067fb08
| |-VarDecl 0x563e0067f938  used A_array 'artsDataBlock[N]'
| `-VarDecl 0x563e0067fa88  used B_array 'artsDataBlock[N]'
|-CallExpr 0x563e0067fcc0 'void'
| |-ImplicitCastExpr 0x563e0067fca8 'void (*)(artsDataBlock *, uint64_t, artsType_t, unsigned int, void *)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067fb20 'void (artsDataBlock *, uint64_t, artsType_t, unsigned int, void *)' Function 0x563e0066abe8 'artsDbCreateArray' 'void (artsDataBlock *, uint64_t, artsType_t, unsigned int, void *)'
| |-ImplicitCastExpr 0x563e0067fd08 'artsDataBlock *' <ArrayToPointerDecay>
| | `-DeclRefExpr 0x563e0067fb40 'artsDataBlock[N]' lvalue Var 0x563e0067f938 'A_array' 'artsDataBlock[N]'
| |-UnaryExprOrTypeTraitExpr 0x563e0067fb78 'unsigned long' sizeof 'double'
| |-ImplicitCastExpr 0x563e0067fd20 'artsType_t':'artsType_t' <IntegralCast>
| | `-DeclRefExpr 0x563e0067fb98 'int' EnumConstant 0x563e0065ac90 'ARTS_DB_PIN' 'int'
| |-ImplicitCastExpr 0x563e0067fd50 'unsigned int' <IntegralCast>
| | `-ImplicitCastExpr 0x563e0067fd38 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e0067fbb8 'int' lvalue Var 0x563e0067f008 'N' 'int'
| `-CStyleCastExpr 0x563e0067fc28 'void *' <BitCast>
|   `-ImplicitCastExpr 0x563e0067fc10 'double *' <LValueToRValue> part_of_explicit_cast
|     `-DeclRefExpr 0x563e0067fbd8 'double *' lvalue Var 0x563e0067f0c0 'A' 'double *'
|-CallExpr 0x563e0067feb0 'void'
| |-ImplicitCastExpr 0x563e0067fe98 'void (*)(artsDataBlock *, uint64_t, artsType_t, unsigned int, void *)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067fd68 'void (artsDataBlock *, uint64_t, artsType_t, unsigned int, void *)' Function 0x563e0066abe8 'artsDbCreateArray' 'void (artsDataBlock *, uint64_t, artsType_t, unsigned int, void *)'
| |-ImplicitCastExpr 0x563e0067fef8 'artsDataBlock *' <ArrayToPointerDecay>
| | `-DeclRefExpr 0x563e0067fd88 'artsDataBlock[N]' lvalue Var 0x563e0067fa88 'B_array' 'artsDataBlock[N]'
| |-UnaryExprOrTypeTraitExpr 0x563e0067fdc0 'unsigned long' sizeof 'double'
| |-ImplicitCastExpr 0x563e0067ff10 'artsType_t':'artsType_t' <IntegralCast>
| | `-DeclRefExpr 0x563e0067fde0 'int' EnumConstant 0x563e0065ac90 'ARTS_DB_PIN' 'int'
| |-ImplicitCastExpr 0x563e0067ff60 'unsigned int' <IntegralCast>
| | `-ImplicitCastExpr 0x563e0067ff28 'int' <LValueToRValue>
| |   `-DeclRefExpr 0x563e0067fe00 'int' lvalue Var 0x563e0067f008 'N' 'int'
| `-CStyleCastExpr 0x563e0067fe70 'void *' <BitCast>
|   `-ImplicitCastExpr 0x563e0067fe58 'double *' <LValueToRValue> part_of_explicit_cast
|     `-DeclRefExpr 0x563e0067fe20 'double *' lvalue Var 0x563e0067f2a0 'B' 'double *'
|-CallExpr 0x563e00680070 'void'
| |-ImplicitCastExpr 0x563e00680058 'void (*)(int, artsDataBlock *, artsDataBlock *)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e0067ff78 'void (int, artsDataBlock *, artsDataBlock *)' Function 0x563e0067bc38 'computeEDT' 'void (int, artsDataBlock *, artsDataBlock *)'
| |-ImplicitCastExpr 0x563e006800a8 'int' <LValueToRValue>
| | `-DeclRefExpr 0x563e0067ff98 'int' lvalue Var 0x563e0067f008 'N' 'int'
| |-ImplicitCastExpr 0x563e006800c0 'artsDataBlock *' <ArrayToPointerDecay>
| | `-DeclRefExpr 0x563e0067ffb8 'artsDataBlock[N]' lvalue Var 0x563e0067f938 'A_array' 'artsDataBlock[N]'
| `-ImplicitCastExpr 0x563e006800d8 'artsDataBlock *' <ArrayToPointerDecay>
|   `-DeclRefExpr 0x563e0067ffd8 'artsDataBlock[N]' lvalue Var 0x563e0067fa88 'B_array' 'artsDataBlock[N]'
|-CallExpr 0x563e00680148 'bool':'unsigned char'
| |-ImplicitCastExpr 0x563e00680130 'bool (*)(artsGuid_t)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e006800f0 'bool (artsGuid_t)' Function 0x563e0066ea78 'artsWaitOnHandle' 'bool (artsGuid_t)'
| `-ImplicitCastExpr 0x563e00680170 'artsGuid_t':'long' <LValueToRValue>
|   `-DeclRefExpr 0x563e00680110 'artsGuid_t':'long' lvalue Var 0x563e0067ea40 'finishMainEpochGuid' 'artsGuid_t':'long'
|-CallExpr 0x563e006801e0 'void'
| |-ImplicitCastExpr 0x563e006801c8 'void (*)(void *)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00680188 'void (void *)' Function 0x563e00661e20 'artsFree' 'void (void *)'
| `-ImplicitCastExpr 0x563e00680220 'void *' <BitCast>
|   `-ImplicitCastExpr 0x563e00680208 'double *' <LValueToRValue>
|     `-DeclRefExpr 0x563e006801a8 'double *' lvalue Var 0x563e0067f0c0 'A' 'double *'
|-CallExpr 0x563e00680290 'void'
| |-ImplicitCastExpr 0x563e00680278 'void (*)(void *)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00680238 'void (void *)' Function 0x563e00661e20 'artsFree' 'void (void *)'
| `-ImplicitCastExpr 0x563e006802d0 'void *' <BitCast>
|   `-ImplicitCastExpr 0x563e006802b8 'double *' <LValueToRValue>
|     `-DeclRefExpr 0x563e00680258 'double *' lvalue Var 0x563e0067f2a0 'B' 'double *'
`-CallExpr 0x563e006803b8 'int'
  |-ImplicitCastExpr 0x563e006803a0 'int (*)(const char *, ...)' <FunctionToPointerDecay>
  | `-DeclRefExpr 0x563e006802e8 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
  `-ImplicitCastExpr 0x563e006803f8 'const char *' <NoOp>
    `-ImplicitCastExpr 0x563e006803e0 'char *' <ArrayToPointerDecay>
      `-StringLiteral 0x563e00680348 'char[41]' lvalue "---------- Main EDT finished ----------\n"
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t **
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t *
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t **
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.const char *
.const char
.unsigned int
.unsigned int &
.unsigned int
.unsigned int
----------------------
ANALYZING FUNCTION: artsGetCurrentNode
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsEdtCreate
----------------------
ANALYZING FUNCTION: finishMainEdt
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdt_t
.artsEdt_t
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t &
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
----------------------
ANALYZING FUNCTION: artsInitializeAndStartEpoch
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.int
.int &
.int
.int
.int
.double *
.double
.double *&
.double *
.double
.double *
.double
----------------------
ANALYZING FUNCTION: artsMalloc
.int
.unsigned long
.double
.unsigned long
.unsigned long
.void *
.double *
.double
.double *
.double
.double *&
.double *
.double
.double *
.double
----------------------
ANALYZING FUNCTION: artsMalloc
.int
.unsigned long
.double
.unsigned long
.unsigned long
.void *
.double *
.double
.int
.int &
.int
.int
.int
.int
.int
.int
.double *
.double
.int
.double
.int
.double
.double *
.double
.int
.double
.int
.double
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.int
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock (&)[N]
.artsDataBlock[N]
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.int
----------------------
ANALYZING FUNCTION: artsDbCreateArray
Getting MLIR Type
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
- MLIR Type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>t
 - Parameter: dbArray type: memref<?x!llvm.struct<(i64, memref<?xi8>)>>
Getting MLIR Type
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
- MLIR Type: i64t
 - Parameter: size type: i64
Getting MLIR Type
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
- MLIR Type: i32t
 - Parameter: mode type: i32
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: numElements type: i32
Getting MLIR Type
.void *
- MLIR Type: memref<?xi8>t
 - Parameter: data type: memref<?xi8>
.void
.void
Creating function: (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> ()
.double
.unsigned long
.int
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
.int
.unsigned int
.double *
.double
.void *
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.unsigned long
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
.unsigned int
.void *
.void
----------------------
ANALYZING FUNCTION: artsDbCreateArray
.double
.unsigned long
.int
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
.int
.unsigned int
.double *
.double
.void *
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.unsigned long
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
.unsigned int
.void *
.void
----------------------
ANALYZING FUNCTION: computeEDT
.int
.int
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.artsDataBlock *
.artsDataBlock
.artsDataBlock
.struct artsDataBlock
.artsDataBlock
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, memref<?xi8>)>
.void
----------------------
ANALYZING FUNCTION: artsWaitOnHandle
.artsGuid_t
.artsGuiEmitting fn: initPerNode
initPerNode
CompoundStmt 0x563e00680da0
|-IfStmt 0x563e00680c30
| |-BinaryOperator 0x563e006807c8 'int' '=='
| | |-ImplicitCastExpr 0x563e00680798 'unsigned int' <LValueToRValue>
| | | `-DeclRefExpr 0x563e00680758 'unsigned int' lvalue ParmVar 0x563e006804b8 'nodeId' 'unsigned int'
| | `-ImplicitCastExpr 0x563e006807b0 'unsigned int' <IntegralCast>
| |   `-IntegerLiteral 0x563e00680778 'int' 0
| `-CompoundStmt 0x563e00680c08
|   |-DeclStmt 0x563e006808c0
|   | `-VarDecl 0x563e00680800  used currentNode 'unsigned int' cinit
|   |   `-CallExpr 0x563e006808a0 'unsigned int'
|   |     `-ImplicitCastExpr 0x563e00680888 'unsigned int (*)()' <FunctionToPointerDecay>
|   |       `-DeclRefExpr 0x563e00680868 'unsigned int ()' Function 0x563e00672860 'artsGetCurrentNode' 'unsigned int ()'
|   |-DeclStmt 0x563e00680a58
|   | `-VarDecl 0x563e006808f8  used mainparams 'uint64_t[0]' cinit
|   |   `-InitListExpr 0x563e006809a0 'uint64_t[0]'
|   `-CallExpr 0x563e00680b48 'artsGuid_t':'long'
|     |-ImplicitCastExpr 0x563e00680b30 'artsGuid_t (*)(artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e00680a70 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)' Function 0x563e00663bd8 'artsEdtCreate' 'artsGuid_t (artsEdt_t, unsigned int, uint32_t, uint64_t *, uint32_t)'
|     |-ImplicitCastExpr 0x563e00680b90 'void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' <FunctionToPointerDecay>
|     | `-DeclRefExpr 0x563e00680a90 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)' Function 0x563e0067e4d8 'mainEdt' 'void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)'
|     |-ImplicitCastExpr 0x563e00680ba8 'unsigned int' <LValueToRValue>
|     | `-DeclRefExpr 0x563e00680ab0 'unsigned int' lvalue Var 0x563e00680800 'currentNode' 'unsigned int'
|     |-ImplicitCastExpr 0x563e00680bc0 'uint32_t':'unsigned int' <IntegralCast>
|     | `-IntegerLiteral 0x563e00680ad0 'int' 1
|     |-ImplicitCastExpr 0x563e00680bd8 'uint64_t *' <ArrayToPointerDecay>
|     | `-DeclRefExpr 0x563e00680af0 'uint64_t[0]' lvalue Var 0x563e006808f8 'mainparams' 'uint64_t[0]'
|     `-ImplicitCastExpr 0x563e00680bf0 'uint32_t':'unsigned int' <IntegralCast>
|       `-IntegerLiteral 0x563e00680b10 'int' 0
`-CallExpr 0x563e00680d28 'int'
  |-ImplicitCastExpr 0x563e00680d10 'int (*)(const char *, ...)' <FunctionToPointerDecay>
  | `-DeclRefExpr 0x563e00680c50 'int (const char *, ...)' Function 0x563e005de4f0 'printf' 'int (const char *, ...)'
  |-ImplicitCastExpr 0x563e00680d70 'const char *' <NoOp>
  | `-ImplicitCastExpr 0x563e00680d58 'char *' <ArrayToPointerDecay>
  |   `-StringLiteral 0x563e00680ca8 'char[22]' lvalue "Node %u initialized.\n"
  `-ImplicitCastExpr 0x563e00680d88 'unsigned int' <LValueToRValue>
    `-DeclRefExpr 0x563e00680cd8 'unsigned int' lvalue ParmVar 0x563e006804b8 'nodeId' 'unsigned int'
Emitting fn: initPerWorker
initPerWorker
CompoundStmt 0x563e00681520
Emitting fn: main
main
CompoundStmt 0x563e00681850
|-CallExpr 0x563e006817c0 'int'
| |-ImplicitCastExpr 0x563e006817a8 'int (*)(int, char **)' <FunctionToPointerDecay>
| | `-DeclRefExpr 0x563e00681720 'int (int, char **)' Function 0x563e00661370 'artsRT' 'int (int, char **)'
| |-ImplicitCastExpr 0x563e006817f0 'int' <LValueToRValue>
| | `-DeclRefExpr 0x563e00681740 'int' lvalue ParmVar 0x563e00681548 'argc' 'int'
| `-ImplicitCastExpr 0x563e00681808 'char **' <LValueToRValue>
|   `-DeclRefExpr 0x563e00681760 'char **' lvalue ParmVar 0x563e006815c8 'argv' 'char **'
`-ReturnStmt 0x563e00681840
  `-IntegerLiteral 0x563e00681820 'int' 0
d_t
.intptr_t
.intptr_t
.long
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.bool
.bool
.uint8_t
.uint8_t
.__uint8_t
.__uint8_t
.unsigned char
----------------------
ANALYZING FUNCTION: artsFree
Getting MLIR Type
.void *
- MLIR Type: memref<?xi8>t
 - Parameter: ptr type: memref<?xi8>
.void
.void
Creating function: (memref<?xi8>) -> ()
.double *
.double
.void *
.void *
.void
----------------------
ANALYZING FUNCTION: artsFree
.double *
.double
.void *
.void *
.void
.const char *
.const char
----------------------
ANALYZING FUNCTION: initPerNode
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: nodeId type: i32
Getting MLIR Type
.int
- MLIR Type: i32t
 - Parameter: argc type: i32
Getting MLIR Type
.char **
.char *
.char
- MLIR Type: memref<?xmemref<?xi8>>t
 - Parameter: argv type: memref<?xmemref<?xi8>>
.void
.void
Creating function: (i32, i32, memref<?xmemref<?xi8>>) -> ()
.unsigned int *
.unsigned int
.unsigned int
.int *
.int
.int
.char ***
.char **
.char *
.char
.char **
.char *
.char
.unsigned int
.int
.unsigned int
.int
.unsigned int
.unsigned int &
.unsigned int
.unsigned int
----------------------
ANALYZING FUNCTION: artsGetCurrentNode
.unsigned int
.uint64_t[0]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[0]
.uint64_t[0]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t[0]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint64_t (&)[0]
.uint64_t[0]
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
----------------------
ANALYZING FUNCTION: artsEdtCreate
----------------------
ANALYZING FUNCTION: mainEdt
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.void (*)(uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void (uint32_t, uint64_t *, uint32_t, artsEdtDep_t *)
.void
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsEdtDep_t *
.artsEdtDep_t[]
.artsEdtDep_t
.artsEdtDep_t
.struct artsEdtDep_t
.artsEdtDep_t
    Field: guid
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
    Field: mode
.artsType_t
.artsType_t
.enum artsType_t
.artsType_t
    Field: ptr
.void *
  - Returning LLVM struct type: !llvm.struct<(i64, i32, memref<?xi8>)>
.unsigned int
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.uint64_t *
.uint64_t
.uint64_t
.__uint64_t
.__uint64_t
.unsigned long
.uint32_t
.uint32_t
.__uint32_t
.__uint32_t
.unsigned int
.artsGuid_t
.artsGuid_t
.intptr_t
.intptr_t
.long
.const char *
.const char
.unsigned int
.unsigned int
----------------------
ANALYZING FUNCTION: initPerWorker
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: nodeId type: i32
Getting MLIR Type
.unsigned int
- MLIR Type: i32t
 - Parameter: workerId type: i32
Getting MLIR Type
.int
- MLIR Type: i32t
 - Parameter: argc type: i32
Getting MLIR Type
.char **
.char *
.char
- MLIR Type: memref<?xmemref<?xi8>>t
 - Parameter: argv type: memref<?xmemref<?xi8>>
.void
.void
Creating function: (i32, i32, i32, memref<?xmemref<?xi8>>) -> ()
.unsigned int *
.unsigned int
.unsigned int
.unsigned int *
.unsigned int
.unsigned int
.int *
.int
.int
.char ***
.char **
.char *
.char
.char **
.char *
.char
----------------------
ANALYZING FUNCTION: main
Getting MLIR Type
.int
- MLIR Type: i32t
 - Parameter: argc type: i32
Getting MLIR Type
.char **
.char *
.char
- MLIR Type: memref<?xmemref<?xi8>>t
 - Parameter: argv type: memref<?xmemref<?xi8>>
.int
.int
Creating function: (i32, memref<?xmemref<?xi8>>) -> i32
.int *
.int
.int
.char ***
.char **
.char *
.char
.char **
.char *
.char
----------------------
ANALYZING FUNCTION: artsRT
Getting MLIR Type
.int
- MLIR Type: i32t
 - Parameter: argc type: i32
Getting MLIR Type
.char **
.char *
.char
- MLIR Type: memref<?xmemref<?xi8>>t
 - Parameter: argv type: memref<?xmemref<?xi8>>
.int
.int
Creating function: (i32, memref<?xmemref<?xi8>>) -> i32
.int
.char **
.char *
.char
.int
.char **
.char *
.char
.int
.int
.int
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
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
  func.func @computeA(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 2.000000e+00 : f64
    %0 = affine.load %arg1[0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = affine.load %arg1[1] : memref<?xi64>
    %3 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %4 = llvm.getelementptr %3[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
    %5 = llvm.load %4 : !llvm.ptr -> memref<?xi8>
    %6 = "polygeist.memref2pointer"(%5) : (memref<?xi8>) -> !llvm.ptr
    %7 = llvm.load %3 : !llvm.ptr -> i64
    %8 = arith.uitofp %1 : i32 to f64
    %9 = arith.mulf %8, %cst : f64
    llvm.store %9, %6 : f64, !llvm.ptr
    %10 = llvm.mlir.addressof @str0 : !llvm.ptr
    %11 = llvm.getelementptr %10[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<86 x i8>
    %12 = llvm.load %6 : !llvm.ptr -> f64
    %13 = llvm.call @printf(%11, %1, %12, %7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64, i64) -> i32
    call @artsEventSatisfySlot(%2, %7, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @computeB(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c2 = arith.constant 2 : index
    %0 = affine.load %arg1[0] : memref<?xi64>
    %1 = arith.trunci %0 : i64 to i32
    %2 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, memref<?xi8>)>}> : () -> index
    %3 = arith.index_cast %2 : index to i64
    %4 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %5 = llvm.getelementptr %4[%3] : (!llvm.ptr, i64) -> !llvm.ptr, i8
    %6 = llvm.getelementptr %5[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
    %7 = llvm.load %6 : !llvm.ptr -> memref<?xi8>
    %8 = "polygeist.memref2pointer"(%7) : (memref<?xi8>) -> !llvm.ptr
    %9 = arith.cmpi ugt, %1, %c0_i32 : i32
    %10 = scf.if %9 -> (f64) {
      %20 = arith.muli %2, %c2 : index
      %21 = arith.index_cast %20 : index to i64
      %22 = llvm.getelementptr %4[%21] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %23 = llvm.getelementptr %22[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
      %24 = llvm.load %23 : !llvm.ptr -> memref<?xi8>
      %25 = "polygeist.memref2pointer"(%24) : (memref<?xi8>) -> !llvm.ptr
      %26 = llvm.load %25 : !llvm.ptr -> f64
      scf.yield %26 : f64
    } else {
      scf.yield %cst : f64
    }
    %11 = llvm.getelementptr %4[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
    %12 = llvm.load %11 : !llvm.ptr -> memref<?xi8>
    %13 = "polygeist.memref2pointer"(%12) : (memref<?xi8>) -> !llvm.ptr
    %14 = llvm.load %8 : !llvm.ptr -> f64
    %15 = arith.addf %14, %10 : f64
    llvm.store %15, %13 : f64, !llvm.ptr
    %16 = llvm.mlir.addressof @str1 : !llvm.ptr
    %17 = llvm.getelementptr %16[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<74 x i8>
    %18 = llvm.load %13 : !llvm.ptr -> f64
    %19 = llvm.call @printf(%17, %1, %18) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
    return
  }
  func.func @printDataBlockA(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, memref<?xi8>)>}> : () -> index
    %1 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %2 = llvm.mlir.addressof @str2 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %4 = scf.while (%arg4 = %c0_i32) : (i32) -> i32 {
      %5 = arith.extsi %arg4 : i32 to i64
      %6 = affine.load %arg1[0] : memref<?xi64>
      %7 = arith.cmpi slt, %5, %6 : i64
      scf.condition(%7) %arg4 : i32
    } do {
    ^bb0(%arg4: i32):
      %5 = arith.index_cast %arg4 : i32 to index
      %6 = arith.muli %5, %0 : index
      %7 = arith.index_cast %6 : index to i64
      %8 = llvm.getelementptr %1[%7] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %9 = llvm.getelementptr %8[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
      %10 = llvm.load %9 : !llvm.ptr -> memref<?xi8>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.load %11 : !llvm.ptr -> f64
      %13 = llvm.call @printf(%3, %arg4, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %14 = arith.addi %arg4, %c1_i32 : i32
      scf.yield %14 : i32
    }
    return
  }
  func.func @printDataBlockB(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, i32, memref<?xi8>)>}> : () -> index
    %1 = "polygeist.memref2pointer"(%arg3) : (memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) -> !llvm.ptr
    %2 = llvm.mlir.addressof @str3 : !llvm.ptr
    %3 = llvm.getelementptr %2[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %4 = scf.while (%arg4 = %c0_i32) : (i32) -> i32 {
      %5 = arith.extsi %arg4 : i32 to i64
      %6 = affine.load %arg1[0] : memref<?xi64>
      %7 = arith.cmpi slt, %5, %6 : i64
      scf.condition(%7) %arg4 : i32
    } do {
    ^bb0(%arg4: i32):
      %5 = arith.index_cast %arg4 : i32 to index
      %6 = arith.muli %5, %0 : index
      %7 = arith.index_cast %6 : index to i64
      %8 = llvm.getelementptr %1[%7] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %9 = llvm.getelementptr %8[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, i32, memref<?xi8>)>
      %10 = llvm.load %9 : !llvm.ptr -> memref<?xi8>
      %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
      %12 = llvm.load %11 : !llvm.ptr -> f64
      %13 = llvm.call @printf(%3, %arg4, %12) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, f64) -> i32
      %14 = arith.addi %arg4, %c1_i32 : i32
      scf.yield %14 : i32
    }
    return
  }
  func.func @parallelEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c8_i64 = arith.constant 8 : i64
    %c3_i32 = arith.constant 3 : i32
    %c2_i32 = arith.constant 2 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<1xi64>
    %alloca_0 = memref.alloca() : memref<2xi64>
    %0 = llvm.mlir.addressof @str4 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<44 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = affine.load %arg1[0] : memref<?xi64>
    %6 = arith.trunci %5 : i64 to i32
    %7 = arith.index_cast %6 : i32 to index
    %alloca_1 = memref.alloca(%7) : memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %alloca_2 = memref.alloca(%7) : memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    call @artsDbCreateArrayFromDeps(%alloca_1, %6, %arg3, %c0_i32) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    call @artsDbCreateArrayFromDeps(%alloca_2, %6, %arg3, %6) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) -> ()
    %8 = arith.extsi %6 : i32 to i64
    %9 = arith.muli %8, %c8_i64 : i64
    %10 = call @artsMalloc(%9) : (i64) -> memref<?xi8>
    %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    scf.for %arg4 = %c0 to %7 step %c1 {
      %19 = arith.index_cast %arg4 : index to i32
      %20 = func.call @artsEventCreate(%3, %c1_i32) : (i32, i32) -> i64
      %21 = llvm.getelementptr %11[%19] : (!llvm.ptr, i32) -> !llvm.ptr, i64
      llvm.store %20, %21 : i64, !llvm.ptr
    }
    %12 = "polygeist.get_func"() <{name = @computeA}> : () -> !llvm.ptr
    %cast = memref.cast %alloca_0 : memref<2xi64> to memref<?xi64>
    %13 = "polygeist.pointer2memref"(%12) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %14 = "polygeist.get_func"() <{name = @computeB}> : () -> !llvm.ptr
    %cast_3 = memref.cast %alloca : memref<1xi64> to memref<?xi64>
    %15 = "polygeist.pointer2memref"(%14) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %16 = "polygeist.typeSize"() <{source = !llvm.struct<(i64, memref<?xi8>)>}> : () -> index
    %17 = "polygeist.memref2pointer"(%alloca_1) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>) -> !llvm.ptr
    %18 = "polygeist.memref2pointer"(%alloca_2) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>) -> !llvm.ptr
    scf.for %arg4 = %c0 to %7 step %c1 {
      %19 = arith.index_cast %arg4 : index to i32
      %20 = arith.extsi %19 : i32 to i64
      affine.store %20, %alloca_0[0] : memref<2xi64>
      %21 = llvm.getelementptr %11[%19] : (!llvm.ptr, i32) -> !llvm.ptr, i64
      %22 = llvm.load %21 : !llvm.ptr -> i64
      affine.store %22, %alloca_0[1] : memref<2xi64>
      %23 = func.call @artsEdtCreateWithEpoch(%13, %3, %c2_i32, %cast, %c1_i32, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      affine.store %20, %alloca[0] : memref<1xi64>
      %24 = arith.cmpi sgt, %19, %c0_i32 : i32
      %25 = arith.select %24, %c3_i32, %c2_i32 : i32
      %26 = func.call @artsEdtCreateWithEpoch(%15, %3, %c2_i32, %cast_3, %25, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
      %27 = llvm.load %21 : !llvm.ptr -> i64
      func.call @artsAddDependence(%27, %26, %c1_i32) : (i64, i64, i32) -> ()
      scf.if %24 {
        %34 = arith.addi %19, %c-1_i32 : i32
        %35 = llvm.getelementptr %11[%34] : (!llvm.ptr, i32) -> !llvm.ptr, i64
        %36 = llvm.load %35 : !llvm.ptr -> i64
        func.call @artsAddDependence(%36, %26, %c2_i32) : (i64, i64, i32) -> ()
      }
      %28 = arith.muli %arg4, %16 : index
      %29 = arith.index_cast %28 : index to i64
      %30 = llvm.getelementptr %17[%29] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %31 = llvm.load %30 : !llvm.ptr -> i64
      func.call @artsSignalEdt(%23, %c0_i32, %31) : (i64, i32, i64) -> ()
      %32 = llvm.getelementptr %18[%29] : (!llvm.ptr, i64) -> !llvm.ptr, i8
      %33 = llvm.load %32 : !llvm.ptr -> i64
      func.call @artsSignalEdt(%26, %c0_i32, %33) : (i64, i32, i64) -> ()
    }
    return
  }
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateArrayFromDeps(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsMalloc(i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @parallelDoneEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str5 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<45 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return
  }
  func.func @computeEDT(%arg0: i32, %arg1: memref<?x!llvm.struct<(i64, memref<?xi8>)>>, %arg2: memref<?x!llvm.struct<(i64, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2_i32 = arith.constant 2 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<1xi64>
    %alloca_0 = memref.alloca() : memref<1xi64>
    %0 = llvm.mlir.addressof @str6 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = call @artsGetCurrentEpochGuid() : () -> i64
    %5 = "polygeist.get_func"() <{name = @parallelDoneEdt}> : () -> !llvm.ptr
    %6 = llvm.mlir.zero : !llvm.ptr
    %7 = "polygeist.pointer2memref"(%6) : (!llvm.ptr) -> memref<?xi64>
    %8 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %9 = call @artsEdtCreate(%8, %3, %c0_i32, %7, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %10 = call @artsInitializeAndStartEpoch(%9, %c0_i32) : (i64, i32) -> i64
    %11 = arith.extsi %arg0 : i32 to i64
    affine.store %11, %alloca_0[0] : memref<1xi64>
    %12 = arith.muli %arg0, %c2_i32 : i32
    %13 = "polygeist.get_func"() <{name = @parallelEdt}> : () -> !llvm.ptr
    %cast = memref.cast %alloca_0 : memref<1xi64> to memref<?xi64>
    %14 = "polygeist.pointer2memref"(%13) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %15 = call @artsEdtCreateWithEpoch(%14, %3, %c1_i32, %cast, %12, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalDbs(%arg1, %15, %c0_i32, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    call @artsSignalDbs(%arg2, %15, %arg0, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    %16 = call @artsWaitOnHandle(%10) : (i64) -> i8
    affine.store %11, %alloca[0] : memref<1xi64>
    %17 = "polygeist.get_func"() <{name = @printDataBlockA}> : () -> !llvm.ptr
    %cast_1 = memref.cast %alloca : memref<1xi64> to memref<?xi64>
    %18 = "polygeist.pointer2memref"(%17) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %19 = call @artsEdtCreateWithEpoch(%18, %3, %c1_i32, %cast_1, %arg0, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalDbs(%arg1, %19, %c0_i32, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    %20 = "polygeist.get_func"() <{name = @printDataBlockB}> : () -> !llvm.ptr
    %21 = "polygeist.pointer2memref"(%20) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %22 = call @artsEdtCreateWithEpoch(%21, %3, %c1_i32, %cast_1, %arg0, %4) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalDbs(%arg2, %22, %c0_i32, %arg0) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) -> ()
    %23 = llvm.mlir.addressof @str7 : !llvm.ptr
    %24 = llvm.getelementptr %23[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<44 x i8>
    %25 = llvm.call @printf(%24) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return
  }
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalDbs(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsWaitOnHandle(i64) -> i8 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @finishMainEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %0 = llvm.mlir.addressof @str8 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = llvm.mlir.addressof @str9 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<40 x i8>
    %5 = llvm.call @printf(%4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %6 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    call @artsShutdown() : () -> ()
    return
  }
  func.func private @artsShutdown() attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @mainEdt(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c5 = arith.constant 5 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c40_i64 = arith.constant 40 : i64
    %c8_i64 = arith.constant 8 : i64
    %c9_i32 = arith.constant 9 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c5_i32 = arith.constant 5 : i32
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = llvm.mlir.addressof @str10 : !llvm.ptr
    %1 = llvm.getelementptr %0[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<32 x i8>
    %2 = llvm.call @printf(%1) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %3 = call @artsGetCurrentNode() : () -> i32
    %4 = "polygeist.get_func"() <{name = @finishMainEdt}> : () -> !llvm.ptr
    %5 = llvm.mlir.zero : !llvm.ptr
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?xi64>
    %7 = "polygeist.pointer2memref"(%4) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
    %8 = call @artsEdtCreate(%7, %3, %c0_i32, %6, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %9 = call @artsInitializeAndStartEpoch(%8, %c0_i32) : (i64, i32) -> i64
    %10 = call @artsMalloc(%c40_i64) : (i64) -> memref<?xi8>
    %11 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    %12 = call @artsMalloc(%c40_i64) : (i64) -> memref<?xi8>
    %13 = "polygeist.memref2pointer"(%12) : (memref<?xi8>) -> !llvm.ptr
    scf.for %arg4 = %c0 to %c5 step %c1 {
      %18 = arith.index_cast %arg4 : index to i32
      %19 = arith.sitofp %18 : i32 to f64
      %20 = llvm.getelementptr %11[%18] : (!llvm.ptr, i32) -> !llvm.ptr, f64
      llvm.store %19, %20 : f64, !llvm.ptr
      %21 = llvm.getelementptr %13[%18] : (!llvm.ptr, i32) -> !llvm.ptr, f64
      llvm.store %cst, %21 : f64, !llvm.ptr
    }
    %alloca = memref.alloca() : memref<5x!llvm.struct<(i64, memref<?xi8>)>>
    %cast = memref.cast %alloca : memref<5x!llvm.struct<(i64, memref<?xi8>)>> to memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    %alloca_0 = memref.alloca() : memref<5x!llvm.struct<(i64, memref<?xi8>)>>
    %cast_1 = memref.cast %alloca_0 : memref<5x!llvm.struct<(i64, memref<?xi8>)>> to memref<?x!llvm.struct<(i64, memref<?xi8>)>>
    call @artsDbCreateArray(%cast, %c8_i64, %c9_i32, %c5_i32, %10) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> ()
    call @artsDbCreateArray(%cast_1, %c8_i64, %c9_i32, %c5_i32, %12) : (memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) -> ()
    call @computeEDT(%c5_i32, %cast, %cast_1) : (i32, memref<?x!llvm.struct<(i64, memref<?xi8>)>>, memref<?x!llvm.struct<(i64, memref<?xi8>)>>) -> ()
    %14 = call @artsWaitOnHandle(%9) : (i64) -> i8
    call @artsFree(%10) : (memref<?xi8>) -> ()
    call @artsFree(%12) : (memref<?xi8>) -> ()
    %15 = llvm.mlir.addressof @str11 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<41 x i8>
    %17 = llvm.call @printf(%16) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    return
  }
  func.func private @artsDbCreateArray(memref<?x!llvm.struct<(i64, memref<?xi8>)>>, i64, i32, i32, memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsFree(memref<?xi8>) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func @initPerNode(%arg0: i32, %arg1: i32, %arg2: memref<?xmemref<?xi8>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c1_i32 = arith.constant 1 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<0xi64>
    %0 = arith.cmpi eq, %arg0, %c0_i32 : i32
    scf.if %0 {
      %4 = func.call @artsGetCurrentNode() : () -> i32
      %5 = "polygeist.get_func"() <{name = @mainEdt}> : () -> !llvm.ptr
      %cast = memref.cast %alloca : memref<0xi64> to memref<?xi64>
      %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>
      %7 = func.call @artsEdtCreate(%6, %4, %c1_i32, %cast, %c0_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, memref<?xi8>)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    }
    %1 = llvm.mlir.addressof @str12 : !llvm.ptr
    %2 = llvm.getelementptr %1[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<22 x i8>
    %3 = llvm.call @printf(%2, %arg0) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    return
  }
  func.func @initPerWorker(%arg0: i32, %arg1: i32, %arg2: i32, %arg3: memref<?xmemref<?xi8>>) attributes {llvm.linkage = #llvm.linkage<external>} {
    return
  }
  func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsRT(%arg0, %arg1) : (i32, memref<?xmemref<?xi8>>) -> i32
    return %c0_i32 : i32
  }
  func.func private @artsRT(i32, memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
