// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --pipeline pattern-pipeline | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: arts.for(%c0) to(%{{.*}}) step(%c1)
// CHECK: } {arts.id = 7 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 32 : i64
// CHECK-NOT: tripCount = 4 : i64

module {
  func.func @main(%arg0: memref<4x8x256x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c256 = arith.constant 256 : index

    arts.for(%c0) to(%c4) step(%c1) {{
    ^bb0(%i: index):
      scf.for %j = %c0 to %c8 step %c1 {
        scf.for %k = %c0 to %c256 step %c1 {
          scf.for %l = %c0 to %c8 step %c1 {
            %0 = memref.load %arg0[%i, %j, %k, %l] : memref<4x8x256x8xf32>
            %1 = arith.addf %0, %0 : f32
            memref.store %1, %arg0[%i, %j, %k, %l] : memref<4x8x256x8xf32>
          }
        }
      }
      arts.yield
    }} {arts.id = 7 : i64, arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "test.c:10:3">}

    return
  }
}
