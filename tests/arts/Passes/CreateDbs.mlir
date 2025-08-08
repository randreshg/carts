// RUN: %carts opt %s --create-dbs | %filecheck %s --check-prefix=CREATE_DBS

// Test that CreateDB pass inserts appropriate DB allocation/acquire/release operations

module {
  func.func @test_db_creation(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    
    // EDT that accesses shared data should get DB operations inserted
    arts.edt "shared_data_task" {
      %val = arith.constant 42 : i32
      memref.store %val, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    return
  }
}

// CREATE_DBS: func.func @test_db_creation
// CREATE_DBS: arts.db_alloc
// CREATE_DBS: arts.db_acquire
// CREATE_DBS: arts.db_release
// CREATE_DBS: arts.edt
