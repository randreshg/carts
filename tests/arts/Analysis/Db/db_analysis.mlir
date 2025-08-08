// RUN: %carts opt %s --db -debug-only=db-analysis 2>&1 | %filecheck %s --check-prefix=DBAN

// Verify DbAnalysis debug output on a minimal module with DB-relevant ops

module {
  func.func @db_test(%A: memref<4xi32>) {
    %c0 = arith.constant 0 : index
    %db = arts.db_alloc : !arts.db<memref<4xi32>>
    arts.db_acquire %db : !arts.db<memref<4xi32>>
    memref.store (arith.constant 1 : i32), %A[%c0] : memref<4xi32>
    arts.db_release %db : !arts.db<memref<4xi32>>
    return
  }
}

// DBAN: Initializing DbAnalysis for module
// DBAN: Creating new DbGraph for function: db_test

