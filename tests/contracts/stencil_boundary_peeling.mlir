// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --stop-at loop-reordering | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: scf.for %[[ROW:.*]] = %c0 to %c8 step %c1 {
// CHECK: %[[ROW_I32:.*]] = arith.index_cast %[[ROW]] : index to i32
// CHECK: %[[ROW_LAST:.*]] = arith.cmpi eq, %[[ROW_I32]], %c7_i32 : i32
// CHECK: %[[ROW_FIRST:.*]] = arith.cmpi eq, %[[ROW]], %c0 : index
// CHECK: %[[ROW_BOUNDARY:.*]] = arith.ori %[[ROW_FIRST]], %[[ROW_LAST]] : i1
// CHECK: scf.if %[[ROW_BOUNDARY]] {
// CHECK: scf.for %[[BOUNDARY_COL:.*]] = %c0 to %c8 step %c1 {
// CHECK: memref.load %arg2[%[[ROW]], %[[BOUNDARY_COL]]] : memref<8x8xf64>
// CHECK: memref.store %{{.*}}, %arg1[%[[ROW]], %[[BOUNDARY_COL]]] : memref<8x8xf64>
// CHECK: } else {
// CHECK: scf.for %[[LEFT_COL:.*]] = %c0 to %c1 step %c1 {
// CHECK: memref.load %arg2[%[[ROW]], %[[LEFT_COL]]] : memref<8x8xf64>
// CHECK: memref.store %{{.*}}, %arg1[%[[ROW]], %[[LEFT_COL]]] : memref<8x8xf64>
// CHECK: %c7 = arith.constant 7 : index
// CHECK: scf.for %[[MID_COL:.*]] = %c1 to %c7 step %c1 {
// CHECK-NOT: scf.if
// CHECK: %[[UP_ROW:.*]] = arith.addi %[[ROW]], %c-1 : index
// CHECK: memref.load %arg0[%[[UP_ROW]], %[[MID_COL]]] : memref<8x8xf64>
// CHECK: %[[RIGHT_COL:.*]] = arith.addi %[[MID_COL]], %c1 : index
// CHECK: memref.load %arg0[%[[ROW]], %[[RIGHT_COL]]] : memref<8x8xf64>
// CHECK: %[[LEFT_NEIGHBOR:.*]] = arith.addi %[[MID_COL]], %c-1 : index
// CHECK: memref.load %arg0[%[[ROW]], %[[LEFT_NEIGHBOR]]] : memref<8x8xf64>
// CHECK: %[[DOWN_ROW:.*]] = arith.addi %[[ROW]], %c1 : index
// CHECK: memref.load %arg0[%[[DOWN_ROW]], %[[MID_COL]]] : memref<8x8xf64>
// CHECK: memref.store %{{.*}}, %arg1[%[[ROW]], %[[MID_COL]]] : memref<8x8xf64>
// CHECK: scf.for %[[RIGHT_EDGE:.*]] = %c7 to %c8 step %c1 {
// CHECK: memref.load %arg2[%[[ROW]], %[[RIGHT_EDGE]]] : memref<8x8xf64>
// CHECK: memref.store %{{.*}}, %arg1[%[[ROW]], %[[RIGHT_EDGE]]] : memref<8x8xf64>

module {
  func.func @main(%src: memref<8x8xf64>, %dst: memref<8x8xf64>, %rhs: memref<8x8xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %cneg1 = arith.constant -1 : index
    %c7_i32 = arith.constant 7 : i32
    %c0_i32 = arith.constant 0 : i32
    %true = arith.constant true
    %cneg1_i32 = arith.constant -1 : i32
    %c1_i32 = arith.constant 1 : i32
    %cquarter = arith.constant 2.500000e-01 : f64
    scf.for %i = %c0 to %c8 step %c1 {
      %i_i32 = arith.index_cast %i : index to i32
      %row_first = arith.cmpi eq, %i_i32, %c0_i32 : i32
      %row_last = arith.cmpi eq, %i_i32, %c7_i32 : i32
      %im1_i32 = arith.addi %i_i32, %cneg1_i32 : i32
      %im1 = arith.index_cast %im1_i32 : i32 to index
      %ip1_i32 = arith.addi %i_i32, %c1_i32 : i32
      %ip1 = arith.index_cast %ip1_i32 : i32 to index
      scf.for %j = %c0 to %c8 step %c1 {
        %j_i32 = arith.index_cast %j : index to i32
        %is_lower = scf.if %row_first -> (i1) {
          scf.yield %true : i1
        } else {
          %cmp = arith.cmpi eq, %j_i32, %c0_i32 : i32
          scf.yield %cmp : i1
        }
        %not_lower = arith.xori %is_lower, %true : i1
        %row_last_only = arith.andi %not_lower, %row_last : i1
        %prefix_boundary = arith.ori %is_lower, %row_last_only : i1
        %is_boundary = scf.if %prefix_boundary -> (i1) {
          scf.yield %true : i1
        } else {
          %cmp = arith.cmpi eq, %j_i32, %c7_i32 : i32
          scf.yield %cmp : i1
        }
        scf.if %is_boundary {
          %v = memref.load %rhs[%i, %j] : memref<8x8xf64>
          memref.store %v, %dst[%i, %j] : memref<8x8xf64>
        } else {
          %up = memref.load %src[%im1, %j] : memref<8x8xf64>
          %right_idx = arith.addi %j, %c1 : index
          %right = memref.load %src[%i, %right_idx] : memref<8x8xf64>
          %sum0 = arith.addf %up, %right : f64
          %left_idx = arith.addi %j, %cneg1 : index
          %left = memref.load %src[%i, %left_idx] : memref<8x8xf64>
          %sum1 = arith.addf %sum0, %left : f64
          %down = memref.load %src[%ip1, %j] : memref<8x8xf64>
          %sum2 = arith.addf %sum1, %down : f64
          %f = memref.load %rhs[%i, %j] : memref<8x8xf64>
          %sum3 = arith.addf %sum2, %f : f64
          %out = arith.mulf %sum3, %cquarter : f64
          memref.store %out, %dst[%i, %j] : memref<8x8xf64>
        }
      }
    }
    return
  }
}
