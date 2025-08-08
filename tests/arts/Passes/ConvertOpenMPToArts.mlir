// RUN: %carts opt %s --convert-openmp-to-arts | %filecheck %s --check-prefix=CONVERT

// Test that OpenMP parallel for loops are converted to ARTS parallel constructs

module {
  func.func @test_parallel_for(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    
    // This should be converted to arts.parallel
    omp.parallel {
      omp.wsloop for (%i) : index = (%c0) to (%c10) step (%c1) {
        %val = arith.constant 42 : i32
        memref.store %val, %arg0[%i] : memref<10xi32>
        omp.yield
      }
      omp.terminator
    }
    return
  }
}

// CONVERT: func.func @test_parallel_for
// CONVERT: arts.parallel
// CONVERT-NOT: omp.parallel
// CONVERT-NOT: omp.wsloop
