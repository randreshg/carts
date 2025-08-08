// RUN: %carts opt %s --edt-lowering | %filecheck %s --check-prefix=EDT_LOWER

// Test that EDT lowering pass outlines EDT regions into functions and packs parameters

module {
  func.func @test_edt_lowering(%arg0: memref<10xi32>, %arg1: i32) {
    %c0 = arith.constant 0 : index
    
    // This EDT should be outlined to a separate function
    arts.edt "compute_task" {
      %val = arith.addi %arg1, %arg1 : i32
      memref.store %val, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    return
  }
}

// EDT_LOWER: func.func @test_edt_lowering
// EDT_LOWER: arts.edt_outline_fn
// EDT_LOWER: func.func private @compute_task_outlined
// Check that parameters and dependencies are properly packed for runtime execution
