taskwithdeps-events.mlir:29:11: error: 'arts.edt' op operand count (5) does not match with the total size (4) specified in attribute 'operandSegmentSizes'
          arts.edt parameters(%11, %10, %arg0) : (i32, f64, index), dependencies(%12) : (memref<1xf64>), events(%13) : (i64) {
          ^
taskwithdeps-events.mlir:29:11: note: see current operation: 
"arts.edt"(%21, %20, %arg0, %22, %23) <{operandSegmentSizes = array<i32: 3, 0, 1, 0>}> ({
  %30 = "arith.sitofp"(%21) : (i32) -> f64
  %31 = "arith.addf"(%30, %20) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
  "memref.store"(%31, %22, %2) : (f64, memref<1xf64>, index) -> ()
  "arts.yield"() : () -> ()
}) : (i32, f64, index, memref<1xf64>, i64) -> ()
