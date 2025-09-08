// RUN: %carts opt %s --normalize-dbs | %filecheck %s --check-prefix=OPAQUE

// Test that DB to opaque pointer conversion makes DBs compatible with ARTS runtime

module {
  func.func @test_opaque_conversion(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    
    // DB operations with typed memrefs that should be converted to opaque pointers
    %db = arts.db_alloc : !arts.db<memref<10xi32>>
    arts.db_acquire %db : !arts.db<memref<10xi32>>
    
    arts.edt "db_using_task" {
      %val = arith.constant 42 : i32
      memref.store %val, %arg0[%c0] : memref<10xi32>
      arts.edt_terminator
    }
    
    arts.db_release %db : !arts.db<memref<10xi32>>
    
    return
  }
}

// OPAQUE: func.func @test_opaque_conversion
// Check that DB element types are rewritten from typed memrefs to opaque i8 pointers
// OPAQUE: arts.db_alloc
// OPAQUE: arts.db_acquire
// OPAQUE: memref<10xi8>
// OPAQUE: arts.db_release
