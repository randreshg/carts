// RUN: %carts opt %s --arts-inliner | %filecheck %s --check-prefix=INLINE

// Test that ARTS inliner properly inlines function calls

module {
  func.func private @simple_computation(%arg0: i32) -> i32 {
    %c1 = arith.constant 1 : i32
    %result = arith.addi %arg0, %c1 : i32
    return %result : i32
  }
  
  func.func @test_inlining(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    %val = arith.constant 42 : i32
    
    // This function call should be inlined
    %result = func.call @simple_computation(%val) : (i32) -> i32
    memref.store %result, %arg0[%c0] : memref<10xi32>
    
    return
  }
}

// INLINE: func.func @test_inlining
// Check that the function call has been inlined and @simple_computation is removed
// INLINE-NOT: func.call @simple_computation
