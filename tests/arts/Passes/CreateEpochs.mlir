// RUN: %carts opt %s --create-epochs | %filecheck %s --check-prefix=EPOCH

// Test that epoch pass inserts appropriate synchronization points

module {
  func.func @test_epoch_barriers(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    
    // Multiple EDT regions that should have epoch synchronization
    arts.edt "phase1" {
      %val1 = arith.constant 10 : i32
      memref.store %val1, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    // Barrier point should be inserted here
    
    arts.edt "phase2" {
      %val2 = memref.load %arg0[%c0] : memref<10xi32>
      %result = arith.addi %val2, %val2 : i32
      memref.store %result, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    return
  }
}

// EPOCH: func.func @test_epoch_barriers
// EPOCH: arts.epoch
// EPOCH: arts.edt "phase1"
// EPOCH: arts.edt "phase2"
// Check that epoch operations ensure proper synchronization between task phases
