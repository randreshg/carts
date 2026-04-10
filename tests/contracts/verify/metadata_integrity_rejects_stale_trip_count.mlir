// RUN: not %carts-compile %s --arts-config %S/../../examples/arts.cfg --verify-metadata-integrity 2>&1 | %FileCheck %s

// CHECK: has stale metadata tripCount=99 (static trip count is 4)

module {
  func.func @main() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index

    arts.for(%c0) to(%c4) step(%c1) {{
    ^bb0(%iv: index):
      arts.yield
    }} {arts.id = 11 : i64, arts.metadata_provenance = "exact", arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 99 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "metadata_fixture.c:7:3">}

    return
  }
}
