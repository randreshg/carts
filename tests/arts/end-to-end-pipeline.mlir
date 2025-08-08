// RUN: %carts run %s --O3 --arts-opt --emit-llvm | %filecheck %s --check-prefix=PIPELINE

// Test end-to-end pipeline from high-level ARTS constructs to LLVM IR

module {
  func.func @pipeline_test(%arg0: memref<100xi32>) {
    %c0 = arith.constant 0 : index
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    
    // High-level parallel construct that should be fully lowered
    arts.parallel {
      scf.for %i = %c0 to %c100 step %c1 {
        %val = arith.constant 42 : i32
        memref.store %val, %arg0[%i] : memref<100xi32>
      }
      arts.parallel_terminator
    }
    
    return
  }
}

// PIPELINE: define {{.*}} @pipeline_test
// Check that the full pipeline produces executable LLVM IR with runtime calls
// PIPELINE: call {{.*}} @artsEdtCreate
// PIPELINE: ret
