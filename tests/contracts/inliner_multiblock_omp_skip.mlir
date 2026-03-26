// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline raise-memref-dimensionality | %FileCheck %s

// Verify that multi-block callees inside OMP regions are skipped (not crashed)
// by the Inliner. Only single-block callees can be manually inlined in OMP
// because the OMP dialect lacks a DialectInlinerInterface.
// CHECK: func.call @multiblock_helper

module {
  func.func private @multiblock_helper(%arg0: memref<4xf64>, %idx: index) -> f64 {
    %c0 = arith.constant 0 : index
    %cmp = arith.cmpi slt, %idx, %c0 : index
    cf.cond_br %cmp, ^bb1, ^bb2
  ^bb1:
    %zero = arith.constant 0.0 : f64
    return %zero : f64
  ^bb2:
    %val = memref.load %arg0[%idx] : memref<4xf64>
    return %val : f64
  }

  func.func @main() -> f64 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %out = memref.alloc() : memref<4xf64>
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%iv) : index = (%c0) to (%c4) step (%c1) {
          %buf = memref.alloc() : memref<4xf64>
          %result = func.call @multiblock_helper(%buf, %iv) : (memref<4xf64>, index) -> f64
          memref.store %result, %out[%iv] : memref<4xf64>
          memref.dealloc %buf : memref<4xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    %r = memref.load %out[%c0] : memref<4xf64>
    memref.dealloc %out : memref<4xf64>
    return %r : f64
  }
}
