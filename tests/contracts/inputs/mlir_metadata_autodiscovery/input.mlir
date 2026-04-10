module {
  func.func @kernel() -> i32 {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c1 = arith.constant 1 : index
    %c3 = arith.constant 3 : index
    %buf = memref.alloc() : memref<4xi32>
    scf.for %i = %c0 to %c4 step %c1 {
      %v = arith.index_cast %i : index to i32
      memref.store %v, %buf[%i] : memref<4xi32>
    } loc("metadata_fixture.c":7:3)
    %last = memref.load %buf[%c3] : memref<4xi32>
    memref.dealloc %buf : memref<4xi32>
    return %last : i32
  }
}
