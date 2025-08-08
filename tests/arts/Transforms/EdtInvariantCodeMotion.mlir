// RUN: %carts opt %s --edt-invariant-code-motion | %filecheck %s --check-prefix=LICM

// Test that invariant code motion hoists loop-invariant operations from EDT regions

module {
  func.func @test_edt_licm(%arg0: memref<10xi32>) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    
    arts.edt "compute_task" {
      // This constant should be hoisted out of any loops inside the EDT
      scf.for %i = %c0 to %c10 step %c1 {
        %invariant_val = arith.constant 42 : i32  // Loop invariant
        %loop_var = arith.index_cast %i : index to i32
        %result = arith.addi %invariant_val, %loop_var : i32
        memref.store %result, %arg0[%i] : memref<10xi32>
      }
      arts.edt_terminator
    }
    
    return
  }
}

// LICM: func.func @test_edt_licm
// LICM: arts.edt
// LICM: arith.constant 42 : i32
// Check that invariant operations are properly identified and potentially hoisted
