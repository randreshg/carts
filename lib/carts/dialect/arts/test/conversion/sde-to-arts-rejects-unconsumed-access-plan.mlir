// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 | %FileCheck %s

// A committed SDE access window must become an ARTS acquire/EDT dependency.
// Leaving it outside any schedulable CU/SU work is a producer error, not
// something the boundary may erase.

// CHECK: error: {{.*}}arts.db_access_plan
// CHECK-SAME: was not consumed during SDE access realization into ARTS dependencies

func.func @unconsumed_access_plan() {
  %A = memref.alloc() : memref<4x256xf32>
  "arts.db_access_plan"(%A) <{
    mode = #arts.mode<out>,
    ownerDimCount = 1 : i64,
    blockLo = [0],
    blockHi = [4],
    validExtents = [256]
  }> : (memref<4x256xf32>) -> ()
  return
}
