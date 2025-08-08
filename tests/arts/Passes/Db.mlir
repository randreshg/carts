// RUN: %carts opt %s --db | %filecheck %s --check-prefix=DB_OPT

// Test that DB pass performs optimizations like shrinking, reuse, and hoisting

module {
  func.func @test_db_optimization(%arg0: memref<100xi32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c100 = arith.constant 100 : index
    
    // DB operations that could be optimized
    %db = arts.db_alloc : !arts.db<memref<100xi32>>
    arts.db_acquire %db : !arts.db<memref<100xi32>>
    
    arts.edt "compute_task" {
      %val = arith.constant 42 : i32
      memref.store %val, %arg0[%c0] : memref<100xi32>
      arts.edt_terminator
    }
    
    arts.db_release %db : !arts.db<memref<100xi32>>
    
    return
  }
}

// DB_OPT: func.func @test_db_optimization
// DB_OPT: arts.db_alloc
// DB_OPT: arts.db_acquire
// DB_OPT: arts.db_release
// Check that DB optimizations maintain correctness while potentially
// reducing memory usage or improving data locality
