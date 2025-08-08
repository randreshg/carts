// RUN: %carts run %s --emit-llvm | %filecheck %s --check-prefix=LLVM_IR

// Test that ARTS to LLVM conversion generates appropriate runtime calls

module {
  func.func @test_arts_to_llvm(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    
    // ARTS operations that should be lowered to LLVM runtime calls
    %db = arts.db_alloc : !arts.db<memref<10xi32>>
    arts.db_acquire %db : !arts.db<memref<10xi32>>
    
    arts.edt_outline_fn @task_function() {
      %val = arith.constant 42 : i32
      memref.store %val, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    arts.db_release %db : !arts.db<memref<10xi32>>
    
    return
  }
}

// LLVM_IR: define {{.*}} @test_arts_to_llvm
// LLVM_IR: call {{.*}} @artsDbCreate
// LLVM_IR: call {{.*}} @artsDbAcquire
// LLVM_IR: call {{.*}} @artsEdtCreate
// LLVM_IR: call {{.*}} @artsDbRelease
// Check that ARTS operations are properly lowered to runtime API calls
