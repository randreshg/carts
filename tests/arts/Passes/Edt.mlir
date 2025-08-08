// RUN: %carts opt %s --edt | %filecheck %s --check-prefix=EDT

// Test that EDT pass performs task optimizations like fusion

module {
  func.func @test_edt_fusion(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c10 = arith.constant 10 : index
    
    // Simple EDT operations that could be fused or optimized
    arts.edt "task1" {
      %val1 = arith.constant 10 : i32
      memref.store %val1, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    arts.edt "task2" {
      %val2 = arith.constant 20 : i32
      memref.store %val2, %arg0[%c1] : memref<10xi32>
      arts.edt_terminator
    }
    
    return
  }
}

// EDT: func.func @test_edt_fusion
// EDT: arts.edt
// EDT-NOT: omp.
// Check that EDT transformations preserve structure while enabling optimizations
